#include "Engine/Source/Retail/RetailConfig.h"
#include "Tools/Tests/Check.h"

using Retail::Config;
using Retail::ParseConfig;
using Retail::StorageMode;

static const std::filesystem::path s_base = "C:/Game/Data/Retail";
static const char* s_build = "88d5e8ef05ec5a8446cdec93d142d7c6";

TEST(OnlineConfigParsesEveryKey) {
    Config config;
    std::string error;
    std::string text = std::string("# comment\nmode=online\npath=Cache/Retail\nproduct=wowt\nregion=eu\nbuild=") + s_build
        + "\nlocale=20\naliases=replacements.csv\n";

    CHECK(ParseConfig(text, s_base, config, error));
    CHECK(config.mode == StorageMode::Online);
    CHECK(config.path == s_base / "Cache/Retail");
    CHECK(config.product == "wowt");
    CHECK(config.region == "eu");
    CHECK(config.build == s_build);
    CHECK(config.locale == 0x20);
    CHECK(config.aliases == s_base / "replacements.csv");
}

TEST(ConfigTrimsSpacesAndCarriageReturns) {
    Config config;
    std::string error;

    CHECK(ParseConfig(std::string("  mode = online \r\n path = Cache \r\n build = ") + s_build + " \r\n", s_base, config, error));
    CHECK(config.path == s_base / "Cache");
    CHECK(config.build == s_build);
}

TEST(AbsolutePathsAreKept) {
    Config config;
    std::string error;

    CHECK(ParseConfig("mode=local\npath=D:/Games/World of Warcraft\n", s_base, config, error));
    CHECK(config.mode == StorageMode::Local);
    CHECK(config.path == std::filesystem::path("D:/Games/World of Warcraft"));
    CHECK(config.build.empty());
}

TEST(OnlineConfigRequiresAPinnedBuild) {
    Config config;
    std::string error;

    CHECK(!ParseConfig("mode=online\npath=Cache\n", s_base, config, error));
    CHECK(!error.empty());
    CHECK(!ParseConfig("mode=online\npath=Cache\nbuild=12.1.5.70448\n", s_base, config, error));
    CHECK(!ParseConfig("mode=online\npath=Cache\nbuild=88d5e8ef05ec5a8446cdec93d142d7cZ\n", s_base, config, error));
}

TEST(CacheDefaultsBesideTheIni) {
    Config config;
    std::string error;

    CHECK(ParseConfig("mode=local\npath=E:/Retail\n", s_base, config, error));
    CHECK(config.cache == s_base / "Cache");
    CHECK(ParseConfig("mode=local\npath=E:/Retail\ncache=Decoded\n", s_base, config, error));
    CHECK(config.cache == s_base / "Decoded");
    CHECK(ParseConfig("mode=local\npath=E:/Retail\ncache=F:/RetailCache\n", s_base, config, error));
    CHECK(config.cache == std::filesystem::path("F:/RetailCache"));
}

TEST(ConvertedCacheCapDefaultsAndParses) {
    Config config;
    std::string error;

    CHECK(ParseConfig("mode=local\npath=E:/Retail\n", s_base, config, error));
    CHECK(config.convertedCacheMb == 2048);
    CHECK(ParseConfig("mode=local\npath=E:/Retail\nconverted_cache_mb=512\n", s_base, config, error));
    CHECK(config.convertedCacheMb == 512);
    CHECK(ParseConfig("mode=local\npath=E:/Retail\nconverted_cache_mb=0\n", s_base, config, error));
    CHECK(config.convertedCacheMb == 0);
    CHECK(!ParseConfig("mode=local\npath=E:/Retail\nconverted_cache_mb=-1\n", s_base, config, error));
    CHECK(!ParseConfig("mode=local\npath=E:/Retail\nconverted_cache_mb=2GB\n", s_base, config, error));
    CHECK(!ParseConfig("mode=local\npath=E:/Retail\nconverted_cache_mb=\n", s_base, config, error));
    CHECK(!ParseConfig("mode=local\npath=E:/Retail\nconverted_cache_mb=99999999999\n", s_base, config, error));
}

