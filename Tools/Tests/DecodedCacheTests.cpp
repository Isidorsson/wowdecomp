#include <fstream>
#include "Engine/Source/Retail/DecodedCache.h"
#include "Tools/Tests/Check.h"

using Retail::Config;
using Retail::DecodedCachePath;
using Retail::ReadDecodedCache;
using Retail::WriteDecodedCache;

static std::filesystem::path ScratchDir(const char* name) {
    auto dir = std::filesystem::temp_directory_path() / "wowdecomp-tests" / name;
    std::filesystem::remove_all(dir);
    return dir;
}

TEST(CachePathsSeparateProductsAndBuilds) {
    Config config;
    config.path = "E:/World of Warcraft";
    config.cache = "C:/Cache";
    config.build = "88d5e8ef05ec5a8446cdec93d142d7c6";

    CHECK(DecodedCachePath(config, 916541) == std::filesystem::path("C:/Cache/wow-88d5e8ef05ec5a8446cdec93d142d7c6/decoded/916541"));

    config.product = "wowt";
    CHECK(DecodedCachePath(config, 916541) == std::filesystem::path("C:/Cache/wowt-88d5e8ef05ec5a8446cdec93d142d7c6/decoded/916541"));

    config.build.clear();
    CHECK(DecodedCachePath(config, 7) == std::filesystem::path("C:/Cache/wowt-local/decoded/7"));
}

TEST(CacheRoundTripsBytes) {
    auto path = ScratchDir("roundtrip") / "decoded" / "1";
    std::vector<uint8_t> bytes = { 'M', 'D', '2', '0', 0, 1, 2, 255 };
    std::vector<uint8_t> read;
    std::string error;

    CHECK(WriteDecodedCache(path, bytes, error));
    CHECK(ReadDecodedCache(path, read));
    CHECK(read == bytes);
}

TEST(CacheRoundTripsEmptyFiles) {
    auto path = ScratchDir("empty") / "2";
    std::vector<uint8_t> read = { 1 };
    std::string error;

    CHECK(WriteDecodedCache(path, {}, error));
    CHECK(ReadDecodedCache(path, read));
    CHECK(read.empty());
}

TEST(MissingCacheEntriesAreMisses) {
    std::vector<uint8_t> read;

    CHECK(!ReadDecodedCache(ScratchDir("missing") / "3", read));
}

TEST(CorruptCacheEntriesAreMisses) {
    auto dir = ScratchDir("corrupt");
    std::vector<uint8_t> bytes(64, 7);
    std::vector<uint8_t> read;
    std::string error;

    auto flipped = dir / "flipped";
    CHECK(WriteDecodedCache(flipped, bytes, error));
    {
        std::fstream file(flipped, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(-1, std::ios::end);
        file.put(8);
    }
    CHECK(!ReadDecodedCache(flipped, read));
    CHECK(read.empty());

    auto truncated = dir / "truncated";
    CHECK(WriteDecodedCache(truncated, bytes, error));
    std::filesystem::resize_file(truncated, std::filesystem::file_size(truncated) - 1);
    CHECK(!ReadDecodedCache(truncated, read));
}

TEST(CacheWritesLeaveNoTemporaryFiles) {
    auto dir = ScratchDir("temporary");
    std::string error;

    CHECK(WriteDecodedCache(dir / "4", { 1, 2, 3 }, error));
    size_t files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        files += entry.is_regular_file();
    }
    CHECK(files == 1);
}

TEST(OversizedCacheWritesAreRefused) {
    std::string error;
    std::vector<uint8_t> bytes(Retail::DECODED_CACHE_LIMIT + 1);

    CHECK(!WriteDecodedCache(ScratchDir("oversized") / "5", bytes, error));
    CHECK(!error.empty());
}
