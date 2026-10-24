#include "Engine/Source/Retail/RetailDisplays.h"
#include "Engine/Source/Retail/RetailPath.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/CreatureFixtures.h"

using Retail::Geoset;
using Retail::RetailCreatureModel;
using Retail::RetailDisplay;
using Retail::RetailDisplays;

// The client serves a display id its own CreatureDisplayInfo lacks from retail's tables (WowClientDB GetRecord), so
// every caller (units, mounts, forms, model frames) sees retail displays without a downported DBC.

constexpr uint32_t BOAR = 7939217;

// CHECK is a macro, so a template argument list's comma would split its argument.
using Variations = std::array<uint32_t, 3>;
using GeoBox = std::array<float, 6>;

static RetailDisplays Loaded(const std::vector<CreatureModelRow>& models, const std::vector<CreatureDisplayRow>& displays, const std::vector<GeosetRow>& geosets = {}) {
    RetailDisplays retail;
    std::string error;
    if (!retail.Load({ CreatureModels(models), CreatureDisplays(displays), DisplayGeosets(geosets) }, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return retail;
}

TEST(ADisplayNamesItsModelAndFirstThreeVariations) {
    RetailDisplays retail = Loaded({ { 40, BOAR } }, { { 144739, 40, { 7939219, 7939221, 7939222, 7939220 } } });
    const RetailDisplay* display = retail.Display(144739);
    const RetailCreatureModel* model = retail.Model(40);

    CHECK(display && display->modelId == 40);
    CHECK(display && display->variations == Variations({ 7939219, 7939221, 7939222 }));
    CHECK(model && model->fileDataId == BOAR);
}

TEST(DisplayScaleAlphaAndSizeClassComeFromRetail) {
    RetailDisplays retail = Loaded({ { 40, BOAR } }, { { 7, 40, { 1 }, 1.5f, 128, 3 } });
    const RetailDisplay* display = retail.Display(7);

    CHECK(display && display->scale == 1.5f && display->alpha == 128 && display->sizeClass == 3);
}

TEST(ModelBoundsAndSizesComeFromRetail) {
    CreatureModelRow row = { 40, BOAR, { -1, -2, 0, 1, 2, 3 }, 0.75f, 2.5f, 1.25f, 1.75f, 2 };
    RetailDisplays retail = Loaded({ row }, { { 7, 40, { 1 } } });
    const RetailCreatureModel* model = retail.Model(40);

    CHECK(model && model->geoBox == GeoBox({ -1, -2, 0, 1, 2, 3 }));
    CHECK(model && model->collisionWidth == 0.75f && model->collisionHeight == 2.5f);
    CHECK(model && model->modelScale == 1.25f && model->mountHeight == 1.75f && model->sizeClass == 2);
}

TEST(UnknownIdsAndDisplaysWithoutTheirModelAreNotServed) {
    RetailDisplays retail = Loaded({ { 40, BOAR } }, { { 7, 40, { 1 } }, { 8, 99, { 1 } } });

    CHECK(retail.Display(7) != nullptr);
    CHECK(retail.Display(8) == nullptr);
    CHECK(retail.Display(9) == nullptr);
    CHECK(retail.Model(99) == nullptr);
}

TEST(EachDisplayPacksOnlyItsOwnGeosets) {
    RetailDisplays retail = Loaded({ { 40, BOAR } }, { { 7, 40, { 1 } }, { 8, 40, { 1 } }, { 9, 40, { 1 } } },
        { { 7, 0, 2 }, { 7, 2, 1 }, { 8, 1, 3 } });

    CHECK(retail.Display(7) && retail.Display(7)->geosetData == 0xFFFFF1F2);
    CHECK(retail.Display(8) && retail.Display(8)->geosetData == 0xFFFFFF3F);
    CHECK(retail.Display(9) && retail.Display(9)->geosetData == 0);
}

TEST(GeosetDataHasOneNibblePerGroup) {
    CHECK(Retail::PackGeosetData({}) == 0);
    CHECK(Retail::PackGeosetData({ { 0, 1 } }) == 0xFFFFFFF1);
    CHECK(Retail::PackGeosetData({ { 7, 15 }, { 3, 4 } }) == 0xFFFF4FFF);
}

TEST(GeosetsTheNibblesCannotNameStayHidden) {
    // Group 9 has no nibble; variant 16 does not fit one; variant 0 would draw every variant of the group.
    CHECK(Retail::PackGeosetData({ { 8, 1 }, { 1, 16 }, { 2, 0 } }) == 0xFFFFFFFF);
}

TEST(UnknownGeosetLayoutsAreRefused) {
    RetailDisplays retail;
    std::string error;

    CHECK(!retail.Load({ CreatureModels({}), CreatureDisplays({}), DisplayGeosets({}, 0x4444) }, error));
    CHECK(error.find("CreatureDisplayInfoGeosetData") != std::string::npos);
}

TEST(ServedModelsHaveIdsAboveEveryStockId) {
    uint32_t retailId = 0;

    CHECK(Retail::SyntheticModelId(40) == 0x01000028);
    CHECK(Retail::RetailModelIdOf(0x01000028, retailId) && retailId == 40);
    CHECK(!Retail::RetailModelIdOf(40, retailId));
}

TEST(ServedNamesResolveToTheRetailFiles) {
    CHECK(Retail::CreatureModelName(BOAR) == "retail\\7939217.m2");
    CHECK(Retail::ClaimsPath(Retail::NormalizePath(Retail::CreatureModelName(BOAR).c_str()), Retail::AliasTable()));
    CHECK(Retail::VariationName(7939219) == "7939219");
    CHECK(Retail::VariationName(0).empty());
}
