#ifndef TOOLS_TESTS_DB2FIXTURES_H
#define TOOLS_TESTS_DB2FIXTURES_H

#include <utility>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/Wdc5Builder.h"

// Retail tables built from rows of plain 32-bit columns, with their physical column numbers as the real layouts
// have them.

using Db2Row = std::pair<uint32_t, std::vector<uint32_t>>;   // Id, then every column's values in order.

// widths[c] is how many values column c holds; relations[r] is row r's parent (0 for none).
inline Retail::Db2Table PlainTable(uint32_t layout, const std::vector<uint32_t>& widths, const std::vector<Db2Row>& rows, const std::vector<uint32_t>& relations = {}) {
    Wdc5Spec spec;
    spec.layoutHash = layout;
    uint16_t offset = 0;
    for (uint32_t width : widths) {
        spec.columns.push_back({ 0, offset, static_cast<uint16_t>(width * 32), 0, 0, { 0, 0, 0 } });
        offset = static_cast<uint16_t>(offset + width * 32);
    }
    spec.recordSize = offset / 8;

    Wdc5Section section;
    for (uint32_t r = 0; r < rows.size(); r++) {
        section.ids.push_back(rows[r].first);
        for (uint32_t value : rows[r].second) {
            AppendU32(section.records, value);
        }
        if (r < relations.size() && relations[r]) {
            section.relations.push_back({ relations[r], r });
        }
    }
    spec.sections = { section };

    Retail::Db2Table table;
    std::string error;
    bool parsed = table.Parse(BuildWdc5(spec), error);
    CHECK(parsed);
    if (!parsed) {
        std::printf("    (fixture: %s)\n", error.c_str());
    }
    return table;
}

#endif
