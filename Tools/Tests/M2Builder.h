#ifndef TOOLS_TESTS_M2BUILDER_H
#define TOOLS_TESTS_M2BUILDER_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "Tools/Tests/Bytes.h"

// Builds retail model files byte by byte: an MD20 blob (264 header layout, which 272 to 274 keep) wrapped in
// an MD21 chunk with optional SFID/TXID/AFID/SKID chunks, and retail .skin files.

namespace M2At {
    constexpr size_t VERSION = 0x04;
    constexpr size_t FLAGS = 0x10;
    constexpr size_t SEQUENCES = 0x1C;
    constexpr size_t SEQUENCE_LOOKUP = 0x24;
    constexpr size_t BONES = 0x2C;
    constexpr size_t VERTICES = 0x3C;
    constexpr size_t VIEWS = 0x44;
    constexpr size_t TEXTURES = 0x50;
    constexpr size_t MATERIALS = 0x70;
    constexpr size_t TEXTURE_COMBOS = 0x80;
    constexpr size_t COORD_COMBOS = 0x88;
    constexpr size_t EVENTS = 0x100;
    constexpr size_t CAMERAS = 0x110;
    constexpr size_t PARTICLES = 0x128;
    constexpr size_t TEXTURE_COMBINER_COMBOS = 0x130;   // Read when global flag 8 is set.
    constexpr size_t HEADER = 0x138;
}

namespace SkinAt {
    constexpr size_t VERTICES = 0x04;
    constexpr size_t INDICES = 0x0C;
    constexpr size_t SECTIONS = 0x1C;
    constexpr size_t BATCHES = 0x24;
    constexpr size_t BONE_COUNT_MAX = 0x2C;
    constexpr size_t HEADER = 0x38;   // Retail adds shadow batches after boneCountMax.
}

// Appends records 16-byte aligned and points the M2Array at header to them.
inline size_t AddArray(std::vector<uint8_t>& blob, size_t header, uint32_t count, const std::vector<uint8_t>& records) {
    blob.resize((blob.size() + 15) & ~size_t(15), 0);
    size_t offset = blob.size();
    blob.insert(blob.end(), records.begin(), records.end());
    PutU32(blob, header, count);
    PutU32(blob, header + 4, static_cast<uint32_t>(offset));
    return offset;
}

inline std::vector<uint8_t> NewMd20(uint32_t version = 272) {
    std::vector<uint8_t> blob(M2At::HEADER, 0);
    std::memcpy(blob.data(), "MD20", 4);
    PutU32(blob, M2At::VERSION, version);
    PutU32(blob, M2At::VIEWS, 1);
    return blob;
}

inline std::vector<uint8_t> Chunk(const char* tag, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> chunk(8);
    std::memcpy(chunk.data(), tag, 4);
    PutU32(chunk, 4, static_cast<uint32_t>(data.size()));
    chunk.insert(chunk.end(), data.begin(), data.end());
    return chunk;
}

inline std::vector<uint8_t> U32s(std::vector<uint32_t> values) {
    std::vector<uint8_t> bytes;
    for (uint32_t value : values) {
        AppendU32(bytes, value);
    }
    return bytes;
}

inline std::vector<uint8_t> Concat(std::vector<std::vector<uint8_t>> parts) {
    std::vector<uint8_t> joined;
    for (const auto& part : parts) {
        joined.insert(joined.end(), part.begin(), part.end());
    }
    return joined;
}

inline std::vector<uint8_t> Sequence(uint16_t id, uint32_t flags, uint16_t variation = 0, uint32_t frequency = 0x7FFF, uint16_t aliasNext = 0xFFFF) {
    std::vector<uint8_t> record(0x40, 0);
    PutU16(record, 0x00, id);
    PutU16(record, 0x02, variation);
    PutU32(record, 0x0C, flags);
    PutU32(record, 0x10, frequency);
    PutU16(record, 0x3C, 0xFFFF);
    PutU16(record, 0x3E, aliasNext);
    return record;
}

namespace SeqAt {
    constexpr size_t ID = 0x00;
    constexpr size_t VARIATION = 0x02;
    constexpr size_t FLAGS = 0x0C;
    constexpr size_t FREQUENCY = 0x10;
    constexpr size_t VARIATION_NEXT = 0x3C;
}

// One AFID row: animation id, variation, the .anim FileDataID.
inline std::vector<uint8_t> AfidRow(uint16_t id, uint16_t variation, uint32_t fileDataId) {
    std::vector<uint8_t> row(8, 0);
    PutU16(row, 0, id);
    PutU16(row, 2, variation);
    PutU32(row, 4, fileDataId);
    return row;
}

// An M2Track at track: per-sequence (count, offset) entries for timestamps, and for keys unless keyless.
struct TrackEntries {
    std::vector<std::pair<uint32_t, uint32_t>> times;
    std::vector<std::pair<uint32_t, uint32_t>> keys;
};

inline void SetTrack(std::vector<uint8_t>& blob, size_t track, const TrackEntries& entries, bool keyless = false, uint16_t loop = 0xFFFF) {
    auto table = [](const std::vector<std::pair<uint32_t, uint32_t>>& rows) {
        std::vector<uint8_t> bytes;
        for (const auto& row : rows) {
            AppendU32(bytes, row.first);
            AppendU32(bytes, row.second);
        }
        return bytes;
    };

    PutU16(blob, track + 2, loop);
    AddArray(blob, track + 4, static_cast<uint32_t>(entries.times.size()), table(entries.times));
    if (!keyless) {
        AddArray(blob, track + 0x0C, static_cast<uint32_t>(entries.keys.size()), table(entries.keys));
    }
}

