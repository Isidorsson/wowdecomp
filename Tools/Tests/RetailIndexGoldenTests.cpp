#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "Engine/Source/Retail/BrowserMarshal.h"
#include "Tools/Tests/BrowserFixtures.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/ItemSparseFixture.h"
#include "Tools/Tests/RetailIndexGolden.h"
#include "Tools/Tests/RealBudget.h"

using namespace Retail;

// The index's results as the browser sees them, held against a golden taken before the index's storage was
// compacted: every search row, every entry's details and the builders' SQL over the fixtures. Run with
// WOWDECOMP_PRINT_GOLDEN=1 to print the snapshot as RetailIndexGolden.h lines.

namespace {

void Serialize(const BrowserValue& value, std::string& out) {
    char number[40];
    switch (value.type) {
        case BrowserValue::Type::Nil:
            out += "nil";
            return;
        case BrowserValue::Type::Bool:
            out += value.flag ? "true" : "false";
            return;
        case BrowserValue::Type::Number:
            std::snprintf(number, sizeof(number), "%.17g", value.number);
            out += number;
            return;
        case BrowserValue::Type::String:
            out += "'";
            for (char c : value.text) {
                out += c == '\n' ? std::string("\\n") : std::string(1, c);
            }
            out += "'";
            return;
        default:
            break;
    }
    out += "{";
    for (const BrowserValue& item : value.items) {
        Serialize(item, out);
        out += ",";
    }
    for (const auto& [key, field] : value.fields) {
        out += key + "=";
        Serialize(field, out);
        out += ",";
    }
    out += "}";
}

Db2Table GoldenSparse() {
    Wdc5Spec spec;
    spec.flags = 0x5;
    spec.layoutHash = ITEM_SPARSE_LAYOUT;
    spec.recordSize = 328;
    spec.columns = ItemSparseColumns();
    Wdc5Section section;
    section.sparseRecords = { JudgmentBreastplateRecord(), OnyxiaScaleCloakRecord() };
    section.ids = { 16958, 15138 };
    section.copies = { { 16959, 16958 } };
    spec.sections = { section };
    Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error, ItemSparseStrings()));
    return table;
}

// The browser fixture with three more items that have ItemSparse rows (a copy row among them) and share TransmogSet
// 17, so items with and without stats, both set sources and models of several users are all in it.
RetailIndex GoldenIndex() {
    BrowserFixture f = MakeBrowserFixture();
    f.items = TextTable(ITEM_LAYOUT, std::vector<uint32_t>(15, 1), {
        ItemRow(12939, 2, 7, 1, 22, 135350),
        ItemRow(12940, 2, 7, 1, 21, 135353),
        ItemRow(15138, 4, 1, 7, 16, 0),
        ItemRow(16958, 4, 4, 6, 5, 0),
        ItemRow(16959, 4, 4, 6, 5, 0),
        ItemRow(19019, 2, 7, 1, 13, 135349, 1),
        ItemRow(124913, 4, 4, 6, 5, 0),
        ItemRow(124914, 4, 4, 6, 10, 0),
    });
    std::vector<uint32_t> searchWidths(11, 1);
    searchWidths.push_back(5);
    searchWidths.push_back(2);
    f.searchNames = TextTable(ITEM_SEARCH_NAME_LAYOUT, searchWidths, {
        SearchNameRow(12939, "Dal'Rend's Tribal Guardian", 3, 0, 0xFFFFFFFF, 29),
        SearchNameRow(12940, "Dal'Rend's Sacred Charge", 3, 0, 0xFFFFFFFF, 29),
        SearchNameRow(15138, "Onyxia Scale Cloak", 3, 0, 0xFFFFFFFF, 29),
        SearchNameRow(16958, "Judgment Breastplate", 4, 0, 2, 29),
        SearchNameRow(16959, "Judgment Breastplate", 4, 0, 2, 29),
        SearchNameRow(19019, "Thunderfury, Blessed Blade of the Windseeker", 5, 0, 0xFFFFFFFF, 29),
        SearchNameRow(124913, "Wild Combatant's Dreadplate Chestpiece", 3, 5, 32, 44),
        SearchNameRow(124914, "Wild Combatant's Dreadplate Gauntlets", 3, 5, 32, 44),
    });
    f.modifiedAppearances = TextTable(ITEM_MODIFIED_APPEARANCE_LAYOUT, std::vector<uint32_t>(7, 1), {
        AppearanceOfItem(4595, 12939, 3668),
        AppearanceOfItem(4596, 12940, 3669),
        AppearanceOfItem(7545, 19019, 5353),
        AppearanceOfItem(70584, 124913, 25923),
        AppearanceOfItem(70585, 124914, 25924),
        AppearanceOfItem(80001, 16958, 90001),
        AppearanceOfItem(80002, 15138, 90002),
        AppearanceOfItem(80003, 16959, 90003),
    });
    f.appearances = TextTable(ITEM_APPEARANCE_LAYOUT, std::vector<uint32_t>(5, 1), {
        AppearanceRow(3668, 25647, 135350),
        AppearanceRow(3669, 25646, 135353),
        AppearanceRow(5353, 30606, 135349),
        AppearanceRow(25923, 142221, 1091986),
        AppearanceRow(25924, 142222, 1091987),
        AppearanceRow(90001, 31019, 1),
        AppearanceRow(90002, 31020, 2),
        AppearanceRow(90003, 30606, 3),
    });
    f.itemDisplays[31019] = { { 145000, 145001 }, 1 };
    f.transmogSetItems = TextTable(TRANSMOG_SET_ITEM_LAYOUT, { 1, 1, 1 }, {
        { 900, { 17, 70584, 0 }, {}, 17 },
        { 901, { 17, 70585, 0 }, {}, 17 },
        { 902, { 17, 80001, 0 }, {}, 17 },
        { 903, { 17, 80002, 0 }, {}, 17 },
    });
    RetailIndex index = BuildFixtureIndex(f);
    std::string error;
    CHECK(index.AttachSparse(GoldenSparse(), error));
    index.SetBudget(RealBudget());
    return index;
}

