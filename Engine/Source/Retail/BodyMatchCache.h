#ifndef ENGINE_SOURCE_RETAIL_BODYMATCHCACHE_H
#define ENGINE_SOURCE_RETAIL_BODYMATCHCACHE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "Engine/Source/Retail/BakeWarp.h"
#include "Engine/Source/Retail/RetailConfig.h"

namespace Retail {

// Matching an installed body to its retail HD body takes a quarter second (BodyMatch.h), so the result is kept in the
// converted cache: a later session reads the warp instead of matching again.

// What the warped bakes need of a match: whether the body is the HD port and, if so, the warp.
struct BodyMatchResult {
    double ratio = 0;
    bool fits = false;
    BakeWarp warp;
};

// Bumped by hand whenever the entry's layout changes.
constexpr uint32_t BODY_MATCH_FORMAT = 1;

// What a match is of: the converter (which hashes the matcher's sources in dev builds), the installed body and its
// skin by content, and the retail body's FileDataID. Another installed body is another key, so its entry is a miss.
uint64_t BodyMatchKey(uint64_t converterKey, const std::vector<uint8_t>& installedModel, const std::vector<uint8_t>& installedSkin, uint32_t retailFileDataId);

// The match of the installed body of race and sex, beside the warped bakes.
std::filesystem::path ConvertedBodyMatchPath(const Config& config, uint32_t race, uint32_t sex);

// False is a miss: absent, another key (deleted), truncated, failing its checksum or not a match's layout.
bool ReadBodyMatchCache(const std::filesystem::path& path, uint64_t key, BodyMatchResult& result);
bool WriteBodyMatchCache(const std::filesystem::path& path, uint64_t key, const BodyMatchResult& result, std::string& error);

}

#endif
