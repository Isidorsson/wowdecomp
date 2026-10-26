#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "Engine/Source/Retail/AnimationFallbacks.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/RetailDisplays.h"

// Reads retail paths through the same source the client uses, so a cold CDN fetch can happen here
// instead of stalling the game.
//   RetailAssetProbe <RetailCasc.ini> <retail/123.m2|aliased path>...
//   RetailAssetProbe <RetailCasc.ini> --animation-fallbacks   (id;fallback;resolved for ids past 3.3.5)
//   RetailAssetProbe <RetailCasc.ini> --save <dir> <path>...  (also writes what the client would read)
//   RetailAssetProbe <RetailCasc.ini> --displays <id>...      (the served CreatureDisplayInfo rows and model geosets)
//   RetailAssetProbe <RetailCasc.ini> --model-displays <CreatureModelData id>  (the displays that use the model)
//   RetailAssetProbe <RetailCasc.ini> --rows <table fdid> <id>... | all  (every column of the rows with, or related to, an id)

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

static int PrintDisplays(Retail::CascSource& source, int count, char** ids) {
    const Retail::RetailDisplays* displays = source.Displays();
    if (!displays) {
        std::printf("the display tables did not load\n");
        return 1;
    }

    int failures = 0;
    for (int i = 0; i < count; i++) {
        uint32_t id = static_cast<uint32_t>(std::strtoul(ids[i], nullptr, 10));
        const Retail::RetailDisplay* display = displays->Display(id);
        const Retail::RetailCreatureModel* model = display ? displays->Model(display->modelId) : nullptr;
        if (!model) {
            std::printf("display %u: not served\n", id);
            failures++;
            continue;
        }

        std::printf("display %u: model %u fdid %u scale %g extra %u variations %u %u %u geosets %08X; model geosets", id, display->modelId, model->fileDataId,
            display->scale, display->extendedDisplayId, display->variations[0], display->variations[1], display->variations[2], display->geosetData);
        for (uint32_t geoset : source.ModelGeosets(model->fileDataId)) {
            std::printf(" %u", geoset);
        }
        std::printf("\n");
    }

    return failures ? 1 : 0;
}

constexpr size_t MAX_PRINTED_COLUMNS = 40;

// Every column of the rows of a DBFilesClient table (by its FileDataID) with the given ids, for checking a layout.
static int PrintRows(Retail::CascSource& source, uint32_t table, int count, char** ids) {
    std::string path = "retail/" + std::to_string(table) + ".db2";
    std::vector<uint8_t> bytes;
    Retail::Db2Table rows;
    std::string error;
    if (!source.Read(path.c_str(), bytes, error) || !rows.Parse(std::move(bytes), error)) {
        std::printf("%s: %s\n", path.c_str(), error.c_str());
        return 1;
    }

    bool all = count == 1 && !std::strcmp(ids[0], "all");
    for (int i = 0; i < count; i++) {
        uint32_t id = static_cast<uint32_t>(std::strtoul(ids[i], nullptr, 10));
        for (size_t row = 0; row < rows.RowCount(); row++) {
            if (!all && rows.RowId(row) != id && rows.Relation(row) != id) {
                continue;
            }
            std::printf("row %u (relation %u):", rows.RowId(row), rows.Relation(row));
            for (size_t column = 0; column < MAX_PRINTED_COLUMNS; column++) {
                std::vector<uint32_t> values = rows.Values(row, column);
                std::printf(" [%zu]", column);
                for (uint32_t value : values) {
                    std::printf(" %u", value);
                }
            }
            std::printf("\n");
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

    if (!std::strcmp(argv[2], "--rows") && argc >= 5) {
        return PrintRows(source, static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)), argc - 4, argv + 4);
    }

    if (!std::strcmp(argv[2], "--model-displays") && argc >= 4) {
        uint32_t modelId = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
        const Retail::RetailDisplays* displays = source.Displays();
        for (const Retail::RetailDisplay& display : displays ? displays->All() : std::vector<Retail::RetailDisplay>()) {
            if (display.modelId == modelId) {
                std::printf("%u\n", display.id);
            }
        }
        return 0;
    }

    if (!std::strcmp(argv[2], "--displays")) {
        return PrintDisplays(source, argc - 3, argv + 3);
    }

    if (!std::strcmp(argv[2], "--save") && argc >= 5) {
        return ProbePaths(source, argc - 4, argv + 4, argv[3]);
    }

    return ProbePaths(source, argc - 2, argv + 2, nullptr);
}
