#ifndef WOW_SOURCE_DB_RETAILHELMETRECORDS_H
#define WOW_SOURCE_DB_RETAILHELMETRECORDS_H

#include <cstddef>
#include <cstdint>

// Not in the original. A HelmetGeosetVisData mask holds 1 << race, so races 1 to 31 only: for a race above (a served
// retail race), whether a served helm's vis row hides slot (HelmetGeosetVisDataRec::m_hideGeoset order). False for a
// stock vis id, whose rows name no such race.
bool RetailHelmHides(int32_t visId, uint32_t race, size_t slot);

#endif
