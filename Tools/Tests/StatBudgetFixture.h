#ifndef TOOLS_TESTS_STATBUDGETFIXTURE_H
#define TOOLS_TESTS_STATBUDGETFIXTURE_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/ItemSparse.h"
#include "Engine/Source/Retail/StatBudget.h"

// Real budget rows of the pinned 12.1.5 build (scratchpad s2: RandPropPoints 1310245 through dump2.exe, the three
// gametables read raw from CASC) at the item levels wowhead shows the test items at, and the ItemSparse allocations
// of those items. Floats are kept as their bits.

struct RandPropBits { uint32_t itemLevel; uint32_t epic[5]; uint32_t superior[5]; uint32_t good[5]; };

inline std::vector<RandPropBits> RealRandPropRows() {
    return {
        { 13, { 1087637722, 1084163748, 1080986102, 1079249114, 1079249114 }, { 1087637722, 1084163748, 1080986102, 1079249114, 1079249114 }, { 1087637722, 1084163748, 1080986102, 1079249114, 1079249114 } },
        { 15, { 1090566587, 1086396056, 1083232498, 1082177979, 1082177979 }, { 1090566587, 1086396056, 1083232498, 1082177979, 1082177979 }, { 1090566587, 1086396056, 1083232498, 1082177979, 1082177979 } },
        { 29, { 1096423350, 1092850121, 1089821357, 1088034742, 1088034742 }, { 1096423350, 1092850121, 1089821357, 1088034742, 1088034742 }, { 1096423350, 1092850121, 1089821357, 1088034742, 1088034742 } },
        { 40, { 1098937104, 1094757528, 1091600754, 1090548496, 1090548496 }, { 1098937104, 1094757528, 1091600754, 1090548496, 1090548496 }, { 1098937104, 1094757528, 1091600754, 1090548496, 1090548496 } },
        { 44, { 1099541996, 1095664866, 1092281258, 1091153388, 1091153388 }, { 1099541996, 1095664866, 1092281258, 1091153388, 1091153388 }, { 1099541996, 1095664866, 1092281258, 1091153388, 1091153388 } },
        { 45, { 1099707412, 1095912990, 1092467351, 1091318804, 1091318804 }, { 1099707412, 1095912990, 1092467351, 1091318804, 1091318804 }, { 1099707412, 1095912990, 1092467351, 1091318804, 1091318804 } },
        { 47, { 1100056756, 1096437006, 1092860363, 1091668148, 1091668148 }, { 1100056756, 1096437006, 1092860363, 1091668148, 1091668148 }, { 1100056756, 1096437006, 1092860363, 1091668148, 1091668148 } },
        { 49, { 1100432343, 1097000386, 1093282898, 1092043735, 1092043735 }, { 1100432343, 1097000386, 1093282898, 1092043735, 1092043735 }, { 1100432343, 1097000386, 1093282898, 1092043735, 1092043735 } },
        { 50, { 1100630604, 1097297777, 1093505941, 1092241996, 1092241996 }, { 1100630604, 1097297777, 1093505941, 1092241996, 1092241996 }, { 1100630604, 1097297777, 1093505941, 1092241996, 1092241996 } },
        { 53, { 1101270548, 1098257693, 1094225878, 1092881940, 1092881940 }, { 1101270548, 1098257693, 1094225878, 1092881940, 1092881940 }, { 1101270548, 1098257693, 1094225878, 1092881940, 1092881940 } },
        { 60, { 1103068203, 1099930912, 1096248240, 1094679595, 1094679595 }, { 1103068203, 1099930912, 1096248240, 1094679595, 1094679595 }, { 1103068203, 1099930912, 1096248240, 1094679595, 1094679595 } },
        { 73, { 1106962351, 1102851524, 1099768403, 1098573743, 1098573743 }, { 1106962351, 1102851524, 1099768403, 1098573743, 1098573743 }, { 1106962351, 1102851524, 1099768403, 1098573743, 1098573743 } },
        { 200, { 1120780866, 1117409713, 1114077834, 1112392258, 1112392258 }, { 1120780866, 1117409713, 1114077834, 1112392258, 1112392258 }, { 1120780866, 1117409713, 1114077834, 1112392258, 1112392258 } },
        { 259, { 1127367833, 1124447091, 1120439596, 1118979225, 1118979225 }, { 1127367833, 1124447091, 1120439596, 1118979225, 1118979225 }, { 1127367833, 1124447091, 1120439596, 1118979225, 1118979225 } },
        { 289, { 1131135590, 1127272909, 1124375898, 1122746981, 1122746981 }, { 1131135590, 1127272909, 1124375898, 1122746981, 1122746981 }, { 1131135590, 1127272909, 1124375898, 1122746981, 1122746981 } },
        { 321, { 1134482439, 1131298315, 1127394952, 1126093831, 1126093831 }, { 1134482439, 1131298315, 1127394952, 1126093831, 1126093831 }, { 1134482439, 1131298315, 1127394952, 1126093831, 1126093831 } },
        { 340, { 1136498334, 1133392118, 1129662833, 1128109726, 1128109726 }, { 1136498334, 1133392118, 1129662833, 1128109726, 1128109726 }, { 1136498334, 1133392118, 1129662833, 1128109726, 1128109726 } },
        { 684, { 1175591658, 1171990623, 1168399943, 1167203050, 1167203050 }, { 1175591658, 1171990623, 1168399943, 1167203050, 1167203050 }, { 1175591658, 1171990623, 1168399943, 1167203050, 1167203050 } },
    };
}

