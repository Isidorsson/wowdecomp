#include "Engine/Source/Retail/CascSource.h"
#include <algorithm>
#include <cstring>
#include <set>
#include "Engine/Source/Retail/BodyMatch.h"
#include "Engine/Source/Retail/HelmFit.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/RetailModelData.h"

// The seed half of CascSource: the decoded originals the client reads to draw a list of displays, written as a
// seed folder (DecodedCache.h SeedEntryPath).

namespace Retail {

// Every table the display loaders read: ReadDisplayTables, LoadNpcTables, LoadRaceTables and Fallbacks.
static const uint32_t DISPLAY_TABLES[] = {
    CREATURE_MODEL_DATA_FDID, CREATURE_DISPLAY_INFO_FDID, CREATURE_DISPLAY_INFO_GEOSET_DATA_FDID,
    MODEL_FILE_DATA_FDID, ITEM_DISPLAY_INFO_FDID, ITEM_DISPLAY_INFO_MODEL_MAT_RES_FDID, TEXTURE_FILE_DATA_FDID, COMPONENT_MODEL_FILE_DATA_FDID,
    ITEM_APPEARANCE_FDID, ITEM_DISPLAY_INFO_MATERIAL_RES_FDID, COMPONENT_TEXTURE_FILE_DATA_FDID, HELMET_GEOSET_DATA_FDID,
    CREATURE_DISPLAY_INFO_EXTRA_FDID, NPC_MODEL_ITEM_SLOT_DISPLAY_INFO_FDID, CREATURE_DISPLAY_INFO_OPTION_FDID, CHR_CUSTOMIZATION_CHOICE_FDID,
    CHR_CUSTOMIZATION_ELEMENT_FDID, CHR_CUSTOMIZATION_GEOSET_FDID, CHR_CUSTOMIZATION_MATERIAL_FDID,
    CHR_RACES_FDID, CHR_RACE_X_CHR_MODEL_FDID, CHR_MODEL_FDID, CHR_MODEL_TEXTURE_LAYER_FDID, ANIMATION_DATA_FDID,
};

constexpr size_t NPC_HELM_SLOT = 0;   // The first of NPC_ITEM_SLOTS.
constexpr uint32_t LAST_STOCK_RACE = 11;   // ChrRaces ids 1 to 11 are the races the client's own tables hold.

// One export. A file is written once however many displays name it; a display that fails stops at its first failure,
// leaving the files it already wrote.
class CascSource::SeedJob {
    public:
    SeedJob(CascSource& source, const std::filesystem::path& folder);

    SeedReport Run(const std::vector<uint32_t>& creatureDisplayIds, const std::vector<uint32_t>& itemDisplayIds);

    private:
    // Who wears an item: the model of a helm is the race and gender's own file.
    struct Wearer {
        bool helm;
        uint32_t race;
        uint32_t sex;
    };

    bool Table(uint32_t fileDataId);
    bool CreatureDisplay(uint32_t id);
    bool ItemDisplay(uint32_t id);
    bool Npc(const RetailDisplay& display, const RetailCreatureModel& model);
    bool NpcBody(const NpcExtra& extra, const NpcPlan& plan, const RetailCreatureModel& model);
    bool BodyWithSkins(uint32_t fileDataId);
    bool NpcItems(const NpcExtra& extra, const NpcPlan& plan);
    bool WornItem(const RetailItemDisplays& items, const RetailItemDisplay& item, const Wearer& wearer);
    bool HelmFitBody(uint32_t race, uint32_t sex);
    bool Model(uint32_t fileDataId);
    bool PlayerBodyModel(uint32_t fileDataId);
    bool Walk(uint32_t fileDataId, const ModelSources& given);
    bool ModelFiles(const std::vector<uint8_t>& file, uint32_t fileDataId, const ModelSources& given);
    bool ConversionReads(const std::vector<uint8_t>& file, ModelSources sources);
    bool File(uint32_t fileDataId);
    bool Fetch(uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error);
    bool Store(uint32_t fileDataId, const std::vector<uint8_t>& bytes, std::string& error);
    bool Fail(const std::string& message);
    bool Refuse(const std::string& display, const std::string& reason);

