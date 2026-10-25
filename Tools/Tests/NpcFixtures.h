#ifndef TOOLS_TESTS_NPCFIXTURES_H
#define TOOLS_TESTS_NPCFIXTURES_H

#include <vector>
#include "Engine/Source/Retail/ItemTables.h"
#include "Engine/Source/Retail/RetailNpcDisplay.h"
#include "Tools/Tests/Db2Fixtures.h"

// The retail NPC tables with the columns RetailNpcTables reads at their physical numbers, and rows copied from the
// pinned build's CSV dumps.

struct NpcExtraRow { uint32_t id, race, sex, cls, flags, sdMaterial, hdMaterial; };
struct NpcItemRow { uint32_t extra, display, slot; };
struct NpcOptionRow { uint32_t id, extra, option, choice; };
struct NpcChoiceRow { uint32_t id, order; };
struct NpcElementRow { uint32_t id, choice, related, geoset, material; };
struct NpcGeosetRow { uint32_t id, type, geoset; };
struct NpcMaterialRow { uint32_t id, target, resource; };
struct NpcTextureRow { uint32_t file, material, usage; };

struct NpcRows {
    std::vector<NpcExtraRow> extras;
    std::vector<NpcItemRow> items;
    std::vector<NpcOptionRow> options;
    std::vector<NpcChoiceRow> choices;
    std::vector<NpcElementRow> elements;
    std::vector<NpcGeosetRow> geosets;
    std::vector<NpcMaterialRow> materials;
    std::vector<NpcTextureRow> textures;
};

// The tables, kept alive with the NpcTables view of them.
struct NpcTableSet {
    Retail::Db2Table extra, itemSlots, options, choices, elements, geosets, materials, textureFiles;

    Retail::NpcTables View() const { return { extra, itemSlots, options, choices, elements, geosets, materials, textureFiles }; }
};

inline NpcTableSet BuildNpcTables(const NpcRows& in) {
    NpcTableSet set;
    std::vector<Db2Row> rows;
    std::vector<uint32_t> relations;
    // CreatureDisplayInfoExtra: id (inline), race, sex, class, flags, SD and HD bake materials.
    for (const NpcExtraRow& r : in.extras) {
        rows.push_back({ r.id, { r.id, r.race, r.sex, r.cls, r.flags, r.sdMaterial, r.hdMaterial } });
    }
    set.extra = PlainTable(Retail::CREATURE_DISPLAY_INFO_EXTRA_LAYOUT, { 1, 1, 1, 1, 1, 1, 1 }, rows);

    rows.clear();
    for (uint32_t i = 0; i < in.items.size(); i++) {
        rows.push_back({ i + 1, { in.items[i].display, in.items[i].slot } });
        relations.push_back(in.items[i].extra);
    }
    set.itemSlots = PlainTable(Retail::NPC_MODEL_ITEM_SLOT_DISPLAY_INFO_LAYOUT, { 1, 1 }, rows, relations);

    rows.clear();
    relations.clear();
    for (const NpcOptionRow& r : in.options) {
        rows.push_back({ r.id, { r.option, r.choice } });
        relations.push_back(r.extra);
    }
    set.options = PlainTable(Retail::CREATURE_DISPLAY_INFO_OPTION_LAYOUT, { 1, 1 }, rows, relations);

    rows.clear();
    for (const NpcChoiceRow& r : in.choices) {
        std::vector<uint32_t> values(11, 0);
        values[5] = r.order;
        rows.push_back({ r.id, values });
    }
    set.choices = PlainTable(Retail::CHR_CUSTOMIZATION_CHOICE_LAYOUT, std::vector<uint32_t>(11, 1), rows);

    rows.clear();
    for (const NpcElementRow& r : in.elements) {
        std::vector<uint32_t> values(13, 0);
        values[0] = r.choice;
        values[1] = r.related;
        values[2] = r.geoset;
        values[4] = r.material;
        rows.push_back({ r.id, values });
    }
    set.elements = PlainTable(Retail::CHR_CUSTOMIZATION_ELEMENT_LAYOUT, std::vector<uint32_t>(13, 1), rows);

    rows.clear();
    for (const NpcGeosetRow& r : in.geosets) {
        rows.push_back({ r.id, { r.type, r.geoset, 0 } });
    }
    set.geosets = PlainTable(Retail::CHR_CUSTOMIZATION_GEOSET_LAYOUT, { 1, 1, 1 }, rows);

    rows.clear();
    for (const NpcMaterialRow& r : in.materials) {
        rows.push_back({ r.id, { r.target, r.resource } });
    }
    set.materials = PlainTable(Retail::CHR_CUSTOMIZATION_MATERIAL_LAYOUT, { 1, 1 }, rows);

    rows.clear();
    relations.clear();
    for (const NpcTextureRow& r : in.textures) {
        rows.push_back({ r.file, { r.file, r.usage, r.material } });
        relations.push_back(r.material);
    }
    set.textureFiles = PlainTable(Retail::TEXTURE_FILE_DATA_LAYOUT, { 1, 1, 1 }, rows, relations);
    return set;
}

