#ifndef ENGINE_SOURCE_RETAIL_RETAILEFFECTS_H
#define ENGINE_SOURCE_RETAIL_RETAILEFFECTS_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Internals the shading files share: what a retail batch shader draws in 3.3.5 terms.

namespace Retail {

// M2COMBINER operations.
constexpr uint16_t OP_OPAQUE = 0;
constexpr uint16_t OP_MOD = 1;
constexpr uint16_t OP_ADD = 3;
constexpr uint16_t OP_MOD2X = 4;
constexpr uint16_t OP_MOD2X_NA = 6;
constexpr uint16_t OP_ADD_NA = 7;

// Texture coordinate sources, as indices into the coordinate pairs: UV1, UV2, the environment map.
constexpr uint16_t UV1 = 0;
constexpr uint16_t UV2 = 1;
constexpr uint16_t ENV = 2;

// Material blend modes of the passes.
constexpr uint16_t BLEND_ALPHA = 2;
constexpr uint16_t BLEND_ADD = 4;
constexpr uint16_t BLEND_MOD = 5;

constexpr uint16_t EFFECT_FLAG = 0x8000;
constexpr uint16_t CLIENT_EFFECTS = 3;
constexpr int16_t NO_CLIENT_EFFECT = -1;

struct Shading {
    uint16_t ops[2];
    uint16_t coords[2];
};

// A texture past an effect's base, drawn over it as a batch of its own: blended by its material, on its own
// coordinates, reading the batch's texture weight slot weight.
struct Pass {
    uint16_t texture;
    uint16_t blend;
    uint16_t coord;
    uint16_t weight;
};

// A retail shader effect in 3.3.5 terms (wow-toolbox-editor downport/transform/shader_effects.rs, after
// wow.export's ShaderMapper): its base batch is one of the client's effects or one or two combiners, and
// textures past the base are passes or lost.
struct Effect {
    const char* pixelShader;
    uint16_t textures;          // Of the base batch.
    Shading base;
    int16_t clientEffect;       // The client effect (0 to 2) the base is, or NO_CLIENT_EFFECT.
    uint16_t passCount;
    const char* lost;
    Pass passes[2];
};

// What a retail batch draws in 3.3.5: one of the client's effects, or combiners on coordinate sources.
struct BaseShading {
    const Effect* effect;       // Null for a batch without the effect flag or past the table.
    int16_t clientEffect;
    uint16_t textures;
    Shading shading;
};

BaseShading ResolveShading(uint16_t shader, uint16_t textures);

const Effect& ClientEffect(int16_t index);

// A pass is drawn when its texture is one the batch declares and the texture lookup has it.
inline bool PassFits(const Pass& pass, uint16_t declared, uint16_t textureCombo, uint32_t textureCombos) {
    return pass.texture < declared && uint32_t(textureCombo) + pass.texture < textureCombos;
}

// "skin 0: batches 3, 4" for the warnings, one per view.
std::map<uint32_t, std::string> BatchNames(const std::map<uint32_t, std::vector<uint32_t>>& byView);

}

#endif
