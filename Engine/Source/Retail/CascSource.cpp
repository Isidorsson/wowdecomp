#include "Engine/Source/Retail/CascSource.h"
#include <CascLib.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "Engine/Source/Retail/ConvertedCache.h"
#include "Engine/Source/Retail/DecodedCache.h"
#include "Engine/Source/Retail/Db2Table.h"
#include "Engine/Source/Retail/HelmFit.h"
#include "Engine/Source/Retail/RetailModel.h"
#include <cstring>

namespace Retail {

static const char* DEFAULT_CONFIG = "Data/Retail/RetailCasc.ini";

static bool ReadText(const std::filesystem::path& path, std::string& text) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    std::ostringstream contents;
    contents << input.rdbuf();
    text = contents.str();
    return true;
}

// Remapped sequences keep their retail ids here, for a later step that plays them by their own id.
static void WriteRemapped(const std::filesystem::path& path, const std::vector<RemappedSequence>& remapped) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return;
    }

    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream output(path);
    output << "# sequence index;retail id;retail variation;3.3.5 id\n";

    for (const RemappedSequence& sequence : remapped) {
        output << sequence.index << ';' << sequence.originalId << ';' << sequence.originalVariation << ';' << sequence.id << '\n';
    }
}

static bool CascFail(std::string& error, const std::string& message) {
    error = message + " (CascLib error " + std::to_string(GetCascError()) + ")";
    return false;
}

CascSource& CascSource::Instance() {
    static CascSource s_source;
    static std::once_flag s_configured;

    std::call_once(s_configured, [] {
        const char* configured = std::getenv("WOW_RETAIL_CONFIG");
        std::filesystem::path path = configured && *configured ? configured : DEFAULT_CONFIG;
        std::error_code ec;

        if (configured || std::filesystem::exists(path, ec)) {
            std::string error;
            if (!s_source.Configure(path, error)) {
                s_source.m_configError = error;
            }
        }
    });

    return s_source;
}

CascSource::CascSource() = default;

CascSource::~CascSource() {
    if (this->m_storage) {
        CascCloseStorage(this->m_storage);
    }
}

bool CascSource::Configure(const std::filesystem::path& configPath, std::string& error) {
    std::string text;
    if (!ReadText(configPath, text)) {
        error = "cannot read " + configPath.string();
        return false;
    }

    Config config;
    if (!ParseConfig(text, std::filesystem::absolute(configPath).parent_path(), config, error)) {
        return false;
    }

    if (config.mode == StorageMode::Local) {
        std::string buildInfo;
        if (!ReadText(config.path / ".build.info", buildInfo)) {
            error = "no .build.info in " + config.path.string();
            return false;
        }

        if (!ParseBuildInfo(buildInfo, config.product, config.build, error)) {
            return false;
        }
    }

    AliasTable aliases;
    if (!config.aliases.empty()) {
        std::string rows;
        if (!ReadText(config.aliases, rows)) {
            error = "cannot read " + config.aliases.string();
            return false;
        }

        if (!aliases.Parse(rows, error)) {
            return false;
        }
    }

    this->m_config = config;
    this->m_aliases = aliases;
    this->m_configured = true;
    return true;
}

// A path that starts with "r" or whose file name starts with "retail-", in any case: what ClaimsPath can claim
// without an alias.
static bool MayBeRetail(const char* name) {
    if ((name[0] | 0x20) == 'r') {
        return true;
    }
    const char* file = name;
    for (const char* c = name; *c; c++) {
        if (*c == '\\' || *c == '/') {
            file = c + 1;
        }
    }
    size_t prefix = 0;
    while (RETAIL_FILE_PREFIX[prefix] && (file[prefix] | 0x20) == RETAIL_FILE_PREFIX[prefix]) {
        prefix++;
    }
    return !RETAIL_FILE_PREFIX[prefix];
}

bool CascSource::Handles(const char* name) const {
    // Skips the allocation for nearly every stock path. retail/<FileDataID> is claimed even when
    // unconfigured, so it fails loudly in Logs/RetailAssets.log instead of reaching the MPQs.
    if (!this->m_aliases.Size() && !MayBeRetail(name)) {
        return false;
    }

    std::string normalized = NormalizePath(name);
    Dependency dependency;
    return this->FindDependency(normalized, dependency) || ClaimsPath(normalized, this->m_aliases);
}

