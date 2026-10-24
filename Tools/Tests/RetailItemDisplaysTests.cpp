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
};

static RetailItemDisplays Loaded(const Fixture& fixture) {
    RetailItemDisplays retail;
    std::string error;
    if (!retail.Load({ ItemDisplays(fixture.displays), ModelFiles(fixture.modelFiles), ComponentModels(fixture.componentModels),
            TextureFiles(fixture.textureFiles), Appearances(fixture.appearances) }, error)) {
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
        Retail::TEXTURE_FILE_DATA_LAYOUT, Retail::ITEM_APPEARANCE_LAYOUT };
    layouts[badTable] = 0x5555;
    RetailItemDisplays retail;
    std::string error;
    bool loaded = retail.Load({ ItemDisplays({}, layouts[0]), ModelFiles({}, layouts[1]), ComponentModels({}, layouts[2]), TextureFiles({}, layouts[3]),
                                  Appearances({}, layouts[4]) },
        error);
    return !loaded && error.find(name) != std::string::npos;
}

TEST(UnknownItemLayoutsAreRefusedByName) {
    CHECK(Refused(0, "ItemDisplayInfo"));
    CHECK(Refused(1, "ModelFileData"));
    CHECK(Refused(2, "ComponentModelFileData"));
    CHECK(Refused(3, "TextureFileData"));
    CHECK(Refused(4, "ItemAppearance"));
}
