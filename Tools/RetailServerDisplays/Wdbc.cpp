#include "Tools/RetailServerDisplays/Wdbc.h"
#include <algorithm>
#include <cstring>

namespace ServerDisplays {

constexpr char MAGIC[4] = { 'W', 'D', 'B', 'C' };
constexpr size_t HEADER_SIZE = 20;
constexpr size_t FIELD_SIZE = 4;

static uint32_t ReadU32(const uint8_t* at) {
    uint32_t value;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

static void AppendU32(std::vector<uint8_t>& bytes, uint32_t value) {
    uint8_t raw[sizeof(value)];
    std::memcpy(raw, &value, sizeof(value));
    bytes.insert(bytes.end(), raw, raw + sizeof(value));
}

bool Wdbc::Parse(const std::vector<uint8_t>& bytes, std::string& error) {
    if (bytes.size() < HEADER_SIZE || std::memcmp(bytes.data(), MAGIC, sizeof(MAGIC))) {
        error = "not a WDBC file";
        return false;
    }

    uint32_t rows = ReadU32(bytes.data() + 4);
    uint32_t fields = ReadU32(bytes.data() + 8);
    uint32_t recordSize = ReadU32(bytes.data() + 12);
    uint32_t stringSize = ReadU32(bytes.data() + 16);
    if (fields == 0 || recordSize != fields * FIELD_SIZE) {
        error = "records are not whole 32-bit fields";
        return false;
    }

    uint64_t recordBytes = uint64_t(rows) * recordSize;
    if (bytes.size() != HEADER_SIZE + recordBytes + stringSize) {
        error = "size does not match the header";
        return false;
    }

    const uint8_t* records = bytes.data() + HEADER_SIZE;
    this->m_fieldCount = fields;
    this->m_recordSize = recordSize;
    this->m_records.assign(records, records + recordBytes);
    this->m_strings.assign(records + recordBytes, records + recordBytes + stringSize);
    if (this->m_strings.empty()) {
        this->m_strings.push_back('\0');
    }
    return true;
}

std::vector<uint8_t> Wdbc::Serialize() const {
    std::vector<uint8_t> bytes(MAGIC, MAGIC + sizeof(MAGIC));
    AppendU32(bytes, static_cast<uint32_t>(this->RowCount()));
    AppendU32(bytes, this->m_fieldCount);
    AppendU32(bytes, this->m_recordSize);
    AppendU32(bytes, static_cast<uint32_t>(this->m_strings.size()));
    bytes.insert(bytes.end(), this->m_records.begin(), this->m_records.end());
    bytes.insert(bytes.end(), this->m_strings.begin(), this->m_strings.end());
    return bytes;
}

size_t Wdbc::RowCount() const {
    return this->m_recordSize ? this->m_records.size() / this->m_recordSize : 0;
}

uint32_t Wdbc::Field(size_t row, size_t field) const {
    return ReadU32(this->m_records.data() + row * this->m_recordSize + field * FIELD_SIZE);
}

bool Wdbc::HasId(uint32_t id) const {
    for (size_t row = 0; row < this->RowCount(); row++) {
        if (this->Id(row) == id) {
            return true;
        }
    }
    return false;
}

uint32_t Wdbc::MaxId() const {
    uint32_t largest = 0;
    for (size_t row = 0; row < this->RowCount(); row++) {
        largest = std::max(largest, this->Id(row));
    }
    return largest;
}

uint32_t Wdbc::AddString(const std::string& value) {
    if (value.empty()) {
        return 0;
    }

    // Matching with the terminator, after a terminator, finds only whole strings.
    std::string whole = std::string(1, '\0') + value + '\0';
    auto found = std::search(this->m_strings.begin(), this->m_strings.end(), whole.begin(), whole.end());
    if (found != this->m_strings.end()) {
        return static_cast<uint32_t>(found - this->m_strings.begin()) + 1;
    }

    uint32_t offset = static_cast<uint32_t>(this->m_strings.size());
    this->m_strings.insert(this->m_strings.end(), value.c_str(), value.c_str() + value.size() + 1);
    return offset;
}

const char* Wdbc::String(uint32_t offset) const {
    return offset < this->m_strings.size() ? this->m_strings.data() + offset : "";
}

bool Wdbc::AppendRow(const std::vector<uint32_t>& fields) {
    if (fields.size() != this->m_fieldCount) {
        return false;
    }

    for (uint32_t value : fields) {
        AppendU32(this->m_records, value);
    }
    return true;
}

}