struct GoldenQuery {
    const char* kind;
    const char* text;
    std::map<std::string, double> filters;
};

void AddSearches(const RetailIndex& index, std::vector<std::string>& lines) {
    const GoldenQuery queries[] = { { "creature", "guard", {} }, { "creature", "STORM", {} }, { "creature", "", { { "humanoid", 1 } } },
        { "creature", "", { { "classification", 0 } } }, { "creature", "3258", {} }, { "creature", "", { { "family", 1 } } },
        { "item", "dal", {} }, { "item", "judgment", {} }, { "item", "", { { "quality", 3 } } }, { "item", "142221", {} },
        { "item", "e", { { "armour", 4 } } }, { "item", "", { { "weaponSubclass", 7 }, { "maxItemLevel", 40 } } }, { "item", "  wild   COMBATANT's ", {} },
        { "set", "", { { "classId", 6 } } }, { "set", "a", {} }, { "set", "41", {} }, { "model", "guard", {} }, { "model", "", { { "quality", 1 } } },
        { "model", "1011653", {} }, { "model", "judgment", {} }, { "mount", "horse", {} }, { "mount", "458", {} }, { "mount", "2404", {} } };
    for (const GoldenQuery& query : queries) {
        BrowseKind kind;
        CHECK(ParseBrowseKind(query.kind, kind));
        std::string line = std::string("search ") + query.kind + " '" + query.text + "'";
        for (const auto& [name, value] : query.filters) {
            line += " " + name + "=" + std::to_string(static_cast<int>(value));
        }
        for (const SearchRow& row : index.SearchAll(kind, query.text, ParseBrowseFilters(query.filters))) {
            line += " | " + std::to_string(row.id) + ", " + row.name + ", " + row.detail + ", " + std::to_string(row.displayId) + ", " +
                std::to_string(row.icon) + ", " + std::to_string(row.quality);
        }
        lines.push_back(line);
    }
}

