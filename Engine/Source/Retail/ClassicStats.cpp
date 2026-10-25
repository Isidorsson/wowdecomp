#include "Engine/Source/Retail/ClassicStats.h"
#include <algorithm>
#include <map>

namespace Retail {

constexpr int32_t MOD_AGILITY = 3;
constexpr int32_t MOD_STRENGTH = 4;
constexpr int32_t MOD_INTELLECT = 5;
constexpr int32_t MOD_STAMINA = 7;
constexpr int32_t MOD_VERSATILITY = 40;   // Retail; FERAL_ATTACK_POWER on 3.3.5.
constexpr int32_t LAST_SHARED_MOD = 48;   // BLOCK_VALUE: 0 to 48 are shared, but 40.

// Retail ITEM_MOD names (TrinityCore master ItemModType), the 3.3.5 ones where they differ.
static const std::map<int32_t, const char*>& RetailNames() {
    static const std::map<int32_t, const char*> names = {
        { 0, "Mana" }, { 1, "Health" }, { 3, "Agility" }, { 4, "Strength" }, { 5, "Intellect" }, { 6, "Spirit" }, { 7, "Stamina" },
        { 12, "Defense rating" }, { 13, "Dodge rating" }, { 14, "Parry rating" }, { 15, "Block rating" }, { 16, "Melee hit rating" },
        { 17, "Ranged hit rating" }, { 18, "Spell hit rating" }, { 19, "Melee crit rating" }, { 20, "Ranged crit rating" },
        { 21, "Spell crit rating" }, { 22, "Melee hit avoidance" }, { 23, "Ranged hit avoidance" }, { 24, "Spell hit avoidance" },
        { 25, "Melee crit avoidance" }, { 26, "Ranged crit avoidance" }, { 27, "Spell crit avoidance" }, { 28, "Melee haste rating" },
        { 29, "Ranged haste rating" }, { 30, "Spell haste rating" }, { 31, "Hit rating" }, { 32, "Crit rating" },
        { 33, "Hit avoidance rating" }, { 34, "Crit avoidance rating" }, { 35, "Resilience rating" }, { 36, "Haste rating" },
        { 37, "Expertise rating" }, { 38, "Attack power" }, { 39, "Ranged attack power" }, { 40, "Versatility" },
        { 41, "Spell healing" }, { 42, "Spell damage" }, { 43, "Mana regeneration" }, { 44, "Armor penetration rating" },
        { 45, "Spell power" }, { 46, "Health regeneration" }, { 47, "Spell penetration" }, { 48, "Block value" }, { 49, "Mastery" },
        { 50, "Bonus armor" }, { 51, "Fire resistance" }, { 52, "Frost resistance" }, { 53, "Holy resistance" },
        { 54, "Shadow resistance" }, { 55, "Nature resistance" }, { 56, "Arcane resistance" }, { 57, "PvP power" },
        { 58, "Amplify" }, { 59, "Multistrike" }, { 60, "Readiness" }, { 61, "Speed" }, { 62, "Leech" }, { 63, "Avoidance" },
        { 64, "Indestructible" }, { 66, "Cleave" }, { 71, "Agility or Strength or Intellect" }, { 72, "Agility or Strength" },
        { 73, "Agility or Intellect" }, { 74, "Strength or Intellect" }, { 168, "Corruption" }, { 169, "Corruption resistance" },
    };
    return names;
}

std::string RetailStatName(int32_t type) {
    auto found = RetailNames().find(type);
    return found != RetailNames().end() ? found->second : "Stat " + std::to_string(type);
}

std::string ClassicStatName(int32_t type) {
    if (type == MOD_VERSATILITY) {
        return "Feral attack power";
    }
    return type >= 0 && type <= LAST_SHARED_MOD ? RetailStatName(type) : "Stat " + std::to_string(type);
}

const char* ResistanceColumn(Resistance resistance) {
    static const char* const columns[] = { "holy_res", "fire_res", "nature_res", "frost_res", "shadow_res", "arcane_res" };
    size_t index = static_cast<size_t>(resistance);
    return index < static_cast<size_t>(Resistance::Count) ? columns[index] : "";
}

static int32_t ResistanceOf(int32_t type) {
    switch (type) {
        case 51: return static_cast<int32_t>(Resistance::Fire);
        case 52: return static_cast<int32_t>(Resistance::Frost);
        case 53: return static_cast<int32_t>(Resistance::Holy);
        case 54: return static_cast<int32_t>(Resistance::Shadow);
        case 55: return static_cast<int32_t>(Resistance::Nature);
        case 56: return static_cast<int32_t>(Resistance::Arcane);
        default: return -1;
    }
}

// The primaries a hybrid stands for, the first as retail names it.
static std::vector<int32_t> ComboMembers(int32_t type) {
    switch (type) {
        case 71: return { MOD_AGILITY, MOD_STRENGTH, MOD_INTELLECT };
        case 72: return { MOD_AGILITY, MOD_STRENGTH };
        case 73: return { MOD_AGILITY, MOD_INTELLECT };
        case 74: return { MOD_STRENGTH, MOD_INTELLECT };
        default: return {};
    }
}

static int32_t ChosenPrimary(ComboPrimary combo) {
    switch (combo) {
        case ComboPrimary::Strength: return MOD_STRENGTH;
        case ComboPrimary::Agility: return MOD_AGILITY;
        case ComboPrimary::Intellect: return MOD_INTELLECT;
        default: return -1;
    }
}

// Where one retail stat goes: a stat type, a resistance, or nowhere, with the rule's note.
static void Route(StatLine& line, ComboPrimary combo) {
    int32_t type = line.retailType;
    std::vector<int32_t> members = ComboMembers(type);
    if (!members.empty()) {
        int32_t chosen = ChosenPrimary(combo);
        bool member = std::find(members.begin(), members.end(), chosen) != members.end();
        line.classicType = member ? chosen : members[0];
        line.note = "hybrid primary: " + ClassicStatName(line.classicType) + (combo != ComboPrimary::First && !member ? " (its first; the chosen primary is not one of it)" : "");
    } else if (type == MOD_VERSATILITY) {
        line.classicType = MOD_STAMINA;
        line.note = "no versatility on 3.3.5: stamina, 1:1";
    } else if (type >= 0 && type <= LAST_SHARED_MOD) {
        line.classicType = type;
    } else if (ResistanceOf(type) >= 0) {
        line.resistance = ResistanceOf(type);
        line.note = std::string("to ") + ResistanceColumn(static_cast<Resistance>(line.resistance));
    } else {
        line.note = "no 3.3.5 equivalent: dropped";
    }
}

static void AddToSlots(ClassicStats& mapped, StatLine& line) {
    auto slot = std::find_if(mapped.stats.begin(), mapped.stats.end(), [&](const auto& s) { return s.first == static_cast<uint32_t>(line.classicType); });
    if (slot == mapped.stats.end() && mapped.stats.size() == CLASSIC_STAT_SLOTS) {
        line.classicType = -1;
        line.note = "no free stat slot (10 used): dropped";
        return;
    }
    if (slot == mapped.stats.end()) {
        mapped.stats.push_back({ static_cast<uint32_t>(line.classicType), 0 });
        slot = mapped.stats.end() - 1;
    }
    int32_t room = CLASSIC_STAT_MAX - slot->second;
    if (line.retailValue > room) {
        line.note += std::string(line.note.empty() ? "" : "; ") + "capped at 32767 (stat_value is a smallint)";
    }
    line.value = std::max(std::min(line.retailValue, room), -CLASSIC_STAT_MAX);
    slot->second += line.value;
}

static void AddToResistance(ClassicStats& mapped, StatLine& line) {
    int32_t& total = mapped.resistances[static_cast<size_t>(line.resistance)];
    int32_t room = CLASSIC_RESISTANCE_MAX - total;
    if (line.retailValue > room) {
        line.note += "; capped at 255";
    }
    line.value = std::max(std::min(line.retailValue, room), 0);
    total += line.value;
}

ClassicStats MapToClassic(const std::vector<RetailStat>& stats, ComboPrimary combo) {
    ClassicStats mapped;
    for (const RetailStat& stat : stats) {
        StatLine line;
        line.retailType = stat.type;
        line.retailValue = stat.value;
        Route(line, combo);
        if (line.classicType >= 0) {
            AddToSlots(mapped, line);
        } else if (line.resistance >= 0) {
            AddToResistance(mapped, line);
        }
        mapped.lines.push_back(line);
    }
    return mapped;
}

uint32_t ClassicSocketColor(uint32_t socketType) {
    switch (socketType) {
        case 1: return 1;    // Meta.
        case 2: return 2;    // Red.
        case 3: return 4;    // Yellow.
        case 4: return 8;    // Blue.
        case 7: return 14;   // Prismatic: red, yellow and blue.
        default: return 0;
    }
}

}
