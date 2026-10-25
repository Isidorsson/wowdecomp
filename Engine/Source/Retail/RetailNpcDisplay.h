#ifndef ENGINE_SOURCE_RETAIL_RETAILNPCDISPLAY_H
#define ENGINE_SOURCE_RETAIL_RETAILNPCDISPLAY_H

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"

namespace Retail {

// Retail humanoid NPCs: a creature display with an ExtendedDisplayInfoID names a CreatureDisplayInfoExtra row (race,
// sex, baked skin, worn items and customization choices) and a retail player-race model the converter refuses. A
// served display instead points at the installed body of that race and gender and at a served Extra row, which the
// client's character component draws as it draws a stock NPC: the bake replaces the whole skin, the row's hair and
// facial hair pick geosets and the items attach.

constexpr uint32_t CREATURE_DISPLAY_INFO_EXTRA_FDID = 1264997;
constexpr uint32_t CREATURE_DISPLAY_INFO_EXTRA_LAYOUT = 0x4D9FE25C;
constexpr uint32_t NPC_MODEL_ITEM_SLOT_DISPLAY_INFO_FDID = 1340661;
constexpr uint32_t NPC_MODEL_ITEM_SLOT_DISPLAY_INFO_LAYOUT = 0xC2057F5B;
constexpr uint32_t CREATURE_DISPLAY_INFO_OPTION_FDID = 3692043;
constexpr uint32_t CREATURE_DISPLAY_INFO_OPTION_LAYOUT = 0x2F331C33;
constexpr uint32_t CHR_CUSTOMIZATION_CHOICE_FDID = 3450554;
constexpr uint32_t CHR_CUSTOMIZATION_CHOICE_LAYOUT = 0x9559C358;
constexpr uint32_t CHR_CUSTOMIZATION_ELEMENT_FDID = 3512765;
constexpr uint32_t CHR_CUSTOMIZATION_ELEMENT_LAYOUT = 0x6483C37E;
constexpr uint32_t CHR_CUSTOMIZATION_GEOSET_FDID = 3456171;
constexpr uint32_t CHR_CUSTOMIZATION_GEOSET_LAYOUT = 0xBF55F2FB;
constexpr uint32_t CHR_CUSTOMIZATION_MATERIAL_FDID = 3459652;
constexpr uint32_t CHR_CUSTOMIZATION_MATERIAL_LAYOUT = 0xBE9767E9;

// Retail Extra ids overlap stock ones, so a served display names its Extra row by an id above every stock id.
constexpr uint32_t SYNTHETIC_EXTRA_ID_BASE = 1000000;
// Helm, shoulders, shirt, chest, belt, legs, boots, wrists, gloves, tabard, cape: retail ItemSlot 0 to 10 are the
// 3.3.5 NPCItemDisplay columns in the same order.
constexpr size_t NPC_ITEM_SLOTS = 11;

uint32_t SyntheticExtraId(uint32_t retailExtraId);
// False for an id below the synthetic range.
bool RetailExtraIdOf(uint32_t syntheticExtraId, uint32_t& retailExtraId);

struct NpcTables {
    const Db2Table& extra;          // CreatureDisplayInfoExtra.
    const Db2Table& itemSlots;      // NPCModelItemSlotDisplayInfo, related to the Extra row.
    const Db2Table& options;        // CreatureDisplayInfoOption: the row's customization choices, related to it.
    const Db2Table& choices;        // ChrCustomizationChoice: order index.
    const Db2Table& elements;       // ChrCustomizationElement: what a choice shows, maybe only with another choice.
    const Db2Table& geosets;        // ChrCustomizationGeoset.
    const Db2Table& materials;      // ChrCustomizationMaterial: texture target and material resource.
    const Db2Table& textureFiles;   // TextureFileData: material resource to file.
};

// A geoset a choice shows: retail GeosetType (0 hair, 1 beard, 2 sideburns, 3 moustache, 24 tauren horns) and id.
struct CustomizationGeoset {
    uint32_t type;
    uint32_t id;