bool CascSource::FindDependency(const std::string& normalized, Dependency& dependency) const {
    std::shared_lock<std::shared_mutex> lock(this->m_dependencyMutex);
    auto found = this->m_dependencies.find(normalized);

    if (found == this->m_dependencies.end()) {
        return false;
    }

    dependency = found->second;
    return true;
}

bool CascSource::Resolve(const std::string& normalized, uint32_t& fileDataId, std::string& error) {
    if (!this->m_configured) {
        error = this->m_configError.empty() ? std::string("no ") + DEFAULT_CONFIG + " (or WOW_RETAIL_CONFIG)" : this->m_configError;
        return false;
    }

    if (this->m_aliases.Find(normalized, fileDataId) || ParseRetailPath(normalized, fileDataId)) {
        return true;
    }

    RaceFileRequest request;
    if (ParseRaceFileName(normalized, request)) {
        const RetailItemDisplays* displays = this->ItemDisplays();
        fileDataId = displays ? displays->RaceFile(request.fileDataId, request.race, request.gender) : 0;
        if (!fileDataId) {
            error = displays ? "model " + std::to_string(request.fileDataId) + " has no file for race " + std::to_string(request.race) + (request.gender ? " female" : " male")
                             : "no item tables to choose the race file from";
        }
        return fileDataId != 0;
    }

    error = "not a retail/<FileDataID> path or alias";
    return false;
}

bool CascSource::IsCached(const char* name, uint32_t& size) {
    std::string normalized = NormalizePath(name);
    Dependency dependency;
    uint32_t id;
    std::string error;
    std::vector<uint8_t> bytes;

    if (this->FindDependency(normalized, dependency)) {
        id = dependency.skin;
    } else if (!this->Resolve(normalized, id, error)) {
        return false;
    }

    if (!ReadDecodedCache(DecodedCachePath(this->m_config, id), bytes)) {
        return false;
    }

    size = static_cast<uint32_t>(bytes.size());
    return true;
}

bool CascSource::Exists(const char* name, std::string& error) {
    std::vector<uint8_t> bytes;
    return this->Read(name, bytes, error);
}

bool CascSource::Read(const char* name, std::vector<uint8_t>& bytes, std::string& error) {
    std::string normalized = NormalizePath(name);
    Dependency dependency;

    // Before the FileDataID rule: retail/91654100.skin is view 0 of model 916541, not FileDataID 91654100.
    if (this->FindDependency(normalized, dependency)) {
        return this->ReadSkin(normalized, dependency, bytes, error);
    }

    return this->ReadModel(normalized, bytes, error);
}

// Any claimed file; a retail model comes back converted, from the converted cache when it holds it, its skins
// registered under the client's names.
bool CascSource::ReadModel(const std::string& normalized, std::vector<uint8_t>& bytes, std::string& error) {
    uint32_t id;
    if (!this->Resolve(normalized, id, error)) {
        return false;
    }

    ConvertedModel model;
    if (!ReadConvertedCache(ConvertedCachePath(this->m_config, id), ConverterKey(), model)) {
        if (!this->ReadOriginal(id, bytes, error)) {
            return false;
        }

        if (bytes.size() < 4 || std::memcmp(bytes.data(), "MD21", 4) != 0) {
            return true;
        }

        if (!this->Convert(id, bytes, model, error)) {
            return false;
        }
    }

    return this->Serve(normalized, id, model, bytes, error);
}

// Published unless what it read may differ next time: online, tables that failed to load or a fetch that failed
// may succeed on a later try. Locally they fail the same way every time.
bool CascSource::Convert(uint32_t id, const std::vector<uint8_t>& file, ConvertedModel& model, std::string& error) {
    bool readFailed = false;
    ModelSources sources;
    sources.fallbacks = this->Fallbacks();
    sources.skin = this->Skins() ? this->Skins()->Default(id) : nullptr;
    sources.read = [this, &readFailed](uint32_t dependency, std::vector<uint8_t>& data, std::string& readError) {
        bool read = this->ReadOriginal(dependency, data, readError);
        readFailed |= !read;
        return read;
    };

    if (!ConvertModel(file, sources, model, error)) {
        return false;
    }

    bool complete = sources.fallbacks && this->Skins() && !readFailed;
    if (complete || this->m_config.mode == StorageMode::Local) {
        // A failed cache write only costs a conversion next time.
        std::string cacheError;
        WriteConvertedCache(ConvertedCachePath(this->m_config, id), ConverterKey(), model, cacheError);
    }
    return true;
}

