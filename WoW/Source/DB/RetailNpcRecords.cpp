#include "WoW/Source/DB/RetailNpcRecords.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoExtraRec.h"

// Not in the original. Retail humanoid NPCs: a served display names a served CreatureDisplayInfoExtra row by a
// synthetic id above every stock one, built from the retail Extra row, its customization choices mapped onto the
// installed hair tables (or, on a retail body, onto the body's own geosets) and its items. Stock rows always win.

static CreatureDisplayInfoExtraRec* StockRecord(const WowClientDB<CreatureDisplayInfoExtraRec>& db, int32_t id) {
    if (id < db.m_minID || id > db.m_maxID) {
        return nullptr;
    }
    return db.m_recordsById[id - db.m_minID];
}

#if defined(WOWDECOMP_RETAIL_ASSETS)
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/RetailBodyLook.h"
#include "Engine/Source/Retail/RetailNpcLook.h"
#include "WoW/Source/DB/RetailRaceRecords.h"
#include "WoW/Source/DB/Rec/CharHairGeosetsRec.h"
#include "WoW/Source/DB/Rec/CharacterFacialHairStylesRec.h"
#include "WoW/Source/DB/Rec/ChrRacesRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"

// Built once per retail Extra id and kept for the session: callers hold the record, its bake name and its look. Map
// nodes never move.
struct ServedExtra {
    bool served = false;
    CreatureDisplayInfoExtraRec record;
    std::string bakeName;
    int32_t bodyModelId = 0;
    bool retailBody = false;
    std::vector<uint32_t> geosets;   // A retail body's meshes, one per geoset group.
    std::vector<std::string> texturePaths;
    std::vector<RetailBodyTextureView> textures;   // Naming texturePaths.
};

static std::mutex s_servedMutex;
static std::map<uint32_t, ServedExtra> s_extras;
static std::mutex s_logMutex;
static std::set<std::string> s_logged;

// Each line once a session: many NPCs share a missing hair style.
static void LogOnce(const std::string& line) {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (s_logged.insert(line).second) {
        Retail::CascSource::Instance().Warn(line);
    }
}

static Retail::InstalledLook InstalledLook() {
    Retail::InstalledLook look;
    for (int32_t i = 0; i < g_charHairGeosetsDB.m_numRecords; i++) {
        const CharHairGeosetsRec& rec = g_charHairGeosetsDB.m_records[i];
        look.hairStyles.push_back({ uint32_t(rec.m_raceID), uint32_t(rec.m_sexID), uint32_t(rec.m_variationID), uint32_t(rec.m_geosetID) });
    }
    for (int32_t i = 0; i < g_characterFacialHairStylesDB.m_numRecords; i++) {
        const CharacterFacialHairStylesRec& rec = g_characterFacialHairStylesDB.m_records[i];
        look.facialHair.push_back({ uint32_t(rec.m_raceID), uint32_t(rec.m_sexID), uint32_t(rec.m_variationID),
            { uint32_t(rec.m_geoset[0]), uint32_t(rec.m_geoset[1]), uint32_t(rec.m_geoset[2]) } });
    }
    return look;
}

static bool ItemServable(uint32_t displayId) {
    ItemDisplayInfoRec rec;
    return g_itemDisplayInfoDB.GetRecord(static_cast<int32_t>(displayId), &rec) != 0;
}

// A stock helm names files by the race's installed prefix, which only installed races have; a served one is the file
// RetailItemDisplays::RaceFile chooses for the race.
static bool HelmWearable(uint32_t displayId, uint32_t race, uint32_t sex) {
    const WowClientDB_RLE<ItemDisplayInfoRec>& stock = g_itemDisplayInfoDB;
    int32_t id = static_cast<int32_t>(displayId);
    if (id >= stock.m_minID && id <= stock.m_maxID && stock.m_recordsById[id - stock.m_minID]) {
        return race <= static_cast<uint32_t>(g_chrRacesDB.m_maxID);
    }
    const Retail::RetailItemDisplays* retail = Retail::CascSource::Instance().ItemDisplays();
    const Retail::RetailItemDisplay* display = retail ? retail->Display(displayId) : nullptr;
    return display && display->models[0] && retail->RaceFile(display->models[0], race, sex);
}

