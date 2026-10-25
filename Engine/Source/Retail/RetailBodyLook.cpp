#include "Engine/Source/Retail/RetailBodyLook.h"
#include <cstring>
#include <map>
#include <set>
#include "Engine/Source/Retail/RetailModelData.h"

namespace Retail {

constexpr uint32_t GEOSET_GROUP = 100;
constexpr uint32_t DEFAULT_VARIANT = 1;
constexpr uint32_t BODY_MESH = 0;
// Retail bodies made 1701 an additive eye glow (3.3.5's is an empty mesh): drawn only when chosen.
constexpr uint32_t EYE_GLOW_GROUP = 17;
constexpr size_t SECTION_ID = 0x00;

// Id 0 is x00, which no model has: the group draws nothing.
static uint32_t MeshOf(const CustomizationGeoset& geoset) {
    return geoset.type * GEOSET_GROUP + geoset.id;
}

// The model's variants per group, the body mesh left out.
static std::map<uint32_t, std::set<uint32_t>> VariantsByGroup(const std::vector<uint32_t>& modelGeosets) {
    std::map<uint32_t, std::set<uint32_t>> groups;
    for (uint32_t mesh : modelGeosets) {
        if (mesh != BODY_MESH) {
            groups[mesh / GEOSET_GROUP].insert(mesh);
        }
    }
    return groups;
}

// The first choice of the group that names none or a mesh the model has, else x01 (none for the eye glows), else
// the group's lowest variant.
static uint32_t ChosenVariant(uint32_t group, const std::set<uint32_t>& variants, const NpcLook& look) {
    for (const CustomizationGeoset& geoset : look.geosets) {
        if (geoset.type == group && (!geoset.id || variants.count(MeshOf(geoset)))) {
            return MeshOf(geoset);
        }
    }
    if (group == EYE_GLOW_GROUP) {
        return group * GEOSET_GROUP;
    }
    uint32_t standard = group * GEOSET_GROUP + DEFAULT_VARIANT;
    return variants.count(standard) ? standard : *variants.begin();
}

std::vector<uint32_t> RetailBodyGeosets(const NpcLook& look, const std::vector<uint32_t>& modelGeosets) {
    std::vector<uint32_t> meshes;
    for (const auto& [group, variants] : VariantsByGroup(modelGeosets)) {
        meshes.push_back(ChosenVariant(group, variants, look));
    }
    return meshes;
}

bool SkinGeosetIds(const std::vector<uint8_t>& skin, std::vector<uint32_t>& ids, std::string& error) {
    Span sections;
    if (skin.size() < SkinAt::HEADER || std::memcmp(skin.data(), "SKIN", 4) != 0) {
        error = "not a skin file";
        return false;
    }
    if (!ReadSpan(skin, SkinAt::SECTIONS, SECTION_SIZE, "submeshes", sections, error)) {
        return false;
    }

    std::set<uint32_t> unique;
    for (uint32_t i = 0; i < sections.count; i++) {
        unique.insert(Get<uint16_t>(skin, sections.At(i, SECTION_SIZE) + SECTION_ID));
    }
    ids.assign(unique.begin(), unique.end());
    return true;
}

constexpr uint32_t BAKED_TEXTURE_TYPE = 1;
constexpr uint32_t HAIR_TEXTURE_TYPE = 6;
constexpr uint32_t FACIAL_HAIR_TEXTURE_TYPE = 7;

static bool HasType(const std::vector<TextureLayer>& layers, uint32_t type) {
    for (const TextureLayer& layer : layers) {
        if (layer.type == type) {
            return true;
        }
    }
    return false;
}

std::vector<LookTexture> RetailBodyTextures(const NpcLook& look, const std::vector<TextureLayer>& layers) {
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> lowest;   // Type to (layer, file).
    for (const ChosenMaterial& material : look.materials) {
        for (const TextureLayer& layer : layers) {
            if (layer.target != material.target || layer.type == BAKED_TEXTURE_TYPE) {
                continue;
            }
            auto found = lowest.find(layer.type);
            if (found == lowest.end() || layer.layer < found->second.first) {
                lowest[layer.type] = { layer.layer, material.file };
            }
        }
    }
    auto hair = lowest.find(HAIR_TEXTURE_TYPE);
    if (hair != lowest.end() && !lowest.count(FACIAL_HAIR_TEXTURE_TYPE) && HasType(layers, FACIAL_HAIR_TEXTURE_TYPE)) {
        lowest[FACIAL_HAIR_TEXTURE_TYPE] = hair->second;
    }
    std::vector<LookTexture> textures;
    for (const auto& [type, placed] : lowest) {
        textures.push_back({ type, placed.second });
    }
    return textures;
}

}
