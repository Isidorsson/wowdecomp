#ifndef TOOLS_TESTS_REALBUDGET_H
#define TOOLS_TESTS_REALBUDGET_H

#include <cstring>
#include <string>
#include "Engine/Source/Retail/StatBudget.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/StatBudgetFixture.h"

// The real budget tables of StatBudgetFixture.h as a StatBudget.

inline float BudgetBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline Retail::StatBudget RealBudget() {
    Retail::StatBudget budget;
    for (const RandPropBits& row : RealRandPropRows()) {
        Retail::RandPropRow& into = budget.randProp[row.itemLevel];
        for (size_t i = 0; i < Retail::BUDGET_SLOTS; i++) {
            into.epic[i] = BudgetBits(row.epic[i]);
            into.superior[i] = BudgetBits(row.superior[i]);
            into.good[i] = BudgetBits(row.good[i]);
        }
    }
    std::string error;
    CHECK(Retail::ParseGameTable(RealCombatRatingsText(), budget.combatRatings, error));
    CHECK(Retail::ParseGameTable(RealStaminaText(), budget.stamina, error));
    CHECK(Retail::ParseGameTable(RealSocketCostText(), budget.socketCost, error));
    return budget;
}

#endif
