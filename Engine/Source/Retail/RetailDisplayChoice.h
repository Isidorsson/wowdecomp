#ifndef ENGINE_SOURCE_RETAIL_RETAILDISPLAYCHOICE_H
#define ENGINE_SOURCE_RETAIL_RETAILDISPLAYCHOICE_H

#include <array>
#include <cstdint>
#include <string>
#include "Engine/Source/Retail/RetailConfig.h"
#include "Engine/Source/Retail/RetailDisplays.h"

namespace Retail {

// Whether a creature display id the client's own CreatureDisplayInfo has draws the retail row of that id instead
// (RetailCasc.ini creature_displays). Only stock 3.3.5 ids qualify: the ids above them are the admin's own rows, and
// retail uses the same numbers for unrelated creatures.

// The last id of the stock 3.3.5 CreatureDisplayInfo (12340, enUS).
constexpr uint32_t STOCK_CREATURE_DISPLAY_MAX = 32754;

// What a stock row draws. A character body without an Extra row is a player's own display (Human Male is 49), which
// the character's customization skins; retail's row of that id is a bare body, so it stays stock in every mode.
enum class StockDisplayKind {
    Creature,
    NpcHumanoid,   // An Extra row: a humanoid NPC, baked and dressed.
    PlayerBody,    // A character body without an Extra row.
};

// retail is the retail row of the id, null when retail lacks it.
bool RetailDisplayWins(CreatureDisplays mode, uint32_t id, StockDisplayKind kind, const RetailDisplay* retail);

// A CreatureDisplayInfo row, field by field, with names in place of the client's string pointers.
struct DisplayRow {
    int32_t id = 0;
    int32_t modelId = 0;
    int32_t soundId = 0;
    int32_t extendedDisplayId = 0;
    float scale = 1;
    int32_t alpha = 255;
    std::array<std::string, 3> variations;
    std::string fourthVariation;   // Retail's type 5 skin, served beside the row (RetailFourthVariation).
    std::string portrait;
    int32_t sizeClass = 0;
    int32_t bloodId = 0;
    int32_t npcSoundId = 0;
    int32_t particleColorId = 0;
    int32_t geosetData = 0;
    int32_t objectEffectPackageId = 0;
};

// The stock row drawn with retail's look: retail's model, skins, scale, alpha and geosets. Its sounds, blood, NPC
// sounds, particle colours, object effects, portrait and size class stay: retail's ids for those index tables the
// client does not have. The Extra row goes too; the NPC path serves retail's own when the caller draws humanoids.
DisplayRow OverlayRetailDisplay(const DisplayRow& stock, const RetailDisplay& retail);

}

#endif
