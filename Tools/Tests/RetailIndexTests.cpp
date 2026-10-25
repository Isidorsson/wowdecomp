#include <string>
#include "Engine/Source/Retail/RetailIndex.h"
#include "Tools/Tests/BrowserFixtures.h"
#include "Tools/Tests/Check.h"

using Retail::BrowseFilters;
using Retail::BrowseKind;

static std::vector<uint32_t> IdsOf(const Retail::SearchPage& page) {
    std::vector<uint32_t> ids;
    for (const Retail::SearchRow& row : page.rows) {
        ids.push_back(row.id);
    }
    return ids;
}

static std::string NameOf(const std::map<uint32_t, std::string>& names, uint32_t id) {
    auto found = names.find(id);
    return found != names.end() ? found->second : std::string();
}

// R1: tables to entries.

TEST(BrowserNormalizesCaseAccentsAndSpaces) {
    CHECK(Retail::NormalizeSearchText("  F\xC3\xAB" "anor   \xC3\x89" "LITE ") == "feanor elite");
    CHECK(Retail::NormalizeSearchText("Dal'Rend's") == "dal'rend's");
    CHECK(Retail::NormalizeSearchText("\xC3\x9F\xC3\xB1") == "ssn");
    CHECK(Retail::NormalizeSearchText("   ").empty());
}

TEST(BrowserKindsParseByName) {
    BrowseKind kind;
    CHECK(Retail::ParseBrowseKind("mount", kind) && kind == BrowseKind::Mount);
    CHECK(Retail::ParseBrowseKind("creature", kind) && kind == BrowseKind::Creature);
    CHECK(!Retail::ParseBrowseKind("spell", kind));
    CHECK(std::string(Retail::BrowseKindName(BrowseKind::Set)) == "set");
}

// Creature.Classification values of the 12.1.5 dump (841631.csv): 0 16719 rows, 1 5734 (Ragnaros 11502, Archaedas 2748),
// 6 317 (Wild Imp 55659), 4 188 (Nagidna 85555), 2 145 (Titarus 83713). No row has 3, the old world boss value:
// retail bosses are elites, so the groups follow 3.3.5's ranks without a boss one.
TEST(BrowserGroupsClassifications) {
    using Retail::ClassificationGroup;
    CHECK(Retail::GroupOfClassification(0) == ClassificationGroup::Normal);
    CHECK(Retail::GroupOfClassification(1) == ClassificationGroup::Elite);
    CHECK(Retail::GroupOfClassification(2) == ClassificationGroup::RareElite);
    CHECK(Retail::GroupOfClassification(4) == ClassificationGroup::Rare);
    CHECK(Retail::GroupOfClassification(3) == ClassificationGroup::Elite);
    CHECK(Retail::GroupOfClassification(5) == ClassificationGroup::Normal);
    CHECK(Retail::GroupOfClassification(6) == ClassificationGroup::Normal);
}

TEST(BrowserJoinsCreaturesToUpToFourDisplays) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    CHECK(index.Count(BrowseKind::Creature) == 5);
    const Retail::BrowseCreature* guard = index.Creature(92123);
    CHECK(guard && guard->name == "Stormwind Guard" && guard->type == 7 && guard->classification == 0);
    CHECK(guard && guard->displays.size() == 1 && guard->displays[0].id == 3258 && guard->displays[0].probability == 1.0f);
    CHECK(guard && guard->displays[0].known && guard->displays[0].facts.extraId == 130617 && guard->displays[0].facts.race == 1);
    CHECK(guard && guard->displays[0].facts.modelFdid == 1011653);
    const Retail::BrowseCreature* captain = index.Creature(178184);
    CHECK(captain && captain->displays.size() == 2 && captain->displays[1].id == 3258);
    const Retail::BrowseCreature* elite = index.Creature(178185);
    CHECK(elite && elite->displays.size() == 1 && !elite->displays[0].known);
    CHECK(!index.Creature(68));
    CHECK(NameOf(index.CreatureTypeNames(), 7) == "Humanoid" && NameOf(index.FamilyNames(), 1) == "Wolf");
}

TEST(BrowserJoinsItemNamesToTheirDisplayInfo) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    CHECK(index.Count(BrowseKind::Item) == 5);
    const Retail::BrowseItem* thunderfury = index.Item(19019);
    CHECK(thunderfury && thunderfury->name == "Thunderfury, Blessed Blade of the Windseeker");
    CHECK(thunderfury && thunderfury->quality == 5 && thunderfury->itemLevel == 29 && thunderfury->inventoryType == 13);
    CHECK(thunderfury && thunderfury->classId == 2 && thunderfury->subclassId == 7 && thunderfury->icon == 135349);
    CHECK(thunderfury && thunderfury->sheatheType == 1 && thunderfury->material == 1);
    CHECK(thunderfury && thunderfury->appearances.size() == 1);
    if (thunderfury && thunderfury->appearances.size() == 1) {
        const Retail::BrowseAppearance& look = thunderfury->appearances[0];
        CHECK(look.modifiedAppearanceId == 7545 && look.appearanceId == 5353 && look.displayId == 30606 && look.icon == 135349);
        CHECK(look.known && look.facts.models[0] == 145000);
    }
    const Retail::BrowseItem* chest = index.Item(124913);
    CHECK(chest && chest->icon == 1091986 && chest->expansion == 5 && chest->allowableClass == 32 && chest->inventoryType == 5);
    CHECK(index.SubclassName(4, 4) == "Plate" && index.SubclassName(2, 7) == "Swords" && index.SubclassName(9, 9).empty());
}

