#include <string>
#include "Engine/Source/Retail/BrowserMarshal.h"
#include "Tools/Tests/BrowserFixtures.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/ItemSparseFixture.h"
#include "Tools/Tests/RealBudget.h"

using namespace Retail;

// RetailBuildSql (MarshalBuild) and the item stats of RetailDetails, on the browser fixture with real ItemSparse rows:
// Judgment Breastplate (16958, plate chest), a copy of it (16959) and Onyxia Scale Cloak (15138), in TransmogSet 17.

static bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

static Db2Table SparseTable(bool withCopy = true) {
    Wdc5Spec spec;
    spec.flags = 0x5;
    spec.layoutHash = ITEM_SPARSE_LAYOUT;
    spec.recordSize = 328;
    spec.columns = ItemSparseColumns();
    Wdc5Section section;
    section.sparseRecords = { JudgmentBreastplateRecord(), OnyxiaScaleCloakRecord() };
    section.ids = { 16958, 15138 };
    if (withCopy) {
        section.copies = { { 16959, 16958 } };
    }
    spec.sections = { section };
    Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error, ItemSparseStrings()));
    return table;
}

static BrowserFixture BuildFixture() {
    BrowserFixture f = MakeBrowserFixture();
    f.items = TextTable(ITEM_LAYOUT, std::vector<uint32_t>(15, 1), {
        ItemRow(15138, 4, 1, 7, 16, 0),
        ItemRow(16958, 4, 4, 6, 5, 0),
        ItemRow(16959, 4, 4, 6, 5, 0),
    });
    std::vector<uint32_t> searchWidths(11, 1);
    searchWidths.push_back(5);
    searchWidths.push_back(2);
    f.searchNames = TextTable(ITEM_SEARCH_NAME_LAYOUT, searchWidths, {
        SearchNameRow(15138, "Onyxia Scale Cloak", 3, 0, 0xFFFFFFFF, 29),
        SearchNameRow(16958, "Judgment Breastplate", 4, 0, 2, 29),
        SearchNameRow(16959, "Judgment Breastplate", 4, 0, 2, 29),
    });
    f.modifiedAppearances = TextTable(ITEM_MODIFIED_APPEARANCE_LAYOUT, std::vector<uint32_t>(7, 1), {
        AppearanceOfItem(80001, 16958, 90001),
        AppearanceOfItem(80002, 15138, 90002),
        AppearanceOfItem(80003, 16959, 90003),
    });
    f.appearances = TextTable(ITEM_APPEARANCE_LAYOUT, std::vector<uint32_t>(5, 1), {
        AppearanceRow(90001, 31019, 1),
        AppearanceRow(90002, 31020, 2),
        AppearanceRow(90003, 31021, 3),
    });
    f.transmogSetItems = TextTable(TRANSMOG_SET_ITEM_LAYOUT, { 1, 1, 1 }, {
        { 900, { 17, 80001, 0 }, {}, 17 },
        { 901, { 17, 80002, 0 }, {}, 17 },
        { 902, { 17, 80003, 0 }, {}, 17 },
    });
    return f;
}

static RetailIndex BuildIndex() {
    RetailIndex index = BuildFixtureIndex(BuildFixture());
    std::string error;
    CHECK(index.AttachSparse(SparseTable(), error));
    index.SetBudget(RealBudget());
    return index;
}

static BrowserValue Build(const RetailIndex& index, const std::string& kind, uint32_t id, BuildOptions options = {}) {
    BrowserValue out;
    std::string error;
    BrowserIndexView view{ IndexState::Ready, &index, "" };
    CHECK(MarshalBuild(view, kind, id, options, out, error));
    if (!error.empty()) {
        std::printf("    (build: %s)\n", error.c_str());
    }
    return out;
}

static std::string Text(const BrowserValue& value, const char* key) {
    const BrowserValue* field = value.Get(key);
    return field ? field->text : std::string();
}

static double NumberOf(const BrowserValue& value, const char* key) {
    const BrowserValue* field = value.Get(key);
    return field ? field->number : -1;
}

