#include <string>
#include "Engine/Source/Retail/ItemArmour.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/ItemArmourFixture.h"

using namespace Retail;

// Retail armour and weapon damage: the formulas of ItemArmour.h on the real tables, against wowhead's tooltips for real
// items at the item level wowhead shows them at (scratchpad armour\check.py).

static ArmourTables RealArmour() {
    return RealArmourTables();
}

static SparseItem Item(uint32_t quality, uint32_t inventoryType, uint32_t delay = 0, float variance = 0, bool caster = false) {
    SparseItem item;
    item.quality = quality;
    item.inventoryType = inventoryType;
    item.delay = delay;
    item.damageVariance = variance;
    item.casterWeapon = caster;
    return item;
}

static uint32_t ArmourAt(const ArmourTables& tables, uint32_t quality, uint32_t inventoryType, uint32_t subclass, uint32_t itemLevel) {
    return ComputeRetailArmour(tables, Item(quality, inventoryType), { 4, subclass }, itemLevel);
}

static bool DamageIs(const RetailDamage& damage, uint32_t min, uint32_t max) {
    bool same = damage.min == min && damage.max == max;
    if (!same) {
        std::printf("    (damage %u - %u, expected %u - %u)\n", damage.min, damage.max, min, max);
    }
    return same;
}

TEST(ArmourTablesReadEveryLevelAndSlot) {
    ArmourTables tables = RealArmour();
    CHECK(tables.Ready());
    CHECK(tables.quality[40][0] > 0.89f && tables.quality[40][0] < 0.91f && tables.quality[40][4] == 1.0f);
    CHECK(tables.total.size() == 10 && tables.total[200][3] > tables.total[200][0]);
    CHECK(tables.location.count(5) && tables.location.count(16) && !tables.location.count(2));
    CHECK(tables.shield.count(50) && tables.damage[static_cast<size_t>(DamageTable::TwoHandCaster)].count(70));

    Db2Table other = PlainTable(0x12345678, { 7 }, { { 1, { 0, 0, 0, 0, 0, 0, 0 } } });
    Db2Table total = RealArmorTotal();
    Db2Table location = RealArmorLocation();
    Db2Table shield = RealArmorShield();
    Db2Table damage = RealDamageOneHand();
    std::string error;
    CHECK(!ReadArmourTables({ other, total, location, shield, { &damage, &damage, &damage, &damage, &damage } }, tables, error));
    CHECK(error.find("ItemArmorQuality") != std::string::npos);
}

TEST(RetailArmourMatchesWowheadForArmourTypesAndSlots) {
    ArmourTables tables = RealArmour();
    CHECK(ArmourAt(tables, 4, 5, 4, 40) == 28);     // Judgment Breastplate
    CHECK(ArmourAt(tables, 4, 5, 4, 44) == 30);     // Gladiator's Scaled Chestpiece
    CHECK(ArmourAt(tables, 4, 7, 3, 321) == 232);   // Galerider's Chausses
    CHECK(ArmourAt(tables, 2, 5, 4, 74) == 56);     // Heroic Armor
    CHECK(ArmourAt(tables, 2, 5, 4, 11) == 15);     // Greenstone Breastplate
    CHECK(ArmourAt(tables, 4, 10, 1, 50) == 5);     // Malevolent Gladiator's Felweave Handguards
    CHECK(ArmourAt(tables, 2, 10, 2, 80) == 15);    // Cragwood Clutches
    CHECK(ArmourAt(tables, 3, 16, 1, 13) == 3);     // Onyxia Scale Cloak
}

TEST(RetailArmourOfARobeReadsTheChestRow) {
    ArmourTables tables = RealArmour();
    CHECK(ArmourAt(tables, 4, 20, 1, 49) == 9);     // Robes of Dying Light
    CHECK(ArmourAt(tables, 4, 20, 1, 49) == ArmourAt(tables, 4, 5, 1, 49));
}

