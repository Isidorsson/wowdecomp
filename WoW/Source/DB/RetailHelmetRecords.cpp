#include "WoW/Source/DB/RetailHelmetRecords.h"
#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/DB/Rec/HelmetGeosetVisDataRec.h"

// Not in the original. A served retail helm names its hidden geosets by a synthetic HelmetGeosetVisData id above
// every stock one (RetailItemRecords.cpp), which this lookup turns back into the retail HelmetGeosetData rows.
// Stock rows always win.

static HelmetGeosetVisDataRec* StockRecord(const WowClientDB<HelmetGeosetVisDataRec>& db, int32_t id) {
    if (id < db.m_minID || id > db.m_maxID) {
        return nullptr;
    }
    return db.m_recordsById[id - db.m_minID];
}

#if defined(WOWDECOMP_RETAIL_ASSETS)
#include <cstring>
#include <map>
#include <mutex>
#include "Engine/Source/Retail/CascSource.h"

// Built once per id and kept for the session: callers hold the record. Map nodes never move.
static std::mutex s_servedMutex;
static std::map<int32_t, HelmetGeosetVisDataRec> s_helmetVis;

static HelmetGeosetVisDataRec* ServedRecord(int32_t id) {
    if (id <= static_cast<int32_t>(Retail::SYNTHETIC_HELMET_VIS_BASE)) {
        return nullptr;
    }
    const Retail::RetailItemDisplays* retail = Retail::CascSource::Instance().ItemDisplays();
    const Retail::HelmetMasks* masks = retail ? retail->HelmetVis(static_cast<uint32_t>(id) - Retail::SYNTHETIC_HELMET_VIS_BASE) : nullptr;
    if (!masks) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [served, added] = s_helmetVis.try_emplace(id);
    if (added) {
        HelmetGeosetVisDataRec& rec = served->second;
        // The record's constructor leaves its fields as they were, so {} would not clear what the row does not set.
        std::memset(&rec, 0, sizeof(rec));
        rec.m_id = id;
        for (size_t slot = 0; slot < Retail::HELMET_HIDE_SLOTS; slot++) {
            rec.m_hideGeoset[slot] = (*masks)[slot];
        }
    }
    return &served->second;
}

bool RetailHelmHides(int32_t visId, uint32_t race, size_t slot) {
    if (visId <= static_cast<int32_t>(Retail::SYNTHETIC_HELMET_VIS_BASE) || slot >= Retail::HELMET_HIDE_SLOTS) {
        return false;
    }
    const Retail::RetailItemDisplays* retail = Retail::CascSource::Instance().ItemDisplays();
    return retail && retail->HelmetHides(static_cast<uint32_t>(visId) - Retail::SYNTHETIC_HELMET_VIS_BASE, race)[slot];
}

#else

static HelmetGeosetVisDataRec* ServedRecord(int32_t) {
    return nullptr;
}

bool RetailHelmHides(int32_t, uint32_t, size_t) {
    return false;
}

#endif

template <>
HelmetGeosetVisDataRec* WowClientDB<HelmetGeosetVisDataRec>::GetRecord(int32_t id) const {
    HelmetGeosetVisDataRec* stock = StockRecord(*this, id);
    return stock ? stock : ServedRecord(id);
}
