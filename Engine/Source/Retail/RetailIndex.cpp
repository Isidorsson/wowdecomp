#include "Engine/Source/Retail/RetailIndex.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include "Engine/Source/Retail/ItemTables.h"
#include "Engine/Source/Retail/RetailIndexJoins.h"

// Builds the retail browser's index: each kind's entries by id and the keys Search compares. Names are views of one
// TextPool and keys ranges of two flat vectors, so an entry costs no allocation of its own beyond its displays. Sets,
// mounts and models are in RetailIndexSets.cpp, search in RetailIndexSearch.cpp.

namespace Retail {

// Physical columns of the layouts (WoWDBDefs, BrowserTables.h).
constexpr size_t CREATURE_NAME = 0;
constexpr size_t CREATURE_TITLE = 2;
constexpr size_t CREATURE_CLASSIFICATION = 4;
constexpr size_t CREATURE_TYPE = 5;
constexpr size_t CREATURE_FAMILY = 6;
constexpr size_t CREATURE_DISPLAYS = 8;
constexpr size_t CREATURE_PROBABILITIES = 9;
constexpr size_t ITEM_CLASS = 0;
constexpr size_t ITEM_SUBCLASS = 1;
constexpr size_t ITEM_MATERIAL = 2;
constexpr size_t ITEM_INVENTORY_TYPE = 3;
constexpr size_t ITEM_SHEATHE_TYPE = 4;
constexpr size_t ITEM_ICON = 6;
constexpr size_t SEARCH_NAME = 0;
constexpr size_t SEARCH_QUALITY = 1;
constexpr size_t SEARCH_EXPANSION = 2;
constexpr size_t SEARCH_ALLOWABLE_CLASS = 5;
constexpr size_t SEARCH_ITEM_LEVEL = 10;
constexpr size_t IMA_ITEM = 1;
constexpr size_t IMA_MODIFIER = 2;
constexpr size_t IMA_APPEARANCE = 3;
constexpr size_t IMA_ORDER = 4;
constexpr size_t APPEARANCE_DISPLAY = 1;
constexpr size_t APPEARANCE_ICON = 2;
constexpr size_t NAME_COLUMN = 0;
constexpr size_t SUB_CLASS_CLASS = 3;
constexpr size_t SUB_CLASS_SUBCLASS = 4;

// A TextPool block: the names of a full index fill about a hundred.
constexpr size_t TEXT_BLOCK = 64 * 1024;

// Compares ItemModifiedAppearance rows with item ids, for equal_range over ItemJoins::byItem.
struct ItemOrder {
    bool operator()(const ModifiedAppearance& row, uint32_t item) const { return row.item < item; }
    bool operator()(uint32_t item, const ModifiedAppearance& row) const { return item < row.item; }
};

static float AsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string_view TextPool::Add(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    if (this->m_blocks.empty() || this->m_size - this->m_used < text.size()) {
        this->m_size = std::max(TEXT_BLOCK, text.size());
        this->m_blocks.push_back(std::make_unique<char[]>(this->m_size));
        this->m_used = 0;
    }
    char* at = this->m_blocks.back().get() + this->m_used;
    std::memcpy(at, text.data(), text.size());
    this->m_used += text.size();
    return { at, text.size() };
}

ItemJoins ReadItemJoins(const BrowserTables& tables) {
    ItemJoins joins;
    const Db2Table& items = tables.items;
    joins.facts.reserve(items.RowCount());
    joins.appearances.reserve(tables.appearances.RowCount());
    joins.byItem.reserve(tables.modifiedAppearances.RowCount());
    for (size_t r = 0; r < items.RowCount(); r++) {
        joins.facts.push_back({ items.RowId(r), items.Value(r, ITEM_CLASS), items.Value(r, ITEM_SUBCLASS), items.Value(r, ITEM_MATERIAL),
            items.Value(r, ITEM_INVENTORY_TYPE), items.Value(r, ITEM_ICON), items.Value(r, ITEM_SHEATHE_TYPE) });
    }
    const Db2Table& appearances = tables.appearances;
    for (size_t r = 0; r < appearances.RowCount(); r++) {
        joins.appearances.push_back({ appearances.RowId(r), appearances.Value(r, APPEARANCE_DISPLAY), appearances.Value(r, APPEARANCE_ICON) });
    }
    const Db2Table& ima = tables.modifiedAppearances;
    for (size_t r = 0; r < ima.RowCount(); r++) {
        joins.byItem.push_back({ ima.RowId(r), ima.Value(r, IMA_ITEM), ima.Value(r, IMA_MODIFIER), ima.Value(r, IMA_APPEARANCE), ima.Value(r, IMA_ORDER) });
    }
    joins.modified = joins.byItem;
    KeepLastById(joins.facts);
    KeepLastById(joins.appearances);
    KeepLastById(joins.modified);
    std::sort(joins.byItem.begin(), joins.byItem.end(), [](const ModifiedAppearance& a, const ModifiedAppearance& b) {
        return a.item != b.item ? a.item < b.item : a.order != b.order ? a.order < b.order : a.id < b.id;
    });
    return joins;
}

ItemLooks LooksOf(const ItemJoins& joins, uint32_t item) {
    auto range = std::equal_range(joins.byItem.begin(), joins.byItem.end(), item, ItemOrder());
    const ModifiedAppearance* base = joins.byItem.data();
    return { base + (range.first - joins.byItem.begin()), base + (range.second - joins.byItem.begin()) };
}

BrowseDisplay CreatureDisplayOf(uint32_t id, float probability, const BrowserLookups& lookups) {
    BrowseDisplay display;
    display.id = id;
    display.probability = probability;
    display.known = lookups.creatureDisplay && lookups.creatureDisplay(id, display.facts);
    return display;
}

static std::vector<std::pair<const Db2Table*, uint32_t>> Layouts(const BrowserTables& t) {
    return { { &t.creatures, CREATURE_LAYOUT }, { &t.items, ITEM_LAYOUT }, { &t.searchNames, ITEM_SEARCH_NAME_LAYOUT },
        { &t.modifiedAppearances, ITEM_MODIFIED_APPEARANCE_LAYOUT }, { &t.appearances, ITEM_APPEARANCE_LAYOUT },
        { &t.transmogSets, TRANSMOG_SET_LAYOUT }, { &t.transmogSetItems, TRANSMOG_SET_ITEM_LAYOUT }, { &t.itemSets, ITEM_SET_LAYOUT },
        { &t.mounts, MOUNT_LAYOUT }, { &t.mountDisplays, MOUNT_X_DISPLAY_LAYOUT }, { &t.creatureTypes, CREATURE_TYPE_LAYOUT },
        { &t.creatureFamilies, CREATURE_FAMILY_LAYOUT }, { &t.itemSubClasses, ITEM_SUB_CLASS_LAYOUT } };
}

bool RetailIndex::Build(const BrowserTables& tables, const BrowserLookups& lookups, std::string& error) {
    *this = RetailIndex();
    this->m_text = std::make_shared<TextPool>();
    for (const auto& [table, layout] : Layouts(tables)) {
        if (table->LayoutHash() != layout) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "browser table layout %08X, expected %08X", table->LayoutHash(), layout);
            error = buffer;
            return false;
        }
        this->m_skippedRows += table->SkippedRows();
    }
    this->BuildNames(tables);
    this->BuildCreatures(tables, lookups);
    {
        ItemJoins joins = ReadItemJoins(tables);   // Freed before the models join the entries.
        this->BuildItems(tables, joins, lookups);
        this->BuildSets(tables, joins);
    }
    this->BuildMounts(tables, lookups);
    this->BuildModels();
    this->BuildNameOrders();
    this->Compact();
    return true;
}

