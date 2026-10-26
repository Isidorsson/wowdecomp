#include "Engine/Source/Retail/RetailDisplayChoice.h"
#include "Engine/Source/Retail/RetailDisplays.h"
#include "Tools/Tests/Check.h"

using Retail::CreatureDisplays;
using Retail::DisplayRow;
using Retail::OverlayRetailDisplay;
using Retail::ParseCreatureDisplays;
using Retail::RetailDisplay;
using Retail::RetailDisplayWins;
using Retail::STOCK_CREATURE_DISPLAY_MAX;
using Retail::StockDisplayKind;

static const StockDisplayKind CREATURE = StockDisplayKind::Creature;

static RetailDisplay PlainDisplay(uint32_t id) {
    RetailDisplay display;
    display.id = id;
    display.modelId = 2423;
    display.scale = 0.4f;
    display.alpha = 200;
    display.sizeClass = 3;
    display.variations = { 123795, 0, 0 };
    display.fourthVariation = 123799;
    display.geosetData = 0x21;
    return display;
}

static DisplayRow StockRow() {
    DisplayRow row;
    row.id = 25002;
    row.modelId = 2423;
    row.soundId = 2275;
    row.extendedDisplayId = 0;
    row.scale = 1.5f;
    row.alpha = 255;
    row.variations = { "ETHERIALSKIN_PURPLE", "", "" };
    row.portrait = "PortraitEthereal";
    row.sizeClass = 1;
    row.bloodId = 4;
    row.npcSoundId = 77;
    row.particleColorId = 9;
    row.geosetData = 0;
    row.objectEffectPackageId = 12;
    return row;
}

TEST(CreatureDisplaysParsesItsThreeModes) {
    CreatureDisplays mode = CreatureDisplays::All;
    CHECK(ParseCreatureDisplays("stock", mode) && mode == CreatureDisplays::Stock);
    CHECK(ParseCreatureDisplays("models", mode) && mode == CreatureDisplays::Models);
    CHECK(ParseCreatureDisplays("all", mode) && mode == CreatureDisplays::All);
    CHECK(!ParseCreatureDisplays("retail", mode));
    CHECK(!ParseCreatureDisplays("", mode));
}

TEST(StockModeKeepsEveryStockRow) {
    RetailDisplay display = PlainDisplay(25002);
    CHECK(!RetailDisplayWins(CreatureDisplays::Stock, 25002, CREATURE, &display));
}

TEST(ModelsModeServesRetailForAPlainStockDisplay) {
    RetailDisplay display = PlainDisplay(25002);
    CHECK(RetailDisplayWins(CreatureDisplays::Models, 25002, CREATURE, &display));
    CHECK(RetailDisplayWins(CreatureDisplays::All, 25002, CREATURE, &display));
}

// Ids past the stock range are the admin's own rows; retail reuses those ids for unrelated creatures.
TEST(RowsPastTheStockRangeAlwaysWin) {
    RetailDisplay last = PlainDisplay(STOCK_CREATURE_DISPLAY_MAX);
    RetailDisplay custom = PlainDisplay(STOCK_CREATURE_DISPLAY_MAX + 1);
    CHECK(RetailDisplayWins(CreatureDisplays::All, STOCK_CREATURE_DISPLAY_MAX, CREATURE, &last));
    CHECK(!RetailDisplayWins(CreatureDisplays::All, STOCK_CREATURE_DISPLAY_MAX + 1, CREATURE, &custom));
    CHECK(!RetailDisplayWins(CreatureDisplays::All, 0, CREATURE, &custom));
}

TEST(ADisplayRetailLacksKeepsItsStockRow) {
    CHECK(!RetailDisplayWins(CreatureDisplays::All, 25002, CREATURE, nullptr));
}

// Humanoids draw through an Extra row and the NPC bake path: only "all" hands them to retail, whichever side has it.
TEST(ModelsModeLeavesHumanoidDisplaysStock) {
    RetailDisplay plain = PlainDisplay(1291);
    RetailDisplay humanoid = PlainDisplay(1291);
    humanoid.extendedDisplayId = 61;

    CHECK(!RetailDisplayWins(CreatureDisplays::Models, 1291, StockDisplayKind::NpcHumanoid, &plain));
    CHECK(!RetailDisplayWins(CreatureDisplays::Models, 1291, CREATURE, &humanoid));
    CHECK(RetailDisplayWins(CreatureDisplays::All, 1291, StockDisplayKind::NpcHumanoid, &humanoid));
}

// A character body without an Extra row is a player's own display (Human Male is 49): the character's customization
// skins it, and retail's row of that id is a bare body. It stays stock in every mode.
TEST(PlayerBodyDisplaysAlwaysStayStock) {
    RetailDisplay bareBody = PlainDisplay(49);
    CHECK(!RetailDisplayWins(CreatureDisplays::Models, 49, StockDisplayKind::PlayerBody, &bareBody));
    CHECK(!RetailDisplayWins(CreatureDisplays::All, 49, StockDisplayKind::PlayerBody, &bareBody));
}

TEST(OverlayTakesTheLookFromRetail) {
    DisplayRow row = OverlayRetailDisplay(StockRow(), PlainDisplay(25002));

    CHECK(row.modelId == Retail::SyntheticModelId(2423));
    CHECK(row.variations[0] == Retail::VariationName(123795));
    CHECK(row.variations[1].empty() && row.variations[2].empty());
    CHECK(row.fourthVariation == Retail::VariationName(123799));
    CHECK(row.scale == 0.4f);
    CHECK(row.alpha == 200);
    CHECK(row.geosetData == 0x21);
}

// Sound, blood, NPC sounds, particle colours and effects index stock tables a retail row's ids do not.
TEST(OverlayKeepsWhatOnlyTheStockRowCanMean) {
    DisplayRow row = OverlayRetailDisplay(StockRow(), PlainDisplay(25002));

    CHECK(row.id == 25002);
    CHECK(row.soundId == 2275);
    CHECK(row.npcSoundId == 77);
    CHECK(row.bloodId == 4);
    CHECK(row.particleColorId == 9);
    CHECK(row.objectEffectPackageId == 12);
    CHECK(row.portrait == "PortraitEthereal");
    CHECK(row.sizeClass == 1);
}

// The retail model draws plain unless the NPC path serves the retail Extra row afterwards.
TEST(OverlayDropsAStockExtraRow) {
    DisplayRow stock = StockRow();
    stock.extendedDisplayId = 61;
    CHECK(OverlayRetailDisplay(stock, PlainDisplay(25002)).extendedDisplayId == 0);
}
