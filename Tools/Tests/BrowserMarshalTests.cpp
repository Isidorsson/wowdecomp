#include <string>
#include "Engine/Source/Retail/BrowserMarshal.h"
#include "Tools/Tests/BrowserFixtures.h"
#include "Tools/Tests/Check.h"

using Retail::BrowserValue;

static std::string TextOf(const BrowserValue* value) {
    return value && value->type == BrowserValue::Type::String ? value->text : std::string("<none>");
}

static double NumberOf(const BrowserValue* value) {
    return value && value->type == BrowserValue::Type::Number ? value->number : -1;
}

static bool FlagOf(const BrowserValue* value) {
    return value && value->type == BrowserValue::Type::Bool && value->flag;
}

static const BrowserValue* Item(const BrowserValue* table, size_t index) {
    return table && index < table->items.size() ? &table->items[index] : nullptr;
}

static const BrowserValue* Field(const BrowserValue* table, const char* key) {
    return table ? table->Get(key) : nullptr;
}

static Retail::BrowserIndexView Ready(const Retail::RetailIndex& index) {
    return { Retail::IndexState::Ready, &index, "" };
}

// The label a details field list gives a value under, "<none>" when absent.
static std::string Labelled(const BrowserValue& details, const std::string& label) {
    const BrowserValue* fields = details.Get("fields");
    for (const BrowserValue& field : fields ? fields->items : std::vector<BrowserValue>()) {
        if (TextOf(field.Get("label")) == label) {
            return TextOf(field.Get("value"));
        }
    }
    return "<none>";
}

// R4: Lua marshalling.

TEST(BrowserSearchReportsBuildingUntilTheIndexIsReady) {
    BrowserValue out;
    std::string error;
    CHECK(Retail::MarshalSearch({ Retail::IndexState::Building, nullptr, "" }, "creature", "guard", {}, 1, out, error));
    CHECK(TextOf(out.Get("status")) == "building" && !out.Get("rows"));
    CHECK(Retail::MarshalSearch({ Retail::IndexState::Failed, nullptr, "no tables" }, "creature", "guard", {}, 1, out, error));
    CHECK(TextOf(out.Get("status")) == "failed" && TextOf(out.Get("error")) == "no tables");
}

TEST(BrowserSearchRefusesAnUnknownKind) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowserValue out;
    std::string error;
    CHECK(!Retail::MarshalSearch(Ready(index), "spell", "guard", {}, 1, out, error));
    CHECK(error.find("spell") != std::string::npos);
    CHECK(!Retail::MarshalDetails(Ready(index), "spell", 1, {}, out, error));
}

TEST(BrowserSearchRowsBecomeLuaFields) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowserValue out;
    std::string error;
    CHECK(Retail::MarshalSearch(Ready(index), "item", "dal'rend", {}, 1, out, error));
    CHECK(TextOf(out.Get("status")) == "ok" && NumberOf(out.Get("total")) == 2 && NumberOf(out.Get("page")) == 1 && NumberOf(out.Get("pages")) == 1);
    const BrowserValue* first = Item(out.Get("rows"), 0);
    CHECK(NumberOf(Field(first, "id")) == 12940 && TextOf(Field(first, "name")) == "Dal'Rend's Sacred Charge");
    CHECK(NumberOf(Field(first, "displayId")) == 25646 && NumberOf(Field(first, "quality")) == 3 && NumberOf(Field(first, "icon")) == 135353);
    CHECK(TextOf(Field(first, "detail")) != "<none>");
}

TEST(BrowserFiltersParseFromLuaNames) {
    Retail::BrowseFilters filters = Retail::ParseBrowseFilters({ { "armour", 4 }, { "inventoryType", 5 }, { "minItemLevel", 40 }, { "bogus", 3 } });
    CHECK(filters.armour == 4 && filters.inventoryType == 5 && filters.minItemLevel == 40 && filters.quality == -1);
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowserValue out;
    std::string error;
    CHECK(Retail::MarshalSearch(Ready(index), "item", "", { { "armour", 4 }, { "inventoryType", 5 } }, 1, out, error));
    CHECK(NumberOf(out.Get("total")) == 1 && NumberOf(Field(Item(out.Get("rows"), 0), "id")) == 124913);
}

TEST(BrowserCreatureDetailsCarryEveryId) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowserValue out;
    std::string error;
    CHECK(Retail::MarshalDetails(Ready(index), "creature", 92123, {}, out, error));
    CHECK(NumberOf(out.Get("creatureId")) == 92123 && TextOf(out.Get("name")) == "Stormwind Guard" && TextOf(out.Get("typeName")) == "Humanoid");
    CHECK(NumberOf(out.Get("displayId")) == 3258);
    const BrowserValue* display = Item(out.Get("displays"), 0);
    CHECK(NumberOf(Field(display, "displayId")) == 3258 && NumberOf(Field(display, "probability")) == 1);
    CHECK(NumberOf(Field(display, "modelFdid")) == 1011653 && NumberOf(Field(display, "extraId")) == 130617);
    CHECK(NumberOf(Field(display, "race")) == 1 && NumberOf(Field(display, "sex")) == 0 && NumberOf(Field(display, "hdBake")) == 157059);
    CHECK(Labelled(out, "Creature id") == "92123");
    CHECK(Labelled(out, "Display 1") != "<none>");
    CHECK(!Retail::MarshalDetails(Ready(index), "creature", 68, {}, out, error) && !error.empty());
}

