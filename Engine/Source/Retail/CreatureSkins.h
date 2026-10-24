#ifndef ENGINE_SOURCE_RETAIL_CREATURESKINS_H
#define ENGINE_SOURCE_RETAIL_CREATURESKINS_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"

namespace Retail {

// DBFilesClient tables and the WoWDBDefs layouts read from them (wow-toolbox-editor casc/skins.rs).
constexpr uint32_t CREATURE_MODEL_DATA_FDID = 1365368;
constexpr uint32_t CREATURE_MODEL_DATA_LAYOUT = 0x0F5449F8;
constexpr uint32_t CREATURE_DISPLAY_INFO_FDID = 1108759;
constexpr uint32_t CREATURE_DISPLAY_INFO_LAYOUT = 0x7275F5F6;

// One texture of a skin: the M2 texture type it fills and its file.
struct SkinTexture {
    uint32_t type;
    uint32_t fileDataId;

    bool operator==(const SkinTexture& other) const { return type == other.type && fileDataId == other.fileDataId; }
};

// A way the displays of a model dress its skin slots (texture types 11 to 13, and 5 for retail's fourth variation).
struct CreatureSkin {
    uint32_t displayId = 0;   // The lowest display showing it.
    uint32_t displays = 0;    // How many displays show it.
    uint32_t others = 0;      // The model's other skins, which only their own displays show.
    std::vector<SkinTexture> textures;
};

// The skin most displays of each model show, from CreatureModelData and CreatureDisplayInfo of the same build as
// the models. Ties go to the lowest display.
class CreatureSkins {
    public:
    bool Load(const Db2Table& modelData, const Db2Table& displayInfo, std::string& error);

    // Null for a model no display dresses.
    const CreatureSkin* Default(uint32_t modelFileDataId) const;

    private:
    std::map<uint32_t, CreatureSkin> m_defaults;
};

}

#endif
