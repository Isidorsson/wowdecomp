#ifndef TOOLS_TESTS_BROWSERFIXTURES_H
#define TOOLS_TESTS_BROWSERFIXTURES_H

#include <map>
#include <string>
#include <vector>
#include "Engine/Source/Retail/BrowserTables.h"
#include "Engine/Source/Retail/ItemTables.h"
#include "Engine/Source/Retail/RetailIndex.h"
#include "Tools/Tests/CreatureFixtures.h"
#include "Tools/Tests/Db2Fixtures.h"

// The retail browser's tables, with rows taken from a 12.1.5 dump (scratchpad browser\csv, dump2.exe) and columns at
// their WoWDBDefs physical numbers.

struct TextRow {
    uint32_t id;
    std::vector<uint32_t> values;           // Every column's values in order; a string column holds a placeholder.
    std::map<size_t, std::string> strings;  // By physical column.
    uint32_t relation = 0;
};

// A string column holds the distance from the field to its string, as if every record came first and the string
// block after them.
inline Retail::Db2Table TextTable(uint32_t layout, const std::vector<uint32_t>& widths, const std::vector<TextRow>& rows) {
    Wdc5Spec spec;
    spec.layoutHash = layout;
    std::vector<uint32_t> firstValue;
    uint16_t offset = 0;
    for (uint32_t width : widths) {
        firstValue.push_back(offset / 32);
        spec.columns.push_back({ 0, offset, static_cast<uint16_t>(width * 32), 0, 0, { 0, 0, 0 } });
        offset = static_cast<uint16_t>(offset + width * 32);
    }
    spec.recordSize = offset / 8;

    Wdc5Section section;
    section.strings.push_back(0);
    size_t recordBytes = rows.size() * spec.recordSize;
    for (uint32_t r = 0; r < rows.size(); r++) {
        std::vector<uint32_t> values = rows[r].values;
        values.resize(offset / 32, 0);
        for (const auto& [column, text] : rows[r].strings) {
            size_t field = r * spec.recordSize + firstValue[column] * 4;
            values[firstValue[column]] = static_cast<uint32_t>(recordBytes + section.strings.size() - field);
            section.strings.insert(section.strings.end(), text.begin(), text.end());
            section.strings.push_back(0);
        }
        section.ids.push_back(rows[r].id);
        for (uint32_t value : values) {
            AppendU32(section.records, value);
        }
        if (rows[r].relation) {
            section.relations.push_back({ rows[r].relation, r });
        }
    }
    spec.sections = { section };

    Retail::Db2Table table;
    std::string error;
    bool parsed = table.Parse(BuildWdc5(spec), error);
    CHECK(parsed);
    return table;
}

inline uint32_t One() {
    return FloatBits(1.0f);
}

// Creature: Name 0, Title 2, Classification 4, CreatureType 5, CreatureFamily 6, DisplayID[4] 8, DisplayProbability[4] 9.
inline TextRow CreatureRow(uint32_t id, const std::string& name, uint32_t classification, uint32_t type, uint32_t family,
    std::vector<uint32_t> displays) {
    std::vector<uint32_t> values(8, 0);
    values[4] = classification;
    values[5] = type;
    values[6] = family;
    displays.resize(4, 0);
    values.insert(values.end(), displays.begin(), displays.end());
    for (uint32_t display : displays) {
        values.push_back(display ? One() : 0);
    }
    return { id, values, { { 0, name } } };
}

struct BrowserFixture {
    Retail::Db2Table creatures;
    Retail::Db2Table items;
    Retail::Db2Table searchNames;
    Retail::Db2Table modifiedAppearances;
    Retail::Db2Table appearances;
    Retail::Db2Table transmogSets;
    Retail::Db2Table transmogSetItems;
    Retail::Db2Table itemSets;
    Retail::Db2Table mounts;
    Retail::Db2Table mountDisplays;
    Retail::Db2Table creatureTypes;
    Retail::Db2Table creatureFamilies;
    Retail::Db2Table itemSubClasses;
    std::map<uint32_t, Retail::CreatureDisplayFacts> creatureDisplays;
    std::map<uint32_t, Retail::ItemDisplayFacts> itemDisplays;

    Retail::BrowserTables Tables() const {
        return { creatures, items, searchNames, modifiedAppearances, appearances, transmogSets, transmogSetItems, itemSets, mounts,
            mountDisplays, creatureTypes, creatureFamilies, itemSubClasses };
    }

