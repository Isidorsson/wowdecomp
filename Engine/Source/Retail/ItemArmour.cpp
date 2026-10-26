#include "Engine/Source/Retail/ItemArmour.h"
#include <cmath>
#include <cstring>
#include <iterator>

namespace Retail {

// Physical columns (WoWDBDefs; ID is noninline and is the item level, or ArmorLocation's inventory type).
constexpr size_t QUALITY_MODS = 0;     // ItemArmorQuality: Qualitymod[7].
constexpr size_t TOTAL_CLOTH = 1;      // ItemArmorTotal: ItemLevel, Cloth, Leather, Mail, Plate.
constexpr size_t LOCATION_CLOTH = 0;   // ArmorLocation: Clothmodifier, Leathermodifier, Chainmodifier, Platemodifier, Modifier.
constexpr size_t SHIELD_QUALITY = 0;   // ItemArmorShield: Quality[7], ItemLevel.
constexpr size_t DAMAGE_QUALITY = 1;   // ItemDamage*: ItemLevel, Quality[7].

constexpr uint32_t CLASS_WEAPON = 2;
constexpr uint32_t CLASS_ARMOUR = 4;
constexpr uint32_t SUBCLASS_CLOTH = 1;
constexpr uint32_t SUBCLASS_PLATE = 4;
constexpr uint32_t SUBCLASS_SHIELD = 6;
constexpr uint32_t SUBCLASS_BOW = 2;
constexpr uint32_t SUBCLASS_GUN = 3;
constexpr uint32_t SUBCLASS_CROSSBOW = 18;
constexpr uint32_t SUBCLASS_WAND = 19;
constexpr uint32_t QUALITY_RARE = 3;
constexpr uint32_t QUALITY_HEIRLOOM = 7;
constexpr uint32_t INVENTORY_CHEST = 5;
constexpr uint32_t INVENTORY_ROBE = 20;

static const char* const DAMAGE_NAMES[DAMAGE_TABLES] = { "ItemDamageAmmo", "ItemDamageOneHand", "ItemDamageOneHandCaster",
    "ItemDamageTwoHand", "ItemDamageTwoHandCaster" };

static float AsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// N floats of a row: one array column, or N one-float columns from column on.
template <size_t N>
static std::array<float, N> Floats(const Db2Table& table, size_t row, size_t column, bool array) {
    std::vector<uint32_t> values = table.Values(row, column);
    std::array<float, N> into = {};
    for (size_t i = 0; i < N; i++) {
        uint32_t bits = array ? (i < values.size() ? values[i] : 0) : table.Value(row, column + i);
        into[i] = AsFloat(bits);
    }
    return into;
}

template <size_t N>
static bool ReadRows(const Db2Table& table, uint32_t layout, const char* name, size_t column, bool array,
    std::map<uint32_t, std::array<float, N>>& rows, std::string& error) {
    if (!KnownLayout(table, layout, name, error)) {
        return false;
    }
    rows.clear();
    for (size_t row = 0; row < table.RowCount(); row++) {
        rows[table.RowId(row)] = Floats<N>(table, row, column, array);
    }
    return true;
}

bool ReadArmourTables(const ArmourTableFiles& files, ArmourTables& tables, std::string& error) {
    if (!ReadRows(files.quality, ITEM_ARMOR_QUALITY_LAYOUT, "ItemArmorQuality", QUALITY_MODS, true, tables.quality, error) ||
        !ReadRows(files.total, ITEM_ARMOR_TOTAL_LAYOUT, "ItemArmorTotal", TOTAL_CLOTH, false, tables.total, error) ||
        !ReadRows(files.location, ARMOR_LOCATION_LAYOUT, "ArmorLocation", LOCATION_CLOTH, false, tables.location, error) ||
        !ReadRows(files.shield, ITEM_ARMOR_SHIELD_LAYOUT, "ItemArmorShield", SHIELD_QUALITY, true, tables.shield, error)) {
        return false;
    }
    for (size_t t = 0; t < DAMAGE_TABLES; t++) {
        if (!files.damage[t]) {
            error = std::string(DAMAGE_NAMES[t]) + " is missing";
            return false;
        }
        if (!ReadRows(*files.damage[t], ITEM_DAMAGE_LAYOUT, DAMAGE_NAMES[t], DAMAGE_QUALITY, true, tables.damage[t], error)) {
            return false;
        }
    }
    // ArmorLocation lists every inventory type; the ones without armour are all zero.
    for (auto row = tables.location.begin(); row != tables.location.end();) {
        bool none = row->second == ArmourTypeValues{};
        row = none ? tables.location.erase(row) : std::next(row);
    }
    return true;
}

// The quality column: heirlooms read rare; false past artifact.
static bool QualityIndex(uint32_t quality, size_t& index) {
    index = quality == QUALITY_HEIRLOOM ? QUALITY_RARE : quality;
    return index < ITEM_QUALITIES;
}

template <typename Row>
static const Row* Find(const std::map<uint32_t, Row>& rows, uint32_t key) {
    auto found = rows.find(key);
    return found != rows.end() ? &found->second : nullptr;
}

uint32_t ComputeRetailArmour(const ArmourTables& tables, const SparseItem& item, ItemClass kind, uint32_t itemLevel) {
    size_t quality;
    if (kind.classId != CLASS_ARMOUR || !QualityIndex(item.quality, quality)) {
        return 0;
    }
    if (kind.subclass == SUBCLASS_SHIELD) {
        const QualityValues* shield = Find(tables.shield, itemLevel);
        return shield ? static_cast<uint32_t>((*shield)[quality] + 0.5f) : 0;
    }
    if (kind.subclass < SUBCLASS_CLOTH || kind.subclass > SUBCLASS_PLATE) {
        return 0;
    }
    const QualityValues* mods = Find(tables.quality, itemLevel);
    const ArmourTypeValues* total = Find(tables.total, itemLevel);
    const ArmourTypeValues* location = Find(tables.location, item.inventoryType == INVENTORY_ROBE ? INVENTORY_CHEST : item.inventoryType);
    if (!mods || !total || !location) {
        return 0;
    }
    size_t type = kind.subclass - SUBCLASS_CLOTH;
    return static_cast<uint32_t>((*mods)[quality] * (*total)[type] * (*location)[type] + 0.5f);
}

// GetDPS's table: by inventory type, then by subclass for the ranged slots; the caster flag picks the caster tables.
static bool DamageTableOf(const SparseItem& item, uint32_t subclass, DamageTable& table) {
    DamageTable twoHand = item.casterWeapon ? DamageTable::TwoHandCaster : DamageTable::TwoHand;
    switch (item.inventoryType) {
        case 24:   // Ammo.
            table = DamageTable::Ammo;
            return true;
        case 17:   // Two-hand.
            table = twoHand;
            return true;
        case 15: case 25: case 26:   // Ranged, thrown, ranged right.
            table = subclass == SUBCLASS_WAND ? DamageTable::OneHandCaster : twoHand;
            return subclass == SUBCLASS_WAND || subclass == SUBCLASS_BOW || subclass == SUBCLASS_GUN || subclass == SUBCLASS_CROSSBOW;
        case 13: case 21: case 22:   // One-hand, main hand, off hand.
            table = item.casterWeapon ? DamageTable::OneHandCaster : DamageTable::OneHand;
            return true;
        default:
            return false;
    }
}

RetailDamage ComputeRetailDamage(const ArmourTables& tables, const SparseItem& item, ItemClass kind, uint32_t itemLevel) {
    size_t quality;
    DamageTable table;
    if (kind.classId != CLASS_WEAPON || !QualityIndex(item.quality, quality) || !DamageTableOf(item, kind.subclass, table)) {
        return {};
    }
    const QualityValues* row = Find(tables.damage[static_cast<size_t>(table)], itemLevel);
    if (!row || (*row)[quality] <= 0 || !item.delay) {
        return {};
    }
    RetailDamage damage;
    damage.dps = (*row)[quality];
    float average = damage.dps * item.delay * 0.001f;
    damage.max = static_cast<uint32_t>(std::floor(average * (1 + item.damageVariance * 0.5f) + 0.5f));
    // The tooltip shows at least 1, and never a minimum above the maximum.
    float min = std::floor(average * (1 - item.damageVariance * 0.5f));
    damage.min = min < 1 ? 1 : static_cast<uint32_t>(min);
    damage.min = damage.min > damage.max ? damage.max : damage.min;
    return damage;
}

}