bool CascSource::Serve(const std::string& normalized, uint32_t id, ConvertedModel& model, std::vector<uint8_t>& bytes, std::string& error) {
    // Fitted per read, never cached: both caches hold the model by FileDataID, which a weapon-style name or another
    // race's helm may share.
    uint32_t race;
    uint32_t gender;
    if (HelmRaceForPath(normalized, race, gender) && !FitHelmToStockHead(model.m2, this->FitFor(race, gender), error)) {
        return false;
    }

    for (const std::string& warning : model.warnings) {
        this->Warn(normalized + ": " + warning);
    }

    if (!model.remapped.empty()) {
        WriteRemapped(DecodedCachePath(this->m_config, id).parent_path().parent_path() / "sequences" / (std::to_string(id) + ".csv"), model.remapped);
    }

    std::shared_ptr<ConvertedSkins> converted;
    if (model.skinBytes.size() == model.skins.size()) {
        converted = std::make_shared<ConvertedSkins>(std::move(model.skinBytes));
    }

    {
        std::unique_lock<std::shared_mutex> lock(this->m_dependencyMutex);
        for (uint32_t view = 0; view < model.skins.size(); view++) {
            this->m_dependencies[SkinPathFor(normalized, view)] = { model.skins[view], id, view, converted };
        }
    }

    bytes = std::move(model.m2);
    return true;
}

HelmFit CascSource::FitFor(uint32_t race, uint32_t gender) {
    std::lock_guard<std::mutex> lock(this->m_fitMutex);
    uint32_t key = race * 2 + gender;
    auto found = this->m_fits.find(key);
    if (found == this->m_fits.end()) {
        found = this->m_fits.emplace(key, this->ChooseFit(race, gender)).first;
    }
    return found->second;
}

// The installed body's own fit when it is an HD port, read off it and the retail body; the table otherwise.
HelmFit CascSource::ChooseFit(uint32_t race, uint32_t gender) {
    std::string body = StockBodyPath(race, gender);
    std::vector<uint8_t> stock;
    std::vector<uint8_t> retail;
    HeadFrame stockFrame;
    HeadFrame retailFrame;
    HelmFit fit;
    std::string error;

    if (!this->m_readStock || !this->m_readStock(body, stock)) {
        error = "the body is unreadable";
    } else if (ReadHeadFrame(stock, stockFrame, error) && this->ReadOriginal(RetailBodyFileDataId(race, gender), retail, error)
               && ReadHeadFrame(retail, retailFrame, error)) {
        if (DeriveHelmFit(stockFrame, retailFrame, fit)) {
            this->Warn(body + ": the retail skeleton, so retail helms move by (" + std::to_string(fit.shiftX) + ", " + std::to_string(fit.shiftZ) + ")");
            return fit;
        }
        error = "not the retail skeleton";
    }

    this->Warn(body + ": helms fitted by the table, " + error);
    return *HelmFitFor(race, gender);
}

// The skin the model's conversion kept when there is one, else the converted cache's; else converted from the originals.
bool CascSource::ReadSkin(const std::string& normalized, const Dependency& dependency, std::vector<uint8_t>& bytes, std::string& error) {
    if (this->TakeConvertedSkin(normalized, bytes)) {
        return true;
    }

    ConvertedModel cached;
    if (ReadConvertedCache(ConvertedCachePath(this->m_config, dependency.model), ConverterKey(), cached) && dependency.view < cached.skinBytes.size()
        && !cached.skinBytes[dependency.view].empty()) {
        bytes = std::move(cached.skinBytes[dependency.view]);
        return true;
    }

    std::vector<uint8_t> original;
    std::vector<uint8_t> file;

    if (!this->ReadOriginal(dependency.model, file, error) || !this->ReadOriginal(dependency.skin, original, error)) {
        return false;
    }

    SkinInput input{ original, file, dependency.view, [this](uint32_t skin, std::vector<uint8_t>& data, std::string& readError) {
        return this->ReadOriginal(skin, data, readError);
    } };
    return ConvertSkin(input, bytes, error);
}

