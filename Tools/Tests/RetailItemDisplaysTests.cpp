#include "Engine/Source/Retail/RetailItemDisplays.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/ItemFixtures.h"

using Retail::RetailItemDisplay;
using Retail::RetailItemDisplays;

// The client serves an item display id its own ItemDisplayInfo lacks from retail's tables, so held items,
// shoulders, cloaks and icons show retail gear without a downported DBC (wow-toolbox-editor casc/gear.rs).

// CHECK is a macro, so a template argument list's comma would split its argument.
using Sides = std::array<uint32_t, 2>;
using Groups = std::array<uint32_t, 3>;

constexpr uint32_t MACE = 8266369;
constexpr uint32_t MACE_DISPLAY = 748769;
constexpr uint32_t MACE_SKIN = 8267107;

struct Fixture {
    std::vector<ItemDisplayRow> displays;
    std::vector<std::pair<uint32_t, uint32_t>> modelFiles;   // File, model resource.
    std::vector<ComponentModelRow> componentModels;
    std::vector<TextureFileRow> textureFiles;
    std::vector<AppearanceRow> appearances;
    std::vector<SectionRow> sections;
    std::vector<ComponentTextureRow> componentTextures;
};

static RetailItemDisplays Loaded(const Fixture& fixture) {
    RetailItemDisplays retail;
    std::string error;
    if (!retail.Load({ ItemDisplays(fixture.displays), ModelFiles(fixture.modelFiles), ComponentModels(fixture.componentModels),
            TextureFiles(fixture.textureFiles), Appearances(fixture.appearances), Sections(fixture.sections), ComponentTextures(fixture.componentTextures) },
            error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return retail;
}

TEST(AWeaponDisplayNamesItsModelAndObjectSkin) {
    Fixture fixture;
    fixture.displays = { { MACE_DISPLAY, { 70, 0 }, { 300, 0 } } };
    fixture.modelFiles = { { MACE, 70 } };
    fixture.textureFiles = { { MACE_SKIN, 0, 300 }, { 8267999, 1, 300 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(MACE_DISPLAY);

    CHECK(display && display->models == Sides({ MACE, 0 }));
    CHECK(display && display->skins == Sides({ MACE_SKIN, 0 }));
}

TEST(AShoulderPairTakesTheFileOfEachSide) {
    Fixture fixture;
    fixture.displays = { { 50, { 80, 80 }, { 400, 401 } } };
    fixture.modelFiles = { { 9002, 80 }, { 9001, 80 } };
    fixture.componentModels = { { 9001, 2, 0, 0 }, { 9002, 2, 0, 1 } };
    fixture.textureFiles = { { 7400, 0, 400 }, { 7401, 0, 401 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(50);

    CHECK(display && display->models == Sides({ 9001, 9002 }));
    CHECK(display && display->skins == Sides({ 7400, 7401 }));
}

TEST(AResourceWithRaceFilesStartsFromItsHumanMaleFile) {
    Fixture fixture;
    fixture.displays = { { 51, { 90, 0 }, { 0, 0 } } };
    fixture.modelFiles = { { 9099, 90 }, { 9100, 90 }, { 9101, 90 } };
    fixture.componentModels = { { 9099, 1, 1, 0 }, { 9100, 0, 1, 0 }, { 9101, 0, 2, 0 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(51);

    CHECK(display && display->models == Sides({ 9100, 0 }));
}

TEST(ASideWithoutAModelKeepsItsSkinForACape) {
    Fixture fixture;
    fixture.displays = { { 52, { 0, 0 }, { 500, 0 } } };
    fixture.textureFiles = { { 7500, 0, 500 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(52);

    CHECK(display && display->models == Sides({ 0, 0 }));
    CHECK(display && display->skins == Sides({ 7500, 0 }));
}

TEST(ASideWhoseModelHasNoFilesHasNoSkinEither) {
    // Retail's display 154382: a cape would otherwise draw the skin of a model that does not exist.
    Fixture fixture;
    fixture.displays = { { 57, { 99, 0 }, { 500, 0 } } };
    fixture.textureFiles = { { 7500, 0, 500 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(57);

    CHECK(display && display->models == Sides({ 0, 0 }));
    CHECK(display && display->skins == Sides({ 0, 0 }));
}

TEST(TheSkinIsTheLowestObjectSkinFileOfTheMaterial) {
    Fixture fixture;
    fixture.displays = { { 53, { 70, 0 }, { 600, 0 } } };
    fixture.modelFiles = { { MACE, 70 } };
    fixture.textureFiles = { { 700, 0, 600 }, { 650, 0, 600 }, { 600, 1, 600 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(53);

    CHECK(display && display->skins == Sides({ 650, 0 }));
}

TEST(GeosetGroupsAreTheFirstThreeOfRetailsSix) {
    Fixture fixture;
    fixture.displays = { { 54, { 0, 0 }, { 0, 0 }, { 1, 2, 3, 4, 5, 6 } } };
    const RetailItemDisplay* display = Loaded(fixture).Display(54);

    CHECK(display && display->geosetGroups == Groups({ 1, 2, 3 }));
}

TEST(TheIconIsTheLowestAppearanceOfTheDisplayWithOne) {
    Fixture fixture;
    fixture.displays = { { 55, { 0, 0 }, { 800, 0 } } };   // An all-zero section reads as damaged.
    fixture.appearances = { { 3, 55, 0 }, { 20, 55, 111 }, { 10, 55, 222 }, { 5, 56, 333 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(55);

    CHECK(display && display->icon == 222);
}

TEST(UnknownItemDisplaysAreNotServed) {
    Fixture fixture;
    fixture.displays = { { 55, { 0, 0 }, { 800, 0 } } };   // An all-zero section reads as damaged.
    RetailItemDisplays retail = Loaded(fixture);

    CHECK(retail.Display(55) != nullptr);
    CHECK(retail.Display(56) == nullptr);
}

static bool Refused(uint32_t badTable, const char* name) {
    std::vector<uint32_t> layouts = { Retail::ITEM_DISPLAY_INFO_LAYOUT, Retail::MODEL_FILE_DATA_LAYOUT, Retail::COMPONENT_MODEL_FILE_DATA_LAYOUT,
        Retail::TEXTURE_FILE_DATA_LAYOUT, Retail::ITEM_APPEARANCE_LAYOUT, Retail::ITEM_DISPLAY_INFO_MATERIAL_RES_LAYOUT,
        Retail::COMPONENT_TEXTURE_FILE_DATA_LAYOUT };
    layouts[badTable] = 0x5555;
    RetailItemDisplays retail;
    std::string error;
    bool loaded = retail.Load({ ItemDisplays({}, layouts[0]), ModelFiles({}, layouts[1]), ComponentModels({}, layouts[2]), TextureFiles({}, layouts[3]),
                                  Appearances({}, layouts[4]), Sections({}, layouts[5]), ComponentTextures({}, layouts[6]) },
        error);
    return !loaded && error.find(name) != std::string::npos;
}

TEST(UnknownItemLayoutsAreRefusedByName) {
    CHECK(Refused(0, "ItemDisplayInfo"));
    CHECK(Refused(1, "ModelFileData"));
    CHECK(Refused(2, "ComponentModelFileData"));
    CHECK(Refused(3, "TextureFileData"));
    CHECK(Refused(4, "ItemAppearance"));
    CHECK(Refused(5, "ItemDisplayInfoMaterialRes"));
    CHECK(Refused(6, "ComponentTextureFileData"));
}

// Body sections: ItemDisplayInfoMaterialRes gives a display's section materials, TextureFileData (usage 0) their
// files and ComponentTextureFileData each file's gender (0 male, 1 female, 2 either) and race (0 every race).

using Body = std::array<Retail::BodyTexture, Retail::BODY_SECTIONS>;

// An armour display with no models: one geoset group, as its row must not read as all zeros.
static Fixture Armour(const std::vector<SectionRow>& sections, const std::vector<TextureFileRow>& files, const std::vector<ComponentTextureRow>& owners) {
    Fixture fixture;
    fixture.displays = { { 60, { 0, 0 }, { 0, 0 }, { 1 } } };
    fixture.sections = sections;
    fixture.textureFiles = files;
    fixture.componentTextures = owners;
    return fixture;
}

static Body BodyOf(const Fixture& fixture) {
    const RetailItemDisplay* display = Loaded(fixture).Display(60);
    return display ? display->body : Body();
}

TEST(AnArmourDisplayNamesEachSectionsTexture) {
    // 9000 has no ComponentTextureFileData row, so it is for everyone, as 9100's row says.
    Body body = BodyOf(Armour({ { 60, 3, 900 }, { 60, 5, 901 } }, { { 9000, 0, 900 }, { 9100, 0, 901 } }, { { 9100, 2, 0 } }));

    CHECK(body[3] == Retail::BodyTexture({ 9000, 9000 }));
    CHECK(body[5] == Retail::BodyTexture({ 9100, 9100 }));
    CHECK(body[0] == Retail::BodyTexture());
}

TEST(GenderedFilesFillTheirGender) {
    Body body = BodyOf(Armour({ { 60, 0, 902 } }, { { 9010, 0, 902 }, { 9011, 0, 902 } }, { { 9010, 0, 0 }, { 9011, 1, 0 } }));

    CHECK(body[0] == Retail::BodyTexture({ 9010, 9011 }));
}

TEST(AGenderWithoutAFileGetsNone) {
    Body body = BodyOf(Armour({ { 60, 0, 902 } }, { { 9080, 0, 902 } }, { { 9080, 0, 0 }, { 1, 2, 0 } }));

    CHECK(body[0] == Retail::BodyTexture({ 9080, 0 }));
}

TEST(AFileForBothGendersWinsOverGenderedOnes) {
    Body body = BodyOf(Armour({ { 60, 1, 903 } }, { { 9021, 0, 903 }, { 9020, 0, 903 }, { 9022, 0, 903 } }, { { 9020, 2, 0 }, { 9021, 0, 0 }, { 9022, 1, 0 } }));

    CHECK(body[1] == Retail::BodyTexture({ 9020, 9020 }));
}

TEST(RaceFilesFallBackToTheHumansThenTheLowestRaces) {
    Body body = BodyOf(Armour({ { 60, 2, 904 }, { 60, 4, 905 }, { 60, 6, 906 } },
        { { 9031, 0, 904 }, { 9030, 0, 904 }, { 9041, 0, 905 }, { 9040, 0, 905 }, { 9050, 0, 906 }, { 9052, 0, 906 } },
        { { 9031, 2, 2 }, { 9030, 2, 1 }, { 9041, 2, 5 }, { 9040, 2, 3 }, { 9050, 2, 1 }, { 9052, 2, 0 } }));

    CHECK(body[2] == Retail::BodyTexture({ 9030, 9030 }));   // Human.
    CHECK(body[4] == Retail::BodyTexture({ 9040, 9040 }));   // The lowest race, 3.
    CHECK(body[6] == Retail::BodyTexture({ 9052, 9052 }));   // Every race wins over the human's.
}

TEST(OnlyObjectSkinFilesAreBodyTextures) {
    Body body = BodyOf(Armour({ { 60, 7, 907 } }, { { 9060, 1, 907 }, { 9061, 0, 907 } }, { { 9061, 2, 0 } }));

    CHECK(body[7] == Retail::BodyTexture({ 9061, 9061 }));
}

TEST(TheLowestFileWinsATie) {
    Body body = BodyOf(Armour({ { 60, 3, 908 } }, { { 9071, 0, 908 }, { 9070, 0, 908 } }, { { 9071, 2, 0 }, { 9070, 2, 0 } }));

    CHECK(body[3] == Retail::BodyTexture({ 9070, 9070 }));
}

TEST(SectionsPastTheEighthAreDropped) {
    Body body = BodyOf(Armour({ { 60, 9, 909 }, { 60, 3, 910 } }, { { 9090, 0, 909 }, { 9091, 0, 910 } }, { { 9090, 2, 0 }, { 9091, 2, 0 } }));

    Body expected;
    expected[3] = { 9091, 9091 };
    CHECK(body == expected);
}
