#include <algorithm>
#include <map>
#include <set>
#include "Engine/Source/Retail/RetailIndex.h"
#include "Engine/Source/Retail/RetailIndexJoins.h"

// The retail browser index's sets (TransmogSet and ItemSet pieces), mounts and models (each model file with what
// draws it).

namespace Retail {

constexpr size_t TRANSMOG_SET_NAME = 0;
constexpr size_t TRANSMOG_SET_CLASS_MASK = 2;
constexpr size_t TRANSMOG_SET_EXPANSION = 9;
constexpr size_t TRANSMOG_SET_ITEM_SET = 0;
constexpr size_t TRANSMOG_SET_ITEM_IMA = 1;
constexpr size_t ITEM_SET_NAME = 0;
constexpr size_t ITEM_SET_ITEMS = 4;
constexpr size_t MOUNT_NAME = 0;
constexpr size_t MOUNT_TYPE = 4;
constexpr size_t MOUNT_SPELL = 7;
constexpr size_t MOUNT_X_DISPLAY_DISPLAY = 0;

constexpr uint32_t ARMOR_CLASS = 4;
constexpr uint32_t LAST_ARMOUR_TYPE = 4;   // Plate; 1 cloth, 2 leather, 3 mail.
// A model drawn under more names than this (a generic body) is searched by its first ones only.
constexpr size_t MODEL_NAME_KEYS = 8;

// The armour subclass most pieces have, the lowest on a tie; 0 for a set without cloth to plate pieces.
static uint32_t ArmourOf(const std::vector<SetPiece>& pieces, const ItemJoins& joins) {
    uint32_t counts[LAST_ARMOUR_TYPE + 1] = {};
    for (const SetPiece& piece : pieces) {
        const ItemFacts* facts = FindById(joins.facts, piece.itemId);
        if (facts && facts->classId == ARMOR_CLASS && facts->subclassId >= 1 && facts->subclassId <= LAST_ARMOUR_TYPE) {
            counts[facts->subclassId]++;
        }
    }
    uint32_t armour = 0;
    for (uint32_t type = 1; type <= LAST_ARMOUR_TYPE; type++) {
        armour = counts[type] > counts[armour] ? type : armour;
    }
    return armour;
}

static SetPiece PieceOf(uint32_t itemId, const ModifiedAppearance* look, const ItemJoins& joins) {
    SetPiece piece;
    piece.itemId = itemId;
    const ItemFacts* facts = FindById(joins.facts, itemId);
    piece.inventoryType = facts ? facts->inventoryType : 0;
    if (!look) {
        ItemLooks looks = LooksOf(joins, itemId);
        look = looks.empty() ? nullptr : looks.first;
    } else {
        piece.modifiedAppearanceId = look->id;
    }
    if (look) {
        const AppearanceRow* appearance = FindById(joins.appearances, look->appearance);
        piece.displayId = appearance ? appearance->display : 0;
    }
    return piece;
}

void RetailIndex::BuildTransmogSets(const BrowserTables& tables, const ItemJoins& joins) {
    std::map<uint32_t, std::vector<uint32_t>> piecesOf;   // ItemModifiedAppearance ids by set, in row order.
    const Db2Table& items = tables.transmogSetItems;
    for (size_t r = 0; r < items.RowCount(); r++) {
        piecesOf[items.Value(r, TRANSMOG_SET_ITEM_SET)].push_back(items.Value(r, TRANSMOG_SET_ITEM_IMA));
    }
    const Db2Table& sets = tables.transmogSets;
    for (size_t r = 0; r < sets.RowCount(); r++) {
        BrowseSet set;
        set.key = set.setId = sets.RowId(r);
        std::string name = sets.String(r, TRANSMOG_SET_NAME);
        set.classMask = sets.Value(r, TRANSMOG_SET_CLASS_MASK);
        set.expansion = sets.Value(r, TRANSMOG_SET_EXPANSION);
        for (uint32_t id : piecesOf[set.setId]) {
            const ModifiedAppearance* look = FindById(joins.modified, id);
            if (look) {
                set.pieces.push_back(PieceOf(look->item, look, joins));
            }
        }
        set.armour = ArmourOf(set.pieces, joins);
        if (!name.empty() && !set.pieces.empty()) {
            set.name = this->m_text->Add(name);
            set.pieces.shrink_to_fit();
            this->m_sets.push_back(std::move(set));
        }
    }
}

void RetailIndex::BuildItemSets(const BrowserTables& tables, const ItemJoins& joins) {
    const Db2Table& sets = tables.itemSets;
    for (size_t r = 0; r < sets.RowCount(); r++) {
        BrowseSet set;
        set.setId = sets.RowId(r);
        set.key = ITEM_SET_KEY_BASE + set.setId;
        set.source = SetSource::ItemSet;
        std::string name = sets.String(r, ITEM_SET_NAME);
        for (uint32_t item : sets.Values(r, ITEM_SET_ITEMS)) {
            if (item) {
                set.pieces.push_back(PieceOf(item, nullptr, joins));
            }
        }
        set.armour = ArmourOf(set.pieces, joins);
        if (!name.empty() && !set.pieces.empty()) {
            set.name = this->m_text->Add(name);
            set.pieces.shrink_to_fit();
            this->m_sets.push_back(std::move(set));
        }
    }
}

void RetailIndex::BuildSets(const BrowserTables& tables, const ItemJoins& joins) {
    this->BuildTransmogSets(tables, joins);
    this->BuildItemSets(tables, joins);
    std::sort(this->m_sets.begin(), this->m_sets.end(), [](const BrowseSet& a, const BrowseSet& b) { return a.key < b.key; });
    this->m_setKeys.reserve(this->m_sets.size());
    for (const BrowseSet& set : this->m_sets) {
        Keys keys = this->StartKeys();
        this->m_keyNames.push_back(this->AddNormalized(set.name));
        this->m_keyIds.push_back(set.setId);
        this->AddKeys(this->m_setKeys, keys);
    }
}

void RetailIndex::BuildMounts(const BrowserTables& tables, const BrowserLookups& lookups) {
    std::map<uint32_t, std::vector<uint32_t>> displaysOf;
    const Db2Table& displays = tables.mountDisplays;
    for (size_t r = 0; r < displays.RowCount(); r++) {
        displaysOf[displays.Relation(r)].push_back(displays.Value(r, MOUNT_X_DISPLAY_DISPLAY));
    }
    const Db2Table& mounts = tables.mounts;
    for (size_t r = 0; r < mounts.RowCount(); r++) {
        BrowseMount mount;
        mount.id = mounts.RowId(r);
        mount.name = this->m_text->Add(mounts.String(r, MOUNT_NAME));
        mount.mountType = mounts.Value(r, MOUNT_TYPE);
        mount.spellId = mounts.Value(r, MOUNT_SPELL);
        for (uint32_t display : displaysOf[mount.id]) {
            mount.displays.push_back(CreatureDisplayOf(display, 1, lookups));
        }
        if (!mount.name.empty()) {
            this->m_mounts.push_back(std::move(mount));
        }
    }
    SortById(this->m_mounts);
    this->m_mountKeys.reserve(this->m_mounts.size());
    for (const BrowseMount& mount : this->m_mounts) {
        Keys keys = this->StartKeys();
        this->m_keyNames.push_back(this->AddNormalized(mount.name));
        this->m_keyIds.push_back(mount.id);
        for (const BrowseDisplay& display : mount.displays) {
            this->m_keyIds.push_back(display.id);
        }
        this->m_keyIds.push_back(mount.spellId);
        this->AddKeys(this->m_mountKeys, keys);
    }
}

// A model while the models are joined: its users and the normalized name of each.
struct ModelDraft {
    BrowseModel model;
    std::vector<std::string_view> names;
};

// One user per entry: its first display that draws the model.
static void AddUser(std::map<uint32_t, ModelDraft>& models, uint32_t fdid, bool creature, ModelUser user, std::string_view name) {
    if (!fdid) {
        return;
    }
    ModelDraft& draft = models[fdid];
    BrowseModel& model = draft.model;
    if (!model.users.empty() && model.users.back().kind == user.kind && model.users.back().id == user.id) {
        return;
    }
    if (model.users.empty()) {
        model.fdid = fdid;
        model.creature = creature;
    }
    model.users.push_back(user);
    draft.names.push_back(name);
}

// Every creature display the loaders know, under its model: also the displays no creature or mount names.
static void AddDisplays(std::map<uint32_t, ModelDraft>& models, const BrowserLookups& lookups) {
    if (!lookups.creatureDisplayIds) {
        return;
    }
    for (uint32_t id : lookups.creatureDisplayIds()) {
        BrowseDisplay display = CreatureDisplayOf(id, 0, lookups);
        if (!display.facts.modelFdid) {
            continue;
        }
        BrowseModel& model = models[display.facts.modelFdid].model;
        if (model.users.empty() && model.displays.empty()) {
            model.fdid = display.facts.modelFdid;
            model.creature = true;
        }
        model.displays.push_back(display);
    }
    for (auto& entry : models) {
        std::vector<BrowseDisplay>& displays = entry.second.model.displays;
        std::sort(displays.begin(), displays.end(), [](const BrowseDisplay& a, const BrowseDisplay& b) { return a.id < b.id; });
    }
}

void RetailIndex::BuildModels(const BrowserLookups& lookups) {
    std::map<uint32_t, ModelDraft> models;
    for (size_t entry = 0; entry < this->m_creatures.size(); entry++) {
        const BrowseCreature& creature = this->m_creatures[entry];
        for (const BrowseDisplay& display : creature.displays) {
            AddUser(models, display.facts.modelFdid, true, { BrowseKind::Creature, creature.id, display.id }, this->FirstName(this->m_creatureKeys[entry]));
        }
    }
    for (size_t entry = 0; entry < this->m_mounts.size(); entry++) {
        const BrowseMount& mount = this->m_mounts[entry];
        for (const BrowseDisplay& display : mount.displays) {
            AddUser(models, display.facts.modelFdid, true, { BrowseKind::Mount, mount.id, display.id }, this->FirstName(this->m_mountKeys[entry]));
        }
    }
    for (size_t entry = 0; entry < this->m_items.size(); entry++) {
        const BrowseItem& item = this->m_items[entry];
        for (const BrowseAppearance& look : item.appearances) {
            for (uint32_t file : look.facts.models) {
                AddUser(models, file, false, { BrowseKind::Item, item.id, look.displayId }, this->FirstName(this->m_itemKeys[entry]));
            }
        }
    }
    AddDisplays(models, lookups);
    this->m_models.reserve(models.size());
    this->m_modelKeys.reserve(models.size());
    for (auto& [fdid, draft] : models) {
        Keys keys = this->StartKeys();
        std::set<std::string_view> seen;
        for (std::string_view name : draft.names) {
            if (seen.size() < MODEL_NAME_KEYS && seen.insert(name).second) {
                this->m_keyNames.push_back(name);
            }
        }
        this->m_keyIds.push_back(fdid);
        this->AddKeys(this->m_modelKeys, keys);
        draft.model.users.shrink_to_fit();
        draft.model.displays.shrink_to_fit();
        this->m_models.push_back(std::move(draft.model));
    }
}

}