TEST(BrowserItemDetailsCarryEveryId) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowserValue out;
    std::string error;
    CHECK(Retail::MarshalDetails(Ready(index), "item", 19019, {}, out, error));
    CHECK(NumberOf(out.Get("itemId")) == 19019 && NumberOf(out.Get("quality")) == 5 && NumberOf(out.Get("itemLevel")) == 29);
    CHECK(NumberOf(out.Get("inventoryType")) == 13 && NumberOf(out.Get("classId")) == 2 && NumberOf(out.Get("subclassId")) == 7);
    CHECK(TextOf(out.Get("subclassName")) == "Swords" && NumberOf(out.Get("icon")) == 135349 && NumberOf(out.Get("displayId")) == 30606);
    const BrowserValue* look = Item(out.Get("appearances"), 0);
    CHECK(NumberOf(Field(look, "displayId")) == 30606 && NumberOf(Field(look, "appearanceId")) == 5353);
    CHECK(NumberOf(Field(look, "modifiedAppearanceId")) == 7545 && NumberOf(Item(Field(look, "models"), 0)) == 145000);
    CHECK(Labelled(out, "Item id") == "19019");
}

TEST(BrowserSetDetailsListPieces) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowserValue out;
    std::string error;
    CHECK(Retail::MarshalDetails(Ready(index), "set", 17, {}, out, error));
    CHECK(NumberOf(out.Get("setId")) == 17 && TextOf(out.Get("source")) == "transmog" && NumberOf(out.Get("classMask")) == 32);
    const BrowserValue* piece = Item(out.Get("pieces"), 1);
    CHECK(NumberOf(Field(piece, "itemId")) == 124914 && NumberOf(Field(piece, "displayId")) == 142222);
    CHECK(NumberOf(Field(piece, "modifiedAppearanceId")) == 70585);
    CHECK(Retail::MarshalDetails(Ready(index), "set", Retail::ITEM_SET_KEY_BASE + 41, {}, out, error));
    CHECK(NumberOf(out.Get("setId")) == 41 && TextOf(out.Get("source")) == "itemset");
}

TEST(BrowserModelAndMountDetailsListUsers) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowserValue out;
    std::string error;
    CHECK(Retail::MarshalDetails(Ready(index), "model", 1011653, {}, out, error));
    CHECK(NumberOf(out.Get("fdid")) == 1011653 && FlagOf(out.Get("creature")));
    const BrowserValue* user = Item(out.Get("users"), 0);
    CHECK(TextOf(Field(user, "kind")) == "creature" && NumberOf(Field(user, "id")) == 92123 && NumberOf(Field(user, "displayId")) == 3258);
    CHECK(Retail::MarshalDetails(Ready(index), "mount", 6, {}, out, error));
    CHECK(NumberOf(out.Get("mountId")) == 6 && NumberOf(out.Get("spellId")) == 458 && NumberOf(out.Get("displayId")) == 2404);
}

// R3: an id the installed DBC has keeps its custom row.

TEST(BrowserFlagsDisplaysTheInstalledDbcHas) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    Retail::InstalledDisplays installed = { [](uint32_t id) { return id == 3167; }, [](uint32_t id) { return id == 142221; } };
    BrowserValue out;
    std::string error;
    CHECK(Retail::MarshalDetails(Ready(index), "creature", 178184, installed, out, error));
    CHECK(FlagOf(Field(Item(out.Get("displays"), 0), "custom")) && !FlagOf(Field(Item(out.Get("displays"), 1), "custom")));
    CHECK(Labelled(out, "Display 1").find("custom row wins") != std::string::npos);
    CHECK(Labelled(out, "Display 2").find("custom row wins") == std::string::npos);
    CHECK(Retail::MarshalDetails(Ready(index), "set", 17, installed, out, error));
    CHECK(FlagOf(Field(Item(out.Get("pieces"), 0), "custom")) && !FlagOf(Field(Item(out.Get("pieces"), 1), "custom")));
    CHECK(Retail::MarshalDetails(Ready(index), "item", 124913, installed, out, error));
    CHECK(FlagOf(Field(Item(out.Get("appearances"), 0), "custom")));
    CHECK(Retail::MarshalDetails(Ready(index), "creature", 92123, {}, out, error));
    CHECK(!FlagOf(Field(Item(out.Get("displays"), 0), "custom")));
}

TEST(BrowserInfoCountsEachKind) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowserValue info = Retail::MarshalInfo(Ready(index));
    CHECK(TextOf(info.Get("status")) == "ready");
    CHECK(NumberOf(Field(info.Get("counts"), "creature")) == 5 && NumberOf(Field(info.Get("counts"), "set")) == 2);
    const BrowserValue* types = info.Get("creatureTypes");
    CHECK(types && types->items.size() == 2 && TextOf(Field(Item(types, 1), "name")) == "Humanoid");
    CHECK(TextOf(Retail::MarshalInfo({ Retail::IndexState::Idle, nullptr, "" }).Get("status")) == "idle");
}

TEST(BrowserSearchExportsCsv) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    std::string csv;
    std::string error;
    CHECK(Retail::SearchCsv(Ready(index), "item", "thunderfury", {}, csv, error));
    CHECK(csv == "id,name,detail,displayId,icon,quality\n19019,\"Thunderfury, Blessed Blade of the Windseeker\",\"" +
        std::string("ilvl 29, One-Hand, Swords") + "\",30606,135349,5\n");
    CHECK(!Retail::SearchCsv(Ready(index), "spell", "x", {}, csv, error));
}
