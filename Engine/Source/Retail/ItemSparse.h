#ifndef ENGINE_SOURCE_RETAIL_ITEMSPARSE_H
#define ENGINE_SOURCE_RETAIL_ITEMSPARSE_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"

namespace Retail {

// ItemSparse, the sparse table of every item's name, level, stats and sockets (WoWDBDefs layout 1C17D17F, 12.0.7 to
// 12.1.5). Its stats are allocations, not values: StatBudget.h turns them into values at an item level.
constexpr uint32_t ITEM_SPARSE_FDID = 1572924;
constexpr uint32_t ITEM_SPARSE_LAYOUT = 0x1C17D17F;
constexpr size_t ITEM_STAT_SLOTS = 10;
constexpr size_t ITEM_SOCKETS = 3;

// Description_lang and Display3_lang to Display_lang: the inline string columns Db2Table::Parse needs named.
std::vector<size_t> ItemSparseStrings();

struct SparseItem {
    uint32_t id = 0;
    std::string name;
    uint32_t itemLevel = 0;
    uint32_t quality = 0;
    uint32_t inventoryType = 0;
    uint32_t requiredLevel = 0;
    uint32_t expansion = 0;
    std::array<int32_t, ITEM_STAT_SLOTS> statTypes = {};      // StatModifier_bonusStat, -1 for none.
    std::array<int32_t, ITEM_STAT_SLOTS> statPercents = {};   // StatPercentEditor: budget share in 1/10000.
    std::array<float, ITEM_STAT_SLOTS> socketCosts = {};      // StatPercentageOfSocket: sockets paid per stat.
    std::array<uint32_t, ITEM_SOCKETS> sockets = {};          // SocketType: 1 meta, 2 red, 3 yellow, 4 blue, 7 prismatic...
    uint32_t socketBonus = 0;                                 // Socket_match_enchantment_ID.
    uint32_t delay = 0;                                       // ItemDelay, ms.
    float damageVariance = 0;                                 // DmgVariance.
    float qualityModifier = 0;
    uint32_t damageType = 0;
    uint32_t bonding = 0;
    uint32_t itemSet = 0;
    uint32_t squishEra = 0;                                   // ItemSquishEraID.
    uint32_t levelCurve = 0;                                  // PlayerLevelToItemLevelCurveID: the item scales.
    uint32_t contentTuning = 0;                               // ContentTuningID: the item scales within a level range.
    bool casterWeapon = false;                                // Flags[1] ITEM_FLAG2_CASTER_WEAPON: the caster damage tables.
};

// False for a row past the table's end.
bool ReadSparseItem(const Db2Table& table, size_t row, SparseItem& item);

// How the item scales with the player's level in retail ("ContentTuning 2905", "curve 956" or both), empty when it does
// not: its level, stats, armour and damage then follow the player, not its stored item level.
std::string LevelScaling(const SparseItem& item);

}

#endif
