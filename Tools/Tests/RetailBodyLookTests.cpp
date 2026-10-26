#include <algorithm>
#include <map>
#include "Engine/Source/Retail/RetailBodyLook.h"
#include "Tools/Tests/BodyFixtures.h"
#include "Tools/Tests/Check.h"

using Retail::CustomizationGeoset;
using Retail::NpcLook;

// B4: a retail body draws its customization choices as geosets, one variant of every group the model has. A choice
// of GeosetType t and id g is mesh t * 100 + g; id 0 is no mesh (x00, which no model has: the group draws nothing).

// Groups of a retail body: hair 1 to 5, beards, sideburns, moustaches, gloves and boots, ears 701/702, eye glows
// 1701/1702 (Kul Tiran male: both additive glows), faces 3202/3203 (no 3201), eyebrows 3401/3402.
static const std::vector<uint32_t> MODEL = { 0, 1, 2, 5, 101, 102, 103, 201, 202, 301, 302, 401, 402, 501, 502, 701, 702, 1701, 1702, 3202, 3203,
    3401, 3402 };

static NpcLook Look(std::vector<CustomizationGeoset> geosets) {
    NpcLook look;
    look.geosets = std::move(geosets);
    return look;
}

static std::map<uint32_t, std::vector<uint32_t>> ByGroup(const std::vector<uint32_t>& meshes) {
    std::map<uint32_t, std::vector<uint32_t>> groups;
    for (uint32_t mesh : meshes) {
        groups[mesh / 100].push_back(mesh);
    }
    return groups;
}

static bool Shows(const std::vector<uint32_t>& meshes, uint32_t mesh) {
    return std::find(meshes.begin(), meshes.end(), mesh) != meshes.end();
}

TEST(RetailBodiesShowTheChosenHairAndFacialHair) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({ { 0, 5 }, { 1, 3 }, { 2, 2 }, { 3, 0 } }), MODEL);

    CHECK(Shows(meshes, 5) && Shows(meshes, 103) && Shows(meshes, 202));
    // Moustache 0: none of the group's meshes.
    CHECK(Shows(meshes, 300) && !Shows(meshes, 301) && !Shows(meshes, 302));
}

TEST(EyeGlowsShowOnlyWhenChosen) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({}), MODEL);
    CHECK(Shows(meshes, 1700) && !Shows(meshes, 1701) && !Shows(meshes, 1702));

    meshes = Retail::RetailBodyGeosets(Look({ { 17, 2 } }), MODEL);
    CHECK(Shows(meshes, 1702) && !Shows(meshes, 1701));
}

TEST(RetailBodiesShowTheOtherChoicesByTheirGroup) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({ { 32, 3 }, { 7, 2 }, { 34, 2 } }), MODEL);

    CHECK(Shows(meshes, 3203) && Shows(meshes, 702) && Shows(meshes, 3402));
}

TEST(AGroupWithoutAChoiceShowsItsDefaultVariant) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({}), MODEL);

    CHECK(Shows(meshes, 1) && Shows(meshes, 101) && Shows(meshes, 401) && Shows(meshes, 701) && Shows(meshes, 3401));
    // No x01 mesh: the lowest variant the model has.
    CHECK(Shows(meshes, 3202));
}

// Group 32 is the head swap: on the retail race bodies 3201 is a small stub (worgen 238 vertices, pandaren female
// and goblin 64) and 3202 the head (1859, 1083, 1396). Without a choice, x01 drew worgen and pandaren NPCs headless,
// with their eyes and hair floating.
static const std::vector<uint32_t> HEAD_SWAP_MODEL = { 0, 1, 2, 3201, 3202, 3301 };

TEST(AHeadSwapWithoutAChoiceShowsTheHeadNotTheStub) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({}), HEAD_SWAP_MODEL);

    CHECK(Shows(meshes, 3202) && !Shows(meshes, 3201));
    CHECK(Shows(meshes, 3301));
}

// Retail's default is variant 1: a retail-only group (19 and up) without an x01 mesh draws nothing unless chosen. The
// void elf body's only head attachment is 2405, tendrils drawn with a multiply blend, which the lowest-variant default
// hung across Magister Umbric's face (128688). Groups below 19 keep the lowest variant: ears have no 701, only 702.
static const std::vector<uint32_t> VOID_ELF_MODEL = { 0, 1, 2, 702, 2405, 3201, 3202, 4004, 4104 };

TEST(ARetailOnlyGroupWithoutX01DrawsNothingUnlessChosen) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({}), VOID_ELF_MODEL);

    CHECK(!Shows(meshes, 2405) && !Shows(meshes, 4004) && !Shows(meshes, 4104));
    CHECK(Shows(meshes, 702));
}