static void FillExtra(ServedExtra& served, const Retail::NpcExtraFields& fields) {
    CreatureDisplayInfoExtraRec& rec = served.record;
    // The record's constructor leaves its fields as they were, so {} would not clear what the row does not set.
    std::memset(&rec, 0, sizeof(rec));
    served.bakeName = fields.bakeName;
    rec.m_id = fields.id;
    rec.m_displayRaceID = fields.race;
    rec.m_displaySexID = fields.sex;
    rec.m_skinID = fields.skin;
    rec.m_faceID = fields.face;
    rec.m_hairStyleID = fields.hairStyle;
    rec.m_hairColorID = fields.hairColor;
    rec.m_facialHairID = fields.facialHair;
    for (size_t slot = 0; slot < Retail::NPC_ITEM_SLOTS; slot++) {
        rec.m_npcitemDisplay[slot] = fields.items[slot];
    }
    rec.m_bakeName = served.bakeName.c_str();
}

// The served row's race needs a ChrRaces row (installed or served) and a place in the race-sized component tables.
static bool RaceServable(uint32_t race, uint32_t sex, std::string& reason) {
    if (!Retail::RaceFitsTables(race, sex, ChrRaceCapacity())) {
        reason = "race " + std::to_string(race) + " is past the race tables (" + std::to_string(ChrRaceCapacity()) + ")";
        return false;
    }
    if (!g_chrRacesDB.GetRecord(static_cast<int32_t>(race))) {
        reason = "race " + std::to_string(race) + " has no ChrRaces row";
        return false;
    }
    return true;
}

// A retail body draws the row's geosets and its chosen textures itself, placed by its race's texture layout
// (RetailBodyLook.h).
static void FillRetailBodyLook(ServedExtra& served, const Retail::NpcExtra& extra, const Retail::NpcBody& body) {
    Retail::CascSource& source = Retail::CascSource::Instance();
    served.retailBody = true;
    served.geosets = Retail::RetailBodyGeosets(extra.look, source.ModelGeosets(body.fileDataId));
    const Retail::RetailRaces* races = source.Races();
    const Retail::RetailRace* race = races ? races->Race(extra.race) : nullptr;
    if (!race || extra.sex > 1) {
        return;
    }
    std::vector<Retail::LookTexture> textures = Retail::RetailBodyTextures(extra.look, races->TextureLayers(race->layouts[extra.sex]));
    served.texturePaths.reserve(textures.size());
    for (const Retail::LookTexture& texture : textures) {
        served.texturePaths.push_back("Character\\" + Retail::RetailFileName(texture.file) + ".blp");
        served.textures.push_back({ texture.type, served.texturePaths.back().c_str() });
    }
}

// The served row for a retail Extra id, or false with the reason the display keeps its retail model.
static bool BuildServedExtra(uint32_t retailExtraId, const Retail::NpcBody& body, ServedExtra& served, std::string& reason) {
    Retail::CascSource& source = Retail::CascSource::Instance();
    const Retail::RetailNpcTables* tables = source.NpcTables();
    const Retail::NpcExtra* extra = tables ? tables->Extra(retailExtraId) : nullptr;
    if (!extra) {
        reason = tables ? "extra " + std::to_string(retailExtraId) + " is not in the retail tables" : "no NPC tables";
        return false;
    }

    Retail::NpcPlan plan;
    if (!Retail::PlanNpc(*extra, body, [&source](uint32_t race, uint32_t sex) { return source.InstalledBodyFits(race, sex); }, plan, reason)
        || !RaceServable(plan.race, extra->sex, reason)) {
        return false;
    }
    Retail::NpcAppearance appearance = plan.retailBody ? Retail::NpcAppearance() : Retail::MapNpcLook(plan.race, extra->sex, extra->look, InstalledLook());
    std::vector<std::string> notes = appearance.notes;
    Retail::NpcExtraFields fields = Retail::ServedNpcExtra(*extra, plan, appearance, ItemServable, notes);
    if (plan.retailBody) {
        Retail::DropUnwearableHelm(fields, [&plan, extra](uint32_t display) { return HelmWearable(display, plan.race, extra->sex); }, notes);
    }
    FillExtra(served, fields);
    served.bodyModelId = plan.bodyModelId;
    if (plan.retailBody) {
        FillRetailBodyLook(served, *extra, body);
    }
    for (const std::string& note : notes) {
        LogOnce(note);
    }
    return true;
}

