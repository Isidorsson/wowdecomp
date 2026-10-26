#include <algorithm>
#include "Engine/Source/Retail/RetailBodyLook.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/NpcFixtures.h"

using Retail::ChosenMaterial;
using Retail::CreatureLook;
using Retail::LookTexture;
using Retail::NpcExtra;

// A customizable creature (the dragonriding drakes) is a retail body whose look a mount or a plain model frame draws
// without the character component: the bake on the skin, the chosen materials on the other character texture types,
// and one mesh per geoset group beside the body mesh. Rows of ext 152002 (Renewed Proto-Drake 107058) from the pinned
// build.

// ChrModelTextureLayer rows of layout 161 (ChrModel 124): type, layer, target.
static const std::vector<Retail::TextureLayer> DRAKE_LAYERS = { { 1, 0, 1 }, { 8, 1, 2 }, { 1, 2, 3 }, { 6, 3, 10 }, { 7, 4, 16 }, { 19, 5, 25 },
    { 19, 6, 44 } };

// The submesh ids of model 4278602's skin 0.
static const std::vector<uint32_t> DRAKE_MODEL = { 0, 101, 102, 103, 104, 105, 106, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
    301, 302, 303, 304, 305, 306, 307, 308, 401, 402, 403, 404, 405, 406, 407, 408, 501, 502, 503, 504, 505, 601, 602, 603, 604, 605, 701, 702, 703,
    704, 705, 706, 708, 801, 802, 3301, 3901, 3902, 3903, 3904, 4001, 4002 };

static const NpcExtra* LoadDrake(const NpcTableSet& set, Retail::RetailNpcTables& tables) {
    std::string error;
    CHECK(tables.Load(set.View(), error));
    const NpcExtra* extra = tables.Extra(152002);
    CHECK(extra != nullptr);
    return extra;
}

static bool Shows(const CreatureLook& look, uint32_t mesh) {
    return std::find(look.meshes.begin(), look.meshes.end(), mesh) != look.meshes.end();
}

TEST(ADrakesChoicesResolveToItsMaterialsWhereTheirOtherChoiceIsChosen) {
    NpcTableSet set = BuildNpcTables(ProtoDrake152002());
    Retail::RetailNpcTables tables;
    const NpcExtra* extra = LoadDrake(set, tables);
    if (!extra) {
        return;
    }

    std::vector<ChosenMaterial> materials = extra->look.materials;
    std::sort(materials.begin(), materials.end(), [](const ChosenMaterial& a, const ChosenMaterial& b) { return a.target < b.target; });
    CHECK(materials == std::vector<ChosenMaterial>({ { 1, 4278660 }, { 2, 4885868 }, { 10, 4278638 }, { 16, 4278633 }, { 25, 4261013 } }));
    CHECK(extra->sdBake == 4547582 && extra->hdBake == 0);
}

TEST(ADrakeWithoutAComponentDrawsItsBakeAndChosenTexturesByType) {
    NpcTableSet set = BuildNpcTables(ProtoDrake152002());
    Retail::RetailNpcTables tables;
    const NpcExtra* extra = LoadDrake(set, tables);
    if (!extra) {
        return;
    }

    CreatureLook look = Retail::RetailCreatureLook(extra->look, DRAKE_LAYERS, extra->sdBake, DRAKE_MODEL);
    // Skin 1 the bake, horns 8 (target 2), feathers 6 (target 10), armour plates 7 (target 16), eyes 19 (the lower of
    // its two layers, target 25). The ones the mount drew white before.
    CHECK(look.textures == std::vector<LookTexture>({ { 1, 4547582 }, { 6, 4278638 }, { 7, 4278633 }, { 8, 4885868 }, { 19, 4261013 } }));
}

TEST(ADrakeWithoutAComponentShowsTheBodyAndOneMeshPerGroup) {
    NpcTableSet set = BuildNpcTables(ProtoDrake152002());
    Retail::RetailNpcTables tables;
    const NpcExtra* extra = LoadDrake(set, tables);
    if (!extra) {
        return;
    }

    CreatureLook look = Retail::RetailCreatureLook(extra->look, DRAKE_LAYERS, extra->sdBake, DRAKE_MODEL);
    CHECK(Shows(look, 0));
    // Chosen variants: horns 101, crest 201, tail 601, armour 3901; the eyes 3301 default (no choice).
    CHECK(Shows(look, 101) && Shows(look, 201) && Shows(look, 601) && Shows(look, 3301) && Shows(look, 3901));
    // Choices of id 0 hide their group; a group's other variants stay hidden.
    for (uint32_t hidden : { 102u, 301u, 401u, 501u, 701u, 801u, 3902u, 4001u, 4002u }) {
        CHECK(!Shows(look, hidden));
    }
    CHECK(look.meshes.size() == 12);
}

TEST(ACreatureLookWithoutABakeLeavesTheSkinAlone) {
    Retail::NpcLook look;
    look.materials = { { 10, 4278638 } };
    CreatureLook creature = Retail::RetailCreatureLook(look, { { 1, 0, 1 }, { 6, 3, 10 } }, 0, { 0, 101, 102 });

    CHECK(creature.textures == std::vector<LookTexture>({ { 6, 4278638 } }));
    CHECK(creature.meshes == std::vector<uint32_t>({ 0, 101 }));
}

// On the character component (an in-world unit, a DressUpModel) the 3.3.5 groups follow items and their defaults: a
// drake drew its x01 jaw spikes 401, brow 501 and chin crest 801 that its choices of id 0 hide. The groups a choice
// names are the look's.
TEST(ADrakesChosenGroupsAreTheOnesItsChoicesName) {
    NpcTableSet set = BuildNpcTables(ProtoDrake152002());
    Retail::RetailNpcTables tables;
    const NpcExtra* extra = LoadDrake(set, tables);
    if (!extra) {
        return;
    }

    std::vector<uint32_t> chosen = Retail::RetailBodyChosenGeosets(extra->look, DRAKE_MODEL);
    CHECK(chosen == std::vector<uint32_t>({ 101, 201, 300, 400, 500, 601, 700, 800, 3901, 4000 }));
}

TEST(AGroupWithoutAChoiceIsNotChosen) {
    Retail::NpcLook look;
    look.geosets = { { 0, 5 }, { 3, 0 } };

    CHECK(Retail::RetailBodyChosenGeosets(look, { 0, 1, 5, 301, 401, 402 }) == std::vector<uint32_t>({ 5, 300 }));
    CHECK(Retail::RetailBodyChosenGeosets(Retail::NpcLook(), { 0, 401 }).empty());
}
