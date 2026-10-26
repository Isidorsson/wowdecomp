#include <algorithm>
#include <atomic>
#include <set>
#include <string>
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/DecodedCache.h"
#include "Engine/Source/Retail/HelmFit.h"
#include "Engine/Source/Retail/RetailPath.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/SeedFixtures.h"

// The seed export: a source that has the originals writes a seed folder, and a fresh source holding only that folder,
// with storage that cannot open, reads everything the client asks for the exported displays.

namespace {

// A storage that never opens, counting the tries: a read that missed the seed is one.
struct NoStorage {
    std::atomic<int> opens { 0 };

    Retail::CascSource::StorageOpener Make() {
        return [this](std::string& error) -> void* {
            this->opens++;
            error = "offline";
            return nullptr;
        };
    }
};

// The two sides of an export: the admin's source holds the install, the player's only the seed it is given.
struct Trip {
    RetailWorld world = BuildWorld();
    LocalSource admin;
    LocalSource player;
    NoStorage storage;
};

}

static bool Begin(Trip& trip, const char* name) {
    std::string player = std::string(name) + "-player";
    if (!Configure(trip.admin, name) || !Install(trip.admin, trip.world) || !Configure(trip.player, player.c_str())) {
        return false;
    }
    trip.player.source.SetStorageOpener(trip.storage.Make());
    return true;
}

static std::vector<std::vector<uint8_t>> ReadAll(Retail::CascSource& source, const std::vector<std::string>& paths) {
    std::vector<std::vector<uint8_t>> served;
    for (const std::string& path : paths) {
        std::vector<uint8_t> bytes;
        std::string error;
        if (!source.Read(path.c_str(), bytes, error)) {
            std::printf("    (%s: %s)\n", path.c_str(), error.c_str());
            bytes.clear();
        }
        served.push_back(std::move(bytes));
    }
    return served;
}

// The entries of the seed folder, 0 for a file that is no entry.
static std::set<uint32_t> SeedIds(const LocalSource& local) {
    std::set<uint32_t> ids;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(Retail::SeedEntryPath(local.config, 0).parent_path(), ec)) {
        std::string name = entry.path().filename().string();
        bool digits = name.find_first_not_of("0123456789") == std::string::npos;
        ids.insert(digits ? static_cast<uint32_t>(std::stoul(name)) : 0);
    }
    return ids;
}

static std::set<uint32_t> Union(std::set<uint32_t> ids, const std::set<uint32_t>& more) {
    ids.insert(more.begin(), more.end());
    return ids;
}

// The names of the file textures a converted model carries.
static std::vector<std::string> TextureNames(const std::vector<uint8_t>& m2) {
    std::vector<std::string> names;
    for (uint32_t i = 0; i < GetU32(m2, M2At::TEXTURES); i++) {
        size_t record = RecordAt(m2, M2At::TEXTURES, i, 0x10);
        uint32_t length = GetU32(m2, record + 8);
        uint32_t offset = GetU32(m2, record + 12);
        if (length > 1 && offset + length <= m2.size()) {
            names.emplace_back(reinterpret_cast<const char*>(m2.data() + offset), length - 1);
        }
    }
    return names;
}

static std::vector<std::string> BeastPaths() {
    std::string model = Retail::CreatureModelName(BEAST_MODEL);
    return { model, Retail::SkinPathFor(Retail::NormalizePath(model.c_str()), 0), "retail/" + std::to_string(BEAST_COAT) + ".blp",
        "retail/" + std::to_string(BEAST_MANE) + ".blp", "retail/" + std::to_string(BEAST_BELLY) + ".blp" };
}

// The paths a served row gives the client, which puts its own folder before each name.
static std::string SkinOf(const std::string& model) {
    return Retail::SkinPathFor(Retail::NormalizePath(model.c_str()), 0);
}

static std::string BakePath(uint32_t file) {
    return "Textures\\BakedNpcTextures\\" + Retail::RetailFileName(file) + ".blp";
}

static std::string TexturePath(uint32_t file) {
    return "Item\\Textures\\" + Retail::RetailFileName(file) + ".blp";
}