RetailIndex::Keys RetailIndex::StartKeys() const {
    return { static_cast<uint32_t>(this->m_keyNames.size()), 0, static_cast<uint32_t>(this->m_keyIds.size()), 0, 0 };
}

// After the entry's names and ids were added to m_keyNames and m_keyIds.
void RetailIndex::AddKeys(std::vector<Keys>& keys, Keys entry) {
    entry.names = static_cast<uint32_t>(this->m_keyNames.size()) - entry.firstName;
    entry.ids = static_cast<uint32_t>(this->m_keyIds.size()) - entry.firstId;
    keys.push_back(entry);
}

std::string_view RetailIndex::AddNormalized(std::string_view name) {
    return this->m_text->Add(NormalizeSearchText(name));
}

// Every vector at its size: the entries and keys grew while building.
void RetailIndex::Compact() {
    this->m_creatures.shrink_to_fit();
    this->m_items.shrink_to_fit();
    this->m_sets.shrink_to_fit();
    this->m_models.shrink_to_fit();
    this->m_mounts.shrink_to_fit();
    for (std::vector<Keys>* keys : { &this->m_creatureKeys, &this->m_itemKeys, &this->m_setKeys, &this->m_modelKeys, &this->m_mountKeys }) {
        keys->shrink_to_fit();
    }
    this->m_keyNames.shrink_to_fit();
    this->m_keyIds.shrink_to_fit();
}

