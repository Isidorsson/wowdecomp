#include "Engine/Source/Retail/ItemSparse.h"
#include <cstring>

namespace Retail {

// Physical columns of layout 1C17D17F (ID is noninline).
enum ItemSparseColumn : size_t {
    DISPLAY_LANG = 4,
    EXPANSION = 5,
    DAMAGE_VARIANCE = 6,
    QUALITY_MODIFIER = 9,
    SOCKET_COSTS = 14,
    STAT_PERCENTS = 15,
    STAT_TYPES = 16,
    LEVEL_CURVE = 31,
    SQUISH_ERA = 34,
    SOCKET_BONUS = 39,
    ITEM_SET = 43,
    DELAY = 46,
    ITEM_LEVEL = 50,
    SOCKET_TYPES = 55,
    BONDING = 59,
    DAMAGE_TYPE = 60,
    REQUIRED_LEVEL = 64,
    INVENTORY_TYPE = 65,
    QUALITY = 66,
};

std::vector<size_t> ItemSparseStrings() {
    return { 0, 1, 2, 3, DISPLAY_LANG };
}

static float AsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

template <typename T, size_t N>
static void Fill(std::array<T, N>& into, const std::vector<uint32_t>& values) {
    for (size_t i = 0; i < N; i++) {
        into[i] = i < values.size() ? static_cast<T>(values[i]) : T();
    }
}

bool ReadSparseItem(const Db2Table& table, size_t row, SparseItem& item) {
    if (row >= table.RowCount()) {
        return false;
    }
    item = SparseItem();
    item.id = table.RowId(row);
    item.name = table.String(row, DISPLAY_LANG);
    item.itemLevel = table.Value(row, ITEM_LEVEL);
    item.quality = table.Value(row, QUALITY);
    item.inventoryType = table.Value(row, INVENTORY_TYPE);
    item.requiredLevel = table.Value(row, REQUIRED_LEVEL);
    item.expansion = table.Value(row, EXPANSION);
    Fill(item.statTypes, table.Values(row, STAT_TYPES));
    Fill(item.statPercents, table.Values(row, STAT_PERCENTS));
    std::vector<uint32_t> costs = table.Values(row, SOCKET_COSTS);
    for (size_t i = 0; i < ITEM_STAT_SLOTS; i++) {
        item.socketCosts[i] = i < costs.size() ? AsFloat(costs[i]) : 0;
    }
    Fill(item.sockets, table.Values(row, SOCKET_TYPES));
    item.socketBonus = table.Value(row, SOCKET_BONUS);
    item.delay = table.Value(row, DELAY);
    item.damageVariance = AsFloat(table.Value(row, DAMAGE_VARIANCE));
    item.qualityModifier = AsFloat(table.Value(row, QUALITY_MODIFIER));
    item.damageType = table.Value(row, DAMAGE_TYPE);
    item.bonding = table.Value(row, BONDING);
    item.itemSet = table.Value(row, ITEM_SET);
    item.squishEra = table.Value(row, SQUISH_ERA);
    item.levelCurve = table.Value(row, LEVEL_CURVE);
    return true;
}

}