// Ext 165251 (display 145747, human male, HD bake only) as the pinned build has it: its items, its 13 customization
// choices and what they show.
inline NpcRows HumanMale165251() {
    NpcRows rows;
    rows.extras = { { 165251, 1, 0, 1, 4, 0, 1121068 } };
    rows.items = { { 165251, 743700, 3 }, { 165251, 743701, 4 }, { 165251, 743702, 5 }, { 165251, 743703, 6 }, { 165251, 679414, 7 },
        { 165251, 679445, 8 } };
    rows.options = { { 1074492, 165251, 9, 10 }, { 1074493, 165251, 10, 15424 }, { 1074494, 165251, 11, 4995 }, { 1074495, 165251, 12, 73 },
        { 1074496, 165251, 13, 78 }, { 1074497, 165251, 463, 4134 }, { 1074498, 165251, 525, 5056 }, { 1074499, 165251, 888, 9910 },
        { 1074500, 165251, 889, 9913 }, { 1074501, 165251, 890, 9915 }, { 1074502, 165251, 6338, 45086 }, { 1074503, 165251, 8522, 54350 },
        { 1074504, 165251, 8789, 56651 } };
    rows.choices = { { 10, 9 }, { 73, 12 }, { 78, 2 }, { 4134, 8 }, { 4995, 24 }, { 5056, 0 }, { 9910, 4 }, { 9913, 0 }, { 9915, 0 },
        { 15424, 32 }, { 45086, 0 }, { 54350, 0 }, { 56651, 0 } };
    rows.elements = { { 2136, 10, 0, 0, 19 }, { 18742, 4134, 0, 0, 14408 }, { 24021, 4995, 0, 68, 0 }, { 24083, 5056, 0, 2083, 0 },
        { 24336, 73, 0, 0, 15391 }, { 51425, 78, 0, 92, 0 }, { 51435, 9910, 0, 73, 0 }, { 51438, 9913, 0, 362, 0 }, { 52157, 9915, 73, 0, 25639 },
        { 52158, 9915, 5127, 0, 25640 }, { 52496, 9915, 0, 2299, 0 }, { 52825, 10, 0, 0, 2 }, { 60189, 15424, 0, 0, 0 }, { 187523, 56651, 0, 13285, 0 } };
    rows.geosets = { { 68, 0, 25 }, { 2083, 32, 2 }, { 92, 1, 3 }, { 73, 3, 5 }, { 362, 2, 0 }, { 2299, 34, 0 }, { 13285, 7, 2 } };
    rows.materials = { { 19, 1, 151983 }, { 14408, 25, 669516 }, { 15391, 10, 679676 }, { 25639, 8, 684694 }, { 25640, 8, 684695 }, { 2, 13, 151950 } };
    rows.textures = { { 1027743, 151950, 0 }, { 1027776, 151983, 0 }, { 3484651, 669516, 0 }, { 3582353, 679676, 0 }, { 3685141, 684694, 0 },
        { 8046191, 1121068, 0 }, { 8046192, 1121068, 2 } };
    return rows;
}

#endif
