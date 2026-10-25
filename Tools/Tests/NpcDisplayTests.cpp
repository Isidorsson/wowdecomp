#include <algorithm>
#include "Engine/Source/Retail/BodyMatch.h"
#include "Engine/Source/Retail/RetailDisplays.h"
#include "Engine/Source/Retail/RetailNpcDisplay.h"
#include "Engine/Source/Retail/RetailNpcLook.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/NpcFixtures.h"

using Retail::BakeUse;
using Retail::CustomizationGeoset;
using Retail::NpcExtra;
using Retail::NpcPlan;

static bool Has(const std::vector<CustomizationGeoset>& geosets, uint32_t type, uint32_t id) {
    return std::find(geosets.begin(), geosets.end(), CustomizationGeoset{ type, id }) != geosets.end();
}

static const Retail::BodyFits FITS = [](uint32_t, uint32_t) { return true; };

TEST(NpcTablesReadTheExtraRowItsBakesAndItems) {
    NpcTableSet set = BuildNpcTables(HumanMale165251());
    Retail::RetailNpcTables tables;
    std::string error;
    CHECK(tables.Load(set.View(), error));

    const NpcExtra* extra = tables.Extra(165251);
    CHECK(extra != nullptr);
    CHECK(tables.Extra(165252) == nullptr);
    if (!extra) {
        return;
    }
    CHECK(extra->race == 1 && extra->sex == 0);
    CHECK(extra->sdBake == 0 && extra->hdBake == 8046191);
    CHECK(extra->items[0] == 0 && extra->items[3] == 743700 && extra->items[4] == 743701 && extra->items[8] == 679445);
}

TEST(NpcTablesReadTheChoicesGeosetsAndHairColour) {
    NpcTableSet set = BuildNpcTables(HumanMale165251());
    Retail::RetailNpcTables tables;
    std::string error;
    CHECK(tables.Load(set.View(), error));
    const NpcExtra* extra = tables.Extra(165251);
    if (!extra) {
        CHECK(extra != nullptr);
        return;
    }

    CHECK(Has(extra->look.geosets, 0, 25));   // Hair style 4995.
    CHECK(Has(extra->look.geosets, 1, 3));    // Beard.
    CHECK(Has(extra->look.geosets, 3, 5));    // Moustache.
    CHECK(Has(extra->look.geosets, 2, 0));    // Sideburns.
    CHECK(extra->look.hasHairColor && extra->look.hairColorOrder == 12 && extra->look.hairTexture == 3582353);
    // Every chosen material by target: skin 1, eyes 25, hair 10, the scalp 13; target 8 only with choice 73 chosen.
    auto has = [&](uint32_t target, uint32_t file) {
        return std::find(extra->look.materials.begin(), extra->look.materials.end(), Retail::ChosenMaterial{ target, file }) != extra->look.materials.end();
    };
    CHECK(has(1, 1027776) && has(25, 3484651) && has(10, 3582353) && has(13, 1027743) && has(8, 3685141));
}

TEST(NpcTablesRefuseAnUnknownLayout) {
    NpcTableSet set = BuildNpcTables(HumanMale165251());
    set.extra = PlainTable(0x12345678, { 1 }, { { 1, { 1 } } });
    Retail::RetailNpcTables tables;
    std::string error;
    CHECK(!tables.Load(set.View(), error));
    CHECK(error.find("CreatureDisplayInfoExtra") != std::string::npos);
}

static NpcExtra Extra(uint32_t race, uint32_t sex, uint32_t sd, uint32_t hd) {
    NpcExtra extra;
    extra.id = 165251;
    extra.race = race;
    extra.sex = sex;
    extra.sdBake = sd;
    extra.hdBake = hd;
    return extra;
}

