#ifndef TOOLS_TESTS_ITEMFIXTURES_H
#define TOOLS_TESTS_ITEMFIXTURES_H

#include "Engine/Source/Retail/ItemTables.h"
#include "Tools/Tests/Db2Fixtures.h"

// The retail item tables, with the columns the loaders read at their WoWDBDefs physical numbers.

// GeoBox[6] is column 0, the FileDataID (inline id) 1 and ModelResourcesID 4.
inline Retail::Db2Table ModelFiles(const std::vector<std::pair<uint32_t, uint32_t>>& files, uint32_t layout = Retail::MODEL_FILE_DATA_LAYOUT) {
    std::vector<Db2Row> rows;
    for (const auto& [file, resource] : files) {
        std::vector<uint32_t> values(6, 0);
        values.insert(values.end(), { file, 0, 0, resource });
        rows.push_back({ file, values });
    }
    return PlainTable(layout, { 6, 1, 1, 1, 1 }, rows);
}

struct ItemDisplayRow {
    uint32_t id;
    std::vector<uint32_t> resources;      // Two model resources, 0 for none.
    std::vector<uint32_t> materials;      // Each side's object skin material resource.
    std::vector<uint32_t> geosetGroups;   // Up to six.
};

// ModelResourcesID[2] is column 10, ModelMaterialResourcesID[2] 11, ModelType[2] 12 and GeosetGroup[6] 13.
inline Retail::Db2Table ItemDisplays(const std::vector<ItemDisplayRow>& displays, uint32_t layout = Retail::ITEM_DISPLAY_INFO_LAYOUT) {
    std::vector<uint32_t> widths(10, 1);
    widths.insert(widths.end(), { 2, 2, 2, 6 });
    std::vector<Db2Row> rows;
    for (const ItemDisplayRow& display : displays) {
        std::vector<uint32_t> resources = display.resources;
        std::vector<uint32_t> materials = display.materials;
        std::vector<uint32_t> groups = display.geosetGroups;
        resources.resize(2, 0);
        materials.resize(2, 0);
        groups.resize(6, 0);
        std::vector<uint32_t> values(10, 0);
        values.insert(values.end(), resources.begin(), resources.end());
        values.insert(values.end(), materials.begin(), materials.end());
        values.insert(values.end(), { 0, 0 });
        values.insert(values.end(), groups.begin(), groups.end());
        rows.push_back({ display.id, values });
    }
    return PlainTable(layout, widths, rows);
}

struct TextureFileRow {
    uint32_t file;
    uint32_t usage;
    uint32_t material;
};

// UsageType is column 1 and MaterialResourcesID 2.
inline Retail::Db2Table TextureFiles(const std::vector<TextureFileRow>& files, uint32_t layout = Retail::TEXTURE_FILE_DATA_LAYOUT) {
    std::vector<Db2Row> rows;
    for (const TextureFileRow& file : files) {
        rows.push_back({ file.file, { 0, file.usage, file.material } });
    }
    return PlainTable(layout, { 1, 1, 1 }, rows);
}

struct ComponentModelRow {
    uint32_t file;
    uint32_t gender;   // 0 male, 1 female, 2 either.
    uint32_t race;     // 0 for every race.
    uint32_t position;
};

// GenderIndex is column 0, ClassID 1, RaceID 2 and PositionIndex 3 (the id, the file, is not inline).
inline Retail::Db2Table ComponentModels(const std::vector<ComponentModelRow>& files, uint32_t layout = Retail::COMPONENT_MODEL_FILE_DATA_LAYOUT) {
    std::vector<Db2Row> rows;
    for (const ComponentModelRow& file : files) {
        rows.push_back({ file.file, { file.gender, 0, file.race, file.position } });
    }
    return PlainTable(layout, { 1, 1, 1, 1 }, rows);
}

struct AppearanceRow {
    uint32_t id;
    uint32_t display;
    uint32_t icon;
};

// ItemDisplayInfoID is column 1 and DefaultIconFileDataID 2 (the id is not inline).
inline Retail::Db2Table Appearances(const std::vector<AppearanceRow>& appearances, uint32_t layout = Retail::ITEM_APPEARANCE_LAYOUT) {
    std::vector<Db2Row> rows;
    for (const AppearanceRow& appearance : appearances) {
        rows.push_back({ appearance.id, { 0, appearance.display, appearance.icon, 0, 0 } });
    }
    return PlainTable(layout, { 1, 1, 1, 1, 1 }, rows);
}

struct SectionRow {
    uint32_t display;
    uint32_t section;
    uint32_t material;
};

// ComponentSection is column 0 and MaterialResourcesID 1; the display is the row's relation.
inline Retail::Db2Table Sections(const std::vector<SectionRow>& sections, uint32_t layout = Retail::ITEM_DISPLAY_INFO_MATERIAL_RES_LAYOUT) {
    std::vector<Db2Row> rows;
    std::vector<uint32_t> relations;
    for (uint32_t i = 0; i < sections.size(); i++) {
        rows.push_back({ i + 1, { sections[i].section, sections[i].material } });
        relations.push_back(sections[i].display);
    }
    return PlainTable(layout, { 1, 1 }, rows, relations);
}

struct ComponentTextureRow {
    uint32_t file;
    uint32_t gender;   // 0 male, 1 female, 2 either.
    uint32_t race;     // 0 for every race.
};

// GenderIndex is column 0, ClassID 1 and RaceID 2 (the id, the file, is not inline).
inline Retail::Db2Table ComponentTextures(const std::vector<ComponentTextureRow>& files, uint32_t layout = Retail::COMPONENT_TEXTURE_FILE_DATA_LAYOUT) {
    std::vector<Db2Row> rows;
    for (const ComponentTextureRow& file : files) {
        rows.push_back({ file.file, { file.gender, 0, file.race } });
    }
    return PlainTable(layout, { 1, 1, 1 }, rows);
}

#endif
