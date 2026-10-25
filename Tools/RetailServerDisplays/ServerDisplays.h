#ifndef TOOLS_RETAILSERVERDISPLAYS_SERVERDISPLAYS_H
#define TOOLS_RETAILSERVERDISPLAYS_SERVERDISPLAYS_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailDisplays.h"
#include "Tools/RetailServerDisplays/Wdbc.h"

namespace ServerDisplays {

// Server-only placeholder rows that let a 3.3.5 worldserver spawn a creature with a retail display id the client
// serves from CASC. The server reads only a display's model and scale, and its model's scale, collision height,
// mount height and name (DBCfmt.h); the rest stays 0. The client keeps its stock rows, so these never ship to it.

// Field counts of the 3.3.5 layouts (CreatureDisplayInfofmt, CreatureModelDatafmt).
constexpr uint32_t DISPLAY_FIELDS = 16;
constexpr uint32_t MODEL_FIELDS = 28;

// The CombatReach most stock creature_model_info rows have at scale 1: the player default.
constexpr float STOCK_COMBAT_REACH = 1.5f;

struct ModelInfo {
    uint32_t displayId = 0;
    float boundingRadius = 0;
    float combatReach = 0;
};

// As stock rows relate to their CreatureModelData: BoundingRadius half the collision width, CombatReach the
// player default, both times the display and model scale.
ModelInfo ModelInfoOf(const Retail::RetailDisplay& display, const Retail::RetailCreatureModel& model);

std::vector<uint32_t> DisplayRow(uint32_t displayId, uint32_t serverModelId, float scale);
std::vector<uint32_t> ModelRow(uint32_t serverModelId, uint32_t nameOffset, const Retail::RetailCreatureModel& model);

bool KnownLayouts(const Wdbc& displays, const Wdbc& models, std::string& error);

struct Added {
    std::vector<uint32_t> displays;   // Appended, ascending.
    std::vector<uint32_t> stock;      // Refused: the server already has them, and stock rows win.
    std::vector<uint32_t> unknown;    // Not a retail display with a model.
    std::vector<ModelInfo> modelInfo;
    uint32_t models = 0;              // Placeholder model rows appended.
};

// Appends a display row per new id, naming a placeholder model appended once per retail model with an id above
// every model the server has. The server never sends model ids to the client, so any free id does.
Added AddRetailDisplays(Wdbc& displays, Wdbc& models, const Retail::RetailDisplays& retail, std::vector<uint32_t> ids);

// Every retail display id the server lacks.
std::vector<uint32_t> MissingDisplayIds(const Wdbc& displays, const Retail::RetailDisplays& retail);

// ids without those table has, which go to excluded. The client's own rows win there, so a server-only row with
// one of their ids would mismatch the model the client shows.
std::vector<uint32_t> WithoutIdsOf(const Wdbc& table, const std::vector<uint32_t>& ids, std::vector<uint32_t>& excluded);

// REPLACE statements for world.creature_model_info, genderless with no other-gender display.
std::string ModelInfoSql(const std::vector<ModelInfo>& rows);

}

#endif
