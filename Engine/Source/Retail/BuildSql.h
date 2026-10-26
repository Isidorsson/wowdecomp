#ifndef ENGINE_SOURCE_RETAIL_BUILDSQL_H
#define ENGINE_SOURCE_RETAIL_BUILDSQL_H

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/Retail/ClassicStats.h"

namespace Retail {

// SQL for a TrinityCore 3.3.5 world database built from retail entries. Never a hand-built row: each statement copies
// a stock row the user picks (the base) and overrides only the columns it names, after deleting the new entry so an
// export can run again,
//   DELETE FROM item_template WHERE entry = 9000000;
//   INSERT INTO item_template (entry, class, ...) SELECT 9000000, 4, ... FROM item_template WHERE entry = <base>;
// with every column listed (the schema of the 3.3.5 world DB, SHOW COLUMNS), so a different schema fails loudly
// instead of shifting values. New entries count up from a start the user enters (the client cannot see which are free).

constexpr uint32_t DEFAULT_START_ENTRY = 9000000;
constexpr uint32_t DEFAULT_BASE_CREATURE = 15166;   // Luis Barriga: a plain level 60 humanoid, friendly to all, no AI.

// A MySQL string literal: quotes doubled, backslashes and control characters escaped.
std::string SqlQuote(const std::string& text);

const std::vector<std::string>& ItemTemplateColumns();
const std::vector<std::string>& CreatureTemplateColumns();

// INSERT INTO table (columns) SELECT <override, else the column>, ... FROM table WHERE entry = base;  Empty, with error,
// when an override names a column the table does not have.
std::string InsertFromBase(const std::string& table, const std::vector<std::string>& columns, uint32_t base,
    const std::map<std::string, std::string>& overrides, std::string& error);

struct ItemSql {
    uint32_t entry = 0;
    uint32_t base = 0;
    std::string name;
    uint32_t displayId = 0;
    uint32_t quality = 0;
    uint32_t inventoryType = 0;
    uint32_t classId = 0;
    uint32_t subclass = 0;
    uint32_t itemLevel = 0;
    int32_t requiredLevel = -1;   // -1 keeps the base's.
    ClassicStats stats;
    std::array<uint32_t, 3> socketColors = {};
    int32_t armor = -1;           // -1 keeps the base's.
    bool weapon = false;          // Sets dmg_min1, dmg_max1, dmg_type1 and delay, and clears the second damage.
    uint32_t damageMin = 0;
    uint32_t damageMax = 0;
    uint32_t damageType = 0;      // School: 0 physical, 1 holy, 2 fire, 3 nature, 4 frost, 5 shadow, 6 arcane.
    uint32_t delay = 0;           // ms.
    std::string comment;          // One line, written as an SQL comment above the statement.
};

struct CreatureSql {
    uint32_t entry = 0;
    uint32_t base = 0;
    std::string name;
    std::string subname;
    std::array<uint32_t, 4> models = {};   // modelid1..4: retail CreatureDisplayInfo ids.
    float scale = 0;                       // 0 keeps the base's.
    std::string comment;
};

// The statement (with its comment line), or empty with error.
std::string BuildItemSql(const ItemSql& item, std::string& error);
std::string BuildCreatureSql(const CreatureSql& creature, std::string& error);

// A stock 3.3.5 item of the class, subclass and inventory type to copy (epic, item level 200 to 245, no spell, set or
// socket where one exists; from the world DB), 0 when there is none. A robe falls back to a chest and back.
uint32_t SuggestedBaseItem(uint32_t classId, uint32_t subclass, uint32_t inventoryType);

// The 3.3.5 class and subclass of a retail one: warglaives become swords, cosmetic and relic armour becomes misc.
// Note says what changed; empty when nothing did.
void ClassicItemClass(uint32_t& classId, uint32_t& subclass, std::string& note);

}

#endif
