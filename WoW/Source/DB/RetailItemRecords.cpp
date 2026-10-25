#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"

// Not in the original. An item display id the stock ItemDisplayInfo lacks is served from the retail tables of the
// pinned CASC build, so held items, shoulders, cloaks and icons show retail gear without a downported DBC. Stock
// rows always win, save an empty icon. The client puts a stock folder before each name (Item\ObjectComponents\Weapon\ + model), so a
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
#include <climits>
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
    std::string body[Retail::BODY_SECTIONS];
};

static std::mutex s_servedMutex;
static std::map<int32_t, ServedItemDisplay> s_displays;

static const char* FileName(std::string& name, uint32_t fileDataId, const char* extension) {
    name = fileDataId ? Retail::RetailFileName(fileDataId) + extension : std::string();
    return name.c_str();
}

// The served HelmetGeosetVisData id of a retail vis (RetailHelmetRecords.cpp); 0, which hides nothing, for a vis
// HelmetGeosetData has no rows for.
static int32_t ServedHelmetVis(const Retail::RetailItemDisplays& retail, uint32_t vis) {
    bool fits = vis <= static_cast<uint32_t>(INT32_MAX) - Retail::SYNTHETIC_HELMET_VIS_BASE;
    return fits && retail.HelmetVis(vis) ? static_cast<int32_t>(Retail::SYNTHETIC_HELMET_VIS_BASE + vis) : 0;
}

// Flags, group sound, spell and item visuals and particle colours index retail tables or mean retail things, so
// they stay 0. A body section names both genders' files, as the client adds _U, _M or _F.
static void FillItemDisplay(ServedItemDisplay& served, const Retail::RetailItemDisplays& retail, const Retail::RetailItemDisplay& display) {
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
    for (size_t section = 0; section < Retail::BODY_SECTIONS; section++) {
        const Retail::BodyTexture& body = display.body[section];
        served.body[section] = body.male || body.female ? Retail::RetailBodyTextureName(body.male, body.female) : std::string();
        rec.m_texture[section] = served.body[section].c_str();
    }
    for (size_t sex = 0; sex < display.helmetVis.size(); sex++) {
        rec.m_helmetGeosetVisID[sex] = ServedHelmetVis(retail, display.helmetVis[sex]);
    }
}

static int32_t ServedRecord(int32_t id, ItemDisplayInfoRec* rec) {
    if (id <= 0) {
        return 0;
    }
    const Retail::RetailItemDisplays* retail = Retail::CascSource::Instance().ItemDisplays();
    const Retail::RetailItemDisplay* display = retail ? retail->Display(static_cast<uint32_t>(id)) : nullptr;
    if (!display) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [served, added] = s_displays.try_emplace(id);
    if (added) {
        FillItemDisplay(served->second, *retail, *display);
    }
    memcpy(rec, &served->second.record, sizeof(ItemDisplayInfoRec));
    return 1;
}

static std::map<int32_t, std::string> s_stockIcons;

// A downported stock row keeps the retail id but names no icon when no appearance gave the display one, so it takes
// the icon retail's would be served with. Stock icons still win.
static void FillStockIcon(int32_t id, ItemDisplayInfoRec* rec) {
    if (id <= 0 || (rec->m_inventoryIcon[0] && *rec->m_inventoryIcon[0])) {
        return;
    }
    const Retail::RetailItemDisplays* retail = Retail::CascSource::Instance().ItemDisplays();
    const Retail::RetailItemDisplay* display = retail ? retail->Display(static_cast<uint32_t>(id)) : nullptr;
    if (!display || !display->icon) {
        return;
    }

    std::lock_guard<std::mutex> lock(s_servedMutex);
    auto [icon, added] = s_stockIcons.try_emplace(id);
    if (added) {
        icon->second = Retail::RetailFileName(display->icon);
    }
    rec->m_inventoryIcon[0] = icon->second.c_str();
}

#else

static int32_t ServedRecord(int32_t, ItemDisplayInfoRec*) {
    return 0;
}

static void FillStockIcon(int32_t, ItemDisplayInfoRec*) {
}

#endif

template <>
int32_t WowClientDB_RLE<ItemDisplayInfoRec>::GetRecord(int32_t id, ItemDisplayInfoRec* rec) const {
    if (StockRecord(*this, id, rec)) {
        FillStockIcon(id, rec);
        return 1;
    }
    return ServedRecord(id, rec);
}
