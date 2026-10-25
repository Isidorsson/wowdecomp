#ifndef TOOLS_TESTS_WDBCFIXTURES_H
#define TOOLS_TESTS_WDBCFIXTURES_H

#include <string>
#include <vector>
#include "Tools/Tests/Bytes.h"

// A WDBC file of 32-bit fields: rows of field values, then the string block as given (it starts with a NUL).
inline std::vector<uint8_t> WdbcBytes(uint32_t fieldCount, const std::vector<std::vector<uint32_t>>& rows, const std::string& strings = std::string(1, '\0')) {
    std::vector<uint8_t> file = { 'W', 'D', 'B', 'C' };
    AppendU32(file, static_cast<uint32_t>(rows.size()));
    AppendU32(file, fieldCount);
    AppendU32(file, fieldCount * 4);
    AppendU32(file, static_cast<uint32_t>(strings.size()));
    for (const std::vector<uint32_t>& row : rows) {
        for (uint32_t field = 0; field < fieldCount; field++) {
            AppendU32(file, field < row.size() ? row[field] : 0);
        }
    }
    file.insert(file.end(), strings.begin(), strings.end());
    return file;
}

#endif
