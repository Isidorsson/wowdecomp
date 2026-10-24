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
    const Db2Table& sections;          // ItemDisplayInfoMaterialRes: each display's body section materials, related to it.
    const Db2Table& componentTextures; // ComponentTextureFileData: each texture file's gender and race.
};

// The 3.3.5 body sections: ArmUpper, ArmLower, Hand, TorsoUpper, TorsoLower, LegUpper, LegLower, Foot. Retail's
// later ones (accessory, scalp) have no 3.3.5 column.
constexpr size_t BODY_SECTIONS = 8;

// A body section's texture per gender, the same file for a texture of both; 0 for none.
struct BodyTexture {
    uint32_t male = 0;
    uint32_t female = 0;

    bool operator==(const BodyTexture& other) const { return male == other.male && female == other.female; }
};

struct RetailItemDisplay {
    uint32_t id = 0;
    std::array<uint32_t, 2> models = {};   // Per side (left, right): the model file, 0 for none.
    std::array<uint32_t, 2> skins = {};    // Per side: the object skin file, 0 for none.
    std::array<uint32_t, 3> geosetGroups = {};
    uint32_t icon = 0;
    // Per section, one file for every race, as wow-toolbox-editor gear_textures.rs chooses: the race 0 files, else
    // the human's, else the lowest race's; a file for both genders wins over gendered ones, as the client asks for
    // "_U" first; the lowest file on a tie.
    std::array<BodyTexture, BODY_SECTIONS> body = {};
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
