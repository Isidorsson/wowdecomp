#ifndef TOOLS_TESTS_ITEMARMOURFIXTURE_H
#define TOOLS_TESTS_ITEMARMOURFIXTURE_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "Engine/Source/Retail/ItemArmour.h"
#include "Tools/Tests/Db2Fixtures.h"

// Real rows of the pinned 12.1.5 armour and weapon damage tables (scratchpad armour: the tables read from CASC with the
// worktree's Db2Table, dump.exe CSVs) at the item levels the test items are checked at, as tables of plain 32-bit
// columns in the real layouts. Floats are kept as their bits. ItemArmorQuality has one row (0.9, 0.95, then 1) copied
// to every item level; here it is copied to the levels used.

inline Retail::Db2Table RealArmorQuality() {
    return PlainTable(Retail::ITEM_ARMOR_QUALITY_LAYOUT, { 7 }, {
        { 1, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 11, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 13, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 40, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 44, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 49, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 50, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 74, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 80, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 200, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
        { 321, { 1063675494, 1064514355, 1065353216, 1065353216, 1065353216, 1065353216, 1065353216 } },
    });
}

inline Retail::Db2Table RealArmorTotal() {
    return PlainTable(Retail::ITEM_ARMOR_TOTAL_LAYOUT, { 1, 1, 1, 1, 1 }, {
        { 11, { 11, 1103369481, 1109940964, 1115177436, 1119533053 } },
        { 13, { 13, 1104593333, 1110991756, 1116205902, 1120698462 } },
        { 40, { 40, 1111134720, 1117794350, 1122776803, 1127328059 } },
        { 44, { 44, 1112013327, 1118548718, 1123889197, 1128164710 } },
        { 49, { 49, 1113306559, 1119659079, 1124800007, 1129396186 } },
        { 50, { 50, 1113594533, 1119906332, 1124982307, 1129670409 } },
        { 74, { 74, 1119405974, 1126082199, 1131016830, 1135604917 } },
        { 80, { 80, 1120112185, 1126688548, 1131910956, 1136277404 } },
        { 200, { 200, 1133784450, 1140749053, 1144757031, 1150097939 } },
        { 321, { 321, 1143169146, 1150043714, 1154406775, 1159435070 } },
    });
}

inline Retail::Db2Table RealArmorLocation() {
    return PlainTable(Retail::ARMOR_LOCATION_LAYOUT, { 1, 1, 1, 1, 1 }, {
        { 1, { 1039516303, 1039516303, 1039516303, 1039516303, 1039516303 } },
        { 3, { 1038174126, 1038174126, 1038174126, 1038174126, 1040522936 } },
        { 5, { 1042536202, 1042536202, 1042536202, 1042536202, 1042536202 } },
        { 6, { 1035489772, 1035489772, 1035489772, 1035489772, 1035489772 } },
        { 7, { 1041194025, 1041194025, 1041194025, 1041194025, 1041194025 } },
        { 8, { 1036831949, 1036831949, 1036831949, 1036831949, 1038174126 } },
        { 9, { 1034147594, 1034147594, 1034147594, 1034147594, 1036831949 } },
        { 10, { 1035489772, 1035489772, 1035489772, 1035489772, 1032805417 } },
        { 16, { 1038174126, 1038174126, 1038174126, 1038174126, 1034147594 } },
    });
}

inline Retail::Db2Table RealArmorShield() {
    return PlainTable(Retail::ITEM_ARMOR_SHIELD_LAYOUT, { 7, 1 }, {
        { 33, { 1116108571, 1116598144, 1117087717, 1117087717, 1117087717, 1117087717, 1117087717, 33 } },
        { 50, { 1119098217, 1119753882, 1120409546, 1120409546, 1120409546, 1120409546, 1120409546, 50 } },
        { 80, { 1125983378, 1126555518, 1127127658, 1127127658, 1127127658, 1127127658, 1127127658, 80 } },
        { 200, { 1139680401, 1140547452, 1141132596, 1141132596, 1141132596, 1141132596, 1141132596, 200 } },
    });
}

inline Retail::Db2Table RealDamageTwoHand() {
    return PlainTable(Retail::ITEM_DAMAGE_LAYOUT, { 1, 7 }, {
        { 1, { 1, 1050949844, 1057230805, 1057544710, 1057544710, 1057544710, 1057544710, 1057544710 } },
        { 79, { 79, 1090337996, 1095476520, 1095960575, 1095960575, 1095960575, 1095960575, 1095960575 } },
        { 80, { 80, 1090493353, 1095601453, 1096090040, 1096090040, 1096090040, 1096090040, 1096090040 } },
        { 200, { 200, 1103329868, 1109209688, 1109583338, 1109583338, 1109583338, 1109583338, 1109583338 } },
    });
}

inline Retail::Db2Table RealDamageTwoHandCaster() {
    return PlainTable(Retail::ITEM_DAMAGE_LAYOUT, { 1, 7 }, {
        { 70, { 70, 1080273533, 1085740246, 1086175422, 1086175422, 1086175422, 1086175422, 1086175422 } },
        { 200, { 200, 1094941260, 1100821080, 1101194730, 1101194730, 1101194730, 1101194730, 1101194730 } },
    });
}

inline Retail::Db2Table RealDamageOneHand() {
    return PlainTable(Retail::ITEM_DAMAGE_LAYOUT, { 1, 7 }, {
        { 40, { 40, 1077430448, 1083453931, 1083806184, 1083806184, 1083806184, 1083806184, 1083806184 } },
        { 200, { 200, 1100192093, 1106076534, 1106640795, 1106640795, 1106640795, 1106640795, 1106640795 } },
    });
}

inline Retail::Db2Table RealDamageOneHandCaster() {
    return PlainTable(Retail::ITEM_DAMAGE_LAYOUT, { 1, 7 }, {
        { 16, { 16, 1061165270, 1067088480, 1067455667, 1067455667, 1067455667, 1067455667, 1067455667 } },
        { 200, { 200, 1091803485, 1097687926, 1098252187, 1098252187, 1098252187, 1098252187, 1098252187 } },
        { 246, { 246, 1096979651, 1102460287, 1102893389, 1102893389, 1102893389, 1102893389, 1102893389 } },
        { 259, { 259, 1098891759, 1103997941, 1104486813, 1104486813, 1104486813, 1104486813, 1104486813 } },
        { 340, { 340, 1107820501, 1113242487, 1113762403, 1113762403, 1113762403, 1113762403, 1113762403 } },
    });
}

inline Retail::Db2Table RealDamageAmmo() {
    return PlainTable(Retail::ITEM_DAMAGE_LAYOUT, { 1, 7 }, {
        { 200, { 200, 1110875427, 1114767345, 1115082906, 1116172787, 1117080025, 1117080025, 1117080025 } },
    });
}

// Every table above as ReadArmourTables gives them.
inline Retail::ArmourTables RealArmourTables() {
    Retail::Db2Table quality = RealArmorQuality();
    Retail::Db2Table total = RealArmorTotal();
    Retail::Db2Table location = RealArmorLocation();
    Retail::Db2Table shield = RealArmorShield();
    Retail::Db2Table ammo = RealDamageAmmo();
    Retail::Db2Table one = RealDamageOneHand();
    Retail::Db2Table oneCaster = RealDamageOneHandCaster();
    Retail::Db2Table two = RealDamageTwoHand();
    Retail::Db2Table twoCaster = RealDamageTwoHandCaster();
    Retail::ArmourTables tables;
    std::string error;
    bool read = Retail::ReadArmourTables({ quality, total, location, shield, { &ammo, &one, &oneCaster, &two, &twoCaster } }, tables, error);
    CHECK(read);
    if (!read) {
        std::printf("    (armour tables: %s)\n", error.c_str());
    }
    return tables;
}

// An ItemSparse row of layout 1C17D17F as plain columns for PlainTable (no strings, no stats): 0 but what is set.
struct PlainSparse {
    uint32_t quality = 0;
    uint32_t inventoryType = 0;
    uint32_t itemLevel = 0;
    uint32_t delay = 0;
    float variance = 0;
    uint32_t damageType = 0;
    uint32_t flags2 = 0;
    uint32_t contentTuning = 0;
    uint32_t levelCurve = 0;
};

inline std::vector<uint32_t> PlainSparseWidths() {
    std::vector<uint32_t> widths(67, 1);
    widths[14] = widths[15] = widths[16] = 10;
    widths[21] = widths[42] = 2;
    widths[27] = 5;
    widths[55] = 3;
    return widths;
}

inline uint32_t PlainSparseValue(const PlainSparse& row, size_t column, uint32_t element) {
    uint32_t variance;
    std::memcpy(&variance, &row.variance, sizeof(variance));
    switch (column) {
        case 6: return variance;
        case 16: return 0xFFFFFFFF;   // No stat types.
        case 27: return element == 1 ? row.flags2 : 0;
        case 30: return row.contentTuning;
        case 31: return row.levelCurve;
        case 46: return row.delay;
        case 50: return row.itemLevel;
        case 60: return row.damageType;
        case 65: return row.inventoryType;
        case 66: return row.quality;
        default: return 0;
    }
}

inline std::vector<uint32_t> PlainSparseValues(const PlainSparse& row) {
    std::vector<uint32_t> widths = PlainSparseWidths();
    std::vector<uint32_t> values;
    for (size_t column = 0; column < widths.size(); column++) {
        for (uint32_t i = 0; i < widths[column]; i++) {
            values.push_back(PlainSparseValue(row, column, i));
        }
    }
    return values;
}

#endif
