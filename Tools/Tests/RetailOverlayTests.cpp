#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"
#include "Tools/Tests/ShadingFixtures.h"

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ConvertSkin;

// Effects that sample more textures than a 3.3.5 batch takes become their base batch and one batch per further
// texture on the same submesh (wow-toolbox-editor downport/transform/passes.rs). Layer materials sit right
// after a copy of their base material, because the client takes a layer's base from materialIndex -
// materialLayer (M2Scene.cpp).

namespace BatchAt {
    constexpr size_t PRIORITY = 0x01;
    constexpr size_t SHADER = 0x02;
    constexpr size_t SECTION = 0x04;
    constexpr size_t MATERIAL = 0x0A;
    constexpr size_t LAYER = 0x0C;
    constexpr size_t TEXTURES = 0x0E;
    constexpr size_t TEXTURE_COMBO = 0x10;
    constexpr size_t WEIGHT_COMBO = 0x14;
    constexpr size_t TRANSFORM_COMBO = 0x16;
}

constexpr uint16_t BLEND_ALPHA = 2;
constexpr uint16_t BLEND_ADD = 4;
constexpr uint16_t BLEND_MOD = 5;
constexpr uint16_t NO_DEPTH_WRITE = 0x10;
// Material m's blocks start at 2 + m * 7: [copy, Add], [copy, Alpha, Alpha], [copy, Mod].
constexpr uint16_t GLOW_BLOCK = 2;
constexpr uint16_t COVER_BLOCK = 4;
constexpr uint16_t PRODUCT_BLOCK = 7;

static std::vector<uint8_t> EffectBatch(uint16_t effect, uint16_t textures, uint16_t layer = 0) {
    std::vector<uint8_t> batch = SkinBatch(0, static_cast<uint16_t>(0x8000 | effect), textures);
    batch[BatchAt::PRIORITY] = 3;
    PutU16(batch, BatchAt::LAYER, layer);
    return batch;
}

static uint16_t Field(const std::vector<uint8_t>& skin, size_t batch, size_t field) {
    return GetU16(skin, RecordAt(skin, SkinAt::BATCHES, batch, BATCH) + field);
}

static std::pair<uint16_t, uint16_t> MaterialAt(const std::vector<uint8_t>& m2, size_t index) {
    size_t at = RecordAt(m2, M2At::MATERIALS, index, 4);
    return { GetU16(m2, at), GetU16(m2, at + 2) };
}

struct Overlaid {
    ConvertedModel model;
    std::vector<uint8_t> skin;
};

