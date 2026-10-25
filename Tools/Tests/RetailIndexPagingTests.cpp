#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailIndex.h"
#include "Tools/Tests/BrowserFixtures.h"
#include "Tools/Tests/Check.h"

using Retail::BrowseFilters;
using Retail::BrowseKind;

// Search ranks only the page it shows and keeps the last ranking for page turns; every page must still be exactly
// SearchAll's rows at that place (exact, then prefix, then substring; ties by name, then id).

static bool SameRow(const Retail::SearchRow& a, const Retail::SearchRow& b) {
    return a.id == b.id && a.name == b.name && a.detail == b.detail && a.displayId == b.displayId && a.icon == b.icon && a.quality == b.quality;
}

static bool PageIsSlice(const Retail::RetailIndex& index, BrowseKind kind, const std::string& text, const BrowseFilters& filters, uint32_t page) {
    std::vector<Retail::SearchRow> all = index.SearchAll(kind, text, filters);
    Retail::SearchPage shown = index.Search(kind, text, filters, page);
    size_t first = static_cast<size_t>(page) * Retail::BROWSE_PAGE_SIZE;
    size_t expected = first < all.size() ? std::min<size_t>(Retail::BROWSE_PAGE_SIZE, all.size() - first) : 0;
    bool same = shown.total == all.size() && shown.page == page && shown.rows.size() == expected &&
        shown.pages == (all.size() + Retail::BROWSE_PAGE_SIZE - 1) / Retail::BROWSE_PAGE_SIZE;
    for (size_t i = 0; same && i < expected; i++) {
        same = SameRow(shown.rows[i], all[first + i]);
    }
    if (!same) {
        std::printf("    (page %u of \"%s\": %zu rows of %zu)\n", page, text.c_str(), shown.rows.size(), all.size());
    }
    return same;
}

// 170 creatures out of id order: equal names (ties the ids must break), names that rank exact, prefix and substring
// for "guard", elites among them and a display many share.
static Retail::RetailIndex TiedIndex() {
    static const char* const NAMES[] = { "Guard", "guard", "Guard Captain", "Old Guard", "Guardian", "Stormwind Guard", "Gnoll" };
    BrowserFixture fixture = MakeBrowserFixture();
    std::vector<TextRow> rows;
    for (uint32_t i = 0; i < 170; i++) {
        uint32_t id = 5000 + (i * 37) % 170;
        rows.push_back(CreatureRow(id, NAMES[i % 7], i % 3 == 0 ? 1 : 0, 7, 0, { i % 4 == 0 ? 3258u : 4000 + i }));
    }
    fixture.creatures = TextTable(Retail::CREATURE_LAYOUT, { 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 3 }, rows);
    return BuildFixtureIndex(fixture);
}

TEST(BrowserSearchPagesAreSearchAllSliced) {
    Retail::RetailIndex index = TiedIndex();
    BrowseFilters elites;
    elites.classification = 1;
    const std::vector<std::pair<std::string, BrowseFilters>> queries = { { "guard", {} }, { "g", {} }, { "", elites },
        { "guard", elites }, { "3258", {} }, { "nothing like it", {} } };
    for (const auto& [text, filters] : queries) {
        for (uint32_t page = 0; page < 5; page++) {
            CHECK(PageIsSlice(index, BrowseKind::Creature, text, filters, page));
        }
    }
}

TEST(BrowserSearchPageTurnsBackAndForthKeepTheOrder) {
    Retail::RetailIndex index = TiedIndex();
    for (uint32_t page : { 2u, 0u, 3u, 1u, 1u, 0u }) {
        CHECK(PageIsSlice(index, BrowseKind::Creature, "g", {}, page));
    }
    BrowseFilters elites;
    elites.classification = 1;
    CHECK(PageIsSlice(index, BrowseKind::Creature, "g", elites, 1));
    CHECK(PageIsSlice(index, BrowseKind::Creature, "G", {}, 1));
    CHECK(PageIsSlice(index, BrowseKind::Item, "dal", {}, 0));
}

TEST(BrowserSearchTiesGoByIdWithinAName) {
    Retail::RetailIndex index = TiedIndex();
    Retail::SearchPage page = index.Search(BrowseKind::Creature, "guard", {}, 0);
    CHECK(page.rows.size() == 50);
    for (size_t i = 1; i < page.rows.size(); i++) {
        bool sameName = Retail::NormalizeSearchText(page.rows[i - 1].name) == Retail::NormalizeSearchText(page.rows[i].name);
        CHECK(!sameName || page.rows[i - 1].id < page.rows[i].id);
    }
    CHECK(!page.rows.empty() && Retail::NormalizeSearchText(page.rows[0].name) == "guard" && page.rows[0].id == 5000);
}
