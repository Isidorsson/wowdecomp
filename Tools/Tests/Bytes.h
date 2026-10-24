#ifndef TOOLS_TESTS_BYTES_H
#define TOOLS_TESTS_BYTES_H

#include <cstdint>
#include <cstring>
#include <vector>

// Little-endian reads and writes for the file builders the tests use.

inline void PutU16(std::vector<uint8_t>& file, size_t at, uint16_t value) {
    std::memcpy(file.data() + at, &value, 2);
}

inline void PutU32(std::vector<uint8_t>& file, size_t at, uint32_t value) {
    std::memcpy(file.data() + at, &value, 4);
}

inline uint16_t GetU16(const std::vector<uint8_t>& file, size_t at) {
    uint16_t value = 0;
    if (at + 2 <= file.size()) {
        std::memcpy(&value, file.data() + at, 2);
    }
    return value;
}

inline uint32_t GetU32(const std::vector<uint8_t>& file, size_t at) {
    uint32_t value = 0;
    if (at + 4 <= file.size()) {
        std::memcpy(&value, file.data() + at, 4);
    }
    return value;
}

inline void AppendU32(std::vector<uint8_t>& file, uint32_t value) {
    file.resize(file.size() + 4);
    PutU32(file, file.size() - 4, value);
}

// Writes width bits of value at bit position into record, least significant bit first.
inline void PackBits(std::vector<uint8_t>& record, uint32_t position, uint32_t width, uint32_t value) {
    for (uint32_t i = 0; i < width; i++) {
        uint32_t bit = position + i;
        if ((value >> i) & 1) {
            record[bit / 8] |= static_cast<uint8_t>(1 << (bit % 8));
        }
    }
}

#endif