static Overlaid ConvertBatches(std::vector<std::vector<uint8_t>> batches) {
    Overlaid out;
    std::string error;
    if (!ConvertModel(ShadedModel(), {}, out.model, error) || !ConvertSkin({ SkinOf(batches), ShadedModel(), 0 }, out.skin, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return out;
}

TEST(ModelsCarryLayerMaterialsAfterACopyOfEachMaterial) {
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(ShadedModel(), {}, model, error));
    CHECK(GetU32(model.m2, M2At::MATERIALS) == 2 + 2 * 7);
    CHECK(MaterialAt(model.m2, 2) == std::make_pair(uint16_t(0x4), uint16_t(0)));
    CHECK(MaterialAt(model.m2, 3) == std::make_pair(uint16_t(0x4 | NO_DEPTH_WRITE), BLEND_ADD));
    CHECK(MaterialAt(model.m2, 4) == std::make_pair(uint16_t(0x4), uint16_t(0)));
    CHECK(MaterialAt(model.m2, 5) == std::make_pair(uint16_t(0x4 | NO_DEPTH_WRITE), BLEND_ALPHA));
    CHECK(MaterialAt(model.m2, 6) == std::make_pair(uint16_t(0x4 | NO_DEPTH_WRITE), BLEND_ALPHA));
    CHECK(MaterialAt(model.m2, 8) == std::make_pair(uint16_t(0x4 | NO_DEPTH_WRITE), BLEND_MOD));
    CHECK(MaterialAt(model.m2, 9) == std::make_pair(uint16_t(0x1), uint16_t(1)));
    CHECK(MaterialAt(model.m2, 10) == std::make_pair(uint16_t(0x1 | NO_DEPTH_WRITE), BLEND_ADD));
}

TEST(ReflectiveGlowBecomesTheClientEffectAndAnAddedPass) {
    // Effect 3, Combiners_Opaque_Mod2xNA_Alpha_Add: effect 0 on textures 1 and 2, texture 3 added on UV1 by
    // the third texture weight.
    Overlaid out = ConvertBatches({ EffectBatch(3, 3) });

    CHECK(GetU32(out.skin, SkinAt::BATCHES) == 2);
    CHECK(Field(out.skin, 0, BatchAt::SHADER) == 0x8001 && Field(out.skin, 0, BatchAt::TEXTURES) == 2);
    CHECK(Field(out.skin, 0, BatchAt::MATERIAL) == GLOW_BLOCK && Field(out.skin, 0, BatchAt::LAYER) == 0);

    CHECK(Shading(out.model.m2, out.skin, 1) == ClientShading({ { MOD }, { UV1 } }));
    CHECK(Field(out.skin, 1, BatchAt::TEXTURE_COMBO) == 2);
    CHECK(Field(out.skin, 1, BatchAt::MATERIAL) == GLOW_BLOCK + 1 && Field(out.skin, 1, BatchAt::LAYER) == 1);
    CHECK(Field(out.skin, 1, BatchAt::WEIGHT_COMBO) == 2 && Field(out.skin, 1, BatchAt::TRANSFORM_COMBO) == 0);
    CHECK(Field(out.skin, 1, BatchAt::SECTION) == 0 && GetU16(out.skin, RecordAt(out.skin, SkinAt::BATCHES, 1, BATCH)) >> 8 == 3);
    // What the client takes as the layer's base material.
    CHECK(MaterialAt(out.model.m2, Field(out.skin, 1, BatchAt::MATERIAL) - Field(out.skin, 1, BatchAt::LAYER)) == MaterialAt(out.model.m2, 0));
}

TEST(CoverOnUv2IsAlphaBlendedWithTheSecondTransform) {
    // Effect 20, Combiners_Opaque_Mod2xNA_Alpha_Alpha.
    Overlaid out = ConvertBatches({ EffectBatch(20, 3) });

    CHECK(GetU32(out.skin, SkinAt::BATCHES) == 2);
    CHECK(Field(out.skin, 0, BatchAt::MATERIAL) == COVER_BLOCK);
    CHECK(Shading(out.model.m2, out.skin, 1) == ClientShading({ { MOD }, { UV2 } }));
    CHECK(Field(out.skin, 1, BatchAt::MATERIAL) == COVER_BLOCK + 1 && Field(out.skin, 1, BatchAt::TRANSFORM_COMBO) == 1);
    CHECK(Field(out.skin, 1, BatchAt::WEIGHT_COMBO) == 0);
}

TEST(CrossfadesStackTwoAlphaPasses) {
    // Effect 18, Combiners_Mod_Dual_Crossfade: texture 1 drawn plain, 2 and 3 faded over it by their weights.
    Overlaid out = ConvertBatches({ EffectBatch(18, 3) });

    CHECK(GetU32(out.skin, SkinAt::BATCHES) == 3);
    CHECK(Shading(out.model.m2, out.skin, 0) == ClientShading({ { MOD }, { UV1 } }));
    CHECK(Field(out.skin, 0, BatchAt::MATERIAL) == COVER_BLOCK);
    for (uint16_t pass = 1; pass <= 2; pass++) {
        CHECK(Field(out.skin, pass, BatchAt::TEXTURE_COMBO) == pass && Field(out.skin, pass, BatchAt::WEIGHT_COMBO) == pass);
        CHECK(Field(out.skin, pass, BatchAt::MATERIAL) == COVER_BLOCK + pass && Field(out.skin, pass, BatchAt::LAYER) == pass);
    }
}

TEST(ProductsMultiplyTheThirdTextureOnUv2) {
    // Effect 30, Combiners_Mod_Mod_Mod_Const.
    Overlaid out = ConvertBatches({ EffectBatch(30, 3) });

    CHECK(GetU32(out.skin, SkinAt::BATCHES) == 2);
    CHECK(Shading(out.model.m2, out.skin, 0) == ClientShading({ { MOD, MOD }, { UV1, UV2 } }));
    CHECK(Field(out.skin, 0, BatchAt::MATERIAL) == PRODUCT_BLOCK);
    CHECK(Shading(out.model.m2, out.skin, 1) == ClientShading({ { MOD }, { UV2 } }));
    CHECK(Field(out.skin, 1, BatchAt::MATERIAL) == PRODUCT_BLOCK + 1);
}

TEST(PassesFollowTheirBaseInTheBatchArray) {
    Overlaid out = ConvertBatches({ SkinBatch(0, 0x0010, 1), EffectBatch(3, 3), SkinBatch(0, 0x0000, 1) });

    CHECK(GetU32(out.skin, SkinAt::BATCHES) == 4);
    CHECK(Field(out.skin, 1, BatchAt::SHADER) == 0x8001);
    CHECK(Field(out.skin, 2, BatchAt::LAYER) == 1 && Field(out.skin, 2, BatchAt::TEXTURES) == 1);
    CHECK(Shading(out.model.m2, out.skin, 3) == ClientShading({ { OPAQUE }, { UV1 } }));
}

TEST(PassesWithoutTheirTextureAreLeftOut) {
    // Effect 3 declaring two textures has no third to add.
    Overlaid out = ConvertBatches({ EffectBatch(3, 2) });

    CHECK(GetU32(out.skin, SkinAt::BATCHES) == 1);
    CHECK(Field(out.skin, 0, BatchAt::SHADER) == 0x8001);
}

TEST(PassesOnALayeredBaseStackAboveIt) {
    Overlaid out = ConvertBatches({ EffectBatch(3, 3, 1) });

    CHECK(GetU32(out.skin, SkinAt::BATCHES) == 2);
    CHECK(Field(out.skin, 1, BatchAt::LAYER) == 2);
}

static ConvertedModel Reported(std::vector<std::vector<uint8_t>> batches) {
    std::vector<uint8_t> skin = SkinOf(batches);
    Retail::ModelSources sources;
    sources.read = [skin](uint32_t, std::vector<uint8_t>& bytes, std::string&) {
        bytes = skin;
        return true;
    };

    ConvertedModel model;
    std::string error;
    if (!ConvertModel(ShadedModel({ Chunk("SFID", U32s({ 7001 })) }), sources, model, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return model;
}

static bool Warned(const ConvertedModel& model, std::vector<const char*> parts) {
    for (const std::string& warning : model.warnings) {
        bool all = true;
        for (const char* part : parts) {
            all &= warning.find(part) != std::string::npos;
        }
        if (all) {
            return true;
        }
    }
    return false;
}

TEST(SplitEffectsAreReportedLikeTheDownporter) {
    ConvertedModel model = Reported({ EffectBatch(3, 3), EffectBatch(18, 3), EffectBatch(3, 2), EffectBatch(3, 3, 1) });

    // Batches 0 and 3 are the same effect with the same pass, so they share an entry.
    CHECK(Warned(model, { "skin 0: batches 0, 3", "shader effect 3 (Combiners_Opaque_Mod2xNA_Alpha_Add)", "Each became 2 batches on the same submesh", "texture 3 added over it" }));
    CHECK(Warned(model, { "shader effect 18", "Each became 3 batches", "texture 2 alpha blended over it, then texture 3 alpha blended over it", "Not reproduced: the crossfade" }));
    CHECK(Warned(model, { "skin 0: batch 2", "texture 3", "not drawn" }));
    CHECK(Warned(model, { "skin 0: batch 3", "base material" }));
}
