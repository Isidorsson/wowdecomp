#include <fstream>
#include <memory>
#include <process.h>
#include <string>
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Engine/Source/Retail/DecodedCache.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/RetailPath.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"

// A source on a local install that holds no storage, so every file it reads comes from the decoded cache.

constexpr uint32_t MODEL_ID = 916541;
constexpr uint32_t SKIN_ID = 916542;
constexpr const char* BUILD_KEY = "88d5e8ef05ec5a8446cdec93d142d7c6";
constexpr const char* MODEL_PATH = "retail/916541.m2";

struct LocalSource {
    std::filesystem::path dir;
    Retail::Config config;
    Retail::CascSource source;
};

static void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

// Another source on the same install and cache, as after a restart.
static bool Reopen(LocalSource& local, const std::filesystem::path& dir) {
    local.dir = dir;
    local.config.cache = local.dir / "cache";
    local.config.build = BUILD_KEY;
    std::string error;
    return local.source.Configure(local.dir / "RetailCasc.ini", error);
}

static bool Configure(LocalSource& local, const char* name) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / ("wowdecomp-tests-" + std::to_string(_getpid())) / name;
    std::filesystem::remove_all(dir);
    WriteText(dir / "storage" / ".build.info", std::string("Active!DEC:1|Build Key!HEX:16|Product!STRING:0\n1|") + BUILD_KEY + "|wow\n");
    WriteText(dir / "RetailCasc.ini", "mode=local\npath=storage\ncache=cache\n");
    return Reopen(local, dir);
}

static void DropOriginals(const LocalSource& local) {
    std::filesystem::remove(Retail::DecodedCachePath(local.config, MODEL_ID));
    std::filesystem::remove(Retail::DecodedCachePath(local.config, SKIN_ID));
}

static bool Cache(const LocalSource& local, uint32_t id, const std::vector<uint8_t>& bytes) {
    std::string error;
    return Retail::WriteDecodedCache(Retail::DecodedCachePath(local.config, id), bytes, error);
}

