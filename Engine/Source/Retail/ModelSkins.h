#ifndef ENGINE_SOURCE_RETAIL_MODELSKINS_H
#define ENGINE_SOURCE_RETAIL_MODELSKINS_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/Retail/CreatureTables.h"
#include "Engine/Source/Retail/ItemTables.h"

namespace Retail {

// One texture of a skin: the M2 texture type it fills and its file.
struct SkinTexture {
    uint32_t type;
    uint32_t fileDataId;

    bool operator==(const SkinTexture& other) const { return type == other.type && fileDataId == other.fileDataId; }
};

// A way the displays of a model dress the texture slots it leaves to them: a creature's skin (types 11 to 13, and 5
// for retail's fourth variation), an item's object skin (type 2) and its other layers.
struct ModelSkin {
    uint32_t displayId = 0;   // The lowest display showing it.
    uint32_t displays = 0;    // How many displays show it.
    uint32_t others = 0;      // The model's other skins, which only their own displays show.
    std::vector<SkinTexture> textures;   // A creature's in variation order; an item's by type, then file.
    std::vector<Geoset> geosets;         // A creature's, sorted; an item has none.
};

// The item tables, all of one build.
struct ItemSkinTables {
    const Db2Table& modelFiles;     // ModelFileData: the model files of each model resource.
    const Db2Table& displays;       // ItemDisplayInfo: each display's model resources and object skins.
    const Db2Table& layers;         // ItemDisplayInfoModelMatRes: further layers, related to their display.
    const Db2Table& textureFiles;   // TextureFileData: the files of each material resource.
};

// The skin most displays of each model show, creature and item displays pooled, ties to the lowest display. Displays
// of the same textures but other geosets show skins of their own, as in the downporter. The creature and item halves
// load separately, so a table one of them lacks leaves the other.
class ModelSkins {
    public:
    bool LoadCreatures(const CreatureTables& tables, std::string& error);

    // A display missing a layer type its colourway (model resource and object skin) has elsewhere takes the file
    // most of the colourway's displays name, the lowest on a tie.
    bool LoadItems(const ItemSkinTables& tables, std::string& error);

    // Null for a model no display dresses.
    const ModelSkin* Default(uint32_t modelFileDataId) const;

    private:
    // Counts display under the model's variant with the same textures and geosets, or starts one. No textures, no
    // variant.
    void Add(uint32_t model, uint32_t display, const std::vector<SkinTexture>& textures, const std::vector<Geoset>& geosets);
    void Rank();

    std::map<uint32_t, std::vector<ModelSkin>> m_variants;
    std::map<uint32_t, ModelSkin> m_defaults;
};

}

#endif
