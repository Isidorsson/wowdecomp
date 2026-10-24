#ifndef TOOLS_TESTS_CREATUREFIXTURES_H
#define TOOLS_TESTS_CREATUREFIXTURES_H

#include <array>
#include <cstring>
#include "Engine/Source/Retail/CreatureTables.h"
#include "Tools/Tests/Db2Fixtures.h"

// The retail creature tables, with the columns the loaders read at their WoWDBDefs physical numbers.

inline uint32_t FloatBits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

struct CreatureModelRow {
    uint32_t id;
    uint32_t file;
    std::array<float, 6> geoBox = {};
    float collisionWidth = 0;
    float collisionHeight = 0;
    float modelScale = 1;
    float mountHeight = 0;
    uint32_t sizeClass = 0;
};

// GeoBox[6] is column 0, FileDataID 2, SizeClass 14, CollisionWidth and Height 15 and 16, ModelScale 21 and
// MountHeight 25 (the id is not inline).
inline Retail::Db2Table CreatureModels(const std::vector<CreatureModelRow>& models, uint32_t layout = Retail::CREATURE_MODEL_DATA_LAYOUT) {
    std::vector<uint32_t> widths(26, 1);
    widths[0] = 6;
    std::vector<Db2Row> rows;
    for (const CreatureModelRow& model : models) {
        std::vector<uint32_t> values;
        for (float bound : model.geoBox) {
            values.push_back(FloatBits(bound));
        }
        values.resize(31, 0);   // Six bounds, then columns 1 to 25.
        values[5 + 2] = model.file;
        values[5 + 14] = model.sizeClass;
        values[5 + 15] = FloatBits(model.collisionWidth);
        values[5 + 16] = FloatBits(model.collisionHeight);
        values[5 + 21] = FloatBits(model.modelScale);
        values[5 + 25] = FloatBits(model.mountHeight);
        rows.push_back({ model.id, values });
    }
    return PlainTable(layout, widths, rows);
}

struct CreatureDisplayRow {
    uint32_t id;
    uint32_t model;
    std::vector<uint32_t> variations;   // Up to four texture FileDataIDs, 0 for none.
    float scale = 1;
    uint32_t alpha = 255;
    uint32_t sizeClass = 0;
};

// The id is column 0, ModelID 1, SizeClass 3, CreatureModelScale 4, CreatureModelAlpha 5 and
// TextureVariationFileDataID[4] 27.
inline Retail::Db2Table CreatureDisplays(const std::vector<CreatureDisplayRow>& displays, uint32_t layout = Retail::CREATURE_DISPLAY_INFO_LAYOUT) {
    std::vector<uint32_t> widths(27, 1);
    widths.push_back(4);
    std::vector<Db2Row> rows;
    for (const CreatureDisplayRow& display : displays) {
        std::vector<uint32_t> values(27, 0);
        values[0] = display.id;
        values[1] = display.model;
        values[3] = display.sizeClass;
        values[4] = FloatBits(display.scale);
        values[5] = display.alpha;
        std::vector<uint32_t> variations = display.variations;
        variations.resize(4, 0);
        values.insert(values.end(), variations.begin(), variations.end());
        rows.push_back({ display.id, values });
    }
    return PlainTable(layout, widths, rows);
}

struct GeosetRow {
    uint32_t display;
    uint32_t index;
    uint32_t value;
};

// GeosetIndex is column 0 and GeosetValue 1; the display is the row's relation.
inline Retail::Db2Table DisplayGeosets(const std::vector<GeosetRow>& geosets, uint32_t layout = Retail::CREATURE_DISPLAY_INFO_GEOSET_DATA_LAYOUT) {
    std::vector<Db2Row> rows;
    std::vector<uint32_t> relations;
    for (uint32_t i = 0; i < geosets.size(); i++) {
        rows.push_back({ i + 1, { geosets[i].index, geosets[i].value } });
        relations.push_back(geosets[i].display);
    }
    return PlainTable(layout, { 1, 1 }, rows, relations);
}

#endif
