#ifndef TOOLS_RETAILSERVERDISPLAYS_WDBC_H
#define TOOLS_RETAILSERVERDISPLAYS_WDBC_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ServerDisplays {

// A 3.3.5 WDBC (.dbc) file of 32-bit fields, as the server reads its DataDir tables: rows keep their bytes, so a
// file comes back out unchanged plus whatever was appended.
class Wdbc {
    public:
    bool Parse(const std::vector<uint8_t>& bytes, std::string& error);
    std::vector<uint8_t> Serialize() const;

    uint32_t FieldCount() const { return this->m_fieldCount; }
    size_t RowCount() const;
    uint32_t Field(size_t row, size_t field) const;
    uint32_t Id(size_t row) const { return this->Field(row, 0); }
    bool HasId(uint32_t id) const;
    uint32_t MaxId() const;

    // The offset of value in the string block, added once.
    uint32_t AddString(const std::string& value);
    const char* String(uint32_t offset) const;

    // False when fields is not one value per field.
    bool AppendRow(const std::vector<uint32_t>& fields);

    private:
    uint32_t m_fieldCount = 0;
    uint32_t m_recordSize = 0;
    std::vector<uint8_t> m_records;
    std::vector<char> m_strings;
};

}

#endif
