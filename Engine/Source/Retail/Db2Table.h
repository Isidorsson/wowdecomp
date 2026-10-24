#ifndef ENGINE_SOURCE_RETAIL_DB2TABLE_H
#define ENGINE_SOURCE_RETAIL_DB2TABLE_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Retail {

// A read-only WDC5 (.db2) table: retail DBFilesClient tables. Integer columns by physical column number
// (WoWDBDefs order, noninline fields excluded), row ids from the id list or inline, copied rows. Sections
// encrypted for unreleased content arrive zero-filled and are skipped, as the retail client does without the
// key. Sparse tables and strings are not read yet.
class Db2Table {
    public:
    bool Parse(std::vector<uint8_t> bytes, std::string& error);

    uint32_t LayoutHash() const { return this->m_layoutHash; }
    size_t RowCount() const { return this->m_rows.size(); }
    uint32_t RowId(size_t row) const { return this->m_rows[row].id; }

    // The id the section's relationship map relates row to (a table's noninline foreign key), 0 for none.
    uint32_t Relation(size_t row) const { return this->m_rows[row].relation; }

    // The first (or only) value of column. Signed bit-packed columns come sign-extended.
    uint32_t Value(size_t row, size_t column) const;
    std::vector<uint32_t> Values(size_t row, size_t column) const;

    private:
    enum class Compression : uint32_t {
        None = 0,
        Immediate = 1,
        Common = 2,
        Pallet = 3,
        PalletArray = 4,
        SignedImmediate = 5,
    };

    struct Column {
        uint32_t offsetBits = 0;
        uint32_t sizeBits = 0;
        uint32_t extraBytes = 0;
        uint32_t plainBits = 0;     // 32 - field meta bits: a plain column's element width.
        Compression compression = Compression::None;
        uint32_t defaultValue = 0;  // Common: rows the common data does not list.
        uint32_t widthBits = 0;     // Bit-packed and pallet: bits of the value or pallet index.
        uint32_t cardinality = 0;   // Pallet array: values per row.
        std::vector<uint32_t> pallet;
        std::map<uint32_t, uint32_t> common;
    };

    struct Row {
        uint32_t id;
        size_t offset;
        uint32_t relation = 0;
    };

    bool ParseSection(size_t header, size_t recordSize, size_t idIndex, std::string& error);

    std::vector<uint8_t> m_data;
    uint32_t m_layoutHash = 0;
    std::vector<Column> m_columns;
    std::vector<Row> m_rows;
};

// Refuses table, by name, when its layout is not the one a loader reads.
bool KnownLayout(const Db2Table& table, uint32_t layout, const char* name, std::string& error);

}

#endif