TEST(BrowserListsTransmogAndItemSetPieces) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    CHECK(index.Count(BrowseKind::Set) == 2);
    const Retail::BrowseSet* dreadplate = index.Set(17);
    CHECK(dreadplate && dreadplate->name == "Wild Combatant's Dreadplate Armor" && dreadplate->source == Retail::SetSource::Transmog);
    CHECK(dreadplate && dreadplate->classMask == 32 && dreadplate->expansion == 5 && dreadplate->armour == 4);
    CHECK(dreadplate && dreadplate->pieces.size() == 2);
    if (dreadplate && dreadplate->pieces.size() == 2) {
        CHECK(dreadplate->pieces[0].itemId == 124913 && dreadplate->pieces[0].modifiedAppearanceId == 70584);
        CHECK(dreadplate->pieces[0].displayId == 142221 && dreadplate->pieces[0].inventoryType == 5);
        CHECK(dreadplate->pieces[1].itemId == 124914 && dreadplate->pieces[1].displayId == 142222);
    }
    const Retail::BrowseSet* dalrend = index.Set(Retail::ITEM_SET_KEY_BASE + 41);
    CHECK(dalrend && dalrend->setId == 41 && dalrend->source == Retail::SetSource::ItemSet && dalrend->name == "Dal'Rend's Arms");
    CHECK(dalrend && dalrend->pieces.size() == 2);
    if (dalrend && dalrend->pieces.size() == 2) {
        CHECK(dalrend->pieces[0].itemId == 12940 && dalrend->pieces[0].displayId == 25646 && dalrend->pieces[0].inventoryType == 21);
        CHECK(dalrend->pieces[1].itemId == 12939 && dalrend->pieces[1].displayId == 25647);
    }
}

TEST(BrowserListsWhatUsesEachModel) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    const Retail::BrowseModel* human = index.Model(1011653);
    CHECK(human && human->creature && human->users.size() == 2);
    if (human && human->users.size() == 2) {
        CHECK(human->users[0].kind == BrowseKind::Creature && human->users[0].id == 92123 && human->users[0].displayId == 3258);
        CHECK(human->users[1].id == 178184);
    }
    const Retail::BrowseModel* blade = index.Model(145000);
    CHECK(blade && !blade->creature && blade->users.size() == 1 && blade->users[0].kind == BrowseKind::Item && blade->users[0].id == 19019);
    const Retail::BrowseModel* horse = index.Model(123456);
    CHECK(horse && horse->users.size() == 1 && horse->users[0].kind == BrowseKind::Mount);
}

TEST(BrowserJoinsMountsToTheirDisplays) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    const Retail::BrowseMount* horse = index.Mount(6);
    CHECK(horse && horse->name == "Brown Horse" && horse->spellId == 458 && horse->mountType == 230);
    CHECK(horse && horse->displays.size() == 1 && horse->displays[0].id == 2404 && horse->displays[0].facts.modelFdid == 123456);
}

// R2: search.

TEST(BrowserSearchRanksExactThenPrefixThenSubstring) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "Stormwind Guard", {}, 0)) == std::vector<uint32_t>({ 92123, 178184 }));
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "stormwind", {}, 0)) == std::vector<uint32_t>({ 92123, 178184, 178185 }));
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "ST\xC3\x96" "RMWIND  guard", {}, 0)) == std::vector<uint32_t>({ 92123, 178184 }));
    CHECK(IdsOf(index.Search(BrowseKind::Item, "dal'rend", {}, 0)) == std::vector<uint32_t>({ 12940, 12939 }));
}

TEST(BrowserSearchFindsNumbersById) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "92123", {}, 0)) == std::vector<uint32_t>({ 92123 }));
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "3258", {}, 0)) == std::vector<uint32_t>({ 92123, 178184 }));
    CHECK(IdsOf(index.Search(BrowseKind::Item, "30606", {}, 0)) == std::vector<uint32_t>({ 19019 }));
    CHECK(IdsOf(index.Search(BrowseKind::Item, "19019", {}, 0)) == std::vector<uint32_t>({ 19019 }));
    CHECK(IdsOf(index.Search(BrowseKind::Model, "1011653", {}, 0)) == std::vector<uint32_t>({ 1011653 }));
    CHECK(IdsOf(index.Search(BrowseKind::Mount, "2404", {}, 0)) == std::vector<uint32_t>({ 6 }));
    CHECK(index.Search(BrowseKind::Creature, "999", {}, 0).rows.empty());
}

TEST(BrowserSearchWithoutTextOrFilterFindsNothing) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    CHECK(index.Search(BrowseKind::Creature, "", {}, 0).total == 0);
    CHECK(index.Search(BrowseKind::Item, "   ", {}, 0).rows.empty());
}

