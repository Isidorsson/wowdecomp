#ifndef ENGINE_SOURCE_RETAIL_BROWSERTABLES_H
#define ENGINE_SOURCE_RETAIL_BROWSERTABLES_H

#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"

namespace Retail {

// DBFilesClient tables the retail browser searches, and the WoWDBDefs layouts read from them (12.x; wow-toolbox-editor
// casc/model_names.rs reads the same joins). Names, quality and item level come from ItemSearchName; stats and sockets
// from ItemSparse, attached after the build (RetailIndex::AttachSparse, ItemSparse.h).
constexpr uint32_t CREATURE_FDID = 841631;
constexpr uint32_t CREATURE_LAYOUT = 0x6E14C900;
constexpr uint32_t ITEM_FDID = 841626;
constexpr uint32_t ITEM_LAYOUT = 0x996192AA;
constexpr uint32_t ITEM_SEARCH_NAME_FDID = 1273408;
constexpr uint32_t ITEM_SEARCH_NAME_LAYOUT = 0x4516C8B0;
constexpr uint32_t ITEM_MODIFIED_APPEARANCE_FDID = 982457;
constexpr uint32_t ITEM_MODIFIED_APPEARANCE_LAYOUT = 0x03A6C979;
constexpr uint32_t TRANSMOG_SET_FDID = 1376213;
constexpr uint32_t TRANSMOG_SET_LAYOUT = 0xC6875C71;
constexpr uint32_t TRANSMOG_SET_ITEM_FDID = 1376212;
constexpr uint32_t TRANSMOG_SET_ITEM_LAYOUT = 0xE6EFF061;
constexpr uint32_t ITEM_SET_FDID = 1343609;
constexpr uint32_t ITEM_SET_LAYOUT = 0xF79068A4;
constexpr uint32_t MOUNT_FDID = 921760;
constexpr uint32_t MOUNT_LAYOUT = 0xE4B71B79;
constexpr uint32_t MOUNT_X_DISPLAY_FDID = 1576117;
constexpr uint32_t MOUNT_X_DISPLAY_LAYOUT = 0xC05B05EF;
constexpr uint32_t CREATURE_TYPE_FDID = 1131315;
constexpr uint32_t CREATURE_TYPE_LAYOUT = 0x6FF4CA42;
constexpr uint32_t CREATURE_FAMILY_FDID = 1351351;
constexpr uint32_t CREATURE_FAMILY_LAYOUT = 0x7FE3CBA7;
constexpr uint32_t ITEM_SUB_CLASS_FDID = 1261604;
constexpr uint32_t ITEM_SUB_CLASS_LAYOUT = 0x1E67DB87;

// The browser's tables, all of one build. ItemAppearance is ITEM_APPEARANCE_FDID (ItemTables.h).
struct BrowserTables {
    const Db2Table& creatures;            // Creature: names, type, family, classification, up to 4 displays.
    const Db2Table& items;                // Item: class, subclass, inventory type, icon.
    const Db2Table& searchNames;          // ItemSearchName: name, quality, expansion, item level.
    const Db2Table& modifiedAppearances;  // ItemModifiedAppearance: each item's appearances.
    const Db2Table& appearances;          // ItemAppearance: each appearance's ItemDisplayInfo and icon.
    const Db2Table& transmogSets;         // TransmogSet: name, class mask, expansion.
    const Db2Table& transmogSetItems;     // TransmogSetItem: each set's ItemModifiedAppearance pieces.
    const Db2Table& itemSets;             // ItemSet: name and up to 17 item ids.
    const Db2Table& mounts;               // Mount: name, spell.
    const Db2Table& mountDisplays;        // MountXDisplay: each mount's creature displays, related to it.
    const Db2Table& creatureTypes;        // CreatureType: names.
    const Db2Table& creatureFamilies;     // CreatureFamily: names.
    const Db2Table& itemSubClasses;       // ItemSubClass: names, related to the item class.
};

// What a creature display is, from RetailDisplays and the NPC tables.
struct CreatureDisplayFacts {
    uint32_t modelId = 0;     // CreatureModelData id.
    uint32_t modelFdid = 0;
    float scale = 1;
    uint32_t extraId = 0;     // CreatureDisplayInfoExtra id, 0 for a plain model.
    uint32_t race = 0;        // The Extra row's race and sex, 0 without one.
    uint32_t sex = 0;
    uint32_t sdBake = 0;
    uint32_t hdBake = 0;
};

// What an item display draws, from RetailItemDisplays.
struct ItemDisplayFacts {
    std::array<uint32_t, 2> models = {};   // Per side: the model file, 0 for none.
    uint32_t icon = 0;
};

// False for a display the loaders do not know. creatureDisplayIds: every creature display they know, for the displays
// of each model.
struct BrowserLookups {
    std::function<bool(uint32_t displayId, CreatureDisplayFacts& facts)> creatureDisplay;
    std::function<bool(uint32_t displayId, ItemDisplayFacts& facts)> itemDisplay;
    std::function<std::vector<uint32_t>()> creatureDisplayIds;
};

}

#endif
