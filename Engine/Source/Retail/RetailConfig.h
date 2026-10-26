#ifndef ENGINE_SOURCE_RETAIL_RETAILCONFIG_H
#define ENGINE_SOURCE_RETAIL_RETAILCONFIG_H

#include <cstdint>
#include <filesystem>
#include <string>

namespace Retail {

enum class StorageMode {
    Online,
    Local,
};

// Which source draws a stock creature display id (RetailDisplayChoice.h). Stock: the client's own row, as before.
// Models: the retail row, for displays without an Extra row. All: humanoids too, through the NPC bake path.
enum class CreatureDisplays {
    Stock,
    Models,
    All,
};

bool ParseCreatureDisplays(const std::string& text, CreatureDisplays& mode);

// RetailCasc.ini. Paths are absolute after parsing: relative ones resolve against the ini's folder.
struct Config {
    StorageMode mode = StorageMode::Online;
    std::filesystem::path path;     // Local: the retail install. Online: the CDN cache folder.
    std::filesystem::path cache;    // Decoded originals. Never inside a local install. Defaults to Cache beside the ini.
    std::filesystem::path seed;     // Read-only decoded originals that ship with the client. Defaults to Seed beside the ini.
    std::string product = "wow";
    std::string region = "us";
    std::string build;              // Build config key (32 hex digits). Required online, so a CDN update never changes assets.
    uint32_t locale = 0x2;          // CASC locale mask, enUS.
    std::filesystem::path aliases;  // Optional FDID;client\path rows that replace stock paths.
    uint32_t convertedCacheMb = 2048; // The converted cache's cap; 0 leaves it uncapped.
    uint32_t decodedCacheMb = 4096;   // The decoded cache's cap; 0 leaves it uncapped.
    CreatureDisplays creatureDisplays = CreatureDisplays::Stock;
};

// Where this build's caches live: "<cache>/<product>-<build key>", or "<product>-local" without a build key.
std::filesystem::path BuildCacheFolder(const Config& config);

// Where this build's seed lives, named as its cache folder is: "<seed>/<product>-<build key>", or "<product>-local".
std::filesystem::path SeedFolder(const Config& config);

bool ParseConfig(const std::string& text, const std::filesystem::path& baseDir, Config& config, std::string& error);

// The active Build Key of product in a local install's .build.info. Battle.net patches move it, so it keys the cache.
bool ParseBuildInfo(const std::string& text, const std::string& product, std::string& buildKey, std::string& error);

}

#endif
