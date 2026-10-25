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
        auto facts = joins.facts.find(piece.itemId);
        if (facts != joins.facts.end() && facts->second.classId == ARMOR_CLASS && facts->second.subclassId >= 1 &&
            facts->second.subclassId <= LAST_ARMOUR_TYPE) {
            counts[facts->second.subclassId]++;
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
    auto facts = joins.facts.find(itemId);
    piece.inventoryType = facts != joins.facts.end() ? facts->second.inventoryType : 0;
    if (!look) {
        ItemLooks looks = LooksOf(joins, itemId);
        look = looks.empty() ? nullptr : looks.first;
    } else {
        piece.modifiedAppearanceId = look->id;
    }
    if (look) {
        auto appearance = joins.appearances.find(look->appearance);
        piece.displayId = appearance != joins.appearances.end() ? appearance->second.display : 0;
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
        set.name = sets.String(r, TRANSMOG_SET_NAME);
        set.classMask = sets.Value(r, TRANSMOG_SET_CLASS_MASK);
        set.expansion = sets.Value(r, TRANSMOG_SET_EXPANSION);
        for (uint32_t id : piecesOf[set.setId]) {
            auto look = joins.modified.find(id);
            if (look != joins.modified.end()) {
                set.pieces.push_back(PieceOf(look->second.item, &look->second, joins));
            }
        }
        set.armour = ArmourOf(set.pieces, joins);
        if (!set.name.empty() && !set.pieces.empty()) {
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
        set.name = sets.String(r, ITEM_SET_NAME);
        for (uint32_t item : sets.Values(r, ITEM_SET_ITEMS)) {
            if (item) {
                set.pieces.push_back(PieceOf(item, nullptr, joins));
            }
        }
        set.armour = ArmourOf(set.pieces, joins);
        if (!set.name.empty() && !set.pieces.empty()) {
            this->m_sets.push_back(std::move(set));
        }
    }
}

void RetailIndex::BuildSets(const BrowserTables& tables, const ItemJoins& joins) {
    this->BuildTransmogSets(tables, joins);
    this->BuildItemSets(tables, joins);
    std::sort(this->m_sets.begin(), this->m_sets.end(), [](const BrowseSet& a, const BrowseSet& b) { return a.key < b.key; });
    for (const BrowseSet& set : this->m_sets) {
        this->m_setKeys.push_back({ { NormalizeSearchText(set.name) }, { set.setId } });
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
        mount.name = mounts.String(r, MOUNT_NAME);
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
    for (const BrowseMount& mount : this->m_mounts) {
        Keys keys = { { NormalizeSearchText(mount.name) }, { mount.id } };
        for (const BrowseDisplay& display : mount.displays) {
            keys.ids.push_back(display.id);
        }
        keys.ids.push_back(mount.spellId);
        this->m_mountKeys.push_back(std::move(keys));
    }
}

// One user per entry: its first display that draws the model.
static void AddUser(std::map<uint32_t, BrowseModel>& models, uint32_t fdid, bool creature, ModelUser user) {
    if (!fdid) {
        return;
    }
    BrowseModel& model = models[fdid];
    if (!model.users.empty() && model.users.back().kind == user.kind && model.users.back().id == user.id) {
        return;
    }
    if (model.users.empty()) {
        model.fdid = fdid;
        model.creature = creature;
    }
    model.users.push_back(std::move(user));
}

void RetailIndex::BuildModels() {
    std::map<uint32_t, BrowseModel> models;
    for (const BrowseCreature& creature : this->m_creatures) {
        for (const BrowseDisplay& display : creature.displays) {
            AddUser(models, display.facts.modelFdid, true, { BrowseKind::Creature, creature.id, display.id, creature.name });
        }
    }
    for (const BrowseMount& mount : this->m_mounts) {
        for (const BrowseDisplay& display : mount.displays) {
            AddUser(models, display.facts.modelFdid, true, { BrowseKind::Mount, mount.id, display.id, mount.name });
        }
    }
    for (const BrowseItem& item : this->m_items) {
        for (const BrowseAppearance& look : item.appearances) {
            for (uint32_t file : look.facts.models) {
                AddUser(models, file, false, { BrowseKind::Item, item.id, look.displayId, item.name });
            }
        }
    }
    for (auto& [fdid, model] : models) {
        Keys keys = { {}, { fdid } };
        std::set<std::string> seen;
        for (const ModelUser& user : model.users) {
            std::string name = NormalizeSearchText(user.name);
            if (keys.names.size() < MODEL_NAME_KEYS && seen.insert(name).second) {
                keys.names.push_back(std::move(name));
            }
        }
        this->m_modelKeys.push_back(std::move(keys));
        this->m_models.push_back(std::move(model));
    }
}

}
