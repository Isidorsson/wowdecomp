#ifndef ENGINE_SOURCE_RETAIL_CASCSOURCE_H
#define ENGINE_SOURCE_RETAIL_CASCSOURCE_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>
#include "Engine/Source/Retail/AnimationFallbacks.h"
#include "Engine/Source/Retail/BrowserMarshal.h"
#include "Engine/Source/Retail/BakeWarp.h"
#include "Engine/Source/Retail/CacheBudget.h"
#include "Engine/Source/Retail/HelmFit.h"
#include "Engine/Source/Retail/ModelSkins.h"
#include "Engine/Source/Retail/RetailConfig.h"
#include "Engine/Source/Retail/RetailDisplays.h"
#include "Engine/Source/Retail/RetailItemDisplays.h"
#include "Engine/Source/Retail/RetailNpcDisplay.h"
#include "Engine/Source/Retail/RetailPath.h"
#include "Engine/Source/Retail/RetailRaces.h"

namespace Retail {

struct ConvertedModel;

// Serves retail/<FileDataID> paths and aliased stock paths from one pinned CASC build. Reads come from the
// decoded cache when possible; a miss opens the storage (online: downloads build metadata) and blocks.
// Retail models (MD21) are converted to 3.3.5 once per converter version and kept in the converted cache, and
// the skin names the client derives from them (MakeSkinFileName) are routed to the skins their SFID lists, served
// from that conversion.
class CascSource {
    public:
    // Process-wide source configured from WOW_RETAIL_CONFIG, else Data/Retail/RetailCasc.ini. Without a
    // config file it handles nothing; a broken one leaves ConfigError() set.
    static CascSource& Instance();

    CascSource();
    ~CascSource();
    CascSource(const CascSource&) = delete;
    CascSource& operator=(const CascSource&) = delete;

    bool Configure(const std::filesystem::path& configPath, std::string& error);
    bool Configured() const { return this->m_configured; }
    const std::string& ConfigError() const { return this->m_configError; }

    // Starts the one-time loads a first use would otherwise wait for, on workers: the display tables, the animation
    // fallbacks and the storage a first decoded-cache miss opens. Callers that arrive before one finishes wait for
    // it. Also deletes the caches of the product's other builds. Once, after the warning sink is set: the workers
    // warn through it.
    void Preload();

    // Cheap and lock-free: called on every SFile open.
    bool Handles(const char* name) const;
    // A converted model, or a cached original that is not a retail model, is found without reading it; anything
    // else is read, which leaves it cached for the open that follows.
    bool Exists(const char* name, std::string& error);
    bool IsCached(const char* name, uint32_t& size);
    bool Read(const char* name, std::vector<uint8_t>& bytes, std::string& error);

    // The retail creature displays of the pinned build, read with the default skins on first use. Null without a
    // configured storage or when the creature tables fail, which a warning explains.
    const RetailDisplays* Displays();
    const RetailItemDisplays* ItemDisplays();

    // The retail NPC tables (Extra rows, their items and customization), read on first use. Null without a configured
    // storage or when a table fails, which a warning explains; still-locked rows are skipped with one warning a table.
    const RetailNpcTables* NpcTables();

    // Whether the installed body of race and sex is the HD port of the retail HD body (BodyMatchRatio at least
    // BODY_MATCH_THRESHOLD), so HD bakes can be warped onto it. Matched once per body.
    bool InstalledBodyFits(uint32_t race, uint32_t sex);

    // The retail races (ChrRaces with their body displays), read on first use; null as NpcTables is.
    const RetailRaces* Races();

    // The geoset ids of a retail model's first view (its SFID skin), read once per model; empty when unreadable,
    // which a warning explains.
    const std::vector<uint32_t>& ModelGeosets(uint32_t fileDataId);

    // The retail browser's search index (RetailIndex.h). The first call starts building it on a worker and every call
    // returns at once: Building until it is ready, then Ready with the index, or Failed with the reason (also warned).
    // Idle without a configured storage.
    BrowserIndexView BrowserIndex();

    // Where a read reports data it dropped but did not fail on, such as a missing .anim file.
    void SetWarningSink(std::function<void(const std::string&)> sink) { this->m_warn = std::move(sink); }
    void Warn(const std::string& message) const;

    // Reads a file from the client's own archives: the bodies a race-named helm is fitted to. Without it, helms
    // are fitted by the table.
    using StockReader = std::function<bool(const std::string& path, std::vector<uint8_t>& bytes)>;
    void SetStockReader(StockReader reader) { this->m_readStock = std::move(reader); }

    private:
    // Every view's skin as the model's conversion converted it, shared by the model's dependencies.
    using ConvertedSkins = std::vector<std::vector<uint8_t>>;

    // A skin the client names after a converted model.
    struct Dependency {
        uint32_t skin;
        uint32_t model;
        uint32_t view;
        std::shared_ptr<ConvertedSkins> converted;   // Until one of the model's skins is read.
        bool npcBody = false;                         // The model was converted as an NPC body.
    };