// Built on a display's first use (body set); a lookup by the Extra id alone (body null) finds only a built row.
static ServedExtra* ServedExtraFor(uint32_t retailExtraId, const Retail::NpcBody* body, std::string& reason) {
    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto found = s_extras.find(retailExtraId);
    if (found == s_extras.end() && !body) {
        reason = "not served by a display yet";
        return nullptr;
    }
    if (found == s_extras.end()) {
        found = s_extras.try_emplace(retailExtraId).first;
        found->second.served = BuildServedExtra(retailExtraId, *body, found->second, reason);
    } else if (!found->second.served) {
        reason = "refused before";
    }
    return found->second.served ? &found->second : nullptr;
}

static Retail::NpcBody BodyOf(uint32_t retailModelId) {
    const Retail::RetailDisplays* displays = Retail::CascSource::Instance().Displays();
    const Retail::RetailCreatureModel* model = displays ? displays->Model(retailModelId) : nullptr;
    return { retailModelId, model ? model->fileDataId : 0 };
}

bool ServeRetailNpcDisplay(uint32_t displayId, uint32_t retailExtraId, uint32_t retailModelId, int32_t& modelId, int32_t& extraId) {
    std::string reason;
    Retail::NpcBody body = BodyOf(retailModelId);
    ServedExtra* served = ServedExtraFor(retailExtraId, &body, reason);
    if (!served) {
        LogOnce("display " + std::to_string(displayId) + " (extra " + std::to_string(retailExtraId) + ") keeps its retail model: " + reason);
        return false;
    }
    modelId = served->bodyModelId;
    extraId = served->record.m_id;
    return true;
}

static CreatureDisplayInfoExtraRec* ServedRecord(int32_t id) {
    uint32_t retailId = 0;
    if (id <= 0 || !Retail::RetailExtraIdOf(static_cast<uint32_t>(id), retailId)) {
        return nullptr;
    }
    std::string reason;
    ServedExtra* served = ServedExtraFor(retailId, nullptr, reason);
    return served ? &served->record : nullptr;
}

bool FindRetailBodyLook(const char* npcSkinTexture, RetailBodyLookView& look) {
    uint32_t bake = 0;
    uint32_t retailId = 0;
    if (!npcSkinTexture || !Retail::ParseNpcBakeName(Retail::NormalizePath(npcSkinTexture), bake, retailId)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto found = s_extras.find(retailId);
    if (found == s_extras.end() || !found->second.served || !found->second.retailBody) {
        return false;
    }
    look.geosets = found->second.geosets.data();
    look.count = found->second.geosets.size();
    look.textures = found->second.textures.data();
    look.textureCount = found->second.textures.size();
    return true;
}

#else

bool ServeRetailNpcDisplay(uint32_t, uint32_t, uint32_t, int32_t&, int32_t&) {
    return false;
}

bool FindRetailBodyLook(const char*, RetailBodyLookView&) {
    return false;
}

static CreatureDisplayInfoExtraRec* ServedRecord(int32_t) {
    return nullptr;
}

#endif

template <>
CreatureDisplayInfoExtraRec* WowClientDB<CreatureDisplayInfoExtraRec>::GetRecord(int32_t id) const {
    CreatureDisplayInfoExtraRec* stock = StockRecord(*this, id);
    return stock ? stock : ServedRecord(id);
}
