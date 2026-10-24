#ifndef ENGINE_SOURCE_RETAIL_RETAILMODELDATA_H
#define ENGINE_SOURCE_RETAIL_RETAILMODELDATA_H

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// Internals the RetailModel converter files share: MD20 and skin layout (M2Data.h; versions 272 to 274 keep
// the 264 layout of every field read here), bounds-checked arrays and appending.

namespace Retail {

namespace M2At {
    constexpr size_t VERSION = 0x04;
    constexpr size_t FLAGS = 0x10;
    constexpr size_t SEQUENCES = 0x1C;
    constexpr size_t SEQUENCE_LOOKUP = 0x24;
    constexpr size_t BONES = 0x2C;
    constexpr size_t VERTICES = 0x3C;
    constexpr size_t VIEWS = 0x44;
    constexpr size_t COLORS = 0x48;
    constexpr size_t TEXTURES = 0x50;
    constexpr size_t TEXTURE_WEIGHTS = 0x58;
    constexpr size_t TEXTURE_TRANSFORMS = 0x60;
    constexpr size_t MATERIALS = 0x70;
    constexpr size_t TEXTURE_COMBOS = 0x80;
    constexpr size_t COORD_COMBOS = 0x88;
    constexpr size_t ATTACHMENTS = 0xF0;
    constexpr size_t EVENTS = 0x100;
    constexpr size_t LIGHTS = 0x108;
    constexpr size_t CAMERAS = 0x110;
    constexpr size_t RIBBONS = 0x120;
    constexpr size_t PARTICLES = 0x128;
    constexpr size_t HEADER = 0x130;
}

namespace SkinAt {
    constexpr size_t VERTICES = 0x04;
    constexpr size_t INDICES = 0x0C;
    constexpr size_t SECTIONS = 0x1C;
    constexpr size_t BATCHES = 0x24;
    constexpr size_t BONE_COUNT_MAX = 0x2C;
    constexpr size_t HEADER = 0x30;
}

namespace SequenceAt {
    constexpr size_t ID = 0x00;
    constexpr size_t VARIATION = 0x02;
    constexpr size_t FLAGS = 0x0C;
    constexpr size_t FREQUENCY = 0x10;
    constexpr size_t VARIATION_NEXT = 0x3C;
    constexpr size_t ALIAS_NEXT = 0x3E;
}

constexpr size_t SEQUENCE_SIZE = 0x40;
constexpr size_t BONE_SIZE = 0x58;
constexpr size_t VERTEX_SIZE = 0x30;
constexpr size_t TEXTURE_SIZE = 0x10;
constexpr size_t MATERIAL_SIZE = 4;
constexpr size_t SECTION_SIZE = 0x30;
constexpr size_t BATCH_SIZE = 0x18;

constexpr uint32_t SEQUENCE_EMBEDDED = 0x20;
constexpr uint32_t SEQUENCE_ALIAS = 0x40;
constexpr uint32_t SEQUENCE_LOCAL_BITS = 0x130;   // Any of them: the data is not in an .anim file.
constexpr uint16_t NO_INDEX = 0xFFFF;

template <typename T>
inline T Get(const std::vector<uint8_t>& data, size_t at) {
    T value;
    std::memcpy(&value, data.data() + at, sizeof(T));
    return value;
}

template <typename T>
inline void Put(std::vector<uint8_t>& data, size_t at, T value) {
    std::memcpy(data.data() + at, &value, sizeof(T));
}

inline bool Fail(std::string& error, const std::string& message) {
    error = "retail model: " + message;
    return false;
}

struct Span {
    uint32_t count = 0;
    uint32_t offset = 0;

    size_t At(size_t index, size_t size) const { return this->offset + index * size; }
};

// The M2Array at header, refused when its records run past the blob.
inline bool ReadSpan(const std::vector<uint8_t>& data, size_t header, size_t size, const char* name, Span& span, std::string& error) {
    span.count = Get<uint32_t>(data, header);
    span.offset = Get<uint32_t>(data, header + 4);

    if (uint64_t(span.offset) + uint64_t(span.count) * size > data.size()) {
        return Fail(error, std::string(name) + " run past the end of the file");
    }

    return true;
}

// Appends bytes 16-byte aligned and returns where they start.
inline uint32_t AppendAligned(std::vector<uint8_t>& blob, const std::vector<uint8_t>& bytes) {
    blob.resize((blob.size() + 15) & ~size_t(15), 0);
    uint32_t offset = static_cast<uint32_t>(blob.size());
    blob.insert(blob.end(), bytes.begin(), bytes.end());
    return offset;
}

inline std::map<std::string, std::vector<uint8_t>> ReadChunks(const std::vector<uint8_t>& file) {
    std::map<std::string, std::vector<uint8_t>> chunks;

    for (size_t at = 0; at + 8 <= file.size();) {
        std::string tag(reinterpret_cast<const char*>(file.data() + at), 4);
        size_t size = Get<uint32_t>(file, at + 4);

        if (size > file.size() - at - 8) {
            break;
        }

        chunks[tag].assign(file.begin() + at + 8, file.begin() + at + 8 + size);
        at += 8 + size;
    }

    return chunks;
}

inline std::vector<uint32_t> U32s(const std::vector<uint8_t>& chunk) {
    std::vector<uint32_t> values(chunk.size() / 4);
    if (!values.empty()) {
        std::memcpy(values.data(), chunk.data(), values.size() * 4);
    }
    return values;
}

}

#endif
