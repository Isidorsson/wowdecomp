#include <algorithm>
#include <string>
#include "Engine/Source/Retail/BuildSql.h"
#include "Tools/Tests/Check.h"

using namespace Retail;

// The SQL builders (BuildSql.h): INSERT ... SELECT from a base row, overriding only the named columns.

static bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

static size_t Count(const std::string& text, const std::string& part) {
    size_t count = 0;
    for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) {
        count++;
    }
    return count;
}

TEST(SqlQuoteEscapesQuotesBackslashesAndControls) {
    CHECK(SqlQuote("Dal'Rend's Arms") == "'Dal''Rend''s Arms'");
    CHECK(SqlQuote("a\\b") == "'a\\\\b'");
    CHECK(SqlQuote("one\ntwo\r\t\x1A") == "'one\\ntwo\\r\\t\\Z'");
    CHECK(SqlQuote(std::string("nul\0x", 5)) == "'nul\\0x'");
    CHECK(SqlQuote("") == "''");
}

TEST(TheColumnListsAreTheWorldSchema) {
    // SHOW COLUMNS of the TrinityCore 3.3.5 world DB (tc-mariadb): 139 and 59 columns.
    const std::vector<std::string>& items = ItemTemplateColumns();
    CHECK(items.size() == 139 && items.front() == "entry" && items.back() == "VerifiedBuild");
    CHECK(std::find(items.begin(), items.end(), "stat_value10") != items.end() && std::find(items.begin(), items.end(), "socketColor_3") != items.end());
    const std::vector<std::string>& creatures = CreatureTemplateColumns();
    CHECK(creatures.size() == 59 && creatures.front() == "entry" && creatures[6] == "modelid1" && creatures.back() == "VerifiedBuild");
}

TEST(InsertFromBaseOverridesOnlyTheNamedColumns) {
    std::string error;
    std::string sql = InsertFromBase("t", { "entry", "name", "kept" }, 7, { { "entry", "9000000" }, { "name", "'x'" } }, error);
    CHECK(sql == "DELETE FROM `t` WHERE `entry` = 9000000;\nINSERT INTO `t` (`entry`, `name`, `kept`)\nSELECT 9000000, 'x', `kept`\nFROM `t` WHERE `entry` = 7;\n");
    CHECK(InsertFromBase("t", { "entry" }, 7, { { "nope", "1" } }, error).empty() && Contains(error, "nope"));
}

static ItemSql PlateChest() {
    ItemSql item;
    item.entry = 9000000;
    item.base = 39239;
    item.name = "Judgment Breastplate";
    item.displayId = 31019;
    item.quality = 4;
    item.inventoryType = 5;
    item.classId = 4;
    item.subclass = 4;
    item.itemLevel = 200;
    item.stats = MapToClassic({ { 74, 49 }, { 7, 81 }, { 32, 37 }, { 51, 9 } }, ComboPrimary::First);
    item.comment = "retail item 16958 at item level 200";
    return item;
}

TEST(ItemSqlCopiesTheBaseAndSetsNameDisplayAndStats) {
    std::string error;
    std::string sql = BuildItemSql(PlateChest(), error);
    CHECK(error.empty());
    CHECK(sql.rfind("-- retail item 16958 at item level 200\nDELETE FROM `item_template` WHERE `entry` = 9000000;\nINSERT INTO `item_template` (`entry`, `class`, ", 0) == 0);
    CHECK(Contains(sql, "\nSELECT 9000000, 4, 4, `SoundOverrideSubclass`, 'Judgment Breastplate', 31019, 4, 0, 0, `BuyCount`, "));
    CHECK(Contains(sql, "`ContainerSlots`, 3, 4, 49, 7, 81, 32, 37, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "));
    CHECK(Contains(sql, "`armor`, 0, 9, 0, 0, 0, 0, `delay`"));
    CHECK(Contains(sql, "\nFROM `item_template` WHERE `entry` = 39239;\n"));
    CHECK(Count(sql, "INSERT") == 1 && Count(sql, "DELETE") == 1 && !Contains(sql, "UPDATE"));
}

// A clean item: the base gives its shape (durability, material, sheath, price), never a requirement, spell or set.
TEST(ItemSqlClearsTheBasesRequirementsSpellsAndSet) {
    std::string error;
    std::string sql = BuildItemSql(PlateChest(), error);
    CHECK(Contains(sql, ", 5, -1, -1, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0, `stackable`, "));
    CHECK(Contains(sql, "`RangedModRange`, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, '', 0, "));
    CHECK(Contains(sql, ", 0, 0, `Material`, `sheath`, 0, 0, `block`, 0, `MaxDurability`, 0, 0, "));
    CHECK(Contains(sql, "`ArmorDamageModifier`, 0, 0, 0, '', `DisenchantID`, `FoodType`, 0, 0, 0, `VerifiedBuild`"));
}