inline std::string RealCombatRatingsText() {
    return
        "Item Level\tArmor Multiplier\tWeapon Multiplier\tTrinket Multiplier\tJewelry Multiplier\n"
        "13\t1.5\t1.5\t1.5\t1.5\n"
        "15\t1.5\t1.5\t1.5\t1.5\n"
        "29\t1.5\t1.5\t1.5\t1.5\n"
        "40\t1.5\t1.5\t1.5\t1.5\n"
        "44\t1.5\t1.5\t1.5\t1.5\n"
        "45\t1.5\t1.5\t1.5\t1.5\n"
        "47\t1.5\t1.5\t1.5\t1.5\n"
        "49\t1.5\t1.5\t1.5\t1.5\n"
        "50\t1.5\t1.5\t1.5\t1.5\n"
        "53\t1.5\t1.5\t1.5\t1.5\n"
        "60\t1.5\t1.5\t1.5\t1.5\n"
        "73\t1.5\t1.5\t1.5\t1.5\n"
        "200\t1.264861998\t1.264861998\t1.264861998\t1.526862049\n"
        "259\t1.12390728\t1.12390728\t1.12390728\t1.337756772\n"
        "289\t0.996836226\t0.996836226\t0.996836226\t1.30553802\n"
        "321\t0.856210407\t0.856210407\t0.856210407\t1.201698816\n"
        "340\t0.775179765\t0.775179765\t0.775179765\t1.122500569\n"
        "684\t0.495879298\t0.495879298\t0.495879298\t0.781009895\n";
}

inline std::string RealStaminaText() {
    return
        "Item Level\tArmor Multiplier\tWeapon Multiplier\tTrinket Multiplier\tJewelry Multiplier\n"
        "13\t1\t1\t1\t1\n"
        "15\t1\t1\t1\t1\n"
        "29\t1\t1\t1\t1\n"
        "40\t1\t1\t1\t1\n"
        "44\t1\t1\t1\t1\n"
        "45\t1\t1\t1\t1\n"
        "47\t1\t1\t1\t1\n"
        "49\t1\t1\t1\t1\n"
        "50\t1\t1\t1\t1\n"
        "53\t1\t1\t1\t1\n"
        "60\t1\t1\t1\t1\n"
        "73\t1\t1\t1\t1\n"
        "200\t9.486493557\t9.486493557\t9.486493557\t9.486493557\n"
        "259\t11.50073603\t11.50073603\t11.50073603\t11.50073603\n"
        "289\t12.5072263\t12.5072263\t12.5072263\t12.5072263\n"
        "321\t13.44269711\t13.44269711\t13.44269711\t13.44269711\n"
        "340\t13.99758184\t13.99758184\t13.99758184\t13.99758184\n"
        "684\t16.44259229\t16.44259229\t16.44259229\t16.44259229\n";
}

