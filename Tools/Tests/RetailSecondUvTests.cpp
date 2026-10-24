#include <map>
#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ModelSources;

// A 3.3.5 two-texture batch always draws its second texture with UV2 (Diffuse_T1_T2), so retail batches that
// sample UV1 twice get UV1 copied into UV2 on the vertices they draw, unless another batch reads UV2 there.

constexpr size_t UV1_AT = 0x20;
constexpr size_t UV2_AT = 0x28;
constexpr uint32_t SKIN_A = 5001;
constexpr uint32_t SKIN_B = 5002;

// Four vertices; vertex v has UV1 (v + 1, 0) and UV2 (0, v + 10).
static std::vector<uint8_t> UvModel(uint32_t views) {
    std::vector<uint8_t> vertices;
    for (int v = 0; v < 4; v++) {
        std::vector<uint8_t> vertex = Vertex(255, 0, 0, 0);
        float uv[4] = { float(v + 1), 0.0f, 0.0f, float(v + 10) };
        std::memcpy(vertex.data() + UV1_AT, uv, sizeof(uv));
        vertices.insert(vertices.end(), vertex.begin(), vertex.end());
    }

    std::vector<uint8_t> md20 = MinimalMd20();
    PutU32(md20, M2At::VIEWS, views);
    AddArray(md20, M2At::VERTICES, 4, vertices);
    std::vector<uint32_t> skins = { SKIN_A, SKIN_B };
    skins.resize(views);
    return Md21File(md20, { Chunk("SFID", U32s(skins)) });
}

// Submesh 0 draws vertices 0 and 1, submesh 1 vertices 2 and 3.
static std::vector<uint8_t> UvSkin(const std::vector<std::vector<uint8_t>>& batches) {
    std::vector<uint8_t> skin = NewSkin();
    AddArray(skin, SkinAt::VERTICES, 4, Concat({ { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 } }));
    std::vector<uint8_t> first = SkinSection(1, 0, 1, 0);
    std::vector<uint8_t> second = SkinSection(1, 0, 1, 0);
    PutU16(first, 0x06, 2);
    PutU16(second, 0x04, 2);
    PutU16(second, 0x06, 2);
    AddArray(skin, SkinAt::SECTIONS, 2, Concat({ first, second }));
    AddArray(skin, SkinAt::BATCHES, static_cast<uint32_t>(batches.size()), Concat(batches));
    return skin;
}

// A batch of submesh section.
static std::vector<uint8_t> On(uint16_t section, uint16_t shader, uint16_t textures) {
    std::vector<uint8_t> batch = SkinBatch(0, shader, textures);
    PutU16(batch, 0x04, section);
    return batch;
}

static ModelSources Skins(std::map<uint32_t, std::vector<uint8_t>> files) {
    ModelSources sources;
    sources.read = [files](uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
        auto file = files.find(id);
        if (file == files.end()) {
            error = "no such file";
            return false;
        }
        bytes = file->second;
        return true;
    };
    return sources;
}

static bool Uv2IsUv1(const std::vector<uint8_t>& m2, size_t vertex) {
    size_t at = RecordAt(m2, M2At::VERTICES, vertex, 0x30);
    return std::memcmp(m2.data() + at + UV1_AT, m2.data() + at + UV2_AT, 8) == 0;
}

static bool Mentions(const ConvertedModel& model, const char* text) {
    for (const std::string& warning : model.warnings) {
        if (warning.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static ConvertedModel Converted(uint32_t views, std::map<uint32_t, std::vector<uint8_t>> skins) {
    ConvertedModel model;
    std::string error;
    if (!ConvertModel(UvModel(views), Skins(skins), model, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return model;
}

TEST(Uv1PairsCopyTheFirstSetIntoTheSecond) {
    // 0x0014: Mod, Mod2x on (UV1, UV1). Submesh 1 has one texture on UV1 and stays as it is.
    ConvertedModel model = Converted(1, { { SKIN_A, UvSkin({ On(0, 0x0014, 2), On(1, 0x0000, 1) }) } });

    CHECK(model.m2.size() > 0);
    CHECK(Uv2IsUv1(model.m2, 0) && Uv2IsUv1(model.m2, 1));
    CHECK(!Uv2IsUv1(model.m2, 2) && !Uv2IsUv1(model.m2, 3));
    CHECK(model.warnings.size() == 1 && Mentions(model, "skin 0") && Mentions(model, "copied"));
}

TEST(VerticesAnotherBatchReadsWithUv2AreKept) {
    // 0x4011: Mod, Mod on (UV1, UV2), on the same submesh.
    ConvertedModel model = Converted(1, { { SKIN_A, UvSkin({ On(0, 0x0014, 2), On(0, 0x4011, 2) }) } });

    CHECK(model.m2.size() > 0);
    CHECK(!Uv2IsUv1(model.m2, 0) && !Uv2IsUv1(model.m2, 1));
    CHECK(Mentions(model, "kept"));
}

TEST(SingleTexturesOnUv2KeepThemToo) {
    // 0x4010: one texture on UV2 (Diffuse_T2).
    ConvertedModel model = Converted(1, { { SKIN_A, UvSkin({ On(0, 0x0014, 2), On(0, 0x4010, 1) }) } });

    CHECK(model.m2.size() > 0);
    CHECK(!Uv2IsUv1(model.m2, 0) && !Uv2IsUv1(model.m2, 1));
    CHECK(Mentions(model, "kept"));
}

TEST(OtherViewsReadingUv2KeepThem) {
    ConvertedModel model = Converted(2, { { SKIN_A, UvSkin({ On(0, 0x0014, 2) }) }, { SKIN_B, UvSkin({ On(0, 0x4011, 2) }) } });

    CHECK(model.m2.size() > 0);
    CHECK(!Uv2IsUv1(model.m2, 0) && !Uv2IsUv1(model.m2, 1));
}

TEST(UnreadSkinsCopyNothing) {
    ConvertedModel model = Converted(2, { { SKIN_A, UvSkin({ On(0, 0x0014, 2) }) } });

    CHECK(model.m2.size() > 0);
    CHECK(!Uv2IsUv1(model.m2, 0) && !Uv2IsUv1(model.m2, 1));
    CHECK(Mentions(model, "not read"));
}

TEST(EffectsAndEnvironmentPairsAreNotCopied) {
    // Effect 0 draws Diffuse_T1_Env; 0x000E is (UV1, Env).
    ConvertedModel model = Converted(1, { { SKIN_A, UvSkin({ On(0, 0x8000, 2), On(1, 0x000E, 2) }) } });

    CHECK(model.m2.size() > 0);
    CHECK(!Uv2IsUv1(model.m2, 0) && !Uv2IsUv1(model.m2, 2));
    CHECK(model.warnings.empty());
}

TEST(EffectBasesCountForTheCopy) {
    // Effect 23 (Combiners_Opaque_Alpha) draws (UV1, UV1); effect 21 (Combiners_Mod_Mod) reads UV2.
    ConvertedModel copied = Converted(1, { { SKIN_A, UvSkin({ On(0, 0x8017, 2) }) } });
    ConvertedModel kept = Converted(1, { { SKIN_A, UvSkin({ On(0, 0x8017, 2), On(0, 0x8015, 2) }) } });

    CHECK(copied.m2.size() > 0 && Uv2IsUv1(copied.m2, 0) && Uv2IsUv1(copied.m2, 1));
    CHECK(kept.m2.size() > 0 && !Uv2IsUv1(kept.m2, 0));
    CHECK(Mentions(kept, "kept"));
}
