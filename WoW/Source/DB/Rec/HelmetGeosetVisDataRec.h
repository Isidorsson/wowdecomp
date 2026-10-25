#ifndef WOW_SOURCE_DB_REC_HELMETGEOSETVISDATAREC_H
#define WOW_SOURCE_DB_REC_HELMETGEOSETVISDATAREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class HelmetGeosetVisDataRec {
    public:
    static const int32_t s_numColumns = 8;
    static const int32_t s_rowSize = 32;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_hideGeoset [7];

    HelmetGeosetVisDataRec();
    ~HelmetGeosetVisDataRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

// Not in the original: this table serves, after its own rows, the hidden geosets of retail helms
// (RetailHelmetRecords.cpp). Declared with the record so every caller of GetRecord reaches it.
#include "WoW/Source/DB/WowClientDB.h"

template <>
HelmetGeosetVisDataRec* WowClientDB<HelmetGeosetVisDataRec>::GetRecord(int32_t id) const;

#endif
