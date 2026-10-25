#include <cstring>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailRaces.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/Db2Fixtures.h"

using Retail::RetailRace;
using Retail::RetailRaces;
using Retail::ServedRace;

// ChrRaces, ChrRaceXChrModel and ChrModel with the columns RetailRaces reads at their physical numbers, rows copied
// from the pinned build's CSV dumps (ClientPrefix and Name are the strings of columns 0 and 2).

struct RaceRow { uint32_t id; const char* prefix; const char* name; };

// One 12-byte record per race (prefix, file, name offsets) and the block of their strings.
static Retail::Db2Table RaceTable(const std::vector<RaceRow>& races) {
    Wdc5Spec spec;
    spec.layoutHash = Retail::CHR_RACES_LAYOUT;
    spec.recordSize = 12;
    spec.columns = { { 0, 0, 32, 0, 0, { 0, 0, 0 } }, { 0, 32, 32, 0, 0, { 0, 0, 0 } }, { 0, 64, 32, 0, 0, { 0, 0, 0 } } };
    Wdc5Section section;
    section.strings.push_back(0);
    size_t records = races.size() * spec.recordSize;
    for (size_t r = 0; r < races.size(); r++) {
        uint32_t fields[3] = {};
        for (size_t c = 0; c < 3; c++) {
            const char* text = c == 1 ? "" : (c == 0 ? races[r].prefix : races[r].name);
            if (*text) {
                fields[c] = static_cast<uint32_t>(records + section.strings.size() - (r * spec.recordSize + c * 4));
                section.strings.insert(section.strings.end(), text, text + std::strlen(text) + 1);
            }
            AppendU32(section.records, fields[c]);
        }
        section.ids.push_back(races[r].id);
    }
    spec.sections = { section };

    Retail::Db2Table table;
    std::string error;
    CHECK(table.Parse(BuildWdc5(spec), error));
    return table;
}

struct RaceTableSet {
    Retail::Db2Table races, raceModels, models, layers;

    Retail::RaceTables View() const { return { races, raceModels, models, layers }; }
};

static RaceTableSet Fixture() {
    RaceTableSet set;
    set.races = RaceTable({ { 1, "Hu", "Human" }, { 9, "Go", "Goblin" }, { 22, "Wo", "Worgen" }, { 32, "Kt", "Kul Tiran" }, { 84, "Ed", "Earthen" },
        { 85, "Ed", "Earthen" } });
    // ChrRaceXChrModel: race, ChrModel, sex; related to the race.
    std::vector<Db2Row> rows = { { 1, { 1, 1, 0, 0 } }, { 17, { 9, 17, 0, 0 } }, { 18, { 9, 18, 1, 0 } }, { 43, { 22, 43, 0, 0 } },
        { 44, { 22, 44, 1, 0 } }, { 63, { 32, 63, 0, 0 } }, { 64, { 32, 64, 1, 0 } }, { 269, { 84, 195, 0, 0 } }, { 270, { 84, 196, 1, 0 } },
        { 271, { 85, 195, 0, 0 } }, { 272, { 85, 196, 1, 0 } } };
    set.raceModels = PlainTable(Retail::CHR_RACE_X_CHR_MODEL_LAYOUT, { 1, 1, 1, 1 }, rows, { 1, 9, 9, 22, 22, 32, 32, 84, 84, 85, 85 });
    // ChrModel: columns 0 to 4, sex in 3 and the DisplayID in 4.
    rows = { { 1, { 0, 0, 1, 0, 57899, 103 } }, { 17, { 0, 0, 17, 0, 6894, 119 } }, { 18, { 0, 0, 18, 1, 6895, 120 } }, { 43, { 0, 0, 43, 0, 29422, 125 } },
        { 44, { 0, 0, 44, 1, 29423, 126 } }, { 63, { 0, 0, 63, 0, 80387, 141 } }, { 64, { 0, 0, 64, 1, 80388, 142 } }, { 195, { 0, 0, 195, 0, 115279, 185 } },
        { 196, { 0, 0, 196, 1, 115281, 187 } } };
    set.models = PlainTable(Retail::CHR_MODEL_LAYOUT, { 1, 1, 1, 1, 1, 1 }, rows);
    // ChrModelTextureLayer: texture type, layer, columns 2 to 6, the targets in 7; related to the layout.
    rows = { { 204, { 1, 0, 0, 1, 0, 0, 0, 1 } }, { 213, { 19, 9, 0, 1, 0, 0, 0, 25 } }, { 666, { 20, 10, 0, 1, 0, 0, 0, 38 } },
        { 272, { 1, 0, 0, 1, 0, 0, 0, 1 } }, { 273, { 8, 1, 0, 1, 0, 0, 0, 2 } } };
    set.layers = PlainTable(Retail::CHR_MODEL_TEXTURE_LAYER_LAYOUT, { 1, 1, 1, 1, 1, 1, 1, 1 }, rows, { 119, 119, 119, 125, 125 });
    return set;
}

