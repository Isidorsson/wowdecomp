#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"

using Retail::ConvertedModel;
using Retail::ConvertModel;
using Retail::ModelSources;

constexpr size_t RIBBON_SIZE = 0xB0;
constexpr size_t RIBBON_TEXTURES = 0x14;
constexpr size_t RIBBON_MATERIALS = 0x1C;

// Appends a uint16 list 16-byte aligned; returns where it starts.
static uint32_t AppendList(std::vector<uint8_t>& md20, const std::vector<uint16_t>& values) {
    md20.resize((md20.size() + 15) & ~size_t(15), 0);
    uint32_t at = static_cast<uint32_t>(md20.size());
    for (uint16_t value : values) {
        md20.insert(md20.end(), { uint8_t(value), uint8_t(value >> 8) });
    }
    return at;
}

// A ribbon emitter record whose texture and material lists are appended to md20.
static std::vector<uint8_t> Ribbon(std::vector<uint8_t>& md20, const std::vector<uint16_t>& textures, const std::vector<uint16_t>& materials) {
    std::vector<uint8_t> record(RIBBON_SIZE, 0);
    PutU32(record, 0x00, 0xFFFFFFFF);
    PutU32(record, RIBBON_TEXTURES, static_cast<uint32_t>(textures.size()));
    PutU32(record, RIBBON_TEXTURES + 4, AppendList(md20, textures));
    PutU32(record, RIBBON_MATERIALS, static_cast<uint32_t>(materials.size()));
    PutU32(record, RIBBON_MATERIALS + 4, AppendList(md20, materials));
    return record;
}

static std::vector<uint8_t> ModelWithRibbons(const std::vector<std::pair<std::vector<uint16_t>, std::vector<uint16_t>>>& ribbons) {
    std::vector<uint8_t> md20 = MinimalMd20(274);
    std::vector<std::vector<uint8_t>> textures(9, Texture(0));
    AddArray(md20, M2At::TEXTURES, 9, Concat(textures));
    AddArray(md20, M2At::MATERIALS, 3, Concat({ Material(0, 0), Material(0, 0), Material(0x1255, 4) }));
    std::vector<std::vector<uint8_t>> records;
    for (const auto& ribbon : ribbons) {
        records.push_back(Ribbon(md20, ribbon.first, ribbon.second));
    }
    AddArray(md20, M2At::RIBBONS, static_cast<uint32_t>(records.size()), Concat(records));
    return md20;
}

static bool Convert(const std::vector<uint8_t>& md20, ConvertedModel& model) {
    std::string error;
    bool converted = ConvertModel(Md21File(md20), ModelSources(), model, error);
    if (!converted) {
        std::printf("    (%s)\n", error.c_str());
    }
    return converted;
}

static bool Warned(const ConvertedModel& model, const std::string& text) {
    for (const std::string& warning : model.warnings) {
        if (warning.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// The Voidwing Dragonhawk (6375144) ribbons: textures 6, 7, 8 (a trail with alpha, a noise and an opaque 8x8 grey),
// one additive material. The client draws a pass per texture with the material of the same slot, so the grey drew
// as flat quads with a material read past the list.
TEST(RibbonsKeepOneTexturePerMaterial) {
    ConvertedModel model;

    CHECK(Convert(ModelWithRibbons({ { { 6, 7, 8 }, { 2 } } }), model));
    size_t ribbon = RecordAt(model.m2, M2At::RIBBONS, 0, RIBBON_SIZE);
    CHECK(GetU32(model.m2, ribbon + RIBBON_TEXTURES) == 1);
    CHECK(GetU16(model.m2, GetU32(model.m2, ribbon + RIBBON_TEXTURES + 4)) == 6);
    CHECK(GetU32(model.m2, ribbon + RIBBON_MATERIALS) == 1);
    CHECK(Warned(model, "ribbon emitter 0: draws texture 6 only; textures 7, 8 have no material"));
}

TEST(RibbonsWithAMaterialPerTextureAreKept) {
    ConvertedModel model;

    CHECK(Convert(ModelWithRibbons({ { { 1, 2 }, { 0, 2 } } }), model));
    size_t ribbon = RecordAt(model.m2, M2At::RIBBONS, 0, RIBBON_SIZE);
    CHECK(GetU32(model.m2, ribbon + RIBBON_TEXTURES) == 2);
    CHECK(!Warned(model, "ribbon emitter"));
}
