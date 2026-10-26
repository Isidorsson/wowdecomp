#ifndef TOOLS_TESTS_LOCALSOURCEFIXTURE_H
#define TOOLS_TESTS_LOCALSOURCEFIXTURE_H

#include <filesystem>
#include <fstream>
#include <process.h>
#include <string>
#include <vector>
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/DecodedCache.h"

// A source on a local install that holds no storage, so every file it reads comes from the decoded cache or the seed.

constexpr const char* BUILD_KEY = "88d5e8ef05ec5a8446cdec93d142d7c6";

struct LocalSource {
    std::filesystem::path dir;
    Retail::Config config;
    Retail::CascSource source;
};

inline void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

// Another source on the same install and cache, as after a restart.
inline bool Reopen(LocalSource& local, const std::filesystem::path& dir) {
    local.dir = dir;
    local.config.cache = local.dir / "cache";
    local.config.seed = local.dir / "Seed";
    local.config.build = BUILD_KEY;
    std::string error;
    return local.source.Configure(local.dir / "RetailCasc.ini", error);
}

inline bool Configure(LocalSource& local, const char* name) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / ("wowdecomp-tests-" + std::to_string(_getpid())) / name;
    std::filesystem::remove_all(dir);
    WriteText(dir / "storage" / ".build.info", std::string("Active!DEC:1|Build Key!HEX:16|Product!STRING:0\n1|") + BUILD_KEY + "|wow\n");
    WriteText(dir / "RetailCasc.ini", "mode=local\npath=storage\ncache=cache\n");
    return Reopen(local, dir);
}

inline bool Cache(const LocalSource& local, uint32_t id, const std::vector<uint8_t>& bytes) {
    std::string error;
    return Retail::WriteDecodedCache(Retail::DecodedCachePath(local.config, id), bytes, error);
}

inline bool Seed(const LocalSource& local, uint32_t id, const std::vector<uint8_t>& bytes) {
    std::string error;
    return Retail::WriteDecodedCache(Retail::SeedEntryPath(local.config, id), bytes, error);
}

#endif