void RetailIndex::BuildNames(const BrowserTables& tables) {
    for (size_t r = 0; r < tables.creatureTypes.RowCount(); r++) {
        this->m_typeNames[tables.creatureTypes.RowId(r)] = tables.creatureTypes.String(r, NAME_COLUMN);
    }
    for (size_t r = 0; r < tables.creatureFamilies.RowCount(); r++) {
        this->m_familyNames[tables.creatureFamilies.RowId(r)] = tables.creatureFamilies.String(r, NAME_COLUMN);
    }
    const Db2Table& subclasses = tables.itemSubClasses;
    for (size_t r = 0; r < subclasses.RowCount(); r++) {
        uint32_t key = subclasses.Value(r, SUB_CLASS_CLASS) << 8 | (subclasses.Value(r, SUB_CLASS_SUBCLASS) & 0xFF);
        this->m_subclassNames[key] = subclasses.String(r, NAME_COLUMN);
    }
}

// An unnamed creature is left out: it comes back without its title and displays.
static BrowseCreature CreatureOf(const Db2Table& creatures, size_t r, const BrowserLookups& lookups, TextPool& text) {
    BrowseCreature creature;
    creature.id = creatures.RowId(r);
    creature.name = text.Add(creatures.String(r, CREATURE_NAME));
    if (creature.name.empty()) {
        return creature;
    }
    creature.title = text.Add(creatures.String(r, CREATURE_TITLE));
    creature.classification = creatures.Value(r, CREATURE_CLASSIFICATION);
    creature.type = creatures.Value(r, CREATURE_TYPE);
    creature.family = creatures.Value(r, CREATURE_FAMILY);
    std::vector<uint32_t> displays = creatures.Values(r, CREATURE_DISPLAYS);
    std::vector<uint32_t> probabilities = creatures.Values(r, CREATURE_PROBABILITIES);
    creature.displays.reserve(static_cast<size_t>(std::count_if(displays.begin(), displays.end(), [](uint32_t id) { return id != 0; })));
    for (size_t slot = 0; slot < displays.size(); slot++) {
        if (displays[slot]) {
            float probability = slot < probabilities.size() ? AsFloat(probabilities[slot]) : 0;
            creature.displays.push_back(CreatureDisplayOf(displays[slot], probability, lookups));
        }
    }
    return creature;
}

void RetailIndex::BuildCreatures(const BrowserTables& tables, const BrowserLookups& lookups) {
    this->m_creatures.reserve(tables.creatures.RowCount());
    for (size_t r = 0; r < tables.creatures.RowCount(); r++) {
        BrowseCreature creature = CreatureOf(tables.creatures, r, lookups, *this->m_text);
        if (!creature.name.empty()) {
            this->m_creatures.push_back(std::move(creature));
        }
    }
    SortById(this->m_creatures);
    this->m_creatureKeys.reserve(this->m_creatures.size());
    for (const BrowseCreature& creature : this->m_creatures) {
        Keys keys = this->StartKeys();
        this->m_keyNames.push_back(this->AddNormalized(creature.name));
        this->m_keyIds.push_back(creature.id);
        for (const BrowseDisplay& display : creature.displays) {
            this->m_keyIds.push_back(display.id);
        }
        this->AddKeys(this->m_creatureKeys, keys);
    }
}

