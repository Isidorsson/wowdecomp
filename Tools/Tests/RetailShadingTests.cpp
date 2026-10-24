#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ConvertSkin;

// 3.3.5 combiner operations (M2COMBINER) and texture coordinate sources (textureCoordCombos values).
constexpr uint16_t OPAQUE = 0;
constexpr uint16_t MOD = 1;
constexpr uint16_t ADD = 3;
constexpr uint16_t MOD2X = 4;
constexpr uint16_t MOD2X_NA = 6;
constexpr uint16_t ADD_NA = 7;
constexpr uint16_t UV1 = 0;
constexpr uint16_t UV2 = 1;
constexpr uint16_t ENV = 0xFFFF;
constexpr uint32_t USES_COMBINER_COMBOS = 0x8;
constexpr size_t BATCH = 0x18;

// A retail model whose header ends at 0x130, as every retail file's does: the first array (the name here)
// starts where 3.3.5 keeps the combiner table.
static std::vector<uint8_t> ShadedModel() {
    std::vector<uint8_t> md20 = NewMd20();
    md20.resize(M2At::TEXTURE_COMBINER_COMBOS);
    std::string name = "kobold";
    AddArray(md20, 0x08, static_cast<uint32_t>(name.size() + 1), std::vector<uint8_t>(name.c_str(), name.c_str() + name.size() + 1));
    AddArray(md20, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(md20, M2At::VERTICES, 2, Concat({ Vertex(255, 0, 0, 0), Vertex(255, 0, 0, 0) }));
    AddArray(md20, M2At::TEXTURE_COMBOS, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(md20, M2At::COORD_COMBOS, 1, Concat({ { 1, 0 } }));
    return Md21File(md20);
}

static std::vector<uint8_t> SkinOf(std::vector<std::vector<uint8_t>> batches) {
    std::vector<uint8_t> skin = NewSkin();
    AddArray(skin, SkinAt::VERTICES, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(skin, SkinAt::INDICES, 3, Concat({ { 0, 0 }, { 1, 0 }, { 1, 0 } }));
    AddArray(skin, SkinAt::SECTIONS, 1, SkinSection(1, 0, 1, 3));
    AddArray(skin, SkinAt::BATCHES, static_cast<uint32_t>(batches.size()), Concat(batches));
    return skin;
}

// What SubstituteSimpleShaders and GetEffect (M2Shared.cpp) read for a batch: one combiner operation and one
// coordinate source per texture.
struct ClientShading {
    std::vector<uint16_t> ops;
    std::vector<uint16_t> coords;

    bool operator==(const ClientShading& other) const { return ops == other.ops && coords == other.coords; }
};

static ClientShading Shading(const std::vector<uint8_t>& m2, const std::vector<uint8_t>& skin, size_t index) {
    size_t batch = RecordAt(skin, SkinAt::BATCHES, index, BATCH);
    std::vector<uint16_t> combos = U16sAt(m2, M2At::TEXTURE_COMBINER_COMBOS);
    std::vector<uint16_t> coords = U16sAt(m2, M2At::COORD_COMBOS);
    uint16_t shader = GetU16(skin, batch + 0x02);
    uint16_t coord = GetU16(skin, batch + 0x12);
    ClientShading shading;

    for (uint16_t t = 0; t < GetU16(skin, batch + 0x0E); t++) {
        shading.ops.push_back(size_t(shader) + t < combos.size() ? combos[shader + t] : 0xEEEE);
        shading.coords.push_back(size_t(coord) + t < coords.size() ? coords[coord + t] : 0xEEEE);
    }

    return shading;
}

// Converts the model and a skin holding one batch per (shader, texture count) and returns what the client
// reads for each.
static std::vector<ClientShading> Converted(const std::vector<std::pair<uint16_t, uint16_t>>& batches) {
    std::vector<std::vector<uint8_t>> records;
    for (const auto& batch : batches) {
        records.push_back(SkinBatch(0, batch.first, batch.second));
    }

    ConvertedModel model;
    std::vector<uint8_t> skin;
    std::string error;
    if (!ConvertModel(ShadedModel(), {}, model, error) || !ConvertSkin(SkinOf(records), ShadedModel(), 0, skin, error)) {
        std::printf("    (%s)\n", error.c_str());
        return {};
    }

    std::vector<ClientShading> shadings;
    for (size_t i = 0; i < batches.size(); i++) {
        shadings.push_back(Shading(model.m2, skin, i));
    }
    return shadings;
}

TEST(ModelsCarryEveryCombinerPair) {
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(ShadedModel(), {}, model, error));
    CHECK(GetU32(model.m2, M2At::FLAGS) & USES_COMBINER_COMBOS);
    std::vector<uint16_t> combos = U16sAt(model.m2, M2At::TEXTURE_COMBINER_COMBOS);
    CHECK(combos.size() == 128);
    CHECK(combos.size() == 128 && combos[2 * (MOD * 8 + MOD2X)] == MOD && combos[2 * (MOD * 8 + MOD2X) + 1] == MOD2X);
    CHECK(combos.size() == 128 && combos[2 * (ADD_NA * 8 + ADD_NA)] == ADD_NA);
}

TEST(TheArrayInTheCombinerSlotMoves) {
    ConvertedModel model;
    std::string error;

    CHECK(ConvertModel(ShadedModel(), {}, model, error));
    CHECK(StringAt(model.m2, 0x08) == "kobold");
    CHECK(GetU32(model.m2, 0x08 + 4) >= M2At::HEADER);

    std::vector<uint8_t> sequencesFirst = NewMd20();
    sequencesFirst.resize(M2At::TEXTURE_COMBINER_COMBOS);
    AddArray(sequencesFirst, M2At::SEQUENCES, 1, Sequence(7, 0x20));
    AddArray(sequencesFirst, M2At::VERTICES, 1, Vertex(255, 0, 0, 0));
    CHECK(ConvertModel(Md21File(sequencesFirst), {}, model, error));
    CHECK(GetU16(model.m2, RecordAt(model.m2, M2At::SEQUENCES, 0, 0x40)) == 7);
    CHECK(GetU32(model.m2, M2At::SEQUENCES + 4) >= M2At::HEADER);
}

TEST(ModelsCarryEveryCoordPair) {
    ConvertedModel model;
    std::string error;

    // The model's own combo, then (UV1, UV2, Env) x (UV1, UV2, Env).
    CHECK(ConvertModel(ShadedModel(), {}, model, error));
    CHECK(U16sAt(model.m2, M2At::COORD_COMBOS) == std::vector<uint16_t>({ 1, UV1, UV1, UV1, UV2, UV1, ENV, UV2, UV1, UV2, UV2, UV2, ENV, ENV, UV1, ENV, UV2, ENV, ENV }));
}

TEST(SingleTexturesKeepTheirOperationAndCoords) {
    auto shadings = Converted({ { 0x0000, 1 }, { 0x0010, 1 }, { 0x0090, 1 }, { 0x4010, 1 } });

    CHECK(shadings.size() == 4);
    CHECK(shadings.size() == 4 && shadings[0] == ClientShading({ { OPAQUE }, { UV1 } }));
    CHECK(shadings.size() == 4 && shadings[1] == ClientShading({ { MOD }, { UV1 } }));
    CHECK(shadings.size() == 4 && shadings[2] == ClientShading({ { MOD }, { ENV } }));
    CHECK(shadings.size() == 4 && shadings[3] == ClientShading({ { MOD }, { UV2 } }));
}

// wow-toolbox-editor downport/transform/shading.rs decode_classic, which follows wow.export's ShaderMapper.
TEST(TwoTextureShadersDecodeLikeTheDownporter) {
    auto shadings = Converted({ { 0x0014, 2 }, { 0x000E, 2 }, { 0x0007, 2 }, { 0x0017, 2 }, { 0x4011, 2 }, { 0x40D1, 2 }, { 0x0012, 2 }, { 0x0002, 2 }, { 0x0000, 2 }, { 0x0013, 2 } });

    CHECK(shadings.size() == 10);
    if (shadings.size() != 10) {
        return;
    }

    CHECK(shadings[0] == ClientShading({ { MOD, MOD2X }, { UV1, UV1 } }));
    CHECK(shadings[1] == ClientShading({ { OPAQUE, MOD2X_NA }, { UV1, ENV } }));
    CHECK(shadings[2] == ClientShading({ { OPAQUE, ADD }, { UV1, UV1 } }));
    CHECK(shadings[3] == ClientShading({ { MOD, ADD_NA }, { UV1, UV1 } }));
    CHECK(shadings[4] == ClientShading({ { MOD, MOD }, { UV1, UV2 } }));
    // An environment-mapped first texture leaves the second on UV1, whatever bit 0x4000 says.
    CHECK(shadings[5] == ClientShading({ { MOD, MOD }, { ENV, UV1 } }));
    // Second operations 3.3.5 shaders lack (2, 5) become Mod; Opaque with nothing stays Opaque.
    CHECK(shadings[6] == ClientShading({ { MOD, MOD }, { UV1, UV1 } }));
    CHECK(shadings[7] == ClientShading({ { OPAQUE, MOD }, { UV1, UV1 } }));
    CHECK(shadings[8] == ClientShading({ { OPAQUE, OPAQUE }, { UV1, UV1 } }));
    CHECK(shadings[9] == ClientShading({ { MOD, ADD }, { UV1, UV1 } }));
}

TEST(EffectBatchesReadUv1AndTheEnvironmentMap) {
    ConvertedModel model;
    std::vector<uint8_t> skin;
    std::string error;

    CHECK(ConvertModel(ShadedModel(), {}, model, error));
    CHECK(ConvertSkin(SkinOf({ SkinBatch(0, 0x8000, 2) }), ShadedModel(), 0, skin, error));
    size_t batch = RecordAt(skin, SkinAt::BATCHES, 0, BATCH);
    CHECK(GetU16(skin, batch + 0x02) == 0x8001);
    std::vector<uint16_t> coords = U16sAt(model.m2, M2At::COORD_COMBOS);
    uint16_t coord = GetU16(skin, batch + 0x12);
    CHECK(size_t(coord) + 1 < coords.size() && coords[coord] == UV1 && coords[coord + 1] == ENV);
}