    CascSource& m_source;
    Config m_target;   // The source's configuration with the seed folder replaced.
    std::set<uint32_t> m_written;
    std::set<std::pair<uint32_t, bool>> m_walked;   // Models by file and whether as a player body.
    SeedReport m_report;
};

CascSource::SeedJob::SeedJob(CascSource& source, const std::filesystem::path& folder) : m_source(source), m_target(source.m_config) {
    this->m_target.seed = folder;
}

SeedReport CascSource::SeedJob::Run(const std::vector<uint32_t>& creatureDisplayIds, const std::vector<uint32_t>& itemDisplayIds) {
    for (uint32_t table : DISPLAY_TABLES) {
        this->Table(table);
    }
    for (uint32_t id : creatureDisplayIds) {
        if (!this->CreatureDisplay(id)) {
            this->m_report.skipped++;
        }
    }
    for (uint32_t id : itemDisplayIds) {
        if (!this->ItemDisplay(id)) {
            this->m_report.skipped++;
        }
    }
    return this->m_report;
}

bool CascSource::SeedJob::Table(uint32_t fileDataId) {
    std::vector<uint8_t> bytes;
    std::string error;
    return (this->m_source.ReadTable(fileDataId, bytes, error) && this->Store(fileDataId, bytes, error)) || this->Fail(error);
}

bool CascSource::SeedJob::CreatureDisplay(uint32_t id) {
    const RetailDisplays* displays = this->m_source.Displays();
    const RetailDisplay* display = displays ? displays->Display(id) : nullptr;
    const RetailCreatureModel* model = display ? displays->Model(display->modelId) : nullptr;
    if (!model) {
        return this->Refuse("creature display " + std::to_string(id), displays ? "retail has no such display" : "the creature tables did not load");
    }

    if (display->extendedDisplayId) {
        return this->Npc(*display, *model);
    }
    if (!this->Model(model->fileDataId)) {
        return false;
    }
    for (uint32_t texture : display->variations) {
        if (!this->File(texture)) {
            return false;
        }
    }
    return this->File(display->fourthVariation);
}

// A humanoid NPC as the client plans it: on the installed body of its race with the Extra row's bake, or on the retail
// body when it has no installed one. A display the client plans no body for keeps its retail model, which it cannot draw.
bool CascSource::SeedJob::Npc(const RetailDisplay& display, const RetailCreatureModel& model) {
    const RetailNpcTables* tables = this->m_source.NpcTables();
    const NpcExtra* extra = tables ? tables->Extra(display.extendedDisplayId) : nullptr;
    std::string name = "creature display " + std::to_string(display.id);
    if (!extra) {
        return this->Refuse(name, tables ? "retail has no Extra row " + std::to_string(display.extendedDisplayId) : "the NPC tables did not load");
    }

    NpcPlan plan;
    std::string reason;
    if (!PlanNpc(*extra, { display.modelId, model.fileDataId }, [this](uint32_t race, uint32_t sex) { return this->m_source.BodyFor(race, sex).fits; }, plan, reason)) {
        return this->Refuse(name, reason);
    }
    return this->File(plan.bakeFile) && this->NpcBody(*extra, plan, model) && this->NpcItems(*extra, plan);
}

// A warped bake is matched against the retail HD body of the race. A retail body is converted as an NPC body and
// draws the textures the Extra row's customization chose.
bool CascSource::SeedJob::NpcBody(const NpcExtra& extra, const NpcPlan& plan, const RetailCreatureModel& model) {
    if (plan.bake == BakeUse::Warp) {
        return this->BodyWithSkins(RetailHdBodyFileDataId(plan.race, extra.sex));
    }
    if (!plan.retailBody || !this->PlayerBodyModel(model.fileDataId)) {
        return !plan.retailBody;
    }
    for (const ChosenMaterial& material : extra.look.materials) {
        if (!this->File(material.file)) {
            return false;
        }
    }
    return true;
}

// A body the client reads whole and never converts: the HD body a bake is matched against, with every skin it lists.
bool CascSource::SeedJob::BodyWithSkins(uint32_t fileDataId) {
    std::vector<uint8_t> body;
    std::string error;
    if (!this->Fetch(fileDataId, body, error)) {
        return this->Fail(error);
    }
    for (uint32_t skin : U32s(ReadChunks(body)["SFID"])) {
        if (!this->File(skin)) {
            return false;
        }
    }
    return true;
}

// Whether the client may ask for the display's models by race: a helmet vis, or a model resource whose file differs by
// race or gender.
static bool IsHelm(const RetailItemDisplays& items, const RetailItemDisplay& item) {
    for (uint32_t model : item.models) {
        for (uint32_t race = 1; race <= LAST_STOCK_RACE && model; race++) {
            for (uint32_t sex = 0; sex <= 1; sex++) {
                if (items.RaceFile(model, race, sex) != items.RaceFile(model, 1, 0)) {
                    return true;
                }
            }
        }
    }
    return item.helmetVis[0] || item.helmetVis[1];
}

// An item display as any stock race and gender wears it, with its icon. A display knows no inventory type, so its
// models are asked for as the display names them, and by race too when they may be a helm's.
bool CascSource::SeedJob::ItemDisplay(uint32_t id) {
    const RetailItemDisplays* items = this->m_source.ItemDisplays();
    const RetailItemDisplay* item = items ? items->Display(id) : nullptr;
    if (!item) {
        return this->Refuse("item display " + std::to_string(id), items ? "retail has no such display" : "the item tables did not load");
    }

    for (uint32_t model : item->models) {
        if (!this->Model(model)) {
            return false;
        }
    }
    bool helm = IsHelm(*items, *item);
    for (uint32_t race = 1; race <= LAST_STOCK_RACE; race++) {
        for (uint32_t sex = 0; sex <= 1; sex++) {
            if (!this->WornItem(*items, *item, { helm, race, sex })) {
                return false;
            }
        }
    }
    return this->File(item->icon);
}

// Each worn item as the client draws it on the NPC.
bool CascSource::SeedJob::NpcItems(const NpcExtra& extra, const NpcPlan& plan) {
    const RetailItemDisplays* items = this->m_source.ItemDisplays();
    if (!items) {
        bool dressed = std::any_of(extra.items.begin(), extra.items.end(), [](uint32_t display) { return display != 0; });
        return !dressed || this->Refuse("Extra row " + std::to_string(extra.id), "the item tables did not load");
    }

    for (size_t slot = 0; slot < NPC_ITEM_SLOTS; slot++) {
        const RetailItemDisplay* item = extra.items[slot] ? items->Display(extra.items[slot]) : nullptr;
        if (item && !this->WornItem(*items, *item, { slot == NPC_HELM_SLOT, plan.race, extra.sex })) {
            return false;
        }
    }
    return true;
}

// The files of an item display for one wearer: the helm as its race's file, the body textures of its gender.
bool CascSource::SeedJob::WornItem(const RetailItemDisplays& items, const RetailItemDisplay& item, const Wearer& wearer) {
    for (size_t side = 0; side < item.models.size(); side++) {
        uint32_t file = item.models[side];
        uint32_t raceFile = file && wearer.helm ? items.RaceFile(file, wearer.race, wearer.sex) : 0;
        if (!this->Model(raceFile ? raceFile : file) || !this->File(item.skins[side])) {
            return false;
        }
    }
    for (const BodyTexture& texture : item.body) {
        if (!this->File(wearer.sex ? texture.female : texture.male)) {
            return false;
        }
    }
    return !wearer.helm || this->HelmFitBody(wearer.race, wearer.sex);
}

// The retail body the client reads the stock head's helm frame against, for the races whose helms are moved to fit it.
bool CascSource::SeedJob::HelmFitBody(uint32_t race, uint32_t sex) {
    return HelmFitsUnmoved(race, sex) || this->File(RetailBodyFileDataId(race, sex));
}

bool CascSource::SeedJob::Model(uint32_t fileDataId) {
    return this->Walk(fileDataId, ModelSources());
}

// A player race model asked for as an NPC body.
bool CascSource::SeedJob::PlayerBodyModel(uint32_t fileDataId) {
    ModelSources sources;
    sources.playerBody = true;
    return this->Walk(fileDataId, sources);
}

// A model with what its conversion reads. A file that is no retail model passes through unconverted, so it has none.
bool CascSource::SeedJob::Walk(uint32_t fileDataId, const ModelSources& given) {
    if (!fileDataId || !this->m_walked.insert({ fileDataId, given.playerBody }).second) {
        return true;
    }

    std::vector<uint8_t> file;
    std::string error;
    if (!this->Fetch(fileDataId, file, error)) {
        return this->Fail(error);
    }
    return file.size() < 4 || std::memcmp(file.data(), "MD21", 4) != 0 || this->ModelFiles(file, fileDataId, given);
}

// The skins and textures the model lists, the textures its default skin names, and the files the conversion reads
// (animations and skeletons), which it reports by asking for them.
bool CascSource::SeedJob::ModelFiles(const std::vector<uint8_t>& file, uint32_t fileDataId, const ModelSources& given) {
    std::map<std::string, std::vector<uint8_t>> chunks = ReadChunks(file);
    for (const char* tag : { "SFID", "TXID" }) {
        for (uint32_t listed : U32s(chunks[tag])) {
            if (!this->File(listed)) {
                return false;
            }
        }
    }

    const ModelSkins* skins = this->m_source.Skins();
    ModelSources sources = given;
    sources.fallbacks = this->m_source.Fallbacks();
    sources.skin = skins ? skins->Default(fileDataId) : nullptr;
    for (const SkinTexture& texture : sources.skin ? sources.skin->textures : std::vector<SkinTexture>()) {
        if (!this->File(texture.fileDataId)) {
            return false;
        }
    }
    return this->ConversionReads(file, sources);
}

bool CascSource::SeedJob::ConversionReads(const std::vector<uint8_t>& file, ModelSources sources) {
    bool readFailed = false;
    sources.read = [this, &readFailed](uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error) {
        bool read = this->Fetch(fileDataId, bytes, error);
        if (!read) {
            readFailed = true;
            this->Fail(error);
        }
        return read;
    };

    ConvertedModel converted;
    std::string error;
    return ConvertModel(file, sources, converted, error) ? !readFailed : this->Fail(error);
}

// An original nothing else of the export reads.
bool CascSource::SeedJob::File(uint32_t fileDataId) {
    std::vector<uint8_t> bytes;
    std::string error;
    return !fileDataId || this->m_written.count(fileDataId) || this->Fetch(fileDataId, bytes, error) || this->Fail(error);
}

bool CascSource::SeedJob::Fetch(uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error) {
    return this->m_source.ReadOriginal(fileDataId, bytes, error) && this->Store(fileDataId, bytes, error);
}

bool CascSource::SeedJob::Store(uint32_t fileDataId, const std::vector<uint8_t>& bytes, std::string& error) {
    if (!this->m_written.insert(fileDataId).second) {
        return true;
    }
    if (!WriteDecodedCache(SeedEntryPath(this->m_target, fileDataId), bytes, error)) {
        this->m_written.erase(fileDataId);
        return false;
    }
    this->m_report.files++;
    return true;
}

bool CascSource::SeedJob::Fail(const std::string& message) {
    if (this->m_report.error.empty()) {
        this->m_report.error = message;
    }
    return false;
}

bool CascSource::SeedJob::Refuse(const std::string& display, const std::string& reason) {
    this->m_source.Warn("seed: " + display + " left out, " + reason);
    return false;
}

SeedReport CascSource::ExportSeed(const std::vector<uint32_t>& creatureDisplayIds, const std::vector<uint32_t>& itemDisplayIds, const std::filesystem::path& folder) {
    if (!this->m_configured) {
        SeedReport report;
        report.error = this->m_configError.empty() ? std::string("retail assets are not configured") : this->m_configError;
        return report;
    }

    SeedJob job(*this, folder);
    return job.Run(creatureDisplayIds, itemDisplayIds);
}

}
