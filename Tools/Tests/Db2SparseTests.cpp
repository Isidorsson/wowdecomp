#include <cstring>
#include <string>
#include "Engine/Source/Retail/Db2Table.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/ItemSparseFixture.h"
#include "Tools/Tests/Wdc5Builder.h"

using Retail::Db2Table;

// Sparse tables (ItemSparse): records of their own size with strings inline, found through the offset map, ids from
// the offset map's id list. Columns are read in order, a string column by its terminator.

// Columns: a string, a 32-bit value, a 16-bit value, another string, an 8-bit value.
static Wdc5Spec SparseSpec() {
    Wdc5Spec spec;
    spec.flags = 0x1;
    spec.recordSize = 15;
    spec.columns = {
        { 0, 0, 32, 0, 0, { 0, 0, 0 } },
        { 0, 32, 32, 0, 0, { 0, 0, 0 } },
        { 16, 64, 16, 0, 0, { 0, 0, 0 } },
        { 0, 80, 32, 0, 0, { 0, 0, 0 } },
        { 24, 112, 8, 0, 0, { 0, 0, 0 } },
    };
    return spec;
}

static std::vector<uint8_t> SparseRecord(const std::string& first, uint32_t value, uint16_t small, const std::string& second, uint8_t tiny) {
    std::vector<uint8_t> record(first.begin(), first.end());
    record.push_back(0);
    AppendU32(record, value);
    record.push_back(static_cast<uint8_t>(small));
    record.push_back(static_cast<uint8_t>(small >> 8));
    record.insert(record.end(), second.begin(), second.end());
    record.push_back(0);
    record.push_back(tiny);
    return record;
}

static const std::vector<size_t> SPARSE_STRINGS = { 0, 3 };

TEST(Db2SparseRowsReadInOrderPastInlineStrings) {
    Wdc5Spec spec = SparseSpec();
    Wdc5Section section;
    section.sparseRecords = { SparseRecord("Thunderfury", 4909, 2600, "", 5), SparseRecord("", 7, 65535, "Plate", 200) };
    section.ids = { 19019, 16958 };
    spec.sections = { section };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error, SPARSE_STRINGS));
    CHECK(table.RowCount() == 2 && table.RowId(0) == 19019 && table.RowId(1) == 16958);
    CHECK(table.String(0, 0) == "Thunderfury" && table.String(0, 3).empty());
    CHECK(table.Value(0, 1) == 4909 && table.Value(0, 2) == 2600 && table.Value(0, 4) == 5);
    CHECK(table.String(1, 0).empty() && table.String(1, 3) == "Plate");
    CHECK(table.Value(1, 1) == 7 && table.Value(1, 2) == 65535 && table.Value(1, 4) == 200);
}

TEST(Db2SparseIdsComeFromTheOffsetMapWithoutAnIdList) {
    Wdc5Spec spec = SparseSpec();
    Wdc5Section section;
    section.sparseRecords = { SparseRecord("A", 1, 1, "B", 1) };
    section.ids = { 777 };
    section.noIdList = true;
    spec.sections = { section };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error, SPARSE_STRINGS));
    CHECK(table.RowCount() == 1 && table.RowId(0) == 777 && table.String(0, 3) == "B");
}

TEST(Db2SparseCopiesAndLockedSections) {
    Wdc5Spec spec = SparseSpec();
    Wdc5Section open;
    open.sparseRecords = { SparseRecord("Kept", 11, 1, "", 1) };
    open.ids = { 10 };
    open.copies = { { 12, 10 } };
    Wdc5Section locked;
    locked.tactKey = 0x1122334455667788ull;
    locked.sparseRecords = { std::vector<uint8_t>(9, 0), std::vector<uint8_t>(9, 0) };
    locked.ids = { 20, 21 };
    spec.sections = { open, locked };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error, SPARSE_STRINGS));
    CHECK(table.RowCount() == 2 && table.RowId(1) == 12 && table.String(1, 0) == "Kept" && table.Value(1, 1) == 11);
    CHECK(table.SkippedRows() == 2);
}

