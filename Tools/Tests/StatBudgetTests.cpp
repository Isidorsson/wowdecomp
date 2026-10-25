#include <map>
#include <string>
#include "Engine/Source/Retail/StatBudget.h"
#include "Tools/Tests/BrowserFixtures.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/RealBudget.h"

using namespace Retail;

// Retail stat values: the formula of StatBudget.h on the real budget tables, against the values wowhead's tooltips show
// for real items at the item level wowhead shows them at (scratchpad s2\wh_plain.txt, wh_ring.txt).

// Values by stat type as retail's tooltip shows them (stamina multiplier on), summed as the tooltip sums an item's two
// allocations of one stat.
static std::map<int32_t, int32_t> StatsAt(uint32_t id, uint32_t itemLevel, uint32_t subclass = 0) {
    StatRequest request;
    request.itemLevel = itemLevel;
    request.subclass = subclass;
    request.retailStamina = true;
    std::map<int32_t, int32_t> values;
    for (const RetailStat& stat : ComputeRetailStats(RealSparseItem(id), RealBudget(), request)) {
        values[stat.type] += stat.value;
    }
    return values;
}

using Stats = std::map<int32_t, int32_t>;

TEST(GameTablesParseTheTabSeparatedRows) {
    GameTable table;
    std::string error;
    CHECK(ParseGameTable("Item Level\tArmor Multiplier\tWeapon Multiplier\r\n1\t1.5\t2\r\n40\t1.25\t0.5\r\n", table, error));
    CHECK(table.size() == 2 && table[40].size() == 2 && table[40][0] == 1.25f && table[1][1] == 2.0f);
    CHECK(!ParseGameTable("", table, error) && !error.empty());
    CHECK(!ParseGameTable("Level\tValue\n7\tabc\n", table, error));
}

TEST(RandPropPointsReadTheFloatCellsByItemLevel) {
    // EpicF 4, SuperiorF 5, GoodF 6 (5 floats each), after four scalar columns; the integer cells 7 to 9 follow.
    std::vector<uint32_t> values = { 0, 0, 0, 0 };
    for (uint32_t cell = 0; cell < 15; cell++) {
        values.push_back(FloatBits(10.0f + cell));
    }
    values.resize(4 + 30, 0);
    Db2Table table = TextTable(RAND_PROP_POINTS_LAYOUT, { 1, 1, 1, 1, 5, 5, 5, 5, 5, 5 }, { { 40, values } });
    std::map<uint32_t, RandPropRow> rows;
    std::string error;
    CHECK(ReadRandPropPoints(table, rows, error));
    CHECK(rows.size() == 1 && rows[40].epic[0] == 10.0f && rows[40].epic[4] == 14.0f && rows[40].superior[0] == 15.0f && rows[40].good[4] == 24.0f);

    Db2Table other = TextTable(0x12345678, { 1 }, { { 1, { 5 } } });
    CHECK(!ReadRandPropPoints(other, rows, error) && !error.empty());
}

TEST(BudgetSlotsFollowTheInventoryType) {
    CHECK(BudgetSlot(5, 0) == 0 && BudgetSlot(20, 0) == 0 && BudgetSlot(17, 0) == 0 && BudgetSlot(1, 0) == 0);
    CHECK(BudgetSlot(3, 0) == 1 && BudgetSlot(10, 0) == 1 && BudgetSlot(12, 0) == 1);
    CHECK(BudgetSlot(2, 0) == 2 && BudgetSlot(16, 0) == 2 && BudgetSlot(14, 0) == 2 && BudgetSlot(23, 0) == 2);
    CHECK(BudgetSlot(13, 0) == 3 && BudgetSlot(21, 0) == 3 && BudgetSlot(22, 0) == 3);
    CHECK(BudgetSlot(26, 19) == 3 && BudgetSlot(26, 3) == 0 && BudgetSlot(28, 0) == 4);
    CHECK(BudgetSlot(0, 0) == -1 && BudgetSlot(18, 0) == -1 && BudgetSlot(19, 0) == -1 && BudgetSlot(25, 0) == -1);
    CHECK(MultiplierColumn(5) == 0 && MultiplierColumn(16) == 0 && MultiplierColumn(13) == 1 && MultiplierColumn(14) == 1);
    CHECK(MultiplierColumn(12) == 2 && MultiplierColumn(2) == 3 && MultiplierColumn(11) == 3);
}

