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

// The meshes of RetailBodyGeosets whose group a choice names (a default variant is not chosen), ascending: they win on
// the character component over the meshes its items and defaults set.
std::vector<uint32_t> RetailBodyChosenGeosets(const NpcLook& look, const std::vector<uint32_t>& modelGeosets);

// A texture a retail body replaces by type with a chosen material's file.
struct LookTexture {
    uint32_t type;
    uint32_t file;

    bool operator==(const LookTexture& other) const { return type == other.type && file == other.file; }
};

// The look's materials placed by the body's texture layers: per texture type (ascending) the file of its lowest layer.
// Type 1 is left out: the bake replaces the whole skin. Facial hair (type 7) without a choice takes the hair's file.
std::vector<LookTexture> RetailBodyTextures(const NpcLook& look, const std::vector<TextureLayer>& layers);

// What a retail body draws where no character component dresses it (a mount, a plain model frame, a customizable
// creature such as the dragonriding drakes): the bake on the skin (type 1, none without one), the look's materials on
// the other types (RetailBodyTextures), and the body mesh 0 with one mesh per geoset group (RetailBodyGeosets).
struct CreatureLook {
    std::vector<LookTexture> textures;   // By type, ascending.
    std::vector<uint32_t> meshes;
};

CreatureLook RetailCreatureLook(const NpcLook& look, const std::vector<TextureLayer>& layers, uint32_t bake, const std::vector<uint32_t>& modelGeosets);

// The submesh ids of a .skin, each once, ascending.
bool SkinGeosetIds(const std::vector<uint8_t>& skin, std::vector<uint32_t>& ids, std::string& error);

}

#endif
