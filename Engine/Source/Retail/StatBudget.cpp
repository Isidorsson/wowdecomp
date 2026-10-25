#include "Engine/Source/Retail/StatBudget.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace Retail {

// RandPropPoints physical columns (layout 4FD22743): EpicF, SuperiorF, GoodF, five floats each.
constexpr size_t RAND_PROP_EPIC = 4;
constexpr size_t RAND_PROP_SUPERIOR = 5;
constexpr size_t RAND_PROP_GOOD = 6;

constexpr uint32_t QUALITY_UNCOMMON = 2;
constexpr uint32_t QUALITY_RARE = 3;
constexpr uint32_t QUALITY_EPIC = 4;
constexpr uint32_t QUALITY_LEGENDARY = 5;
constexpr uint32_t QUALITY_ARTIFACT = 6;
constexpr uint32_t QUALITY_HEIRLOOM = 7;

constexpr uint32_t SUBCLASS_WAND = 19;
constexpr double ALLOCATION_SCALE = 10000;

static float AsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool ParseRow(const std::string& line, GameTable& table) {
    std::istringstream cells(line);
    std::string cell;
    if (!std::getline(cells, cell, '\t')) {
        return false;
    }
    char* end = nullptr;
    unsigned long level = std::strtoul(cell.c_str(), &end, 10);
    if (end == cell.c_str() || *end) {
        return false;
    }
    std::vector<float> values;
    while (std::getline(cells, cell, '\t')) {
        float value = std::strtof(cell.c_str(), &end);
        if (end == cell.c_str() || *end) {
            return false;
        }
        values.push_back(value);
    }
    table[static_cast<uint32_t>(level)] = std::move(values);
    return true;
}

bool ParseGameTable(const std::string& text, GameTable& table, std::string& error) {
    table.clear();
    std::istringstream lines(text);
    std::string line;
    bool header = true;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (header || line.empty()) {
            header = false;
            continue;
        }
        if (!ParseRow(line, table)) {
            error = "game table: bad row \"" + line + "\"";
            return false;
        }
    }
    if (table.empty()) {
        error = "game table: no rows";
        return false;
    }
    return true;
}

static std::array<float, BUDGET_SLOTS> Cells(const Db2Table& table, size_t row, size_t column) {
    std::vector<uint32_t> values = table.Values(row, column);
    std::array<float, BUDGET_SLOTS> cells = {};
    for (size_t i = 0; i < BUDGET_SLOTS && i < values.size(); i++) {
        cells[i] = AsFloat(values[i]);
    }
    return cells;
}

bool ReadRandPropPoints(const Db2Table& table, std::map<uint32_t, RandPropRow>& rows, std::string& error) {
    if (!KnownLayout(table, RAND_PROP_POINTS_LAYOUT, "RandPropPoints", error)) {
        return false;
    }
    rows.clear();
    for (size_t row = 0; row < table.RowCount(); row++) {
        RandPropRow& into = rows[table.RowId(row)];
        into.epic = Cells(table, row, RAND_PROP_EPIC);
        into.superior = Cells(table, row, RAND_PROP_SUPERIOR);
        into.good = Cells(table, row, RAND_PROP_GOOD);
    }
    return true;
}

// GetRandomPropertyPoints (TrinityCore master DB2Stores.cpp): which cell of a quality's five an inventory type reads.
int32_t BudgetSlot(uint32_t inventoryType, uint32_t subclass) {
    switch (inventoryType) {
        case 1: case 4: case 5: case 7: case 15: case 17: case 20:   // Head, shirt, chest, legs, ranged, two-hand, robe.
            return 0;
        case 26:   // Ranged right: a wand is a one-hand weapon.
            return subclass == SUBCLASS_WAND ? 3 : 0;
        case 13: case 21: case 22:   // One-hand, main hand, off hand.
            return 3;
        case 3: case 6: case 8: case 10: case 12:   // Shoulders, waist, feet, hands, trinket.
            return 1;
        case 2: case 9: case 11: case 14: case 16: case 23:   // Neck, wrists, finger, shield, back, held in off hand.
            return 2;
        case 28:   // Relic.
            return 4;
        default:
            return -1;
    }
}

