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
    this->StopPrefetch();
    if (this->m_browserWorker.joinable()) {
        this->m_browserWorker.join();
    }
    for (std::thread& preload : this->m_preloads) {
        preload.join();
    }
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
    this->m_convertedBudget.Start(ConvertedCacheFolder(config), uint64_t(config.convertedCacheMb) << 20);
    this->m_decodedBudget.Start(DecodedCacheFolder(config), uint64_t(config.decodedCacheMb) << 20);
    return true;
}

void CascSource::Preload() {
    if (!this->m_configured) {
        return;
    }

    std::call_once(this->m_preloadStarted, [this] {
        this->m_preloads.emplace_back([this] {
            this->ReadDisplayTables();
            this->Fallbacks();
        });
        this->m_preloads.emplace_back([this] { this->NpcTables(); });
        this->m_preloads.emplace_back([this] {
            std::string error;
            if (!this->OpenStorage(error)) {
                this->Warn(error);
            }
        });
        this->m_preloads.emplace_back([this] {
            std::filesystem::path current = BuildCacheFolder(this->m_config);
            for (const std::string& removed : RemoveOtherBuilds(current.parent_path(), this->m_config.product, current.filename().string())) {
                this->Warn("removed the cache of another build, " + (current.parent_path() / removed).string());
            }
        });
    });
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

    if (this->m_aliases.Find(normalized, fileDataId) || ParseRetailPath(normalized, fileDataId) || ParseNpcBodyPath(normalized, fileDataId)) {
        return true;
    }

    RaceFileRequest request;
    if (ParseRaceFileName(normalized, request)) {
        return this->ResolveRaceFile(request, fileDataId, error);
    }

    error = "not a retail/<FileDataID> path or alias";
    return false;
}

bool CascSource::IsCached(const char* name, uint32_t& size) {
    std::string normalized = NormalizePath(name);
    Dependency dependency;
    uint32_t id;
    std::string error;

    if (this->FindDependency(normalized, dependency)) {
        id = dependency.skin;
    } else if (!this->Resolve(normalized, id, error)) {
        return false;
    }

    DecodedCacheHead head;
    if (!PeekDecodedCache(DecodedCachePath(this->m_config, id), head)) {
        return false;
    }

    size = head.size;
    return true;
}

// Skins and race-named helms are always read: a skin's model may need converting, and a helm's fit may fail.
bool CascSource::Exists(const char* name, std::string& error) {
    std::string normalized = NormalizePath(name);
    Dependency dependency;
    uint32_t id;
    uint32_t race;
    uint32_t gender;
    WarpedBakeRequest bake;

    if (ParseWarpedBakeName(normalized, bake)) {
        if (HasConvertedCache(ConvertedBakePath(this->m_config, bake.fileDataId, bake.race, bake.gender), ConverterKey())) {
            return true;
        }
    } else if (ParseNpcBodyPath(normalized, id)) {
        if (HasConvertedCache(ConvertedNpcBodyPath(this->m_config, id), ConverterKey())) {
            return true;
        }
    } else if (!this->FindDependency(normalized, dependency) && !HelmRaceForPath(normalized, race, gender)) {
        if (!this->Resolve(normalized, id, error)) {
            return false;
        }
        if (this->IsServableUnread(id)) {
            return true;
        }
    }

    std::vector<uint8_t> bytes;
    if (!this->Read(name, bytes, error)) {
        return false;
    }
    this->KeepForOpen(normalized, std::move(bytes));
    return true;
}

bool CascSource::IsServableUnread(uint32_t id) {
    std::vector<uint8_t> held;
    if (this->m_memory.Find(id, held) && (held.size() < 4 || std::memcmp(held.data(), "MD21", 4) != 0)) {
        return true;
    }

    if (HasConvertedCache(ConvertedCachePath(this->m_config, id), ConverterKey())) {
        return true;
    }

    DecodedCacheHead head;
    return PeekDecodedCache(DecodedCachePath(this->m_config, id), head) && (head.size < 4 || std::memcmp(head.magic, "MD21", 4) != 0);
}

