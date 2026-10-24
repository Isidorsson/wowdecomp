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

// RetailCasc.ini. Paths are absolute after parsing: relative ones resolve against the ini's folder.
struct Config {
    StorageMode mode = StorageMode::Online;
    std::filesystem::path path;     // Local: the retail install. Online: the CDN cache folder.
    std::filesystem::path cache;    // Decoded originals. Never inside a local install. Defaults to Cache beside the ini.
    std::string product = "wow";
    std::string region = "us";
    std::string build;              // Build config key (32 hex digits). Required online, so a CDN update never changes assets.
    uint32_t locale = 0x2;          // CASC locale mask, enUS.
    std::filesystem::path aliases;  // Optional FDID;client\path rows that replace stock paths.
};

bool ParseConfig(const std::string& text, const std::filesystem::path& baseDir, Config& config, std::string& error);

// The active Build Key of product in a local install's .build.info. Battle.net patches move it, so it keys the cache.
bool ParseBuildInfo(const std::string& text, const std::string& product, std::string& buildKey, std::string& error);

}

#endif
