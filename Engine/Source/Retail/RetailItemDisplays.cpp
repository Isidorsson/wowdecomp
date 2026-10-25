#include "Engine/Source/Retail/RetailItemDisplays.h"
#include <algorithm>
#include <map>

namespace Retail {

// Physical columns of the layouts (WoWDBDefs, as wow-toolbox-editor casc/gear.rs reads them).
constexpr size_t DISPLAY_RESOURCES = 10;
constexpr size_t DISPLAY_MATERIALS = 11;
constexpr size_t DISPLAY_GEOSET_GROUPS = 13;
constexpr size_t DISPLAY_HELMET_VIS = 15;
constexpr size_t MODEL_FILE_RESOURCE = 4;
constexpr size_t COMPONENT_GENDER = 0;
constexpr size_t COMPONENT_RACE = 2;
constexpr size_t COMPONENT_POSITION = 3;
constexpr size_t TEXTURE_USAGE = 1;
constexpr size_t TEXTURE_MATERIAL = 2;
constexpr size_t APPEARANCE_DISPLAY = 1;
constexpr size_t APPEARANCE_ICON = 2;
constexpr size_t SECTION_SECTION = 0;
constexpr size_t SECTION_MATERIAL = 1;
constexpr size_t COMPONENT_TEXTURE_GENDER = 0;
constexpr size_t COMPONENT_TEXTURE_RACE = 2;
constexpr size_t HELMET_GEOSET_RACE = 0;
constexpr size_t HELMET_GEOSET_GROUP = 1;

constexpr uint32_t ANY_GENDER = 2;
constexpr uint32_t MALE = 0;
constexpr uint32_t FEMALE = 1;
constexpr uint32_t HUMAN = 1;
constexpr uint32_t OBJECT_SKIN_USAGE = 0;


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

// A body texture file's gender and race; a file ComponentTextureFileData does not list is for everyone.
struct TextureOwner {
    uint32_t gender = ANY_GENDER;
    uint32_t race = 0;
};

using SectionMaterials = std::vector<std::pair<uint32_t, uint32_t>>;   // Section, material resource.

struct BodyLookups {
    std::map<uint32_t, SectionMaterials> sectionsOf;            // By display.
    std::map<uint32_t, std::vector<uint32_t>> objectFilesOf;    // Usage 0 files by material resource.
    std::map<uint32_t, TextureOwner> owners;                    // By file.
};

static BodyLookups BodyLookupsOf(const ItemDisplayTables& tables) {
    BodyLookups lookups;
    for (size_t row = 0; row < tables.sections.RowCount(); row++) {
        if (uint32_t display = tables.sections.Relation(row)) {
            lookups.sectionsOf[display].push_back({ tables.sections.Value(row, SECTION_SECTION), tables.sections.Value(row, SECTION_MATERIAL) });
        }
    }
    for (size_t row = 0; row < tables.textureFiles.RowCount(); row++) {
        if (tables.textureFiles.Value(row, TEXTURE_USAGE) == OBJECT_SKIN_USAGE) {
            lookups.objectFilesOf[tables.textureFiles.Value(row, TEXTURE_MATERIAL)].push_back(tables.textureFiles.RowId(row));
        }
    }
    for (size_t row = 0; row < tables.componentTextures.RowCount(); row++) {
        uint32_t gender = tables.componentTextures.Value(row, COMPONENT_TEXTURE_GENDER);
        lookups.owners[tables.componentTextures.RowId(row)] = { gender == MALE || gender == FEMALE ? gender : ANY_GENDER,
            tables.componentTextures.Value(row, COMPONENT_TEXTURE_RACE) };
    }
    return lookups;
}

struct SectionFile {
    uint32_t fdid;
    TextureOwner owner;
};

// The race whose files a section takes: every race's, else the human's, else the lowest race's.
static uint32_t SectionRace(const std::vector<SectionFile>& files) {
    uint32_t lowest = UINT32_MAX;
    bool human = false;
    for (const SectionFile& file : files) {
        if (file.owner.race == 0) {
            return 0;
        }
        human |= file.owner.race == HUMAN;
        lowest = std::min(lowest, file.owner.race);
    }
    return human ? HUMAN : lowest;
}

static uint32_t LowestOfGender(const std::vector<SectionFile>& files, uint32_t race, uint32_t gender) {
    uint32_t lowest = 0;
    for (const SectionFile& file : files) {
        if (file.owner.race == race && file.owner.gender == gender && (!lowest || file.fdid < lowest)) {
            lowest = file.fdid;
        }
    }
    return lowest;
}

// One file per gender, a file for both winning, as the client asks for "_U" first (wow-toolbox-editor
// gear_textures.rs choose_section).
static BodyTexture ChooseSection(const std::vector<SectionFile>& files) {
    if (files.empty()) {
        return {};
    }
    uint32_t race = SectionRace(files);
    if (uint32_t both = LowestOfGender(files, race, ANY_GENDER)) {
        return { both, both };
    }
    return { LowestOfGender(files, race, MALE), LowestOfGender(files, race, FEMALE) };
}

static std::array<BodyTexture, BODY_SECTIONS> BodyOf(uint32_t display, const BodyLookups& lookups) {
    std::array<std::vector<SectionFile>, BODY_SECTIONS> filesOf;
    auto sections = lookups.sectionsOf.find(display);
    for (const auto& [section, material] : sections == lookups.sectionsOf.end() ? SectionMaterials() : sections->second) {
        auto files = lookups.objectFilesOf.find(material);
        if (section >= BODY_SECTIONS || files == lookups.objectFilesOf.end()) {
            continue;
        }
        for (uint32_t fdid : files->second) {
            auto owner = lookups.owners.find(fdid);
            filesOf[section].push_back({ fdid, owner == lookups.owners.end() ? TextureOwner() : owner->second });
        }
    }

    std::array<BodyTexture, BODY_SECTIONS> body;
    for (size_t section = 0; section < BODY_SECTIONS; section++) {
        body[section] = ChooseSection(filesOf[section]);
    }
    return body;
}

struct ItemLookups {
    ModelFilesByResource files;
    std::map<uint32_t, uint32_t> skins;
    std::map<uint32_t, uint32_t> icons;
    BodyLookups body;
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
    std::vector<uint32_t> helmetVis = displays.Values(row, DISPLAY_HELMET_VIS);
    for (size_t i = 0; i < helmetVis.size() && i < display.helmetVis.size(); i++) {
        display.helmetVis[i] = helmetVis[i];
    }
    display.icon = Find(lookups.icons, display.id);
    display.body = BodyOf(display.id, lookups.body);
    return display;
}

static bool KnownLayouts(const ItemDisplayTables& tables, std::string& error) {
    return KnownLayout(tables.displays, ITEM_DISPLAY_INFO_LAYOUT, "ItemDisplayInfo", error) && KnownLayout(tables.modelFiles, MODEL_FILE_DATA_LAYOUT, "ModelFileData", error)
        && KnownLayout(tables.componentModels, COMPONENT_MODEL_FILE_DATA_LAYOUT, "ComponentModelFileData", error)
        && KnownLayout(tables.textureFiles, TEXTURE_FILE_DATA_LAYOUT, "TextureFileData", error)
        && KnownLayout(tables.appearances, ITEM_APPEARANCE_LAYOUT, "ItemAppearance", error)
        && KnownLayout(tables.sections, ITEM_DISPLAY_INFO_MATERIAL_RES_LAYOUT, "ItemDisplayInfoMaterialRes", error)
        && KnownLayout(tables.componentTextures, COMPONENT_TEXTURE_FILE_DATA_LAYOUT, "ComponentTextureFileData", error)
        && KnownLayout(tables.helmetGeosets, HELMET_GEOSET_DATA_LAYOUT, "HelmetGeosetData", error);
}

// Each vis id's masks, for the ids HelmetGeosetData has rows for.
static std::map<uint32_t, HelmetMasks> HelmetVisById(const Db2Table& helmetGeosets) {
    std::map<uint32_t, std::vector<HelmetGeoset>> hidden;
    for (size_t row = 0; row < helmetGeosets.RowCount(); row++) {
        if (uint32_t vis = helmetGeosets.Relation(row)) {
            hidden[vis].push_back({ helmetGeosets.Value(row, HELMET_GEOSET_RACE), helmetGeosets.Value(row, HELMET_GEOSET_GROUP) });
        }
    }

    std::map<uint32_t, HelmetMasks> masks;
    for (const auto& [vis, geosets] : hidden) {
        masks[vis] = HelmetVisMasks(geosets);
    }
    return masks;
}

bool RetailItemDisplays::Load(const ItemDisplayTables& tables, std::string& error) {
    if (!KnownLayouts(tables, error)) {
        return false;
    }

    ItemLookups lookups = { FilesByResource(tables.modelFiles, tables.componentModels), SkinsByMaterial(tables.textureFiles), IconsByDisplay(tables.appearances),
        BodyLookupsOf(tables) };
    this->m_resourceOf.clear();
    for (const auto& [resource, files] : lookups.files) {
        for (const ModelFile& file : files) {
            this->m_resourceOf[file.fdid] = resource;
        }
    }
    this->m_helmetVis = HelmetVisById(tables.helmetGeosets);
    this->m_displays.clear();
    for (size_t row = 0; row < tables.displays.RowCount(); row++) {
        this->m_displays.push_back(DisplayAt(tables.displays, row, lookups));
    }
    std::sort(this->m_displays.begin(), this->m_displays.end(), [](const RetailItemDisplay& a, const RetailItemDisplay& b) { return a.id < b.id; });
    this->m_filesOf = std::move(lookups.files);
    return true;
}

const RetailItemDisplay* RetailItemDisplays::Display(uint32_t id) const {
    auto display = std::lower_bound(this->m_displays.begin(), this->m_displays.end(), id, [](const RetailItemDisplay& d, uint32_t key) { return d.id < key; });
    return display != this->m_displays.end() && display->id == id ? &*display : nullptr;
}

// HelmetGeosetVisDataRec's slots: the retail geoset group each hides.
constexpr std::array<uint32_t, HELMET_HIDE_SLOTS> HELMET_SLOT_GROUPS = { 0, 1, 2, 3, 7, 16, 17 };
constexpr uint32_t LAST_STOCK_RACE = 11;

HelmetMasks HelmetVisMasks(const std::vector<HelmetGeoset>& hidden) {
    HelmetMasks masks = {};
    for (const HelmetGeoset& geoset : hidden) {
        auto slot = std::find(HELMET_SLOT_GROUPS.begin(), HELMET_SLOT_GROUPS.end(), geoset.group);
        if (slot != HELMET_SLOT_GROUPS.end() && geoset.race >= 1 && geoset.race <= LAST_STOCK_RACE) {
            masks[slot - HELMET_SLOT_GROUPS.begin()] |= 1 << geoset.race;
        }
    }
    return masks;
}

// How well a file suits a race and gender: 1 for theirs, 2 the race's for either gender, 3 every race's of the
// gender, 4 every race's for either; 0 for another race's or the other gender's.
static uint32_t RaceFileRank(const ModelFile& file, uint32_t race, uint32_t gender) {
    uint32_t fileGender = file.gender == MALE || file.gender == FEMALE ? file.gender : ANY_GENDER;
    if (fileGender != gender && fileGender != ANY_GENDER) {
        return 0;
    }
    uint32_t either = fileGender == ANY_GENDER ? 1 : 0;
    if (file.race == race) {
        return 1 + either;
    }
    return file.race == 0 ? 3 + either : 0;
}

uint32_t RetailItemDisplays::RaceFile(uint32_t fileDataId, uint32_t race, uint32_t gender) const {
    auto resource = this->m_resourceOf.find(fileDataId);
    if (resource == this->m_resourceOf.end() || race == 0) {
        return 0;
    }

    uint32_t best = 0;
    uint32_t bestRank = 0;
    for (const ModelFile& file : this->m_filesOf.at(resource->second)) {
        uint32_t rank = RaceFileRank(file, race, gender);
        if (rank && (!bestRank || rank < bestRank || (rank == bestRank && file.fdid < best))) {
            best = file.fdid;
            bestRank = rank;
        }
    }
    return best;
}

const HelmetMasks* RetailItemDisplays::HelmetVis(uint32_t retailId) const {
    auto masks = this->m_helmetVis.find(retailId);
    return retailId && masks != this->m_helmetVis.end() ? &masks->second : nullptr;
}

}
