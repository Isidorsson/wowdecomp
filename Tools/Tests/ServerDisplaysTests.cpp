#include <cmath>
#include <cstring>
#include "Tools/RetailServerDisplays/ServerDisplays.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/CreatureFixtures.h"
#include "Tools/Tests/WdbcFixtures.h"

using Retail::RetailCreatureModel;
using Retail::RetailDisplay;
using Retail::RetailDisplays;
using ServerDisplays::Added;
using ServerDisplays::ModelInfo;
using ServerDisplays::Wdbc;

// A worldserver spawns a creature only when its display and that display's model are in the server's DBCs, and
// aborts in GetCollisionHeight when either is missing. Retail displays get server-only placeholder rows.

constexpr uint32_t QUILLBOAR = 1000001;
constexpr uint32_t STOCK_DISPLAY = 33107;
constexpr uint32_t STOCK_MODEL = 3555;

static bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-5f;
}

static float AsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static RetailDisplays Retailed(const std::vector<CreatureModelRow>& models, const std::vector<CreatureDisplayRow>& displays) {
    RetailDisplays retail;
    std::string error;
    if (!retail.Load({ CreatureModels(models), CreatureDisplays(displays), DisplayGeosets({}) }, error)) {
        std::printf("    (%s)\n", error.c_str());
    }
    return retail;
}

static Wdbc ServerTable(uint32_t fields, uint32_t id) {
    Wdbc table;
    std::string error;
    table.Parse(WdbcBytes(fields, { { id } }), error);
    return table;
}

struct Server {
    Wdbc displays = ServerTable(ServerDisplays::DISPLAY_FIELDS, STOCK_DISPLAY);
    Wdbc models = ServerTable(ServerDisplays::MODEL_FIELDS, STOCK_MODEL);
};

TEST(ADisplayRowNamesItsModelAndScale) {
    std::vector<uint32_t> row = ServerDisplays::DisplayRow(58940, 3556, 1.25f);

    CHECK(row.size() == ServerDisplays::DISPLAY_FIELDS);
    CHECK(row[0] == 58940 && row[1] == 3556 && AsFloat(row[4]) == 1.25f);
    CHECK(row[2] == 0 && row[3] == 0 && row[5] == 0 && row[15] == 0);
}

TEST(AModelRowCarriesRetailSizesAtTheServerColumns) {
    RetailCreatureModel model;
    model.collisionWidth = 0.75f;
    model.collisionHeight = 2.5f;
    model.modelScale = 1.5f;
    model.mountHeight = 1.75f;
    model.geoBox = { -1, -2, 0, 1, 2, 3 };
    std::vector<uint32_t> row = ServerDisplays::ModelRow(3556, 9, model);

    CHECK(row.size() == ServerDisplays::MODEL_FIELDS);
    CHECK(row[0] == 3556 && row[1] == 0 && row[2] == 9 && AsFloat(row[4]) == 1.5f);
    CHECK(AsFloat(row[14]) == 0.75f && AsFloat(row[15]) == 2.5f && AsFloat(row[16]) == 1.75f);
    CHECK(AsFloat(row[17]) == -1 && AsFloat(row[19]) == 0 && AsFloat(row[22]) == 3);
    CHECK(row[23] == 0 && row[27] == 0);
}

TEST(ModelInfoIsHalfTheWidthAndThePlayerReachScaled) {
    RetailDisplay display;
    display.id = 58940;
    display.scale = 2;
    RetailCreatureModel model;
    model.collisionWidth = 0.6f;
    model.modelScale = 1.5f;
    ModelInfo info = ServerDisplays::ModelInfoOf(display, model);

    CHECK(info.displayId == 58940);
    CHECK(Near(info.boundingRadius, 0.9f));
    CHECK(Near(info.combatReach, 4.5f));
}

TEST(RetailDisplaysGetPlaceholderModelsAboveTheServersOwn) {
    RetailDisplays retail = Retailed({ { 40, QUILLBOAR, {}, 0.5f, 2, 1.25f } }, { { 85000, 40, { 1 }, 2 } });
    Server server;
    Added added = ServerDisplays::AddRetailDisplays(server.displays, server.models, retail, { 85000 });

    CHECK(added.displays == std::vector<uint32_t>({ 85000 }) && added.models == 1);
    CHECK(server.displays.RowCount() == 2 && server.displays.Id(1) == 85000);
    CHECK(server.displays.Field(1, 1) == STOCK_MODEL + 1 && AsFloat(server.displays.Field(1, 4)) == 2);
    CHECK(server.models.RowCount() == 2 && server.models.Id(1) == STOCK_MODEL + 1);
    CHECK(AsFloat(server.models.Field(1, 15)) == 2 && AsFloat(server.models.Field(1, 4)) == 1.25f);
}

