#include "WoW/Source/DB/RetailNpcRecords.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoExtraRec.h"

// Not in the original. Retail humanoid NPCs: a served display names a served CreatureDisplayInfoExtra row by a
// synthetic id above every stock one, built from the retail Extra row, its customization choices mapped onto the
// installed hair tables and its items. Stock rows always win.

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
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/RetailNpcLook.h"
#include "WoW/Source/DB/Rec/CharHairGeosetsRec.h"
#include "WoW/Source/DB/Rec/CharacterFacialHairStylesRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"

// Built once per retail Extra id and kept for the session: callers hold the record and its bake name. Map nodes never move.
struct ServedExtra {
    bool served = false;
    CreatureDisplayInfoExtraRec record;
    std::string bakeName;
    int32_t bodyModelId = 0;
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

// The served row for a retail Extra id, or false with the reason the display keeps its retail model.
static bool BuildServedExtra(uint32_t retailExtraId, ServedExtra& served, std::string& reason) {
    Retail::CascSource& source = Retail::CascSource::Instance();
    const Retail::RetailNpcTables* tables = source.NpcTables();
    const Retail::NpcExtra* extra = tables ? tables->Extra(retailExtraId) : nullptr;
    if (!extra) {
        reason = tables ? "extra " + std::to_string(retailExtraId) + " is not in the retail tables" : "no NPC tables";
        return false;
    }

    Retail::NpcPlan plan;
    if (!Retail::BuildNpcPlan(*extra, [&source](uint32_t race, uint32_t sex) { return source.InstalledBodyFits(race, sex); }, plan, reason)) {
        return false;
    }
    Retail::NpcAppearance appearance = Retail::MapNpcLook(extra->race, extra->sex, extra->look, InstalledLook());
    std::vector<std::string> notes = appearance.notes;
    FillExtra(served, Retail::ServedNpcExtra(*extra, plan, appearance, ItemServable, notes));
    served.bodyModelId = plan.bodyModelId;
    for (const std::string& note : notes) {
        LogOnce(note);
    }
    return true;
}

static ServedExtra* ServedExtraFor(uint32_t retailExtraId, std::string& reason) {
    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [served, added] = s_extras.try_emplace(retailExtraId);
    if (added) {
        served->second.served = BuildServedExtra(retailExtraId, served->second, reason);
    } else if (!served->second.served) {
        reason = "refused before";
    }
    return served->second.served ? &served->second : nullptr;
}

bool ServeRetailNpcDisplay(uint32_t displayId, uint32_t retailExtraId, int32_t& modelId, int32_t& extraId) {
    std::string reason;
    ServedExtra* served = ServedExtraFor(retailExtraId, reason);
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
    ServedExtra* served = ServedExtraFor(retailId, reason);
    return served ? &served->record : nullptr;
}

#else

bool ServeRetailNpcDisplay(uint32_t, uint32_t, int32_t&, int32_t&) {
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
