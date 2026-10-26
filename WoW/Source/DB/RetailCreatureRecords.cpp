#include "WoW/Source/DB/RetailCreatureRecords.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"

// Not in the original. A display id the stock CreatureDisplayInfo lacks is served from the retail tables of the
// pinned CASC build, so units, mounts, forms and model frames show retail displays without a downported DBC.
// Stock rows win unless RetailCasc.ini's creature_displays hands a stock id to retail (RetailDisplayChoice.h): that
// row keeps its stock sounds and takes retail's look. A served display names its model by a synthetic id above every
// stock one, which the CreatureModelData lookup turns back into the retail model.

template <class T>
static T* StockRecord(const WowClientDB<T>& db, int32_t id) {
    if (id < db.m_minID || id > db.m_maxID) {
        return nullptr;
    }
    return db.m_recordsById[id - db.m_minID];
}

#if defined(WOWDECOMP_RETAIL_ASSETS)
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/RetailDisplayChoice.h"
#include "WoW/Source/DB/RetailNpcRecords.h"

// Built once per id and kept for the session: callers hold the record and its strings. Map nodes never move.
struct ServedDisplay {
    CreatureDisplayInfoRec record;
    std::string variations[3];
    std::string fourthVariation;
    std::string portrait;
};

struct ServedModel {
    CreatureModelDataRec record;
    std::string name;
};

static std::mutex s_servedMutex;
static std::map<int32_t, ServedDisplay> s_displays;
static std::map<int32_t, ServedModel> s_models;

// A humanoid NPC (an Extra row) draws on the installed body of its race, which the Extra row skins and dresses: no
// texture variations or geosets of its own.
static void FillNpcDisplay(ServedDisplay& served, int32_t modelId, int32_t extraId) {
    CreatureDisplayInfoRec& rec = served.record;
    rec.m_modelID = modelId;
    rec.m_extendedDisplayInfoID = extraId;
    for (int32_t i = 0; i < 3; i++) {
        served.variations[i].clear();
        rec.m_textureVariation[i] = served.variations[i].c_str();
    }
    served.fourthVariation.clear();
    rec.m_creatureGeosetData = 0;
}

// Sound, blood, NPC sounds, particle colours and object effects index retail tables the client does not have, so
// they stay 0. An extended display is served by RetailNpcRecords.cpp, or the display keeps its retail model.
static void FillDisplay(ServedDisplay& served, const Retail::RetailDisplay& display) {
    CreatureDisplayInfoRec& rec = served.record;
    // The record's constructor leaves its fields as they were, so {} would not clear what the row does not set.
    std::memset(&rec, 0, sizeof(rec));
    rec.m_id = static_cast<int32_t>(display.id);
    rec.m_modelID = static_cast<int32_t>(Retail::SyntheticModelId(display.modelId));
    rec.m_creatureModelScale = display.scale;
    rec.m_creatureModelAlpha = static_cast<int32_t>(display.alpha);
    for (int32_t i = 0; i < 3; i++) {
        served.variations[i] = Retail::VariationName(display.variations[i]);
        rec.m_textureVariation[i] = served.variations[i].c_str();
    }
    served.fourthVariation = Retail::VariationName(display.fourthVariation);
    rec.m_portraitTextureName = "";
    rec.m_sizeClass = display.sizeClass;
    rec.m_creatureGeosetData = static_cast<int32_t>(display.geosetData);

    int32_t modelId = 0;
    int32_t extraId = 0;
    if (display.extendedDisplayId && ServeRetailNpcDisplay(display.id, display.extendedDisplayId, display.modelId, modelId, extraId)) {
        FillNpcDisplay(served, modelId, extraId);
    }
}

