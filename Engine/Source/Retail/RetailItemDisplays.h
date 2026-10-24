#ifndef ENGINE_SOURCE_RETAIL_RETAILITEMDISPLAYS_H
#define ENGINE_SOURCE_RETAIL_RETAILITEMDISPLAYS_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/Db2Table.h"
#include "Engine/Source/Retail/ItemTables.h"

namespace Retail {

// Retail item displays, for the client to serve the display ids its own ItemDisplayInfo lacks (stock rows win):
// held items, shoulders, cloaks and icons (wow-toolbox-editor casc/gear.rs). A display knows no inventory type, so
// each side is described as retail has it and the client's slot picks what it uses: a model and its object skin, or
// for a side without a model (a cloak) the skin alone, which a cape draws.

// The item tables, all of one build.
struct ItemDisplayTables {
    const Db2Table& displays;          // ItemDisplayInfo: each display's model and material resources per side.
    const Db2Table& modelFiles;        // ModelFileData: the files of each model resource.
    const Db2Table& componentModels;   // ComponentModelFileData: each file's race, gender and side.
    const Db2Table& textureFiles;      // TextureFileData: the files of each material resource.
    const Db2Table& appearances;       // ItemAppearance: each display's default icon.
};

struct RetailItemDisplay {
    uint32_t id = 0;
    std::array<uint32_t, 2> models = {};   // Per side (left, right): the model file, 0 for none.
    std::array<uint32_t, 2> skins = {};    // Per side: the object skin file, 0 for none.
    std::array<uint32_t, 3> geosetGroups = {};
    uint32_t icon = 0;
};

class RetailItemDisplays {
    public:
    bool Load(const ItemDisplayTables& tables, std::string& error);

    // Null for an id retail lacks.
    const RetailItemDisplay* Display(uint32_t id) const;

    private:
    std::vector<RetailItemDisplay> m_displays;   // By id.
};

}

#endif
