#include "Engine/Source/Retail/ModelSkins.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/Db2Fixtures.h"

using Retail::Db2Table;
using Retail::ItemSkinTables;
using Retail::ModelSkin;
using Retail::ModelSkins;
using Retail::SkinTexture;

// An item model's object skin (type 2) is its display's own material resource; ItemDisplayInfoModelMatRes adds
// layers (glow, collection slots) per model position in the display, and a display missing a layer type takes the
// one its colourway names most (wow-toolbox-editor casc/skins.rs add_items, casc/colourway.rs).

constexpr uint32_t MACE = 8266369;
constexpr uint32_t HELM_HUMAN = 8100001;
constexpr uint32_t HELM_ORC = 8100002;

struct ItemDisplay {
    uint32_t id;
    std::vector<uint32_t> resources;   // Two model resources, 0 for none.
    std::vector<uint32_t> materials;   // Each one's object skin material resource.
};

struct Layer {
    uint32_t display;
    uint32_t material;
    uint32_t type;
    uint32_t position;   // The model's position in the display.
};

struct TextureFile {
    uint32_t file;
    uint32_t usage;
    uint32_t material;
};

// The four tables, kept alive for the ItemSkinTables that refers to them.
struct ItemTables {
    Db2Table modelFiles;
    Db2Table displays;
    Db2Table layers;
    Db2Table textureFiles;
};

static ItemTables Tables(const std::vector<std::pair<uint32_t, uint32_t>>& models, const std::vector<ItemDisplay>& displays, const std::vector<Layer>& layers,
    const std::vector<TextureFile>& files, uint32_t layerLayout = Retail::ITEM_DISPLAY_INFO_MODEL_MAT_RES_LAYOUT) {
    std::vector<Db2Row> modelRows;
    for (const auto& model : models) {
        modelRows.push_back({ model.first, { 0, 0, 0, 0, model.second } });   // ModelResourcesID is column 4.
    }

    std::vector<Db2Row> displayRows;
    for (const ItemDisplay& display : displays) {
        std::vector<uint32_t> values(10, 0);   // ModelResourcesID[2] is column 10, ModelMaterialResourcesID[2] column 11.
        values.insert(values.end(), display.resources.begin(), display.resources.end());
        values.insert(values.end(), display.materials.begin(), display.materials.end());
        displayRows.push_back({ display.id, values });
    }

    std::vector<Db2Row> layerRows;
    std::vector<uint32_t> parents;
    for (uint32_t i = 0; i < layers.size(); i++) {
        layerRows.push_back({ i + 1, { layers[i].material, layers[i].type, layers[i].position } });
        parents.push_back(layers[i].display);
    }

    std::vector<Db2Row> fileRows;
    for (const TextureFile& file : files) {
        fileRows.push_back({ file.file, { 0, file.usage, file.material } });
    }

    std::vector<uint32_t> displayWidths(10, 1);
    displayWidths.insert(displayWidths.end(), { 2, 2 });
    return { PlainTable(Retail::MODEL_FILE_DATA_LAYOUT, { 1, 1, 1, 1, 1 }, modelRows), PlainTable(Retail::ITEM_DISPLAY_INFO_LAYOUT, displayWidths, displayRows),
        PlainTable(layerLayout, { 1, 1, 1 }, layerRows, parents), PlainTable(Retail::TEXTURE_FILE_DATA_LAYOUT, { 1, 1, 1 }, fileRows) };
}

static bool LoadInto(ModelSkins& skins, const ItemTables& tables, std::string& error) {
    return skins.LoadItems({ tables.modelFiles, tables.displays, tables.layers, tables.textureFiles }, error);
}

