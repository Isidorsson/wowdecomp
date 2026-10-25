#include <algorithm>
#include <chrono>
#include <fstream>
#include <process.h>
#include <string>
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Engine/Source/Retail/CacheBudget.h"
#include "Tools/Tests/Check.h"

using Retail::Config;
using Retail::CacheBudget;
using Retail::ConvertedCachePath;
using Retail::ConvertedModel;
using Retail::HasConvertedCache;
using Retail::ReadConvertedCache;
using Retail::TrimCacheFolder;
using Retail::WriteConvertedCache;

constexpr uint64_t KEY = 0x1234567890abcdefull;

static std::filesystem::path ScratchDir(const char* name) {
    auto dir = std::filesystem::temp_directory_path() / ("wowdecomp-tests-" + std::to_string(_getpid())) / ("converted-" + std::string(name));
    std::filesystem::remove_all(dir);
    return dir;
}

static ConvertedModel Sample() {
    ConvertedModel model;
    model.m2 = { 'M', 'D', '2', '0', 0, 1, 2, 255 };
    model.skins = { 916542, 916543, 916544 };
    model.skinBytes = { { 'S', 'K', 'I', 'N', 9 }, {}, { 'S', 'K', 'I', 'N' } };
    model.remapped = { { 3, 1200, 2, 0 }, { 7, 900, 0, 4 } };
    model.warnings = { "skin 1: no bone windows", "" };
    return model;
}

static bool Same(const ConvertedModel& a, const ConvertedModel& b) {
    if (a.remapped.size() != b.remapped.size()) {
        return false;
    }
    for (size_t i = 0; i < a.remapped.size(); i++) {
        const Retail::RemappedSequence& x = a.remapped[i];
        const Retail::RemappedSequence& y = b.remapped[i];
        if (x.index != y.index || x.originalId != y.originalId || x.originalVariation != y.originalVariation || x.id != y.id) {
            return false;
        }
    }
    return a.m2 == b.m2 && a.skins == b.skins && a.skinBytes == b.skinBytes && a.warnings == b.warnings;
}

static void Corrupt(const std::filesystem::path& path, std::streamoff at) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekg(at, at < 0 ? std::ios::end : std::ios::beg);
    char byte = 0;
    file.get(byte);
    file.seekp(at, at < 0 ? std::ios::end : std::ios::beg);
    file.put(static_cast<char>(byte ^ 0x5a));
}

TEST(ConvertedEntriesLiveBesideTheDecodedOnes) {
    Config config;
    config.cache = "C:/Cache";
    config.build = "88d5e8ef05ec5a8446cdec93d142d7c6";

    CHECK(ConvertedCachePath(config, 916541) == std::filesystem::path("C:/Cache/wow-88d5e8ef05ec5a8446cdec93d142d7c6/converted/916541"));
}

TEST(ConvertedEntriesRoundTrip) {
    auto path = ScratchDir("roundtrip") / "1";
    ConvertedModel read;
    std::string error;

    CHECK(WriteConvertedCache(path, KEY, Sample(), error));
    CHECK(ReadConvertedCache(path, KEY, read));
    CHECK(Same(read, Sample()));
}

TEST(ConvertedEntriesWithoutSkinBytesRoundTrip) {
    auto path = ScratchDir("noskins") / "2";
    ConvertedModel model = Sample();
    model.skinBytes.clear();
    model.remapped.clear();
    model.warnings.clear();
    ConvertedModel read = Sample();
    std::string error;

    CHECK(WriteConvertedCache(path, KEY, model, error));
    CHECK(ReadConvertedCache(path, KEY, read));
    CHECK(Same(read, model));
}

TEST(ConvertedEntriesOfAnotherConverterAreMissesAndRemoved) {
    auto path = ScratchDir("key") / "3";
    ConvertedModel read;
    std::string error;

    CHECK(WriteConvertedCache(path, KEY, Sample(), error));
    CHECK(!ReadConvertedCache(path, KEY + 1, read));
    CHECK(read.m2.empty() && read.skins.empty());
    CHECK(!std::filesystem::exists(path));
}

// Pseudo-random bytes from a fixed seed: incompressible, and the same every run.
static std::vector<uint8_t> Noise(size_t size, uint32_t seed) {
    std::vector<uint8_t> bytes(size);
    for (uint8_t& byte : bytes) {
        seed = seed * 1664525 + 1013904223;
        byte = static_cast<uint8_t>(seed >> 24);
    }
    return bytes;
}

// Several compression blocks of repetitive track data, with an incompressible stretch in the middle.
static ConvertedModel Large() {
    ConvertedModel model = Sample();
    model.m2.clear();
    for (uint32_t i = 0; model.m2.size() < 3500000; i++) {
        uint32_t key[4] = { i / 16, i % 7, 0x3f800000, 0 };
        model.m2.insert(model.m2.end(), reinterpret_cast<uint8_t*>(key), reinterpret_cast<uint8_t*>(key) + sizeof(key));
    }
    std::vector<uint8_t> noise = Noise(1300000, 7);
    model.m2.insert(model.m2.begin() + 1000000, noise.begin(), noise.end());
    model.skinBytes[0] = Noise(70000, 9);
    return model;
}