TEST(Db2SparseRefusesBrokenOffsetMaps) {
    Wdc5Spec spec = SparseSpec();
    Wdc5Section section;
    section.sparseRecords = { SparseRecord("A", 1, 1, "B", 1) };
    section.ids = { 1 };
    spec.sections = { section };
    std::vector<uint8_t> file = BuildWdc5(spec);
    Db2Table table;
    std::string error;

    // The section header's records end, moved before its first record.
    std::vector<uint8_t> shortEnd = file;
    PutU32(shortEnd, 204 + 20, GetU32(shortEnd, 204 + 8) - 1);
    CHECK(!table.Parse(shortEnd, error, SPARSE_STRINGS));

    // A string column without its terminator inside the record reads as empty, not past the record.
    Wdc5Section unterminated;
    unterminated.sparseRecords = { std::vector<uint8_t>({ 'A', 'B', 'C' }) };
    unterminated.ids = { 2 };
    spec.sections = { unterminated };
    CHECK(table.Parse(BuildWdc5(spec), error, SPARSE_STRINGS));
    CHECK(table.String(0, 0).empty() && table.Value(0, 4) == 0);
}

// The real records of the pinned build. Columns: Display_lang 4, StatPercentageOfSocket 14, StatPercentEditor 15,
// StatModifier_bonusStat 16, ItemDelay 46, ItemLevel 50, RequiredLevel 64, InventoryType 65, OverallQualityID 66.
static Db2Table RealItemSparse() {
    Wdc5Spec spec;
    spec.flags = 0x5;
    spec.layoutHash = 0x1C17D17F;
    spec.recordSize = 328;
    spec.columns = ItemSparseColumns();
    Wdc5Section section;
    section.sparseRecords = { JudgmentBreastplateRecord(), OnyxiaScaleCloakRecord() };
    section.ids = { 16958, 15138 };
    spec.sections = { section };
    Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error, { 0, 1, 2, 3, 4 }));
    return table;
}

TEST(Db2SparseReadsRealItemSparseRows) {
    Db2Table table = RealItemSparse();
    CHECK(table.RowCount() == 2);
    if (table.RowCount() != 2) {
        return;
    }
    CHECK(table.String(0, 4) == "Judgment Breastplate");
    CHECK(table.Value(0, 50) == 29 && table.Value(0, 65) == 5 && table.Value(0, 66) == 4 && table.Value(0, 64) == 25);
    std::vector<uint32_t> stats = table.Values(0, 16);
    CHECK(stats.size() == 10 && stats[0] == 74 && stats[1] == 7 && stats[2] == 32 && stats[3] == 51 && stats[4] == 55 && stats[5] == 0xFFFFFFFF);
    std::vector<uint32_t> percents = table.Values(0, 15);
    CHECK(percents.size() == 10 && percents[0] == 4909 && percents[1] == 7889 && percents[2] == 2909 && percents[3] == 1818);
    CHECK(table.String(1, 4) == "Onyxia Scale Cloak" && table.Value(1, 65) == 16 && table.Value(1, 66) == 3);
    CHECK(table.Values(1, 16).size() == 10 && table.Values(1, 16)[0] == 7 && table.Values(1, 16)[1] == 51);
}

// ReadRow finds a sparse row's columns in one walk; its reads equal the table's column by column.
TEST(Db2RowsReadEveryColumnAsTheTableDoes) {
    Wdc5Spec spec = SparseSpec();
    Wdc5Section section;
    section.sparseRecords = { SparseRecord("Thunderfury", 4909, 2600, "", 5), SparseRecord("", 7, 65535, "Plate", 200),
        std::vector<uint8_t>({ 'A', 'B', 'C' }) };
    section.ids = { 19019, 16958, 3 };
    spec.sections = { section };
    Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error, SPARSE_STRINGS));
    for (size_t r = 0; r < table.RowCount(); r++) {
        Retail::Db2Row row = table.ReadRow(r);
        for (size_t c = 0; c < 6; c++) {
            CHECK(row.Value(c) == table.Value(r, c) && row.Values(c) == table.Values(r, c) && row.String(c) == table.String(r, c));
        }
    }
    CHECK(table.ReadRow(0).String(0) == "Thunderfury" && table.ReadRow(1).Value(4) == 200 && table.ReadRow(2).Value(1) == 0);

    Db2Table real = RealItemSparse();
    for (size_t r = 0; r < real.RowCount(); r++) {
        Retail::Db2Row row = real.ReadRow(r);
        for (size_t c = 0; c < 70; c++) {
            CHECK(row.Value(c) == real.Value(r, c) && row.Values(c) == real.Values(r, c) && row.String(c) == real.String(r, c));
        }
    }
}
