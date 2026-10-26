#ifndef ENGINE_SOURCE_RETAIL_ITEMARMOUR_H
#define ENGINE_SOURCE_RETAIL_ITEMARMOUR_H

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include "Engine/Source/Retail/Db2Table.h"
#include "Engine/Source/Retail/ItemSparse.h"

namespace Retail {

// Retail armour and weapon damage at an item level, from the pinned build's tables (TrinityCore master
// ItemTemplate::GetArmor, GetDPS and GetDamage; checked against wowhead tooltips, docs/RETAIL-BROWSER.md):
//   armour = int(ItemArmorQuality[ilvl].Qualitymod[q] * ItemArmorTotal[ilvl].<type> * ArmorLocation[slot].<type> + 0.5)
//   shield = int(ItemArmorShield[ilvl].Quality[q] + 0.5)
//   dps    = ItemDamage<table>[ilvl].Quality[q], the table by slot, subclass and the caster weapon flag
//   avg    = dps * ItemDelay / 1000; min = floor(avg * (1 - DmgVariance / 2)) (at least 1); max = floor(avg * (1 +
//            DmgVariance / 2) + 0.5)
// where q is the quality (heirloom reads rare) and a robe reads the chest's ArmorLocation row.
constexpr uint32_t ITEM_ARMOR_QUALITY_FDID = 1283021;
constexpr uint32_t ITEM_ARMOR_QUALITY_LAYOUT = 0x2935AA9D;
constexpr uint32_t ITEM_ARMOR_TOTAL_FDID = 1283022;
constexpr uint32_t ITEM_ARMOR_TOTAL_LAYOUT = 0xEB155D51;
constexpr uint32_t ITEM_ARMOR_SHIELD_FDID = 1277741;
constexpr uint32_t ITEM_ARMOR_SHIELD_LAYOUT = 0x7E6C94F9;
constexpr uint32_t ARMOR_LOCATION_FDID = 1284818;
constexpr uint32_t ARMOR_LOCATION_LAYOUT = 0xFB67352F;
constexpr uint32_t ITEM_DAMAGE_LAYOUT = 0x56F30531;   // Every ItemDamage table.

constexpr size_t ITEM_QUALITIES = 7;    // Poor to artifact.
constexpr size_t ARMOUR_TYPES = 4;      // Cloth, leather, mail, plate: armour subclasses 1 to 4.

enum class DamageTable : uint8_t { Ammo, OneHand, OneHandCaster, TwoHand, TwoHandCaster, Count };
constexpr size_t DAMAGE_TABLES = static_cast<size_t>(DamageTable::Count);
constexpr uint32_t ITEM_DAMAGE_FDIDS[DAMAGE_TABLES] = { 1277740, 1277743, 1277739, 1277738, 1277742 };

using QualityValues = std::array<float, ITEM_QUALITIES>;
using ArmourTypeValues = std::array<float, ARMOUR_TYPES>;

// Rows by item level, ArmorLocation's by inventory type.
struct ArmourTables {
    std::map<uint32_t, QualityValues> quality;
    std::map<uint32_t, ArmourTypeValues> total;
    std::map<uint32_t, ArmourTypeValues> location;
    std::map<uint32_t, QualityValues> shield;
    std::array<std::map<uint32_t, QualityValues>, DAMAGE_TABLES> damage;

    bool Ready() const { return !this->total.empty(); }
};

// The tables as read, in the order of the ArmourTables members (damage in DamageTable order).
struct ArmourTableFiles {
    const Db2Table& quality;
    const Db2Table& total;
    const Db2Table& location;
    const Db2Table& shield;
    std::array<const Db2Table*, DAMAGE_TABLES> damage;
};

bool ReadArmourTables(const ArmourTableFiles& files, ArmourTables& tables, std::string& error);

struct ItemClass {
    uint32_t classId = 0;    // Item.ClassID: 2 weapon, 4 armour.
    uint32_t subclass = 0;
};

// 0 for an item that is not cloth, leather, mail, plate or a shield, or a level or quality the tables lack.
uint32_t ComputeRetailArmour(const ArmourTables& tables, const SparseItem& item, ItemClass kind, uint32_t itemLevel);

struct RetailDamage {
    float dps = 0;       // The table's; the tooltip shows (min + max) / 2 / speed.
    uint32_t min = 0;
    uint32_t max = 0;
};

// All zero for an item that is not a weapon (or ammo), or a level or quality the tables lack.
RetailDamage ComputeRetailDamage(const ArmourTables& tables, const SparseItem& item, ItemClass kind, uint32_t itemLevel);

}

#endif
