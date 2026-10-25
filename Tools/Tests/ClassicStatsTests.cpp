#include <string>
#include "Engine/Source/Retail/ClassicStats.h"
#include "Tools/Tests/Check.h"

using namespace Retail;

// The retail to 3.3.5 stat mapping (ClassicStats.h): one rule per retail stat type.

static RetailStat Stat(int32_t type, int32_t value) {
    RetailStat stat;
    stat.type = type;
    stat.value = value;
    return stat;
}

using Slots = std::vector<std::pair<uint32_t, int32_t>>;

TEST(SharedStatsKeepTypeAndValue) {
    ClassicStats mapped = MapToClassic({ Stat(4, 80), Stat(7, 120), Stat(32, 50), Stat(36, 41), Stat(45, 90), Stat(35, 12) }, ComboPrimary::First);
    CHECK(mapped.stats == Slots({ { 4, 80 }, { 7, 120 }, { 32, 50 }, { 36, 41 }, { 45, 90 }, { 35, 12 } }));
    CHECK(mapped.lines.size() == 6 && mapped.lines[2].classicType == 32 && mapped.lines[2].value == 50 && mapped.lines[2].note.empty());
}

TEST(VersatilityBecomesStaminaAndSumsWithIt) {
    ClassicStats mapped = MapToClassic({ Stat(7, 100), Stat(40, 30) }, ComboPrimary::First);
    CHECK(mapped.stats == Slots({ { 7, 130 } }));
    CHECK(mapped.lines.size() == 2 && mapped.lines[1].retailType == 40 && mapped.lines[1].classicType == 7 && mapped.lines[1].value == 30);
    CHECK(mapped.lines.size() == 2 && !mapped.lines[1].note.empty());
}

TEST(HybridPrimariesBecomeOnePrimary) {
    // 71 agility or strength or intellect, 72 agility or strength, 73 agility or intellect, 74 strength or intellect.
    ClassicStats first = MapToClassic({ Stat(71, 1), Stat(72, 2), Stat(73, 4), Stat(74, 8) }, ComboPrimary::First);
    CHECK(first.stats == Slots({ { 3, 7 }, { 4, 8 } }));
    ClassicStats intellect = MapToClassic({ Stat(73, 9), Stat(74, 5), Stat(72, 3) }, ComboPrimary::Intellect);
    CHECK(intellect.stats == Slots({ { 5, 14 }, { 3, 3 } }));   // Intellect is not one of 72's: its first.
    CHECK(intellect.lines.size() == 3 && !intellect.lines[0].note.empty() && !intellect.lines[2].note.empty());
    ClassicStats strength = MapToClassic({ Stat(74, 6) }, ComboPrimary::Strength);
    CHECK(strength.stats == Slots({ { 4, 6 } }));
}

TEST(ResistancesGoToTheirColumns) {
    ClassicStats mapped = MapToClassic({ Stat(51, 3), Stat(52, 4), Stat(53, 5), Stat(54, 6), Stat(55, 7), Stat(56, 300) }, ComboPrimary::First);
    CHECK(mapped.stats.empty());
    CHECK(mapped.resistances[static_cast<size_t>(Resistance::Fire)] == 3 && mapped.resistances[static_cast<size_t>(Resistance::Frost)] == 4);
    CHECK(mapped.resistances[static_cast<size_t>(Resistance::Holy)] == 5 && mapped.resistances[static_cast<size_t>(Resistance::Shadow)] == 6);
    CHECK(mapped.resistances[static_cast<size_t>(Resistance::Nature)] == 7 && mapped.resistances[static_cast<size_t>(Resistance::Arcane)] == 255);
    CHECK(mapped.lines.size() == 6 && mapped.lines[0].resistance == static_cast<int32_t>(Resistance::Fire) && mapped.lines[0].classicType == -1);
    CHECK(mapped.lines.size() == 6 && !mapped.lines[5].note.empty() && mapped.lines[5].value == 255);
    CHECK(std::string(ResistanceColumn(Resistance::Holy)) == "holy_res" && std::string(ResistanceColumn(Resistance::Arcane)) == "arcane_res");
}

TEST(RetailOnlyStatsAreDroppedWithANote) {
    // Mastery, bonus armour, PvP power, speed, leech, avoidance, indestructible.
    ClassicStats mapped = MapToClassic({ Stat(49, 30), Stat(50, 10), Stat(57, 1), Stat(61, 2), Stat(62, 3), Stat(63, 4), Stat(64, 5), Stat(7, 9) }, ComboPrimary::First);
    CHECK(mapped.stats == Slots({ { 7, 9 } }));
    CHECK(mapped.lines.size() == 8);
    for (size_t i = 0; i + 1 < mapped.lines.size(); i++) {
        CHECK(mapped.lines[i].classicType == -1 && mapped.lines[i].resistance == -1 && mapped.lines[i].value == 0 && !mapped.lines[i].note.empty());
    }
}

TEST(ClassicStatsKeepTenSlotsAndTheSmallintRange) {
    std::vector<RetailStat> stats;
    for (int32_t type : { 3, 4, 5, 6, 7, 12, 13, 14, 15, 31, 32 }) {
        stats.push_back(Stat(type, 10));
    }
    ClassicStats mapped = MapToClassic(stats, ComboPrimary::First);
    CHECK(mapped.stats.size() == CLASSIC_STAT_SLOTS && mapped.stats.back().first == 31);
    CHECK(mapped.lines.size() == 11 && mapped.lines[10].value == 0 && !mapped.lines[10].note.empty());

    ClassicStats big = MapToClassic({ Stat(7, 68228) }, ComboPrimary::First);
    CHECK(big.stats == Slots({ { 7, CLASSIC_STAT_MAX } }) && !big.lines[0].note.empty());
    ClassicStats unknown = MapToClassic({ Stat(250, 9) }, ComboPrimary::First);
    CHECK(unknown.stats.empty() && unknown.lines.size() == 1 && !unknown.lines[0].note.empty());
}

TEST(StatAndSocketNames) {
    CHECK(RetailStatName(40) == "Versatility" && RetailStatName(49) == "Mastery" && RetailStatName(74) == "Strength or Intellect");
    CHECK(RetailStatName(999) == "Stat 999");
    CHECK(ClassicStatName(45) == "Spell power" && ClassicStatName(7) == "Stamina" && ClassicStatName(32) == "Crit rating");
    CHECK(ClassicSocketColor(1) == 1 && ClassicSocketColor(2) == 2 && ClassicSocketColor(3) == 4 && ClassicSocketColor(4) == 8);
    CHECK(ClassicSocketColor(7) == 14 && ClassicSocketColor(0) == 0 && ClassicSocketColor(6) == 0 && ClassicSocketColor(14) == 0);
}
