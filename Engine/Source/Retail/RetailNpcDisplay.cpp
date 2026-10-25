#include "Engine/Source/Retail/RetailNpcDisplay.h"
#include <algorithm>
#include <map>
#include <set>
#include "Engine/Source/Retail/ItemTables.h"
#include "Engine/Source/Retail/RetailPath.h"

namespace Retail {

// Physical columns of the layouts (WoWDBDefs).
constexpr size_t EXTRA_RACE = 1;
constexpr size_t EXTRA_SEX = 2;
constexpr size_t EXTRA_FLAGS = 4;
constexpr size_t EXTRA_SD_MATERIAL = 5;
constexpr size_t EXTRA_HD_MATERIAL = 6;
constexpr size_t ITEM_DISPLAY = 0;
constexpr size_t ITEM_SLOT = 1;
constexpr size_t OPTION_CHOICE = 1;
constexpr size_t CHOICE_ORDER = 5;
constexpr size_t ELEMENT_CHOICE = 0;
constexpr size_t ELEMENT_RELATED = 1;
constexpr size_t ELEMENT_GEOSET = 2;
constexpr size_t ELEMENT_MATERIAL = 4;
constexpr size_t GEOSET_TYPE = 0;
constexpr size_t GEOSET_ID = 1;
constexpr size_t MATERIAL_TARGET = 0;
constexpr size_t MATERIAL_RESOURCE = 1;
constexpr size_t TEXTURE_USAGE = 1;
constexpr size_t TEXTURE_MATERIAL = 2;

constexpr uint32_t BAKE_USAGE = 0;
// ChrModelTextureLayer: target 10 is texture type 6, the hair, on every player-race layout.
constexpr uint32_t HAIR_TARGET = 10;

// Stock CreatureModelData ids of the bodies, male then female, races 1 to 21 (ChrRaces display ids; FACTS.md Q4).
// Goblin (9) is slice (b): its installed body is not a port of the retail goblin.
static const int32_t STOCK_BODIES[][2] = {
    { 49, 50 }, { 51, 52 }, { 53, 54 }, { 55, 56 }, { 57, 58 }, { 59, 60 }, { 182, 183 }, { 185, 186 }, { 0, 0 }, { 2208, 2209 },
    { 2248, 2250 }, { 2299, 2298 }, { 2353, 2354 }, { 2367, 2368 }, { 2369, 2370 }, { 2594, 2594 }, { 2604, 2604 }, { 2613, 2613 },
    { 2891, 2891 }, { 2940, 2940 }, { 2942, 2942 },
};
constexpr uint32_t STOCK_BODY_RACES = sizeof(STOCK_BODIES) / sizeof(STOCK_BODIES[0]);
constexpr uint32_t LAST_HD_PORT_RACE = 11;
constexpr uint32_t GOBLIN = 9;

uint32_t SyntheticExtraId(uint32_t retailExtraId) {
    return SYNTHETIC_EXTRA_ID_BASE + retailExtraId;
}

bool RetailExtraIdOf(uint32_t syntheticExtraId, uint32_t& retailExtraId) {
    if (syntheticExtraId < SYNTHETIC_EXTRA_ID_BASE) {
        return false;
    }
    retailExtraId = syntheticExtraId - SYNTHETIC_EXTRA_ID_BASE;
    return true;
}

int32_t StockBodyModelId(uint32_t race, uint32_t sex) {
    return race >= 1 && race <= STOCK_BODY_RACES && sex <= 1 ? STOCK_BODIES[race - 1][sex] : 0;
}

bool IsHdPortRace(uint32_t race) {
    return race >= 1 && race <= LAST_HD_PORT_RACE && race != GOBLIN;
}

static bool CheckLayouts(const NpcTables& tables, std::string& error) {
    return KnownLayout(tables.extra, CREATURE_DISPLAY_INFO_EXTRA_LAYOUT, "CreatureDisplayInfoExtra", error)
        && KnownLayout(tables.itemSlots, NPC_MODEL_ITEM_SLOT_DISPLAY_INFO_LAYOUT, "NPCModelItemSlotDisplayInfo", error)
        && KnownLayout(tables.options, CREATURE_DISPLAY_INFO_OPTION_LAYOUT, "CreatureDisplayInfoOption", error)
        && KnownLayout(tables.choices, CHR_CUSTOMIZATION_CHOICE_LAYOUT, "ChrCustomizationChoice", error)
        && KnownLayout(tables.elements, CHR_CUSTOMIZATION_ELEMENT_LAYOUT, "ChrCustomizationElement", error)
        && KnownLayout(tables.geosets, CHR_CUSTOMIZATION_GEOSET_LAYOUT, "ChrCustomizationGeoset", error)
        && KnownLayout(tables.materials, CHR_CUSTOMIZATION_MATERIAL_LAYOUT, "ChrCustomizationMaterial", error)
        && KnownLayout(tables.textureFiles, TEXTURE_FILE_DATA_LAYOUT, "TextureFileData", error);
}

// The lowest usage-0 file of each material resource.
static std::map<uint32_t, uint32_t> FilesByMaterial(const Db2Table& textureFiles) {
    std::map<uint32_t, uint32_t> files;
    for (size_t row = 0; row < textureFiles.RowCount(); row++) {
        if (textureFiles.Value(row, TEXTURE_USAGE) != BAKE_USAGE) {
            continue;
        }
        uint32_t& file = files[textureFiles.Value(row, TEXTURE_MATERIAL)];
        uint32_t id = textureFiles.RowId(row);
        file = file ? std::min(file, id) : id;
    }
    return files;
}

template <typename Value>
static Value Find(const std::map<uint32_t, Value>& map, uint32_t key, Value fallback = {}) {
    auto found = map.find(key);
    return found == map.end() ? fallback : found->second;
}

// The customization tables, indexed for building looks.
struct CustomizationIndex {
    std::map<uint32_t, uint32_t> order;                    // Choice to order index.
    std::map<uint32_t, std::vector<size_t>> elementsOf;    // Choice to element rows.
    std::map<uint32_t, CustomizationGeoset> geosets;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> materials;   // Id to (target, resource).
};

static CustomizationIndex IndexCustomization(const NpcTables& tables) {
    CustomizationIndex index;
    for (size_t row = 0; row < tables.choices.RowCount(); row++) {
        index.order[tables.choices.RowId(row)] = tables.choices.Value(row, CHOICE_ORDER);
    }
    for (size_t row = 0; row < tables.elements.RowCount(); row++) {
        index.elementsOf[tables.elements.Value(row, ELEMENT_CHOICE)].push_back(row);
    }
    for (size_t row = 0; row < tables.geosets.RowCount(); row++) {
        index.geosets[tables.geosets.RowId(row)] = { tables.geosets.Value(row, GEOSET_TYPE), tables.geosets.Value(row, GEOSET_ID) };
    }
    for (size_t row = 0; row < tables.materials.RowCount(); row++) {
        index.materials[tables.materials.RowId(row)] = { tables.materials.Value(row, MATERIAL_TARGET), tables.materials.Value(row, MATERIAL_RESOURCE) };
    }
    return index;
}

// What the chosen choices show: an element counts when it needs no other choice or its other choice is chosen too.
static NpcLook LookOf(const std::set<uint32_t>& chosen, const NpcTables& tables, const CustomizationIndex& index, const std::map<uint32_t, uint32_t>& files) {
    NpcLook look;
    for (uint32_t choice : chosen) {
        for (size_t row : Find(index.elementsOf, choice, std::vector<size_t>())) {
            uint32_t related = tables.elements.Value(row, ELEMENT_RELATED);
            if (related && !chosen.count(related)) {
                continue;
            }
            auto geoset = index.geosets.find(tables.elements.Value(row, ELEMENT_GEOSET));
            if (geoset != index.geosets.end()) {
                look.geosets.push_back(geoset->second);
            }
            auto material = index.materials.find(tables.elements.Value(row, ELEMENT_MATERIAL));
            if (material != index.materials.end() && material->second.first == HAIR_TARGET) {
                look.hasHairColor = true;
                look.hairColorOrder = Find(index.order, choice, 0u);
                look.hairTexture = Find(files, material->second.second, 0u);
            }
        }
    }
    return look;
}

static std::map<uint32_t, std::array<uint32_t, NPC_ITEM_SLOTS>> ItemsByExtra(const Db2Table& itemSlots) {
    std::map<uint32_t, std::array<uint32_t, NPC_ITEM_SLOTS>> items;
    for (size_t row = 0; row < itemSlots.RowCount(); row++) {
        uint32_t slot = itemSlots.Value(row, ITEM_SLOT);
        if (slot < NPC_ITEM_SLOTS) {
            items[itemSlots.Relation(row)][slot] = itemSlots.Value(row, ITEM_DISPLAY);
        }
    }
    return items;
}

static std::map<uint32_t, std::set<uint32_t>> ChoicesByExtra(const Db2Table& options) {
    std::map<uint32_t, std::set<uint32_t>> chosen;
    for (size_t row = 0; row < options.RowCount(); row++) {
        chosen[options.Relation(row)].insert(options.Value(row, OPTION_CHOICE));
    }
    return chosen;
}

bool RetailNpcTables::Load(const NpcTables& tables, std::string& error) {
    if (!CheckLayouts(tables, error)) {
        return false;
    }

    std::map<uint32_t, uint32_t> files = FilesByMaterial(tables.textureFiles);
    std::map<uint32_t, std::array<uint32_t, NPC_ITEM_SLOTS>> items = ItemsByExtra(tables.itemSlots);
    std::map<uint32_t, std::set<uint32_t>> chosen = ChoicesByExtra(tables.options);
    CustomizationIndex index = IndexCustomization(tables);

    this->m_extras.clear();
    for (size_t row = 0; row < tables.extra.RowCount(); row++) {
        NpcExtra extra;
        extra.id = tables.extra.RowId(row);
        extra.race = tables.extra.Value(row, EXTRA_RACE);
        extra.sex = tables.extra.Value(row, EXTRA_SEX);
        extra.flags = tables.extra.Value(row, EXTRA_FLAGS);
        extra.sdBake = Find(files, tables.extra.Value(row, EXTRA_SD_MATERIAL), 0u);
        extra.hdBake = Find(files, tables.extra.Value(row, EXTRA_HD_MATERIAL), 0u);
        extra.items = Find(items, extra.id, std::array<uint32_t, NPC_ITEM_SLOTS>());
        auto choices = chosen.find(extra.id);
        if (choices != chosen.end()) {
            extra.look = LookOf(choices->second, tables, index, files);
        }
        this->m_extras.push_back(std::move(extra));
    }
    std::sort(this->m_extras.begin(), this->m_extras.end(), [](const NpcExtra& a, const NpcExtra& b) { return a.id < b.id; });
    return true;
}

const NpcExtra* RetailNpcTables::Extra(uint32_t id) const {
    auto row = std::lower_bound(this->m_extras.begin(), this->m_extras.end(), id, [](const NpcExtra& r, uint32_t key) { return r.id < key; });
    return row != this->m_extras.end() && row->id == id ? &*row : nullptr;
}

bool BuildNpcPlan(const NpcExtra& extra, const BodyFits& bodyFits, NpcPlan& plan, std::string& reason) {
    plan = NpcPlan();
    plan.bodyModelId = StockBodyModelId(extra.race, extra.sex);
    if (!plan.bodyModelId) {
        reason = "race " + std::to_string(extra.race) + " has no installed body yet (slice b)";
        return false;
    }

    if (IsHdPortRace(extra.race) && extra.hdBake) {
        if (!bodyFits(extra.race, extra.sex)) {
            reason = "the installed body of race " + std::to_string(extra.race) + " is not the HD port its HD bake needs (slice b)";
            return false;
        }
        plan.bake = BakeUse::Warp;
        plan.bakeFile = extra.hdBake;
        plan.bakeName = WarpedBakeName(extra.hdBake, extra.race, extra.sex);
        return true;
    }

    if (!extra.sdBake) {
        reason = "no bake";
        return false;
    }
    plan.bake = BakeUse::AsIs;
    plan.bakeFile = extra.sdBake;
    plan.bakeName = RetailFileName(extra.sdBake) + ".blp";
    return true;
}

}