// Once: the client loads one view of a model per load, so its other views' skins are dropped with it.
bool CascSource::TakeConvertedSkin(const std::string& normalized, std::vector<uint8_t>& bytes) {
    std::unique_lock<std::shared_mutex> lock(this->m_dependencyMutex);
    auto found = this->m_dependencies.find(normalized);
    if (found == this->m_dependencies.end() || !found->second.converted || (*found->second.converted)[found->second.view].empty()) {
        return false;
    }

    std::shared_ptr<ConvertedSkins> converted = found->second.converted;
    bytes = std::move((*converted)[found->second.view]);
    for (auto& entry : this->m_dependencies) {
        if (entry.second.converted == converted) {
            entry.second.converted.reset();
        }
    }
    return true;
}

// AnimationData from the same build as the models, read once. Without it, models with new animation ids are
// refused, which the read error explains.
const AnimationFallbacks* CascSource::Fallbacks() {
    std::call_once(this->m_fallbacksRead, [this] {
        std::vector<uint8_t> bytes;
        Db2Table table;
        std::string error;

        if (!this->ReadTable(ANIMATION_DATA_FDID, bytes, error) || !table.Parse(std::move(bytes), error) || !this->m_fallbacks.Load(table, error)) {
            this->Warn("AnimationData (" + std::to_string(ANIMATION_DATA_FDID) + "): " + error);
            return;
        }

        this->m_hasFallbacks = true;
    });

    return this->m_hasFallbacks ? &this->m_fallbacks : nullptr;
}

// Each table parsed from its FileDataID, through ReadTable.
bool CascSource::ReadTables(const std::vector<std::pair<uint32_t, Db2Table*>>& tables, std::string& error) {
    for (const auto& [fileDataId, table] : tables) {
        std::vector<uint8_t> bytes;
        if (!this->ReadTable(fileDataId, bytes, error) || !table->Parse(std::move(bytes), error)) {
            return false;
        }
    }
    return true;
}

// The display tables, from the same build as the models, read once: the default creature and item skins and the
// retail creature and item displays. The creature and item halves load apart; one that fails leaves its skin slots unnamed
// (drawn white until a display fills them), which its warning explains.
void CascSource::ReadDisplayTables() {
    std::call_once(this->m_displayTablesRead, [this] {
        std::string error;
        if (!this->LoadCreatureTables(error)) {
            this->Warn("creature tables: " + error);
        }
        if (!this->LoadItemTables(error)) {
            this->Warn("item tables: " + error);
        }
    });
}

bool CascSource::LoadCreatureTables(std::string& error) {
    Db2Table modelData;
    Db2Table displayInfo;
    Db2Table geosets;
    if (!this->ReadTables({ { CREATURE_MODEL_DATA_FDID, &modelData }, { CREATURE_DISPLAY_INFO_FDID, &displayInfo },
            { CREATURE_DISPLAY_INFO_GEOSET_DATA_FDID, &geosets } }, error)) {
        return false;
    }

    CreatureTables tables = { modelData, displayInfo, geosets };
    this->m_hasDisplays = this->m_displays.Load(tables, error);
    if (!this->m_hasDisplays || !this->m_skins.LoadCreatures(tables, error)) {
        return false;
    }
    this->m_hasSkins = true;
    return true;
}

bool CascSource::LoadItemTables(std::string& error) {
    Db2Table modelFiles;
    Db2Table itemDisplays;
    Db2Table layers;
    Db2Table textureFiles;
    Db2Table componentModels;
    Db2Table appearances;
    Db2Table sections;
    Db2Table componentTextures;
    Db2Table helmetGeosets;
    if (!this->ReadTables({ { MODEL_FILE_DATA_FDID, &modelFiles }, { ITEM_DISPLAY_INFO_FDID, &itemDisplays }, { ITEM_DISPLAY_INFO_MODEL_MAT_RES_FDID, &layers },
            { TEXTURE_FILE_DATA_FDID, &textureFiles }, { COMPONENT_MODEL_FILE_DATA_FDID, &componentModels }, { ITEM_APPEARANCE_FDID, &appearances },
            { ITEM_DISPLAY_INFO_MATERIAL_RES_FDID, &sections }, { COMPONENT_TEXTURE_FILE_DATA_FDID, &componentTextures },
            { HELMET_GEOSET_DATA_FDID, &helmetGeosets } }, error)) {
        return false;
    }

    this->m_hasItemDisplays = this->m_itemDisplays.Load({ itemDisplays, modelFiles, componentModels, textureFiles, appearances, sections, componentTextures, helmetGeosets }, error);
    if (!this->m_hasItemDisplays || !this->m_skins.LoadItems({ modelFiles, itemDisplays, layers, textureFiles }, error)) {
        return false;
    }
    this->m_hasSkins = true;
    return true;
}

