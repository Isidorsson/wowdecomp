#include <chrono>
#include <cstring>
#include <fstream>
#include <process.h>
#include <string>
#include "Engine/Source/Retail/DecodedCache.h"
#include "Tools/Tests/Check.h"

using Retail::Config;
using Retail::DecodedCacheHead;
using Retail::DecodedCachePath;
using Retail::PeekDecodedCache;
using Retail::ReadDecodedCache;
using Retail::WriteDecodedCache;

// Per process, so two runs at once (a build check beside the dev tree's) do not clear each other's files.
static std::filesystem::path ScratchDir(const char* name) {
    auto dir = std::filesystem::temp_directory_path() / ("wowdecomp-tests-" + std::to_string(_getpid())) / name;
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

static std::string Magic(const std::filesystem::path& path) {
    char magic[4] = {};
    std::ifstream(path, std::ios::binary).read(magic, 4);
    return std::string(magic, 4);
}

// An entry as the first format wrote it: "RDC1", the byte count, then byte-wise FNV-1a 64.
static void WriteFirstFormat(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    uint64_t hash = 14695981039346656037ull;
    for (uint8_t byte : bytes) {
        hash = (hash ^ byte) * 1099511628211ull;
    }
    uint32_t count = static_cast<uint32_t>(bytes.size());
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write("RDC1", 4);
    output.write(reinterpret_cast<const char*>(&count), 4);
    output.write(reinterpret_cast<const char*>(&hash), 8);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

TEST(FirstFormatEntriesStillReadAndAreRewritten) {
    auto path = ScratchDir("first-format") / "6";
    std::vector<uint8_t> bytes = { 'B', 'L', 'P', '2', 9, 8, 7, 6, 5, 4, 3 };
    std::vector<uint8_t> read;

    WriteFirstFormat(path, bytes);
    CHECK(ReadDecodedCache(path, read) && read == bytes);
    CHECK(Magic(path) == "RDC2");
    CHECK(ReadDecodedCache(path, read) && read == bytes);
}

TEST(EntriesAreWrittenInTheCurrentFormat) {
    auto path = ScratchDir("current-format") / "7";
    std::string error;

    CHECK(WriteDecodedCache(path, { 1, 2, 3 }, error));
    CHECK(Magic(path) == "RDC2");
}

TEST(PeeksReadTheHeaderAlone) {
    auto dir = ScratchDir("peek");
    std::vector<uint8_t> bytes = { 'M', 'D', '2', '1', 0, 1, 2, 3, 4, 5 };
    std::vector<uint8_t> read;
    DecodedCacheHead head;
    std::string error;

    CHECK(WriteDecodedCache(dir / "8", bytes, error));
    CHECK(PeekDecodedCache(dir / "8", head) && head.size == bytes.size() && std::memcmp(head.magic, "MD21", 4) == 0);

    // A damaged original still peeks: only a read checks it.
    {
        std::fstream file(dir / "8", std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(-1, std::ios::end);
        file.put(9);
    }
    CHECK(PeekDecodedCache(dir / "8", head) && head.size == bytes.size());
    CHECK(!ReadDecodedCache(dir / "8", read));

    CHECK(WriteDecodedCache(dir / "9", { 'A', 'B' }, error));
    CHECK(PeekDecodedCache(dir / "9", head) && head.size == 2 && head.magic[0] == 'A' && head.magic[1] == 'B' && head.magic[2] == 0);

    std::filesystem::resize_file(dir / "9", std::filesystem::file_size(dir / "9") - 1);
    CHECK(!PeekDecodedCache(dir / "9", head) && head.size == 0);
    CHECK(!PeekDecodedCache(dir / "absent", head));
}

TEST(DecodedHitsMarkEntriesRecentlyUsed) {
    auto path = ScratchDir("touch") / "10";
    std::vector<uint8_t> read;
    std::string error;

    CHECK(WriteDecodedCache(path, { 1, 2, 3 }, error));
    auto aged = std::filesystem::file_time_type::clock::now() - std::chrono::hours(48);
    std::filesystem::last_write_time(path, aged);
    CHECK(ReadDecodedCache(path, read));
    CHECK(std::filesystem::last_write_time(path) - aged > std::chrono::hours(47));
}

TEST(RecentlyMarkedEntriesAreNotMarkedAgain) {
    auto path = ScratchDir("touch-throttle") / "11";
    std::vector<uint8_t> read;
    std::string error;

    CHECK(WriteDecodedCache(path, { 1, 2, 3 }, error));
    auto recent = std::filesystem::file_time_type::clock::now() - std::chrono::minutes(10);
    std::filesystem::last_write_time(path, recent);
    CHECK(ReadDecodedCache(path, read));
    CHECK(std::filesystem::last_write_time(path) == recent);
}

static void FlipLastByte(const std::filesystem::path& path) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekg(-1, std::ios::end);
    char byte = static_cast<char>(file.get());
    file.seekp(-1, std::ios::end);
    file.put(static_cast<char>(byte ^ 0x5a));
}

// A damage after the first read goes unseen by that session, which is the checksum it saves; a new session sees it.
TEST(VerifiedEntriesAreCheckedOncePerSession) {
    auto path = ScratchDir("verified") / "12";
    std::vector<uint8_t> read;
    std::string error;
    Retail::VerifiedEntries session;
    Retail::VerifiedEntries next;

    CHECK(WriteDecodedCache(path, { 1, 2, 3 }, error));
    FlipLastByte(path);
    CHECK(!ReadDecodedCache(path, read, &session) && !session.Has(path));

    CHECK(WriteDecodedCache(path, { 1, 2, 3 }, error));
    CHECK(ReadDecodedCache(path, read, &session) && session.Has(path));
    FlipLastByte(path);
    CHECK(ReadDecodedCache(path, read, &session) && read.size() == 3);
    CHECK(!ReadDecodedCache(path, read, &next));
}
