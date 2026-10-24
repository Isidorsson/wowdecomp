#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "Engine/Source/Retail/AnimationFallbacks.h"
#include "Engine/Source/Retail/CascSource.h"

// Reads retail paths through the same source the client uses, so a cold CDN fetch can happen here
// instead of stalling the game.
//   RetailAssetProbe <RetailCasc.ini> <retail/123.m2|aliased path>...
//   RetailAssetProbe <RetailCasc.ini> --animation-fallbacks   (id;fallback;resolved for ids past 3.3.5)
//   RetailAssetProbe <RetailCasc.ini> --save <dir> <path>...  (also writes what the client would read)

static void Save(const char* dir, const char* path, const std::vector<uint8_t>& bytes) {
    std::string name = path;
    for (char& c : name) {
        c = c == '/' || c == '\\' ? '_' : c;
    }

    std::filesystem::create_directories(dir);
    std::ofstream(std::filesystem::path(dir) / name, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

static int ProbePaths(Retail::CascSource& source, int count, char** paths, const char* saveDir) {
    int failures = 0;

    for (int i = 0; i < count; i++) {
        uint32_t size = 0;
        bool cached = source.IsCached(paths[i], size);
        std::vector<uint8_t> bytes;
        std::string error;

        if (!source.Read(paths[i], bytes, error)) {
            std::printf("%s: %s\n", paths[i], error.c_str());
            failures++;
            continue;
        }

        char magic[5] = {};
        for (size_t c = 0; c < 4 && c < bytes.size(); c++) {
            magic[c] = bytes[c] >= 32 && bytes[c] < 127 ? static_cast<char>(bytes[c]) : '.';
        }

        std::printf("%s: %zu bytes, magic %s, %s\n", paths[i], bytes.size(), magic, cached ? "cached" : "fetched");

        if (saveDir) {
            Save(saveDir, paths[i], bytes);
        }
    }

    return failures ? 1 : 0;
}

static int PrintAnimationFallbacks(Retail::CascSource& source) {
    std::string path = "retail/" + std::to_string(Retail::ANIMATION_DATA_FDID) + ".db2";
    std::vector<uint8_t> bytes;
    Retail::Db2Table table;
    Retail::AnimationFallbacks fallbacks;
    std::string error;

    if (!source.Read(path.c_str(), bytes, error) || !table.Parse(std::move(bytes), error) || !fallbacks.Load(table, error)) {
        std::printf("%s: %s\n", path.c_str(), error.c_str());
        return 1;
    }

    std::printf("# %zu rows, layout %08X\n", table.RowCount(), table.LayoutHash());

    for (size_t row = 0; row < table.RowCount(); row++) {
        uint32_t id = table.RowId(row);
        if (id > Retail::LEGACY_MAX_ANIMATION) {
            std::printf("%u;%u;%u\n", id, table.Value(row, 0), fallbacks.Resolve(id));
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: RetailAssetProbe <RetailCasc.ini> <path>... | --animation-fallbacks\n");
        return 2;
    }

    Retail::CascSource source;
    std::string error;

    if (!source.Configure(argv[1], error)) {
        std::printf("config: %s\n", error.c_str());
        return 1;
    }

    source.SetWarningSink([](const std::string& warning) {
        std::printf("  warning: %s\n", warning.c_str());
    });

    if (!std::strcmp(argv[2], "--animation-fallbacks")) {
        return PrintAnimationFallbacks(source);
    }

    if (!std::strcmp(argv[2], "--save") && argc >= 5) {
        return ProbePaths(source, argc - 4, argv + 4, argv[3]);
    }

    return ProbePaths(source, argc - 2, argv + 2, nullptr);
}
