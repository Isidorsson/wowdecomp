#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"

// Not in the original. A display id the stock CreatureDisplayInfo lacks is served from the retail tables of the
// pinned CASC build, so units, mounts, forms and model frames show retail displays without a downported DBC.
// Stock rows always win. A served display names its model by a synthetic id above every stock one, which the
// CreatureModelData lookup turns back into the retail model.

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
#include "WoW/Source/DB/RetailNpcRecords.h"

// Built once per id and kept for the session: callers hold the record and its strings. Map nodes never move.
struct ServedDisplay {
    CreatureDisplayInfoRec record;
    std::string variations[3];
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
    rec.m_portraitTextureName = "";
    rec.m_sizeClass = display.sizeClass;
    rec.m_creatureGeosetData = static_cast<int32_t>(display.geosetData);

    int32_t modelId = 0;
    int32_t extraId = 0;
    if (display.extendedDisplayId && ServeRetailNpcDisplay(display.id, display.extendedDisplayId, modelId, extraId)) {
        FillNpcDisplay(served, modelId, extraId);
    }
}

// Footprints, foley, camera shakes and sounds are retail ids too, so they stay 0; effect scales are 1.
static void FillModel(ServedModel& served, int32_t id, const Retail::RetailCreatureModel& model) {
    CreatureModelDataRec& rec = served.record;
    // The record's constructor leaves its fields as they were, so {} would not clear what the row does not set.
    std::memset(&rec, 0, sizeof(rec));
    served.name = Retail::CreatureModelName(model.fileDataId);
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
    if (id <= 0 || !Retail::RetailModelIdOf(static_cast<uint32_t>(id), retailId)) {
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
        FillModel(served->second, id, *model);
    }
    return &served->second.record;
}

#else

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
    return stock ? stock : ServedDisplayRecord(id);
}

template <>
CreatureModelDataRec* WowClientDB<CreatureModelDataRec>::GetRecord(int32_t id) const {
    CreatureModelDataRec* stock = StockRecord(*this, id);
    return stock ? stock : ServedModelRecord(id);
}