// Footprints, foley, camera shakes and sounds are retail ids too, so they stay 0; effect scales are 1. An NPC body
// (a player race model drawn with an NPC's bake) has a name of its own, which the converter takes as one.
static void FillModel(ServedModel& served, int32_t id, const Retail::RetailCreatureModel& model, bool npcBody) {
    CreatureModelDataRec& rec = served.record;
    // The record's constructor leaves its fields as they were, so {} would not clear what the row does not set.
    std::memset(&rec, 0, sizeof(rec));
    served.name = npcBody ? Retail::NpcBodyModelName(model.fileDataId) : Retail::CreatureModelName(model.fileDataId);
    rec.m_id = id;
    rec.m_modelName = served.name.c_str();
    rec.m_sizeClass = model.sizeClass;
    rec.m_modelScale = model.modelScale;
    rec.m_collisionWidth = model.collisionWidth;
    rec.m_collisionHeight = model.collisionHeight;
    rec.m_mountHeight = model.mountHeight;
    rec.m_geoBoxMinX = model.geoBox[0];
    rec.m_geoBoxMinY = model.geoBox[1];
    rec.m_geoBoxMinZ = model.geoBox[2];
    rec.m_geoBoxMaxX = model.geoBox[3];
    rec.m_geoBoxMaxY = model.geoBox[4];
    rec.m_geoBoxMaxZ = model.geoBox[5];
    rec.m_worldEffectScale = 1.0f;
    rec.m_attachedEffectScale = 1.0f;
}

static Retail::DisplayRow RowOf(const CreatureDisplayInfoRec& rec) {
    Retail::DisplayRow row;
    row.id = rec.m_id;
    row.modelId = rec.m_modelID;
    row.soundId = rec.m_soundID;
    row.extendedDisplayId = rec.m_extendedDisplayInfoID;
    row.scale = rec.m_creatureModelScale;
    row.alpha = rec.m_creatureModelAlpha;
    for (int32_t i = 0; i < 3; i++) {
        row.variations[i] = rec.m_textureVariation[i] ? rec.m_textureVariation[i] : "";
    }
    row.portrait = rec.m_portraitTextureName ? rec.m_portraitTextureName : "";
    row.sizeClass = rec.m_sizeClass;
    row.bloodId = rec.m_bloodID;
    row.npcSoundId = rec.m_npcsoundID;
    row.particleColorId = rec.m_particleColorID;
    row.geosetData = rec.m_creatureGeosetData;
    row.objectEffectPackageId = rec.m_objectEffectPackageID;
    return row;
}

static void FillRow(ServedDisplay& served, const Retail::DisplayRow& row) {
    CreatureDisplayInfoRec& rec = served.record;
    std::memset(&rec, 0, sizeof(rec));
    rec.m_id = row.id;
    rec.m_modelID = row.modelId;
    rec.m_soundID = row.soundId;
    rec.m_extendedDisplayInfoID = row.extendedDisplayId;
    rec.m_creatureModelScale = row.scale;
    rec.m_creatureModelAlpha = row.alpha;
    for (int32_t i = 0; i < 3; i++) {
        served.variations[i] = row.variations[i];
        rec.m_textureVariation[i] = served.variations[i].c_str();
    }
    served.fourthVariation = row.fourthVariation;
    served.portrait = row.portrait;
    rec.m_portraitTextureName = served.portrait.c_str();
    rec.m_sizeClass = row.sizeClass;
    rec.m_bloodID = row.bloodId;
    rec.m_npcsoundID = row.npcSoundId;
    rec.m_particleColorID = row.particleColorId;
    rec.m_creatureGeosetData = row.geosetData;
    rec.m_objectEffectPackageID = row.objectEffectPackageId;
}

static Retail::StockDisplayKind KindOf(const CreatureDisplayInfoRec& stock) {
    if (stock.m_extendedDisplayInfoID) {
        return Retail::StockDisplayKind::NpcHumanoid;
    }
    const CreatureModelDataRec* model = StockRecord(g_creatureModelDataDB, stock.m_modelID);
    const char* name = model ? model->m_modelName : nullptr;
    return name && _strnicmp(name, "Character\\", 10) == 0 ? Retail::StockDisplayKind::PlayerBody : Retail::StockDisplayKind::Creature;
}

// The retail row creature_displays hands a stock row to, null when the stock row stays.
static const Retail::RetailDisplay* ChosenRetailDisplay(const CreatureDisplayInfoRec& stock) {
    Retail::CascSource& source = Retail::CascSource::Instance();
    Retail::CreatureDisplays mode = source.CreatureDisplayMode();
    if (mode == Retail::CreatureDisplays::Stock) {
        return nullptr;
    }
    const Retail::RetailDisplays* retail = source.Displays();
    const Retail::RetailDisplay* display = retail ? retail->Display(static_cast<uint32_t>(stock.m_id)) : nullptr;
    bool wins = Retail::RetailDisplayWins(mode, static_cast<uint32_t>(stock.m_id), KindOf(stock), display);
    return wins ? display : nullptr;
}