TEST(AChosenRetailOnlyVariantWithoutX01Shows) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({ { 24, 5 }, { 40, 4 }, { 41, 4 } }), VOID_ELF_MODEL);

    CHECK(Shows(meshes, 2405) && Shows(meshes, 4004) && Shows(meshes, 4104));
}

TEST(AChosenHeadSwapStillWins) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({ { 32, 1 } }), HEAD_SWAP_MODEL);

    CHECK(Shows(meshes, 3201) && !Shows(meshes, 3202));
}

TEST(AChoiceTheModelLacksShowsTheDefault) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({ { 7, 9 }, { 0, 25 } }), MODEL);

    CHECK(Shows(meshes, 701) && !Shows(meshes, 709));
    CHECK(Shows(meshes, 1) && !Shows(meshes, 25));
}

TEST(EveryGroupShowsExactlyOneVariant) {
    std::vector<uint32_t> meshes = Retail::RetailBodyGeosets(Look({ { 0, 2 }, { 0, 5 }, { 32, 3 }, { 32, 2 } }), MODEL);
    std::map<uint32_t, std::vector<uint32_t>> groups = ByGroup(meshes);

    CHECK(groups.size() == ByGroup(MODEL).size());
    for (const auto& [group, variants] : groups) {
        CHECK(variants.size() == 1);
    }
    // The body mesh 0 is always drawn by the client; it is no hair choice.
    CHECK(!Shows(meshes, 0));
    // The first choice of a group wins.
    CHECK(Shows(meshes, 2) && Shows(meshes, 3203));
}

TEST(SkinGeosetIdsAreTheSubmeshIdsOnce) {
    std::vector<uint8_t> skin = GeosetSkin({ { 0, 0, 3 }, { 702, 3, 1 }, { 5, 4, 2 }, { 702, 6, 1 } });
    std::vector<uint32_t> ids;
    std::string error;

    CHECK(Retail::SkinGeosetIds(skin, ids, error));
    CHECK(ids == std::vector<uint32_t>({ 0, 5, 702 }));
    CHECK(!Retail::SkinGeosetIds(std::vector<uint8_t>(8, 0), ids, error));
}

// A retail body's other textures (eyes 19, jewelry 20, hair 6, Worgen fur 8) come from the choices' materials, placed
// by the model's ChrModelTextureLayer rows: the target's texture type, the lowest layer of a type winning. Type 1 is
// the bake's. Rows of layout 119 (Goblin male) and materials of ext 18975 from the pinned build.
static const std::vector<Retail::TextureLayer> GOBLIN_LAYERS = { { 1, 0, 1 }, { 6, 1, 10 }, { 1, 2, 13 }, { 1, 3, 4 }, { 19, 9, 25 }, { 20, 10, 38 },
    { 19, 11, 44 } };

TEST(RetailBodyTexturesPlaceEachMaterialByItsTarget) {
    NpcLook look;
    look.materials = { { 1, 652894 }, { 13, 652867 }, { 10, 666441 }, { 25, 669498 }, { 38, 684465 }, { 44, 700000 } };
    std::vector<Retail::LookTexture> textures = Retail::RetailBodyTextures(look, GOBLIN_LAYERS);

    CHECK(textures == std::vector<Retail::LookTexture>({ { 6, 666441 }, { 19, 669498 }, { 20, 684465 } }));
}

TEST(RetailBodyTexturesSkipTargetsTheLayoutLacks) {
    NpcLook look;
    look.materials = { { 44, 700000 }, { 99, 5 } };

    CHECK(Retail::RetailBodyTextures(look, GOBLIN_LAYERS) == std::vector<Retail::LookTexture>({ { 19, 700000 } }));
    CHECK(Retail::RetailBodyTextures(NpcLook(), GOBLIN_LAYERS).empty());
}

// Facial hair (type 7, Void Elf layout 135: target 11) has no choice of its own on most rows; it takes the hair's file,
// as the installed client's facial hair takes the hair colour.
TEST(FacialHairTakesTheHairFileWithoutAChoice) {
    std::vector<Retail::TextureLayer> layers = { { 6, 2, 10 }, { 7, 3, 11 }, { 19, 10, 25 } };
    NpcLook look;
    look.materials = { { 10, 1734222 }, { 25, 3492824 } };
    CHECK(Retail::RetailBodyTextures(look, layers) == std::vector<Retail::LookTexture>({ { 6, 1734222 }, { 7, 1734222 }, { 19, 3492824 } }));

    look.materials.push_back({ 11, 42 });
    CHECK(Retail::RetailBodyTextures(look, layers) == std::vector<Retail::LookTexture>({ { 6, 1734222 }, { 7, 42 }, { 19, 3492824 } }));
}
