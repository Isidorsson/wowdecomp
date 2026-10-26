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
    std::vector<HelmetGeosetRow> helmetGeosets;
};

static RetailItemDisplays Loaded(const Fixture& fixture) {
    RetailItemDisplays retail;
    std::string error;
    if (!retail.Load({ ItemDisplays(fixture.displays), ModelFiles(fixture.modelFiles), ComponentModels(fixture.componentModels),
            TextureFiles(fixture.textureFiles), Appearances(fixture.appearances), Sections(fixture.sections), ComponentTextures(fixture.componentTextures),
            HelmetGeosets(fixture.helmetGeosets) },
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

TEST(ADisplayNoAppearanceNamesTakesTheIconOfTheLowestDisplayWithItsModelAndSkin) {
    // Retail's mace 748769 has no appearance; 748779, the same model and skin, has the mace icon.
    Fixture fixture;
    fixture.displays = { { MACE_DISPLAY, { 70, 0 }, { 300, 0 } }, { 748790, { 70, 0 }, { 300, 0 } }, { 748779, { 70, 0 }, { 300, 0 } },
        { 748700, { 70, 0 }, { 301, 0 } } };
    fixture.modelFiles = { { MACE, 70 } };
    fixture.textureFiles = { { MACE_SKIN, 0, 300 }, { 8267200, 0, 301 } };
    fixture.appearances = { { 1, 748790, 444 }, { 2, 748779, 8267112 }, { 3, 748700, 555 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(MACE_DISPLAY);

    CHECK(display && display->icon == 8267112);
}

TEST(WithoutOneOfItsSkinADisplayTakesTheIconOfTheLowestDisplayWithItsModel) {
    Fixture fixture;
    fixture.displays = { { MACE_DISPLAY, { 70, 0 }, { 300, 0 } }, { 748790, { 70, 0 }, { 301, 0 } }, { 748700, { 70, 0 }, { 302, 0 } } };
    fixture.modelFiles = { { MACE, 70 } };
    fixture.textureFiles = { { MACE_SKIN, 0, 300 }, { 8267200, 0, 301 }, { 8267300, 0, 302 } };
    fixture.appearances = { { 1, 748790, 444 }, { 2, 748700, 555 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(MACE_DISPLAY);

    CHECK(display && display->icon == 555);
}

TEST(ADisplayWithoutAModelOrAppearanceHasNoIcon) {
    // Every cloak has no model: sharing that says nothing of its look.
    Fixture fixture;
    fixture.displays = { { 52, { 0, 0 }, { 500, 0 } }, { 53, { 0, 0 }, { 500, 0 } } };
    fixture.textureFiles = { { 7500, 0, 500 } };
    fixture.appearances = { { 1, 53, 444 } };
    const RetailItemDisplay* display = Loaded(fixture).Display(52);

    CHECK(display && display->icon == 0);
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
        Retail::COMPONENT_TEXTURE_FILE_DATA_LAYOUT, Retail::HELMET_GEOSET_DATA_LAYOUT };
    layouts[badTable] = 0x5555;
    RetailItemDisplays retail;
    std::string error;
    bool loaded = retail.Load({ ItemDisplays({}, layouts[0]), ModelFiles({}, layouts[1]), ComponentModels({}, layouts[2]), TextureFiles({}, layouts[3]),
                                  Appearances({}, layouts[4]), Sections({}, layouts[5]), ComponentTextures({}, layouts[6]),
                                  HelmetGeosets({}, layouts[7]) },
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

TEST(AnUnknownHelmetGeosetLayoutIsRefusedByName) {
    CHECK(Refused(7, "HelmetGeosetData"));
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

// The display audit reports what was dropped: one bit per retail section, only for a section with a file.
TEST(DroppedSectionsWithFilesAreRemembered) {
    Fixture fixture = Armour({ { 60, 9, 909 }, { 60, 10, 911 }, { 60, 3, 910 } }, { { 9090, 0, 909 }, { 9091, 0, 910 } }, { { 9090, 2, 0 }, { 9091, 2, 0 } });
    const RetailItemDisplay* display = Loaded(fixture).Display(60);

    CHECK(display && display->droppedSections == (1u << 9));
}

// Helms: the client asks for Item\ObjectComponents\Head\<name>_<race prefix><M|F>, so a served helm's file is the one
// its model resource has for the character's race and gender (ComponentModelFileData: gender 0 male, 1 female,
// else either; race 0 every race).

// Resource 90: human male and female, a race 2 male, and every race's male, female and either.
static RetailItemDisplays Helms() {
    Fixture fixture;
    fixture.modelFiles = { { 9100, 90 }, { 9101, 90 }, { 9102, 90 }, { 9110, 90 }, { 9111, 90 }, { 9120, 90 } };
    fixture.componentModels = { { 9100, 0, 1, 0 }, { 9101, 1, 1, 0 }, { 9102, 0, 2, 0 }, { 9110, 0, 0, 0 }, { 9111, 1, 0, 0 }, { 9120, 2, 0, 0 } };
    return Loaded(fixture);
}

TEST(AHelmTakesTheFileOfTheRaceAndGender) {
    RetailItemDisplays retail = Helms();

    CHECK(retail.RaceFile(9100, 1, 1) == 9101);
    CHECK(retail.RaceFile(9100, 1, 0) == 9100);
    CHECK(retail.RaceFile(9101, 2, 0) == 9102);   // Any file of the resource names it.
}

TEST(ARaceWithoutAFileTakesEveryRacesOfItsGender) {
    RetailItemDisplays retail = Helms();

    CHECK(retail.RaceFile(9100, 3, 0) == 9110);
    CHECK(retail.RaceFile(9100, 3, 1) == 9111);
    CHECK(retail.RaceFile(9100, 2, 1) == 9111);   // Race 2 has only a male file.
}

TEST(AGenderWithoutAFileTakesEveryRacesForEither) {
    Fixture fixture;
    fixture.modelFiles = { { 9200, 91 }, { 9221, 91 }, { 9220, 91 }, { 9300, 92 }, { 9305, 92 }, { 9400, 93 }, { 9401, 93 } };
    // 9305's gender 3 counts as either; 9401 has no ComponentModelFileData row, so it is everyone's.
    fixture.componentModels = { { 9200, 0, 1, 0 }, { 9221, 2, 0, 0 }, { 9220, 2, 0, 0 }, { 9300, 0, 1, 0 }, { 9305, 3, 0, 0 }, { 9400, 0, 1, 0 } };
    RetailItemDisplays retail = Loaded(fixture);

    CHECK(retail.RaceFile(9200, 4, 1) == 9220);   // The lowest on a tie.
    CHECK(retail.RaceFile(9300, 5, 0) == 9305);
    CHECK(retail.RaceFile(9400, 6, 1) == 9401);
}

TEST(ARaceFileForEitherGenderBeatsEveryRacesFile) {
    Fixture fixture;
    fixture.modelFiles = { { 9700, 97 }, { 9701, 97 }, { 9702, 97 }, { 9703, 97 } };
    // Race 3 has a male file and one for either; every race has a male and a female file.
    fixture.componentModels = { { 9700, 2, 3, 0 }, { 9701, 0, 0, 0 }, { 9702, 0, 3, 0 }, { 9703, 1, 0, 0 } };
    RetailItemDisplays retail = Loaded(fixture);

    CHECK(retail.RaceFile(9701, 3, 1) == 9700);
    CHECK(retail.RaceFile(9701, 3, 0) == 9702);   // The race's gendered file beats its file for either.
    CHECK(retail.RaceFile(9700, 4, 1) == 9703);
}

TEST(AHelmNeverBorrowsAnotherRacesFile) {
    Fixture fixture;
    fixture.modelFiles = { { 9500, 94 }, { 9501, 94 } };
    fixture.componentModels = { { 9500, 0, 1, 0 }, { 9501, 1, 2, 0 } };
    RetailItemDisplays retail = Loaded(fixture);

    CHECK(retail.RaceFile(9500, 2, 0) == 0);
    CHECK(retail.RaceFile(9500, 3, 1) == 0);
    CHECK(retail.RaceFile(9500, 2, 1) == 9501);
}

TEST(TheLowestRaceFileWinsATie) {
    Fixture fixture;
    fixture.modelFiles = { { 9602, 95 }, { 9601, 95 } };
    fixture.componentModels = { { 9602, 0, 1, 0 }, { 9601, 0, 1, 0 } };

    CHECK(Loaded(fixture).RaceFile(9602, 1, 0) == 9601);
}

TEST(AnUnknownFileHasNoRaceFile) {
    RetailItemDisplays retail = Helms();

    CHECK(retail.RaceFile(12345, 1, 0) == 0);
    CHECK(retail.RaceFile(0, 1, 0) == 0);
}

// A helm's hidden geosets: HelmetGeosetData rows of race and retail geoset group, folded into the stock
// HelmetGeosetVisData masks of 1 << race per slot.

using Hidden = std::vector<Retail::HelmetGeoset>;
using Retail::HelmetMasks;
using Retail::HelmetVisMasks;

TEST(EachHiddenGroupFillsItsSlotInTheClientsOrder) {
    HelmetMasks masks = HelmetVisMasks(Hidden({ { 1, 0 }, { 2, 1 }, { 3, 2 }, { 4, 3 }, { 5, 7 }, { 6, 16 }, { 7, 17 } }));

    CHECK(masks == HelmetMasks({ 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7 }));
}

TEST(RacesHidingAGroupShareItsMask) {
    HelmetMasks masks = HelmetVisMasks(Hidden({ { 1, 0 }, { 10, 0 }, { 11, 0 }, { 1, 0 } }));

    CHECK(masks[0] == ((1 << 1) | (1 << 10) | (1 << 11)));
}

TEST(GroupsAndRacesTheClientLacksAreIgnored) {
    CHECK(HelmetVisMasks(Hidden({ { 1, 4 }, { 1, 5 }, { 1, 6 }, { 1, 8 }, { 1, 18 }, { 1, 32 } })) == HelmetMasks());
    CHECK(HelmetVisMasks(Hidden({ { 0, 0 }, { 32, 0 }, { 70, 1 } })) == HelmetMasks());
}

TEST(TheMasksCoverEveryRaceBelow32) {
    HelmetMasks masks = HelmetVisMasks(Hidden({ { 12, 0 }, { 22, 0 }, { 31, 1 } }));

    CHECK(masks[0] == ((1 << 12) | (1 << 22)));
    CHECK(static_cast<uint32_t>(masks[1]) == (1u << 31));
}

using Retail::HelmetHideSlots;

TEST(HelmetHidesNameTheSlotsOfAnyRace) {
    Hidden hidden = { { 32, 0 }, { 32, 7 }, { 84, 1 }, { 1, 3 } };

    CHECK(Retail::HelmetHidesFor(hidden, 32) == HelmetHideSlots({ true, false, false, false, true, false, false }));
    CHECK(Retail::HelmetHidesFor(hidden, 84) == HelmetHideSlots({ false, true, false, false, false, false, false }));
    CHECK(Retail::HelmetHidesFor(hidden, 1) == HelmetHideSlots({ false, false, false, true, false, false, false }));
    CHECK(Retail::HelmetHidesFor(hidden, 85) == HelmetHideSlots());
}

TEST(AHelmetVisHidesItsGroupsForARaceAbove31) {
    Fixture fixture;
    fixture.helmetGeosets = { { 4021, 32, 0 }, { 4021, 36, 17 }, { 4022, 1, 0 } };
    RetailItemDisplays retail = Loaded(fixture);

    CHECK(retail.HelmetHides(4021, 32) == HelmetHideSlots({ true, false, false, false, false, false, false }));
    CHECK(retail.HelmetHides(4021, 36) == HelmetHideSlots({ false, false, false, false, false, false, true }));
    CHECK(retail.HelmetHides(4022, 32) == HelmetHideSlots());
    CHECK(retail.HelmetHides(9999, 32) == HelmetHideSlots());
}

TEST(ADisplayNamesItsHelmetVisPerGender) {
    Fixture fixture;
    fixture.displays = { { 61, { 0, 0 }, { 0, 0 }, { 1 }, { 4021, 4022 } } };
    const RetailItemDisplay* display = Loaded(fixture).Display(61);

    CHECK(display && display->helmetVis == Sides({ 4021, 4022 }));
}

TEST(AHelmetVisHidesWhatItsRowsName) {
    Fixture fixture;
    fixture.helmetGeosets = { { 4021, 1, 0 }, { 4021, 2, 0 }, { 4021, 1, 17 }, { 4022, 3, 7 } };
    RetailItemDisplays retail = Loaded(fixture);
    const HelmetMasks* male = retail.HelmetVis(4021);
    const HelmetMasks* female = retail.HelmetVis(4022);

    CHECK(male && *male == HelmetMasks({ (1 << 1) | (1 << 2), 0, 0, 0, 0, 0, 1 << 1 }));
    CHECK(female && *female == HelmetMasks({ 0, 0, 0, 0, 1 << 3, 0, 0 }));
}

TEST(HelmetVisZeroAndUnknownIdsHideNothing) {
    Fixture fixture;
    fixture.helmetGeosets = { { 4021, 1, 0 } };
    RetailItemDisplays retail = Loaded(fixture);

    CHECK(retail.HelmetVis(4021) != nullptr);
    CHECK(retail.HelmetVis(0) == nullptr);
    CHECK(retail.HelmetVis(4023) == nullptr);
}
