#ifndef WOW_SOURCE_DB_REC_ITEMDISPLAYINFOREC_H
#define WOW_SOURCE_DB_REC_ITEMDISPLAYINFOREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class ItemDisplayInfoRec {
    public:
    static const int32_t s_numColumns = 25;
    static const int32_t s_rowSize = 100;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_modelName [2];
    /* 000C */ const char* m_modelTexture [2];
    /* 0014 */ const char* m_inventoryIcon [2];
    /* 001C */ int32_t m_geosetGroup [3];
    /* 0028 */ int32_t m_flags;
    /* 002C */ int32_t m_spellVisualID;
    /* 0030 */ int32_t m_groupSoundIndex;
    /* 0034 */ int32_t m_helmetGeosetVisID [2];
    /* 003C */ const char* m_texture [8];
    /* 005C */ int32_t m_itemVisual;
    /* 0060 */ int32_t m_particleColorID;

    ItemDisplayInfoRec();
    ~ItemDisplayInfoRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

// Not in the original: this table serves, after its own rows, the retail item displays of a CASC build
// (RetailItemRecords.cpp). Declared with the record so every caller of GetRecord reaches it.
#include "WoW/Source/DB/WowClientDB.h"

template <>
int32_t WowClientDB_RLE<ItemDisplayInfoRec>::GetRecord(int32_t id, ItemDisplayInfoRec* rec) const;

#endif