TEST(NpcPlanWarpsTheHdBakeOntoTheInstalledBody) {
    NpcPlan plan;
    std::string reason;
    CHECK(Retail::BuildNpcPlan(Extra(1, 0, 0, 8046191), FITS, plan, reason));
    CHECK(plan.bodyModelId == 49 && plan.bake == BakeUse::Warp && plan.bakeFile == 8046191);
    CHECK(plan.bakeName == "retail-8046191_bake1m.blp");

    CHECK(Retail::BuildNpcPlan(Extra(1, 1, 242604, 1049291), FITS, plan, reason));
    CHECK(plan.bodyModelId == 50 && plan.bake == BakeUse::Warp && plan.bakeName == "retail-1049291_bake1f.blp");

    CHECK(Retail::BuildNpcPlan(Extra(11, 1, 0, 5), FITS, plan, reason));
    CHECK(plan.bodyModelId == 2250);
    CHECK(Retail::BuildNpcPlan(Extra(7, 0, 0, 5), FITS, plan, reason));
    CHECK(plan.bodyModelId == 182);
}

TEST(NpcPlanServesAnSdOnlyBakeAsIs) {
    NpcPlan plan;
    std::string reason;
    CHECK(Retail::BuildNpcPlan(Extra(10, 0, 570636, 0), FITS, plan, reason));
    CHECK(plan.bodyModelId == 2208 && plan.bake == BakeUse::AsIs && plan.bakeName == "retail-570636.blp");
}

TEST(NpcPlanServesOldNpcRacesOnStockBodiesWithTheSdBake) {
    NpcPlan plan;
    std::string reason;
    CHECK(Retail::BuildNpcPlan(Extra(17, 0, 600001, 0), FITS, plan, reason));
    CHECK(plan.bodyModelId == 2604 && plan.bake == BakeUse::AsIs && plan.bakeName == "retail-600001.blp");
    CHECK(Retail::BuildNpcPlan(Extra(12, 0, 600002, 0), FITS, plan, reason));
    CHECK(plan.bodyModelId == 2299);
    CHECK(Retail::BuildNpcPlan(Extra(12, 1, 600002, 0), FITS, plan, reason));
    CHECK(plan.bodyModelId == 2298);
}

TEST(NpcPlanRefusesWhatSliceADoesNotCover) {
    NpcPlan plan;
    std::string reason;
    CHECK(!Retail::BuildNpcPlan(Extra(9, 0, 1, 0), FITS, plan, reason));
    CHECK(reason.find("race 9") != std::string::npos);
    CHECK(!Retail::BuildNpcPlan(Extra(22, 0, 1, 0), FITS, plan, reason));
    CHECK(!Retail::BuildNpcPlan(Extra(1, 0, 0, 0), FITS, plan, reason));
    CHECK(reason.find("no bake") != std::string::npos);
    CHECK(!Retail::BuildNpcPlan(Extra(17, 0, 0, 0), FITS, plan, reason));

    Retail::BodyFits misfit = [](uint32_t, uint32_t) { return false; };
    CHECK(!Retail::BuildNpcPlan(Extra(1, 0, 0, 8046191), misfit, plan, reason));
    CHECK(reason.find("body") != std::string::npos);
    CHECK(Retail::BuildNpcPlan(Extra(10, 0, 570636, 0), misfit, plan, reason));
}

TEST(SyntheticExtraIdsRoundTrip) {
    CHECK(Retail::SyntheticExtraId(165251) == 1165251);
    uint32_t id = 0;
    CHECK(Retail::RetailExtraIdOf(1165251, id) && id == 165251);
    CHECK(!Retail::RetailExtraIdOf(15476, id));
}

// --- NpcLook::Map -----------------------------------------------------------------------------------------

static Retail::InstalledLook HumanMaleLook() {
    Retail::InstalledLook look;
    look.hairStyles = { { 1, 0, 0, 0 }, { 1, 0, 1, 2 }, { 1, 0, 2, 3 }, { 1, 0, 16, 17 } };
    look.facialHair = { { 1, 0, 1, { 1, 2, 1 } }, { 1, 0, 3, { 2, 1, 2 } }, { 1, 0, 8, { 1, 1, 1 } }, { 1, 0, 0, { 1, 1, 1 } } };
    return look;
}

