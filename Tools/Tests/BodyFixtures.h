#ifndef TOOLS_TESTS_BODYFIXTURES_H
#define TOOLS_TESTS_BODYFIXTURES_H

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#include "Tools/Tests/M2Builder.h"

// Body models for the matcher and the warp: vertices with a position and a first UV, textures by type, and a skin
// whose batch b draws its triangles with texture combo b (combo b names texture b).

inline void AppendU16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
}

struct BodyVertex {
    float x, y, z, u, v;
};

inline std::vector<uint8_t> BodyVertexRecords(const std::vector<BodyVertex>& vertices) {
    std::vector<uint8_t> records;
    for (const BodyVertex& vertex : vertices) {
        std::vector<uint8_t> record = Vertex(255, 0, 0, 0);
        const float position[3] = { vertex.x, vertex.y, vertex.z };
        const float uv[2] = { vertex.u, vertex.v };
        std::memcpy(record.data(), position, sizeof(position));
        std::memcpy(record.data() + 0x20, uv, sizeof(uv));
        records.insert(records.end(), record.begin(), record.end());
    }
    return records;
}

// An installed (MD20 version 264) body.
inline std::vector<uint8_t> BodyModel(const std::vector<BodyVertex>& vertices, const std::vector<uint32_t>& textureTypes) {
    std::vector<uint8_t> md20 = NewMd20(264);
    AddArray(md20, M2At::VERTICES, static_cast<uint32_t>(vertices.size()), BodyVertexRecords(vertices));
    std::vector<uint8_t> textures;
    std::vector<uint8_t> combos;
    for (size_t i = 0; i < textureTypes.size(); i++) {
        std::vector<uint8_t> texture = Texture(textureTypes[i]);
        textures.insert(textures.end(), texture.begin(), texture.end());
        AppendU16(combos, static_cast<uint16_t>(i));
    }
    AddArray(md20, M2At::TEXTURES, static_cast<uint32_t>(textureTypes.size()), textures);
    AddArray(md20, M2At::TEXTURE_COMBOS, static_cast<uint32_t>(textureTypes.size()), combos);
    return md20;
}

// A retail (MD21) body: the vertices only.
inline std::vector<uint8_t> BodyModelMd21(const std::vector<BodyVertex>& vertices) {
    std::vector<uint8_t> md20 = NewMd20();
    AddArray(md20, M2At::VERTICES, static_cast<uint32_t>(vertices.size()), BodyVertexRecords(vertices));
    return Md21File(md20, { Chunk("SKID", U32s({ 1 })) });
}

using BodyTriangles = std::vector<std::array<uint16_t, 3>>;

// One section and batch per triangle list, over an identity vertex lookup.
inline std::vector<uint8_t> BodySkin(const std::vector<BodyTriangles>& batches) {
    uint16_t vertexCount = 0;
    for (const BodyTriangles& triangles : batches) {
        for (const auto& triangle : triangles) {
            for (uint16_t index : triangle) {
                vertexCount = std::max<uint16_t>(vertexCount, static_cast<uint16_t>(index + 1));
            }
        }
    }

    std::vector<uint8_t> skin = NewSkin();
    std::vector<uint8_t> lookup;
    for (uint16_t i = 0; i < vertexCount; i++) {
        AppendU16(lookup, i);
    }
    std::vector<uint8_t> indices;
    std::vector<uint8_t> sections;
    std::vector<uint8_t> records;
    uint16_t indexStart = 0;
    for (uint16_t b = 0; b < batches.size(); b++) {
        for (const auto& triangle : batches[b]) {
            for (uint16_t index : triangle) {
                AppendU16(indices, index);
            }
        }
        uint16_t count = static_cast<uint16_t>(batches[b].size() * 3);
        std::vector<uint8_t> section = SkinSection(1, 0, 1, count);
        PutU16(section, 0x06, vertexCount);
        PutU16(section, 0x08, indexStart);
        sections.insert(sections.end(), section.begin(), section.end());
        std::vector<uint8_t> batch = SkinBatch(0, 0, 1, 0, b);
        PutU16(batch, 0x04, b);
        records.insert(records.end(), batch.begin(), batch.end());
        indexStart = static_cast<uint16_t>(indexStart + count);
    }
    AddArray(skin, SkinAt::VERTICES, vertexCount, lookup);
    AddArray(skin, SkinAt::INDICES, indexStart, indices);
    AddArray(skin, SkinAt::SECTIONS, static_cast<uint32_t>(batches.size()), sections);
    AddArray(skin, SkinAt::BATCHES, static_cast<uint32_t>(batches.size()), records);
    return skin;
}

// A skin of geosets only: each (id, first vertex, vertex count) a section over an identity lookup, no batches.
struct GeosetRange {
    uint16_t id, start, count;
};

inline std::vector<uint8_t> GeosetSkin(const std::vector<GeosetRange>& ranges) {
    uint16_t vertexCount = 0;
    std::vector<uint8_t> sections;
    for (const GeosetRange& range : ranges) {
        vertexCount = std::max<uint16_t>(vertexCount, static_cast<uint16_t>(range.start + range.count));
        std::vector<uint8_t> section = SkinSection(1, 0, 1, 0);
        PutU16(section, 0x00, range.id);
        PutU16(section, 0x04, range.start);
        PutU16(section, 0x06, range.count);
        sections.insert(sections.end(), section.begin(), section.end());
    }
    std::vector<uint8_t> lookup;
    for (uint16_t i = 0; i < vertexCount; i++) {
        AppendU16(lookup, i);
    }
    std::vector<uint8_t> skin = NewSkin();
    AddArray(skin, SkinAt::VERTICES, vertexCount, lookup);
    AddArray(skin, SkinAt::SECTIONS, static_cast<uint32_t>(ranges.size()), sections);
    return skin;
}

#endif
