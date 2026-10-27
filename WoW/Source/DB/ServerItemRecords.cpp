#include "WoW/Source/DB/ServerItemRecords.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/Rec/ItemRec.h"

// Not in the original. Item.dbc lookups fall back to the item query cache (ServerItemRecords.h). Used on the main
// thread, like the cache it reads.
static ServerItemRecords::Served<ItemRec> s_served;

template <>
ItemRec* WowClientDB<ItemRec>::GetRecord(int32_t id) const {
    ItemRec* stock = id < this->m_minID || id > this->m_maxID ? nullptr : this->m_recordsById[id - this->m_minID];
    if (stock) {
        return stock;
    }
    // No callback: only reads what the client already has, never sends a query.
    const ItemStats_C* stats = g_itemDBCache.GetRecord(id, WOWGUID(), nullptr, nullptr, false);
    return s_served.Resolve(id, stock, stats);
}
