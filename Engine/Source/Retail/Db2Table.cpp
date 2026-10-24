#include "Engine/Source/Retail/Db2Table.h"
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

bool Db2Table::Parse(std::vector<uint8_t> bytes, std::string& error) {
    this->m_data = std::move(bytes);
    this->m_columns.clear();
    this->m_rows.clear();
    const std::vector<uint8_t>& data = this->m_data;

    if (!Fits(data, 0, HEADER_SIZE) || std::memcmp(data.data(), "WDC5", 4) != 0) {
        return Fail(error, "not a WDC5 table");
    }

    size_t fieldCount = Read<uint32_t>(data, 140);
    size_t recordSize = Read<uint32_t>(data, 144);
    this->m_layoutHash = Read<uint32_t>(data, 156);
    uint16_t flags = Read<uint16_t>(data, 172);
    size_t idIndex = Read<uint16_t>(data, 174);
    size_t sectionCount = Read<uint32_t>(data, 200);

    if (flags & FLAG_SPARSE) {
        return Fail(error, "sparse tables are not read");
    }

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
        if (!this->ParseSection(HEADER_SIZE + s * SECTION_HEADER_SIZE, recordSize, idIndex, error)) {
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
    size_t copies = Read<uint32_t>(data, header + 36);

    size_t idList = offset + records * recordSize + strings;
    size_t copyTable = idList + idListSize;

    if (!Fits(data, offset, records * recordSize) || !Fits(data, idList, idListSize) || !Fits(data, copyTable, copies * 8)) {
        return Fail(error, "a section runs past the end of the file");
    }

    bool zeroed = records > 0;
    for (size_t i = offset; zeroed && i < offset + records * recordSize; i++) {
        zeroed = data[i] == 0;
    }

    if (zeroed && tactKey) {
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

        this->m_rows.push_back({ id, record });
    }

    std::map<uint32_t, size_t> byId;
    for (size_t i = first; i < this->m_rows.size(); i++) {
        byId[this->m_rows[i].id] = this->m_rows[i].offset;
    }

    for (size_t c = 0; c < copies; c++) {
        auto source = byId.find(Read<uint32_t>(data, copyTable + c * 8 + 4));

        if (source != byId.end()) {
            this->m_rows.push_back({ Read<uint32_t>(data, copyTable + c * 8), source->second });
        }
    }

    return true;
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

}
