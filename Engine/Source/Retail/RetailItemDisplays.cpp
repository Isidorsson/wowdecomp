#include "Engine/Source/Retail/RetailItemDisplays.h"
#include <algorithm>
#include <map>

namespace Retail {

// Physical columns of the layouts (WoWDBDefs, as wow-toolbox-editor casc/gear.rs reads them).
constexpr size_t DISPLAY_RESOURCES = 10;
constexpr size_t DISPLAY_MATERIALS = 11;
constexpr size_t DISPLAY_GEOSET_GROUPS = 13;
constexpr size_t MODEL_FILE_RESOURCE = 4;
constexpr size_t COMPONENT_GENDER = 0;
constexpr size_t COMPONENT_RACE = 2;
constexpr size_t COMPONENT_POSITION = 3;
constexpr size_t TEXTURE_USAGE = 1;
constexpr size_t TEXTURE_MATERIAL = 2;
constexpr size_t APPEARANCE_DISPLAY = 1;
constexpr size_t APPEARANCE_ICON = 2;

constexpr uint32_t ANY_GENDER = 2;
constexpr uint32_t MALE = 0;
constexpr uint32_t HUMAN = 1;
constexpr uint32_t OBJECT_SKIN_USAGE = 0;

// One file of a model resource: a race and gender's (a helm), a side's (a shoulder pair) or everyone's.
struct ModelFile {
    uint32_t fdid;
    uint32_t gender;
    uint32_t race;
    uint32_t position;
};

using ModelFilesByResource = std::map<uint32_t, std::vector<ModelFile>>;

static ModelFilesByResource FilesByResource(const Db2Table& modelFiles, const Db2Table& componentModels) {
    std::map<uint32_t, ModelFile> owners;
    for (size_t row = 0; row < componentModels.RowCount(); row++) {
        uint32_t fdid = componentModels.RowId(row);
        owners[fdid] = { fdid, componentModels.Value(row, COMPONENT_GENDER), componentModels.Value(row, COMPONENT_RACE), componentModels.Value(row, COMPONENT_POSITION) };
    }

    ModelFilesByResource files;
    for (size_t row = 0; row < modelFiles.RowCount(); row++) {
        uint32_t fdid = modelFiles.RowId(row);
        auto owner = owners.find(fdid);
        files[modelFiles.Value(row, MODEL_FILE_RESOURCE)].push_back(owner != owners.end() ? owner->second : ModelFile { fdid, ANY_GENDER, 0, 0 });
    }
    return files;
}

static uint32_t LowestFile(const std::vector<ModelFile>& files, bool (*wanted)(const ModelFile&, uint32_t), uint32_t side) {
    uint32_t lowest = 0;
    for (const ModelFile& file : files) {
        if (wanted(file, side) && (!lowest || file.fdid < lowest)) {
            lowest = file.fdid;
        }
    }
    return lowest;
}

// The human male file when the resource has race files, else the file on that side (a shoulder pair is one resource
// named on both), else the lowest (wow-toolbox-editor gear.rs first_file).
static uint32_t FileForSide(const std::vector<ModelFile>& files, uint32_t side) {
    if (uint32_t humanMale = LowestFile(files, [](const ModelFile& f, uint32_t) { return f.race == HUMAN && f.gender == MALE; }, side)) {
        return humanMale;
    }
    if (uint32_t onSide = LowestFile(files, [](const ModelFile& f, uint32_t s) { return f.position == s; }, side)) {
        return onSide;
    }
    return LowestFile(files, [](const ModelFile&, uint32_t) { return true; }, side);
}

// The lowest object skin file of each material resource.
static std::map<uint32_t, uint32_t> SkinsByMaterial(const Db2Table& textureFiles) {
    std::map<uint32_t, uint32_t> skins;
    for (size_t row = 0; row < textureFiles.RowCount(); row++) {
        if (textureFiles.Value(row, TEXTURE_USAGE) != OBJECT_SKIN_USAGE) {
            continue;
        }
        uint32_t& skin = skins[textureFiles.Value(row, TEXTURE_MATERIAL)];
        uint32_t fdid = textureFiles.RowId(row);
        skin = skin ? std::min(skin, fdid) : fdid;
    }
    return skins;
}

// Each display's icon: that of its lowest appearance with one.
static std::map<uint32_t, uint32_t> IconsByDisplay(const Db2Table& appearances) {
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> lowest;   // Display to (appearance, icon).
    for (size_t row = 0; row < appearances.RowCount(); row++) {
        uint32_t icon = appearances.Value(row, APPEARANCE_ICON);
        uint32_t appearance = appearances.RowId(row);
        if (!icon) {
            continue;
        }
        auto [chosen, added] = lowest.try_emplace(appearances.Value(row, APPEARANCE_DISPLAY), appearance, icon);
        if (!added && appearance < chosen->second.first) {
            chosen->second = { appearance, icon };
        }
    }

    std::map<uint32_t, uint32_t> icons;
    for (const auto& [display, chosen] : lowest) {
        icons[display] = chosen.second;
    }
    return icons;
}

struct ItemLookups {
    ModelFilesByResource files;
    std::map<uint32_t, uint32_t> skins;
    std::map<uint32_t, uint32_t> icons;
};

static uint32_t Find(const std::map<uint32_t, uint32_t>& map, uint32_t key) {
    auto found = map.find(key);
    return found == map.end() ? 0 : found->second;
}

static RetailItemDisplay DisplayAt(const Db2Table& displays, size_t row, const ItemLookups& lookups) {
    RetailItemDisplay display;
    display.id = displays.RowId(row);
    std::vector<uint32_t> resources = displays.Values(row, DISPLAY_RESOURCES);
    std::vector<uint32_t> materials = displays.Values(row, DISPLAY_MATERIALS);
    for (uint32_t side = 0; side < display.models.size(); side++) {
        uint32_t resource = side < resources.size() ? resources[side] : 0;
        auto files = lookups.files.find(resource);
        bool modelMissing = resource && files == lookups.files.end();   // Its skin would dress nothing, or draw as a cape.
        display.models[side] = resource && !modelMissing ? FileForSide(files->second, side) : 0;
        display.skins[side] = side < materials.size() && materials[side] && !modelMissing ? Find(lookups.skins, materials[side]) : 0;
    }
    std::vector<uint32_t> groups = displays.Values(row, DISPLAY_GEOSET_GROUPS);
    for (size_t i = 0; i < groups.size() && i < display.geosetGroups.size(); i++) {
        display.geosetGroups[i] = groups[i];
    }
    display.icon = Find(lookups.icons, display.id);
    return display;
}

static bool KnownLayouts(const ItemDisplayTables& tables, std::string& error) {
    return KnownLayout(tables.displays, ITEM_DISPLAY_INFO_LAYOUT, "ItemDisplayInfo", error) && KnownLayout(tables.modelFiles, MODEL_FILE_DATA_LAYOUT, "ModelFileData", error)
        && KnownLayout(tables.componentModels, COMPONENT_MODEL_FILE_DATA_LAYOUT, "ComponentModelFileData", error)
        && KnownLayout(tables.textureFiles, TEXTURE_FILE_DATA_LAYOUT, "TextureFileData", error)
        && KnownLayout(tables.appearances, ITEM_APPEARANCE_LAYOUT, "ItemAppearance", error);
}

bool RetailItemDisplays::Load(const ItemDisplayTables& tables, std::string& error) {
    if (!KnownLayouts(tables, error)) {
        return false;
    }

    ItemLookups lookups = { FilesByResource(tables.modelFiles, tables.componentModels), SkinsByMaterial(tables.textureFiles), IconsByDisplay(tables.appearances) };
    this->m_displays.clear();
    for (size_t row = 0; row < tables.displays.RowCount(); row++) {
        this->m_displays.push_back(DisplayAt(tables.displays, row, lookups));
    }
    std::sort(this->m_displays.begin(), this->m_displays.end(), [](const RetailItemDisplay& a, const RetailItemDisplay& b) { return a.id < b.id; });
    return true;
}

const RetailItemDisplay* RetailItemDisplays::Display(uint32_t id) const {
    auto display = std::lower_bound(this->m_displays.begin(), this->m_displays.end(), id, [](const RetailItemDisplay& d, uint32_t key) { return d.id < key; });
    return display != this->m_displays.end() && display->id == id ? &*display : nullptr;
}

}