const ModelSkins* CascSource::Skins() {
    this->ReadDisplayTables();
    return this->m_hasSkins ? &this->m_skins : nullptr;
}

const RetailDisplays* CascSource::Displays() {
    if (!this->m_configured) {
        return nullptr;
    }
    this->ReadDisplayTables();
    return this->m_hasDisplays ? &this->m_displays : nullptr;
}

const RetailItemDisplays* CascSource::ItemDisplays() {
    if (!this->m_configured) {
        return nullptr;
    }
    this->ReadDisplayTables();
    return this->m_hasItemDisplays ? &this->m_itemDisplays : nullptr;
}

void CascSource::Warn(const std::string& message) const {
    if (this->m_warn) {
        this->m_warn(message);
    }
}

bool CascSource::ReadOriginal(uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
    return this->ReadCached(id, CASC_STRICT_DATA_CHECK, bytes, error);
}

bool CascSource::ReadTable(uint32_t id, std::vector<uint8_t>& bytes, std::string& error) {
    return this->ReadCached(id, CASC_STRICT_DATA_CHECK | CASC_OVERCOME_ENCRYPTED, bytes, error);
}

bool CascSource::ReadCached(uint32_t id, uint32_t openFlags, std::vector<uint8_t>& bytes, std::string& error) {
    std::filesystem::path cachePath = DecodedCachePath(this->m_config, id);
    if (ReadDecodedCache(cachePath, bytes)) {
        return true;
    }

    std::lock_guard<std::mutex> lock(this->m_storageMutex);

    // Another thread may have fetched it while this one waited for the lock.
    if (ReadDecodedCache(cachePath, bytes)) {
        return true;
    }

    if (!this->ReadStorage(id, openFlags, bytes, error)) {
        return false;
    }

    // A failed cache write only costs a refetch next launch; the bytes are still good.
    std::string cacheError;
    WriteDecodedCache(cachePath, bytes, cacheError);
    return true;
}

bool CascSource::OpenStorage(std::string& error) {
    if (this->m_storage) {
        return true;
    }

    bool online = this->m_config.mode == StorageMode::Online;
    std::string path = this->m_config.path.string();

    CASC_OPEN_STORAGE_ARGS args = {};
    args.Size = sizeof(args);
    args.szLocalPath = path.c_str();
    args.szCodeName = this->m_config.product.c_str();
    args.szRegion = this->m_config.region.c_str();
    args.szBuildKey = this->m_config.build.empty() ? nullptr : this->m_config.build.c_str();
    args.dwLocaleMask = this->m_config.locale;

    if (online) {
        std::error_code ec;
        std::filesystem::create_directories(this->m_config.path, ec);
    }

    HANDLE storage = nullptr;
    if (!CascOpenStorageEx(nullptr, &args, online, &storage)) {
        return CascFail(error, "cannot open " + std::string(online ? "online" : "local") + " CASC storage at " + path);
    }

    this->m_storage = storage;
    return true;
}

bool CascSource::ReadStorage(uint32_t fileDataId, uint32_t openFlags, std::vector<uint8_t>& bytes, std::string& error) {
    if (!this->OpenStorage(error)) {
        return false;
    }

    HANDLE file = nullptr;
    if (!CascOpenFile(this->m_storage, CASC_FILE_DATA_ID(fileDataId), this->m_config.locale, CASC_OPEN_BY_FILEID | openFlags, &file)) {
        return CascFail(error, "FileDataID " + std::to_string(fileDataId) + " is not in this build");
    }

    ULONGLONG size = 0;
    DWORD read = 0;
    bool ok = CascGetFileSize64(file, &size) && size <= DECODED_CACHE_LIMIT;

    if (ok) {
        bytes.resize(static_cast<size_t>(size));
        ok = size == 0 || (CascReadFile(file, bytes.data(), static_cast<DWORD>(size), &read) && read == size);
    }

    CascCloseFile(file);

    if (!ok) {
        bytes.clear();
        return CascFail(error, "cannot read FileDataID " + std::to_string(fileDataId) + " (missing data, encryption key or over the size limit)");
    }

    return true;
}

}