TEST(AttachSparseGivesIndexedItemsTheirRows) {
    RetailIndex index = BuildIndex();
    const BrowseItem* chest = index.Item(16958);
    const BrowseItem* copy = index.Item(16959);
    CHECK(chest && chest->hasSparse && chest->sparse.statTypes[0] == 74 && chest->sparse.statPercents[1] == 7889);
    CHECK(copy && copy->hasSparse && copy->sparse.itemLevel == 29);
    CHECK(index.Budget().Ready());
}

TEST(ItemDetailsListTheRetailStats) {
    RetailIndex index = BuildIndex();
    BrowserIndexView view{ IndexState::Ready, &index, "" };
    BrowserValue out;
    std::string error;
    CHECK(MarshalDetails(view, "item", 16958, {}, out, error));
    std::string all;
    for (const BrowserValue& field : out.Get("fields")->items) {
        all += field.Get("label")->text + "=" + field.Get("value")->text + "\n";
    }
    CHECK(Contains(all, "Stat 1=Strength or Intellect, 49.09%, 7 at item level 29\n"));
    CHECK(Contains(all, "Stat 3=Crit rating, 29.09%"));
    CHECK(Contains(all, "Sockets=none\n"));
    CHECK(!Contains(all, "not read"));
    const BrowserValue* stats = out.Get("stats");
    CHECK(stats && stats->items.size() == 5 && NumberOf(stats->items[0], "type") == 74 && NumberOf(stats->items[0], "percent") == 4909);
}

TEST(ItemBuildCopiesTheSuggestedBaseWithMappedStats) {
    RetailIndex index = BuildIndex();
    BuildOptions options;
    options.numbers["itemLevel"] = 40;
    BrowserValue out = Build(index, "item", 16958, options);
    std::string sql = Text(out, "sql");
    CHECK(Contains(sql, "\nSELECT 9000000, 4, 4, `SoundOverrideSubclass`, 'Judgment Breastplate', 31019, 4, "));
    CHECK(Contains(sql, "`ContainerSlots`, 3, 4, 8, 7, 13, 32, 7, 0, 0, "));
    CHECK(Contains(sql, "`armor`, 0, 2, 2, 0, 0, 0, `delay`"));
    CHECK(Contains(sql, "\nFROM `item_template` WHERE `entry` = 39239;\n"));
    CHECK(NumberOf(out, "firstEntry") == 9000000 && NumberOf(out, "nextEntry") == 9000001 && NumberOf(out, "base") == 39239);
    const BrowserValue* lines = out.Get("lines");
    CHECK(lines && lines->items.size() == 5);
    if (lines && lines->items.size() == 5) {
        CHECK(Text(lines->items[0], "retail") == "Strength or Intellect" && Text(lines->items[0], "classic") == "Strength");
        CHECK(NumberOf(lines->items[0], "value") == 8 && NumberOf(lines->items[3], "value") == 2 && Text(lines->items[3], "classic") == "fire_res");
    }
    CHECK(out.Get("notes") && !out.Get("notes")->items.empty());
}

TEST(ItemBuildTakesTheChosenEntriesNameAndCombo) {
    RetailIndex index = BuildIndex();
    BuildOptions options;
    options.numbers = { { "itemLevel", 40 }, { "startEntry", 9100000 }, { "baseEntry", 50606 }, { "requiredLevel", 80 }, { "combo", 3 } };
    options.texts["name"] = "Judgment's \"Best\" Plate";
    std::string sql = Text(Build(index, "item", 16958, options), "sql");
    CHECK(Contains(sql, "\nSELECT 9100000, 4, 4, `SoundOverrideSubclass`, 'Judgment''s \"Best\" Plate', 31019, "));
    CHECK(Contains(sql, ", 40, 80, `RequiredSkill`"));
    CHECK(Contains(sql, "`ContainerSlots`, 3, 5, 8, "));
    CHECK(Contains(sql, "WHERE `entry` = 50606;"));
}

TEST(ItemBuildWithoutABudgetHasNoStats) {
    RetailIndex index = BuildIndex();
    BuildOptions options;
    options.numbers["itemLevel"] = 41;   // Not in the fixture's budget rows.
    BrowserValue out = Build(index, "item", 16958, options);
    CHECK(Contains(Text(out, "sql"), "`ContainerSlots`, 0, 0, 0, "));
    std::string notes;
    for (const BrowserValue& note : out.Get("notes")->items) {
        notes += note.text + "\n";
    }
    CHECK(Contains(notes, "item level 41"));
}