// A section's texture as the client asks for it by gender: _U for a file of both, else _M or _F.
static std::string BodyTexturePath(uint32_t male, uint32_t female, char gender) {
    return "Item\\TextureComponents\\" + Retail::RetailBodyTextureName(male, female) + "_" + gender + ".blp";
}

static std::string ModelPath(uint32_t file) {
    return "Item\\ObjectComponents\\Weapon\\" + Retail::RetailFileName(file) + ".m2";
}

// The helm as the client asks for a race and gender: the display's model name with the race's prefix.
static std::string HelmPath(uint32_t race, uint32_t gender) {
    return "Item\\ObjectComponents\\Head\\" + Retail::RetailFileName(HelmFile(HUMAN, MALE)) + "_" + Retail::RacePrefix(race) + (gender ? "f" : "m") + ".m2";
}

// The retail body a helm of the race and gender is fitted from, none where helms hang unmoved.
static std::set<uint32_t> FitBody(uint32_t race, uint32_t gender) {
    return Retail::HelmFitsUnmoved(race, gender) ? std::set<uint32_t>() : std::set<uint32_t>({ Retail::RetailBodyFileDataId(race, gender) });
}

static std::vector<std::string> NpcPaths() {
    std::string helm = HelmPath(HUMAN, MALE);
    return { BakePath(NPC_BAKE), helm, SkinOf(helm), ModelPath(SHOULDER_LEFT), SkinOf(ModelPath(SHOULDER_LEFT)), ModelPath(SHOULDER_RIGHT),
        SkinOf(ModelPath(SHOULDER_RIGHT)), TexturePath(HELM_SKIN), TexturePath(SHOULDER_SKIN), BodyTexturePath(CHEST_MALE, CHEST_FEMALE, 'm'),
        BodyTexturePath(SLEEVE_BOTH, SLEEVE_BOTH, 'u') };
}

TEST(ASeedServesACreatureDisplayWithoutItsOriginals) {
    Trip trip;
    CHECK(Begin(trip, "seed-beast"));
    std::vector<std::string> paths = BeastPaths();
    // The admin has read the model, so its converted cache answers it now: the export must still walk the originals.
    std::vector<std::vector<uint8_t>> expected = ReadAll(trip.admin.source, paths);
    CHECK(!expected[0].empty() && !expected[1].empty() && expected[2] == trip.world.files.at(BEAST_COAT) && expected[3] == trip.world.files.at(BEAST_MANE));

    Retail::SeedReport report = trip.admin.source.ExportSeed({ BEAST_DISPLAY }, {}, trip.player.config.seed);
    std::set<uint32_t> files = { BEAST_MODEL, BEAST_SKIN, BEAST_BODY_TEXTURE, BEAST_HORN_TEXTURE, BEAST_ANIM, BEAST_COAT, BEAST_MANE, BEAST_BELLY };
    CHECK(report.error.empty() && report.skipped == 0);
    CHECK(SeedIds(trip.player) == Union(trip.world.tables, files) && report.files == files.size() + trip.world.tables.size());

    std::vector<std::vector<uint8_t>> served = ReadAll(trip.player.source, paths);
    CHECK(served == expected);
    // The model names its textures: each reads from the seed too.
    std::vector<std::string> textures = TextureNames(served[0]);
    CHECK(textures.size() == 2);
    for (const std::string& texture : textures) {
        std::vector<uint8_t> bytes;
        std::string error;
        CHECK(trip.player.source.Read(texture.c_str(), bytes, error) && bytes.size() == 8);
    }
    CHECK(trip.storage.opens == 0);
}

