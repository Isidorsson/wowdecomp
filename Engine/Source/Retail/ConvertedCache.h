#ifndef ENGINE_SOURCE_RETAIL_CONVERTEDCACHE_H
#define ENGINE_SOURCE_RETAIL_CONVERTEDCACHE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include "Engine/Source/Retail/RetailConfig.h"
#include "Engine/Source/Retail/RetailModel.h"

namespace Retail {

// Bumped by hand whenever the converter's output changes. Dev builds also mix in a hash of the converter's sources.
constexpr uint32_t CONVERTER_VERSION = 1;

constexpr uint64_t CONVERTED_CACHE_LIMIT = 512ull * 1024 * 1024;

// What an entry was converted by: CONVERTER_VERSION and, in dev builds, the converter sources' hash.
uint64_t ConverterKey();

// Converted models per build, so a warm read never converts.
std::filesystem::path ConvertedCacheFolder(const Config& config);
std::filesystem::path ConvertedCachePath(const Config& config, uint32_t fileDataId);

// The m2, skins, skinBytes, remapped sequences and warnings; not the skeleton. False is a miss: absent, another key,
// another format, truncated, oversized or failing a checksum. An entry of another key or format is deleted, since it
// can never hit again; a hit refreshes the entry's last write time, which TrimConvertedCache reads as its last use.
bool ReadConvertedCache(const std::filesystem::path& path, uint64_t key, ConvertedModel& model);

// Compressed, written to a temporary file and renamed, so a crash never leaves a half-written entry.
bool WriteConvertedCache(const std::filesystem::path& path, uint64_t key, const ConvertedModel& model, std::string& error);

}

#endif
