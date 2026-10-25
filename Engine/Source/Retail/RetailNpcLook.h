#ifndef ENGINE_SOURCE_RETAIL_RETAILNPCLOOK_H
#define ENGINE_SOURCE_RETAIL_RETAILNPCLOOK_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailNpcDisplay.h"

namespace Retail {

// Retail NPC customization in the installed client's terms. The installed CharHairGeosets ids are retail's
// ChrCustomizationGeoset ids (FACTS2 Q4), so a hair style is found by its geoset id, not by its order: Draenei male
// lists the same ids in another order, and Tauren horns are GeosetType 24 one below the installed id. Facial hair is
// three retail options but one installed combination, so the closest installed combination is taken.

// A CharHairGeosets row.
struct InstalledHairStyle {
    uint32_t race;
    uint32_t sex;
    uint32_t variation;
    uint32_t geoset;
};

// A CharacterFacialHairStyles row: beard (geoset 100 +), moustache (300 +) and sideburns (200 +).
struct InstalledFacialHair {
    uint32_t race;
    uint32_t sex;
    uint32_t variation;
    std::array<uint32_t, 3> geosets;
};

struct InstalledLook {
    std::vector<InstalledHairStyle> hairStyles;
    std::vector<InstalledFacialHair> facialHair;
};

// The Extra row's customization fields, and a line for each thing the installed client cannot show.
struct NpcAppearance {
    uint32_t hairStyle = 0;
    uint32_t hairColor = 0;
    uint32_t facialHair = 0;
    std::vector<std::string> notes;
};

NpcAppearance MapNpcLook(uint32_t race, uint32_t sex, const NpcLook& look, const InstalledLook& installed);

// The CreatureDisplayInfoExtra row a served display names, field by field. Skin and face stay 0: the bake replaces the
// skin and face, and skin 0 keeps the base row the client requires (its _Extra texture, the mouth, still applies).
struct NpcExtraFields {
    int32_t id = 0;
    int32_t race = 0;
    int32_t sex = 0;
    int32_t skin = 0;
    int32_t face = 0;
    int32_t hairStyle = 0;
    int32_t hairColor = 0;
    int32_t facialHair = 0;
    std::array<int32_t, NPC_ITEM_SLOTS> items = {};
    std::string bakeName;
};

// Whether the client can draw an item display id (a stock row or a served retail one).
using ItemServable = std::function<bool(uint32_t displayId)>;

// An item the client cannot draw leaves its slot empty, with a note.
NpcExtraFields ServedNpcExtra(const NpcExtra& extra, const NpcPlan& plan, const NpcAppearance& appearance, const ItemServable& servable,
    std::vector<std::string>& notes);

}

#endif
