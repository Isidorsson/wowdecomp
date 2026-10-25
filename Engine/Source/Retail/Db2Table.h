#ifndef ENGINE_SOURCE_RETAIL_DB2TABLE_H
#define ENGINE_SOURCE_RETAIL_DB2TABLE_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Retail {

class Db2Table;

// One row of a table with each column's place found once (Db2Table::ReadRow). A sparse row's column lies past every
// inline string before it, which Db2Table::Value walks again for each column; a Db2Row walks them once.
class Db2Row {
    public:
    uint32_t Value(size_t column) const;
    std::vector<uint32_t> Values(size_t column) const;
    std::string String(size_t column) const;

    private:
    friend class Db2Table;
    const Db2Table* m_table = nullptr;
    size_t m_row = 0;
    std::vector<size_t> m_fields;   // A sparse row's column byte offsets (NO_FIELD past its end); empty for a plain one.
};

// A read-only WDC5 (.db2) table: retail DBFilesClient tables. Integer columns by physical column number
// (WoWDBDefs order, noninline fields excluded), row ids from the id list or inline, copied rows. Sections
// encrypted for unreleased content arrive zero-filled and are skipped, as the retail client does without the
// key. A sparse table (ItemSparse) has records of their own size with strings inline: inlineStrings names its string
// columns, which the file gives no other way to skip.
class Db2Table {
    public:
    bool Parse(std::vector<uint8_t> bytes, std::string& error, std::vector<size_t> inlineStrings = {});

    uint32_t LayoutHash() const { return this->m_layoutHash; }
    size_t RowCount() const { return this->m_rows.size(); }
    uint32_t RowId(size_t row) const { return this->m_rows[row].id; }

    // The id the section's relationship map relates row to (a table's noninline foreign key), 0 for none.
    uint32_t Relation(size_t row) const { return this->m_rows[row].relation; }

    // The first (or only) value of column, 0 for none. Signed bit-packed columns come sign-extended.
    uint32_t Value(size_t row, size_t column) const { return this->ValueOf(row, column, nullptr); }
    std::vector<uint32_t> Values(size_t row, size_t column) const { return this->ValuesOf(row, column, nullptr); }
    // The string of a string column, empty for none (offset 0) or one outside the string blocks.
    std::string String(size_t row, size_t column) const { return this->StringOf(row, column, nullptr); }
    // row for reading many of its columns: the same values, a sparse row's columns found in one walk.
    Db2Row ReadRow(size_t row) const;

    // width bits (at most 32) at bit position, least significant first. Bits past the end read as zero.
    static uint32_t Bits(const std::vector<uint8_t>& data, size_t position, uint32_t width);

    // Records of the sections skipped as still locked (encrypted with a key the build does not ship).
    size_t SkippedRows() const { return this->m_skippedRows; }

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
        size_t size = 0;   // A sparse record's bytes.
    };

    // Where a section's records and strings lie. A string offset counts from its field as if every section's
    // records came first and every string block after them, in section order (DBCD WDC5Reader).
    struct Section {
        size_t records = 0;       // File offset of the first record.
        size_t count = 0;
        size_t firstRecord = 0;   // Records of the sections before it.
        size_t strings = 0;       // File offset of its string block.
        size_t stringSize = 0;
        size_t firstString = 0;   // String bytes of the sections before it.
    };

    bool ParseSection(size_t header, size_t recordSize, size_t idIndex, std::string& error);
    bool ParseSparseSection(size_t header, std::string& error);
    void AddCopies(size_t first, size_t copyTable, size_t copies);
    std::string StringAt(int64_t key) const;
    // A sparse row's column: its byte offset, walking the columns before it; false past the record's end.
    bool SparseField(const Row& row, size_t column, size_t& at) const;
    // Every column's offset in one walk, NO_FIELD past the record's end.
    std::vector<size_t> SparseFields(const Row& row) const;
    std::string SparseString(const Row& row, size_t column, size_t at) const;
    // Where column of row starts, in bits; fields (a Db2Row's) saves a sparse row's walk. False for none.
    bool FieldBits(size_t row, size_t column, const std::vector<size_t>* fields, size_t& bits) const;
    // Writes up to max of the column's values at bits and returns how many it has.
    size_t Decode(const Row& at, const Column& c, size_t bits, uint32_t* out, size_t max) const;
    uint32_t ValueOf(size_t row, size_t column, const std::vector<size_t>* fields) const;
    std::vector<uint32_t> ValuesOf(size_t row, size_t column, const std::vector<size_t>* fields) const;
    std::string StringOf(size_t row, size_t column, const std::vector<size_t>* fields) const;

    friend class Db2Row;

    std::vector<uint8_t> m_data;
    uint32_t m_layoutHash = 0;
    std::vector<Column> m_columns;
    std::vector<Row> m_rows;
    std::vector<Section> m_sections;
    size_t m_recordSize = 0;
    size_t m_skippedRows = 0;
    bool m_sparse = false;
    std::vector<bool> m_inlineString;   // By column, for a sparse table.
};

constexpr size_t NO_FIELD = SIZE_MAX;

// Refuses table, by name, when its layout is not the one a loader reads.
bool KnownLayout(const Db2Table& table, uint32_t layout, const char* name, std::string& error);

}

#endif
