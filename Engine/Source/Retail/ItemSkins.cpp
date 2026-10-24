#include "Engine/Source/Retail/ModelSkins.h"
#include <algorithm>

// The item half of ModelSkins (wow-toolbox-editor casc/skins.rs add_items, casc/colourway.rs).

namespace Retail {

// Physical columns of the layouts.
constexpr size_t MODEL_FILE_RESOURCE = 4;
constexpr size_t DISPLAY_RESOURCES = 10;
constexpr size_t DISPLAY_MATERIALS = 11;
constexpr size_t LAYER_MATERIAL = 0;
constexpr size_t LAYER_TYPE = 1;
constexpr size_t LAYER_POSITION = 2;
constexpr size_t TEXTURE_USAGE = 1;
constexpr size_t TEXTURE_MATERIAL = 2;

constexpr uint32_t OBJECT_SKIN = 2;
constexpr uint32_t SKIN_USAGE = 0;

// The textures one display gives the models of one model resource.
struct Dressing {
    uint32_t display;
    uint32_t resource;
    std::vector<SkinTexture> textures;
};

// A model resource and the object skin files of a display of it.
using Colourway = std::pair<uint32_t, std::vector<uint32_t>>;
using Layers = std::vector<std::pair<uint32_t, uint32_t>>;   // Texture type, material resource.

static bool ByTypeThenFile(const SkinTexture& a, const SkinTexture& b) {
    return a.type != b.type ? a.type < b.type : a.fileDataId < b.fileDataId;
}

// The object skin and every layer, as files, by type then file. A layer naming the skin again adds nothing.
static std::vector<SkinTexture> ItemTextures(uint32_t ownSkin, const Layers& layers, const std::map<uint32_t, std::vector<uint32_t>>& filesOf) {
    Layers all = { { OBJECT_SKIN, ownSkin } };
    all.insert(all.end(), layers.begin(), layers.end());

    std::vector<SkinTexture> textures;
    for (const auto& [type, material] : all) {
        auto files = filesOf.find(material);
        if (files == filesOf.end()) {
            continue;
        }
        for (uint32_t file : files->second) {
            textures.push_back({ type, file });
        }
    }

    std::sort(textures.begin(), textures.end(), ByTypeThenFile);
    textures.erase(std::unique(textures.begin(), textures.end()), textures.end());
    return textures;
}

static bool ColourwayOf(const Dressing& dressing, Colourway& colourway) {
    colourway = { dressing.resource, {} };
    for (const SkinTexture& texture : dressing.textures) {
        if (texture.type == OBJECT_SKIN) {
            colourway.second.push_back(texture.fileDataId);
        }
    }
    return !colourway.second.empty();
}

// Per colourway and layer type, the file the most of its displays name, the lowest on a tie.
static std::map<Colourway, std::vector<SkinTexture>> SuppliedLayers(const std::vector<Dressing>& dressings) {
    std::map<Colourway, std::map<std::pair<uint32_t, uint32_t>, uint32_t>> tallies;
    for (const Dressing& dressing : dressings) {
        Colourway colourway;
        if (!ColourwayOf(dressing, colourway)) {
            continue;
        }
        for (const SkinTexture& texture : dressing.textures) {
            if (texture.type != OBJECT_SKIN) {
                tallies[colourway][{ texture.type, texture.fileDataId }]++;
            }
        }
    }

    std::map<Colourway, std::vector<SkinTexture>> supplied;
    for (const auto& [colourway, tally] : tallies) {
        std::map<uint32_t, std::pair<uint32_t, uint32_t>> best;   // Type to (file, count); the tally runs by type, then file.
        for (const auto& [texture, count] : tally) {
            auto chosen = best.find(texture.first);
            if (chosen == best.end() || count > chosen->second.second) {
                best[texture.first] = { texture.second, count };
            }
        }
        for (const auto& [type, file] : best) {
            supplied[colourway].push_back({ type, file.first });
        }
    }
    return supplied;
}

// Each dressing gains the layer types its colourway supplies and it lacks; what it names itself is kept.
static void CompleteColourways(std::vector<Dressing>& dressings) {
    std::map<Colourway, std::vector<SkinTexture>> supplied = SuppliedLayers(dressings);
    for (Dressing& dressing : dressings) {
        Colourway colourway;
        auto layers = ColourwayOf(dressing, colourway) ? supplied.find(colourway) : supplied.end();
        if (layers == supplied.end()) {
            continue;
        }
        for (const SkinTexture& layer : layers->second) {
            auto named = std::find_if(dressing.textures.begin(), dressing.textures.end(), [&](const SkinTexture& t) { return t.type == layer.type; });
            if (named == dressing.textures.end()) {
                dressing.textures.push_back(layer);
            }
        }
        std::sort(dressing.textures.begin(), dressing.textures.end(), ByTypeThenFile);
    }
}

static std::vector<Dressing> Dressings(const ItemSkinTables& tables, const std::map<uint32_t, std::vector<uint32_t>>& modelsOf) {
    std::map<uint32_t, std::vector<uint32_t>> filesOf;
    for (size_t row = 0; row < tables.textureFiles.RowCount(); row++) {
        if (tables.textureFiles.Value(row, TEXTURE_USAGE) == SKIN_USAGE) {
            filesOf[tables.textureFiles.Value(row, TEXTURE_MATERIAL)].push_back(tables.textureFiles.RowId(row));
        }
    }

    std::map<std::pair<uint32_t, uint32_t>, Layers> layersOf;   // By display and model position.
    for (size_t row = 0; row < tables.layers.RowCount(); row++) {
        if (uint32_t display = tables.layers.Relation(row)) {
            layersOf[{ display, tables.layers.Value(row, LAYER_POSITION) }].push_back({ tables.layers.Value(row, LAYER_TYPE), tables.layers.Value(row, LAYER_MATERIAL) });
        }
    }

    std::vector<Dressing> dressings;
    for (size_t row = 0; row < tables.displays.RowCount(); row++) {
        uint32_t display = tables.displays.RowId(row);
        std::vector<uint32_t> resources = tables.displays.Values(row, DISPLAY_RESOURCES);
        std::vector<uint32_t> materials = tables.displays.Values(row, DISPLAY_MATERIALS);

        for (uint32_t position = 0; position < resources.size(); position++) {
            if (!modelsOf.count(resources[position])) {
                continue;
            }
            auto layers = layersOf.find({ display, position });
            uint32_t ownSkin = position < materials.size() ? materials[position] : 0;
            dressings.push_back({ display, resources[position], ItemTextures(ownSkin, layers == layersOf.end() ? Layers() : layers->second, filesOf) });
        }
    }
    return dressings;
}

bool ModelSkins::LoadItems(const ItemSkinTables& tables, std::string& error) {
    if (!KnownLayout(tables.modelFiles, MODEL_FILE_DATA_LAYOUT, "ModelFileData", error) || !KnownLayout(tables.displays, ITEM_DISPLAY_INFO_LAYOUT, "ItemDisplayInfo", error)
        || !KnownLayout(tables.layers, ITEM_DISPLAY_INFO_MODEL_MAT_RES_LAYOUT, "ItemDisplayInfoModelMatRes", error)
        || !KnownLayout(tables.textureFiles, TEXTURE_FILE_DATA_LAYOUT, "TextureFileData", error)) {
        return false;
    }

    std::map<uint32_t, std::vector<uint32_t>> modelsOf;   // Model files by model resource.
    for (size_t row = 0; row < tables.modelFiles.RowCount(); row++) {
        modelsOf[tables.modelFiles.Value(row, MODEL_FILE_RESOURCE)].push_back(tables.modelFiles.RowId(row));
    }

    std::vector<Dressing> dressings = Dressings(tables, modelsOf);
    CompleteColourways(dressings);
    for (const Dressing& dressing : dressings) {
        for (uint32_t model : modelsOf[dressing.resource]) {
            this->Add(model, dressing.display, dressing.textures, {});
        }
    }

    this->Rank();
    return true;
}

}
