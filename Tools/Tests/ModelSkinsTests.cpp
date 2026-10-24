#include "Engine/Source/Retail/ModelSkins.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"
#include "Tools/Tests/CreatureFixtures.h"

using Retail::ModelSkin;
using Retail::ModelSkins;
using Retail::Db2Table;
using Retail::SkinTexture;

// A retail model leaves texture slots to its displays: a creature's skin (types 11 to 13, and 5 for retail's fourth
// variation) to CreatureDisplayInfo, an item's object skin (type 2) and layers to ItemDisplayInfo. The client loads a
// named file for any texture until a display replaces it (M2Shared.cpp, M2Model.cpp ReplaceTexture), so the skin most
// displays show is named in those slots and their types are kept.

constexpr uint32_t BOAR = 7939217;

static ModelSkins Loaded(const std::vector<CreatureDisplayRow>& displays, const std::vector<GeosetRow>& geosets = {}) {
    ModelSkins skins;
    std::string error;
    if (!skins.LoadCreatures({ CreatureModels({ { 40, BOAR }, { 41, 1001 } }), CreatureDisplays(displays), DisplayGeosets(geosets) }, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return skins;
}

TEST(TheDefaultIsTheSkinMostDisplaysShow) {
    ModelSkins skins = Loaded({ { 10, 40, { 500, 0, 0, 0 } }, { 11, 40, { 600, 0, 0, 0 } }, { 12, 40, { 600, 0, 0, 0 } } });
    const ModelSkin* skin = skins.Default(BOAR);

    CHECK(skin != nullptr);
    if (skin) {
        CHECK(skin->displayId == 11 && skin->displays == 2 && skin->others == 1);
        CHECK(skin->textures == std::vector<SkinTexture>({ { 11, 600 } }));
    }
}

TEST(TiesGoToTheLowestDisplay) {
    ModelSkins skins = Loaded({ { 21, 40, { 700, 0, 0, 0 } }, { 20, 40, { 800, 0, 0, 0 } } });
    const ModelSkin* skin = skins.Default(BOAR);

    CHECK(skin && skin->displayId == 20 && skin->textures == std::vector<SkinTexture>({ { 11, 800 } }));
}

TEST(VariationsFillTypes11To13And5) {
    ModelSkins skins = Loaded({ { 10, 40, { 1, 0, 3, 4 } } });
    const ModelSkin* skin = skins.Default(BOAR);

    CHECK(skin && skin->textures == std::vector<SkinTexture>({ { 11, 1 }, { 13, 3 }, { 5, 4 } }));
}

TEST(ModelsWithoutDressedDisplaysHaveNoDefault) {
    // Display 13 names no texture, display 14 a model CreatureModelData lacks.
    ModelSkins skins = Loaded({ { 13, 41, { 0, 0, 0, 0 } }, { 14, 99, { 900, 0, 0, 0 } } });

    CHECK(skins.Default(1001) == nullptr);
    CHECK(skins.Default(BOAR) == nullptr);
}

TEST(UnknownLayoutsAreRefused) {
    ModelSkins skins;
    std::string error;

    CHECK(!skins.LoadCreatures({ CreatureModels({ { 40, BOAR } }, 0x1111), CreatureDisplays({}), DisplayGeosets({}) }, error)
        && error.find("CreatureModelData") != std::string::npos);
    CHECK(!skins.LoadCreatures({ CreatureModels({ { 40, BOAR } }), CreatureDisplays({}, 0x2222), DisplayGeosets({}) }, error)
        && error.find("CreatureDisplayInfo") != std::string::npos);
    CHECK(!skins.LoadCreatures({ CreatureModels({ { 40, BOAR } }), CreatureDisplays({}), DisplayGeosets({}, 0x3333) }, error)
        && error.find("CreatureDisplayInfoGeosetData") != std::string::npos);
}

TEST(TheSameTexturesWithOtherGeosetsAreASkinOfTheirOwn) {
    // Displays 10 and 11 show variant 1 of group 3, displays 12 to 14 variant 2, all with the same textures.
    ModelSkins skins = Loaded({ { 10, 40, { 600 } }, { 11, 40, { 600 } }, { 12, 40, { 600 } }, { 13, 40, { 600 } }, { 14, 40, { 600 } } },
        { { 10, 2, 1 }, { 11, 2, 1 }, { 12, 2, 2 }, { 13, 2, 2 }, { 14, 2, 2 }, { 14, 2, 2 } });
    const ModelSkin* skin = skins.Default(BOAR);

    CHECK(skin && skin->displayId == 12 && skin->displays == 3 && skin->others == 1);
    CHECK(skin && skin->geosets == std::vector<Retail::Geoset>({ { 2, 2 } }));
}

// A model with a file texture named by TXID, then skin slots 11, 12, an object skin (2), 5 and 13, unless types says.
static Retail::ConvertedModel Dressed(const ModelSkin* skin, std::vector<uint32_t> types = { 0, 11, 12, 2, 5, 13 }) {
    std::vector<uint8_t> md20 = MinimalMd20();
    std::vector<std::vector<uint8_t>> textures;
    std::vector<uint32_t> ids(types.size(), 0);
    for (uint32_t type : types) {
        textures.push_back(Texture(type));
    }
    ids[0] = types[0] == 0 ? 775194 : 0;
    AddArray(md20, M2At::TEXTURES, static_cast<uint32_t>(types.size()), Concat(textures));
    Retail::ModelSources sources;
    sources.skin = skin;
    Retail::ConvertedModel model;
    std::string error;
    if (!Retail::ConvertModel(Md21File(md20, { Chunk("TXID", U32s(ids)) }), sources, model, error)) {
        std::printf("    (%s)\n", error.c_str());
        model.m2 = NewMd20();
    }
    return model;
}

static std::pair<uint32_t, std::string> TextureAt(const std::vector<uint8_t>& m2, size_t index) {
    size_t at = RecordAt(m2, M2At::TEXTURES, index, 0x10);
    return { GetU32(m2, at), StringAt(m2, at + 8) };
}

static bool Warned(const Retail::ConvertedModel& model, std::vector<const char*> parts) {
    for (const std::string& warning : model.warnings) {
        bool all = true;
        for (const char* part : parts) {
            all &= warning.find(part) != std::string::npos;
        }
        if (all) {
            return true;
        }
    }
    return false;
}

TEST(SkinSlotsNameTheDefaultSkinAndKeepTheirTypes) {
    ModelSkin skin;
    skin.displayId = 31;
    skin.displays = 4;
    skin.others = 2;
    skin.textures = { { 11, 7001 }, { 12, 7002 }, { 5, 7004 } };
    Retail::ConvertedModel model = Dressed(&skin);

    CHECK(TextureAt(model.m2, 0) == std::make_pair(uint32_t(0), std::string("retail/775194.blp")));
    CHECK(TextureAt(model.m2, 1) == std::make_pair(uint32_t(11), std::string("retail/7001.blp")));
    CHECK(TextureAt(model.m2, 2) == std::make_pair(uint32_t(12), std::string("retail/7002.blp")));
    CHECK(TextureAt(model.m2, 3) == std::make_pair(uint32_t(2), std::string()));
    CHECK(TextureAt(model.m2, 4) == std::make_pair(uint32_t(5), std::string("retail/7004.blp")));
    // The skin has no third variation.
    CHECK(TextureAt(model.m2, 5) == std::make_pair(uint32_t(13), std::string()));
    CHECK(Warned(model, { "textures 1, 2, 4", "display 31", "4 displays", "2 other skins" }));
}

TEST(ModelsWithoutADefaultSkinLeaveTheSlotsUnnamed) {
    Retail::ConvertedModel model = Dressed(nullptr);

    CHECK(TextureAt(model.m2, 1) == std::make_pair(uint32_t(11), std::string()));
    CHECK(TextureAt(model.m2, 4) == std::make_pair(uint32_t(5), std::string()));
    CHECK(!Warned(model, { "display" }));
}

TEST(ItemSlotsNameTheirObjectSkinAndLayers) {
    // The candy mace's shape: an object skin and a layer of the same file, then retail-only types 24 and 26.
    ModelSkin skin;
    skin.displayId = 748769;
    skin.displays = 1;
    skin.textures = { { 2, 8267107 }, { 3, 8267107 }, { 24, 8300000 } };
    Retail::ConvertedModel model = Dressed(&skin, { 2, 3, 24, 26 });

    CHECK(TextureAt(model.m2, 0) == std::make_pair(uint32_t(2), std::string("retail/8267107.blp")));
    CHECK(TextureAt(model.m2, 1) == std::make_pair(uint32_t(3), std::string("retail/8267107.blp")));
    // No 3.3.5 display fills a retail-only type, so it becomes a file texture; without a file it stays blank.
    CHECK(TextureAt(model.m2, 2) == std::make_pair(uint32_t(0), std::string("retail/8300000.blp")));
    CHECK(TextureAt(model.m2, 3) == std::make_pair(uint32_t(0), std::string()));
    CHECK(Warned(model, { "textures 0, 1, 2 (skin slots)", "display 748769", "1 display shows" }));
}

TEST(ATypeOfSeveralFilesNamesTheLowest) {
    ModelSkin skin;
    skin.displayId = 5;
    skin.displays = 1;
    skin.textures = { { 2, 900 }, { 2, 500 } };

    CHECK(TextureAt(Dressed(&skin, { 2 }).m2, 0) == std::make_pair(uint32_t(2), std::string("retail/500.blp")));
}
