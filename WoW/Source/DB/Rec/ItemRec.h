#ifndef WOW_SOURCE_DB_REC_ITEMREC_H
#define WOW_SOURCE_DB_REC_ITEMREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class ItemRec {
    public:
    static const int32_t s_numColumns = 8;
    static const int32_t s_rowSize = 32;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_classID;
    /* 0008 */ int32_t m_subclassID;
    /* 000C */ int32_t m_soundOverrideSubclassID;
    /* 0010 */ int32_t m_material;
    /* 0014 */ int32_t m_displayInfoID;
    /* 0018 */ int32_t m_inventoryType;
    /* 001C */ int32_t m_sheatheType;

    ItemRec();
    ~ItemRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

// Not in the original: this table serves, after its own rows, items only the server defines, built from their cached
// query reply (ServerItemRecords.cpp). Declared with the record so every caller of GetRecord reaches it.
#include "WoW/Source/DB/WowClientDB.h"

template <>
ItemRec* WowClientDB<ItemRec>::GetRecord(int32_t id) const;

#endif
