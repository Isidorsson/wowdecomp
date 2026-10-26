#ifndef ENGINE_SOURCE_RETAIL_DECODEDCACHE_H
#define ENGINE_SOURCE_RETAIL_DECODEDCACHE_H

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>
#include "Engine/Source/Retail/RetailConfig.h"

namespace Retail {

constexpr uint32_t DECODED_CACHE_LIMIT = 128 * 1024 * 1024;
constexpr uint32_t DECODED_CACHE_HEADER_SIZE = 16;   // An entry takes its original's size plus this.

// Decoded original bytes per build, so a warm read never opens CASC storage.
std::filesystem::path DecodedCacheFolder(const Config& config);
std::filesystem::path DecodedCachePath(const Config& config, uint32_t fileDataId);

// The entries whose checksum held this session. Thread-safe.
class VerifiedEntries {
    public:
    bool Has(const std::filesystem::path& path) const;
    void Add(const std::filesystem::path& path);

    private:
    mutable std::mutex m_mutex;
    std::unordered_set<std::wstring> m_paths;
};

// A hit that refreshes an entry's last write time does so only once DECODED_CACHE_TOUCH_AGE has passed since the last.
constexpr std::chrono::hours DECODED_CACHE_TOUCH_AGE { 1 };

// False is a cache miss: absent, truncated, oversized or failing its checksum; an entry in verified skips the checksum,
// and one that passes it is added. A hit refreshes the entry's last write time once it is DECODED_CACHE_TOUCH_AGE old,
// which TrimCacheFolder reads as its last use, and rewrites an entry of the first format (RDC1) in the current one.
bool ReadDecodedCache(const std::filesystem::path& path, std::vector<uint8_t>& bytes, VerifiedEntries* verified = nullptr);

// What an entry's header says, read without the original or its checksum.
struct DecodedCacheHead {
    uint32_t size = 0;
    uint8_t magic[4] = {};   // The original's first bytes; zero past its end.
};

// False when the entry is absent or its header disagrees with the file's size.
bool PeekDecodedCache(const std::filesystem::path& path, DecodedCacheHead& head);

// Written to a temporary file and renamed, so a crash never leaves a half-written entry.
bool WriteDecodedCache(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string& error);

}

#endif
