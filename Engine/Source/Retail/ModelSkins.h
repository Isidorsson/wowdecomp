#ifndef ENGINE_SOURCE_RETAIL_MODELSKINS_H
#define ENGINE_SOURCE_RETAIL_MODELSKINS_H

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
constexpr uint32_t MODEL_FILE_DATA_FDID = 1337833;
constexpr uint32_t MODEL_FILE_DATA_LAYOUT = 0x2AE4E788;
constexpr uint32_t ITEM_DISPLAY_INFO_FDID = 1266429;
constexpr uint32_t ITEM_DISPLAY_INFO_LAYOUT = 0x9F3AB8A9;
constexpr uint32_t ITEM_DISPLAY_INFO_MODEL_MAT_RES_FDID = 4050937;
constexpr uint32_t ITEM_DISPLAY_INFO_MODEL_MAT_RES_LAYOUT = 0x52510D63;
constexpr uint32_t TEXTURE_FILE_DATA_FDID = 982459;
constexpr uint32_t TEXTURE_FILE_DATA_LAYOUT = 0xBD7C74C2;

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
};

// The item tables, all of one build.
struct ItemSkinTables {
    const Db2Table& modelFiles;     // ModelFileData: the model files of each model resource.
    const Db2Table& displays;       // ItemDisplayInfo: each display's model resources and object skins.
    const Db2Table& layers;         // ItemDisplayInfoModelMatRes: further layers, related to their display.
    const Db2Table& textureFiles;   // TextureFileData: the files of each material resource.
};

// The skin most displays of each model show, creature and item displays pooled, ties to the lowest display. The
// creature and item halves load separately, so a table one of them lacks leaves the other.
class ModelSkins {
    public:
    bool LoadCreatures(const Db2Table& modelData, const Db2Table& displayInfo, std::string& error);

    // A display missing a layer type its colourway (model resource and object skin) has elsewhere takes the file
    // most of the colourway's displays name, the lowest on a tie.
    bool LoadItems(const ItemSkinTables& tables, std::string& error);

    // Null for a model no display dresses.
    const ModelSkin* Default(uint32_t modelFileDataId) const;

    private:
    // Counts display under the model's variant with the same textures, or starts one. No textures, no variant.
    void Add(uint32_t model, uint32_t display, const std::vector<SkinTexture>& textures);
    void Rank();

    std::map<uint32_t, std::vector<ModelSkin>> m_variants;
    std::map<uint32_t, ModelSkin> m_defaults;
};

// Refuses table, by name, when its layout is not the one the loaders read.
bool KnownLayout(const Db2Table& table, uint32_t layout, const char* name, std::string& error);

}

#endif
