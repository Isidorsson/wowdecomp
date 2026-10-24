#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "Engine/Source/Retail/AnimationFallbacks.h"
#include "Engine/Source/Retail/CascSource.h"

// Reads retail paths through the same source the client uses, so a cold CDN fetch can happen here
// instead of stalling the game.
//   RetailAssetProbe <RetailCasc.ini> <retail/123.m2|aliased path>...
//   RetailAssetProbe <RetailCasc.ini> --animation-fallbacks   (id;fallback;resolved for ids past 3.3.5)

static int ProbePaths(Retail::CascSource& source, int count, char** paths) {
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

    if (!std::strcmp(argv[2], "--animation-fallbacks")) {
        return PrintAnimationFallbacks(source);
    }

    return ProbePaths(source, argc - 2, argv + 2);
}