TEST(ASeedServesAModelOnASkeletonAndTheDefaultSkinItIsNamedAfter) {
    Trip trip;
    CHECK(Begin(trip, "seed-wolf"));
    std::string model = Retail::CreatureModelName(WOLF_MODEL);
    std::vector<std::string> paths = { model, SkinOf(model), TexturePath(WOLF_PELT_A) };
    std::vector<std::vector<uint8_t>> expected = ReadAll(trip.admin.source, paths);
    CHECK(!expected[0].empty() && !expected[1].empty());

    Retail::SeedReport report = trip.admin.source.ExportSeed({ WOLF_DISPLAY }, {}, trip.player.config.seed);
    CHECK(report.error.empty() && report.skipped == 0);
    // The skeleton, its parent and their animations, and pelt B, which the model names and the display does not.
    std::set<uint32_t> files = { WOLF_MODEL, WOLF_SKIN, WOLF_EYES, WOLF_SKELETON, WOLF_PARENT_SKELETON, WOLF_ANIM, WOLF_PARENT_ANIM, WOLF_PELT_A, WOLF_PELT_B };
    CHECK(SeedIds(trip.player) == Union(trip.world.tables, files));

    std::vector<std::vector<uint8_t>> served = ReadAll(trip.player.source, paths);
    CHECK(served == expected);
    std::vector<std::string> named = TextureNames(served[0]);
    CHECK(named.size() == 2);
    for (const std::string& texture : named) {
        std::vector<uint8_t> bytes;
        std::string error;
        CHECK(trip.player.source.Read(texture.c_str(), bytes, error) && bytes.size() == 8);
    }
    CHECK(trip.storage.opens == 0);
}

TEST(ASeedServesAnNpcOnItsRetailBody) {
    Trip trip;
    CHECK(Begin(trip, "seed-body"));
    std::string body = Retail::NpcBodyModelName(BODY_MODEL);
    std::vector<std::string> paths = { body, SkinOf(body), "Textures\\BakedNpcTextures\\" + Retail::NpcBakeName(BODY_BAKE, BODY_EXTRA),
        "Character\\" + Retail::RetailFileName(BODY_LOOK) + ".blp" };
    std::vector<std::vector<uint8_t>> expected = ReadAll(trip.admin.source, paths);
    CHECK(!expected[0].empty() && !expected[1].empty());

    Retail::SeedReport report = trip.admin.source.ExportSeed({ BODY_DISPLAY }, {}, trip.player.config.seed);
    CHECK(report.error.empty() && report.skipped == 0);
    // The body is on the wolf's skeleton.
    std::set<uint32_t> files = { BODY_MODEL, BODY_SKIN, BODY_BAKE, BODY_LOOK, WOLF_SKELETON, WOLF_PARENT_SKELETON, WOLF_ANIM, WOLF_PARENT_ANIM };
    CHECK(SeedIds(trip.player) == Union(trip.world.tables, files));

    std::vector<std::vector<uint8_t>> served = ReadAll(trip.player.source, paths);
    CHECK(served == expected && served[3] == trip.world.files.at(BODY_LOOK) && trip.storage.opens == 0);
}

// The armour as a player of any stock race and gender wears it, and its icon.
static std::vector<std::string> ArmourPaths() {
    std::vector<std::string> paths;
    for (uint32_t race = 1; race <= LAST_STOCK_RACE; race++) {
        for (uint32_t gender = 0; gender <= 1; gender++) {
            paths.push_back(HelmPath(race, gender));
            paths.push_back(SkinOf(paths.back()));
        }
    }
    for (const std::string& path : { TexturePath(HELM_SKIN), TexturePath(ARMOUR_ICON), BodyTexturePath(CHEST_MALE, CHEST_FEMALE, 'm'),
            BodyTexturePath(CHEST_MALE, CHEST_FEMALE, 'f'), BodyTexturePath(SLEEVE_BOTH, SLEEVE_BOTH, 'u') }) {
        paths.push_back(path);
    }
    return paths;
}