static RetailRaces Loaded() {
    RaceTableSet set = Fixture();
    RetailRaces races;
    std::string error;
    CHECK(races.Load(set.View(), error));
    return races;
}

// --- B1: the retail race rows ---------------------------------------------------------------------------------

TEST(RetailRacesReadThePrefixNameAndBodyDisplays) {
    RetailRaces races = Loaded();
    const RetailRace* worgen = races.Race(22);
    const RetailRace* earthen = races.Race(85);

    CHECK(worgen && worgen->prefix == "Wo" && worgen->name == "Worgen" && worgen->displays[0] == 29422 && worgen->displays[1] == 29423);
    CHECK(earthen && earthen->prefix == "Ed" && earthen->displays[0] == 115279 && earthen->displays[1] == 115281);
    CHECK(races.Race(23) == nullptr && races.Race(0) == nullptr);
    CHECK(races.MaxId() == 85);
}

TEST(RetailRacesRefuseAnUnknownLayout) {
    RaceTableSet set = Fixture();
    set.models = PlainTable(0x12345678, { 1 }, { { 1, { 1 } } });
    RetailRaces races;
    std::string error;
    CHECK(!races.Load(set.View(), error));
    CHECK(error.find("ChrModel") != std::string::npos);
}

TEST(RetailRacesFindTheLowestRaceOfAPrefix) {
    RetailRaces races = Loaded();

    CHECK(races.RaceOfPrefix("ed") == 84);
    CHECK(races.RaceOfPrefix("KT") == 32);
    CHECK(races.RaceOfPrefix("hu") == 1);
    CHECK(races.RaceOfPrefix("zz") == 0);
}

TEST(ServedRacesAreHiddenFromCharacterCreation) {
    RetailRaces races = Loaded();
    ServedRace served;

    CHECK(Retail::ServeRace(*races.Race(32), 21, served));
    CHECK(served.id == 32 && (served.flags & Retail::RACE_HIDDEN_FROM_CREATION) && served.prefix == "Kt" && served.name == "Kul Tiran");
    CHECK(served.displays[0] == 80387 && served.displays[1] == 80388);
}

TEST(InstalledRaceRowsWin) {
    RetailRaces races = Loaded();
    ServedRace served;

    CHECK(!Retail::ServeRace(*races.Race(1), 21, served));
    CHECK(!Retail::ServeRace(*races.Race(9), 21, served));
    CHECK(Retail::ServeRace(*races.Race(22), 21, served));
    CHECK(!Retail::ServeRace(*races.Race(22), 22, served));
}

TEST(RetailRacesKnowTheirBodiesTextureLayers) {
    RetailRaces races = Loaded();
    const RetailRace* goblin = races.Race(9);

    CHECK(goblin && goblin->layouts[0] == 119 && goblin->layouts[1] == 120);
    std::vector<Retail::TextureLayer> layers = races.TextureLayers(119);
    CHECK(layers.size() == 3);
    CHECK(layers.size() == 3 && layers[1].type == 19 && layers[1].layer == 9 && layers[1].target == 25);
    CHECK(races.TextureLayers(125).size() == 2 && races.TextureLayers(999).empty());
}

// --- B2: race-sized client tables -----------------------------------------------------------------------------

TEST(RaceCapacityCoversTheHighestServedRace) {
    CHECK(Retail::RaceCapacity(21, 85) == 85);
    CHECK(Retail::RaceCapacity(21, 0) == 21);
    CHECK(Retail::RaceCapacity(30, 22) == 30);
}

TEST(ARaceAboveTheCapacityDoesNotFitTheTables) {
    CHECK(Retail::RaceFitsTables(85, 1, 85));
    CHECK(Retail::RaceFitsTables(1, 0, 21));
    CHECK(!Retail::RaceFitsTables(86, 0, 85));
    CHECK(!Retail::RaceFitsTables(22, 2, 85));
}

// --- B5: CharSections ------------------------------------------------------------------------------------------

TEST(OnlyABakedServedRaceSkipsCharSections) {
    CHECK(Retail::NeedsCharSections(true, true));
    CHECK(Retail::NeedsCharSections(true, false));
    CHECK(!Retail::NeedsCharSections(false, true));
    CHECK(Retail::NeedsCharSections(false, false));
}