TEST(BrowserSearchPagesByFifty) {
    BrowserFixture fixture = MakeBrowserFixture();
    std::vector<TextRow> rows;
    for (uint32_t i = 0; i < 120; i++) {
        rows.push_back(CreatureRow(1000 + i, "Guard " + std::to_string(1000 + i), 0, 7, 0, { 3258 }));
    }
    fixture.creatures = TextTable(Retail::CREATURE_LAYOUT, { 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 3 }, rows);
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    Retail::SearchPage first = index.Search(BrowseKind::Creature, "guard", {}, 0);
    CHECK(first.total == 120 && first.pages == 3 && first.page == 0 && first.rows.size() == 50 && first.rows[0].id == 1000);
    Retail::SearchPage last = index.Search(BrowseKind::Creature, "guard", {}, 2);
    CHECK(last.rows.size() == 20 && last.rows[19].id == 1119 && last.page == 2);
    CHECK(index.Search(BrowseKind::Creature, "guard", {}, 3).rows.empty());
    CHECK(index.SearchAll(BrowseKind::Creature, "guard", {}).size() == 120);
}

TEST(BrowserSearchFiltersItems) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters plateChest;
    plateChest.armour = 4;
    plateChest.inventoryType = 5;
    CHECK(IdsOf(index.Search(BrowseKind::Item, "", plateChest, 0)) == std::vector<uint32_t>({ 124913 }));
    CHECK(IdsOf(index.Search(BrowseKind::Item, "wild", plateChest, 0)) == std::vector<uint32_t>({ 124913 }));
    BrowseFilters robes;
    robes.inventoryType = 20;
    CHECK(IdsOf(index.Search(BrowseKind::Item, "", robes, 0)) == std::vector<uint32_t>({ 124913 }));
    BrowseFilters legendary;
    legendary.quality = 5;
    CHECK(IdsOf(index.Search(BrowseKind::Item, "", legendary, 0)) == std::vector<uint32_t>({ 19019 }));
    BrowseFilters levels;
    levels.minItemLevel = 40;
    levels.maxItemLevel = 50;
    CHECK(index.Search(BrowseKind::Item, "", levels, 0).total == 2);
    BrowseFilters swords;
    swords.weaponSubclass = 7;
    CHECK(index.Search(BrowseKind::Item, "", swords, 0).total == 3);
    BrowseFilters draenor;
    draenor.expansion = 5;
    CHECK(index.Search(BrowseKind::Item, "", draenor, 0).total == 2);
    const Retail::SearchPage found = index.Search(BrowseKind::Item, "thunderfury", {}, 0);
    CHECK(found.rows.size() == 1 && found.rows[0].displayId == 30606 && found.rows[0].icon == 135349 && found.rows[0].quality == 5);
}

TEST(BrowserSearchFiltersCreatures) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters beasts;
    beasts.creatureType = 1;
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "", beasts, 0)) == std::vector<uint32_t>({ 3823, 85555 }));
    BrowseFilters wolves;
    wolves.family = 1;
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "", wolves, 0)) == std::vector<uint32_t>({ 3823 }));
    BrowseFilters rares;
    rares.classification = 2;
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "", rares, 0)) == std::vector<uint32_t>({ 85555 }));
    BrowseFilters npcs;
    npcs.humanoid = 1;
    CHECK(IdsOf(index.Search(BrowseKind::Creature, "", npcs, 0)) == std::vector<uint32_t>({ 92123, 178184 }));
    BrowseFilters plain;
    plain.humanoid = 0;
    CHECK(index.Search(BrowseKind::Creature, "", plain, 0).total == 3);
    BrowseFilters humans;
    humans.race = 1;
    CHECK(index.Search(BrowseKind::Creature, "stormwind", humans, 0).total == 2);
    const Retail::SearchPage guard = index.Search(BrowseKind::Creature, "Stormwind Guard", {}, 0);
    CHECK(!guard.rows.empty() && guard.rows[0].displayId == 3258);
}

TEST(BrowserSearchFiltersSets) {
    BrowserFixture fixture = MakeBrowserFixture();
    Retail::RetailIndex index = BuildFixtureIndex(fixture);
    BrowseFilters deathKnight;
    deathKnight.classId = 6;
    CHECK(index.Search(BrowseKind::Set, "", deathKnight, 0).total == 2);
    BrowseFilters warrior;
    warrior.classId = 1;
    CHECK(IdsOf(index.Search(BrowseKind::Set, "", warrior, 0)) == std::vector<uint32_t>({ Retail::ITEM_SET_KEY_BASE + 41 }));
    BrowseFilters plate;
    plate.armour = 4;
    CHECK(IdsOf(index.Search(BrowseKind::Set, "", plate, 0)) == std::vector<uint32_t>({ 17 }));
    CHECK(IdsOf(index.Search(BrowseKind::Set, "41", {}, 0)) == std::vector<uint32_t>({ Retail::ITEM_SET_KEY_BASE + 41 }));
}
