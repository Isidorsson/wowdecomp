#include "Engine/Source/Retail/Db2Table.h"
#include <cstdio>
#include <cstring>

namespace Retail {

// WDC5 layout, as DBCD's WDC5Reader (wowdev/DBCD, MIT) and wow-toolbox-editor read it.
static constexpr size_t HEADER_SIZE = 204;
static constexpr size_t SECTION_HEADER_SIZE = 40;
static constexpr size_t FIELD_META_SIZE = 4;
static constexpr size_t COLUMN_META_SIZE = 24;
static constexpr uint16_t FLAG_SPARSE = 0x1;

static bool Fits(const std::vector<uint8_t>& data, size_t at, size_t size) {
    return at <= data.size() && size <= data.size() - at;
}

template <typename T>
static T Read(const std::vector<uint8_t>& data, size_t at) {
    T value;
    std::memcpy(&value, data.data() + at, sizeof(T));
    return value;
}

static bool Fail(std::string& error, const std::string& message) {
    error = "db2: " + message;
    return false;
}

bool Db2Table::Parse(std::vector<uint8_t> bytes, std::string& error, std::vector<size_t> inlineStrings) {
    this->m_data = std::move(bytes);
    this->m_columns.clear();
    this->m_rows.clear();
    this->m_sections.clear();
    this->m_skippedRows = 0;
    this->m_inlineString.clear();
    const std::vector<uint8_t>& data = this->m_data;

    if (!Fits(data, 0, HEADER_SIZE) || std::memcmp(data.data(), "WDC5", 4) != 0) {
        return Fail(error, "not a WDC5 table");
    }

    size_t fieldCount = Read<uint32_t>(data, 140);
    size_t recordSize = Read<uint32_t>(data, 144);
    this->m_recordSize = recordSize;
    this->m_layoutHash = Read<uint32_t>(data, 156);
    uint16_t flags = Read<uint16_t>(data, 172);
    size_t idIndex = Read<uint16_t>(data, 174);
    size_t sectionCount = Read<uint32_t>(data, 200);

    this->m_sparse = flags & FLAG_SPARSE;

    size_t fieldMeta = HEADER_SIZE + sectionCount * SECTION_HEADER_SIZE;
    size_t columnMeta = fieldMeta + fieldCount * FIELD_META_SIZE;
    size_t at = columnMeta + fieldCount * COLUMN_META_SIZE;

    if (recordSize == 0 || !Fits(data, HEADER_SIZE, at - HEADER_SIZE)) {
        return Fail(error, "the header runs past the end of the file");
    }

    for (size_t i = 0; i < fieldCount; i++) {
        size_t meta = columnMeta + i * COLUMN_META_SIZE;
        uint32_t compression = Read<uint32_t>(data, meta + 8);

        if (compression > static_cast<uint32_t>(Compression::SignedImmediate)) {
            return Fail(error, "unknown column compression " + std::to_string(compression));
        }

        Column column;
        column.offsetBits = Read<uint16_t>(data, meta);
        column.sizeBits = Read<uint16_t>(data, meta + 2);
        column.extraBytes = Read<uint32_t>(data, meta + 4);
        column.compression = static_cast<Compression>(compression);
        column.defaultValue = Read<uint32_t>(data, meta + 12);
        column.widthBits = Read<uint32_t>(data, meta + 16);
        column.cardinality = Read<uint32_t>(data, meta + 20);
        int32_t bits = Read<int16_t>(data, fieldMeta + i * FIELD_META_SIZE);
        column.plainBits = bits < 32 ? static_cast<uint32_t>(32 - bits) : 0;
        this->m_columns.push_back(column);
    }

    this->m_inlineString.assign(fieldCount, false);
    for (size_t column : inlineStrings) {
        if (column < fieldCount) {
            this->m_inlineString[column] = true;
        }
    }

    // Pallet data for pallet and pallet array columns in column order, then common data.
    for (Column& column : this->m_columns) {
        if (column.compression != Compression::Pallet && column.compression != Compression::PalletArray) {
            continue;
        }

        if (!Fits(data, at, column.extraBytes)) {
            return Fail(error, "pallet data runs past the end of the file");
        }

        for (size_t i = 0; i < column.extraBytes / 4; i++) {
            column.pallet.push_back(Read<uint32_t>(data, at + i * 4));
        }

        at += column.extraBytes;
    }

    for (Column& column : this->m_columns) {
        if (column.compression != Compression::Common) {
            continue;
        }

        if (!Fits(data, at, column.extraBytes)) {
            return Fail(error, "common data runs past the end of the file");
        }

        for (size_t i = 0; i < column.extraBytes / 8; i++) {
            column.common[Read<uint32_t>(data, at + i * 8)] = Read<uint32_t>(data, at + i * 8 + 4);
        }

        at += column.extraBytes;
    }

    for (size_t s = 0; s < sectionCount; s++) {
        size_t header = HEADER_SIZE + s * SECTION_HEADER_SIZE;
        if (!(this->m_sparse ? this->ParseSparseSection(header, error) : this->ParseSection(header, recordSize, idIndex, error))) {
            return false;
        }
    }

    return true;
}

bool Db2Table::ParseSection(size_t header, size_t recordSize, size_t idIndex, std::string& error) {
    const std::vector<uint8_t>& data = this->m_data;
    uint64_t tactKey = Read<uint64_t>(data, header);
    size_t offset = Read<uint32_t>(data, header + 8);
    size_t records = Read<uint32_t>(data, header + 12);
    size_t strings = Read<uint32_t>(data, header + 16);
    size_t idListSize = Read<uint32_t>(data, header + 24);
    size_t relationSize = Read<uint32_t>(data, header + 28);
    size_t copies = Read<uint32_t>(data, header + 36);

    size_t idList = offset + records * recordSize + strings;
    size_t copyTable = idList + idListSize;
    size_t relationMap = copyTable + copies * 8;

    if (!Fits(data, offset, records * recordSize) || !Fits(data, idList, idListSize) || !Fits(data, copyTable, copies * 8) || !Fits(data, relationMap, relationSize)) {
        return Fail(error, "a section runs past the end of the file");
    }

    Section section;
    section.records = offset;
    section.count = records;
    section.strings = offset + records * recordSize;
    section.stringSize = strings;
    if (!this->m_sections.empty()) {
        const Section& last = this->m_sections.back();
        section.firstRecord = last.firstRecord + last.count;
        section.firstString = last.firstString + last.stringSize;
    }
    this->m_sections.push_back(section);

    bool zeroed = records > 0;
    for (size_t i = offset; zeroed && i < offset + records * recordSize; i++) {
        zeroed = data[i] == 0;
    }

    if (zeroed && tactKey) {
        this->m_skippedRows += records;
        return true;
    }

    if (zeroed) {
        return Fail(error, "an unencrypted section reads as zeros: the file is damaged");
    }

    if (idListSize && idListSize != records * 4) {
        return Fail(error, "the id list does not match the record count");
    }

    if (!idListSize && idIndex >= this->m_columns.size()) {
        return Fail(error, "the id column is missing");
    }

    // Relationship map: a count, the lowest and highest foreign id, then (foreign id, record index) entries.
    std::map<uint32_t, uint32_t> relations;
    size_t relationCount = relationSize >= 12 ? Read<uint32_t>(data, relationMap) : 0;
    if (relationSize && relationSize < 12 + relationCount * 8) {
        return Fail(error, "the relationship map runs past its size");
    }

    for (size_t r = 0; r < relationCount; r++) {
        relations[Read<uint32_t>(data, relationMap + 12 + r * 8 + 4)] = Read<uint32_t>(data, relationMap + 12 + r * 8);
    }

    size_t first = this->m_rows.size();

    for (size_t i = 0; i < records; i++) {
        size_t record = offset + i * recordSize;
        uint32_t id;

        if (idListSize) {
            id = Read<uint32_t>(data, idList + i * 4);
        } else {
            const Column& column = this->m_columns[idIndex];
            id = Db2Table::Bits(data, record * 8 + column.offsetBits, column.sizeBits < 32 ? column.sizeBits : 32);
        }

        auto relation = relations.find(static_cast<uint32_t>(i));
        this->m_rows.push_back({ id, record, relation == relations.end() ? 0 : relation->second });
    }

    this->AddCopies(first, copyTable, copies);
    return true;
}

// A copy takes its source's record and relation.
void Db2Table::AddCopies(size_t first, size_t copyTable, size_t copies) {
    std::map<uint32_t, Row> byId;
    for (size_t i = first; i < this->m_rows.size(); i++) {
        byId[this->m_rows[i].id] = this->m_rows[i];
    }

    for (size_t c = 0; c < copies; c++) {
        auto source = byId.find(Read<uint32_t>(this->m_data, copyTable + c * 8 + 4));

        if (source != byId.end()) {
            Row copy = source->second;
            copy.id = Read<uint32_t>(this->m_data, copyTable + c * 8);
            this->m_rows.push_back(copy);
        }
    }
}

// A sparse section (DBCD WDC5Reader): records up to the records end, then the id list, the copy table, the offset map
// (u32 file offset and u16 size per record), the relationship map and the offset map's ids, which name the rows.
bool Db2Table::ParseSparseSection(size_t header, std::string& error) {
    const std::vector<uint8_t>& data = this->m_data;
    uint64_t tactKey = Read<uint64_t>(data, header);
    size_t offset = Read<uint32_t>(data, header + 8);
    size_t records = Read<uint32_t>(data, header + 12);
    size_t recordsEnd = Read<uint32_t>(data, header + 20);
    size_t idListSize = Read<uint32_t>(data, header + 24);
    size_t relationSize = Read<uint32_t>(data, header + 28);
    size_t mapCount = Read<uint32_t>(data, header + 32);
    size_t copies = Read<uint32_t>(data, header + 36);

    size_t copyTable = recordsEnd + idListSize;
    size_t offsetMap = copyTable + copies * 8;
    size_t relationMap = offsetMap + mapCount * 6;
    size_t mapIds = relationMap + relationSize;

    if (recordsEnd < offset || !Fits(data, offset, recordsEnd - offset) || !Fits(data, recordsEnd, mapIds + mapCount * 4 - recordsEnd)) {
        return Fail(error, "a sparse section runs past the end of the file");
    }

    if (mapCount != records) {
        return Fail(error, "the offset map does not match the record count");
    }

    bool zeroed = recordsEnd > offset;
    for (size_t i = offset; zeroed && i < recordsEnd; i++) {
        zeroed = data[i] == 0;
    }

    if (zeroed && tactKey) {
        this->m_skippedRows += records;
        return true;
    }

    if (zeroed) {
        return Fail(error, "an unencrypted section reads as zeros: the file is damaged");
    }

    std::map<uint32_t, uint32_t> relations;
    size_t relationCount = relationSize >= 12 ? Read<uint32_t>(data, relationMap) : 0;
    if (relationSize && relationSize < 12 + relationCount * 8) {
        return Fail(error, "the relationship map runs past its size");
    }
    for (size_t r = 0; r < relationCount; r++) {
        relations[Read<uint32_t>(data, relationMap + 12 + r * 8 + 4)] = Read<uint32_t>(data, relationMap + 12 + r * 8);
    }

    size_t first = this->m_rows.size();
    for (size_t i = 0; i < records; i++) {
        size_t at = Read<uint32_t>(data, offsetMap + i * 6);
        size_t size = Read<uint16_t>(data, offsetMap + i * 6 + 4);
        if (at < offset || at > recordsEnd || size > recordsEnd - at) {
            return Fail(error, "a sparse record lies outside its section");
        }
        auto relation = relations.find(static_cast<uint32_t>(i));
        this->m_rows.push_back({ Read<uint32_t>(data, mapIds + i * 4), at, relation == relations.end() ? 0 : relation->second, size });
    }

    this->AddCopies(first, copyTable, copies);
    return true;
}

bool KnownLayout(const Db2Table& table, uint32_t layout, const char* name, std::string& error) {
    if (table.LayoutHash() == layout) {
        return true;
    }

    char message[128];
    std::snprintf(message, sizeof(message), "%s layout %08X is not the known %08X (a newer build?)", name, table.LayoutHash(), layout);
    error = message;
    return false;
}

}