static Retail::NpcLook Look(std::vector<CustomizationGeoset> geosets) {
    Retail::NpcLook look;
    look.geosets = std::move(geosets);
    return look;
}

TEST(NpcLookFindsTheHairStyleByGeosetId) {
    Retail::NpcAppearance a = Retail::MapNpcLook(1, 0, Look({ { 0, 3 } }), HumanMaleLook());
    CHECK(a.hairStyle == 2 && a.notes.empty());
    a = Retail::MapNpcLook(1, 0, Look({ { 0, 17 } }), HumanMaleLook());
    CHECK(a.hairStyle == 16);
}

TEST(NpcLookTakesStyleZeroForAMissingHairStyle) {
    Retail::NpcAppearance a = Retail::MapNpcLook(1, 0, Look({ { 0, 25 } }), HumanMaleLook());
    CHECK(a.hairStyle == 0);
    CHECK(a.notes.size() == 1 && a.notes[0].find("hair style geoset 25") != std::string::npos);
}

TEST(NpcLookMapsTaurenHornsOneBelowTheInstalledId) {
    Retail::InstalledLook tauren;
    tauren.hairStyles = { { 6, 0, 0, 2 }, { 6, 0, 1, 3 }, { 6, 0, 4, 6 } };
    Retail::NpcAppearance a = Retail::MapNpcLook(6, 0, Look({ { 24, 5 } }), tauren);
    CHECK(a.hairStyle == 4 && a.notes.empty());
    // Tauren rows also carry a plain hair geoset; the horns are what CharHairGeosets lists.
    a = Retail::MapNpcLook(6, 0, Look({ { 0, 1 }, { 24, 2 } }), tauren);
    CHECK(a.hairStyle == 1 && a.notes.empty());
}

TEST(NpcLookMapsDraeneiMaleByIdNotOrder) {
    Retail::InstalledLook draenei;
    draenei.hairStyles = { { 11, 0, 7, 9 }, { 11, 0, 8, 0 }, { 11, 0, 9, 10 }, { 11, 0, 10, 11 }, { 11, 0, 13, 14 } };
    CHECK(Retail::MapNpcLook(11, 0, Look({ { 0, 0 } }), draenei).hairStyle == 8);
    CHECK(Retail::MapNpcLook(11, 0, Look({ { 0, 11 } }), draenei).hairStyle == 10);
}

TEST(NpcLookPicksTheMatchingFacialHairCombination) {
    // Beard 2, moustache 1 (retail none), sideburns 2.
    Retail::NpcAppearance a = Retail::MapNpcLook(1, 0, Look({ { 1, 2 }, { 3, 0 }, { 2, 2 } }), HumanMaleLook());
    CHECK(a.facialHair == 3 && a.notes.empty());
    // Nothing at all: every group none.
    a = Retail::MapNpcLook(1, 0, Look({}), HumanMaleLook());
    CHECK(a.facialHair == 0 && a.notes.empty());
}

TEST(NpcLookTakesTheClosestFacialHairAndSaysSo) {
    Retail::NpcAppearance a = Retail::MapNpcLook(1, 0, Look({ { 1, 3 }, { 3, 5 }, { 2, 0 } }), HumanMaleLook());
    CHECK(a.notes.size() == 1 && a.notes[0].find("facial hair") != std::string::npos);
}

TEST(NpcLookKeepsTheHairColourOrder) {
    Retail::NpcLook look = Look({});
    look.hasHairColor = true;
    look.hairColorOrder = 7;
    CHECK(Retail::MapNpcLook(1, 0, look, HumanMaleLook()).hairColor == 7);
}

// --- The served Extra row ---------------------------------------------------------------------------------

