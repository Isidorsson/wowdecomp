#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ConvertSkin;

static bool Convert(const std::vector<uint8_t>& file, ConvertedModel& model) {
    std::string error;
    bool converted = ConvertModel(file, {}, model, error);
    if (!converted) {
        std::printf("    (%s)\n", error.c_str());
    }
    return converted;
}

static bool Refused(const std::vector<uint8_t>& file, const char* reason) {
    ConvertedModel model;
    std::string error;
    return !ConvertModel(file, {}, model, error) && error.find(reason) != std::string::npos;
}

TEST(ModelsUnwrapToMd20Version264) {
    ConvertedModel model;

    CHECK(Convert(Md21File(MinimalMd20(272), { Chunk("SFID", U32s({ 916542 })) }), model));
    CHECK(model.m2.size() >= M2At::HEADER && std::memcmp(model.m2.data(), "MD20", 4) == 0);
    CHECK(GetU32(model.m2, M2At::VERSION) == 264);
    CHECK(model.skins == std::vector<uint32_t>({ 916542 }));
    CHECK(Convert(Md21File(MinimalMd20(274)), model));
}

TEST(SkinsCoverOnlyTheViews) {
    std::vector<uint8_t> md20 = MinimalMd20();
    PutU32(md20, M2At::VIEWS, 2);
    ConvertedModel model;

    // LOD skins follow the views in SFID; 3.3.5 has no use for them.
    CHECK(Convert(Md21File(md20, { Chunk("SFID", U32s({ 10, 11, 12 })) }), model));
    CHECK(model.skins == std::vector<uint32_t>({ 10, 11 }));
    CHECK(Refused(Md21File(md20, { Chunk("SFID", U32s({ 10 })) }), "SFID"));
}

TEST(OnlyMd21Versions272To274AreConverted) {
    CHECK(Refused(Md21File(MinimalMd20(271)), "version"));
    CHECK(Refused(Md21File(MinimalMd20(275)), "version"));
    CHECK(Refused(MinimalMd20(272), "MD21"));
    CHECK(Refused(Chunk("SFID", U32s({ 1 })), "MD21"));
}

TEST(TxidNamesTheFileTextures) {
    std::vector<uint8_t> md20 = MinimalMd20();
    AddArray(md20, M2At::TEXTURES, 2, Concat({ Texture(2), Texture(0) }));
    ConvertedModel model;

    CHECK(Convert(Md21File(md20, { Chunk("TXID", U32s({ 0, 775194 })) }), model));
    size_t replaceable = RecordAt(model.m2, M2At::TEXTURES, 0, 0x10);
    size_t file = RecordAt(model.m2, M2At::TEXTURES, 1, 0x10);
    CHECK(GetU32(model.m2, replaceable) == 2 && StringAt(model.m2, replaceable + 8).empty());
    CHECK(GetU32(model.m2, file) == 0 && StringAt(model.m2, file + 8) == "retail/775194.blp");
}

TEST(SequencesWithoutAfidAreEmbedded) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::SEQUENCES, 2, Concat({ Sequence(0, 0x0), Sequence(4, 0x1) }));
    ConvertedModel model;

    CHECK(Convert(Md21File(md20), model));
    CHECK(GetU32(model.m2, RecordAt(model.m2, M2At::SEQUENCES, 0, 0x40) + 0x0C) == 0x20);
    CHECK(GetU32(model.m2, RecordAt(model.m2, M2At::SEQUENCES, 1, 0x40) + 0x0C) == 0x21);
}

TEST(FlagsAre3_3_5Masked) {
    std::vector<uint8_t> md20 = NewMd20();
    PutU32(md20, M2At::FLAGS, 0x80 | 0x200 | 0x8 | 0x1);
    AddArray(md20, M2At::BONES, 1, Bone(0x400 | 0x200 | 0x8));
    AddArray(md20, M2At::MATERIALS, 2, Concat({ Material(0x200 | 0x10, 7), Material(0x1, 2) }));
    AddArray(md20, M2At::TEXTURES, 1, Texture(16));
    ConvertedModel model;

    CHECK(Convert(Md21File(md20), model));
    CHECK(GetU32(model.m2, M2At::FLAGS) == 0x9);
    CHECK(GetU32(model.m2, RecordAt(model.m2, M2At::BONES, 0, 0x58) + 4) == 0x208);
    size_t materials = RecordAt(model.m2, M2At::MATERIALS, 0, 4);
    CHECK(GetU16(model.m2, materials) == 0x10 && GetU16(model.m2, materials + 2) == 4);
    CHECK(GetU16(model.m2, materials + 4) == 0x1 && GetU16(model.m2, materials + 6) == 2);
    CHECK(GetU32(model.m2, RecordAt(model.m2, M2At::TEXTURES, 0, 0x10)) == 0);
}

