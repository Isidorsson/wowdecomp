#ifndef ENGINE_SOURCE_RETAIL_RETAILITEMDISPLAYS_H
#define ENGINE_SOURCE_RETAIL_RETAILITEMDISPLAYS_H

#include <array>
#include <cstdint>
#include <map>
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
    const Db2Table& helmetGeosets;     // HelmetGeosetData: the race and geoset group each helmet vis hides, related to it.
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

// A helm's hidden geosets as HelmetGeosetVisDataRec::m_hideGeoset has them: per slot (retail geoset groups 0, 1, 2,
// 3, 7, 16 and 17: hair, beard, sideburns, moustache, ears, facial jewelry, eye effects) a mask of 1 << race over
// the stock races 1 to 11.
constexpr size_t HELMET_HIDE_SLOTS = 7;
using HelmetMasks = std::array<int32_t, HELMET_HIDE_SLOTS>;

// The served HelmetGeosetVisData id of retail vis id v is this plus v, above the stock ids (245 to 376).
constexpr uint32_t SYNTHETIC_HELMET_VIS_BASE = 100000;

// One HelmetGeosetData row: a ChrRaces id and the retail geoset group a helm hides for it.
struct HelmetGeoset {
    uint32_t race = 0;
    uint32_t group = 0;
};

// Groups outside the seven slots and races outside 1 to 31 (the bits of a mask) are ignored.
HelmetMasks HelmetVisMasks(const std::vector<HelmetGeoset>& hidden);

// Per slot, whether a helm hides the group for one race: what the masks say for races up to 31, for any race.
using HelmetHideSlots = std::array<bool, HELMET_HIDE_SLOTS>;
HelmetHideSlots HelmetHidesFor(const std::vector<HelmetGeoset>& hidden, uint32_t race);

// One file of a model resource: a race and gender's (a helm), a side's (a shoulder pair) or everyone's.
struct ModelFile {
    uint32_t fdid;
    uint32_t gender;
    uint32_t race;
    uint32_t position;
};

using ModelFilesByResource = std::map<uint32_t, std::vector<ModelFile>>;

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
    std::array<uint32_t, 2> helmetVis = {};   // Retail HelmetGeosetVis ids (male, female), 0 for none.
};

class RetailItemDisplays {
    public:
    bool Load(const ItemDisplayTables& tables, std::string& error);

    // Null for an id retail lacks.
    const RetailItemDisplay* Display(uint32_t id) const;

    // The file of fileDataId's model resource for race and gender (0 male, 1 female): that race and gender's, else
    // every race's of that gender, else every race's of either; never another race's, and the lowest on a tie. 0
    // for none or an unknown file.
    uint32_t RaceFile(uint32_t fileDataId, uint32_t race, uint32_t gender) const;

    // Null for vis id 0 and for an id HelmetGeosetData has no rows for.
    const HelmetMasks* HelmetVis(uint32_t retailId) const;
    // The slots a retail vis id hides for a race, races above 31 included; none for an unknown id.
    HelmetHideSlots HelmetHides(uint32_t retailId, uint32_t race) const;

    private:
    std::vector<RetailItemDisplay> m_displays;   // By id.
    ModelFilesByResource m_filesOf;
    std::map<uint32_t, uint32_t> m_resourceOf;   // By file.
    std::map<uint32_t, HelmetMasks> m_helmetVis;   // By retail vis id.
    std::map<uint32_t, std::vector<HelmetGeoset>> m_helmetHidden;   // By retail vis id.
};

}

#endif