static BrowseAppearance AppearanceOf(const ModifiedAppearance& row, const ItemJoins& joins, const BrowserLookups& lookups) {
    BrowseAppearance look;
    look.modifiedAppearanceId = row.id;
    look.modifier = row.modifier;
    look.appearanceId = row.appearance;
    const AppearanceRow* appearance = FindById(joins.appearances, row.appearance);
    if (appearance) {
        look.displayId = appearance->display;
        look.icon = appearance->icon;
    }
    look.known = look.displayId && lookups.itemDisplay && lookups.itemDisplay(look.displayId, look.facts);
    return look;
}

// An unnamed item is left out: it comes back with its id and name only.
static BrowseItem ItemOf(const Db2Table& names, size_t r, const ItemJoins& joins, const BrowserLookups& lookups, TextPool& text) {
    BrowseItem item;
    item.id = names.RowId(r);
    item.name = text.Add(names.String(r, SEARCH_NAME));
    if (item.name.empty()) {
        return item;
    }
    item.quality = names.Value(r, SEARCH_QUALITY);
    item.expansion = names.Value(r, SEARCH_EXPANSION);
    item.allowableClass = names.Value(r, SEARCH_ALLOWABLE_CLASS);
    item.itemLevel = names.Value(r, SEARCH_ITEM_LEVEL);
    const ItemFacts* facts = FindById(joins.facts, item.id);
    if (facts) {
        item.classId = facts->classId;
        item.subclassId = facts->subclassId;
        item.material = facts->material;
        item.inventoryType = facts->inventoryType;
        item.icon = facts->icon;
        item.sheatheType = facts->sheatheType;
    }
    ItemLooks looks = LooksOf(joins, item.id);
    item.appearances.reserve(static_cast<size_t>(looks.end() - looks.begin()));
    for (const ModifiedAppearance& row : looks) {
        item.appearances.push_back(AppearanceOf(row, joins, lookups));
    }
    if (!item.icon && !item.appearances.empty()) {
        item.icon = item.appearances[0].icon;
    }
    return item;
}

void RetailIndex::BuildItems(const BrowserTables& tables, const ItemJoins& joins, const BrowserLookups& lookups) {
    this->m_items.reserve(tables.searchNames.RowCount());
    for (size_t r = 0; r < tables.searchNames.RowCount(); r++) {
        BrowseItem item = ItemOf(tables.searchNames, r, joins, lookups, *this->m_text);
        if (!item.name.empty()) {
            this->m_items.push_back(std::move(item));
        }
    }
    SortById(this->m_items);
    this->m_itemKeys.reserve(this->m_items.size());
    for (const BrowseItem& item : this->m_items) {
        Keys keys = this->StartKeys();
        this->m_keyNames.push_back(this->AddNormalized(item.name));
        this->m_keyIds.push_back(item.id);
        for (const BrowseAppearance& look : item.appearances) {
            this->m_keyIds.push_back(look.displayId);
        }
        this->AddKeys(this->m_itemKeys, keys);
    }
}

// Values in pool by the hash of their bytes.
using Places = std::unordered_multimap<uint64_t, uint32_t>;

static uint64_t HashOfBytes(const void* data, size_t size) {
    uint64_t hash = 14695981039346656037ull;   // FNV-1a.
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ static_cast<const uint8_t*>(data)[i]) * 1099511628211ull;
    }
    return hash;
}

// Each distinct value once in pool; the index of value. Values compare by their bytes, so a float is kept bit for bit.
template <typename Value>
static uint32_t Shared(std::vector<Value>& pool, Places& places, const Value& value) {
    uint64_t hash = HashOfBytes(&value, sizeof(value));
    auto range = places.equal_range(hash);
    for (auto place = range.first; place != range.second; ++place) {
        if (std::memcmp(&pool[place->second], &value, sizeof(value)) == 0) {
            return place->second;
        }
    }
    places.emplace(hash, static_cast<uint32_t>(pool.size()));
    pool.push_back(value);
    return static_cast<uint32_t>(pool.size() - 1);
}

