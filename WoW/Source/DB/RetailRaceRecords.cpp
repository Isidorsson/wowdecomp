#include "WoW/Source/DB/RetailRaceRecords.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ChrRacesRec.h"

// Not in the original. Serves, after the installed ChrRaces rows, the retail races (Worgen, Pandaren, the allied
// races, Earthen, ...) whose NPCs draw on their retail bodies. Installed rows always win.

static ChrRacesRec* StockRecord(const WowClientDB<ChrRacesRec>& db, int32_t id) {
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

// Built once per id and kept for the session: callers hold the record and its strings. Map nodes never move.
struct ServedRaceRecord {
    bool served = false;
    ChrRacesRec record;
    std::string prefix;
    std::string name;
};

static std::mutex s_servedMutex;
static std::map<int32_t, ServedRaceRecord> s_races;

static void FillRace(ServedRaceRecord& served, const Retail::ServedRace& race) {
    ChrRacesRec& rec = served.record;
    // The record's constructor leaves its fields as they were, so {} would not clear what the row does not set.
    std::memset(&rec, 0, sizeof(rec));
    served.prefix = race.prefix;
    served.name = race.name;
    rec.m_id = static_cast<int32_t>(race.id);
    rec.m_flags = race.flags;
    rec.m_maleDisplayID = static_cast<int32_t>(race.displays[0]);
    rec.m_femaleDisplayID = static_cast<int32_t>(race.displays[1]);
    rec.m_clientPrefix = served.prefix.c_str();
    rec.m_clientFileString = served.name.c_str();
    rec.m_name = served.name.c_str();
    rec.m_nameFemale = served.name.c_str();
    rec.m_nameMale = served.name.c_str();
    rec.m_facialHairCustomization[0] = "";
    rec.m_facialHairCustomization[1] = "";
    rec.m_hairCustomization = "";
}

static ChrRacesRec* ServedRecord(const WowClientDB<ChrRacesRec>& db, int32_t id) {
    const Retail::RetailRaces* races = id > db.m_maxID ? Retail::CascSource::Instance().Races() : nullptr;
    const Retail::RetailRace* race = races ? races->Race(static_cast<uint32_t>(id)) : nullptr;
    if (!race) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [served, added] = s_races.try_emplace(id);
    Retail::ServedRace row;
    if (added && Retail::ServeRace(*race, static_cast<uint32_t>(db.m_maxID), row)) {
        FillRace(served->second, row);
        served->second.served = true;
    }
    return served->second.served ? &served->second.record : nullptr;
}

uint32_t ChrRaceCapacity() {
    const Retail::RetailRaces* races = Retail::CascSource::Instance().Races();
    return Retail::RaceCapacity(static_cast<uint32_t>(g_chrRacesDB.m_maxID), races ? races->MaxId() : 0);
}

bool ChrRaceNeedsCharSections(uint32_t race, bool baked) {
    return Retail::NeedsCharSections(StockRecord(g_chrRacesDB, static_cast<int32_t>(race)) != nullptr, baked);
}

void LogChrRaceRefused(uint32_t race, uint32_t sex) {
    static std::mutex s_logMutex;
    static std::set<uint32_t> s_logged;
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (s_logged.insert(race * 2 + sex).second) {
        Retail::CascSource::Instance().Warn("race " + std::to_string(race) + " sex " + std::to_string(sex) + " is past the race tables ("
            + std::to_string(ChrRaceCapacity()) + "), so it is drawn without its character component");
    }
}

#else

static ChrRacesRec* ServedRecord(const WowClientDB<ChrRacesRec>&, int32_t) {
    return nullptr;
}

uint32_t ChrRaceCapacity() {
    return static_cast<uint32_t>(g_chrRacesDB.m_maxID);
}

bool ChrRaceNeedsCharSections(uint32_t, bool) {
    return true;
}

void LogChrRaceRefused(uint32_t, uint32_t) {
}

#endif

template <>
ChrRacesRec* WowClientDB<ChrRacesRec>::GetRecord(int32_t id) const {
    ChrRacesRec* stock = StockRecord(*this, id);
    return stock ? stock : ServedRecord(*this, id);
}
