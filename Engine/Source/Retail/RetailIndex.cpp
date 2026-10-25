#include "Engine/Source/Retail/RetailIndex.h"
#include <cstdio>
#include <cstring>
#include "Engine/Source/Retail/ItemTables.h"
#include "Engine/Source/Retail/RetailIndexJoins.h"

// Builds the retail browser's index: each kind's entries by id and the keys Search compares. Sets, mounts and models
// are in RetailIndexSets.cpp, search in RetailIndexSearch.cpp.

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

static float AsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

ItemJoins ReadItemJoins(const BrowserTables& tables) {
    ItemJoins joins;
    const Db2Table& items = tables.items;
    for (size_t r = 0; r < items.RowCount(); r++) {
        joins.facts[items.RowId(r)] = { items.Value(r, ITEM_CLASS), items.Value(r, ITEM_SUBCLASS), items.Value(r, ITEM_MATERIAL),
            items.Value(r, ITEM_INVENTORY_TYPE), items.Value(r, ITEM_ICON), items.Value(r, ITEM_SHEATHE_TYPE) };
    }
    for (size_t r = 0; r < tables.appearances.RowCount(); r++) {
        joins.appearances[tables.appearances.RowId(r)] = { tables.appearances.Value(r, APPEARANCE_DISPLAY), tables.appearances.Value(r, APPEARANCE_ICON) };
    }
    const Db2Table& ima = tables.modifiedAppearances;
    for (size_t r = 0; r < ima.RowCount(); r++) {
        ModifiedAppearance row = { ima.RowId(r), ima.Value(r, IMA_ITEM), ima.Value(r, IMA_MODIFIER), ima.Value(r, IMA_APPEARANCE), ima.Value(r, IMA_ORDER) };
        joins.modified[row.id] = row;
        joins.ofItem[row.item].push_back(row);
    }
    for (auto& [item, rows] : joins.ofItem) {
        std::sort(rows.begin(), rows.end(), [](const ModifiedAppearance& a, const ModifiedAppearance& b) {
            return a.order != b.order ? a.order < b.order : a.id < b.id;
        });
    }
    return joins;
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
    for (const auto& [table, layout] : Layouts(tables)) {
        if (table->LayoutHash() != layout) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "browser table layout %08X, expected %08X", table->LayoutHash(), layout);
            error = buffer;
            return false;
        }
        this->m_skippedRows += table->SkippedRows();
    }
    ItemJoins joins = ReadItemJoins(tables);
    this->BuildNames(tables);
    this->BuildCreatures(tables, lookups);
    this->BuildItems(tables, joins, lookups);
    this->BuildSets(tables, joins);
    this->BuildMounts(tables, lookups);
    this->BuildModels();
    return true;
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

static BrowseCreature CreatureOf(const Db2Table& creatures, size_t r, const BrowserLookups& lookups) {
    BrowseCreature creature;
    creature.id = creatures.RowId(r);
    creature.name = creatures.String(r, CREATURE_NAME);
    creature.title = creatures.String(r, CREATURE_TITLE);
    creature.classification = creatures.Value(r, CREATURE_CLASSIFICATION);
    creature.type = creatures.Value(r, CREATURE_TYPE);
    creature.family = creatures.Value(r, CREATURE_FAMILY);
    std::vector<uint32_t> displays = creatures.Values(r, CREATURE_DISPLAYS);
    std::vector<uint32_t> probabilities = creatures.Values(r, CREATURE_PROBABILITIES);
    for (size_t slot = 0; slot < displays.size(); slot++) {
        if (displays[slot]) {
            float probability = slot < probabilities.size() ? AsFloat(probabilities[slot]) : 0;
            creature.displays.push_back(CreatureDisplayOf(displays[slot], probability, lookups));
        }
    }
    return creature;
}

void RetailIndex::BuildCreatures(const BrowserTables& tables, const BrowserLookups& lookups) {
    for (size_t r = 0; r < tables.creatures.RowCount(); r++) {
        BrowseCreature creature = CreatureOf(tables.creatures, r, lookups);
        if (!creature.name.empty()) {
            this->m_creatures.push_back(std::move(creature));
        }
    }
    SortById(this->m_creatures);
    for (const BrowseCreature& creature : this->m_creatures) {
        Keys keys = { { NormalizeSearchText(creature.name) }, { creature.id } };
        for (const BrowseDisplay& display : creature.displays) {
            keys.ids.push_back(display.id);
        }
        this->m_creatureKeys.push_back(std::move(keys));
    }
}

static BrowseAppearance AppearanceOf(const ModifiedAppearance& row, const ItemJoins& joins, const BrowserLookups& lookups) {
    BrowseAppearance look;
    look.modifiedAppearanceId = row.id;
    look.modifier = row.modifier;
    look.appearanceId = row.appearance;
    auto appearance = joins.appearances.find(row.appearance);
    if (appearance != joins.appearances.end()) {
        look.displayId = appearance->second.display;
        look.icon = appearance->second.icon;
    }
    look.known = look.displayId && lookups.itemDisplay && lookups.itemDisplay(look.displayId, look.facts);
    return look;
}

static BrowseItem ItemOf(const Db2Table& names, size_t r, const ItemJoins& joins, const BrowserLookups& lookups) {
    BrowseItem item;
    item.id = names.RowId(r);
    item.name = names.String(r, SEARCH_NAME);
    item.quality = names.Value(r, SEARCH_QUALITY);
    item.expansion = names.Value(r, SEARCH_EXPANSION);
    item.allowableClass = names.Value(r, SEARCH_ALLOWABLE_CLASS);
    item.itemLevel = names.Value(r, SEARCH_ITEM_LEVEL);
    auto facts = joins.facts.find(item.id);
    if (facts != joins.facts.end()) {
        item.classId = facts->second.classId;
        item.subclassId = facts->second.subclassId;
        item.material = facts->second.material;
        item.inventoryType = facts->second.inventoryType;
        item.icon = facts->second.icon;
        item.sheatheType = facts->second.sheatheType;
    }
    auto looks = joins.ofItem.find(item.id);
    if (looks != joins.ofItem.end()) {
        for (const ModifiedAppearance& row : looks->second) {
            item.appearances.push_back(AppearanceOf(row, joins, lookups));
        }
    }
    if (!item.icon && !item.appearances.empty()) {
        item.icon = item.appearances[0].icon;
    }
    return item;
}

void RetailIndex::BuildItems(const BrowserTables& tables, const ItemJoins& joins, const BrowserLookups& lookups) {
    for (size_t r = 0; r < tables.searchNames.RowCount(); r++) {
        BrowseItem item = ItemOf(tables.searchNames, r, joins, lookups);
        if (!item.name.empty()) {
            this->m_items.push_back(std::move(item));
        }
    }
    SortById(this->m_items);
    for (const BrowseItem& item : this->m_items) {
        Keys keys = { { NormalizeSearchText(item.name) }, { item.id } };
        for (const BrowseAppearance& look : item.appearances) {
            keys.ids.push_back(look.displayId);
        }
        this->m_itemKeys.push_back(std::move(keys));
    }
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
