#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Engine/Source/Retail/DecodedCache.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/RetailPath.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/LocalSourceFixture.h"
#include "Tools/Tests/M2Builder.h"

constexpr uint32_t MODEL_ID = 916541;
constexpr uint32_t SKIN_ID = 916542;
constexpr const char* MODEL_PATH = "retail/916541.m2";

static void DropOriginals(const LocalSource& local) {
    std::filesystem::remove(Retail::DecodedCachePath(local.config, MODEL_ID));
    std::filesystem::remove(Retail::DecodedCachePath(local.config, SKIN_ID));
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

// Over the memory cache's entry limit, so only the handoff from Exists can serve it once its caches are gone.
static std::vector<uint8_t> LargeModelFile() {
    std::vector<uint8_t> md20 = NewMd20();
    PutU32(md20, M2At::VIEWS, 1);
    AddArray(md20, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(md20, M2At::VERTICES, 2, Concat({ Vertex(255, 0, 0, 0), Vertex(128, 127, 0, 0) }));
    AddArray(md20, M2At::TEXTURE_COMBOS, 2, Concat({ { 0, 0 }, { 1, 0 } }));
    AddArray(md20, M2At::COORD_COMBOS, 1, Concat({ { 1, 0 } }));
    return Md21File(md20, { Chunk("SFID", U32s({ SKIN_ID })), Chunk("XPAD", std::vector<uint8_t>(256 * 1024)) });
}

TEST(ExistsHandsTheFileItReadToTheOpenThatFollows) {
    LocalSource local;
    std::vector<uint8_t> bytes;
    std::string error;

    CHECK(Configure(local, "exists-handoff") && Cache(local, MODEL_ID, LargeModelFile()) && Cache(local, SKIN_ID, SkinFile()));
    CHECK(local.source.Exists(MODEL_PATH, error));
    DropOriginals(local);
    std::filesystem::remove(Retail::ConvertedCachePath(local.config, MODEL_ID));
    CHECK(local.source.Read(MODEL_PATH, bytes, error) && bytes.size() > 4 && std::memcmp(bytes.data(), "MD20", 4) == 0);
    CHECK(!local.source.Read(MODEL_PATH, bytes, error));
}

TEST(SmallFilesReadAgainComeFromMemory) {
    LocalSource local;
    std::vector<uint8_t> bytes;
    std::string error;
    std::vector<uint8_t> texture = { 'B', 'L', 'P', '2', 5, 6, 7, 8 };

    CHECK(Configure(local, "memory-hit") && Cache(local, 134402, texture));
    CHECK(local.source.Read("retail/134402.blp", bytes, error) && bytes == texture);
    CorruptLastByte(Retail::DecodedCachePath(local.config, 134402));
    CHECK(local.source.Read("retail/134402.blp", bytes, error) && bytes == texture);
}

// An opener that fails after a while, counting its calls.
struct SlowOpener {
    std::atomic<int> opens { 0 };
    std::atomic<bool> entered { false };

    Retail::CascSource::StorageOpener Make(std::chrono::milliseconds delay) {
        return [this, delay](std::string& error) -> void* {
            this->opens++;
            this->entered = true;
            std::this_thread::sleep_for(delay);
            error = "offline";
            return nullptr;
        };
    }

    void WaitEntered() const {
        while (!this->entered) {
            std::this_thread::yield();
        }
    }
};

TEST(MissesDuringAnOpenWaitForItInsteadOfOpeningAgain) {
    LocalSource local;
    SlowOpener opener;
    std::vector<uint8_t> bytes;
    std::string error;

    CHECK(Configure(local, "open-shared"));
    local.source.SetStorageOpener(opener.Make(std::chrono::milliseconds(300)));
    std::thread first([&local] {
        std::vector<uint8_t> read;
        std::string readError;
        local.source.Read("retail/5.blp", read, readError);
    });
    opener.WaitEntered();
    CHECK(!local.source.Read("retail/6.blp", bytes, error) && error.find("offline") != std::string::npos);
    first.join();
    CHECK(opener.opens == 1);

    // A failed open is tried again by the next miss.
    CHECK(!local.source.Read("retail/7.blp", bytes, error));
    CHECK(opener.opens == 2);
}

TEST(CachedReadsDoNotWaitForAnOpen) {
    LocalSource local;
    SlowOpener opener;
    std::vector<uint8_t> bytes;
    std::string error;

    CHECK(Configure(local, "open-cached") && Cache(local, 134403, { 'B', 'L', 'P', '2' }));
    local.source.SetStorageOpener(opener.Make(std::chrono::milliseconds(600)));
    std::thread miss([&local] {
        std::vector<uint8_t> read;
        std::string readError;
        local.source.Read("retail/5.blp", read, readError);
    });
    opener.WaitEntered();
    auto start = std::chrono::steady_clock::now();
    CHECK(local.source.Read("retail/134403.blp", bytes, error) && bytes.size() == 4);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(300));
    miss.join();
}

static bool WaitPrefetched(Retail::CascSource& source, const std::vector<uint32_t>& ids) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (uint32_t id : ids) {
        while (source.IconPending(id)) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return true;
}

TEST(PrefetchedIconsAreServedFromMemory) {
    LocalSource local;
    SlowOpener opener;
    std::vector<uint8_t> bytes;
    std::string error;
    std::vector<uint8_t> icon = { 'B', 'L', 'P', '2', 9, 9 };

    CHECK(Configure(local, "prefetch") && Cache(local, 134404, icon));
    local.source.SetStorageOpener(opener.Make(std::chrono::milliseconds(0)));
    local.source.PrefetchIcons({ 134405, 134404 });
    CHECK(WaitPrefetched(local.source, { 134405, 134404 }));
    std::filesystem::remove(Retail::DecodedCachePath(local.config, 134404));
    CHECK(local.source.Read("retail/134404.blp", bytes, error) && bytes == icon);
    CHECK(opener.opens == 1);
}

TEST(FilesOnlyTheSeedHoldsAreRead) {
    LocalSource local;
    SlowOpener opener;
    std::vector<uint8_t> bytes;
    std::string error;
    std::vector<uint8_t> texture = { 'B', 'L', 'P', '2', 3, 1, 4, 1 };

    CHECK(Configure(local, "seed-read") && Seed(local, 134410, texture));
    local.source.SetStorageOpener(opener.Make(std::chrono::milliseconds(0)));
    CHECK(local.source.Read("retail/134410.blp", bytes, error) && bytes == texture);
    CHECK(opener.opens == 0 && !std::filesystem::exists(Retail::DecodedCachePath(local.config, 134410)));
}

TEST(ACorruptSeedEntryFallsThroughToTheDecodedCache) {
    LocalSource local;
    std::vector<uint8_t> bytes;
    std::string error;
    std::vector<uint8_t> seeded = { 'B', 'L', 'P', '2', 1, 1, 1, 1 };
    std::vector<uint8_t> cached = { 'B', 'L', 'P', '2', 2, 2, 2, 2 };

    CHECK(Configure(local, "seed-corrupt") && Seed(local, 134411, seeded) && Cache(local, 134411, cached));
    CorruptLastByte(Retail::SeedEntryPath(local.config, 134411));
    CHECK(local.source.Read("retail/134411.blp", bytes, error) && bytes == cached);
}

// Every file below the folder with its last write time (a folder's is updated lazily, so it says nothing).
static std::map<std::filesystem::path, std::filesystem::file_time_type> Listing(const std::filesystem::path& folder) {
    std::map<std::filesystem::path, std::filesystem::file_time_type> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(folder)) {
        if (entry.is_regular_file()) {
            files[entry.path()] = entry.last_write_time();
        }
    }
    return files;
}