    bool operator==(const CustomizationGeoset& other) const { return type == other.type && id == other.id; }
};

// A chosen material: its ChrModelTextureTarget and file, which a retail body places by its texture layers.
struct ChosenMaterial {
    uint32_t target;
    uint32_t file;

    bool operator==(const ChosenMaterial& other) const { return target == other.target && file == other.file; }
};

// What an Extra row's customization choices show that a 3.3.5 NPC can: geosets and the hair colour.
struct NpcLook {
    std::vector<CustomizationGeoset> geosets;
    std::vector<ChosenMaterial> materials;   // Every chosen material with a file.
    bool hasHairColor = false;
    uint32_t hairColorOrder = 0;   // The Hair Color choice's order index, which is the installed colour index.
    uint32_t hairTexture = 0;      // Its texture (target 10), for the log.
};

// A retail Extra row and what hangs off it. Bakes are files: the material's first usage-0 TextureFileData row.
struct NpcExtra {
    uint32_t id = 0;
    uint32_t race = 0;
    uint32_t sex = 0;
    uint32_t flags = 0;
    uint32_t sdBake = 0;
    uint32_t hdBake = 0;
    std::array<uint32_t, NPC_ITEM_SLOTS> items = {};   // Retail ItemDisplayInfo ids, 0 for an empty slot.
    NpcLook look;
};

class RetailNpcTables {
    public:
    bool Load(const NpcTables& tables, std::string& error);

    // Null for an unknown id.
    const NpcExtra* Extra(uint32_t id) const;

    private:
    std::vector<NpcExtra> m_extras;   // By id.
};

// How a served display draws a retail Extra row on the installed client.
enum class BakeUse {
    Warp,   // The HD bake, warped into the installed HD-port body's square layout.
    AsIs,   // The SD bake, already in the stock square layout.
};

struct NpcPlan {
    int32_t bodyModelId = 0;   // The stock CreatureModelData id of the race and gender's body, or a retail body's
                               // SyntheticNpcBodyModelId.
    BakeUse bake = BakeUse::AsIs;
    uint32_t bakeFile = 0;
    std::string bakeName;      // What the served Extra row names, under Textures\BakedNpcTextures\.
    uint32_t race = 0;         // The race the served Extra row names: the installed body's.
    bool retailBody = false;   // Drawn on the retail model, shaped by RetailBodyLook.
};

// The retail model a display draws its NPC with: its CreatureModelData id and file.
struct NpcBody {
    uint32_t retailModelId = 0;
    uint32_t fileDataId = 0;
};

// Whether the installed body of race and gender is the HD port the HD bakes can be warped onto
// (BodyMatchRatio >= BODY_MATCH_THRESHOLD).
using BodyFits = std::function<bool(uint32_t race, uint32_t sex)>;

// The plan for a retail Extra row, or false with the reason the display keeps its missing model: an unsupported
// race (Goblin, 22 and up), a body that is not the HD port, or no usable bake.
bool BuildNpcPlan(const NpcExtra& extra, const BodyFits& bodyFits, NpcPlan& plan, std::string& reason);

// Goblin (9) and races 22 and up (slice b): the installed body when it is a port of the display's model (HdPortRace,
// fitting), with the bake warped onto it, else the display's retail model with the bake as is (these bakes are in
// the retail 2:1 layout of their own model). Refused without a model or a bake.
bool BuildRetailRacePlan(const NpcExtra& extra, const NpcBody& body, const BodyFits& bodyFits, NpcPlan& plan, std::string& reason);

// BuildRetailRacePlan for Goblin and races 22 and up, BuildNpcPlan for the rest.
bool PlanNpc(const NpcExtra& extra, const NpcBody& body, const BodyFits& bodyFits, NpcPlan& plan, std::string& reason);

// Goblin and races 22 and up, whose installed bodies (if any) are no ports of their retail ones.
bool IsRetailBodyRace(uint32_t race);

// The stock body's CreatureModelData id for race 1 to 21 and sex; 0 otherwise.
int32_t StockBodyModelId(uint32_t race, uint32_t sex);

// The races whose installed bodies are HD ports (1 to 8, 10, 11), whose HD bakes are warped.
bool IsHdPortRace(uint32_t race);

}

#endif
