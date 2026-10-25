#include <fstream>
#include <process.h>
#include <string>
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Tools/Tests/Check.h"

using Retail::Config;
using Retail::ConvertedCachePath;
using Retail::ConvertedModel;
using Retail::ReadConvertedCache;
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

TEST(ConvertedEntriesOfAnotherConverterAreMisses) {
    auto path = ScratchDir("key") / "3";
    ConvertedModel read;
    std::string error;

    CHECK(WriteConvertedCache(path, KEY, Sample(), error));
    CHECK(!ReadConvertedCache(path, KEY + 1, read));
    CHECK(read.m2.empty() && read.skins.empty());
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
