#include <algorithm>
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
