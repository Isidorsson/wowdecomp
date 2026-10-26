#include <string>
#include <vector>
#include "Engine/Source/Retail/BrowserMarshal.h"
#include "Engine/Source/Retail/RetailIndex.h"
#include "Tools/Tests/BrowserFixtures.h"
#include "Tools/Tests/Check.h"

using Retail::BrowseFilters;
using Retail::BrowseKind;
using Retail::SortColumn;

// The grid's column filters and header sort (Interface\AddOns\RetailBrowser\Columns.lua): every column filters and
// sorts over the whole index, not over the page the grid shows.

static std::vector<uint32_t> PageIds(const Retail::SearchPage& page) {
    std::vector<uint32_t> ids;
    for (const Retail::SearchRow& row : page.rows) {
        ids.push_back(row.id);
    }
    return ids;
}

static std::vector<uint32_t> Find(const Retail::RetailIndex& index, BrowseKind kind, const BrowseFilters& filters, const std::string& text = "") {
    return PageIds(index.Search(kind, text, filters, 0));
}

static BrowseFilters Everything() {
    BrowseFilters filters;
    filters.minId = 0;
    return filters;
}

TEST(BrowserColumnsFilterByIdRange) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters guards;
    guards.minId = 90000;
    guards.maxId = 180000;
    CHECK(Find(index, BrowseKind::Creature, guards) == std::vector<uint32_t>({ 178185, 92123, 178184 }));
    BrowseFilters old;
    old.maxId = 20000;
    CHECK(Find(index, BrowseKind::Item, old) == std::vector<uint32_t>({ 12940, 12939, 19019 }));
    BrowseFilters itemSet;
    itemSet.minId = 41;
    itemSet.maxId = 41;
    CHECK(Find(index, BrowseKind::Set, itemSet) == std::vector<uint32_t>({ Retail::ITEM_SET_KEY_BASE + 41 }));
    BrowseFilters horse;
    horse.minId = 6;
    horse.maxId = 6;
    CHECK(Find(index, BrowseKind::Mount, horse) == std::vector<uint32_t>({ 6 }));
}

TEST(BrowserColumnsFilterByAnyDisplay) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters display;
    display.displayId = 3258;
    CHECK(Find(index, BrowseKind::Creature, display) == std::vector<uint32_t>({ 92123, 178184 }));
    CHECK(Find(index, BrowseKind::Model, display) == std::vector<uint32_t>({ 1011653 }));
    display.displayId = 142222;
    CHECK(Find(index, BrowseKind::Item, display) == std::vector<uint32_t>({ 124914 }));
    CHECK(Find(index, BrowseKind::Set, display) == std::vector<uint32_t>({ 17 }));
    display.displayId = 2404;
    CHECK(Find(index, BrowseKind::Mount, display) == std::vector<uint32_t>({ 6 }));
    display.displayId = 1;
    CHECK(Find(index, BrowseKind::Creature, display).empty());
}

TEST(BrowserColumnsFilterByDisplayCount) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters several;
    several.minCount = 2;
    CHECK(Find(index, BrowseKind::Creature, several) == std::vector<uint32_t>({ 178184 }));
    CHECK(Find(index, BrowseKind::Model, several) == std::vector<uint32_t>({ 1011653 }));
    CHECK(index.Search(BrowseKind::Set, "", several, 0).total == 2);
    BrowseFilters one;
    one.maxCount = 1;
    CHECK(index.Search(BrowseKind::Creature, "", one, 0).total == 4);
}

TEST(BrowserColumnsFilterBySetSourceModelKindAndMountType) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters itemSets;
    itemSets.setSource = 1;
    CHECK(Find(index, BrowseKind::Set, itemSets) == std::vector<uint32_t>({ Retail::ITEM_SET_KEY_BASE + 41 }));
    itemSets.setSource = 0;
    CHECK(Find(index, BrowseKind::Set, itemSets) == std::vector<uint32_t>({ 17 }));
    BrowseFilters itemModels;
    itemModels.modelKind = 0;
    CHECK(Find(index, BrowseKind::Model, itemModels) == std::vector<uint32_t>({ 145000 }));
    BrowseFilters ground;
    ground.mountType = 230;
    CHECK(Find(index, BrowseKind::Mount, ground) == std::vector<uint32_t>({ 6 }));
    ground.mountType = 231;
    CHECK(Find(index, BrowseKind::Mount, ground).empty());
}