void AddDetails(const RetailIndex& index, std::vector<std::string>& lines) {
    BrowserIndexView view{ IndexState::Ready, &index, "" };
    InstalledDisplays installed = { [](uint32_t id) { return id == 3167; }, [](uint32_t id) { return id == 25646; } };
    for (BrowseKind kind : { BrowseKind::Creature, BrowseKind::Item, BrowseKind::Set, BrowseKind::Model, BrowseKind::Mount }) {
        // A filter its kind does not use, or a level every item has, lists every entry.
        BrowseFilters every;
        (kind == BrowseKind::Item ? every.minItemLevel : every.quality) = 0;
        for (const SearchRow& row : index.SearchAll(kind, "", every)) {
            BrowserValue out;
            std::string error;
            std::string line = std::string("details ") + BrowseKindName(kind) + " " + std::to_string(row.id) + " ";
            CHECK(MarshalDetails(view, BrowseKindName(kind), row.id, installed, out, error));
            Serialize(out, line);
            lines.push_back(line);
        }
    }
    const std::pair<const char*, uint32_t> builds[] = { { "item", 16958 }, { "set", 17 }, { "set", ITEM_SET_KEY_BASE + 41 }, { "creature", 92123 } };
    for (const auto& [kind, id] : builds) {
        BuildOptions options;
        options.numbers["allVariants"] = 1;
        BrowserValue out;
        std::string error;
        std::string line = std::string("build ") + kind + " " + std::to_string(id) + " ";
        CHECK(MarshalBuild(view, kind, id, options, out, error));
        Serialize(out, line);
        lines.push_back(line);
    }
}

std::vector<std::string> Snapshot(const RetailIndex& index) {
    std::vector<std::string> lines;
    for (BrowseKind kind : { BrowseKind::Creature, BrowseKind::Item, BrowseKind::Set, BrowseKind::Model, BrowseKind::Mount }) {
        lines.push_back(std::string("count ") + BrowseKindName(kind) + " " + std::to_string(index.Count(kind)));
    }
    AddSearches(index, lines);
    AddDetails(index, lines);
    return lines;
}

void PrintGolden(const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
        std::printf("GOLDEN R\"golden(%s)golden\",\n", line.c_str());
    }
}

}

TEST(BrowserIndexResultsMatchTheGolden) {
    std::vector<std::string> actual = Snapshot(GoldenIndex());
    const std::vector<std::string> golden = RetailIndexGolden();
    CHECK(actual.size() == golden.size());
    for (size_t i = 0; i < actual.size() && i < golden.size(); i++) {
        if (actual[i] != golden[i]) {
            CHECK(actual[i] == golden[i]);
            std::printf("    (line %zu\n     got  %s\n     want %s)\n", i, actual[i].c_str(), golden[i].c_str());
            break;
        }
    }
    if (std::getenv("WOWDECOMP_PRINT_GOLDEN")) {
        PrintGolden(actual);
    }
}

// Items that share their ItemSparse values share the kept row and each still reads as its own; an item without a
// row has none.
TEST(BrowserItemsReadTheirOwnSparseRows) {
    RetailIndex index = GoldenIndex();
    SparseItem chest, copy, cloak, none;
    const BrowseItem* blade = index.Item(19019);
    CHECK(index.Item(16958) && index.SparseOf(*index.Item(16958), chest) && chest.id == 16958 && chest.statPercents[1] == 7889);
    CHECK(index.Item(16959) && index.SparseOf(*index.Item(16959), copy) && copy.id == 16959 && copy.statPercents == chest.statPercents);
    CHECK(index.Item(15138) && index.SparseOf(*index.Item(15138), cloak) && cloak.inventoryType == 16 && cloak.statTypes[0] == 7);
    CHECK(index.Item(16958)->sparseStats == index.Item(16959)->sparseStats && index.Item(16958)->sparseStats != index.Item(15138)->sparseStats);
    CHECK(blade && !index.SparseOf(*blade, none));
}

// A model's users are named by their entries, not by copies of the names.
TEST(BrowserModelUsersShareTheirEntriesNames) {
    RetailIndex index = GoldenIndex();
    const BrowseCreature* guard = index.Creature(92123);
    const BrowseModel* human = index.Model(1011653);
    CHECK(guard && human && !human->users.empty() && index.UserName(human->users[0]).data() == guard->name.data());
}

// A copy of the index answers on its own: what its entries refer to outlives the index it was copied from.
TEST(BrowserIndexCopiesOutliveTheOriginal) {
    std::vector<std::string> expected = Snapshot(GoldenIndex());
    RetailIndex copy;
    {
        RetailIndex original = GoldenIndex();
        copy = original;
        original = RetailIndex();
    }
    CHECK(Snapshot(copy) == expected);
}
