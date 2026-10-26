#ifndef ENGINE_SOURCE_RETAIL_DISPLAYAUDIT_H
#define ENGINE_SOURCE_RETAIL_DISPLAYAUDIT_H

#include <cstdint>
#include <functional>
#include <vector>
#include "Engine/Source/Retail/RetailDisplays.h"
#include "Engine/Source/Retail/RetailItemDisplays.h"
#include "Engine/Source/Retail/RetailNpcDisplay.h"

namespace Retail {

// What a served display uses that the client cannot draw, read from its rows alone (no model is converted), so every
// display can be checked in seconds and a fix chosen by how many displays it repairs. A mesh worn in another colour
// is the same finding: colour lives in skins and bakes, which the client draws.

enum class AuditFlag {
    MissingExtra,         // detail: the CreatureDisplayInfoExtra id the tables lack.
    NoBake,               // The Extra row names neither an SD nor an HD bake.
    MissingItem,          // detail: the NPC item slot whose ItemDisplayInfo the tables lack.
    ArmourModelDropped,   // detail: the NPC item slot whose model (a collection, a buckle) is never attached.
    BodySectionDropped,   // detail: the retail body section with no 3.3.5 column.
    GeosetDropped,        // detail: the geoset group PackGeosetData cannot carry.
};

struct AuditFinding {
    AuditFlag flag;
    uint32_t detail;

    bool operator==(const AuditFinding& other) const { return flag == other.flag && detail == other.detail; }
};

// An ItemDisplayInfo id's display, null for one retail lacks.
using ItemDisplayLookup = std::function<const RetailItemDisplay*(uint32_t)>;

// extra is the display's Extra row, null for none or one the tables lack. Findings come in AuditFlag order, then by
// detail, each once.
std::vector<AuditFinding> AuditDisplay(const RetailDisplay& display, const NpcExtra* extra, const ItemDisplayLookup& items);

const char* AuditFlagName(AuditFlag flag);

}

#endif