// The (count, offset) a track holds for sequence s, from its times (keys = false) or keys table.
inline std::pair<uint32_t, uint32_t> TrackEntry(const std::vector<uint8_t>& blob, size_t track, size_t s, bool keys) {
    size_t table = GetU32(blob, track + (keys ? 0x10 : 0x08)) + s * 8;
    return { GetU32(blob, table), GetU32(blob, table + 4) };
}

// A retail camera (116 bytes): type, far, near, positions, base, targets, base, roll, then an FoV track.
inline std::vector<uint8_t> RetailCamera(uint32_t type, float farClip, float nearClip) {
    std::vector<uint8_t> record(116, 0);
    PutU32(record, 0x00, type);
    std::memcpy(record.data() + 0x04, &farClip, 4);
    std::memcpy(record.data() + 0x08, &nearClip, 4);
    PutU32(record, 0x0C + 4, 3);     // positions: 3 per-sequence entries, a marker the conversion must carry over.
    PutU32(record, 0x0C + 8, 0x1234);
    return record;
}

inline std::vector<uint8_t> Bone(uint32_t flags) {
    std::vector<uint8_t> record(0x58, 0);
    PutU32(record, 0x00, 0xFFFFFFFF);
    PutU32(record, 0x04, flags);
    PutU16(record, 0x08, 0xFFFF);
    return record;
}

// A retail particle emitter (492 bytes): the 476-byte 3.3.5 record, then 16 scroll bytes (filled with 0xEE).
// The two bytes at 0x2C hold retail's multi-texture scale (0x11 here).
inline std::vector<uint8_t> RetailParticle(uint32_t id, uint32_t flags, uint16_t texture = 0, uint8_t blend = 2) {
    std::vector<uint8_t> record(492, 0);
    PutU32(record, 0x00, id);
    PutU32(record, 0x04, flags);
    PutU16(record, 0x16, texture);
    record[0x28] = blend;
    record[0x2C] = 0x11;
    record[0x2D] = 0x11;
    std::memset(record.data() + 476, 0xEE, 16);
    return record;
}

inline std::vector<uint8_t> Texture(uint32_t type) {
    std::vector<uint8_t> record(0x10, 0);
    PutU32(record, 0x00, type);
    return record;
}

inline std::vector<uint8_t> Material(uint16_t flags, uint16_t blend) {
    std::vector<uint8_t> record(4, 0);
    PutU16(record, 0, flags);
    PutU16(record, 2, blend);
    return record;
}

inline std::vector<uint8_t> Vertex(uint8_t w0, uint8_t w1, uint8_t w2, uint8_t w3) {
    std::vector<uint8_t> record(0x30, 0);
    record[0x0C] = w0;
    record[0x0D] = w1;
    record[0x0E] = w2;
    record[0x0F] = w3;
    return record;
}

// A one-sequence, one-bone, one-vertex model: the smallest blob the converter accepts.
inline std::vector<uint8_t> MinimalMd20(uint32_t version = 272) {
    std::vector<uint8_t> blob = NewMd20(version);
    AddArray(blob, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(blob, M2At::BONES, 1, Bone(0));
    AddArray(blob, M2At::VERTICES, 1, Vertex(255, 0, 0, 0));
    return blob;
}

inline std::vector<uint8_t> Md21File(const std::vector<uint8_t>& md20, std::vector<std::vector<uint8_t>> chunks = {}) {
    chunks.insert(chunks.begin(), Chunk("MD21", md20));
    return Concat(chunks);
}

inline std::string StringAt(const std::vector<uint8_t>& blob, size_t arrayAt) {
    uint32_t count = GetU32(blob, arrayAt);
    uint32_t offset = GetU32(blob, arrayAt + 4);
    if (!count || offset + count > blob.size()) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(blob.data() + offset), count - 1);
}

inline uint32_t RecordAt(const std::vector<uint8_t>& blob, size_t arrayAt, size_t index, size_t size) {
    return GetU32(blob, arrayAt + 4) + static_cast<uint32_t>(index * size);
}

inline std::vector<uint8_t> SkinSection(uint16_t bones, uint16_t combo, uint16_t influences, uint16_t indexCount, uint16_t level = 0) {
    std::vector<uint8_t> record(0x30, 0);
    PutU16(record, 0x02, level);
    PutU16(record, 0x0A, indexCount);
    PutU16(record, 0x0C, bones);
    PutU16(record, 0x0E, combo);
    PutU16(record, 0x10, influences);
    return record;
}

inline std::vector<uint8_t> SkinBatch(uint8_t flags, uint16_t shader, uint16_t textureCount, uint16_t flags2 = 0, uint16_t textureCombo = 0, uint16_t coordCombo = 0) {
    std::vector<uint8_t> record(0x18, 0);
    record[0] = flags;
    PutU16(record, 0x02, shader);
    PutU16(record, 0x06, flags2);
    PutU16(record, 0x0E, textureCount);
    PutU16(record, 0x10, textureCombo);
    PutU16(record, 0x12, coordCombo);
    return record;
}

inline std::vector<uint16_t> U16sAt(const std::vector<uint8_t>& blob, size_t arrayAt) {
    std::vector<uint16_t> values;
    for (uint32_t i = 0; i < GetU32(blob, arrayAt); i++) {
        values.push_back(GetU16(blob, GetU32(blob, arrayAt + 4) + i * 2));
    }
    return values;
}

inline std::vector<uint8_t> NewSkin() {
    std::vector<uint8_t> skin(SkinAt::HEADER, 0);
    std::memcpy(skin.data(), "SKIN", 4);
    return skin;
}

#endif