TEST(LaterSliceFeaturesAreRefusedByName) {
    std::vector<uint8_t> md20 = MinimalMd20();

    CHECK(Refused(Md21File(md20, { Chunk("SKID", U32s({ 1 })) }), "SKID"));


    std::vector<uint8_t> particles = MinimalMd20();
    AddArray(particles, M2At::PARTICLES, 1, std::vector<uint8_t>(492, 0));
    CHECK(Refused(Md21File(particles), "particle"));
}

TEST(ArraysPastTheBlobAreRefused) {
    std::vector<uint8_t> md20 = MinimalMd20();
    PutU32(md20, M2At::BONES, 1000);

    CHECK(Refused(Md21File(md20), "bones"));
    CHECK(Refused(Md21File(std::vector<uint8_t>(md20.begin(), md20.begin() + 0x100)), "header"));
}

TEST(ModelsGetTheEffectCoordWindow) {
    std::vector<uint8_t> md20 = MinimalMd20();
    AddArray(md20, M2At::COORD_COMBOS, 1, Concat({ { 1, 0 } }));
    ConvertedModel model;

    // UV1 for the first texture, the environment map (-1) for the second: what Diffuse_T1_Env samples.
    CHECK(Convert(Md21File(md20), model));
    CHECK(U16sAt(model.m2, M2At::COORD_COMBOS) == std::vector<uint16_t>({ 1, 0, 0xFFFF }));
}