inline std::string RealSocketCostText() {
    return
        "5.0 Level\tSocket Cost\n"
        "13\t0\n"
        "15\t0\n"
        "29\t1\n"
        "37\t2\n"
        "39\t2\n"
        "40\t2\n"
        "44\t3\n"
        "45\t3\n"
        "47\t3\n"
        "49\t3\n"
        "50\t3\n"
        "53\t3\n"
        "60\t4\n"
        "73\t4\n"
        "200\t8\n"
        "259\t8\n"
        "289\t8\n"
        "321\t8\n"
        "340\t8\n"
        "684\t8\n";
}

// An item as ReadSparseItem gives it: id, inventory type, quality, stat types, allocations, socket costs.
inline Retail::SparseItem RealSparseItem(uint32_t id) {
    Retail::SparseItem item;
    item.id = id;
    switch (id) {
        case 19019:   // Thunderfury, Blessed Blade of the Windseeker, wowhead item level 40
            item.inventoryType = 13;
            item.quality = 5;
            item.itemLevel = 29;
            item.statTypes = { 3, 7, 51, 55, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 2000, 3200, 3200, 3600, 0, 0, 0, 0, 0, 0 };
            break;
        case 16958:   // Judgment Breastplate, wowhead item level 40
            item.inventoryType = 5;
            item.quality = 4;
            item.itemLevel = 29;
            item.statTypes = { 74, 7, 32, 51, 55, -1, -1, -1, -1, -1 };
            item.statPercents = { 4909, 7889, 2909, 1818, 1818, 0, 0, 0, 0, 0 };
            break;
        case 15138:   // Onyxia Scale Cloak, wowhead item level 13
            item.inventoryType = 16;
            item.quality = 3;
            item.itemLevel = 29;
            item.statTypes = { 7, 51, -1, -1, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 7889, 8421, 0, 0, 0, 0, 0, 0, 0, 0 };
            break;
        case 249370:   // Draconic Nullcape, wowhead item level 289
            item.inventoryType = 16;
            item.quality = 4;
            item.itemLevel = 219;
            item.statTypes = { 71, 7, 36, 49, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7889, 4707, 2293, 0, 0, 0, 0, 0, 0 };
            break;
        case 255271:   // Galactic Aspirant's Leather Gloves, wowhead item level 60
            item.inventoryType = 10;
            item.quality = 3;
            item.itemLevel = 197;
            item.statTypes = { 73, 7, 40, 49, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7889, 3950, 3050, 0, 0, 0, 0, 0, 0 };
            break;
        case 273780:   // Venom-Etched Crescent, wowhead item level 340
            item.inventoryType = 13;
            item.quality = 3;
            item.itemLevel = 219;
            item.statTypes = { 5, 7, 49, 32, -1, 5, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7889, 3700, 3300, 0, 25370, 0, 0, 0, 0 };
            break;
        case 263901:   // Void-Touched Sacrificial Knife, wowhead item level 259
            item.inventoryType = 13;
            item.quality = 4;
            item.itemLevel = 246;
            item.statTypes = { 5, 7, 40, 32, -1, 5, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7889, 4800, 2200, 0, 25370, 0, 0, 0, 0 };
            break;
        case 272251:   // Galerider's Chausses, wowhead item level 321
            item.inventoryType = 7;
            item.quality = 4;
            item.itemLevel = 206;
            item.statTypes = { 73, 7, 36, 40, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7889, 4150, 2850, 0, 0, 0, 0, 0, 0 };
            break;
        case 27879:   // Gladiator's Scaled Chestpiece, wowhead item level 44
            item.inventoryType = 5;
            item.quality = 4;
            item.itemLevel = 32;
            item.statTypes = { 7, 40, 32, 74, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 7889, 3506, 3506, 5259, 0, 0, 0, 0, 0, 0 };
            item.socketCosts = { 0.0f, 0.75f, 0.75f, 1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            item.sockets = { 7, 7, 7 };
            break;
        case 186376:   // Oscillating Ouroboros, wowhead item level 49
            item.inventoryType = 11;
            item.quality = 4;
            item.itemLevel = 226;
            item.statTypes = { -1, 7, 40, 49, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 0, 7889, 12980, 4520, 0, 0, 0, 0, 0, 0 };
            break;
        case 189854:   // Rygelon's Heraldric Ring, wowhead item level 50
            item.inventoryType = 11;
            item.quality = 4;
            item.itemLevel = 233;
            item.statTypes = { -1, 7, 49, 32, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 0, 7889, 14450, 3050, 0, 0, 0, 0, 0, 0 };
            break;
        case 189838:   // Beacon of Stormwind, wowhead item level 49
            item.inventoryType = 2;
            item.quality = 4;
            item.itemLevel = 226;
            item.statTypes = { -1, 7, 32, 49, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 0, 7889, 14000, 3500, 0, 0, 0, 0, 0, 0 };
            break;
        case 235422:   // Venture Co Seal, wowhead item level 53
            item.inventoryType = 11;
            item.quality = 4;
            item.itemLevel = 246;
            item.statTypes = { -1, 7, 32, 49, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 0, 7889, 11750, 5750, 0, 0, 0, 0, 0, 0 };
            break;
        case 203460:   // Onyx Annulet, wowhead item level 73
            item.inventoryType = 11;
            item.quality = 4;
            item.itemLevel = 405;
            item.statTypes = { 7, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 7889, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            item.sockets = { 25, 25, 25 };
            break;
        case 99883:   // Tyrannical Gladiator's Ornamented Headcover, wowhead item level 50
            item.inventoryType = 1;
            item.quality = 4;
            item.itemLevel = 39;
            item.statTypes = { 74, 7, 49, 32, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7889, 3506, 3506, 0, 0, 0, 0, 0, 0 };
            item.socketCosts = { 1.5f, 0.0f, 1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            item.sockets = { 1, 7, 0 };
            break;
        case 48341:   // Nobundo's Chestguard of Conquest, wowhead item level 45
            item.inventoryType = 5;
            item.quality = 4;
            item.itemLevel = 35;
            item.statTypes = { 73, 7, 32, 36, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 5259, 6224, 5259, 2766, 0, 0, 0, 0, 0, 0 };
            item.socketCosts = { 0.0f, 0.0f, 1.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            item.sockets = { 7, 7, 0 };
            break;
        case 48288:   // Nobundo's Legguards of Triumph, wowhead item level 45
            item.inventoryType = 7;
            item.quality = 4;
            item.itemLevel = 35;
            item.statTypes = { 73, 7, 32, 40, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7494, 3506, 3506, 0, 0, 0, 0, 0, 0 };
            item.socketCosts = { 1.0f, 0.0f, 0.375f, 0.625f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            item.sockets = { 7, 7, 0 };
            break;
        case 40810:   // Relentless Gladiator's Plate Gauntlets, wowhead item level 47
            item.inventoryType = 10;
            item.quality = 4;
            item.itemLevel = 35;
            item.statTypes = { 74, 7, 32, 40, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7889, 3506, 3506, 0, 0, 0, 0, 0, 0 };
            item.socketCosts = { 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            item.sockets = { 7, 0, 0 };
            break;
        case 88248:   // Gladiator's Scaled Helm, wowhead item level 15 (below its own 37)
            item.inventoryType = 1;
            item.quality = 3;
            item.itemLevel = 37;
            item.statTypes = { 74, 7, 49, 40, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 5259, 7889, 3506, 3506, 0, 0, 0, 0, 0, 0 };
            item.socketCosts = { 1.5f, 0.0f, 1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            item.sockets = { 1, 7, 0 };
            break;
        case 228842:   // Gobfather's Gifted Bling, wowhead item level 684
            item.inventoryType = 2;
            item.quality = 4;
            item.itemLevel = 623;
            item.statTypes = { -1, 7, 49, 36, -1, -1, -1, -1, -1, -1 };
            item.statPercents = { 0, 7889, 14250, 3250, 0, 0, 0, 0, 0, 0 };
            break;
    }
    return item;
}

#endif