TEST(ItemSqlSetsRequiredLevelAndSocketsWhenAsked) {
    ItemSql item = PlateChest();
    item.requiredLevel = 80;
    item.socketColors = { 2, 14, 0 };
    item.name = "O'Brien \\ \"the\" bold";
    item.comment = "two\nlines";
    std::string error;
    std::string sql = BuildItemSql(item, error);
    CHECK(Contains(sql, ", 200, 80, 0, "));
    CHECK(Contains(sql, "`TotemCategory`, 2, `socketContent_1`, 14, `socketContent_2`, 0, `socketContent_3`, 0, `GemProperties`"));
    CHECK(Contains(sql, "'O''Brien \\\\ \"the\" bold'"));
    CHECK(sql.rfind("-- two lines\nDELETE", 0) == 0);
}

TEST(ItemSqlSetsRetailArmourAndDamageWhenAsked) {
    ItemSql chest = PlateChest();
    chest.armor = 812;
    std::string error;
    CHECK(Contains(BuildItemSql(chest, error), "`dmg_min1`, `dmg_max1`, `dmg_type1`, `dmg_min2`, `dmg_max2`, `dmg_type2`, 812, 0, 9, 0, 0, 0, 0, `delay`, "));
    ItemSql dagger = PlateChest();
    dagger.classId = 2;
    dagger.subclass = 15;
    dagger.weapon = true;
    dagger.damageMin = 35;
    dagger.damageMax = 60;
    dagger.damageType = 2;
    dagger.delay = 1800;
    CHECK(Contains(BuildItemSql(dagger, error), ", 0, 0, 35, 60, 2, 0, 0, 0, `armor`, 0, 9, 0, 0, 0, 0, 1800, `ammo_type`, "));
}

TEST(CreatureSqlSetsNameModelsAndScale) {
    CreatureSql creature;
    creature.entry = 9000010;
    creature.base = DEFAULT_BASE_CREATURE;
    creature.name = "Stormwind Guard";
    creature.subname = "Captain's Watch";
    creature.models = { 3258, 0, 0, 0 };
    creature.comment = "retail creature 92123";
    std::string error;
    std::string sql = BuildCreatureSql(creature, error);
    CHECK(sql.rfind("-- retail creature 92123\nDELETE FROM `creature_template` WHERE `entry` = 9000010;\nINSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, ", 0) == 0);
    CHECK(Contains(sql, "\nSELECT 9000010, 0, 0, 0, `KillCredit1`, `KillCredit2`, 3258, 0, 0, 0, 'Stormwind Guard', 'Captain''s Watch', `IconName`, "));
    CHECK(Contains(sql, ", `speed_run`, `scale`, `rank`, "));
    CHECK(Contains(sql, "\nFROM `creature_template` WHERE `entry` = 15166;\n"));
    creature.scale = 1.5f;
    sql = BuildCreatureSql(creature, error);
    CHECK(Contains(sql, ", `speed_run`, 1.5, `rank`, "));
}

TEST(SuggestedBaseItemsComeFromTheWorldDb) {
    CHECK(SuggestedBaseItem(4, 4, 5) == 39239);    // Plate chest: Chestplate of the Risen Soldier.
    CHECK(SuggestedBaseItem(4, 1, 20) == 42100);   // Cloth robe: Moonshroud Robe.
    CHECK(SuggestedBaseItem(4, 4, 20) == 39239);   // No plate robe: the plate chest.
    CHECK(SuggestedBaseItem(2, 7, 13) == 37401);   // One-hand sword.
    CHECK(SuggestedBaseItem(4, 6, 14) == 40266);   // Shield.
    CHECK(SuggestedBaseItem(9, 0, 0) == 0);
}

TEST(RetailOnlyItemClassesBecomeTheirNearest) {
    uint32_t classId = 2, subclass = 9;
    std::string note;
    ClassicItemClass(classId, subclass, note);
    CHECK(classId == 2 && subclass == 7 && !note.empty());
    classId = 4, subclass = 5, note.clear();
    ClassicItemClass(classId, subclass, note);
    CHECK(classId == 4 && subclass == 0 && !note.empty());
    classId = 4, subclass = 4, note.clear();
    ClassicItemClass(classId, subclass, note);
    CHECK(classId == 4 && subclass == 4 && note.empty());
}
