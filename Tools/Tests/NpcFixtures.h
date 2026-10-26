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

// Ext 152002 (displays 107058 and 113594, the Renewed Proto-Drake, race 72 on ChrModel 124) as the pinned build has
// it: its SD bake, its 21 customization choices and what they show. Three choices show something only with another
// choice: the skin with 19602 (chosen) or 19603, the horns' material with 28285 (chosen) or 28286, the eyes with 20113
// (chosen), 20114 or 20115.
inline NpcRows ProtoDrake152002() {
    NpcRows rows;
    rows.extras = { { 152002, 72, 0, 0, 0, 764468, 0 } };
    // Option row id, option, choice.
    const uint32_t chosen[][3] = { { 698487, 1611, 19612 }, { 698488, 1612, 19602 }, { 698489, 1659, 19912 }, { 698490, 1660, 19913 },
        { 698491, 1662, 19939 }, { 698492, 1664, 19954 }, { 698493, 1666, 19965 }, { 698494, 1667, 19966 }, { 698495, 1668, 19971 },
        { 698496, 1669, 19978 }, { 698497, 1670, 19981 }, { 698498, 1671, 19983 }, { 698499, 1672, 19985 }, { 698500, 1673, 19988 },
        { 698501, 1674, 19994 }, { 698502, 1675, 20014 }, { 698503, 1676, 20009 }, { 698504, 1680, 20066 }, { 698505, 1681, 20070 },
        { 698506, 1682, 20113 }, { 698507, 1848, 28285 } };
    for (const auto& option : chosen) {
        rows.options.push_back({ option[0], 152002, option[1], option[2] });
        rows.choices.push_back({ option[2], 0 });
    }
    rows.elements = { { 79579, 19612, 19602, 0, 57947 }, { 79580, 19612, 19603, 0, 57953 }, { 80048, 19912, 0, 6268, 0 },
        { 80049, 19913, 0, 6269, 0 }, { 80075, 19939, 0, 6287, 0 }, { 80090, 19954, 0, 6310, 0 }, { 80101, 19965, 0, 6315, 0 },
        { 80102, 19966, 0, 6320, 0 }, { 80107, 19971, 0, 6325, 0 }, { 80114, 19978, 0, 6332, 0 }, { 80119, 19983, 0, 6337, 0 },
        { 80121, 19985, 0, 6339, 0 }, { 80124, 19988, 0, 0, 58013 }, { 80130, 19994, 28285, 0, 58019 }, { 90497, 19994, 28286, 0, 58024 },
        { 80145, 20009, 0, 0, 58034 }, { 80277, 20070, 20113, 0, 56780 }, { 80278, 20070, 20115, 0, 56794 }, { 80279, 20070, 20114, 0, 56808 } };
    rows.geosets = { { 6268, 1, 1 }, { 6269, 2, 1 }, { 6287, 3, 0 }, { 6310, 4, 0 }, { 6315, 5, 0 }, { 6320, 6, 1 }, { 6325, 7, 0 },
        { 6332, 8, 0 }, { 6337, 39, 1 }, { 6339, 40, 0 } };
    rows.materials = { { 57947, 1, 752220 }, { 57953, 1, 752226 }, { 58013, 10, 752198 }, { 58019, 2, 780592 }, { 58024, 2, 752209 },
        { 58034, 16, 752193 }, { 56780, 25, 750787 }, { 56794, 25, 750800 }, { 56808, 25, 750814 } };
    rows.textures = { { 4278660, 752220, 0 }, { 4278666, 752226, 0 }, { 4278638, 752198, 0 }, { 4885868, 780592, 0 }, { 4278649, 752209, 0 },
        { 4278633, 752193, 0 }, { 4261013, 750787, 0 }, { 4261027, 750800, 0 }, { 4261041, 750814, 0 }, { 4547582, 764468, 0 } };
    return rows;
}

#endif