TEST(DecodedCacheCapDefaultsAndParses) {
    Config config;
    std::string error;

    CHECK(ParseConfig("mode=local\npath=E:/Retail\n", s_base, config, error));
    CHECK(config.decodedCacheMb == 4096);
    CHECK(ParseConfig("mode=local\npath=E:/Retail\ndecoded_cache_mb=0\n", s_base, config, error));
    CHECK(config.decodedCacheMb == 0);
    CHECK(!ParseConfig("mode=local\npath=E:/Retail\ndecoded_cache_mb=4GB\n", s_base, config, error));
}

TEST(CreatureDisplaysDefaultsToStockAndParses) {
    Config config;
    std::string error;

    CHECK(ParseConfig("mode=local\npath=E:/Retail\n", s_base, config, error));
    CHECK(config.creatureDisplays == Retail::CreatureDisplays::Stock);
    CHECK(ParseConfig("mode=local\npath=E:/Retail\ncreature_displays=models\n", s_base, config, error));
    CHECK(config.creatureDisplays == Retail::CreatureDisplays::Models);
    CHECK(!ParseConfig("mode=local\npath=E:/Retail\ncreature_displays=retail\n", s_base, config, error));
    CHECK(error.find("creature_displays") != std::string::npos);
}

TEST(BuildCacheFoldersAreNamedByProductAndBuild) {
    Config config;
    config.cache = "C:/Cache";
    config.build = s_build;

    CHECK(Retail::BuildCacheFolder(config) == std::filesystem::path("C:/Cache/wow-88d5e8ef05ec5a8446cdec93d142d7c6"));
    config.build.clear();
    CHECK(Retail::BuildCacheFolder(config) == std::filesystem::path("C:/Cache/wow-local"));
}

static const char* s_buildInfo =
    "Branch!STRING:0|Active!DEC:1|Build Key!HEX:16|CDN Key!HEX:16|Version!STRING:0|Product!STRING:0\r\n"
    "eu|0|11111111111111111111111111111111|aa|12.1.0.1|wow\r\n"
    "eu|1|18b43890bb75affcb09e78272ee9d00c|bb|12.1.5.70529|wow\r\n"
    "eu|1|22222222222222222222222222222222|cc|12.2.0.9|wow_beta\r\n";

TEST(BuildInfoGivesTheActiveBuildOfTheProduct) {
    std::string key;
    std::string error;

    CHECK(Retail::ParseBuildInfo(s_buildInfo, "wow", key, error));
    CHECK(key == "18b43890bb75affcb09e78272ee9d00c");
    CHECK(Retail::ParseBuildInfo(s_buildInfo, "wow_beta", key, error));
    CHECK(key == "22222222222222222222222222222222");
}

TEST(BuildInfoWithoutTheProductFails) {
    std::string key;
    std::string error;

    CHECK(!Retail::ParseBuildInfo(s_buildInfo, "wow_classic", key, error));
    CHECK(!error.empty());
    CHECK(!Retail::ParseBuildInfo("Branch!STRING:0|Active!DEC:1\neu|1\n", "wow", key, error));
    CHECK(!Retail::ParseBuildInfo("", "wow", key, error));
}

TEST(ConfigRejectsMistakes) {
    Config config;
    std::string error;
    std::string pinned = std::string("\nbuild=") + s_build;

    CHECK(!ParseConfig("mode=online\n" + pinned, s_base, config, error));
    CHECK(!ParseConfig("mode=cloud\npath=Cache" + pinned, s_base, config, error));
    CHECK(!ParseConfig("mode=online\npath=Cache\nlocal=2" + pinned, s_base, config, error));
    CHECK(!ParseConfig("mode=online\npath=Cache\nlocale=xyz" + pinned, s_base, config, error));
    CHECK(!ParseConfig("mode=online\npath=Cache\nno equals sign" + pinned, s_base, config, error));
}