// GetIlvlStatMultiplier (TrinityCore master): the multiplier tables' columns.
size_t MultiplierColumn(uint32_t inventoryType) {
    switch (inventoryType) {
        case 2: case 11:
            return 3;
        case 12:
            return 2;
        case 13: case 14: case 15: case 17: case 21: case 22: case 23: case 26:
            return 1;
        default:
            return 0;
    }
}

float BudgetPoints(const StatBudget& budget, uint32_t itemLevel, uint32_t quality, uint32_t inventoryType, uint32_t subclass) {
    int32_t slot = BudgetSlot(inventoryType, subclass);
    auto row = budget.randProp.find(itemLevel);
    if (slot < 0 || row == budget.randProp.end()) {
        return 0;
    }
    switch (quality) {
        case QUALITY_UNCOMMON:
            return row->second.good[slot];
        case QUALITY_RARE: case QUALITY_HEIRLOOM:
            return row->second.superior[slot];
        case QUALITY_EPIC: case QUALITY_LEGENDARY: case QUALITY_ARTIFACT:
            return row->second.epic[slot];
        default:
            return 0;
    }
}

// Combat ratings (ITEM_MOD 12 to 37, versatility, armour penetration, mastery, PvP power and the tertiaries) take the
// rating multiplier; resistances 51 to 56 truncate.
StatScaling ScalingOf(int32_t statType) {
    if (statType == 7) {
        return StatScaling::Stamina;
    }
    if ((statType >= 12 && statType <= 37) || statType == 40 || statType == 44 || statType == 49 || (statType >= 57 && statType <= 64)) {
        return StatScaling::Rating;
    }
    if (statType >= 51 && statType <= 56) {
        return StatScaling::Resistance;
    }
    return StatScaling::Plain;
}

static float TableValue(const GameTable& table, uint32_t itemLevel, size_t column, float fallback) {
    auto row = table.find(itemLevel);
    return row != table.end() && column < row->second.size() ? row->second[column] : fallback;
}

static double MultiplierOf(const StatBudget& budget, int32_t type, uint32_t itemLevel, uint32_t inventoryType, bool retailStamina) {
    size_t column = MultiplierColumn(inventoryType);
    switch (ScalingOf(type)) {
        case StatScaling::Stamina:
            return retailStamina ? TableValue(budget.stamina, itemLevel, column, 1) : 1;
        case StatScaling::Rating:
            return TableValue(budget.combatRatings, itemLevel, column, 1);
        default:
            return 1;
    }
}

std::vector<RetailStat> ComputeRetailStats(const SparseItem& item, const StatBudget& budget, const StatRequest& request) {
    double points = BudgetPoints(budget, request.itemLevel, item.quality, item.inventoryType, request.subclass);
    if (points <= 0) {
        return {};
    }
    double socketCost = TableValue(budget.socketCost, request.itemLevel, 0, 0);
    std::vector<RetailStat> stats;
    for (size_t i = 0; i < ITEM_STAT_SLOTS; i++) {
        if (item.statTypes[i] < 0 || item.statPercents[i] == 0) {
            continue;
        }
        RetailStat stat;
        stat.type = item.statTypes[i];
        stat.budget = item.statPercents[i] * points / ALLOCATION_SCALE;
        double value = stat.budget - static_cast<int32_t>(item.socketCosts[i] * socketCost);
        stat.multiplier = MultiplierOf(budget, stat.type, request.itemLevel, item.inventoryType, request.retailStamina);
        bool truncates = ScalingOf(stat.type) == StatScaling::Resistance;
        stat.value = static_cast<int32_t>(truncates ? std::floor(value) : std::floor(value * stat.multiplier + 0.5));
        stats.push_back(stat);
    }
    return stats;
}

}
