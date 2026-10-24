#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"

// Not in the original. An item display id the stock ItemDisplayInfo lacks is served from the retail tables of the
// pinned CASC build, so held items, shoulders, cloaks and icons show retail gear without a downported DBC. Stock
// rows always win. The client puts a stock folder before each name (Item\ObjectComponents\Weapon\ + model), so a
// served row names files "retail-<fdid>", which CascSource reads in any folder.

static int32_t StockRecord(const WowClientDB_RLE<ItemDisplayInfoRec>& db, int32_t id, ItemDisplayInfoRec* rec) {
    if (id < db.m_minID || id > db.m_maxID) {
        return 0;
    }

    ItemDisplayInfoRec* const* slot = &db.m_recordsById[id - db.m_minID];
    if (!*slot) {
        return 0;
    }

    if (g_clientDbPerformCompress) {
        DecompressRow(reinterpret_cast<const uint8_t*>(*slot), sizeof(ItemDisplayInfoRec), reinterpret_cast<uint8_t*>(rec));
    } else {
        memcpy(rec, *slot, sizeof(ItemDisplayInfoRec));
    }
    return 1;
}

#if defined(WOWDECOMP_RETAIL_ASSETS)
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include "Engine/Source/Retail/CascSource.h"

// Built once per id and kept for the session: the copies callers get point at these strings. Map nodes never move.
struct ServedItemDisplay {
    ItemDisplayInfoRec record;
    std::string models[2];
    std::string skins[2];
    std::string icon;
};

static std::mutex s_servedMutex;
static std::map<int32_t, ServedItemDisplay> s_displays;

static const char* FileName(std::string& name, uint32_t fileDataId, const char* extension) {
    name = fileDataId ? Retail::RetailFileName(fileDataId) + extension : std::string();
    return name.c_str();
}

// Flags, group sound, spell and item visuals, particle colours and helmet geosets index retail tables or mean
// retail things, so they stay 0; the body textures are B4b's.
static void FillItemDisplay(ServedItemDisplay& served, const Retail::RetailItemDisplay& display) {
    ItemDisplayInfoRec& rec = served.record;
    // The record's constructor leaves its fields as they were, so {} would not clear what the row does not set.
    std::memset(&rec, 0, sizeof(rec));
    rec.m_id = static_cast<int32_t>(display.id);
    for (int32_t side = 0; side < 2; side++) {
        rec.m_modelName[side] = FileName(served.models[side], display.models[side], ".m2");
        rec.m_modelTexture[side] = FileName(served.skins[side], display.skins[side], "");
    }
    rec.m_inventoryIcon[0] = FileName(served.icon, display.icon, "");
    rec.m_inventoryIcon[1] = "";
    for (int32_t group = 0; group < 3; group++) {
        rec.m_geosetGroup[group] = static_cast<int32_t>(display.geosetGroups[group]);
    }
    for (const char*& texture : rec.m_texture) {
        texture = "";
    }
}

static int32_t ServedRecord(int32_t id, ItemDisplayInfoRec* rec) {
    const Retail::RetailItemDisplays* retail = Retail::CascSource::Instance().ItemDisplays();
    const Retail::RetailItemDisplay* display = retail && id > 0 ? retail->Display(static_cast<uint32_t>(id)) : nullptr;
    if (!display) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [served, added] = s_displays.try_emplace(id);
    if (added) {
        FillItemDisplay(served->second, *display);
    }
    memcpy(rec, &served->second.record, sizeof(ItemDisplayInfoRec));
    return 1;
}

#else

static int32_t ServedRecord(int32_t, ItemDisplayInfoRec*) {
    return 0;
}

#endif

template <>
int32_t WowClientDB_RLE<ItemDisplayInfoRec>::GetRecord(int32_t id, ItemDisplayInfoRec* rec) const {
    return StockRecord(*this, id, rec) || ServedRecord(id, rec);
}
