#ifndef ENGINE_SOURCE_RETAIL_CHECKSUM_H
#define ENGINE_SOURCE_RETAIL_CHECKSUM_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Retail {

// The caches' checksum. Word-wise, so 36 MB checks in a few milliseconds where byte-wise FNV-1a takes 34; the tail is
// mixed in byte by byte.
inline uint64_t WordChecksum(const uint8_t* bytes, size_t size) {
    uint64_t hash = 14695981039346656037ull ^ size;
    size_t words = size / 8;

    for (size_t i = 0; i < words; i++) {
        uint64_t word;
        std::memcpy(&word, bytes + i * 8, 8);
        hash = (hash ^ word) * 0x9e3779b97f4a7c15ull;
        hash ^= hash >> 29;
    }

    for (size_t i = words * 8; i < size; i++) {
        hash = (hash ^ bytes[i]) * 1099511628211ull;
    }

    return hash;
}

}

#endif