TEST(LargeConvertedEntriesRoundTripCompressed) {
    auto path = ScratchDir("large") / "6";
    ConvertedModel model = Large();
    ConvertedModel read;
    std::string error;

    CHECK(WriteConvertedCache(path, KEY, model, error));
    CHECK(std::filesystem::file_size(path) < model.m2.size() / 2);
    CHECK(ReadConvertedCache(path, KEY, read));
    CHECK(Same(read, model));
}

TEST(IncompressibleConvertedEntriesRoundTrip) {
    auto path = ScratchDir("noise") / "7";
    ConvertedModel model = Sample();
    model.m2 = Noise(2200000, 3);
    ConvertedModel read;
    std::string error;

    CHECK(WriteConvertedCache(path, KEY, model, error));
    CHECK(ReadConvertedCache(path, KEY, read));
    CHECK(Same(read, model));
}

TEST(CorruptCompressedBlocksAreMisses) {
    auto dir = ScratchDir("corrupt-block");
    ConvertedModel read;
    std::string error;

    for (std::streamoff at : { std::streamoff(200), std::streamoff(400000), std::streamoff(1100000) }) {
        auto path = dir / std::to_string(at);
        CHECK(WriteConvertedCache(path, KEY, Large(), error));
        Corrupt(path, at);
        CHECK(!ReadConvertedCache(path, KEY, read));
        CHECK(read.m2.empty());
    }
}

// The first converted cache format: uncompressed, read now as a stale entry.
TEST(FirstFormatEntriesAreMissesAndRemoved) {
    auto path = ScratchDir("rcc1") / "8";
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream file(path, std::ios::binary);
        uint32_t magic = 0x31434352;
        uint64_t header[5] = { KEY, 8, 0, 4, 0 };
        file.write(reinterpret_cast<const char*>(&magic), 4);
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write("MD20", 4);
        file.write("\0\0\0\0", 4);
        file.write("\0\0\0\0", 4);
    }
    ConvertedModel read;

    CHECK(!ReadConvertedCache(path, KEY, read));
    CHECK(!std::filesystem::exists(path));
}

static void Age(const std::filesystem::path& path, int hours) {
    std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now() - std::chrono::hours(hours));
}

TEST(HitsMarkEntriesRecentlyUsed) {
    auto path = ScratchDir("touch") / "9";
    ConvertedModel read;
    std::string error;

    CHECK(WriteConvertedCache(path, KEY, Sample(), error));
    Age(path, 48);
    auto aged = std::filesystem::last_write_time(path);
    CHECK(ReadConvertedCache(path, KEY, read));
    CHECK(std::filesystem::last_write_time(path) - aged > std::chrono::hours(47));
}

static void WriteFile(const std::filesystem::path& path, size_t size, int hoursOld) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << std::string(size, 'x');
    Age(path, hoursOld);
}

TEST(TrimDeletesLeastRecentlyUsedEntriesToNinetyPercentOfTheCap) {
    auto dir = ScratchDir("trim");
    WriteFile(dir / "11", 100, 1);
    WriteFile(dir / "12", 100, 4);
    WriteFile(dir / "13", 100, 2);
    WriteFile(dir / "14", 100, 3);
    WriteFile(dir / "notes.txt", 5000, 9);
    WriteFile(dir / "15.4242.1.tmp", 5000, 9);

    CHECK(TrimCacheFolder(dir, 1000) == 400);
    CHECK(std::filesystem::exists(dir / "12"));
    CHECK(TrimCacheFolder(dir, 350) == 300);
    CHECK(!std::filesystem::exists(dir / "12"));
    CHECK(TrimCacheFolder(dir, 250) == 200);
    CHECK(!std::filesystem::exists(dir / "14") && std::filesystem::exists(dir / "13") && std::filesystem::exists(dir / "11"));
    CHECK(std::filesystem::exists(dir / "notes.txt") && std::filesystem::exists(dir / "15.4242.1.tmp"));
    CHECK(TrimCacheFolder(dir / "absent", 250) == 0);
}

TEST(BudgetTrimsAtStartAndOncePublishesPassTheCap) {
    auto dir = ScratchDir("budget");
    WriteFile(dir / "21", 100, 3);
    WriteFile(dir / "22", 100, 2);
    WriteFile(dir / "23", 100, 1);

    CacheBudget budget;
    budget.Start(dir, 250);
    budget.Wait();
    CHECK(!std::filesystem::exists(dir / "21") && std::filesystem::exists(dir / "22"));

    WriteFile(dir / "24", 20, 0);
    budget.Added(20);
    budget.Wait();
    CHECK(std::filesystem::exists(dir / "22"));

    WriteFile(dir / "25", 100, 0);
    budget.Added(100);
    budget.Wait();
    CHECK(!std::filesystem::exists(dir / "22") && std::filesystem::exists(dir / "23"));
    CHECK(std::filesystem::exists(dir / "24") && std::filesystem::exists(dir / "25"));
}

