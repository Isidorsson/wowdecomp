#ifndef TOOLS_TESTS_SHADINGFIXTURES_H
#define TOOLS_TESTS_SHADINGFIXTURES_H

#include <cstdint>
#include <string>
#include <vector>
#include "Tools/Tests/M2Builder.h"

// A model and skins for the shading tests, and what the 3.3.5 client reads back for a batch.

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

// A retail model with two materials (flags 0x4 opaque, flags 0x1 alpha key) whose header ends at 0x130, as every retail file's does: the first array (the name here)
// starts where 3.3.5 keeps the combiner table.
inline std::vector<uint8_t> ShadedModel(std::vector<std::vector<uint8_t>> chunks = {}) {
    std::vector<uint8_t> md20 = NewMd20();
    md20.resize(M2At::TEXTURE_COMBINER_COMBOS);
    std::string name = "kobold";
    AddArray(md20, 0x08, static_cast<uint32_t>(name.size() + 1), std::vector<uint8_t>(name.c_str(), name.c_str() + name.size() + 1));
    AddArray(md20, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(md20, M2At::VERTICES, 2, Concat({ Vertex(255, 0, 0, 0), Vertex(255, 0, 0, 0) }));
    AddArray(md20, M2At::MATERIALS, 2, Concat({ Material(0x4, 0), Material(0x1, 1) }));
    AddArray(md20, M2At::TEXTURE_COMBOS, 4, Concat({ { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 } }));
    AddArray(md20, M2At::WEIGHT_COMBOS, 3, Concat({ { 0, 0 }, { 1, 0 }, { 2, 0 } }));
    AddArray(md20, M2At::TRANSFORM_COMBOS, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(md20, M2At::COORD_COMBOS, 1, Concat({ { 1, 0 } }));
    return Md21File(md20, chunks);
}

inline std::vector<uint8_t> SkinOf(std::vector<std::vector<uint8_t>> batches) {
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

inline ClientShading Shading(const std::vector<uint8_t>& m2, const std::vector<uint8_t>& skin, size_t index) {
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

#endif
