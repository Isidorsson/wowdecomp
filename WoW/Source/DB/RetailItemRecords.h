#ifndef WOW_SOURCE_DB_RETAILITEMRECORDS_H
#define WOW_SOURCE_DB_RETAILITEMRECORDS_H

#include <cstdint>

// Not in the original. One line in Logs/RetailAssets.log, once per model and mesh: an item names a geoset mesh the
// model lacks (a retail boots value on an installed body), so the group's default is drawn (ItemGeosets.h).
void LogItemGeosetFallback(const char* model, uint32_t requested, uint32_t drawn);

#endif
