#include "Engine/Source/Retail/RetailDisplays.h"
#include <algorithm>
#include <cstring>

namespace Retail {

// Physical columns of the layouts (WoWDBDefs).
constexpr size_t MODEL_GEO_BOX = 0;
constexpr size_t MODEL_FILE_DATA_ID = 2;
constexpr size_t MODEL_SIZE_CLASS = 14;
constexpr size_t MODEL_COLLISION_WIDTH = 15;
constexpr size_t MODEL_COLLISION_HEIGHT = 16;
constexpr size_t MODEL_SCALE = 21;
constexpr size_t MODEL_MOUNT_HEIGHT = 25;
constexpr size_t DISPLAY_MODEL_ID = 1;
constexpr size_t DISPLAY_SIZE_CLASS = 3;
constexpr size_t DISPLAY_SCALE = 4;
constexpr size_t DISPLAY_ALPHA = 5;
constexpr size_t DISPLAY_EXTENDED = 7;
constexpr size_t DISPLAY_TEXTURE_VARIATIONS = 27;

constexpr uint32_t NIBBLE_GROUPS = 8;
constexpr uint32_t NIBBLE_BITS = 4;
constexpr uint32_t HIDDEN = 0xF;

static float AsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

template <typename Row>
static const Row* FindById(const std::vector<Row>& rows, uint32_t id) {
    auto row = std::lower_bound(rows.begin(), rows.end(), id, [](const Row& r, uint32_t key) { return r.id < key; });
    return row != rows.end() && row->id == id ? &*row : nullptr;
}

template <typename Row>
static void SortById(std::vector<Row>& rows) {
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.id < b.id; });
}

uint32_t SyntheticModelId(uint32_t retailModelId) {
    return SYNTHETIC_MODEL_ID_BASE + retailModelId;
}

bool RetailModelIdOf(uint32_t syntheticModelId, uint32_t& retailModelId) {
    if (syntheticModelId < SYNTHETIC_MODEL_ID_BASE || syntheticModelId >= SYNTHETIC_NPC_BODY_MODEL_ID_BASE) {
        return false;
    }
    retailModelId = syntheticModelId - SYNTHETIC_MODEL_ID_BASE;
    return true;
}

std::string CreatureModelName(uint32_t fileDataId) {
    return "retail\\" + std::to_string(fileDataId) + ".m2";
}

std::string VariationName(uint32_t fileDataId) {
    return fileDataId ? std::to_string(fileDataId) : std::string();
}

uint32_t PackGeosetData(const std::vector<Geoset>& geosets) {
    if (geosets.empty()) {
        return 0;
    }

    uint32_t packed = 0;
    for (uint32_t group = 0; group < NIBBLE_GROUPS; group++) {
        packed |= HIDDEN << (NIBBLE_BITS * group);
    }
    for (const Geoset& geoset : geosets) {
        if (geoset.index < NIBBLE_GROUPS && geoset.value >= 1 && geoset.value <= HIDDEN) {
            uint32_t shift = NIBBLE_BITS * geoset.index;
            packed = (packed & ~(HIDDEN << shift)) | (geoset.value << shift);
        }
    }
    return packed;
}

static RetailCreatureModel ModelAt(const Db2Table& table, size_t row) {
    RetailCreatureModel model;
    model.id = table.RowId(row);
    model.fileDataId = table.Value(row, MODEL_FILE_DATA_ID);
    std::vector<uint32_t> box = table.Values(row, MODEL_GEO_BOX);
    for (size_t i = 0; i < box.size() && i < model.geoBox.size(); i++) {
        model.geoBox[i] = AsFloat(box[i]);
    }
    model.collisionWidth = AsFloat(table.Value(row, MODEL_COLLISION_WIDTH));
    model.collisionHeight = AsFloat(table.Value(row, MODEL_COLLISION_HEIGHT));
    model.modelScale = AsFloat(table.Value(row, MODEL_SCALE));
    model.mountHeight = AsFloat(table.Value(row, MODEL_MOUNT_HEIGHT));
    model.sizeClass = static_cast<int8_t>(table.Value(row, MODEL_SIZE_CLASS));
    return model;
}

static RetailDisplay DisplayAt(const Db2Table& table, size_t row, const std::map<uint32_t, std::vector<Geoset>>& geosetsOf) {
    RetailDisplay display;
    display.id = table.RowId(row);
    display.modelId = table.Value(row, DISPLAY_MODEL_ID);
    display.scale = AsFloat(table.Value(row, DISPLAY_SCALE));
    display.alpha = table.Value(row, DISPLAY_ALPHA);
    display.extendedDisplayId = table.Value(row, DISPLAY_EXTENDED);
    display.sizeClass = static_cast<int8_t>(table.Value(row, DISPLAY_SIZE_CLASS));
    std::vector<uint32_t> files = table.Values(row, DISPLAY_TEXTURE_VARIATIONS);
    for (size_t i = 0; i < files.size() && i < display.variations.size(); i++) {
        display.variations[i] = files[i];
    }
    auto shown = geosetsOf.find(display.id);
    display.geosetData = shown == geosetsOf.end() ? 0 : PackGeosetData(shown->second);
    return display;
}

bool RetailDisplays::Load(const CreatureTables& tables, std::string& error) {
    if (!KnownLayout(tables.modelData, CREATURE_MODEL_DATA_LAYOUT, "CreatureModelData", error)
        || !KnownLayout(tables.displayInfo, CREATURE_DISPLAY_INFO_LAYOUT, "CreatureDisplayInfo", error)
        || !KnownLayout(tables.geosets, CREATURE_DISPLAY_INFO_GEOSET_DATA_LAYOUT, "CreatureDisplayInfoGeosetData", error)) {
        return false;
    }

    this->m_models.clear();
    for (size_t row = 0; row < tables.modelData.RowCount(); row++) {
        this->m_models.push_back(ModelAt(tables.modelData, row));
    }
    SortById(this->m_models);

    std::map<uint32_t, std::vector<Geoset>> geosetsOf = GeosetsByDisplay(tables.geosets);
    this->m_displays.clear();
    for (size_t row = 0; row < tables.displayInfo.RowCount(); row++) {
        RetailDisplay display = DisplayAt(tables.displayInfo, row, geosetsOf);
        if (this->Model(display.modelId)) {
            this->m_displays.push_back(display);
        }
    }
    SortById(this->m_displays);
    return true;
}

const RetailDisplay* RetailDisplays::Display(uint32_t id) const {
    return FindById(this->m_displays, id);
}

const RetailCreatureModel* RetailDisplays::Model(uint32_t id) const {
    return FindById(this->m_models, id);
}

uint32_t SyntheticNpcBodyModelId(uint32_t retailModelId) {
    return SYNTHETIC_NPC_BODY_MODEL_ID_BASE + retailModelId;
}

bool RetailNpcBodyModelIdOf(uint32_t syntheticModelId, uint32_t& retailModelId) {
    if (syntheticModelId < SYNTHETIC_NPC_BODY_MODEL_ID_BASE) {
        return false;
    }
    retailModelId = syntheticModelId - SYNTHETIC_NPC_BODY_MODEL_ID_BASE;
    return true;
}

}
