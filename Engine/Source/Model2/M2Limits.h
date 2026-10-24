#ifndef ENGINE_SOURCE_MODEL2_M2LIMITS_H
#define ENGINE_SOURCE_MODEL2_M2LIMITS_H

#include <cstdint>
#include "Engine/Source/ShaderEffect/Types.h"

// Not in the original: limits the stock engine assumed but never checked, so converted or newer
// models that exceed them are refused instead of corrupting memory.

// DebugLog is compiled out of release builds, so refusals go to a log that every build writes.
constexpr const char* M2_LIMITS_LOG = "Logs/Model2.log";

struct M2LegacyEffect {
    const char* vertexShader;
    const char* pixelShader;
    EGxTexOp colorOp;
    EGxTexOp alphaOp;
};

// Bones upload from vertex register c31 at 3 registers each, into a 256-register budget. The
// layout is compiled into the stock .bls shaders, so this cannot grow without new bytecode.
constexpr uint32_t M2_MAX_SECTION_BONES = (256 - 31) / 3;

// Effect ids are batch shader ids with the 0x8000 bit cleared. Id 0 hides a batch and has no
// shaders; ids above 3 have no stock shaders either, so the caller falls back.
inline const M2LegacyEffect* M2FindLegacyEffect(uint32_t id) {
    static const M2LegacyEffect s_effects[] = {
        { "Diffuse_T1_Env", "Combiners_Opaque_Mod2xNA_Alpha", GxTexOp_Mod, GxTexOp_PassThru },
        { "Diffuse_T1_Env", "Combiners_Opaque_AddAlpha", GxTexOp_Mod, GxTexOp_PassThru },
        { "Diffuse_T1_Env", "Combiners_Opaque_AddAlpha_Alpha", GxTexOp_Mod, GxTexOp_PassThru },
    };

    if (id == 0 || id > sizeof(s_effects) / sizeof(s_effects[0])) {
        return nullptr;
    }

    return &s_effects[id - 1];
}

// maxBones is M2_MAX_SECTION_BONES when bones upload to shader registers. CPU skinning has no window.
inline bool M2SkinSectionFits(uint32_t boneCount, uint32_t comboIndex, uint32_t comboCount, uint32_t maxBones) {
    return boneCount > 0
        && boneCount <= maxBones
        && comboIndex <= comboCount
        && boneCount <= comboCount - comboIndex;
}

#endif
