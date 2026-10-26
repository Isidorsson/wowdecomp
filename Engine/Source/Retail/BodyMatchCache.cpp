#include "Engine/Source/Retail/BodyMatchCache.h"
#include <cstring>
#include "Engine/Source/Retail/Checksum.h"
#include "Engine/Source/Retail/ConvertedCache.h"

namespace Retail {

// The entry is a converted cache entry whose m2 is the match: magic, format, ratio, fits, then the warp's triangles
// (installed and retail UVs, fromBase). The converted cache checks its size and checksum.
static constexpr uint32_t MATCH_MAGIC = 0x31434D42;   // "BMC1"
static constexpr size_t MATCH_HEADER = 4 + 4 + 8 + 1 + 4;
static constexpr size_t TRIANGLE_SIZE = 12 * 4 + 1;

static uint64_t Mix(uint64_t hash, uint64_t value) {
    hash = (hash ^ value) * 0x9e3779b97f4a7c15ull;
    return hash ^ (hash >> 29);
}

uint64_t BodyMatchKey(uint64_t converterKey, const std::vector<uint8_t>& installedModel, const std::vector<uint8_t>& installedSkin, uint32_t retailFileDataId) {
    uint64_t hash = Mix(converterKey, BODY_MATCH_FORMAT);
    hash = Mix(hash, WordChecksum(installedModel.data(), installedModel.size()));
    hash = Mix(hash, WordChecksum(installedSkin.data(), installedSkin.size()));
    return Mix(hash, retailFileDataId);
}

std::filesystem::path ConvertedBodyMatchPath(const Config& config, uint32_t race, uint32_t sex) {
    return ConvertedCacheFolder(config) / ("bodymatch-" + std::to_string(race) + "-" + std::to_string(sex));
}

static void Put(std::vector<uint8_t>& bytes, const void* data, size_t size) {
    const uint8_t* begin = static_cast<const uint8_t*>(data);
    bytes.insert(bytes.end(), begin, begin + size);
}

static void PutUvs(std::vector<uint8_t>& bytes, const std::array<Vec2, 3>& uvs) {
    Put(bytes, uvs.data(), sizeof(uvs));
}

static std::vector<uint8_t> SerializeMatch(const BodyMatchResult& result) {
    std::vector<uint8_t> bytes;
    bytes.reserve(MATCH_HEADER + result.warp.triangles.size() * TRIANGLE_SIZE);
    uint32_t count = static_cast<uint32_t>(result.warp.triangles.size());
    uint8_t fits = result.fits ? 1 : 0;
    Put(bytes, &MATCH_MAGIC, 4);
    Put(bytes, &BODY_MATCH_FORMAT, 4);
    Put(bytes, &result.ratio, 8);
    Put(bytes, &fits, 1);
    Put(bytes, &count, 4);
    for (const UvTriangle& triangle : result.warp.triangles) {
        uint8_t fromBase = triangle.fromBase ? 1 : 0;
        PutUvs(bytes, triangle.installed);
        PutUvs(bytes, triangle.retail);
        Put(bytes, &fromBase, 1);
    }
    return bytes;
}

static bool DeserializeMatch(const std::vector<uint8_t>& bytes, BodyMatchResult& result) {
    uint32_t magic = 0;
    uint32_t format = 0;
    uint32_t count = 0;
    if (bytes.size() < MATCH_HEADER) {
        return false;
    }
    std::memcpy(&magic, bytes.data(), 4);
    std::memcpy(&format, bytes.data() + 4, 4);
    std::memcpy(&count, bytes.data() + 17, 4);
    if (magic != MATCH_MAGIC || format != BODY_MATCH_FORMAT || bytes[16] > 1 || bytes.size() != MATCH_HEADER + uint64_t(count) * TRIANGLE_SIZE) {
        return false;
    }
    std::memcpy(&result.ratio, bytes.data() + 8, 8);
    result.fits = bytes[16] == 1;
    result.warp.triangles.resize(count);
    const uint8_t* at = bytes.data() + MATCH_HEADER;
    for (UvTriangle& triangle : result.warp.triangles) {
        std::memcpy(triangle.installed.data(), at, sizeof(triangle.installed));
        std::memcpy(triangle.retail.data(), at + sizeof(triangle.installed), sizeof(triangle.retail));
        triangle.fromBase = at[TRIANGLE_SIZE - 1] != 0;
        at += TRIANGLE_SIZE;
    }
    return true;
}

bool ReadBodyMatchCache(const std::filesystem::path& path, uint64_t key, BodyMatchResult& result) {
    result = BodyMatchResult();
    ConvertedModel entry;
    BodyMatchResult read;
    if (!ReadConvertedCache(path, key, entry) || !DeserializeMatch(entry.m2, read)) {
        return false;
    }
    result = std::move(read);
    return true;
}

bool WriteBodyMatchCache(const std::filesystem::path& path, uint64_t key, const BodyMatchResult& result, std::string& error) {
    ConvertedModel entry;
    entry.m2 = SerializeMatch(result);
    return WriteConvertedCache(path, key, entry, error);
}

}