TEST(ServedNpcExtraFillsTheRowAndEmptiesUnservableSlots) {
    NpcExtra extra = Extra(1, 0, 0, 8046191);
    extra.items[0] = 100;
    extra.items[3] = 200;
    NpcPlan plan;
    std::string reason;
    CHECK(Retail::BuildNpcPlan(extra, FITS, plan, reason));
    Retail::NpcAppearance appearance;
    appearance.hairStyle = 4;
    appearance.hairColor = 3;
    appearance.facialHair = 2;

    std::vector<std::string> notes;
    Retail::NpcExtraFields fields = Retail::ServedNpcExtra(extra, plan, appearance, [](uint32_t id) { return id == 100; }, notes);
    CHECK(fields.id == 1165251 && fields.race == 1 && fields.sex == 0);
    CHECK(fields.skin == 0 && fields.face == 0 && fields.hairStyle == 4 && fields.hairColor == 3 && fields.facialHair == 2);
    CHECK(fields.items[0] == 100 && fields.items[3] == 0);
    CHECK(fields.bakeName == "retail-8046191_bake1m.blp");
    CHECK(notes.size() == 1 && notes[0].find("200") != std::string::npos);
}

// --- B7: the body of a Goblin or retail race NPC -----------------------------------------------------------------

using Retail::NpcBody;

// Real rows: display 28601 (ext 18975, goblin male on retail model 831 = GoblinMale.m2 119376, SD bake 253789), 30164
// (ext 19994, worgen male, 3141 = WorgenMale.m2 307454, bake 319801), 86324 and 85555 (ext 146013 / 145546, Mag'har on
// 10844 = OrcMale_HD 917116 / 10845 = OrcFemale_HD 949470, bakes 2029169 / 1989274), 86326 (ext 146015, Mag'har male
// on 10883 = orcmaleupright 1968587).
static NpcExtra RetailRaceExtra(uint32_t id, uint32_t race, uint32_t sex, uint32_t sd) {
    NpcExtra extra = Extra(race, sex, sd, 0);
    extra.id = id;
    return extra;
}

TEST(HdPortRaceNamesTheInstalledBodyMadeFromARetailBody) {
    CHECK(Retail::HdPortRace(917116, 0) == 2);
    CHECK(Retail::HdPortRace(949470, 1) == 2);
    CHECK(Retail::HdPortRace(1011653, 0) == 1);
    CHECK(Retail::HdPortRace(917116, 1) == 0);
    CHECK(Retail::HdPortRace(119376, 0) == 0);
    CHECK(Retail::HdPortRace(1968587, 0) == 0);
}

TEST(AGoblinDrawsOnItsRetailBodyWithItsBakeAsIs) {
    NpcPlan plan;
    std::string reason;
    CHECK(Retail::PlanNpc(RetailRaceExtra(18975, 9, 0, 253789), NpcBody{ 831, 119376 }, FITS, plan, reason));
    CHECK(plan.retailBody && plan.race == 9 && plan.bodyModelId == static_cast<int32_t>(Retail::SyntheticNpcBodyModelId(831)));
    CHECK(plan.bake == BakeUse::AsIs && plan.bakeFile == 253789 && plan.bakeName == "retail-253789_npc18975.blp");
}

TEST(AWorgenDrawsOnItsRetailBody) {
    NpcPlan plan;
    std::string reason;
    CHECK(Retail::PlanNpc(RetailRaceExtra(19994, 22, 0, 319801), NpcBody{ 3141, 307454 }, FITS, plan, reason));
    CHECK(plan.retailBody && plan.race == 22 && plan.bodyModelId == static_cast<int32_t>(Retail::SyntheticNpcBodyModelId(3141)));
    CHECK(plan.bakeName == "retail-319801_npc19994.blp");
}

TEST(AMagharOnTheOrcHdBodyDrawsOnTheInstalledOrcWithAWarpedBake) {
    NpcPlan plan;
    std::string reason;
    CHECK(Retail::PlanNpc(RetailRaceExtra(146013, 36, 0, 2029169), NpcBody{ 10844, 917116 }, FITS, plan, reason));
    CHECK(!plan.retailBody && plan.race == 2 && plan.bodyModelId == 51);
    CHECK(plan.bake == BakeUse::Warp && plan.bakeFile == 2029169 && plan.bakeName == "retail-2029169_bake2m.blp");

    CHECK(Retail::PlanNpc(RetailRaceExtra(145546, 36, 1, 1989274), NpcBody{ 10845, 949470 }, FITS, plan, reason));
    CHECK(plan.race == 2 && plan.bodyModelId == 52 && plan.bakeName == "retail-1989274_bake2f.blp");
}

