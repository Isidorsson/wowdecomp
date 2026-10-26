#include <fstream>
#include <process.h>
#include <string>
#include "Engine/Source/Retail/BodyMatchCache.h"
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Tools/Tests/Check.h"

using Retail::BodyMatchKey;
using Retail::BodyMatchResult;
using Retail::ReadBodyMatchCache;
using Retail::WriteBodyMatchCache;

constexpr uint32_t RETAIL_BODY = 1011653;

static std::filesystem::path ScratchDir(const char* name) {
    auto dir = std::filesystem::temp_directory_path() / ("wowdecomp-tests-" + std::to_string(_getpid())) / ("bodymatch-" + std::string(name));
    std::filesystem::remove_all(dir);
    return dir;
}

static BodyMatchResult SampleMatch() {
    BodyMatchResult result;
    result.ratio = 0.987654321;
    result.fits = true;
    Retail::UvTriangle first;
    first.installed = { { { 0.1f, 0.2f }, { 0.3f, 0.4f }, { 0.5f, 0.6f } } };
    first.retail = { { { 0.05f, 0.2f }, { 0.15f, 0.4f }, { 0.25f, 0.6f } } };
    Retail::UvTriangle second = first;
    second.installed[2] = { 0.9f, 0.875f };
    second.fromBase = true;
    result.warp.triangles = { first, second };
    return result;
}

static bool SameMatch(const BodyMatchResult& a, const BodyMatchResult& b) {
    if (a.ratio != b.ratio || a.fits != b.fits || a.warp.triangles.size() != b.warp.triangles.size()) {
        return false;
    }
    for (size_t i = 0; i < a.warp.triangles.size(); i++) {
        const Retail::UvTriangle& x = a.warp.triangles[i];
        const Retail::UvTriangle& y = b.warp.triangles[i];
        if (x.installed != y.installed || x.retail != y.retail || x.fromBase != y.fromBase) {
            return false;
        }
    }
    return true;
}

static void FlipByte(const std::filesystem::path& path, std::streamoff at) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekg(at, at < 0 ? std::ios::end : std::ios::beg);
    char byte = 0;
    file.get(byte);
    file.seekp(at, at < 0 ? std::ios::end : std::ios::beg);
    file.put(static_cast<char>(byte ^ 0x5a));
}

TEST(BodyMatchesRoundTripThroughTheConvertedCache) {
    auto path = ScratchDir("roundtrip") / "bodymatch-1-0";
    std::string error;
    CHECK(WriteBodyMatchCache(path, 42, SampleMatch(), error));

    BodyMatchResult read;
    CHECK(ReadBodyMatchCache(path, 42, read));
    CHECK(SameMatch(read, SampleMatch()));

    BodyMatchResult refused;
    refused.ratio = 0.01;
    CHECK(WriteBodyMatchCache(path, 42, refused, error));
    CHECK(ReadBodyMatchCache(path, 42, read));
    CHECK(SameMatch(read, refused));
}

TEST(BodyMatchesLiveBesideTheWarpedBakesByRaceAndSex) {
    Retail::Config config;
    config.cache = "cache";
    config.build = "88d5e8ef05ec5a8446cdec93d142d7c6";
    auto path = Retail::ConvertedBodyMatchPath(config, 1, 0);
    CHECK(path.parent_path() == Retail::ConvertedCacheFolder(config));
    CHECK(path != Retail::ConvertedBodyMatchPath(config, 1, 1));
    CHECK(path != Retail::ConvertedBodyMatchPath(config, 2, 0));
    CHECK(path != Retail::ConvertedCachePath(config, 1));
}

TEST(BodyMatchKeysChangeWithTheInstalledBodyTheRetailBodyAndTheConverter) {
    std::vector<uint8_t> model = { 'M', 'D', '2', '0', 1, 2, 3 };
    std::vector<uint8_t> skin = { 'S', 'K', 'I', 'N', 4 };
    uint64_t key = BodyMatchKey(7, model, skin, RETAIL_BODY);
    CHECK(key == BodyMatchKey(7, model, skin, RETAIL_BODY));

    std::vector<uint8_t> edited = model;
    edited.back() ^= 1;
    CHECK(key != BodyMatchKey(7, edited, skin, RETAIL_BODY));
    std::vector<uint8_t> longer = model;
    longer.push_back(0);
    CHECK(key != BodyMatchKey(7, longer, skin, RETAIL_BODY));
    std::vector<uint8_t> otherSkin = skin;
    otherSkin[4] = 5;
    CHECK(key != BodyMatchKey(7, model, otherSkin, RETAIL_BODY));
    CHECK(key != BodyMatchKey(7, model, skin, RETAIL_BODY + 1));
    CHECK(key != BodyMatchKey(8, model, skin, RETAIL_BODY));
    // The model's bytes and the skin's are told apart: moving a byte across them is another body.
    CHECK(BodyMatchKey(7, { 1, 2 }, { 3 }, RETAIL_BODY) != BodyMatchKey(7, { 1 }, { 2, 3 }, RETAIL_BODY));
}

TEST(BodyMatchesOfAnotherInstalledBodyAreMissesAndRemoved) {
    auto path = ScratchDir("otherbody") / "bodymatch-1-0";
    std::string error;
    std::vector<uint8_t> skin = { 1 };
    uint64_t before = BodyMatchKey(7, { 1, 2, 3 }, skin, RETAIL_BODY);
    uint64_t after = BodyMatchKey(7, { 1, 2, 4 }, skin, RETAIL_BODY);
    CHECK(WriteBodyMatchCache(path, before, SampleMatch(), error));

    BodyMatchResult read;
    CHECK(!ReadBodyMatchCache(path, after, read));
    CHECK(!read.fits && read.warp.triangles.empty());
    CHECK(!std::filesystem::exists(path));
}

TEST(CorruptBodyMatchesAreMisses) {
    auto dir = ScratchDir("corrupt");
    std::string error;
    BodyMatchResult read;

    auto flipped = dir / "flipped";
    CHECK(WriteBodyMatchCache(flipped, 42, SampleMatch(), error));
    FlipByte(flipped, -1);
    CHECK(!ReadBodyMatchCache(flipped, 42, read));
    CHECK(!read.fits && read.warp.triangles.empty());

    auto truncated = dir / "truncated";
    CHECK(WriteBodyMatchCache(truncated, 42, SampleMatch(), error));
    std::filesystem::resize_file(truncated, std::filesystem::file_size(truncated) - 1);
    CHECK(!ReadBodyMatchCache(truncated, 42, read));

    // A converted model under the same name is not a match.
    auto model = dir / "model";
    Retail::ConvertedModel converted;
    converted.m2 = { 'M', 'D', '2', '0' };
    CHECK(Retail::WriteConvertedCache(model, 42, converted, error));
    CHECK(!ReadBodyMatchCache(model, 42, read));

    CHECK(!ReadBodyMatchCache(dir / "missing", 42, read));
}
