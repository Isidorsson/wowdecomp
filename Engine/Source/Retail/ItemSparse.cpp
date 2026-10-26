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
    FLAGS = 27,
    CONTENT_TUNING = 30,
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

constexpr size_t FLAGS_2 = 1;
constexpr uint32_t FLAG2_CASTER_WEAPON = 0x200;

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
    Db2Row fields = table.ReadRow(row);
    item.name = fields.String(DISPLAY_LANG);
    item.itemLevel = fields.Value(ITEM_LEVEL);
    item.quality = fields.Value(QUALITY);
    item.inventoryType = fields.Value(INVENTORY_TYPE);
    item.requiredLevel = fields.Value(REQUIRED_LEVEL);
    item.expansion = fields.Value(EXPANSION);
    Fill(item.statTypes, fields.Values(STAT_TYPES));
    Fill(item.statPercents, fields.Values(STAT_PERCENTS));
    std::vector<uint32_t> costs = fields.Values(SOCKET_COSTS);
    for (size_t i = 0; i < ITEM_STAT_SLOTS; i++) {
        item.socketCosts[i] = i < costs.size() ? AsFloat(costs[i]) : 0;
    }
    Fill(item.sockets, fields.Values(SOCKET_TYPES));
    item.socketBonus = fields.Value(SOCKET_BONUS);
    item.delay = fields.Value(DELAY);
    item.damageVariance = AsFloat(fields.Value(DAMAGE_VARIANCE));
    item.qualityModifier = AsFloat(fields.Value(QUALITY_MODIFIER));
    item.damageType = fields.Value(DAMAGE_TYPE);
    item.bonding = fields.Value(BONDING);
    item.itemSet = fields.Value(ITEM_SET);
    item.squishEra = fields.Value(SQUISH_ERA);
    item.levelCurve = fields.Value(LEVEL_CURVE);
    item.contentTuning = fields.Value(CONTENT_TUNING);
    std::vector<uint32_t> flags = fields.Values(FLAGS);
    item.casterWeapon = flags.size() > FLAGS_2 && (flags[FLAGS_2] & FLAG2_CASTER_WEAPON);
    return true;
}

std::string LevelScaling(const SparseItem& item) {
    std::string how = item.contentTuning ? "ContentTuning " + std::to_string(item.contentTuning) : std::string();
    if (item.levelCurve) {
        how += (how.empty() ? "" : ", ") + std::string("curve ") + std::to_string(item.levelCurve);
    }
    return how;
}

}
