#ifndef ENGINE_SOURCE_RETAIL_DECODEDCACHE_H
#define ENGINE_SOURCE_RETAIL_DECODEDCACHE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailConfig.h"

namespace Retail {

constexpr uint32_t DECODED_CACHE_LIMIT = 128 * 1024 * 1024;

// Decoded original bytes per build, so a warm read never opens CASC storage.
std::filesystem::path DecodedCachePath(const Config& config, uint32_t fileDataId);

// False is a cache miss: absent, truncated, oversized or failing its checksum.
bool ReadDecodedCache(const std::filesystem::path& path, std::vector<uint8_t>& bytes);

// Written to a temporary file and renamed, so a crash never leaves a half-written entry.
bool WriteDecodedCache(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string& error);

}

#endif
