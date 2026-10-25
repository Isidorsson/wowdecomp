#include <cstring>
#include "Tools/RetailServerDisplays/Wdbc.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/WdbcFixtures.h"

using ServerDisplays::Wdbc;

static Wdbc Parsed(const std::vector<uint8_t>& bytes) {
    Wdbc table;
    std::string error;
    if (!table.Parse(bytes, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return table;
}

TEST(WdbcReadsRowsFieldsAndStrings) {
    std::string strings("\0a\\b.m2\0", 8);
    Wdbc table = Parsed(WdbcBytes(3, { { 7, 1, 1 }, { 9, 2, 0 } }, strings));

    CHECK(table.FieldCount() == 3 && table.RowCount() == 2);
    CHECK(table.Id(1) == 9 && table.Field(1, 1) == 2);
    CHECK(!std::strcmp(table.String(table.Field(0, 2)), "a\\b.m2"));
}

TEST(WdbcRejectsOtherMagicAndTruncatedFiles) {
    std::vector<uint8_t> bytes = WdbcBytes(2, { { 1, 2 } });
    std::vector<uint8_t> wrongMagic = bytes;
    wrongMagic[0] = 'X';
    std::vector<uint8_t> truncated(bytes.begin(), bytes.end() - 3);
    Wdbc table;
    std::string error;

    CHECK(!table.Parse(wrongMagic, error) && !error.empty());
    CHECK(!table.Parse(truncated, error));
    CHECK(!table.Parse({ 'W', 'D', 'B', 'C' }, error));
}

TEST(WdbcRejectsRecordsThatAreNotWholeFields) {
    std::vector<uint8_t> bytes = WdbcBytes(2, { { 1, 2 } });
    PutU32(bytes, 12, 9);
    Wdbc table;
    std::string error;

    CHECK(!table.Parse(bytes, error));
}

TEST(WdbcSerializesUnchanged) {
    std::vector<uint8_t> bytes = WdbcBytes(2, { { 3, 1 }, { 1, 0 } }, std::string("\0x\0", 3));

    CHECK(Parsed(bytes).Serialize() == bytes);
}

TEST(WdbcAppendsRowsAndStrings) {
    Wdbc table = Parsed(WdbcBytes(2, { { 3, 0 } }));
    uint32_t name = table.AddString("retail\\1.m2");

    CHECK(table.AppendRow({ 40, name }));
    Wdbc reread = Parsed(table.Serialize());
    CHECK(reread.RowCount() == 2 && reread.Id(1) == 40);
    CHECK(!std::strcmp(reread.String(reread.Field(1, 1)), "retail\\1.m2"));
    CHECK(!std::strcmp(reread.String(0), ""));
}

TEST(WdbcAddsAStringOnceAndTheEmptyStringAtZero) {
    Wdbc table = Parsed(WdbcBytes(1, {}));
    uint32_t first = table.AddString("a");

    CHECK(table.AddString("a") == first);
    CHECK(table.AddString("") == 0);
}

TEST(WdbcRefusesARowOfTheWrongWidth) {
    Wdbc table = Parsed(WdbcBytes(2, {}));

    CHECK(!table.AppendRow({ 1 }));
    CHECK(!table.AppendRow({ 1, 2, 3 }));
    CHECK(table.RowCount() == 0);
}

TEST(WdbcFindsIdsAndTheLargest) {
    Wdbc table = Parsed(WdbcBytes(1, { { 5 }, { 33107 }, { 12 } }));

    CHECK(table.HasId(12) && !table.HasId(13));
    CHECK(table.MaxId() == 33107);
    CHECK(Parsed(WdbcBytes(1, {})).MaxId() == 0);
}
