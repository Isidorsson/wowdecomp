#include <fstream>
#include <process.h>
#include <string>
#include "Engine/Source/Retail/BakeBlp.h"
#include "Engine/Source/Retail/BodyMatchCache.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Engine/Source/Retail/DecodedCache.h"
#include "Tools/Tests/BodyFixtures.h"
#include "Tools/Tests/Check.h"

// The warped bake through the source: a local install without storage, the retail body and bake in the decoded
// cache, the installed body from a stock reader.

constexpr uint32_t BAKE = 8046191;
constexpr uint32_t HUMAN_MALE_HD = 1011653;
constexpr const char* BAKE_PATH = "Textures\\BakedNpcTextures\\retail-8046191_bake1m.blp";

namespace {

struct BakeSource {
    std::filesystem::path dir;
    Retail::Config config;
    Retail::CascSource source;
    int stockReads = 0;
    std::vector<BodyVertex> installed;   // The installed body's vertices; the retail body's left half by default.
    std::vector<std::string> warnings;
};

}

static void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

static Retail::Image BakePattern() {
    Retail::Image image;
    image.width = 1024;
    image.height = 512;
    for (uint32_t y = 0; y < image.height; y++) {
        for (uint32_t x = 0; x < image.width; x++) {
            image.texels.push_back({ static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(x >> 8), 255 });
        }
    }
    return image;
}

static std::vector<BodyVertex> Square(float uScale) {
    return { { 0, 0, 0, 0, 0 }, { 1, 0, 0, uScale, 0 }, { 0, 1, 0, 0, 1 }, { 1, 1, 0, uScale, 1 } };
}

static bool Open(BakeSource& local, const char* name, bool fresh) {
    local.dir = std::filesystem::temp_directory_path() / ("wowdecomp-tests-" + std::to_string(_getpid())) / name;
    if (fresh) {
        std::filesystem::remove_all(local.dir);
        WriteFile(local.dir / "storage" / ".build.info", "Active!DEC:1|Build Key!HEX:16|Product!STRING:0\n1|88d5e8ef05ec5a8446cdec93d142d7c6|wow\n");
        WriteFile(local.dir / "RetailCasc.ini", "mode=local\npath=storage\ncache=cache\n");
    }
    local.config.cache = local.dir / "cache";
    local.config.build = "88d5e8ef05ec5a8446cdec93d142d7c6";
    std::string error;
    if (!local.source.Configure(local.dir / "RetailCasc.ini", error)) {
        return false;
    }
    local.source.SetWarningSink([&local](const std::string& warning) { local.warnings.push_back(warning); });
    local.source.SetStockReader([&local](const std::string& path, std::vector<uint8_t>& bytes) {
        local.stockReads++;
        if (path == "Character\\Human\\Male\\HumanMale.m2") {
            bytes = BodyModel(local.installed.empty() ? Square(1.0f) : local.installed, { 1 });
            return true;
        }
        if (path == "Character\\Human\\Male\\HumanMale00.skin") {
            bytes = BodySkin({ { { 0, 1, 2 }, { 1, 3, 2 } } });
            return true;
        }
        return false;
    });
    return true;
}

static bool Seed(const BakeSource& local) {
    std::string error;
    return Retail::WriteDecodedCache(Retail::DecodedCachePath(local.config, HUMAN_MALE_HD), BodyModelMd21(Square(0.5f)), error)
        && Retail::WriteDecodedCache(Retail::DecodedCachePath(local.config, BAKE), Retail::EncodeRawBlp(BakePattern()), error);
}

static bool IsLeftHalf(const std::vector<uint8_t>& blp) {
    Retail::Image warped;
    std::string error;
    if (!Retail::DecodeBakeBlp(blp, warped, error) || warped.width != 512 || warped.height != 512) {
        return false;
    }
    Retail::Image bake = BakePattern();
    for (uint32_t y = 0; y < 512; y += 31) {
        for (uint32_t x = 0; x < 512; x += 17) {
            if (!(warped.At(x, y) == bake.At(x, y))) {
                return false;
            }
        }
    }
    return true;
}

TEST(WarpedBakesAreServedUnderTheirName) {
    BakeSource local;
    CHECK(Open(local, "bake-served", true) && Seed(local));
    CHECK(local.source.Handles(BAKE_PATH));

    std::vector<uint8_t> bytes;
    std::string error;
    CHECK(local.source.Read(BAKE_PATH, bytes, error));
    CHECK(IsLeftHalf(bytes));
    CHECK(local.source.Exists(BAKE_PATH, error));
}