// A stock row that creature_displays hands to retail, or the stock row itself.
static CreatureDisplayInfoRec* ChosenStockRecord(CreatureDisplayInfoRec* stock) {
    const Retail::RetailDisplay* display = ChosenRetailDisplay(*stock);
    if (!display) {
        return stock;
    }

    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [served, added] = s_displays.try_emplace(stock->m_id);
    if (added) {
        FillRow(served->second, Retail::OverlayRetailDisplay(RowOf(*stock), *display));
        int32_t modelId = 0;
        int32_t extraId = 0;
        if (display->extendedDisplayId && ServeRetailNpcDisplay(display->id, display->extendedDisplayId, display->modelId, modelId, extraId)) {
            FillNpcDisplay(served->second, modelId, extraId);
        }
    }
    return &served->second.record;
}

static CreatureDisplayInfoRec* ServedDisplayRecord(int32_t id) {
    if (id <= 0) {
        return nullptr;
    }
    const Retail::RetailDisplays* retail = Retail::CascSource::Instance().Displays();
    const Retail::RetailDisplay* display = retail ? retail->Display(static_cast<uint32_t>(id)) : nullptr;
    if (!display) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [served, added] = s_displays.try_emplace(id);
    if (added) {
        FillDisplay(served->second, *display);
    }
    return &served->second.record;
}

static CreatureModelDataRec* ServedModelRecord(int32_t id) {
    uint32_t retailId = 0;
    bool npcBody = id > 0 && Retail::RetailNpcBodyModelIdOf(static_cast<uint32_t>(id), retailId);
    if (id <= 0 || (!npcBody && !Retail::RetailModelIdOf(static_cast<uint32_t>(id), retailId))) {
        return nullptr;
    }
    const Retail::RetailDisplays* retail = Retail::CascSource::Instance().Displays();
    const Retail::RetailCreatureModel* model = retail ? retail->Model(retailId) : nullptr;
    if (!model) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [served, added] = s_models.try_emplace(id);
    if (added) {
        FillModel(served->second, id, *model, npcBody);
    }
    return &served->second.record;
}

const char* RetailFourthVariation(const CreatureDisplayInfoRec* displayRec) {
    if (!displayRec) {
        return "";
    }
    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto served = s_displays.find(displayRec->m_id);
    return served != s_displays.end() && &served->second.record == displayRec ? served->second.fourthVariation.c_str() : "";
}

bool DrawsInstalledCreatureDisplay(uint32_t id) {
    const CreatureDisplayInfoRec* stock = StockRecord(g_creatureDisplayInfoDB, static_cast<int32_t>(id));
    return stock && !ChosenRetailDisplay(*stock);
}

#else

const char* RetailFourthVariation(const CreatureDisplayInfoRec*) {
    return "";
}

bool DrawsInstalledCreatureDisplay(uint32_t id) {
    return StockRecord(g_creatureDisplayInfoDB, static_cast<int32_t>(id)) != nullptr;
}

static CreatureDisplayInfoRec* ChosenStockRecord(CreatureDisplayInfoRec* stock) {
    return stock;
}

static CreatureDisplayInfoRec* ServedDisplayRecord(int32_t) {
    return nullptr;
}

static CreatureModelDataRec* ServedModelRecord(int32_t) {
    return nullptr;
}

#endif

template <>
CreatureDisplayInfoRec* WowClientDB<CreatureDisplayInfoRec>::GetRecord(int32_t id) const {
    CreatureDisplayInfoRec* stock = StockRecord(*this, id);
    return stock ? ChosenStockRecord(stock) : ServedDisplayRecord(id);
}

template <>
CreatureModelDataRec* WowClientDB<CreatureModelDataRec>::GetRecord(int32_t id) const {
    CreatureModelDataRec* stock = StockRecord(*this, id);
    return stock ? stock : ServedModelRecord(id);
}