TEST(BrowserColumnsFilterByInstalledDisplays) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters installed;
    installed.installed = 1;
    installed.installedCreature = [](uint32_t id) { return id == 3167; };
    installed.installedItem = [](uint32_t id) { return id == 142221; };
    CHECK(Find(index, BrowseKind::Creature, installed) == std::vector<uint32_t>({ 178184 }));
    CHECK(Find(index, BrowseKind::Item, installed) == std::vector<uint32_t>({ 124913 }));
    CHECK(Find(index, BrowseKind::Set, installed) == std::vector<uint32_t>({ 17 }));
    installed.installed = 0;
    CHECK(index.Search(BrowseKind::Creature, "", installed, 0).total == 4);
    BrowseFilters unknown;
    unknown.installed = 1;
    CHECK(Find(index, BrowseKind::Creature, unknown).empty());
}

TEST(BrowserColumnsSortTheWholeMatchList) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters byId = Everything();
    byId.sort = static_cast<int32_t>(SortColumn::Id);
    CHECK(Find(index, BrowseKind::Item, byId) == std::vector<uint32_t>({ 12939, 12940, 19019, 124913, 124914 }));
    BrowseFilters byLevel = Everything();
    byLevel.sort = static_cast<int32_t>(SortColumn::ItemLevel);
    byLevel.descending = 1;
    CHECK(Find(index, BrowseKind::Item, byLevel) == std::vector<uint32_t>({ 124913, 124914, 12940, 12939, 19019 }));
    BrowseFilters byName = Everything();
    byName.sort = static_cast<int32_t>(SortColumn::Name);
    byName.descending = 1;
    CHECK(Find(index, BrowseKind::Item, byName) == std::vector<uint32_t>({ 124914, 124913, 19019, 12939, 12940 }));
    byName.descending = 0;
    CHECK(Find(index, BrowseKind::Item, byName) == std::vector<uint32_t>({ 12940, 12939, 19019, 124913, 124914 }));
    BrowseFilters byCount = Everything();
    byCount.sort = static_cast<int32_t>(SortColumn::Count);
    byCount.descending = 1;
    CHECK(Find(index, BrowseKind::Creature, byCount) == std::vector<uint32_t>({ 178184, 178185, 3823, 85555, 92123 }));
}

TEST(BrowserColumnsSortOverridesTheTextRanking) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters byId;
    byId.sort = static_cast<int32_t>(SortColumn::Id);
    CHECK(Find(index, BrowseKind::Creature, byId, "stormwind") == std::vector<uint32_t>({ 92123, 178184, 178185 }));
    byId.descending = 1;
    CHECK(Find(index, BrowseKind::Creature, byId, "stormwind") == std::vector<uint32_t>({ 178185, 178184, 92123 }));
    std::vector<Retail::SearchRow> all = index.SearchAll(BrowseKind::Creature, "stormwind", byId);
    CHECK(all.size() == 3 && all[0].id == 178185 && all[2].id == 92123);
}

TEST(BrowserColumnFiltersParseFromLuaNames) {
    BrowseFilters f = Retail::ParseBrowseFilters({ { "minId", 5 }, { "maxId", 9 }, { "displayId", 3258 }, { "minCount", 2 }, { "maxCount", 4 },
        { "installed", 1 }, { "setSource", 1 }, { "modelKind", 0 }, { "mountType", 230 }, { "sort", 2 }, { "descending", 1 } });
    CHECK(f.minId == 5 && f.maxId == 9 && f.displayId == 3258 && f.minCount == 2 && f.maxCount == 4 && f.installed == 1);
    CHECK(f.setSource == 1 && f.modelKind == 0 && f.mountType == 230 && f.sort == 2 && f.descending == 1);
    CHECK(Retail::ParseBrowseFilters({}).sort == -1 && !Retail::ParseBrowseFilters({ { "sort", 1 } }).Any());
}

TEST(BrowserSearchPassesTheInstalledDisplaysToItsFilter) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    Retail::InstalledDisplays installed = { [](uint32_t id) { return id == 3167; }, [](uint32_t id) { return id == 142221; } };
    Retail::BrowserValue out;
    std::string error;
    Retail::BrowserIndexView view = { Retail::IndexState::Ready, &index, "" };
    CHECK(Retail::MarshalSearch(view, "creature", "", { { "installed", 1 } }, 1, out, error, installed));
    const Retail::BrowserValue* total = out.Get("total");
    CHECK(total && total->number == 1);
    std::string csv;
    CHECK(Retail::SearchCsv(view, "item", "", { { "installed", 1 } }, csv, error, installed));
    CHECK(csv.find("124913") != std::string::npos && csv.find("19019") == std::string::npos);
}
