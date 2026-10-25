#include "Engine/Source/Retail/Db2Table.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/Wdc5Builder.h"

using Retail::Db2Table;

// Two plain columns (a 32-bit and a 16-bit one) in 6-byte records.
static Wdc5Spec PlainSpec() {
    Wdc5Spec spec;
    spec.recordSize = 6;
    spec.columns = {
        { 0, 0, 32, 0, 0, { 0, 0, 0 } },
        { 16, 32, 16, 0, 0, { 0, 0, 0 } },
    };
    return spec;
}

static std::vector<uint8_t> PlainRecord(uint32_t first, uint16_t second) {
    std::vector<uint8_t> record(6);
    std::memcpy(record.data(), &first, 4);
    std::memcpy(record.data() + 4, &second, 2);
    return record;
}

static std::vector<uint8_t> Join(std::vector<std::vector<uint8_t>> records) {
    std::vector<uint8_t> joined;
    for (const auto& record : records) {
        joined.insert(joined.end(), record.begin(), record.end());
    }
    return joined;
}

TEST(Db2PlainColumnsAndIdList) {
    Wdc5Spec spec = PlainSpec();
    spec.layoutHash = 0xBBF66A3C;
    spec.sections = { { 0, Join({ PlainRecord(111, 7), PlainRecord(222, 65535) }), { 10, 20 }, {} } };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.LayoutHash() == 0xBBF66A3C);
    CHECK(table.RowCount() == 2);
    CHECK(table.RowCount() == 2 && table.RowId(0) == 10 && table.RowId(1) == 20);
    CHECK(table.RowCount() == 2 && table.Value(0, 0) == 111 && table.Value(0, 1) == 7);
    CHECK(table.RowCount() == 2 && table.Value(1, 0) == 222 && table.Value(1, 1) == 65535);
}

TEST(Db2InlineIdsWithoutAnIdList) {
    Wdc5Spec spec = PlainSpec();
    spec.idIndex = 0;
    spec.sections = { { 0, Join({ PlainRecord(5000, 1), PlainRecord(5001, 2) }), {}, {} } };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.RowCount() == 2 && table.RowId(0) == 5000 && table.RowId(1) == 5001);
}

TEST(Db2PlainArrays) {
    Wdc5Spec spec;
    spec.recordSize = 8;
    spec.columns = { { 0, 0, 64, 0, 0, { 0, 0, 0 } } };
    std::vector<uint8_t> record(8);
    uint32_t values[2] = { 0x10, 0x20 };
    std::memcpy(record.data(), values, 8);
    spec.sections = { { 0, record, { 1 }, {} } };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.RowCount() == 1 && table.Values(0, 0) == std::vector<uint32_t>({ 0x10, 0x20 }));
}

TEST(Db2BitpackedColumnsAndSignExtension) {
    Wdc5Spec spec;
    spec.recordSize = 2;
    spec.columns = {
        { 0, 0, 5, 0, 1, { 0, 5, 0 } },
        { 0, 5, 4, 0, 5, { 5, 4, 1 } },
        { 0, 9, 7, 0, 1, { 9, 7, 0 } },
    };
    std::vector<uint8_t> record(2);
    PackBits(record, 0, 5, 19);
    PackBits(record, 5, 4, 0xD);    // -3 in 4 bits.
    PackBits(record, 9, 7, 100);
    spec.sections = { { 0, record, { 1 }, {} } };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.RowCount() == 1 && table.Value(0, 0) == 19);
    CHECK(table.RowCount() == 1 && static_cast<int32_t>(table.Value(0, 1)) == -3);
    CHECK(table.RowCount() == 1 && table.Value(0, 2) == 100);
}

TEST(Db2PalletCommonAndPalletArrayInColumnOrder) {
    // The pallet array comes first: pallet data belongs to columns in column order.
    Wdc5Spec spec;
    spec.recordSize = 1;
    spec.columns = {
        { 0, 0, 1, 16, 4, { 0, 1, 2 } },
        { 0, 1, 2, 12, 3, { 1, 2, 0 } },
        { 0, 0, 0, 8, 2, { 7, 0, 0 } },
    };
    spec.extra = { 1, 2, 3, 4, 100, 200, 300, 20, 9 };
    std::vector<uint8_t> first(1), second(1);
    PackBits(first, 0, 1, 1);
    PackBits(first, 1, 2, 2);
    PackBits(second, 1, 2, 0);
    spec.sections = { { 0, Join({ first, second }), { 10, 20 }, {} } };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.RowCount() == 2 && table.Values(0, 0) == std::vector<uint32_t>({ 3, 4 }));
    CHECK(table.RowCount() == 2 && table.Values(1, 0) == std::vector<uint32_t>({ 1, 2 }));
    CHECK(table.RowCount() == 2 && table.Value(0, 1) == 300 && table.Value(1, 1) == 100);
    CHECK(table.RowCount() == 2 && table.Value(0, 2) == 7 && table.Value(1, 2) == 9);
}

TEST(Db2CopyTableAddsRows) {
    Wdc5Spec spec = PlainSpec();
    spec.sections = { { 0, PlainRecord(42, 3), { 1 }, { { 99, 1 } } } };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.RowCount() == 2 && table.RowId(1) == 99 && table.Value(1, 0) == 42);
}