TEST(BudgetPointsNeedAQualityWithABudget) {
    StatBudget budget = RealBudget();
    CHECK(BudgetPoints(budget, 40, 4, 13, 7) > 8.02f && BudgetPoints(budget, 40, 4, 13, 7) < 8.03f);
    CHECK(BudgetPoints(budget, 40, 1, 13, 7) == 0 && BudgetPoints(budget, 40, 0, 5, 0) == 0);
    CHECK(BudgetPoints(budget, 40, 4, 19, 0) == 0);
    CHECK(BudgetPoints(budget, 41, 4, 5, 0) == 0);   // Not in the fixture's rows.
}

TEST(RetailStatsMatchWowheadForLegacyItems) {
    CHECK(StatsAt(19019, 40, 7) == Stats({ { 3, 2 }, { 7, 3 }, { 51, 2 }, { 55, 2 } }));
    CHECK(StatsAt(16958, 40) == Stats({ { 74, 8 }, { 7, 13 }, { 32, 7 }, { 51, 2 }, { 55, 2 } }));
    CHECK(StatsAt(15138, 13) == Stats({ { 7, 3 }, { 51, 3 } }));
}

TEST(RetailStatsMatchWowheadForModernItems) {
    CHECK(StatsAt(249370, 289) == Stats({ { 71, 70 }, { 7, 1309 }, { 36, 62 }, { 49, 30 } }));
    CHECK(StatsAt(255271, 60) == Stats({ { 73, 9 }, { 7, 14 }, { 40, 11 }, { 49, 8 } }));
    CHECK(StatsAt(273780, 340, 15) == Stats({ { 5, 581 }, { 7, 2094 }, { 49, 54 }, { 32, 48 } }));
    CHECK(StatsAt(263901, 259, 15) == Stats({ { 5, 273 }, { 7, 809 }, { 40, 48 }, { 32, 22 } }));
    CHECK(StatsAt(272251, 321) == Stats({ { 73, 167 }, { 7, 3369 }, { 36, 113 }, { 40, 78 } }));
}

TEST(RetailStatsUseTheJewelleryMultiplierForRingsAndNecks) {
    CHECK(StatsAt(186376, 49) == Stats({ { 7, 8 }, { 40, 21 }, { 49, 7 } }));
    CHECK(StatsAt(189854, 50) == Stats({ { 7, 9 }, { 32, 5 }, { 49, 24 } }));
    CHECK(StatsAt(189838, 49) == Stats({ { 7, 8 }, { 32, 22 }, { 49, 6 } }));
    CHECK(StatsAt(235422, 53) == Stats({ { 7, 9 }, { 32, 20 }, { 49, 10 } }));
    CHECK(StatsAt(228842, 684) == Stats({ { 7, 68228 }, { 36, 1335 }, { 49, 5854 } }));
}

TEST(SocketCostsTakeTheTruncatedSocketShare) {
    // Gladiator's Scaled Chestpiece at 44: costs 0.75, 0.75, 1.5 of 3 points a socket. Stamina and both ratings match
    // wowhead; wowhead shows 4 for the primary where int(1.5 * 3) = 4 leaves round(9.05 - 4) = 5 (docs/RETAIL-BROWSER.md).
    CHECK(StatsAt(27879, 44) == Stats({ { 7, 14 }, { 40, 6 }, { 32, 6 }, { 74, 5 } }));
}

TEST(RetailStaminaMultiplierCanBeLeftOut) {
    StatRequest request;
    CHECK(!request.retailStamina);
    request.itemLevel = 289;
    request.retailStamina = false;
    std::vector<RetailStat> stats = ComputeRetailStats(RealSparseItem(249370), RealBudget(), request);
    CHECK(stats.size() == 4 && stats[1].type == 7 && stats[1].value == 105 && stats[1].multiplier == 1);
    request.retailStamina = true;
    stats = ComputeRetailStats(RealSparseItem(249370), RealBudget(), request);
    CHECK(stats.size() == 4 && stats[1].value == 1309 && stats[1].multiplier > 12.5 && stats[1].budget > 104.5 && stats[1].budget < 104.8);
}

TEST(RetailStatsOfItemsWithoutABudgetAreEmpty) {
    StatRequest request;
    request.itemLevel = 999;   // No such row.
    CHECK(ComputeRetailStats(RealSparseItem(16958), RealBudget(), request).empty());
    SparseItem common = RealSparseItem(16958);
    common.quality = 1;
    request.itemLevel = 40;
    CHECK(ComputeRetailStats(common, RealBudget(), request).empty());
    SparseItem none;
    none.quality = 4;
    none.inventoryType = 5;
    none.statTypes.fill(-1);
    CHECK(ComputeRetailStats(none, RealBudget(), request).empty());
}