TEST(SetBuildWritesOnePiecePerSlotWithConsecutiveEntries) {
    RetailIndex index = BuildIndex();
    BuildOptions options;
    options.numbers["itemLevel"] = 40;
    BrowserValue out = Build(index, "set", 17, options);
    std::string sql = Text(out, "sql");
    CHECK(NumberOf(out, "pieces") == 2 && NumberOf(out, "nextEntry") == 9000002);
    CHECK(Contains(sql, "SELECT 9000000, 4, 4, ") && Contains(sql, "WHERE `entry` = 39239;"));
    CHECK(Contains(sql, "SELECT 9000001, 4, 1, `SoundOverrideSubclass`, 'Onyxia Scale Cloak', 31020, 3, ") && Contains(sql, "WHERE `entry` = 39225;"));
    options.numbers["allVariants"] = 1;
    out = Build(index, "set", 17, options);
    CHECK(NumberOf(out, "pieces") == 3 && Contains(Text(out, "sql"), "SELECT 9000002, 4, 4, ") && Contains(Text(out, "sql"), "31021"));
}

TEST(SetBuildPrefersThePieceOfASlotWithStats) {
    // 16959 comes first for the chest but has no ItemSparse row (as a set's stat-less transmog copy has no allocations).
    BrowserFixture f = BuildFixture();
    f.transmogSetItems = TextTable(TRANSMOG_SET_ITEM_LAYOUT, { 1, 1, 1 }, {
        { 902, { 17, 80003, 0 }, {}, 17 },
        { 900, { 17, 80001, 0 }, {}, 17 },
        { 901, { 17, 80002, 0 }, {}, 17 },
    });
    RetailIndex index = BuildFixtureIndex(f);
    std::string error;
    CHECK(index.AttachSparse(SparseTable(false), error));
    index.SetBudget(RealBudget());
    BuildOptions options;
    options.numbers["itemLevel"] = 40;
    BrowserValue out = Build(index, "set", 17, options);
    std::string sql = Text(out, "sql");
    CHECK(NumberOf(out, "pieces") == 2 && Contains(sql, "'Judgment Breastplate', 31019, ") && !Contains(sql, "31021"));
    CHECK(Contains(sql, "`ContainerSlots`, 3, 4, 8, "));
}

TEST(CreatureBuildCopiesTheDefaultBaseWithTheRetailDisplay) {
    RetailIndex index = BuildIndex();
    BrowserValue out = Build(index, "creature", 178184);
    std::string sql = Text(out, "sql");
    CHECK(Contains(sql, "\nSELECT 9000000, 0, 0, 0, `KillCredit1`, `KillCredit2`, 3167, 0, 0, 0, 'Stormwind Guard Captain', '', "));
    CHECK(Contains(sql, "WHERE `entry` = 15166;") && Contains(sql, "creature_model_info"));
    BuildOptions options;
    options.numbers = { { "display", 3258 }, { "otherDisplays", 1 }, { "scale", 2 }, { "startEntry", 9000500 } };
    options.texts["subname"] = "Watch";
    sql = Text(Build(index, "creature", 178184, options), "sql");
    CHECK(Contains(sql, "SELECT 9000500, 0, 0, 0, `KillCredit1`, `KillCredit2`, 3258, 3167, 0, 0, 'Stormwind Guard Captain', 'Watch', "));
    CHECK(Contains(sql, ", `speed_run`, 2, `rank`"));
}

TEST(BuildRefusesOtherKindsAndUnknownIds) {
    RetailIndex index = BuildIndex();
    BrowserIndexView view{ IndexState::Ready, &index, "" };
    BrowserValue out;
    std::string error;
    CHECK(!MarshalBuild(view, "model", 1011653, {}, out, error) && !error.empty());
    CHECK(!MarshalBuild(view, "item", 1, {}, out, error) && !error.empty());
    CHECK(!MarshalBuild(view, "bogus", 1, {}, out, error));
    BrowserIndexView building{ IndexState::Building, nullptr, "" };
    CHECK(MarshalBuild(building, "item", 16958, {}, out, error) && Text(out, "status") == "building");
}
