#ifndef WOW_SOURCE_DB_SERVERITEMRECORDS_H
#define WOW_SOURCE_DB_SERVERITEMRECORDS_H

#include <cstdint>
#include <map>

// Not in the original. Item.dbc rows for items only the server defines (custom item_template entries): every column
// Item.dbc holds is also in the item query reply the client caches (ItemStats_C), so an item with no stock row is
// served one built from that reply and equips, sheathes and shows like a stock item. Stock rows always win; an item
// the client has not cached yet has no row, as before. ServerItemRecords.cpp wires this into g_itemDB.
namespace ServerItemRecords {

// Rec is ItemRec and Stats is ItemStats_C (templates so the tests can use plain stand-ins).
template <class Stats, class Rec>
void FillItemRec(int32_t id, const Stats& stats, Rec& rec) {
    rec.m_id = id;
    rec.m_classID = static_cast<int32_t>(stats.m_class);
    rec.m_subclassID = static_cast<int32_t>(stats.m_subClass);
    rec.m_soundOverrideSubclassID = static_cast<int32_t>(stats.m_soundOverrideSubclass);
    rec.m_material = static_cast<int32_t>(stats.m_material);
    rec.m_displayInfoID = static_cast<int32_t>(stats.m_displayId);
    rec.m_inventoryType = static_cast<int32_t>(stats.m_inventoryType);
    rec.m_sheatheType = static_cast<int32_t>(stats.m_sheatheType);
}

// The served rows, kept for the session: callers hold the pointer GetRecord returns, and map nodes never move.
template <class Rec>
class Served {
    public:
    // stock: the Item.dbc row or null; stats: the cached query reply or null. Refilled on each lookup, so a newer
    // reply (the item changed on the server) shows through.
    template <class Stats>
    Rec* Resolve(int32_t id, Rec* stock, const Stats* stats) {
        if (stock || !stats) {
            return stock;
        }
        Rec& rec = this->m_rows[id];
        FillItemRec(id, *stats, rec);
        return &rec;
    }

    private:
    std::map<int32_t, Rec> m_rows;
};

}

#endif
