#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "Engine/Source/Retail/CascSource.h"
#include "Tools/RetailServerDisplays/ServerDisplays.h"

// Writes a worldserver's CreatureDisplayInfo.dbc and CreatureModelData.dbc with placeholder rows for retail displays,
// and the creature_model_info rows they need, so creatures can spawn with display ids the client serves from CASC.
//   RetailServerDisplays <RetailCasc.ini> <server dbc dir> <out dir> <display id>...
//   RetailServerDisplays <RetailCasc.ini> <server dbc dir> <out dir> --ids <file>   (ids separated by whitespace)
//   RetailServerDisplays <RetailCasc.ini> <server dbc dir> <out dir> --all-missing
// After <out dir>, --exclude-client <the client's effective CreatureDisplayInfo.dbc> leaves out the ids the client has.

namespace fs = std::filesystem;

constexpr size_t LISTED = 10;

static bool ReadFile(const fs::path& path, std::vector<uint8_t>& bytes) {
    std::ifstream file(path, std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return file.good() || file.eof();
}

static bool WriteFile(const fs::path& path, const void* data, size_t size) {
    std::ofstream file(path, std::ios::binary);
    file.write(static_cast<const char*>(data), size);
    return file.good();
}

static bool LoadTable(const fs::path& path, ServerDisplays::Wdbc& table) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!ReadFile(path, bytes) || !table.Parse(bytes, error)) {
        std::printf("%s: %s\n", path.string().c_str(), error.empty() ? "cannot read" : error.c_str());
        return false;
    }
    return true;
}

static bool ReadIds(const char* path, std::vector<uint32_t>& ids) {
    std::ifstream file(path);
    if (!file) {
        std::printf("%s: cannot read\n", path);
        return false;
    }
    for (uint32_t id; file >> id;) {
        ids.push_back(id);
    }
    return true;
}

static void PrintIds(const char* what, const std::vector<uint32_t>& ids) {
    if (ids.empty()) {
        return;
    }
    std::printf("%zu %s:", ids.size(), what);
    for (size_t i = 0; i < ids.size() && i < LISTED; i++) {
        std::printf(" %u", ids[i]);
    }
    std::printf("%s\n", ids.size() > LISTED ? " ..." : "");
}

static bool WriteOutputs(const fs::path& out, const ServerDisplays::Wdbc& displays, const ServerDisplays::Wdbc& models, const ServerDisplays::Added& added) {
    std::vector<uint8_t> displayBytes = displays.Serialize();
    std::vector<uint8_t> modelBytes = models.Serialize();
    std::string sql = ServerDisplays::ModelInfoSql(added.modelInfo);
    fs::create_directories(out);
    bool written = WriteFile(out / "CreatureDisplayInfo.dbc", displayBytes.data(), displayBytes.size())
        && WriteFile(out / "CreatureModelData.dbc", modelBytes.data(), modelBytes.size())
        && WriteFile(out / "creature_model_info.sql", sql.data(), sql.size());
    if (!written) {
        std::printf("%s: cannot write\n", out.string().c_str());
        return false;
    }
    std::printf("wrote %s: CreatureDisplayInfo.dbc %zu bytes, CreatureModelData.dbc %zu bytes, creature_model_info.sql %zu bytes\n",
        out.string().c_str(), displayBytes.size(), modelBytes.size(), sql.size());
    return true;
}

// The ids args asks for: listed, read from a file, or every retail display the server lacks.
static bool SelectIds(int count, char** args, const ServerDisplays::Wdbc& displays, const Retail::RetailDisplays& retail, std::vector<uint32_t>& ids) {
    if (count < 1) {
        return false;
    }
    if (!std::strcmp(args[0], "--all-missing")) {
        ids = ServerDisplays::MissingDisplayIds(displays, retail);
        return true;
    }
    if (!std::strcmp(args[0], "--ids")) {
        return count >= 2 && ReadIds(args[1], ids);
    }
    for (int i = 0; i < count; i++) {
        ids.push_back(static_cast<uint32_t>(std::strtoul(args[i], nullptr, 10)));
    }
    return true;
}

static bool ExcludeClientIds(const char* clientPath, std::vector<uint32_t>& ids) {
    ServerDisplays::Wdbc client;
    if (!LoadTable(clientPath, client)) {
        return false;
    }
    std::vector<uint32_t> excluded;
    ids = ServerDisplays::WithoutIdsOf(client, ids, excluded);
    std::printf("client: %zu displays, %zu of the ids left out because the client has them\n", client.RowCount(), excluded.size());
    PrintIds("left out", excluded);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::printf("usage: RetailServerDisplays <RetailCasc.ini> <server dbc dir> <out dir> [--exclude-client <dbc>] <id>... | --ids <file> | --all-missing\n");
        return 2;
    }

    Retail::CascSource source;
    std::string error;
    if (!source.Configure(argv[1], error)) {
        std::printf("config: %s\n", error.c_str());
        return 1;
    }
    source.SetWarningSink([](const std::string& warning) { std::printf("  warning: %s\n", warning.c_str()); });

    const Retail::RetailDisplays* retail = source.Displays();
    if (!retail) {
        std::printf("the retail creature tables did not load\n");
        return 1;
    }

    fs::path serverDir = argv[2];
    ServerDisplays::Wdbc displays;
    ServerDisplays::Wdbc models;
    if (!LoadTable(serverDir / "CreatureDisplayInfo.dbc", displays) || !LoadTable(serverDir / "CreatureModelData.dbc", models)) {
        return 1;
    }
    if (!ServerDisplays::KnownLayouts(displays, models, error)) {
        std::printf("%s\n", error.c_str());
        return 1;
    }

    int first = 4;
    const char* clientPath = nullptr;
    if (!std::strcmp(argv[first], "--exclude-client") && argc > first + 1) {
        clientPath = argv[first + 1];
        first += 2;
    }
    std::vector<uint32_t> ids;
    if (!SelectIds(argc - first, argv + first, displays, *retail, ids)) {
        return 2;
    }
    if (clientPath && !ExcludeClientIds(clientPath, ids)) {
        return 1;
    }

    std::printf("server: %zu displays (max %u), %zu models (max %u)\n", displays.RowCount(), displays.MaxId(), models.RowCount(), models.MaxId());
    ServerDisplays::Added added = ServerDisplays::AddRetailDisplays(displays, models, *retail, ids);
    PrintIds("refused, already in the server's CreatureDisplayInfo (stock wins)", added.stock);
    PrintIds("unknown, not a retail display with a model", added.unknown);
    PrintIds("displays added", added.displays);
    std::printf("%u placeholder models added, ids %u to %u\n", added.models, models.MaxId() - added.models + 1, models.MaxId());

    return WriteOutputs(argv[3], displays, models, added) ? 0 : 1;
}