    // A race-named helm resolves to the file RetailItemDisplays::RaceFile chooses, which reads the item tables.
    bool Resolve(const std::string& normalized, uint32_t& fileDataId, std::string& error);
    bool FindDependency(const std::string& normalized, Dependency& dependency) const;
    bool IsServableUnread(uint32_t fileDataId) const;
    bool ReadModel(const std::string& normalized, std::vector<uint8_t>& bytes, std::string& error);
    // A model's conversion, kept at cachePath; as an NPC body (playerBody) a player race model converts too.
    struct Conversion {
        uint32_t fileDataId;
        std::filesystem::path cachePath;
        bool playerBody;
    };
    bool Convert(const Conversion& conversion, const std::vector<uint8_t>& file, ConvertedModel& model, std::string& error);
    bool Serve(const std::string& normalized, const Conversion& conversion, ConvertedModel& model, std::vector<uint8_t>& bytes, std::string& error);
    Conversion ConversionFor(const std::string& normalized, uint32_t fileDataId) const;
    bool ResolveRaceFile(const RaceFileRequest& request, uint32_t& fileDataId, std::string& error);
    bool LoadRaceTables(std::string& error);
    bool ReadSkin(const std::string& normalized, const Dependency& dependency, std::vector<uint8_t>& bytes, std::string& error);
    bool TakeConvertedSkin(const std::string& normalized, std::vector<uint8_t>& bytes);
    // An installed body matched to its retail HD body, once: whether it fits, and the warp HD bakes take onto it.
    struct NpcBody {
        bool fits = false;
        BakeWarp warp;
        bool hasBase = false;
        Image base;   // The installed base skin, for the triangles the bake has no texels for.
    };
    const NpcBody& BodyFor(uint32_t race, uint32_t sex);
    NpcBody MatchBody(uint32_t race, uint32_t sex);
    bool ReadRetailBody(uint32_t fileDataId, BodyMesh& mesh, std::string& error);
    void ReadBaseSkin(uint32_t race, uint32_t sex, NpcBody& body);
    bool ReadWarpedBake(const WarpedBakeRequest& request, std::vector<uint8_t>& bytes, std::string& error);
    bool LoadNpcTables(std::string& error);
    // The helm fit of a stock race and gender, chosen once.
    HelmFit FitFor(uint32_t race, uint32_t gender);
    HelmFit ChooseFit(uint32_t race, uint32_t gender);
    bool ReadOriginal(uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error);
    // A .db2 table: blocks encrypted with a missing key come back zero-filled, which Db2Table skips by section.
    bool ReadTable(uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error);
    bool ReadCached(uint32_t fileDataId, uint32_t openFlags, std::vector<uint8_t>& bytes, std::string& error);
    bool ReadTables(const std::vector<std::pair<uint32_t, Db2Table*>>& tables, std::string& error);
    const AnimationFallbacks* Fallbacks();
    const ModelSkins* Skins();
    void ReadDisplayTables();
    bool LoadCreatureTables(std::string& error);
    bool LoadItemTables(std::string& error);
    bool OpenStorage(std::string& error);
    bool LoadBrowserIndex(std::string& error);
    bool LoadBrowserStats(std::string& error);
    BrowserLookups BrowserDisplayLookups();
    bool ReadStorage(uint32_t fileDataId, uint32_t openFlags, std::vector<uint8_t>& bytes, std::string& error);

    bool m_configured = false;
    std::string m_configError;
    Config m_config;
    AliasTable m_aliases;
    std::mutex m_storageMutex;
    void* m_storage = nullptr;
    mutable std::shared_mutex m_dependencyMutex;
    std::map<std::string, Dependency> m_dependencies;
    std::once_flag m_fallbacksRead;
    AnimationFallbacks m_fallbacks;
    bool m_hasFallbacks = false;
    std::once_flag m_displayTablesRead;
    ModelSkins m_skins;
    bool m_hasSkins = false;
    RetailDisplays m_displays;
    bool m_hasDisplays = false;
    RetailItemDisplays m_itemDisplays;
    bool m_hasItemDisplays = false;
    std::function<void(const std::string&)> m_warn;
    StockReader m_readStock;
    std::once_flag m_npcTablesRead;
    RetailNpcTables m_npcTables;
    bool m_hasNpcTables = false;
    std::mutex m_bodyMutex;
    std::map<uint32_t, NpcBody> m_bodies;
    std::once_flag m_raceTablesRead;
    RetailRaces m_races;
    bool m_hasRaces = false;
    std::mutex m_geosetMutex;
    std::map<uint32_t, std::vector<uint32_t>> m_modelGeosets;
    std::mutex m_fitMutex;
    std::map<uint32_t, HelmFit> m_fits;
    CacheBudget m_convertedBudget;
    CacheBudget m_decodedBudget;
    std::once_flag m_preloadStarted;
    std::vector<std::thread> m_preloads;
    std::once_flag m_browserStarted;
    std::thread m_browserWorker;
    std::atomic<IndexState> m_browserState { IndexState::Idle };
    RetailIndex m_browserIndex;   // Written by the worker before the state turns Ready, then read only.
    std::string m_browserError;   // Written by the worker before the state turns Failed.
};

}

#endif