TEST(Db2RowsReadTheirRelationFromTheRelationshipMap) {
    // Records 0 and 2 relate to parents 100 and 200; the copy of record 2 takes its source's relation.
    Wdc5Spec spec = PlainSpec();
    Wdc5Section section{ 0, Join({ PlainRecord(1, 1), PlainRecord(2, 2), PlainRecord(3, 3) }), { 10, 20, 30 }, { { 40, 30 } } };
    section.relations = { { 100, 0 }, { 200, 2 } };
    spec.sections = { section };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.RowCount() == 4);
    CHECK(table.Relation(0) == 100 && table.Relation(1) == 0 && table.Relation(2) == 200);
    CHECK(table.RowId(3) == 40 && table.Relation(3) == 200);
}

TEST(Db2EncryptedZeroSectionsAreSkipped) {
    Wdc5Spec spec = PlainSpec();
    spec.sections = {
        { 0, PlainRecord(1, 1), { 1 }, {} },
        { 0x1122334455667788ull, std::vector<uint8_t>(12, 0), { 0, 0 }, {} },
        { 0, PlainRecord(3, 3), { 3 }, {} },
    };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.RowCount() == 2 && table.RowId(0) == 1 && table.RowId(1) == 3);
    CHECK(table.SkippedRows() == 2);
}

TEST(Db2TablesWithoutLockedSectionsSkipNothing) {
    Wdc5Spec spec = PlainSpec();
    spec.sections = { { 0, PlainRecord(1, 1), { 1 }, {} } };
    Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.SkippedRows() == 0);
}

TEST(Db2RefusesBrokenFiles) {
    Db2Table table;
    std::string error;
    Wdc5Spec spec = PlainSpec();
    spec.sections = { { 0, PlainRecord(1, 1), { 1 }, {} } };
    std::vector<uint8_t> good = BuildWdc5(spec);

    std::vector<uint8_t> magic = good;
    magic[3] = '4';
    CHECK(!table.Parse(magic, error));
    CHECK(!error.empty());

    std::vector<uint8_t> truncated(good.begin(), good.end() - 3);
    CHECK(!table.Parse(truncated, error));

    CHECK(!table.Parse(std::vector<uint8_t>(100, 0), error));

    Wdc5Spec zeroed = spec;
    zeroed.sections = { { 0, std::vector<uint8_t>(6, 0), { 1 }, {} } };
    CHECK(!table.Parse(BuildWdc5(zeroed), error));
}

// Strings: a string column holds the distance from the field to its string, as if every section's records came first
// and every section's string block after them (DBCD WDC5Reader).
static std::vector<uint8_t> StringRecord(uint32_t offset, uint32_t value) {
    std::vector<uint8_t> record(8);
    std::memcpy(record.data(), &offset, 4);
    std::memcpy(record.data() + 4, &value, 4);
    return record;
}

static Wdc5Spec StringSpec() {
    Wdc5Spec spec;
    spec.recordSize = 8;
    spec.columns = { { 0, 0, 32, 0, 0, { 0, 0, 0 } }, { 0, 32, 32, 0, 0, { 0, 0, 0 } } };
    return spec;
}

static std::vector<uint8_t> Text(const char* text, size_t size) {
    return std::vector<uint8_t>(text, text + size);
}

TEST(Db2StringsReadFromTheSectionsStringBlock) {
    // Two records, block "\0Wo\0Worgen\0": record 0's string at 1 ("Wo"), record 1's at 4 ("Worgen"), 0 for none.
    Wdc5Spec spec = StringSpec();
    Wdc5Section section{ 0, Join({ StringRecord(2 * 8 + 1 - 0, 7), StringRecord(2 * 8 + 4 - 8, 8) }), { 22, 32 }, {} };
    section.strings = Text("\0Wo\0Worgen\0", 11);
    spec.sections = { section };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.String(0, 0) == "Wo" && table.String(1, 0) == "Worgen");
    CHECK(table.Value(1, 1) == 8);
    CHECK(table.String(5, 0).empty() && table.String(0, 9).empty());

    spec.sections[0].records = StringRecord(0, 7);
    spec.sections[0].ids = { 22 };
    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.String(0, 0).empty());
}

TEST(Db2StringsOfALaterSectionCountTheEarlierBlocks) {
    // Records 0 (section 0) and 1 (section 1); 2 records of 8 bytes in all; block 0 "\0Hu\0" (4), block 1 "\0Kt\0".
    Wdc5Spec spec = StringSpec();
    Wdc5Section first{ 0, StringRecord(2 * 8 + 1 - 0, 1), { 1 }, {} };
    first.strings = Text("\0Hu\0", 4);
    Wdc5Section second{ 0, StringRecord(2 * 8 + 4 + 1 - 8, 2), { 32 }, {} };
    second.strings = Text("\0Kt\0", 4);
    spec.sections = { first, second };
    Db2Table table;
    std::string error;

    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.String(0, 0) == "Hu" && table.String(1, 0) == "Kt");
}

TEST(Db2RowsOfAPlainTableReadAsTheTableDoes) {
    Wdc5Spec spec = PlainSpec();
    spec.sections = { { 0, Join({ PlainRecord(111, 7), PlainRecord(222, 65535) }), { 10, 20 }, {} } };
    Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error));
    CHECK(table.ReadRow(1).Value(0) == 222 && table.ReadRow(1).Value(1) == 65535 && table.ReadRow(0).Values(1) == std::vector<uint32_t>({ 7 }));
    CHECK(table.ReadRow(0).Value(2) == 0 && table.ReadRow(0).Values(2).empty());
}