    Retail::BrowserLookups Lookups() const {
        return {
            [this](uint32_t id, Retail::CreatureDisplayFacts& facts) {
                auto found = this->creatureDisplays.find(id);
                return found != this->creatureDisplays.end() && (facts = found->second, true);
            },
            [this](uint32_t id, Retail::ItemDisplayFacts& facts) {
                auto found = this->itemDisplays.find(id);
                return found != this->itemDisplays.end() && (facts = found->second, true);
            },
        };
    }
};

// Item: ClassID 0, SubclassID 1, Material 2, InventoryType 3, SheatheType 4, IconFileDataID 6.
inline TextRow ItemRow(uint32_t id, uint32_t classId, uint32_t subclass, uint32_t material, uint32_t inventoryType, uint32_t icon, uint32_t sheathe = 0) {
    return { id, { classId, subclass, material, inventoryType, sheathe, 255, icon } };
}

// ItemSearchName: Display 0, OverallQualityID 1, ExpansionID 2, AllowableClass 5, ItemLevel 10.
inline TextRow SearchNameRow(uint32_t id, const std::string& name, uint32_t quality, uint32_t expansion, uint32_t allowableClass, uint32_t level) {
    return { id, { 0, quality, expansion, 0, 0, allowableClass, 0, 0, 0, 0, level }, { { 0, name } } };
}

// ItemModifiedAppearance: ID 0, ItemID 1 (the relation too), modifier 2, ItemAppearanceID 3, OrderIndex 4.
inline TextRow AppearanceOfItem(uint32_t id, uint32_t item, uint32_t appearance) {
    return { id, { id, item, 0, appearance, 0, 5, 0 }, {}, item };
}

// ItemAppearance: DisplayType 0, ItemDisplayInfoID 1, DefaultIconFileDataID 2.
inline TextRow AppearanceRow(uint32_t id, uint32_t display, uint32_t icon) {
    return { id, { 11, display, icon } };
}

