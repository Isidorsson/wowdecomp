#ifndef ENGINE_SOURCE_RETAIL_RETAILBODYLOOK_H
#define ENGINE_SOURCE_RETAIL_RETAILBODYLOOK_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailNpcDisplay.h"
#include "Engine/Source/Retail/RetailRaces.h"

namespace Retail {

// A retail body (a Goblin or retail race NPC, slice b) carries every variant of every customization group: faces,
// ears, eyebrows, horns, tails. The installed client knows none of its choices, so the served look names the meshes
// to draw: per geoset group the model has, the chosen variant (a choice of GeosetType t and id g is mesh
// t * 100 + g, id 0 none: x00, which no model has), else the group's default (x01, or its lowest variant without
// one; none for the eye glows 17xx, which retail bodies made additive glows). The body mesh 0 is the client's own.
std::vector<uint32_t> RetailBodyGeosets(const NpcLook& look, const std::vector<uint32_t>& modelGeosets);

// A texture a retail body replaces by type with a chosen material's file.
struct LookTexture {
    uint32_t type;
    uint32_t file;

    bool operator==(const LookTexture& other) const { return type == other.type && file == other.file; }
};

// The look's materials placed by the body's texture layers: per texture type (ascending) the file of its lowest layer.
// Type 1 is left out: the bake replaces the whole skin. Facial hair (type 7) without a choice takes the hair's file.
std::vector<LookTexture> RetailBodyTextures(const NpcLook& look, const std::vector<TextureLayer>& layers);

// The submesh ids of a .skin, each once, ascending.
bool SkinGeosetIds(const std::vector<uint8_t>& skin, std::vector<uint32_t>& ids, std::string& error);

}

#endif
