#include "Engine/Source/Retail/Db2Table.h"

// Reading a parsed WDC5 table: column values by compression, strings, and sparse rows' inline columns. Parsing is in
// Db2Table.cpp.

namespace Retail {

uint32_t Db2Table::Bits(const std::vector<uint8_t>& data, size_t position, uint32_t width) {
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

// As SparseField for every column at once: a column has its offset while the walk is still inside the record.
std::vector<size_t> Db2Table::SparseFields(const Row& row) const {
    std::vector<size_t> fields(this->m_columns.size(), NO_FIELD);
    size_t end = row.offset + row.size;
    size_t at = row.offset;
    for (size_t c = 0; c < fields.size() && at < end; c++) {
        fields[c] = at;
        if (!this->m_inlineString[c]) {
            at += this->m_columns[c].sizeBits / 8;
            continue;
        }
        while (at < end && this->m_data[at]) {
            at++;
        }
        if (at == end) {
            break;
        }
        at++;
    }
    return fields;
}

std::string Db2Table::SparseString(const Row& row, size_t column, size_t at) const {
    if (!this->m_inlineString[column]) {
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

bool Db2Table::FieldBits(size_t row, size_t column, const std::vector<size_t>* fields, size_t& bits) const {
    if (row >= this->m_rows.size() || column >= this->m_columns.size()) {
        return false;
    }
    const Row& at = this->m_rows[row];
    const Column& c = this->m_columns[column];
    if (!this->m_sparse) {
        bits = at.offset * 8 + c.offsetBits;
        return true;
    }
    size_t field = fields ? (*fields)[column] : NO_FIELD;
    if (!fields && !this->SparseField(at, column, field)) {
        return false;
    }
    if (field == NO_FIELD || field + (c.sizeBits ? c.sizeBits : 32) / 8 > at.offset + at.size) {
        return false;
    }
    bits = field * 8;
    return true;
}

size_t Db2Table::Decode(const Row& at, const Column& c, size_t bits, uint32_t* out, size_t max) const {
    uint32_t value = 0;
    switch (c.compression) {
        case Compression::None: {
            uint32_t width = c.plainBits ? c.plainBits : 32;
            size_t count = c.sizeBits / width ? c.sizeBits / width : 1;
            for (size_t i = 0; i < count && i < max; i++) {
                out[i] = Bits(this->m_data, bits + i * width, width);
            }
            return count;
        }
        case Compression::Immediate:
            value = Bits(this->m_data, bits, c.widthBits);
            break;
        case Compression::SignedImmediate:
            value = SignExtend(Bits(this->m_data, bits, c.widthBits), c.widthBits);
            break;
        case Compression::Common: {
            auto common = c.common.find(at.id);
            value = common == c.common.end() ? c.defaultValue : common->second;
            break;
        }
        case Compression::Pallet: {
            size_t index = Bits(this->m_data, bits, c.widthBits);
            value = index < c.pallet.size() ? c.pallet[index] : 0;
            break;
        }
        case Compression::PalletArray: {
            size_t start = static_cast<size_t>(Bits(this->m_data, bits, c.widthBits)) * c.cardinality;
            if (start + c.cardinality > c.pallet.size()) {
                return 0;
            }
            for (size_t i = 0; i < c.cardinality && i < max; i++) {
                out[i] = c.pallet[start + i];
            }
            return c.cardinality;
        }
        default:
            return 0;
    }
    if (max) {
        out[0] = value;
    }
    return 1;
}

uint32_t Db2Table::ValueOf(size_t row, size_t column, const std::vector<size_t>* fields) const {
    size_t bits;
    uint32_t value = 0;
    if (this->FieldBits(row, column, fields, bits)) {
        this->Decode(this->m_rows[row], this->m_columns[column], bits, &value, 1);
    }
    return value;
}

std::vector<uint32_t> Db2Table::ValuesOf(size_t row, size_t column, const std::vector<size_t>* fields) const {
    size_t bits;
    if (!this->FieldBits(row, column, fields, bits)) {
        return {};
    }
    const Row& at = this->m_rows[row];
    const Column& c = this->m_columns[column];
    std::vector<uint32_t> values(this->Decode(at, c, bits, nullptr, 0));
    this->Decode(at, c, bits, values.data(), values.size());
    return values;
}

std::string Db2Table::StringOf(size_t row, size_t column, const std::vector<size_t>* fields) const {
    if (row >= this->m_rows.size() || column >= this->m_columns.size()) {
        return std::string();
    }
    if (this->m_sparse) {
        size_t at = fields ? (*fields)[column] : NO_FIELD;
        if (!fields && !this->SparseField(this->m_rows[row], column, at)) {
            return std::string();
        }
        return at == NO_FIELD ? std::string() : this->SparseString(this->m_rows[row], column, at);
    }
    uint32_t value = this->Value(row, column);
    if (!value || this->m_sections.empty()) {
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

Db2Row Db2Table::ReadRow(size_t row) const {
    Db2Row reader;
    reader.m_table = this;
    reader.m_row = row;
    if (this->m_sparse && row < this->m_rows.size()) {
        reader.m_fields = this->SparseFields(this->m_rows[row]);
    }
    return reader;
}

uint32_t Db2Row::Value(size_t column) const {
    return this->m_table->ValueOf(this->m_row, column, this->m_fields.empty() ? nullptr : &this->m_fields);
}

std::vector<uint32_t> Db2Row::Values(size_t column) const {
    return this->m_table->ValuesOf(this->m_row, column, this->m_fields.empty() ? nullptr : &this->m_fields);
}

std::string Db2Row::String(size_t column) const {
    return this->m_table->StringOf(this->m_row, column, this->m_fields.empty() ? nullptr : &this->m_fields);
}

}