TEST(APlaceholderModelIsNamedAfterTheRetailFile) {
    RetailDisplays retail = Retailed({ { 40, QUILLBOAR } }, { { 85000, 40, { 1 } } });
    Server server;
    ServerDisplays::AddRetailDisplays(server.displays, server.models, retail, { 85000 });

    CHECK(!std::strcmp(server.models.String(server.models.Field(1, 2)), "retail\\1000001.m2"));
}

TEST(DisplaysSharingARetailModelShareItsPlaceholder) {
    RetailDisplays retail = Retailed({ { 40, QUILLBOAR }, { 41, 7 } }, { { 85000, 40, { 1 } }, { 85001, 40, { 2 } }, { 85002, 41, { 3 } } });
    Server server;
    Added added = ServerDisplays::AddRetailDisplays(server.displays, server.models, retail, { 85002, 85000, 85001 });

    CHECK(added.models == 2 && server.models.RowCount() == 3);
    CHECK(added.displays == std::vector<uint32_t>({ 85000, 85001, 85002 }));
    CHECK(server.displays.Field(1, 1) == server.displays.Field(2, 1));
    CHECK(server.displays.Field(3, 1) != server.displays.Field(1, 1));
}

TEST(StockAndUnknownIdsAreRefusedAndRepeatsAddedOnce) {
    RetailDisplays retail = Retailed({ { 40, QUILLBOAR } }, { { STOCK_DISPLAY, 40, { 1 } }, { 85000, 40, { 1 } } });
    Server server;
    Added added = ServerDisplays::AddRetailDisplays(server.displays, server.models, retail, { STOCK_DISPLAY, 85000, 85000, 99 });

    CHECK(added.stock == std::vector<uint32_t>({ STOCK_DISPLAY }));
    CHECK(added.unknown == std::vector<uint32_t>({ 99 }));
    CHECK(added.displays == std::vector<uint32_t>({ 85000 }) && server.displays.RowCount() == 2);
    CHECK(added.modelInfo.size() == 1 && added.modelInfo[0].displayId == 85000);
}

TEST(MissingDisplaysAreTheRetailOnesTheServerLacks) {
    RetailDisplays retail = Retailed({ { 40, QUILLBOAR } }, { { 5, 40, { 1 } }, { STOCK_DISPLAY, 40, { 1 } }, { 85000, 40, { 1 } } });
    Server server;

    CHECK(ServerDisplays::MissingDisplayIds(server.displays, retail) == std::vector<uint32_t>({ 5, 85000 }));
}

TEST(IdsTheClientHasAreLeftOutAndReported) {
    Wdbc client = ServerTable(ServerDisplays::DISPLAY_FIELDS, 5);
    std::vector<uint32_t> excluded;

    CHECK(ServerDisplays::WithoutIdsOf(client, { 85000, 5, 6 }, excluded) == std::vector<uint32_t>({ 85000, 6 }));
    CHECK(excluded == std::vector<uint32_t>({ 5 }));
}

TEST(LayoutsMustBeTheServersFieldCounts) {
    Server server;
    Wdbc wrong = ServerTable(15, 1);
    std::string error;

    CHECK(ServerDisplays::KnownLayouts(server.displays, server.models, error));
    CHECK(!ServerDisplays::KnownLayouts(wrong, server.models, error) && !error.empty());
    CHECK(!ServerDisplays::KnownLayouts(server.displays, wrong, error));
}

TEST(ModelInfoSqlReplacesGenderlessRows) {
    std::string sql = ServerDisplays::ModelInfoSql({ { 85000, 0.5f, 1.5f }, { 88270, 0.25f, 3 } });

    CHECK(sql.find("REPLACE INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`) VALUES") != std::string::npos);
    CHECK(sql.find("(85000, 0.5, 1.5, 2, 0),\n(88270, 0.25, 3, 2, 0);") != std::string::npos);
    CHECK(ServerDisplays::ModelInfoSql({}).find("REPLACE") == std::string::npos);
}
