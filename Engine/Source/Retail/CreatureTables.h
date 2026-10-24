#ifndef ENGINE_SOURCE_RETAIL_CREATURETABLES_H
#define ENGINE_SOURCE_RETAIL_CREATURETABLES_H

#include <cstdint>
#include <map>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"

namespace Retail {

// DBFilesClient creature tables and the WoWDBDefs layouts read from them (wow-toolbox-editor casc/skins.rs).
constexpr uint32_t CREATURE_MODEL_DATA_FDID = 1365368;
constexpr uint32_t CREATURE_MODEL_DATA_LAYOUT = 0x0F5449F8;
constexpr uint32_t CREATURE_DISPLAY_INFO_FDID = 1108759;
constexpr uint32_t CREATURE_DISPLAY_INFO_LAYOUT = 0x7275F5F6;
constexpr uint32_t CREATURE_DISPLAY_INFO_GEOSET_DATA_FDID = 1720141;
constexpr uint32_t CREATURE_DISPLAY_INFO_GEOSET_DATA_LAYOUT = 0x5E539080;

// The creature tables, all of one build.
struct CreatureTables {
    const Db2Table& modelData;     // CreatureModelData: each model's file, bounds and sizes.
    const Db2Table& displayInfo;   // CreatureDisplayInfo: each display's model, skin, scale and alpha.
    const Db2Table& geosets;       // CreatureDisplayInfoGeosetData: the variants a display shows, related to it.
};

// A geoset variant a display shows: geoset (index + 1) * 100 + value.
struct Geoset {
    uint32_t index;   // The group, from 0.
    uint32_t value;

    bool operator==(const Geoset& other) const { return index == other.index && value == other.value; }
    bool operator<(const Geoset& other) const { return index != other.index ? index < other.index : value < other.value; }
};

// Each display's geosets, sorted and without repeats, by display id (the row's relation).
std::map<uint32_t, std::vector<Geoset>> GeosetsByDisplay(const Db2Table& geosets);

}

#endif