// Old enough that a cache would mark a hit as used again.
TEST(TheSourceNeverWritesIntoTheSeed) {
    LocalSource local;
    std::vector<uint8_t> bytes;
    std::string error;
    std::vector<uint8_t> texture = { 'B', 'L', 'P', '2', 6, 2, 8, 3 };

    CHECK(Configure(local, "seed-readonly") && Seed(local, 134412, texture) && Cache(local, 134413, texture));
    std::filesystem::last_write_time(Retail::SeedEntryPath(local.config, 134412), std::filesystem::file_time_type::clock::now() - std::chrono::hours(48));
    auto before = Listing(local.config.seed);

    CHECK(local.source.Read("retail/134412.blp", bytes, error) && bytes == texture);
    CHECK(local.source.Read("retail/134413.blp", bytes, error) && bytes == texture);
    CHECK(!local.source.Read("retail/134414.blp", bytes, error));
    CHECK(Listing(local.config.seed) == before);
}

// Destroying the source waits for the trim its configuration started.
TEST(TrimmingTheDecodedCacheLeavesTheSeedAlone) {
    LocalSource first;
    std::vector<uint8_t> large(600 * 1024, 7);
    const uint32_t ids[] = { 134420, 134421, 134422 };

    CHECK(Configure(first, "seed-untrimmed"));
    WriteText(first.dir / "RetailCasc.ini", "mode=local\npath=storage\ncache=cache\ndecoded_cache_mb=1\n");
    for (uint32_t id : ids) {
        CHECK(Seed(first, id, large) && Cache(first, id, large));
    }

    auto second = std::make_unique<LocalSource>();
    CHECK(Reopen(*second, first.dir));
    second.reset();

    size_t seeded = 0;
    size_t cached = 0;
    for (uint32_t id : ids) {
        seeded += std::filesystem::exists(Retail::SeedEntryPath(first.config, id));
        cached += std::filesystem::exists(Retail::DecodedCachePath(first.config, id));
    }
    CHECK(cached < 3);
    CHECK(seeded == 3);
}