static ModelSkins Loaded(const ItemTables& tables) {
    ModelSkins skins;
    std::string error;
    if (!LoadInto(skins, tables, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return skins;
}

static std::vector<SkinTexture> TexturesOf(const ModelSkins& skins, uint32_t model) {
    const ModelSkin* skin = skins.Default(model);
    return skin ? skin->textures : std::vector<SkinTexture>({ { 0xFFFF, 0 } });
}

TEST(ObjectSkinsComeFromTheDisplaysMaterial) {
    // Usage 1 files of the material are not the skin.
    ModelSkins skins = Loaded(Tables({ { MACE, 70 } }, { { 100, { 70, 0 }, { 300, 0 } } }, {}, { { 8267107, 0, 300 }, { 8267999, 1, 300 } }));

    CHECK(TexturesOf(skins, MACE) == std::vector<SkinTexture>({ { 2, 8267107 } }));
}

TEST(LayersAddTheirTypes) {
    ModelSkins skins = Loaded(Tables({ { MACE, 70 } }, { { 100, { 70, 0 }, { 300, 0 } } }, { { 100, 300, 3, 0 } }, { { 8267107, 0, 300 } }));

    CHECK(TexturesOf(skins, MACE) == std::vector<SkinTexture>({ { 2, 8267107 }, { 3, 8267107 } }));
}

TEST(LayersFollowTheModelsPositionInTheDisplay) {
    // A shoulder pair: the glow layer belongs to the second model.
    ModelSkins skins = Loaded(Tables({ { 501, 70 }, { 502, 71 } }, { { 100, { 70, 71 }, { 300, 301 } } }, { { 100, 302, 3, 1 } },
        { { 9000, 0, 300 }, { 9001, 0, 301 }, { 9002, 0, 302 } }));

    CHECK(TexturesOf(skins, 501) == std::vector<SkinTexture>({ { 2, 9000 } }));
    CHECK(TexturesOf(skins, 502) == std::vector<SkinTexture>({ { 2, 9001 }, { 3, 9002 } }));
}

TEST(EveryModelOfAResourceSharesItsSkin) {
    ModelSkins skins = Loaded(Tables({ { HELM_HUMAN, 80 }, { HELM_ORC, 80 } }, { { 200, { 80, 0 }, { 400, 0 } } }, {}, { { 9100, 0, 400 } }));

    CHECK(TexturesOf(skins, HELM_HUMAN) == std::vector<SkinTexture>({ { 2, 9100 } }));
    CHECK(TexturesOf(skins, HELM_ORC) == std::vector<SkinTexture>({ { 2, 9100 } }));
}

TEST(ColourwaysCompleteMissingLayers) {
    // Displays 100 and 101 wear resource 70 in object skin 300; only 100 names the type 24 layer. Display 102 has
    // another object skin, so another colourway.
    ModelSkins skins = Loaded(Tables({ { MACE, 70 } }, { { 100, { 70, 0 }, { 300, 0 } }, { 101, { 70, 0 }, { 300, 0 } }, { 102, { 70, 0 }, { 310, 0 } } },
        { { 100, 303, 24, 0 } }, { { 9200, 0, 300 }, { 9203, 0, 303 }, { 9210, 0, 310 } }));
    const ModelSkin* skin = skins.Default(MACE);

    CHECK(skin && skin->displayId == 100 && skin->displays == 2 && skin->others == 1);
    CHECK(TexturesOf(skins, MACE) == std::vector<SkinTexture>({ { 2, 9200 }, { 24, 9203 } }));
}

TEST(CreatureAndItemSkinsLoadApart) {
    ModelSkins skins;
    std::string error;
    std::vector<uint32_t> creatureWidths(27, 1);
    creatureWidths.push_back(4);
    std::vector<uint32_t> creatureValues(27, 0);
    creatureValues[1] = 40;
    creatureValues.insert(creatureValues.end(), { 600, 0, 0, 0 });

    CHECK(skins.LoadCreatures(PlainTable(Retail::CREATURE_MODEL_DATA_LAYOUT, { 1, 1, 1 }, { { 40, { 0, 0, 7939217 } } }),
        PlainTable(Retail::CREATURE_DISPLAY_INFO_LAYOUT, creatureWidths, { { 10, creatureValues } }), error));
    CHECK(LoadInto(skins, Tables({ { MACE, 70 } }, { { 100, { 70, 0 }, { 300, 0 } } }, {}, { { 8267107, 0, 300 } }), error));
    CHECK(TexturesOf(skins, 7939217) == std::vector<SkinTexture>({ { 11, 600 } }));
    CHECK(TexturesOf(skins, MACE) == std::vector<SkinTexture>({ { 2, 8267107 } }));

    // An item table of another layout is refused by name and leaves the creature skins.
    CHECK(!LoadInto(skins, Tables({ { MACE, 70 } }, {}, {}, {}, 0x3333), error) && error.find("ItemDisplayInfoModelMatRes") != std::string::npos);
    CHECK(TexturesOf(skins, 7939217) == std::vector<SkinTexture>({ { 11, 600 } }));
}