// One view of two vertices, one weighing a bone and one two, drawn by one batch.
static std::vector<uint8_t> ModelFile() {
    std::vector<uint8_t> md20 = NewMd20();
    PutU32(md20, M2At::VIEWS, 1);
    AddArray(md20, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(md20, M2At::VERTICES, 2, Concat({ Vertex(255, 0, 0, 0), Vertex(128, 127, 0, 0) }));
    AddArray(md20, M2At::TEXTURE_COMBOS, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(md20, M2At::COORD_COMBOS, 1, Concat({ { 1, 0 } }));
    return Md21File(md20, { Chunk("SFID", U32s({ SKIN_ID })) });
}

static std::vector<uint8_t> SkinFile() {
    std::vector<uint8_t> skin = NewSkin();
    AddArray(skin, SkinAt::VERTICES, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(skin, SkinAt::INDICES, 3, Concat({ { 0, 0 }, { 1, 0 }, { 1, 0 } }));
    AddArray(skin, SkinAt::SECTIONS, 1, SkinSection(1, 0, 0, 3));
    AddArray(skin, SkinAt::BATCHES, 1, SkinBatch(0, 0x8000, 2));
    return skin;
}

static std::vector<uint8_t> ConvertedSkin() {
    std::vector<uint8_t> skin = SkinFile();
    std::vector<uint8_t> out;
    std::string error;
    Retail::FileReader read = [&](uint32_t, std::vector<uint8_t>& bytes, std::string&) {
        bytes = skin;
        return true;
    };
    return Retail::ConvertSkin({ skin, ModelFile(), 0, read }, out, error) ? out : std::vector<uint8_t>();
}

static bool ReadSkin(LocalSource& local, std::vector<uint8_t>& bytes) {
    std::string error;
    return local.source.Read(Retail::SkinPathFor(Retail::NormalizePath(MODEL_PATH), 0).c_str(), bytes, error);
}

TEST(SkinsReadAfterTheirModelComeFromItsConversion) {
    LocalSource local;
    std::vector<uint8_t> bytes;
    std::string error;

    CHECK(Configure(local, "served-skins") && Cache(local, MODEL_ID, ModelFile()) && Cache(local, SKIN_ID, SkinFile()));
    CHECK(local.source.Read(MODEL_PATH, bytes, error));
    // Without the model's original, a skin read that converts again fails.
    std::filesystem::remove(Retail::DecodedCachePath(local.config, MODEL_ID));
    CHECK(ReadSkin(local, bytes) && !bytes.empty() && bytes == ConvertedSkin());
}

TEST(SkinsReadAgainAreConvertedAgainToTheSameBytes) {
    LocalSource local;
    std::vector<uint8_t> first;
    std::vector<uint8_t> again;
    std::string error;

    CHECK(Configure(local, "reread-skins") && Cache(local, MODEL_ID, ModelFile()) && Cache(local, SKIN_ID, SkinFile()));
    CHECK(local.source.Read(MODEL_PATH, first, error));
    CHECK(ReadSkin(local, first) && ReadSkin(local, again) && !again.empty() && again == first);
}

// Without the originals nothing converts, so these reads come from the converted cache.
TEST(ConvertedModelsAreServedFromTheCacheAfterARestart) {
    LocalSource first;
    LocalSource second;
    std::vector<uint8_t> converted;
    std::vector<uint8_t> cached;
    std::string error;

    CHECK(Configure(first, "converted-restart") && Cache(first, MODEL_ID, ModelFile()) && Cache(first, SKIN_ID, SkinFile()));
    CHECK(first.source.Read(MODEL_PATH, converted, error) && !converted.empty());
    DropOriginals(first);

    CHECK(Reopen(second, first.dir));
    CHECK(second.source.Read(MODEL_PATH, cached, error) && cached == converted);
    CHECK(ReadSkin(second, cached) && !cached.empty() && cached == ConvertedSkin());
}

TEST(SkinsReadAgainComeFromTheConvertedCache) {
    LocalSource local;
    std::vector<uint8_t> first;
    std::vector<uint8_t> again;
    std::string error;

    CHECK(Configure(local, "converted-skins") && Cache(local, MODEL_ID, ModelFile()) && Cache(local, SKIN_ID, SkinFile()));
    CHECK(local.source.Read(MODEL_PATH, first, error) && ReadSkin(local, first));
    DropOriginals(local);
    CHECK(ReadSkin(local, again) && !again.empty() && again == first);
}

TEST(CorruptConvertedEntriesAreConvertedAgainAndRewritten) {
    LocalSource first;
    LocalSource second;
    std::vector<uint8_t> converted;
    std::vector<uint8_t> again;
    Retail::ConvertedModel entry;
    std::string error;

    CHECK(Configure(first, "converted-corrupt") && Cache(first, MODEL_ID, ModelFile()) && Cache(first, SKIN_ID, SkinFile()));
    CHECK(first.source.Read(MODEL_PATH, converted, error));
    std::filesystem::path path = Retail::ConvertedCachePath(first.config, MODEL_ID);
    std::error_code ec;
    std::filesystem::resize_file(path, std::filesystem::file_size(path, ec) / 2, ec);
    CHECK(!ec);

    CHECK(Reopen(second, first.dir));
    CHECK(second.source.Read(MODEL_PATH, again, error) && again == converted);
    CHECK(Retail::ReadConvertedCache(path, Retail::ConverterKey(), entry) && entry.m2 == converted);
}

static void CorruptLastByte(const std::filesystem::path& path) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekg(-1, std::ios::end);
    char byte = static_cast<char>(file.get());
    file.seekp(-1, std::ios::end);
    file.put(static_cast<char>(byte ^ 0x5a));
}

// Damaged past their headers, so a read would refetch them from the storage, which holds nothing.
TEST(CachedOriginalsExistWithoutBeingRead) {
    LocalSource local;
    uint32_t size = 0;
    std::vector<uint8_t> bytes;
    std::string error;
    std::vector<uint8_t> texture = { 'B', 'L', 'P', '2', 1, 2, 3, 4 };

    CHECK(Configure(local, "exists-unread") && Cache(local, 134400, texture));
    CorruptLastByte(Retail::DecodedCachePath(local.config, 134400));
    CHECK(local.source.Exists("retail/134400.blp", error));
    CHECK(local.source.IsCached("retail/134400.blp", size) && size == texture.size());
    CHECK(!local.source.Read("retail/134400.blp", bytes, error));
    CHECK(!local.source.Exists("retail/134401.blp", error) && !error.empty());
    CHECK(!local.source.IsCached("retail/134401.blp", size));
}

TEST(ModelsNotConvertedYetAreConvertedToExist) {
    LocalSource local;
    std::string error;

    CHECK(Configure(local, "exists-convert") && Cache(local, MODEL_ID, ModelFile()) && Cache(local, SKIN_ID, SkinFile()));
    CHECK(local.source.Exists(MODEL_PATH, error));
    CHECK(Retail::HasConvertedCache(Retail::ConvertedCachePath(local.config, MODEL_ID), Retail::ConverterKey()));
}

TEST(ConvertedModelsExistWithoutBeingRead) {
    LocalSource first;
    LocalSource second;
    std::vector<uint8_t> bytes;
    std::string error;

    CHECK(Configure(first, "exists-converted") && Cache(first, MODEL_ID, ModelFile()) && Cache(first, SKIN_ID, SkinFile()));
    CHECK(first.source.Read(MODEL_PATH, bytes, error));
    DropOriginals(first);
    CorruptLastByte(Retail::ConvertedCachePath(first.config, MODEL_ID));

    CHECK(Reopen(second, first.dir));
    CHECK(second.source.Exists(MODEL_PATH, error));
    CHECK(!second.source.Read(MODEL_PATH, bytes, error));
}

// Destroying the source waits for its preloads.
TEST(PreloadRemovesTheCachesOfOtherBuilds) {
    auto local = std::make_unique<LocalSource>();
    std::vector<std::string> warnings;

    CHECK(Configure(*local, "preload-builds") && Cache(*local, 134400, { 1, 2, 3 }));
    std::filesystem::path kept = Retail::DecodedCachePath(local->config, 134400);
    std::filesystem::path other = local->dir / "cache" / "wow-18b43890bb75affcb09e78272ee9d00c";
    WriteText(other / "decoded" / "134400", "old");
    local->source.SetWarningSink([&warnings](const std::string& warning) { warnings.push_back(warning); });
    local->source.Preload();
    local->source.Preload();
    local.reset();

    CHECK(!std::filesystem::exists(other) && std::filesystem::exists(kept));
    size_t mentions = 0;
    for (const std::string& warning : warnings) {
        mentions += warning.find("wow-18b43890bb75affcb09e78272ee9d00c") != std::string::npos;
    }
    CHECK(mentions == 1);
}