bool CascSource::Read(const char* name, std::vector<uint8_t>& bytes, std::string& error) {
    std::string normalized = NormalizePath(name);
    Dependency dependency;
    WarpedBakeRequest bake;

    if (this->TakeKept(normalized, bytes)) {
        return true;
    }

    if (ParseWarpedBakeName(normalized, bake)) {
        return this->ReadWarpedBake(bake, bytes, error);
    }

    // Before the FileDataID rule: retail/91654100.skin is view 0 of model 916541, not FileDataID 91654100.
    if (this->FindDependency(normalized, dependency)) {
        return this->ReadSkin(normalized, dependency, bytes, error);
    }

    return this->ReadModel(normalized, bytes, error);
}

static bool IsTexture(const std::string& normalized) {
    return normalized.size() >= 4 && normalized.compare(normalized.size() - 4, 4, ".blp") == 0;
}

// Any claimed file; a retail model comes back converted, from the converted cache when it holds it, its skins
// registered under the client's names. A texture is never converted, so it skips the cache lookup.
bool CascSource::ReadModel(const std::string& normalized, std::vector<uint8_t>& bytes, std::string& error) {
    uint32_t id;
    if (!this->Resolve(normalized, id, error)) {
        return false;
    }
    if (IsTexture(normalized)) {
        return this->ReadOriginal(id, bytes, error);
    }

    Conversion conversion = this->ConversionFor(normalized, id);
    ConvertedModel model;
    if (!ReadConvertedCache(conversion.cachePath, ConverterKey(), model)) {
        if (!this->ReadOriginal(id, bytes, error)) {
            return false;
        }

        if (bytes.size() < 4 || std::memcmp(bytes.data(), "MD21", 4) != 0) {
            return true;
        }

        if (!this->Convert(conversion, bytes, model, error)) {
            return false;
        }
    }

    return this->Serve(normalized, conversion, model, bytes, error);
}

// Published unless what it read may differ next time: online, tables that failed to load or a fetch that failed
// may succeed on a later try. Locally they fail the same way every time.
bool CascSource::Convert(const Conversion& conversion, const std::vector<uint8_t>& file, ConvertedModel& model, std::string& error) {
    uint32_t id = conversion.fileDataId;
    bool readFailed = false;
    ModelSources sources;
    sources.playerBody = conversion.playerBody;
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
        const std::filesystem::path& path = conversion.cachePath;
        std::error_code ec;
        uintmax_t size = WriteConvertedCache(path, ConverterKey(), model, cacheError) ? std::filesystem::file_size(path, ec) : 0;
        if (size && !ec) {
            this->m_convertedBudget.Added(size);
        }
    }
    return true;
}

bool CascSource::Serve(const std::string& normalized, const Conversion& conversion, ConvertedModel& model, std::vector<uint8_t>& bytes, std::string& error) {
    // Fitted per read, never cached: both caches hold the model by FileDataID, which a weapon-style name or another
    // race's helm may share. A served race's helm (race 0 here) hangs on its retail body unmoved.
    uint32_t id = conversion.fileDataId;
    uint32_t race;
    uint32_t gender;
    if (HelmRaceForPath(normalized, race, gender) && !HelmFitsUnmoved(race, gender) && !FitHelmToStockHead(model.m2, this->FitFor(race, gender), error)) {
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
            this->m_dependencies[SkinPathFor(normalized, view)] = { model.skins[view], id, view, converted, conversion.playerBody };
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
    std::filesystem::path modelCache = dependency.npcBody ? ConvertedNpcBodyPath(this->m_config, dependency.model) : ConvertedCachePath(this->m_config, dependency.model);
    if (ReadConvertedCache(modelCache, ConverterKey(), cached) && dependency.view < cached.skinBytes.size()
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
    this->m_itemAppearances = std::move(appearances);
    this->m_hasItemAppearances = true;
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

}
