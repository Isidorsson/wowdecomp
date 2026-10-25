#ifndef ENGINE_SOURCE_RETAIL_CLASSICSTATS_H
#define ENGINE_SOURCE_RETAIL_CLASSICSTATS_H

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Engine/Source/Retail/StatBudget.h"

namespace Retail {

// Retail stat types mapped to 3.3.5 item_template stats (ITEM_MOD_*), one rule per type:
// - the types both share (primaries, stamina, ratings, attack and spell power...) keep their number and value;
// - versatility becomes stamina, 1:1;
// - the hybrid primaries (agility or strength or intellect...) become one primary: the first named, or the one chosen;
// - resistances go to the resistance columns (holy_res...), at most 255;
// - the rest (mastery, leech, avoidance, speed, indestructible, PvP power, corruption...) are dropped.
// Every converted or dropped line says so. Equal 3.3.5 types are summed; at most 10 stat slots, values up to 32767.

enum class ComboPrimary : uint8_t { First = 0, Strength = 1, Agility = 2, Intellect = 3 };

constexpr size_t CLASSIC_STAT_SLOTS = 10;
constexpr int32_t CLASSIC_STAT_MAX = 32767;   // stat_value is a smallint.
constexpr int32_t CLASSIC_RESISTANCE_MAX = 255;   // holy_res... are tinyint unsigned.

// item_template resistance columns, in column order.
enum class Resistance : uint8_t { Holy, Fire, Nature, Frost, Shadow, Arcane, Count };
const char* ResistanceColumn(Resistance resistance);

struct StatLine {
    int32_t retailType = 0;
    int32_t retailValue = 0;
    int32_t classicType = -1;   // ITEM_MOD_* of a stat slot, -1 for none.
    int32_t resistance = -1;    // Resistance of a resistance column, -1 for none.
    int32_t value = 0;          // What the 3.3.5 item gets; 0 when dropped.
    std::string note;           // Why it changed or was dropped; empty when kept as is.
};

struct ClassicStats {
    std::vector<std::pair<uint32_t, int32_t>> stats;   // ITEM_MOD_*, value; at most CLASSIC_STAT_SLOTS.
    std::array<int32_t, static_cast<size_t>(Resistance::Count)> resistances = {};
    std::vector<StatLine> lines;                         // One per retail stat, in order.
};

ClassicStats MapToClassic(const std::vector<RetailStat>& stats, ComboPrimary combo);

std::string RetailStatName(int32_t type);
std::string ClassicStatName(int32_t type);

// item_template socketColor of a retail SocketType: 1 meta, 2 red, 4 yellow, 8 blue, 14 prismatic; 0 for the
// retail-only kinds (cogwheel, domination, tinker...).
uint32_t ClassicSocketColor(uint32_t socketType);

}

#endif