TEST(ASeedServesAnArmourItemWithAHelmForEveryRaceAndGender) {
    Trip trip;
    CHECK(Begin(trip, "seed-armour"));
    std::vector<std::string> paths = ArmourPaths();
    std::vector<std::vector<uint8_t>> expected = ReadAll(trip.admin.source, paths);

    Retail::SeedReport report = trip.admin.source.ExportSeed({}, { ARMOUR_DISPLAY }, trip.player.config.seed);
    CHECK(report.error.empty() && report.skipped == 0);
    std::set<uint32_t> files = { HELM_MODEL_SKIN, HELM_MODEL_TEXTURE, HELM_SKIN, ARMOUR_ICON, CHEST_MALE, CHEST_FEMALE, SLEEVE_BOTH };
    for (uint32_t race = 1; race <= LAST_STOCK_RACE; race++) {
        for (uint32_t gender = 0; gender <= 1; gender++) {
            files.insert(HelmFile(race, gender));
            files = Union(files, FitBody(race, gender));
        }
    }
    CHECK(SeedIds(trip.player) == Union(trip.world.tables, files));

    std::vector<std::vector<uint8_t>> served = ReadAll(trip.player.source, paths);
    CHECK(served == expected && trip.storage.opens == 0);
    for (const std::vector<uint8_t>& bytes : served) {
        CHECK(!bytes.empty());
    }
}

static bool Mentions(const std::vector<std::string>& lines, const std::string& text) {
    return std::any_of(lines.begin(), lines.end(), [&text](const std::string& line) { return line.find(text) != std::string::npos; });
}

TEST(ExportingLeavesOutTheIdsRetailLacksAndSaysSo) {
    Trip trip;
    std::vector<std::string> warnings;
    CHECK(Begin(trip, "seed-unknown"));
    trip.admin.source.SetWarningSink([&warnings](const std::string& warning) { warnings.push_back(warning); });

    Retail::SeedReport report = trip.admin.source.ExportSeed({ 999001, BEAST_DISPLAY }, { 999002, ARMOUR_DISPLAY }, trip.player.config.seed);
    CHECK(report.skipped == 2 && report.error.empty());
    CHECK(Mentions(warnings, "999001") && Mentions(warnings, "999002"));
    std::set<uint32_t> ids = SeedIds(trip.player);
    CHECK(ids.count(BEAST_MODEL) && ids.count(HelmFile(HUMAN, MALE)) && !ids.count(999001) && !ids.count(999002));
}

TEST(AnOriginalThatCannotBeReadIsReportedAndItsDisplayLeftOut) {
    Trip trip;
    NoStorage storage;
    CHECK(Begin(trip, "seed-unreadable"));
    trip.admin.source.SetStorageOpener(storage.Make());
    std::filesystem::remove(Retail::DecodedCachePath(trip.admin.config, BEAST_ANIM));

    Retail::SeedReport report = trip.admin.source.ExportSeed({ BEAST_DISPLAY, NPC_DISPLAY }, {}, trip.player.config.seed);
    CHECK(report.skipped == 1 && report.error.find("offline") != std::string::npos);
    // The other display is exported all the same.
    CHECK(SeedIds(trip.player).count(NPC_BAKE));
}

// The HD bake needs the installed body to be the HD port, which the client finds by reading it: without one the display
// keeps its retail model, which it cannot draw, so there is nothing to seed.
TEST(AnNpcTheClientPlansNoBodyForIsLeftOutAndSaysSo) {
    Trip trip;
    std::vector<std::string> warnings;
    CHECK(Begin(trip, "seed-noplan"));
    trip.admin.source.SetWarningSink([&warnings](const std::string& warning) { warnings.push_back(warning); });

    Retail::SeedReport report = trip.admin.source.ExportSeed({ WARP_DISPLAY }, {}, trip.player.config.seed);
    CHECK(report.skipped == 1 && report.error.empty() && Mentions(warnings, std::to_string(WARP_DISPLAY)));
    CHECK(!SeedIds(trip.player).count(HD_BAKE));
}

// Warping needs the retail HD body to match the installed one against, and the admin has matched it and kept the
// warped bake already: the export reads the originals all the same.
TEST(ASeedServesAWarpedNpcBakeAndTheRetailBodyItIsMatchedAgainst) {
    Trip trip;
    CHECK(Begin(trip, "seed-warp"));
    InstallHumanMale(trip.admin.source);
    InstallHumanMale(trip.player.source);
    std::vector<std::string> paths = { "Textures\\BakedNpcTextures\\" + Retail::WarpedBakeName(HD_BAKE, HUMAN, MALE) };
    std::vector<std::vector<uint8_t>> expected = ReadAll(trip.admin.source, paths);
    CHECK(!expected[0].empty());

    Retail::SeedReport report = trip.admin.source.ExportSeed({ WARP_DISPLAY }, {}, trip.player.config.seed);
    CHECK(report.error.empty() && report.skipped == 0);
    std::set<uint32_t> files = { HD_BAKE, Retail::RetailHdBodyFileDataId(HUMAN, MALE), HD_BODY_SKIN };
    CHECK(SeedIds(trip.player) == Union(trip.world.tables, files));

    std::vector<std::vector<uint8_t>> served = ReadAll(trip.player.source, paths);
    CHECK(served == expected && trip.storage.opens == 0);
}

