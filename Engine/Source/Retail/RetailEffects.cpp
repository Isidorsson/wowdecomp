#include "Engine/Source/Retail/RetailEffects.h"
#include <algorithm>
#include "Engine/Source/Retail/RetailModelData.h"
#include "Engine/Source/Retail/RetailShading.h"

namespace Retail {

// The bits of a retail shader without the effect flag (wow.export ShaderMapper, as wow-toolbox-editor's
// downport/transform/shading.rs decodes them).
constexpr uint16_t SHADER_FIRST_MOD = 0x70;
constexpr uint16_t SHADER_FIRST_ENV = 0x80;
constexpr uint16_t SHADER_SECOND_OP = 0x07;
constexpr uint16_t SHADER_SECOND_ENV = 0x08;
constexpr uint16_t SHADER_LAST_UV2 = 0x4000;

static uint16_t SecondOp(uint16_t shader, uint16_t first) {
    uint16_t op = shader & SHADER_SECOND_OP;

    if (first == OP_OPAQUE && op == 0) {
        return OP_OPAQUE;
    }

    if (first == OP_OPAQUE && op == OP_ADD_NA) {
        return OP_ADD;
    }

    // Decal (2) and Fade (5) have no two-texture 3.3.5 shader.
    return op == OP_ADD || op == OP_MOD2X || op == OP_MOD2X_NA || op == OP_ADD_NA ? op : OP_MOD;
}

// Combiner shaders the client substitutes take two textures at most (SubstituteSimpleShaders).
constexpr uint16_t CLIENT_TEXTURES = 2;

static Shading DecodeShader(uint16_t shader, uint16_t textures) {
    uint16_t first = shader & SHADER_FIRST_MOD ? OP_MOD : OP_OPAQUE;
    bool firstEnv = shader & SHADER_FIRST_ENV;
    bool lastUv2 = shader & SHADER_LAST_UV2;

    if (textures <= 1) {
        return { { first, OP_OPAQUE }, { firstEnv ? ENV : lastUv2 ? UV2 : UV1, UV1 } };
    }

    uint16_t secondCoord = shader & SHADER_SECOND_ENV ? ENV : !firstEnv && lastUv2 ? UV2 : UV1;
    return { { first, SecondOp(shader, first) }, { firstEnv ? ENV : UV1, secondCoord } };
}

constexpr const char* UNLIT_GLOW_LOST = "the glow does not darken the base under it";
constexpr const char* CROSSFADE_LOST = "the crossfade runs on the texture weights of the second and third texture, which the model must animate for it to fade";
constexpr const char* PRODUCT_LOST = "the effect's constant factor is not applied, and every pass multiplies the diffuse light again";
constexpr const char* GUILD_LOST = "the tabard's emblem and border textures are not drawn (they depend on the guild's colours, which a 3.3.5 model cannot take)";

static const Effect EFFECTS[] = {
    { "Combiners_Opaque_Mod2xNA_Alpha", 2, { { OP_OPAQUE, OP_MOD2X_NA }, { UV1, ENV } }, 0, 0, "" },
    { "Combiners_Opaque_AddAlpha", 2, { { OP_OPAQUE, OP_ADD }, { UV1, ENV } }, 1, 0, "" },
    { "Combiners_Opaque_AddAlpha_Alpha", 2, { { OP_OPAQUE, OP_ADD }, { UV1, ENV } }, 2, 0, "" },
    { "Combiners_Opaque_Mod2xNA_Alpha_Add", 2, { { OP_OPAQUE, OP_MOD2X_NA }, { UV1, ENV } }, 0, 1, "", { { 2, BLEND_ADD, UV1, 2 } } },
    { "Combiners_Mod_AddAlpha", 2, { { OP_MOD, OP_ADD }, { UV1, ENV } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_AddAlpha", 2, { { OP_OPAQUE, OP_ADD }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod_AddAlpha", 2, { { OP_MOD, OP_ADD }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod_AddAlpha_Alpha", 2, { { OP_MOD, OP_ADD }, { UV1, ENV } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_Alpha_Alpha", 2, { { OP_OPAQUE, OP_MOD }, { UV1, ENV } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_Mod2xNA_Alpha_3s", 2, { { OP_OPAQUE, OP_MOD2X_NA }, { UV1, ENV } }, 0, 1, "", { { 2, BLEND_ALPHA, UV1, 0 } } },
    { "Combiners_Opaque_AddAlpha_Wgt", 2, { { OP_OPAQUE, OP_ADD }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod_Add_Alpha", 2, { { OP_MOD, OP_ADD }, { UV1, ENV } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_ModNA_Alpha", 2, { { OP_OPAQUE, OP_MOD }, { UV1, ENV } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod_AddAlpha_Wgt", 2, { { OP_MOD, OP_ADD }, { UV1, ENV } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod_AddAlpha_Wgt", 2, { { OP_MOD, OP_ADD }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_AddAlpha_Wgt", 2, { { OP_OPAQUE, OP_ADD }, { UV1, UV2 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_Mod_Add_Wgt", 2, { { OP_OPAQUE, OP_MOD }, { UV1, ENV } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_Mod2xNA_Alpha_UnshAlpha", 2, { { OP_OPAQUE, OP_MOD2X_NA }, { UV1, ENV } }, 0, 1, UNLIT_GLOW_LOST, { { 2, BLEND_ADD, UV1, 2 } } },
    { "Combiners_Mod_Dual_Crossfade", 1, { { OP_MOD, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 2, CROSSFADE_LOST, { { 1, BLEND_ALPHA, UV1, 1 }, { 2, BLEND_ALPHA, UV1, 2 } } },
    { "Combiners_Mod_Depth", 1, { { OP_MOD, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_Mod2xNA_Alpha_Alpha", 2, { { OP_OPAQUE, OP_MOD2X_NA }, { UV1, ENV } }, 0, 1, "", { { 2, BLEND_ALPHA, UV2, 0 } } },
    { "Combiners_Mod_Mod", 2, { { OP_MOD, OP_MOD }, { UV1, UV2 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod_Masked_Dual_Crossfade", 1, { { OP_MOD, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 2,
        "the fourth texture, which masks the result's alpha, is not applied, and the crossfade runs on the texture weights of the second and third texture, which the model must animate for it to fade", { { 1, BLEND_ALPHA, UV1, 1 }, { 2, BLEND_ALPHA, UV1, 2 } } },
    { "Combiners_Opaque_Alpha", 2, { { OP_OPAQUE, OP_MOD }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Opaque_Mod2xNA_Alpha_UnshAlpha", 2, { { OP_OPAQUE, OP_MOD2X_NA }, { UV1, ENV } }, 0, 1, UNLIT_GLOW_LOST, { { 2, BLEND_ADD, UV2, 2 } } },
    { "Combiners_Mod_Depth", 1, { { OP_MOD, OP_OPAQUE }, { ENV, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Guild", 1, { { OP_MOD, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, GUILD_LOST },
    { "Guild_NoBorder", 1, { { OP_MOD, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, GUILD_LOST },
    { "Guild_Opaque", 1, { { OP_OPAQUE, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, GUILD_LOST },
    { "Illum", 1, { { OP_OPAQUE, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "the second texture, which the effect uses as an illumination layer, is not drawn" },
    { "Combiners_Mod_Mod_Mod_Const", 2, { { OP_MOD, OP_MOD }, { UV1, UV2 } }, NO_CLIENT_EFFECT, 1, PRODUCT_LOST, { { 2, BLEND_MOD, UV2, 0 } } },
    { "Combiners_Mod_Mod_Mod_Const", 2, { { OP_MOD, OP_MOD }, { UV1, UV2 } }, NO_CLIENT_EFFECT, 1, PRODUCT_LOST, { { 2, BLEND_MOD, UV2, 0 } } },
    { "Combiners_Opaque", 1, { { OP_OPAQUE, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod_Mod2x", 2, { { OP_MOD, OP_MOD2X }, { UV1, UV2 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod", 1, { { OP_MOD, OP_OPAQUE }, { UV1, UV1 } }, NO_CLIENT_EFFECT, 0, "" },
    { "Combiners_Mod_Mod_Depth", 2, { { OP_MOD, OP_MOD }, { UV1, UV2 } }, NO_CLIENT_EFFECT, 0, "" },
};

constexpr uint16_t EFFECT_COUNT = sizeof(EFFECTS) / sizeof(EFFECTS[0]);
// Effects later builds add: their first texture, which every effect samples.
constexpr Shading UNKNOWN_EFFECT_BASE = { { OP_MOD, OP_OPAQUE }, { UV1, UV1 } };

BaseShading ResolveShading(uint16_t shader, uint16_t textures) {
    if (!(shader & EFFECT_FLAG)) {
        return { nullptr, NO_CLIENT_EFFECT, std::min(textures, CLIENT_TEXTURES), DecodeShader(shader, textures) };
    }

    uint16_t id = shader & ~EFFECT_FLAG;
    if (id >= EFFECT_COUNT) {
        return { nullptr, NO_CLIENT_EFFECT, 1, UNKNOWN_EFFECT_BASE };
    }

    const Effect& effect = EFFECTS[id];
    return { &effect, effect.clientEffect, std::min(textures, effect.textures), effect.base };
}

const Effect& ClientEffect(int16_t index) {
    return EFFECTS[index];
}

std::map<uint32_t, std::string> BatchNames(const std::map<uint32_t, std::vector<uint32_t>>& byView) {
    std::map<uint32_t, std::string> names;
    for (const auto& view : byView) {
        std::string name = "skin " + std::to_string(view.first) + (view.second.size() == 1 ? ": batch " : ": batches ");
        for (size_t i = 0; i < view.second.size(); i++) {
            name += (i ? ", " : "") + std::to_string(view.second[i]);
        }
        names[view.first] = name;
    }
    return names;
}

static const char* const OP_NAMES[] = { "Opaque", "Mod", "Decal", "Add", "Mod2x", "Fade", "Mod2xNoAlpha", "AddNoAlpha" };
static const char* const COORD_NAMES[] = { "Uv1", "Uv2", "Env" };

// The downporter's wording (downport/transform/batch_report.rs) from here on.
static std::string DescribeBase(const BaseShading& base) {
    std::string ops;
    std::string coords;
    for (uint16_t t = 0; t < base.textures; t++) {
        ops += std::string(t ? ", " : "") + OP_NAMES[base.shading.ops[t]];
        coords += std::string(t ? ", " : "") + COORD_NAMES[base.shading.coords[t]];
    }
    return "combiner [" + ops + "] on coordinates [" + coords + "]";
}

static std::string DescribePass(const Pass& pass) {
    const char* blend = pass.blend == BLEND_ALPHA ? "alpha blended" : pass.blend == BLEND_ADD ? "added" : "multiplied";
    return "texture " + std::to_string(pass.texture + 1) + " " + blend + " over it";
}

static std::string EffectName(uint16_t id, const Effect& effect) {
    return "shader effect " + std::to_string(id) + " (" + effect.pixelShader + ")";
}

static std::string Lost(const Effect& effect, const char* lead) {
    return *effect.lost ? std::string(lead) + "Not reproduced: " + effect.lost : "";
}

static std::string Converted(uint16_t id, const BaseShading& base) {
    if (!base.effect) {
        return "used shader effect " + std::to_string(id) + ", which no effect table knows; they draw their first texture with " + DescribeBase(base);
    }

    return "used " + EffectName(id, *base.effect) + ", which 3.3.5 lacks; they use " + DescribeBase(base) + Lost(*base.effect, ". ");
}

static std::string Split(uint16_t id, const Effect& effect, const std::vector<uint16_t>& placed) {
    std::string base = effect.clientEffect == NO_CLIENT_EFFECT ? "the base batch"
        : "the base batch, drawn with the client's effect 0x800" + std::to_string(effect.clientEffect + 1) + " (" + ClientEffect(effect.clientEffect).pixelShader + ")";
    std::string passes;
    for (uint16_t p = 0; p < effect.passCount; p++) {
        if (std::find(placed.begin(), placed.end(), effect.passes[p].texture) != placed.end()) {
            passes += (passes.empty() ? "" : ", then ") + DescribePass(effect.passes[p]);
        }
    }

    return "used " + EffectName(id, effect) + ", which 3.3.5 lacks and which samples more textures than one batch takes. Each became " + std::to_string(placed.size() + 1)
        + " batches on the same submesh: " + base + ", then " + passes + "." + Lost(effect, " ");
}

// One view's effect batches, grouped for one entry per effect (and per set of drawn passes).
struct EffectBatches {
    std::map<uint16_t, std::vector<uint32_t>> converted;
    std::map<std::pair<uint16_t, std::vector<uint16_t>>, std::vector<uint32_t>> split;
    std::map<std::pair<uint16_t, uint16_t>, std::vector<uint32_t>> missing;
    std::map<uint16_t, std::vector<uint32_t>> layered;
    std::map<uint16_t, std::vector<uint32_t>> dropped;
};

static void SortBatch(const std::vector<uint8_t>& skin, size_t at, uint32_t index, uint32_t textureCombos, EffectBatches& sorted) {
    uint16_t id = Get<uint16_t>(skin, at + 0x02) & ~EFFECT_FLAG;
    uint16_t declared = Get<uint16_t>(skin, at + 0x0E);
    BaseShading base = ResolveShading(EFFECT_FLAG | id, declared);

    if (!base.effect || !base.effect->passCount) {
        sorted.converted[id].push_back(index);
        return;
    }

    std::vector<uint16_t> placed;
    for (uint16_t p = 0; p < base.effect->passCount; p++) {
        const Pass& pass = base.effect->passes[p];
        if (PassFits(pass, declared, Get<uint16_t>(skin, at + 0x10), textureCombos)) {
            placed.push_back(pass.texture);
        } else {
            sorted.missing[{ id, pass.texture }].push_back(index);
        }
    }

    if (placed.empty()) {
        sorted.converted[id].push_back(index);
    } else {
        sorted.split[{ id, placed }].push_back(index);
    }

    if (Get<uint16_t>(skin, at + 0x0C)) {
        sorted.layered[id].push_back(index);
    }
}

static void Report(uint32_t view, const EffectBatches& sorted, std::vector<std::string>& warnings) {
    auto names = [view](const std::vector<uint32_t>& batches) { return BatchNames({ { view, batches } }).at(view) + ": "; };
    auto effectOf = [](uint16_t id) -> const Effect& { return *ResolveShading(EFFECT_FLAG | id, 2).effect; };

    for (const auto& entry : sorted.converted) {
        warnings.push_back(names(entry.second) + Converted(entry.first, ResolveShading(EFFECT_FLAG | entry.first, 2)));
    }

    for (const auto& entry : sorted.split) {
        warnings.push_back(names(entry.second) + Split(entry.first.first, effectOf(entry.first.first), entry.first.second));
    }

    for (const auto& entry : sorted.missing) {
        warnings.push_back(names(entry.second) + EffectName(entry.first.first, effectOf(entry.first.first)) + " has no texture " + std::to_string(entry.first.second + 1)
            + " for its pass, so it is not drawn");
    }

    for (const auto& entry : sorted.dropped) {
        uint16_t declared = entry.first;
        std::string past = declared == CLIENT_TEXTURES + 1 ? "texture " + std::to_string(declared) : "textures " + std::to_string(CLIENT_TEXTURES + 1) + " to " + std::to_string(declared);
        warnings.push_back(names(entry.second) + std::to_string(declared) + " textures without a shader effect, of which a 3.3.5 batch draws the first "
            + std::to_string(CLIENT_TEXTURES) + ", so " + past + " is not drawn");
    }

    for (const auto& entry : sorted.layered) {
        warnings.push_back(names(entry.second) + EffectName(entry.first, effectOf(entry.first))
            + " sits on a material layer above 0, so the client takes the base material of its passes from another material than the base's");
    }
}

bool ReportEffects(const std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, std::vector<std::string>& warnings, std::string& error) {
    Span textureCombos;
    if (!ReadSpan(m2, M2At::TEXTURE_COMBOS, 2, "texture combos", textureCombos, error)) {
        return false;
    }

    for (uint32_t view = 0; view < skins.size(); view++) {
        Span records;
        if (!ReadSpan(skins[view], SkinAt::BATCHES, BATCH_SIZE, "batches", records, error)) {
            return false;
        }

        EffectBatches sorted;
        for (uint32_t i = 0; i < records.count; i++) {
            size_t at = records.At(i, BATCH_SIZE);
            uint16_t shader = Get<uint16_t>(skins[view], at + 0x02);
            uint16_t declared = Get<uint16_t>(skins[view], at + 0x0E);
            if ((shader & EFFECT_FLAG) && (shader & ~EFFECT_FLAG) >= CLIENT_EFFECTS) {
                SortBatch(skins[view], at, i, textureCombos.count, sorted);
            } else if (!(shader & EFFECT_FLAG) && declared > CLIENT_TEXTURES) {
                sorted.dropped[declared].push_back(i);
            }
        }

        Report(view, sorted, warnings);
    }

    return true;
}

}