// Stormwind Guard (humanoid NPC, Extra 130617), Ghostpaw Runner (wolf), Nagidna (rare); Thunderfury and Dal'Rend's
// two blades (ItemSet 41); the Wild Combatant's Dreadplate chest and gauntlets (TransmogSet 17, death knight);
// the Brown Horse mount.
inline BrowserFixture MakeBrowserFixture() {
    BrowserFixture f;
    f.creatures = TextTable(Retail::CREATURE_LAYOUT, { 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 3 }, {
        CreatureRow(3823, "Ghostpaw Runner", 0, 1, 1, { 802 }),
        CreatureRow(85555, "Nagidna", 4, 1, 68, { 55833 }),
        CreatureRow(92123, "Stormwind Guard", 0, 7, 0, { 3258 }),
        CreatureRow(178184, "Stormwind Guard Captain", 1, 7, 0, { 3167, 3258 }),
        CreatureRow(178185, "Elite of Stormwind", 1, 7, 0, { 5446 }),
    });
    f.creatureDisplays[802] = { 43, 126487, 1, 0, 0, 0, 0, 0 };
    f.creatureDisplays[3258] = { 7661, 1011653, 1, 130617, 1, 0, 157058, 157059 };
    f.creatureDisplays[3167] = { 7661, 1011653, 1, 0, 0, 0, 0, 0 };
    f.creatureDisplays[2404] = { 216, 123456, 1, 0, 0, 0, 0, 0 };

    f.items = TextTable(Retail::ITEM_LAYOUT, std::vector<uint32_t>(15, 1), {
        ItemRow(12939, 2, 7, 1, 22, 135350),
        ItemRow(12940, 2, 7, 1, 21, 135353),
        ItemRow(19019, 2, 7, 1, 13, 135349, 1),
        ItemRow(124913, 4, 4, 6, 5, 0),
        ItemRow(124914, 4, 4, 6, 10, 0),
    });
    std::vector<uint32_t> searchWidths(11, 1);
    searchWidths.push_back(5);
    searchWidths.push_back(2);
    f.searchNames = TextTable(Retail::ITEM_SEARCH_NAME_LAYOUT, searchWidths, {
        SearchNameRow(12939, "Dal'Rend's Tribal Guardian", 3, 0, 0xFFFFFFFF, 29),
        SearchNameRow(12940, "Dal'Rend's Sacred Charge", 3, 0, 0xFFFFFFFF, 29),
        SearchNameRow(19019, "Thunderfury, Blessed Blade of the Windseeker", 5, 0, 0xFFFFFFFF, 29),
        SearchNameRow(124913, "Wild Combatant's Dreadplate Chestpiece", 3, 5, 32, 44),
        SearchNameRow(124914, "Wild Combatant's Dreadplate Gauntlets", 3, 5, 32, 44),
    });
    f.modifiedAppearances = TextTable(Retail::ITEM_MODIFIED_APPEARANCE_LAYOUT, std::vector<uint32_t>(7, 1), {
        AppearanceOfItem(4595, 12939, 3668),
        AppearanceOfItem(4596, 12940, 3669),
        AppearanceOfItem(7545, 19019, 5353),
        AppearanceOfItem(70584, 124913, 25923),
        AppearanceOfItem(70585, 124914, 25924),
    });
    f.appearances = TextTable(Retail::ITEM_APPEARANCE_LAYOUT, std::vector<uint32_t>(5, 1), {
        AppearanceRow(3668, 25647, 135350),
        AppearanceRow(3669, 25646, 135353),
        AppearanceRow(5353, 30606, 135349),
        AppearanceRow(25923, 142221, 1091986),
        AppearanceRow(25924, 142222, 1091987),
    });
    f.itemDisplays[30606] = { { 145000, 0 }, 135349 };
    f.itemDisplays[142221] = { { 0, 0 }, 1091986 };

    // TransmogSet: Name 0, ID 1, ClassMask 2, ExpansionID 9. TransmogSetItem: TransmogSetID 0 (the relation),
    // ItemModifiedAppearanceID 1.
    f.transmogSets = TextTable(Retail::TRANSMOG_SET_LAYOUT, std::vector<uint32_t>(13, 1), {
        { 17, { 0, 17, 32, 43004, 276, 12, 13302, 90, 1, 5 }, { { 0, "Wild Combatant's Dreadplate Armor" } } },
    });
    f.transmogSetItems = TextTable(Retail::TRANSMOG_SET_ITEM_LAYOUT, { 1, 1, 1 }, {
        { 900, { 17, 70584, 0 }, {}, 17 },
        { 901, { 17, 70585, 0 }, {}, 17 },
    });
    // ItemSet: Name 0, ItemID[17] 4.
    std::vector<uint32_t> setItems = { 0, 0, 0, 0, 12940, 12939 };
    setItems.resize(4 + 17, 0);
    f.itemSets = TextTable(Retail::ITEM_SET_LAYOUT, { 1, 1, 1, 1, 17 }, { { 41, setItems, { { 0, "Dal'Rend's Arms" } } } });

    // Mount: Name 0, ID 3, MountTypeID 4, SourceSpellID 7. MountXDisplay: CreatureDisplayInfoID 0, related to the mount.
    f.mounts = TextTable(Retail::MOUNT_LAYOUT, std::vector<uint32_t>(13, 1), {
        { 6, { 0, 0, 0, 6, 230, 0, 2, 458 }, { { 0, "Brown Horse" } } },
    });
    f.mountDisplays = TextTable(Retail::MOUNT_X_DISPLAY_LAYOUT, { 1, 1, 1 }, { { 1, { 2404, 0, 0 }, {}, 6 } });

    f.creatureTypes = TextTable(Retail::CREATURE_TYPE_LAYOUT, { 1, 1 }, {
        { 1, { 0, 0 }, { { 0, "Beast" } } },
        { 7, { 0, 0 }, { { 0, "Humanoid" } } },
    });
    f.creatureFamilies = TextTable(Retail::CREATURE_FAMILY_LAYOUT, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 2 }, { { 1, { 0 }, { { 0, "Wolf" } } } });
    // ItemSubClass: DisplayName 0, ClassID 3 (the relation), SubClassID 4.
    f.itemSubClasses = TextTable(Retail::ITEM_SUB_CLASS_LAYOUT, std::vector<uint32_t>(11, 1), {
        { 23, { 0, 0, 23, 4, 4 }, { { 0, "Plate" } }, 4 },
        { 10, { 0, 0, 10, 2, 7 }, { { 0, "Swords" } }, 2 },
    });
    return f;
}

inline Retail::RetailIndex BuildFixtureIndex(const BrowserFixture& fixture) {
    Retail::RetailIndex index;
    std::string error;
    bool built = index.Build(fixture.Tables(), fixture.Lookups(), error);
    CHECK(built);
    if (!built) {
        std::printf("    (index: %s)\n", error.c_str());
    }
    return index;
}

#endif