// A retail model file with vertices weighing one bone and two bones, two texture combos and one coord
// combo, and its retail skin.
static std::vector<uint8_t> WeightedModel(uint32_t views) {
    std::vector<uint8_t> md20 = NewMd20();
    PutU32(md20, M2At::VIEWS, views);
    AddArray(md20, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(md20, M2At::VERTICES, 2, Concat({ Vertex(255, 0, 0, 0), Vertex(128, 127, 0, 0) }));
    AddArray(md20, M2At::TEXTURE_COMBOS, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(md20, M2At::COORD_COMBOS, 1, Concat({ { 1, 0 } }));
    return Md21File(md20);
}

static std::vector<uint8_t> RetailSkin(std::vector<std::vector<uint8_t>> batches, uint16_t influences = 0, uint16_t level = 0) {
    std::vector<uint8_t> skin = NewSkin();
    AddArray(skin, SkinAt::VERTICES, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(skin, SkinAt::INDICES, 3, Concat({ { 0, 0 }, { 1, 0 }, { 1, 0 } }));
    AddArray(skin, SkinAt::SECTIONS, 1, SkinSection(1, 0, influences, 3, level));
    AddArray(skin, SkinAt::BATCHES, static_cast<uint32_t>(batches.size()), Concat(batches));
    return skin;
}

static uint16_t BatchShader(const std::vector<uint8_t>& skin, size_t batch) {
    return GetU16(skin, RecordAt(skin, SkinAt::BATCHES, batch, 0x18) + 2);
}

TEST(RetailEffectsZeroToTwoBecomeClientEffects) {
    std::vector<uint8_t> out;
    std::string error;

    CHECK(ConvertSkin(RetailSkin({ SkinBatch(0, 0x8000, 2), SkinBatch(0, 0x8002, 2), SkinBatch(0, 0x0010, 1) }), WeightedModel(1), 0, out, error));
    CHECK(BatchShader(out, 0) == 0x8001);
    CHECK(BatchShader(out, 1) == 0x8003);
    CHECK(BatchShader(out, 2) == 0);
}

static uint16_t BatchCoordCombo(const std::vector<uint8_t>& skin, size_t batch) {
    return GetU16(skin, RecordAt(skin, SkinAt::BATCHES, batch, 0x18) + 0x12);
}

TEST(BatchesReadCoordCombosInsideTheTable) {
    std::vector<uint8_t> out;
    std::string error;

    // Window [0, -1] lands at index 1, after the model's own combo.
    CHECK(ConvertSkin(RetailSkin({ SkinBatch(0, 0x8000, 2, 0, 0, 0), SkinBatch(0, 0, 1, 0, 0, 0), SkinBatch(0, 0, 1, 0, 0, 7) }), WeightedModel(1), 0, out, error));
    CHECK(BatchCoordCombo(out, 0) == 1);
    CHECK(BatchCoordCombo(out, 1) == 0);
    CHECK(BatchCoordCombo(out, 2) == 1);
}

TEST(BatchesPastTheTextureCombosAreRefused) {
    std::vector<uint8_t> out;
    std::string error;

    CHECK(!ConvertSkin(RetailSkin({ SkinBatch(0, 0x8000, 2, 0, 1, 0) }), WeightedModel(1), 0, out, error) && error.find("texture combo") != std::string::npos);
}

TEST(SkinBatchesLaterSlicesConvertAreRefused) {
    std::vector<uint8_t> m2 = WeightedModel(1);
    std::vector<uint8_t> out;
    std::string error;

    CHECK(!ConvertSkin(RetailSkin({ SkinBatch(0, 0x8003, 2) }), m2, 0, out, error) && error.find("effect") != std::string::npos);
    CHECK(!ConvertSkin(RetailSkin({ SkinBatch(0, 0x0011, 2) }), m2, 0, out, error) && error.find("shader") != std::string::npos);
    CHECK(!ConvertSkin(RetailSkin({ SkinBatch(0, 0x8000, 3) }), m2, 0, out, error) && error.find("texture") != std::string::npos);
    CHECK(!ConvertSkin(RetailSkin({ SkinBatch(0, 0, 1) }, 1, 1), m2, 0, out, error) && error.find("Level") != std::string::npos);
}

TEST(SkinBatchFlagsAre3_3_5Masked) {
    std::vector<uint8_t> out;
    std::string error;

    std::vector<uint8_t> secondSection = SkinBatch(0x80 | 0x10, 0, 1, 0x8);
    PutU16(secondSection, 0x04, 1);
    std::vector<uint8_t> skin = RetailSkin({ secondSection });
    AddArray(skin, SkinAt::SECTIONS, 2, Concat({ SkinSection(1, 0, 0, 3), SkinSection(1, 0, 0, 3) }));

    // Stock 3.3.5 files keep the old geosetIndex equal to the submesh index; retail reuses it as flags2.
    CHECK(ConvertSkin(skin, WeightedModel(1), 0, out, error));
    size_t batch = RecordAt(out, SkinAt::BATCHES, 0, 0x18);
    CHECK(out[batch] == 0x10);
    CHECK(GetU16(out, batch + 6) == 1);
}

TEST(BoneInfluencesFitTheVertices) {
    std::vector<uint8_t> out;
    std::string error;

    CHECK(ConvertSkin(RetailSkin({ SkinBatch(0, 0, 1) }, 0), WeightedModel(1), 0, out, error));
    CHECK(GetU16(out, RecordAt(out, SkinAt::SECTIONS, 0, 0x30) + 0x10) == 2);
}

TEST(BoneCountMaxIsTheViewsStockPalette) {
    std::vector<uint8_t> out;
    std::string error;

    CHECK(ConvertSkin(RetailSkin({ SkinBatch(0, 0, 1) }), WeightedModel(1), 0, out, error));
    CHECK(GetU32(out, SkinAt::BONE_COUNT_MAX) == 21);
    CHECK(ConvertSkin(RetailSkin({ SkinBatch(0, 0, 1) }), WeightedModel(2), 0, out, error));
    CHECK(GetU32(out, SkinAt::BONE_COUNT_MAX) == 53);
    CHECK(ConvertSkin(RetailSkin({ SkinBatch(0, 0, 1) }), WeightedModel(4), 0, out, error));
    CHECK(GetU32(out, SkinAt::BONE_COUNT_MAX) == 256);
}

TEST(BrokenSkinsAreRefused) {
    std::vector<uint8_t> m2 = WeightedModel(1);
    std::vector<uint8_t> out;
    std::string error;

    std::vector<uint8_t> magic = RetailSkin({ SkinBatch(0, 0, 1) });
    magic[0] = 'X';
    CHECK(!ConvertSkin(magic, m2, 0, out, error));
    CHECK(!ConvertSkin(RetailSkin({ SkinBatch(0, 0, 1) }), m2, 1, out, error) && error.find("view") != std::string::npos);

    std::vector<uint8_t> past = RetailSkin({ SkinBatch(0, 0, 1) });
    PutU32(past, SkinAt::INDICES, 9999);
    CHECK(!ConvertSkin(past, m2, 0, out, error));
}