TEST(UncappedBudgetsDeleteNothing) {
    auto dir = ScratchDir("uncapped");
    WriteFile(dir / "31", 100, 3);

    CacheBudget budget;
    budget.Start(dir, 0);
    budget.Added(1u << 30);
    budget.Wait();
    CHECK(std::filesystem::exists(dir / "31"));
}

TEST(MissingConvertedEntriesAreMisses) {
    ConvertedModel read;

    CHECK(!ReadConvertedCache(ScratchDir("missing") / "4", KEY, read));
}

TEST(CorruptConvertedEntriesAreMisses) {
    auto dir = ScratchDir("corrupt");
    ConvertedModel read;
    std::string error;

    auto flipped = dir / "flipped";
    CHECK(WriteConvertedCache(flipped, KEY, Sample(), error));
    Corrupt(flipped, -1);
    CHECK(!ReadConvertedCache(flipped, KEY, read));
    CHECK(read.m2.empty());

    auto truncated = dir / "truncated";
    CHECK(WriteConvertedCache(truncated, KEY, Sample(), error));
    std::filesystem::resize_file(truncated, std::filesystem::file_size(truncated) - 1);
    CHECK(!ReadConvertedCache(truncated, KEY, read));

    auto shortHeader = dir / "short";
    CHECK(WriteConvertedCache(shortHeader, KEY, Sample(), error));
    std::filesystem::resize_file(shortHeader, 10);
    CHECK(!ReadConvertedCache(shortHeader, KEY, read));
}

TEST(ConvertedEntriesAreReplacedWithoutTemporaryFiles) {
    auto dir = ScratchDir("publish");
    ConvertedModel model = Sample();
    ConvertedModel read;
    std::string error;

    CHECK(WriteConvertedCache(dir / "5", KEY, model, error));
    model.m2 = { 'M', 'D', '2', '0', 42 };
    CHECK(WriteConvertedCache(dir / "5", KEY, model, error));
    CHECK(ReadConvertedCache(dir / "5", KEY, read) && read.m2 == model.m2);

    size_t files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        files += entry.is_regular_file();
    }
    CHECK(files == 1);
}

TEST(ConvertedEntriesAreFoundByTheirHeader) {
    auto dir = ScratchDir("has");
    std::string error;

    CHECK(WriteConvertedCache(dir / "41", KEY, Sample(), error));
    CHECK(HasConvertedCache(dir / "41", KEY));
    CHECK(!HasConvertedCache(dir / "41", KEY + 1));
    // Damage past the header is found only by a read.
    Corrupt(dir / "41", -1);
    CHECK(HasConvertedCache(dir / "41", KEY));
    std::filesystem::resize_file(dir / "41", std::filesystem::file_size(dir / "41") - 1);
    CHECK(!HasConvertedCache(dir / "41", KEY));
    CHECK(!HasConvertedCache(dir / "absent", KEY));
}

TEST(OtherBuildsOfTheProductAreRemoved) {
    auto cache = ScratchDir("builds");
    std::string current = "wow-88d5e8ef05ec5a8446cdec93d142d7c6";
    std::string old = "wow-18b43890bb75affcb09e78272ee9d00c";
    WriteFile(cache / current / "decoded" / "1", 10, 0);
    WriteFile(cache / old / "decoded" / "1", 10, 0);
    WriteFile(cache / "wow-local" / "converted" / "2", 10, 0);
    WriteFile(cache / "wowt-18b43890bb75affcb09e78272ee9d00c" / "decoded" / "1", 10, 0);
    WriteFile(cache / "wow-notabuild" / "decoded" / "1", 10, 0);
    WriteFile(cache / "wow-18b43890bb75affcb09e78272ee9d00c0" / "decoded" / "1", 10, 0);
    WriteFile(cache / "wow-28b43890bb75affcb09e78272ee9d00c", 10, 0);

    std::vector<std::string> removed = Retail::RemoveOtherBuilds(cache, "wow", current);
    std::sort(removed.begin(), removed.end());
    CHECK(removed == std::vector<std::string>({ old, "wow-local" }));
    CHECK(std::filesystem::exists(cache / current / "decoded" / "1"));
    CHECK(!std::filesystem::exists(cache / old) && !std::filesystem::exists(cache / "wow-local"));
    CHECK(std::filesystem::exists(cache / "wowt-18b43890bb75affcb09e78272ee9d00c") && std::filesystem::exists(cache / "wow-notabuild"));
    CHECK(std::filesystem::exists(cache / "wow-18b43890bb75affcb09e78272ee9d00c0") && std::filesystem::exists(cache / "wow-28b43890bb75affcb09e78272ee9d00c"));
    CHECK(Retail::RemoveOtherBuilds(cache / "absent", "wow", current).empty());
}
