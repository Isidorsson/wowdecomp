#ifndef TOOLS_TESTS_WDC5BUILDER_H
#define TOOLS_TESTS_WDC5BUILDER_H

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>
#include "Tools/Tests/Bytes.h"

// Builds WDC5 files byte by byte: the 204-byte header, 40-byte section headers, 4-byte field meta,
// 24-byte column meta, pallet and common data, then each section's records, ids and copy table.

struct Wdc5Column {
    int16_t bits;            // Field meta: 32 - plain element width.
    uint16_t offsetBits;
    uint16_t sizeBits;
    uint32_t extraBytes;     // Pallet or common data this column owns.
    uint32_t compression;    // 0 none, 1 bit-packed, 2 common, 3 pallet, 4 pallet array, 5 signed bit-packed.
    uint32_t values[3];      // Bit-packed: offset, width, flags. Common: default. Pallet: offset, width, cardinality.
};

struct Wdc5Section {
    uint64_t tactKey = 0;
    std::vector<uint8_t> records;
    std::vector<uint32_t> ids;
    std::vector<std::pair<uint32_t, uint32_t>> copies;  // New id, source id.
    std::vector<std::pair<uint32_t, uint32_t>> relations;  // Foreign id, record index in the section.
};

struct Wdc5Spec {
    uint32_t layoutHash = 0x12345678;
    uint16_t flags = 0;
    uint16_t idIndex = 0;
    uint32_t recordSize = 0;
    std::vector<Wdc5Column> columns;
    std::vector<uint32_t> extra;       // Pallet then common data, as the columns own it.
    std::vector<Wdc5Section> sections;
};

inline std::vector<uint8_t> BuildWdc5(const Wdc5Spec& spec) {
    std::vector<uint8_t> file(204, 0);
    std::memcpy(file.data(), "WDC5", 4);
    PutU32(file, 4, 5);

    uint32_t records = 0;
    for (const auto& section : spec.sections) {
        records += static_cast<uint32_t>(section.records.size() / spec.recordSize);
    }

    PutU32(file, 136, records);
    PutU32(file, 140, static_cast<uint32_t>(spec.columns.size()));
    PutU32(file, 144, spec.recordSize);
    PutU32(file, 156, spec.layoutHash);
    PutU16(file, 172, spec.flags);
    PutU16(file, 174, spec.idIndex);
    PutU32(file, 176, static_cast<uint32_t>(spec.columns.size()));
    PutU32(file, 188, static_cast<uint32_t>(spec.columns.size() * 24));
    PutU32(file, 200, static_cast<uint32_t>(spec.sections.size()));

    size_t sectionHeaders = file.size();
    file.resize(file.size() + spec.sections.size() * 40, 0);

    for (const auto& column : spec.columns) {
        file.resize(file.size() + 4, 0);
        PutU16(file, file.size() - 4, static_cast<uint16_t>(column.bits));
    }

    for (const auto& column : spec.columns) {
        size_t at = file.size();
        file.resize(at + 24, 0);
        PutU16(file, at, column.offsetBits);
        PutU16(file, at + 2, column.sizeBits);
        PutU32(file, at + 4, column.extraBytes);
        PutU32(file, at + 8, column.compression);
        PutU32(file, at + 12, column.values[0]);
        PutU32(file, at + 16, column.values[1]);
        PutU32(file, at + 20, column.values[2]);
    }

    for (uint32_t value : spec.extra) {
        AppendU32(file, value);
    }

    for (size_t s = 0; s < spec.sections.size(); s++) {
        const Wdc5Section& section = spec.sections[s];
        size_t header = sectionHeaders + s * 40;
        std::memcpy(file.data() + header, &section.tactKey, 8);
        PutU32(file, header + 8, static_cast<uint32_t>(file.size()));
        PutU32(file, header + 12, static_cast<uint32_t>(section.records.size() / spec.recordSize));
        PutU32(file, header + 24, static_cast<uint32_t>(section.ids.size() * 4));
        PutU32(file, header + 36, static_cast<uint32_t>(section.copies.size()));

        file.insert(file.end(), section.records.begin(), section.records.end());
        for (uint32_t id : section.ids) {
            AppendU32(file, id);
        }
        for (const auto& copy : section.copies) {
            AppendU32(file, copy.first);
            AppendU32(file, copy.second);
        }

        // Relationship map: count, min and max foreign id, then (foreign id, record index) entries.
        if (!section.relations.empty()) {
            PutU32(file, header + 28, static_cast<uint32_t>(12 + section.relations.size() * 8));
            uint32_t low = UINT32_MAX;
            uint32_t high = 0;
            for (const auto& relation : section.relations) {
                low = relation.first < low ? relation.first : low;
                high = relation.first > high ? relation.first : high;
            }
            AppendU32(file, static_cast<uint32_t>(section.relations.size()));
            AppendU32(file, low);
            AppendU32(file, high);
            for (const auto& relation : section.relations) {
                AppendU32(file, relation.first);
                AppendU32(file, relation.second);
            }
        }
    }

    return file;
}

#endif
