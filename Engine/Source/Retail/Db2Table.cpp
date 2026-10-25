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

// width bits (at most 32) at bit position, least significant first. Bits past the end read as zero.
static uint32_t ReadBits(const std::vector<uint8_t>& data, size_t position, uint32_t width) {
    if (width == 0) {
        return 0;
    }

    uint64_t value = 0;
    size_t first = position / 8;

    for (size_t i = 0; i < 8 && first + i < data.size(); i++) {
        value |= static_cast<uint64_t>(data[first + i]) << (i * 8);
    }

    value >>= position % 8;
    return static_cast<uint32_t>(width >= 32 ? value : value & ((1ull << width) - 1));
}

static uint32_t SignExtend(uint32_t value, uint32_t width) {
    if (width == 0 || width >= 32) {
        return value;
    }

    uint32_t sign = 1u << (width - 1);
    return (value ^ sign) - sign;
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
            id = ReadBits(data, record * 8 + column.offsetBits, column.sizeBits < 32 ? column.sizeBits : 32);
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

bool Db2Table::SparseField(const Row& row, size_t column, size_t& at) const {
    size_t end = row.offset + row.size;
    at = row.offset;
    for (size_t c = 0; c < column; c++) {
        if (this->m_inlineString[c]) {
            while (at < end && this->m_data[at]) {
                at++;
            }
            if (at == end) {
                return false;
            }
            at++;
        } else {
            at += this->m_columns[c].sizeBits / 8;
        }
        if (at > end) {
            return false;
        }
    }
    return at < end;
}

std::string Db2Table::SparseString(const Row& row, size_t column) const {
    size_t at;
    if (!this->m_inlineString[column] || !this->SparseField(row, column, at)) {
        return std::string();
    }
    size_t end = at;
    while (end < row.offset + row.size && this->m_data[end]) {
        end++;
    }
    if (end == row.offset + row.size) {
        return std::string();
    }
    return std::string(this->m_data.begin() + at, this->m_data.begin() + end);
}

uint32_t Db2Table::Value(size_t row, size_t column) const {
    std::vector<uint32_t> values = this->Values(row, column);
    return values.empty() ? 0 : values[0];
}

std::vector<uint32_t> Db2Table::Values(size_t row, size_t column) const {
    if (row >= this->m_rows.size() || column >= this->m_columns.size()) {
        return {};
    }

    const Row& at = this->m_rows[row];
    const Column& c = this->m_columns[column];
    size_t bits = at.offset * 8 + c.offsetBits;

    if (this->m_sparse) {
        size_t field;
        if (!this->SparseField(at, column, field) || field + (c.sizeBits ? c.sizeBits : 32) / 8 > at.offset + at.size) {
            return {};
        }
        bits = field * 8;
    }

    switch (c.compression) {
        case Compression::None: {
            uint32_t width = c.plainBits ? c.plainBits : 32;
            size_t count = c.sizeBits / width ? c.sizeBits / width : 1;
            std::vector<uint32_t> values;

            for (size_t i = 0; i < count; i++) {
                values.push_back(ReadBits(this->m_data, bits + i * width, width));
            }

            return values;
        }

        case Compression::Immediate:
            return { ReadBits(this->m_data, bits, c.widthBits) };

        case Compression::SignedImmediate:
            return { SignExtend(ReadBits(this->m_data, bits, c.widthBits), c.widthBits) };

        case Compression::Common: {
            auto value = c.common.find(at.id);
            return { value == c.common.end() ? c.defaultValue : value->second };
        }

        case Compression::Pallet: {
            size_t index = ReadBits(this->m_data, bits, c.widthBits);
            return { index < c.pallet.size() ? c.pallet[index] : 0 };
        }

        case Compression::PalletArray: {
            size_t start = static_cast<size_t>(ReadBits(this->m_data, bits, c.widthBits)) * c.cardinality;
            if (start + c.cardinality > c.pallet.size()) {
                return {};
            }

            return std::vector<uint32_t>(c.pallet.begin() + start, c.pallet.begin() + start + c.cardinality);
        }
    }

    return {};
}

std::string Db2Table::String(size_t row, size_t column) const {
    if (this->m_sparse) {
        return row < this->m_rows.size() && column < this->m_columns.size() ? this->SparseString(this->m_rows[row], column) : std::string();
    }
    uint32_t value = this->Value(row, column);
    if (!value || row >= this->m_rows.size() || column >= this->m_columns.size() || this->m_sections.empty()) {
        return std::string();
    }

    size_t at = this->m_rows[row].offset;
    for (const Section& section : this->m_sections) {
        if (at < section.records || at >= section.records + section.count * this->m_recordSize) {
            continue;
        }
        const Section& last = this->m_sections.back();
        size_t allRecords = (last.firstRecord + last.count) * this->m_recordSize;
        size_t field = (section.firstRecord * this->m_recordSize + at - section.records) + this->m_columns[column].offsetBits / 8;
        int64_t key = static_cast<int64_t>(field) + static_cast<int32_t>(value) - static_cast<int64_t>(allRecords);
        return this->StringAt(key);
    }
    return std::string();
}

// The string at key of the concatenated string blocks.
std::string Db2Table::StringAt(int64_t key) const {
    for (const Section& section : this->m_sections) {
        if (key < static_cast<int64_t>(section.firstString) || key >= static_cast<int64_t>(section.firstString + section.stringSize)) {
            continue;
        }
        size_t start = section.strings + static_cast<size_t>(key) - section.firstString;
        size_t end = start;
        while (end < section.strings + section.stringSize && end < this->m_data.size() && this->m_data[end]) {
            end++;
        }
        return std::string(this->m_data.begin() + start, this->m_data.begin() + end);
    }
    return std::string();
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