TEST(RetailShieldArmourReadsItemArmorShield) {
    ArmourTables tables = RealArmour();
    CHECK(ArmourAt(tables, 4, 14, 6, 50) == 100);   // Bulwark of the Fallen General
    CHECK(ArmourAt(tables, 3, 14, 6, 33) == 75);    // Silvermoon Crest Shield
    CHECK(ArmourAt(tables, 2, 14, 6, 80) == 175);   // Royal Treasury Courier's Shield
}

TEST(RetailArmourIsZeroWithoutAnArmourType) {
    ArmourTables tables = RealArmour();
    CHECK(ArmourAt(tables, 4, 2, 0, 40) == 0);      // A necklace: miscellaneous.
    CHECK(ArmourAt(tables, 4, 5, 4, 41) == 0);      // A level the fixture lacks.
    CHECK(ComputeRetailArmour(tables, Item(4, 13), { 2, 7 }, 40) == 0);   // A weapon.
    CHECK(ArmourAt(tables, 8, 5, 4, 40) == 0);      // Past artifact.
}

TEST(RetailDamageMatchesWowheadForOneHandWeapons) {
    ArmourTables tables = RealArmour();
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(5, 13, 2600, 0.5f), { 2, 7 }, 40), 9, 16));             // Thunderfury
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(4, 13, 1800, 0.5f, true), { 2, 15 }, 259), 35, 60));    // Void-Touched Sacrificial Knife
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(3, 13, 2600, 0.7f, true), { 2, 0 }, 340), 95, 199));    // Venom-Etched Crescent
    RetailDamage thunderfury = ComputeRetailDamage(tables, Item(5, 13, 2600, 0.5f), { 2, 7 }, 40);
    CHECK(thunderfury.dps > 4.79f && thunderfury.dps < 4.81f);
}

TEST(RetailDamageMatchesWowheadForTwoHandAndRangedWeapons) {
    ArmourTables tables = RealArmour();
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(2, 17, 3600, 0.7f), { 2, 1 }, 79), 30, 64));            // Arachnophobic Spelunker's Battleaxe
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(2, 17, 3600, 0.3f, true), { 2, 10 }, 70), 18, 25));     // Suffused Staff (caster)
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(2, 15, 3000, 0.3f), { 2, 2 }, 80), 33, 46));            // Ven'jashi's Bow
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(3, 26, 2000, 0.5f, true), { 2, 19 }, 16), 1, 3));       // Inquisitor's Wand
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(3, 26, 2000, 0.5f, true), { 2, 19 }, 246), 35, 59));    // Wand of Ahn'kahet
}

TEST(RetailDamageOfHeirloomsReadsRareAndKeepsAtLeastOne) {
    ArmourTables tables = RealArmour();
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(7, 17, 3600, 0.7f), { 2, 1 }, 1), 1, 3));     // Bloodied Arcanite Reaper
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(4, 26, 1800, 0.0f), { 2, 3 }, 1), 1, 1));     // Galactic Gladiator's Blaster
}

TEST(RetailDamageIsZeroForNonWeapons) {
    ArmourTables tables = RealArmour();
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(4, 5, 0, 0), { 4, 4 }, 40), 0, 0));
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(4, 26, 3000, 0.5f), { 2, 16 }, 40), 0, 0));   // Thrown is not in a table.
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(4, 13, 2600, 0.5f), { 2, 7 }, 41), 0, 0));    // A level the fixture lacks.
    CHECK(DamageIs(ComputeRetailDamage(tables, Item(4, 13, 0, 0.5f), { 2, 7 }, 40), 0, 0));       // No delay.
}

// ReadSparseItem: Flags[1] 0x200 (ITEM_FLAG2_CASTER_WEAPON) and ContentTuningID (column 30).
TEST(SparseItemsReadTheCasterFlagAndContentTuning) {
    PlainSparse row;
    row.flags2 = 0x2200;
    row.contentTuning = 2905;
    Db2Table table = PlainTable(ITEM_SPARSE_LAYOUT, PlainSparseWidths(), { { 204738, PlainSparseValues(row) } });
    SparseItem item;
    CHECK(ReadSparseItem(table, 0, item));
    CHECK(item.casterWeapon && item.contentTuning == 2905);
}