// One resource named on both sides, a file on each: asked for by the names the display gives, and no helm.
TEST(ASeedServesAPairOfShouldersByTheNamesTheDisplayGivesThem) {
    Trip trip;
    CHECK(Begin(trip, "seed-shoulders"));
    std::string left = ModelPath(SHOULDER_LEFT);
    std::string right = ModelPath(SHOULDER_RIGHT);
    std::vector<std::string> paths = { left, SkinOf(left), right, SkinOf(right), TexturePath(SHOULDER_SKIN) };
    std::vector<std::vector<uint8_t>> expected = ReadAll(trip.admin.source, paths);

    Retail::SeedReport report = trip.admin.source.ExportSeed({}, { SHOULDER_DISPLAY }, trip.player.config.seed);
    CHECK(report.error.empty() && report.skipped == 0);
    CHECK(SeedIds(trip.player) == Union(trip.world.tables, { SHOULDER_LEFT, SHOULDER_RIGHT, SHOULDER_MODEL_SKIN, SHOULDER_SKIN }));

    std::vector<std::vector<uint8_t>> served = ReadAll(trip.player.source, paths);
    CHECK(served == expected && !served[0].empty() && !served[2].empty() && trip.storage.opens == 0);
}

TEST(ASeedHoldsEveryTableTheDisplayLoadersRead) {
    Trip trip;
    CHECK(Begin(trip, "seed-tables"));
    Retail::SeedReport report = trip.admin.source.ExportSeed({}, {}, trip.player.config.seed);
    CHECK(report.error.empty() && report.skipped == 0 && report.files == trip.world.tables.size());
    CHECK(SeedIds(trip.player) == trip.world.tables);

    Retail::CascSource& player = trip.player.source;
    CHECK(player.Displays() && player.ItemDisplays() && player.NpcTables() && player.Races());
    CHECK(player.Displays()->Display(BEAST_DISPLAY) && player.ItemDisplays()->Display(ARMOUR_DISPLAY) && player.NpcTables()->Extra(NPC_EXTRA));
    CHECK(trip.storage.opens == 0);
}

TEST(ASeedServesAnNpcHumanoidDisplay) {
    Trip trip;
    CHECK(Begin(trip, "seed-npc"));
    std::vector<std::string> paths = NpcPaths();
    std::vector<std::vector<uint8_t>> expected = ReadAll(trip.admin.source, paths);

    Retail::SeedReport report = trip.admin.source.ExportSeed({ NPC_DISPLAY }, {}, trip.player.config.seed);
    CHECK(report.error.empty() && report.skipped == 0);
    // The installed body draws the NPC, so its retail model is not read. The helm is the human male's file, and a man
    // wears the male file of a section.
    std::set<uint32_t> files = { NPC_BAKE, HelmFile(HUMAN, MALE), HELM_MODEL_SKIN, HELM_MODEL_TEXTURE, HELM_SKIN, CHEST_MALE, SLEEVE_BOTH, SHOULDER_LEFT,
        SHOULDER_RIGHT, SHOULDER_MODEL_SKIN, SHOULDER_SKIN };
    CHECK(SeedIds(trip.player) == Union(Union(trip.world.tables, files), FitBody(HUMAN, MALE)));

    std::vector<std::vector<uint8_t>> served = ReadAll(trip.player.source, paths);
    CHECK(served == expected && served[0] == trip.world.files.at(NPC_BAKE) && !served[1].empty() && !served[3].empty());
    CHECK(trip.storage.opens == 0);
}