bool RetailIndex::AttachSparse(const Db2Table& itemSparse, std::string& error) {
    if (!KnownLayout(itemSparse, ITEM_SPARSE_LAYOUT, "ItemSparse", error)) {
        return false;
    }
    Places statPlaces, restPlaces;
    for (size_t r = 0; r < itemSparse.RowCount(); r++) {
        uint32_t id = itemSparse.RowId(r);
        auto item = std::lower_bound(this->m_items.begin(), this->m_items.end(), id, [](const BrowseItem& i, uint32_t key) { return i.id < key; });
        SparseItem row;
        if (item == this->m_items.end() || item->id != id || !ReadSparseItem(itemSparse, r, row)) {
            continue;
        }
        SparseStats stats = { row.statTypes, row.statPercents, row.socketCosts };
        SparseRest rest = { row.itemLevel, row.quality, row.inventoryType, row.requiredLevel, row.sockets, row.socketBonus, row.delay,
            row.damageVariance, row.damageType, row.levelCurve, row.contentTuning, row.casterWeapon ? 1u : 0u };
        item->sparseStats = Shared(this->m_sparseStats, statPlaces, stats);
        item->sparseRest = Shared(this->m_sparseRest, restPlaces, rest);
    }
    this->m_sparseStats.shrink_to_fit();
    this->m_sparseRest.shrink_to_fit();
    this->m_skippedRows += itemSparse.SkippedRows();
    return true;
}

bool RetailIndex::SparseOf(const BrowseItem& item, SparseItem& sparse) const {
    if (item.sparseStats >= this->m_sparseStats.size() || item.sparseRest >= this->m_sparseRest.size()) {
        return false;
    }
    const SparseStats& stats = this->m_sparseStats[item.sparseStats];
    const SparseRest& rest = this->m_sparseRest[item.sparseRest];
    sparse = SparseItem();
    sparse.id = item.id;
    sparse.itemLevel = rest.itemLevel;
    sparse.quality = rest.quality;
    sparse.inventoryType = rest.inventoryType;
    sparse.requiredLevel = rest.requiredLevel;
    sparse.statTypes = stats.statTypes;
    sparse.statPercents = stats.statPercents;
    sparse.socketCosts = stats.socketCosts;
    sparse.sockets = rest.sockets;
    sparse.socketBonus = rest.socketBonus;
    sparse.delay = rest.delay;
    sparse.damageVariance = rest.damageVariance;
    sparse.damageType = rest.damageType;
    sparse.levelCurve = rest.levelCurve;
    sparse.contentTuning = rest.contentTuning;
    sparse.casterWeapon = rest.casterWeapon != 0;
    return true;
}

std::string_view RetailIndex::UserName(const ModelUser& user) const {
    const BrowseCreature* creature = user.kind == BrowseKind::Creature ? this->Creature(user.id) : nullptr;
    const BrowseItem* item = user.kind == BrowseKind::Item ? this->Item(user.id) : nullptr;
    const BrowseMount* mount = user.kind == BrowseKind::Mount ? this->Mount(user.id) : nullptr;
    return creature ? creature->name : item ? item->name : mount ? mount->name : std::string_view();
}

const BrowseCreature* RetailIndex::Creature(uint32_t id) const {
    return FindById(this->m_creatures, id);
}

const BrowseItem* RetailIndex::Item(uint32_t id) const {
    return FindById(this->m_items, id);
}

const BrowseSet* RetailIndex::Set(uint32_t key) const {
    return FindByKey(this->m_sets, key, [](const BrowseSet& set) { return set.key; });
}

const BrowseModel* RetailIndex::Model(uint32_t fdid) const {
    return FindByKey(this->m_models, fdid, [](const BrowseModel& model) { return model.fdid; });
}

const BrowseMount* RetailIndex::Mount(uint32_t id) const {
    return FindById(this->m_mounts, id);
}

size_t RetailIndex::Count(BrowseKind kind) const {
    return this->KeysOf(kind).size();
}

std::string RetailIndex::SubclassName(uint32_t classId, uint32_t subclassId) const {
    auto found = this->m_subclassNames.find(classId << 8 | (subclassId & 0xFF));
    return found != this->m_subclassNames.end() ? found->second : std::string();
}

}