TEST(AMagharKeepsItsRetailBodyWhenTheInstalledBodyIsNoPort) {
    NpcPlan plan;
    std::string reason;
    Retail::BodyFits misfit = [](uint32_t, uint32_t) { return false; };
    CHECK(Retail::PlanNpc(RetailRaceExtra(146013, 36, 0, 2029169), NpcBody{ 10844, 917116 }, misfit, plan, reason));
    CHECK(plan.retailBody && plan.race == 36 && plan.bodyModelId == static_cast<int32_t>(Retail::SyntheticNpcBodyModelId(10844)));
    CHECK(plan.bake == BakeUse::AsIs);

    CHECK(Retail::PlanNpc(RetailRaceExtra(146015, 36, 0, 2029191), NpcBody{ 10883, 1968587 }, FITS, plan, reason));
    CHECK(plan.retailBody && plan.bodyModelId == static_cast<int32_t>(Retail::SyntheticNpcBodyModelId(10883)));
}

TEST(ARetailRaceNpcWithoutABakeOrBodyIsRefused) {
    NpcPlan plan;
    std::string reason;
    CHECK(!Retail::PlanNpc(RetailRaceExtra(19994, 22, 0, 0), NpcBody{ 3141, 307454 }, FITS, plan, reason));
    CHECK(reason.find("no bake") != std::string::npos);
    CHECK(!Retail::PlanNpc(RetailRaceExtra(19994, 22, 0, 319801), NpcBody{}, FITS, plan, reason));
    CHECK(reason.find("model") != std::string::npos);
}

TEST(PlanNpcLeavesSliceAPlansAsTheyWere) {
    NpcPlan plan;
    std::string reason;
    CHECK(Retail::PlanNpc(Extra(1, 0, 0, 8046191), NpcBody{ 7661, 1011653 }, FITS, plan, reason));
    CHECK(!plan.retailBody && plan.race == 1 && plan.bodyModelId == 49 && plan.bakeName == "retail-8046191_bake1m.blp");
    CHECK(Retail::PlanNpc(Extra(12, 0, 600002, 0), NpcBody{ 1, 2 }, FITS, plan, reason));
    CHECK(!plan.retailBody && plan.bodyModelId == 2299 && plan.bakeName == "retail-600002.blp");
}

TEST(TheServedRowOfAMagharNamesTheOrc) {
    NpcPlan plan;
    std::string reason;
    NpcExtra extra = RetailRaceExtra(146013, 36, 0, 2029169);
    CHECK(Retail::PlanNpc(extra, NpcBody{ 10844, 917116 }, FITS, plan, reason));
    std::vector<std::string> notes;
    Retail::NpcExtraFields fields = Retail::ServedNpcExtra(extra, plan, Retail::NpcAppearance(), [](uint32_t) { return true; }, notes);
    CHECK(fields.race == 2 && fields.id == 1146013);
}

// A helm with no file for the served row's race (retail ships none for Earthen on display 165676's model; the race
// file never borrows another race's) would draw the missing-model cube on the head: the slot stays empty, with a note.
TEST(AHelmWithoutAFileForTheRaceIsLeftOff) {
    Retail::NpcExtraFields fields;
    fields.id = 1156199;
    fields.race = 84;
    fields.items[0] = 165676;
    fields.items[3] = 200;
    std::vector<std::string> notes;

    Retail::DropUnwearableHelm(fields, [](uint32_t) { return false; }, notes);
    CHECK(fields.items[0] == 0 && fields.items[3] == 200);
    CHECK(notes.size() == 1 && notes[0].find("165676") != std::string::npos && notes[0].find("race 84") != std::string::npos);

    fields.items[0] = 165676;
    notes.clear();
    Retail::DropUnwearableHelm(fields, [](uint32_t display) { return display == 165676; }, notes);
    CHECK(fields.items[0] == 165676 && notes.empty());
}