TEST(WarpedBakesAreKeptInTheConvertedCacheByBakeAndBody) {
    BakeSource local;
    CHECK(Open(local, "bake-cached", true) && Seed(local));
    std::vector<uint8_t> bytes;
    std::string error;
    CHECK(local.source.Read(BAKE_PATH, bytes, error));

    std::filesystem::path male = Retail::ConvertedBakePath(local.config, BAKE, 1, 0);
    CHECK(std::filesystem::exists(male));
    CHECK(male != Retail::ConvertedBakePath(local.config, BAKE, 1, 1));
    CHECK(male != Retail::ConvertedBakePath(local.config, BAKE + 1, 1, 0));
    CHECK(male != Retail::ConvertedCachePath(local.config, BAKE));

    // After a restart the bake comes from the cache: the bodies are not read again.
    BakeSource again;
    CHECK(Open(again, "bake-cached", false));
    std::filesystem::remove(Retail::DecodedCachePath(again.config, HUMAN_MALE_HD));
    std::vector<uint8_t> cached;
    CHECK(again.source.Read(BAKE_PATH, cached, error));
    CHECK(cached == bytes && again.stockReads == 0);
}

TEST(TheInstalledBodyFitsWhenItsSkinVerticesMatch) {
    BakeSource local;
    CHECK(Open(local, "bake-fits", true) && Seed(local));
    CHECK(local.source.InstalledBodyFits(1, 0));
    int reads = local.stockReads;
    CHECK(local.source.InstalledBodyFits(1, 0) && local.stockReads == reads);   // Once per body.
    CHECK(!local.source.InstalledBodyFits(1, 1));                                // Unreadable body.
}

TEST(WarpedBakesOfAnUnreadableBodyFail) {
    BakeSource local;
    CHECK(Open(local, "bake-nobody", true) && Seed(local));
    std::vector<uint8_t> bytes;
    std::string error;
    CHECK(!local.source.Read("Textures\\BakedNpcTextures\\retail-8046191_bake1f.blp", bytes, error));
    CHECK(!error.empty());
}

static bool Matched(const BakeSource& local) {
    for (const std::string& warning : local.warnings) {
        if (warning.find("of its skin vertices lie on retail") != std::string::npos) {
            return true;
        }
    }
    return false;
}

TEST(BodyMatchesAreKeptForTheNextSession) {
    BakeSource local;
    CHECK(Open(local, "bodymatch-kept", true) && Seed(local));
    CHECK(local.source.InstalledBodyFits(1, 0) && Matched(local));
    CHECK(std::filesystem::exists(Retail::ConvertedBodyMatchPath(local.config, 1, 0)));

    // Without the retail body a match would fail: the next session reads the kept one.
    BakeSource again;
    CHECK(Open(again, "bodymatch-kept", false));
    std::filesystem::remove(Retail::DecodedCachePath(again.config, HUMAN_MALE_HD));
    CHECK(again.source.InstalledBodyFits(1, 0));
    CHECK(!Matched(again));

    // And warps bakes with it.
    std::vector<uint8_t> bytes;
    std::string error;
    CHECK(again.source.Read(BAKE_PATH, bytes, error));
    CHECK(IsLeftHalf(bytes));
}

TEST(AnotherInstalledBodyIsMatchedAgain) {
    BakeSource local;
    CHECK(Open(local, "bodymatch-changed", true) && Seed(local));
    CHECK(local.source.InstalledBodyFits(1, 0));

    // A patch replaced the body with one that is no port: the kept match is not used.
    BakeSource again;
    again.installed = { { 0, 0, 0.5f, 0, 0 }, { 1, 0, 0.5f, 1, 0 }, { 0, 1, 0.5f, 0, 1 }, { 1, 1, 0.5f, 1, 1 } };
    CHECK(Open(again, "bodymatch-changed", false));
    CHECK(!again.source.InstalledBodyFits(1, 0));
    CHECK(Matched(again));
}

TEST(ACorruptKeptBodyMatchIsMatchedAgain) {
    BakeSource local;
    CHECK(Open(local, "bodymatch-corrupt", true) && Seed(local));
    CHECK(local.source.InstalledBodyFits(1, 0));
    std::filesystem::path kept = Retail::ConvertedBodyMatchPath(local.config, 1, 0);
    std::filesystem::resize_file(kept, std::filesystem::file_size(kept) - 3);

    BakeSource again;
    CHECK(Open(again, "bodymatch-corrupt", false));
    CHECK(again.source.InstalledBodyFits(1, 0));
    CHECK(Matched(again));
}
