#ifndef ENGINE_SOURCE_RETAIL_STATBUDGET_H
#define ENGINE_SOURCE_RETAIL_STATBUDGET_H

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"
#include "Engine/Source/Retail/ItemArmour.h"
#include "Engine/Source/Retail/ItemSparse.h"

namespace Retail {

// Retail item stat values from ItemSparse allocations, with the retail client's own budget tables (TrinityCore master
// Item::GetItemStatValue and GetRandomPropertyPoints; checked against wowhead tooltips, docs/RETAIL-BROWSER.md):
//   budget = StatPercentEditor * RandPropPoints[itemLevel].<quality>F[slot] / 10000
//   budget -= round(StatPercentageOfSocket * ItemSocketCostPerLevel[max(itemLevel, the item's own level)])
//   value  = floor(budget * multiplier + 0.5); resistances truncate: floor(budget)
// where the multiplier is StaminaMultByILvl for stamina, CombatRatingsMultByILvl for ratings and 1 otherwise, in the
// column of the item's slot (armour, weapon, trinket, jewellery).
constexpr uint32_t RAND_PROP_POINTS_FDID = 1310245;
constexpr uint32_t RAND_PROP_POINTS_LAYOUT = 0x4FD22743;
constexpr uint32_t COMBAT_RATINGS_MULT_FDID = 1391670;   // gametables\combatratingsmultbyilvl.txt
constexpr uint32_t STAMINA_MULT_FDID = 1980632;          // gametables\staminamultbyilvl.txt
constexpr uint32_t SOCKET_COST_FDID = 1391643;           // gametables\itemsocketcostperlevel.txt

constexpr size_t BUDGET_SLOTS = 5;

struct RandPropRow {
    std::array<float, BUDGET_SLOTS> epic = {};
    std::array<float, BUDGET_SLOTS> superior = {};
    std::array<float, BUDGET_SLOTS> good = {};
};

using GameTable = std::map<uint32_t, std::vector<float>>;

// A gametables\*.txt file: a tab-separated header row, then the item level and its values on each row.
bool ParseGameTable(const std::string& text, GameTable& table, std::string& error);
bool ReadRandPropPoints(const Db2Table& table, std::map<uint32_t, RandPropRow>& rows, std::string& error);

struct StatBudget {
    std::map<uint32_t, RandPropRow> randProp;
    GameTable combatRatings;
    GameTable stamina;
    GameTable socketCost;
    ArmourTables armour;   // Armour and weapon damage (ItemArmour.h); empty when its tables did not load.

    bool Ready() const { return !this->randProp.empty(); }
    uint32_t MaxItemLevel() const { return this->randProp.empty() ? 0 : this->randProp.rbegin()->first; }
};

// RandPropPoints cell of an inventory type (0 to 4), -1 for a slot without stats (bags, tabards, ammo, thrown).
int32_t BudgetSlot(uint32_t inventoryType, uint32_t subclass);
// Multiplier table column: 0 armour, 1 weapon, 2 trinket, 3 jewellery.
size_t MultiplierColumn(uint32_t inventoryType);
// The budget points of an item at itemLevel, 0 when its quality or slot carries none or the level is not in the table.
float BudgetPoints(const StatBudget& budget, uint32_t itemLevel, uint32_t quality, uint32_t inventoryType, uint32_t subclass);

enum class StatScaling : uint8_t { Plain, Stamina, Rating, Resistance };
StatScaling ScalingOf(int32_t statType);

struct RetailStat {
    int32_t type = 0;
    int32_t value = 0;
    double budget = 0;       // Before sockets and the multiplier.
    double multiplier = 1;
};

struct StatRequest {
    uint32_t itemLevel = 200;
    uint32_t subclass = 0;        // Item.SubclassID: a wand takes the one-hand cell.
    bool retailStamina = false;   // Apply StaminaMultByILvl as retail does (x9.49 at item level 200, far past 3.3.5 stamina).
};

// One value per filled allocation, in slot order; an allocation without a stat type, or whose sockets take its value
// to 0 or less, is skipped.
std::vector<RetailStat> ComputeRetailStats(const SparseItem& item, const StatBudget& budget, const StatRequest& request);

}

#endif
