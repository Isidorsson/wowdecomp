#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/Retail/AnimationFallbacks.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/DisplayAudit.h"
#include "Engine/Source/Retail/RetailDisplays.h"

// Reads retail paths through the same source the client uses, so a cold CDN fetch can happen here
// instead of stalling the game.
//   RetailAssetProbe <RetailCasc.ini> <retail/123.m2|aliased path>...
//   RetailAssetProbe <RetailCasc.ini> --animation-fallbacks   (id;fallback;resolved for ids past 3.3.5)
//   RetailAssetProbe <RetailCasc.ini> --save <dir> <path>...  (also writes what the client would read)
//   RetailAssetProbe <RetailCasc.ini> --displays <id>...      (the served CreatureDisplayInfo rows and model geosets)
//   RetailAssetProbe <RetailCasc.ini> --model-displays <CreatureModelData id>  (the displays that use the model)
//   RetailAssetProbe <RetailCasc.ini> --rows <table fdid> <id>... | all  (every column of the rows with, or related to, an id)
//   RetailAssetProbe <RetailCasc.ini> --audit <out.csv> <id>... | all  (what each display uses that the client cannot draw)

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

static const char* const NPC_SLOT_NAMES[Retail::NPC_ITEM_SLOTS] = {
    "helm", "shoulders", "shirt", "chest", "belt", "legs", "boots", "wrists", "gloves", "tabard", "cape",
};

static std::string FindingName(const Retail::AuditFinding& finding) {
    std::string name = Retail::AuditFlagName(finding.flag);
    bool slot = finding.flag == Retail::AuditFlag::ArmourModelDropped || finding.flag == Retail::AuditFlag::MissingItem;
    if (slot && finding.detail < Retail::NPC_ITEM_SLOTS) {
        return name + ":" + NPC_SLOT_NAMES[finding.detail];
    }
    return finding.flag == Retail::AuditFlag::NoBake ? name : name + ":" + std::to_string(finding.detail);
}

static std::vector<const Retail::RetailDisplay*> AuditedDisplays(const Retail::RetailDisplays& displays, int count, char** ids) {
    std::vector<const Retail::RetailDisplay*> audited;
    if (count == 1 && !std::strcmp(ids[0], "all")) {
        for (const Retail::RetailDisplay& display : displays.All()) {
            audited.push_back(&display);
        }
        return audited;
    }
    for (int i = 0; i < count; i++) {
        uint32_t id = static_cast<uint32_t>(std::strtoul(ids[i], nullptr, 10));
        const Retail::RetailDisplay* display = displays.Display(id);
        if (!display) {
            std::printf("display %u: not served\n", id);
            continue;
        }
        audited.push_back(display);
    }
    return audited;
}

static void PrintAuditSummary(const std::map<std::string, size_t>& displaysPer, size_t audited, size_t clean) {
    std::vector<std::pair<std::string, size_t>> causes(displaysPer.begin(), displaysPer.end());
    std::sort(causes.begin(), causes.end(), [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
    std::printf("# %zu displays audited, %zu clean\n", audited, clean);
    for (const auto& [cause, displays] : causes) {
        std::printf("%8zu  %s\n", displays, cause.c_str());
    }
}

// One CSV line per display with findings (id, Extra id, findings), then each finding's display count, most first.
static int Audit(Retail::CascSource& source, const char* csvPath, int count, char** ids) {
    const Retail::RetailDisplays* displays = source.Displays();
    const Retail::RetailItemDisplays* items = source.ItemDisplays();
    const Retail::RetailNpcTables* npcs = source.NpcTables();
    if (!displays || !items || !npcs) {
        std::printf("the display, item or NPC tables did not load\n");
        return 1;
    }

    std::ofstream csv(csvPath);
    if (!csv) {
        std::printf("%s: cannot write\n", csvPath);
        return 1;
    }
    csv << "display,extra,findings\n";

    Retail::ItemDisplayLookup lookup = [items](uint32_t id) { return items->Display(id); };
    std::vector<const Retail::RetailDisplay*> audited = AuditedDisplays(*displays, count, ids);
    std::map<std::string, size_t> displaysPer;
    size_t clean = 0;
    for (const Retail::RetailDisplay* display : audited) {
        const Retail::NpcExtra* extra = display->extendedDisplayId ? npcs->Extra(display->extendedDisplayId) : nullptr;
        std::vector<Retail::AuditFinding> findings = Retail::AuditDisplay(*display, extra, lookup);
        if (findings.empty()) {
            clean++;
            continue;
        }
        csv << display->id << ',' << display->extendedDisplayId << ',';
        for (size_t i = 0; i < findings.size(); i++) {
            std::string name = FindingName(findings[i]);
            csv << (i ? ";" : "") << name;
            displaysPer[name]++;
        }
        csv << '\n';
    }

    PrintAuditSummary(displaysPer, audited.size(), clean);
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

    if (!std::strcmp(argv[2], "--audit") && argc >= 5) {
        return Audit(source, argv[3], argc - 4, argv + 4);
    }

    if (!std::strcmp(argv[2], "--save") && argc >= 5) {
        return ProbePaths(source, argc - 4, argv + 4, argv[3]);
    }

    return ProbePaths(source, argc - 2, argv + 2, nullptr);
}
