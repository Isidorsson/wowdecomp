#include "Engine/Source/Retail/RetailPath.h"
#include "Tools/Tests/Check.h"

using Retail::AliasTable;
using Retail::IsRetailPath;
using Retail::NormalizePath;
using Retail::ParseRetailPath;

TEST(PathsNormalizeCaseAndSlashes) {
    CHECK(NormalizePath("Retail\\Item\\Sword.M2") == "retail/item/sword.m2");
    CHECK(NormalizePath("Creature/Rabbit/Rabbit.m2") == "creature/rabbit/rabbit.m2");
}

TEST(RetailPathsNeedThePrefix) {
    CHECK(IsRetailPath("retail/916541.m2"));
    CHECK(!IsRetailPath("creature/rabbit/rabbit.m2"));
    CHECK(!IsRetailPath("retailx/1.m2"));
}

TEST(RetailPathsCarryADecimalFileDataId) {
    uint32_t id = 0;

    CHECK(ParseRetailPath("retail/916541.m2", id) && id == 916541);
    CHECK(ParseRetailPath("retail/916543.blp", id) && id == 916543);
    CHECK(ParseRetailPath("retail/4294967295.blp", id) && id == 4294967295u);
}

TEST(MalformedRetailIdsAreRefused) {
    uint32_t id = 0;

    CHECK(!ParseRetailPath("retail/4294967296.blp", id));
    CHECK(!ParseRetailPath("retail/12a.m2", id));
    CHECK(!ParseRetailPath("retail/.m2", id));
    CHECK(!ParseRetailPath("retail/item/sword.m2", id));
    CHECK(!ParseRetailPath("retail/-5.m2", id));
    CHECK(!ParseRetailPath("creature/rabbit/rabbit.m2", id));
}

TEST(AliasesMapStockPathsToFileDataIds) {
    AliasTable aliases;
    std::string error;
    uint32_t id = 0;

    CHECK(aliases.Parse("# replacements\n916541;Item\\ObjectComponents\\Weapon\\Sword_1H.m2\r\n\n775194;World/Tree.blp\n", error));
    CHECK(aliases.Size() == 2);
    CHECK(aliases.Find("item/objectcomponents/weapon/sword_1h.m2", id) && id == 916541);
    CHECK(aliases.Find("world/tree.blp", id) && id == 775194);
    CHECK(!aliases.Find("world/other.blp", id));
}

TEST(OnlyFileDataIdsAndAliasesAreClaimed) {
    AliasTable aliases;
    std::string error;
    CHECK(aliases.Parse("134400;Interface\\Icons\\INV_Misc_Coin_01.blp\n", error));

    CHECK(Retail::ClaimsPath("retail/916541.m2", aliases));
    CHECK(Retail::ClaimsPath("interface/icons/inv_misc_coin_01.blp", aliases));
    CHECK(!Retail::ClaimsPath("interface/icons/inv_misc_coin_02.blp", aliases));
}

TEST(NamedRetailFoldersStayInTheMpqs) {
    AliasTable aliases;

    // Downported content ships in patch MPQs under Retail\ (wow-toolbox-editor's client paths).
    CHECK(!Retail::ClaimsPath("retail/creature/igc_umbric/igc_umbric.m2", aliases));
    CHECK(!Retail::ClaimsPath("retail/creature/shadowguardethereal_caster01/1572456.blp", aliases));
    CHECK(!Retail::ClaimsPath("retail/4294967296.blp", aliases));
}

TEST(SkinPathsFollowTheClientsNaming) {
    CHECK(Retail::SkinPathFor("retail/916541.m2", 0) == "retail/91654100.skin");
    CHECK(Retail::SkinPathFor("retail/916541.m2", 3) == "retail/91654103.skin");
    CHECK(Retail::SkinPathFor("interface/icons/inv_misc_coin_01.m2", 1) == "interface/icons/inv_misc_coin_0101.skin");
}

TEST(BrokenAliasRowsAreRefused) {
    AliasTable aliases;
    std::string error;

    CHECK(!aliases.Parse("916541\n", error));
    CHECK(!error.empty());
    CHECK(!aliases.Parse("abc;World/Tree.blp\n", error));
    CHECK(!aliases.Parse("1;\n", error));
    CHECK(!aliases.Parse("1;World/Tree.blp\n2;world\\tree.BLP\n", error));
}

// A served item display row names retail files where the client puts a stock folder first
// (Item\ObjectComponents\Weapon\ + name), so a retail file name counts in any folder.
TEST(RetailFileNamesCountInAnyFolder) {
    uint32_t id = 0;

    CHECK(Retail::RetailFileName(8266369) == "retail-8266369");
    CHECK(ParseRetailPath("item/objectcomponents/weapon/retail-8266369.m2", id) && id == 8266369);
    CHECK(ParseRetailPath("item/objectcomponents/shoulder/retail-9002.blp", id) && id == 9002);
    CHECK(ParseRetailPath("interface/icons/retail-134400.blp", id) && id == 134400);
    CHECK(Retail::ClaimsPath("item/objectcomponents/cape/retail-77.blp", AliasTable()));
}

TEST(MalformedRetailFileNamesAreRefused) {
    uint32_t id = 0;

    CHECK(!ParseRetailPath("item/objectcomponents/weapon/retail-12a.m2", id));
    CHECK(!ParseRetailPath("item/objectcomponents/weapon/retail-.m2", id));
    CHECK(!ParseRetailPath("item/objectcomponents/weapon/retail_12.m2", id));
    CHECK(!ParseRetailPath("item/objectcomponents/weapon/sword_retail-12.m2", id));
    CHECK(!ParseRetailPath("item/retail-12/sword.m2", id));
    CHECK(!ParseRetailPath("item/objectcomponents/weapon/retail-4294967296.m2", id));
}

// The client loads a body section's texture as <name>_U.blp, else <name>_M or _F by the character's gender
// (CCharacterComponent::LoadItemComponentTexture), so the name carries both genders' files.
TEST(BodyTextureNamesCarryBothGenders) {
    CHECK(Retail::RetailBodyTextureName(9000, 9000) == "retail-9000");
    CHECK(Retail::RetailBodyTextureName(9010, 9011) == "retail-9010-9011");
    CHECK(Retail::RetailBodyTextureName(9080, 0) == "retail-9080-0");
}

TEST(BodyTexturePathsResolveByTheClientsSuffix) {
    uint32_t id = 0;

    CHECK(ParseRetailPath("item/texturecomponents/torsouppertexture/retail-9000_u.blp", id) && id == 9000);
    CHECK(ParseRetailPath("item/texturecomponents/torsouppertexture/retail-9010-9011_m.blp", id) && id == 9010);
    CHECK(ParseRetailPath("item/texturecomponents/torsouppertexture/retail-9010-9011_f.blp", id) && id == 9011);
    CHECK(Retail::ClaimsPath("item/texturecomponents/foottexture/retail-9010-9011_f.blp", AliasTable()));
}

TEST(BodyTexturePathsTheClientMustNotFindAreRefused) {
    uint32_t id = 0;

    // A gendered pair has no file for both, so the client goes on to its gender's.
    CHECK(!ParseRetailPath("item/texturecomponents/torsouppertexture/retail-9010-9011_u.blp", id));
    // A file for both is found as _U first; the gendered names are never asked for.
    CHECK(!ParseRetailPath("item/texturecomponents/torsouppertexture/retail-9000_m.blp", id));
    // A gender without a file stays unclaimed, so the section draws nothing for it, as in retail.
    CHECK(!ParseRetailPath("item/texturecomponents/torsouppertexture/retail-9080-0_f.blp", id));
    CHECK(!ParseRetailPath("item/texturecomponents/torsouppertexture/retail-9010-9011.blp", id));
    CHECK(!ParseRetailPath("item/texturecomponents/torsouppertexture/retail-1-2-3_m.blp", id));
    CHECK(!ParseRetailPath("item/texturecomponents/torsouppertexture/retail-9010-9011_x.blp", id));
}

// The client asks for a helm as Item\ObjectComponents\Head\<name>_<ChrRaces.ClientPrefix><M|F>.mdx
// (ComposeHelmModelFilePath), which CM2Cache lowercases and loads as .m2.

using Retail::ParseRaceFileName;
using Retail::RaceFileRequest;

static bool Requests(const std::string& path, uint32_t fileDataId, uint32_t race, uint32_t gender) {
    RaceFileRequest request;
    return ParseRaceFileName(path, request) && request.fileDataId == fileDataId && request.race == race && request.gender == gender;
}

TEST(StockRacesHaveTheirClientPrefixes) {
    CHECK(Retail::RacePrefix(1) == "hu");
    CHECK(Retail::RacePrefix(2) == "or");
    CHECK(Retail::RacePrefix(3) == "dw");
    CHECK(Retail::RacePrefix(4) == "ni");
    CHECK(Retail::RacePrefix(5) == "sc");
    CHECK(Retail::RacePrefix(6) == "ta");
    CHECK(Retail::RacePrefix(7) == "gn");
    CHECK(Retail::RacePrefix(8) == "tr");
    CHECK(Retail::RacePrefix(9) == "go");
    CHECK(Retail::RacePrefix(10) == "be");
    CHECK(Retail::RacePrefix(11) == "dr");
    CHECK(Retail::RacePrefix(0).empty());
    CHECK(Retail::RacePrefix(12).empty());
}

TEST(HelmPathsNameTheRaceAndGender) {
    CHECK(Requests("item/objectcomponents/head/retail-9100_hum.m2", 9100, 1, 0));
    CHECK(Requests("item/objectcomponents/head/retail-9100_huf.m2", 9100, 1, 1));
    CHECK(Requests("item/objectcomponents/head/retail-9100_orm.mdx", 9100, 2, 0));
    CHECK(Requests("item/objectcomponents/head/retail-9100_gof.m2", 9100, 9, 1));
    CHECK(Requests("item/objectcomponents/head/retail-9100_bem.m2", 9100, 10, 0));
    CHECK(Requests("item/objectcomponents/head/retail-4294967295_drf.m2", 4294967295u, 11, 1));
    CHECK(Requests("retail-9100_scm.m2", 9100, 5, 0));
}

TEST(MalformedHelmPathsAreRefused) {
    RaceFileRequest request;

    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-9100_x1m.m2", request));    // Not a prefix.
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-9100_hu.m2", request));     // No gender.
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-9100_hux.m2", request));
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-9100_humm.m2", request));
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-9100.m2", request));
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-_hum.m2", request));
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-12a_hum.m2", request));
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-4294967296_hum.m2", request));
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-9010-9011_hum.m2", request));
    CHECK(!ParseRaceFileName("item/objectcomponents/head/helm_hum.m2", request));
    CHECK(!ParseRaceFileName("item/objectcomponents/head/retail-9100_hum00.skin", request));   // The dependency map's.
}

TEST(BodyTexturesAndHelmsDoNotReadAsEachOther) {
    RaceFileRequest request;
    uint32_t id = 0;

    CHECK(!ParseRaceFileName("item/texturecomponents/torsouppertexture/retail-9000_u.blp", request));
    CHECK(!ParseRaceFileName("item/texturecomponents/torsouppertexture/retail-9010-9011_m.blp", request));
    CHECK(!ParseRaceFileName("item/texturecomponents/torsouppertexture/retail-9010-9011_f.blp", request));
    // A helm name means a file only RetailItemDisplays can choose, so it is no FileDataID by itself.
    CHECK(!ParseRetailPath("item/objectcomponents/head/retail-9100_hum.m2", id));
    CHECK(!ParseRetailPath("item/objectcomponents/head/retail-9100_huf.m2", id));
}

TEST(HelmPathsAreClaimed) {
    CHECK(Retail::ClaimsPath("item/objectcomponents/head/retail-9100_hum.m2", AliasTable()));
    CHECK(Retail::ClaimsPath("item/objectcomponents/head/retail-9100_drf.m2", AliasTable()));
    CHECK(Retail::ClaimsPath("item/objectcomponents/head/retail-9100_ktm.m2", AliasTable()));
    CHECK(!Retail::ClaimsPath("item/objectcomponents/head/retail-9100_k1m.m2", AliasTable()));
}

// A served race's helm (B6): its prefix comes from the retail ChrRaces row, which only the race tables resolve.
TEST(ServedRaceHelmPathsKeepTheirPrefix) {
    RaceFileRequest request;
    CHECK(ParseRaceFileName("item/objectcomponents/head/retail-9100_ktm.m2", request));
    CHECK(request.fileDataId == 9100 && request.race == 0 && request.prefix == "kt" && request.gender == 0);
    CHECK(ParseRaceFileName("item/objectcomponents/head/retail-9100_edf.m2", request));
    CHECK(request.race == 0 && request.prefix == "ed" && request.gender == 1);
    CHECK(ParseRaceFileName("item/objectcomponents/head/retail-9100_hum.m2", request));
    CHECK(request.race == 1 && request.prefix == "hu");
}

// A retail player-race body asked for as an NPC body (B3): its own name, which no plain model read shares.
TEST(NpcBodyNamesRoundTrip) {
    CHECK(Retail::NpcBodyModelName(119376) == "Character\\retail-119376_npcbody.m2");
    uint32_t id = 0;
    CHECK(Retail::ParseNpcBodyPath("character/retail-119376_npcbody.m2", id) && id == 119376);
    CHECK(Retail::ParseNpcBodyPath("creature/retail-307454_npcbody.m2", id) && id == 307454);
    CHECK(!Retail::ParseNpcBodyPath("character/retail-119376.m2", id));
    CHECK(!Retail::ParseNpcBodyPath("character/retail-119376_npcbody00.skin", id));
    CHECK(!Retail::ParseNpcBodyPath("character/retail-x_npcbody.m2", id));
    CHECK(!ParseRetailPath("character/retail-119376_npcbody.m2", id));
    CHECK(Retail::ClaimsPath("character/retail-119376_npcbody.m2", AliasTable()));
}

// A retail body's bake (B4, B7): the bake file and the Extra row, so the client finds the row's look by its skin.
TEST(NpcBakeNamesCarryTheBakeAndTheExtraRow) {
    CHECK(Retail::NpcBakeName(253789, 18975) == "retail-253789_npc18975.blp");
    uint32_t file = 0;
    uint32_t extra = 0;
    CHECK(Retail::ParseNpcBakeName("textures/bakednpctextures/retail-253789_npc18975.blp", file, extra) && file == 253789 && extra == 18975);
    CHECK(!Retail::ParseNpcBakeName("textures/bakednpctextures/retail-253789.blp", file, extra));
    CHECK(!Retail::ParseNpcBakeName("textures/bakednpctextures/retail-253789_npc.blp", file, extra));
    CHECK(!Retail::ParseNpcBakeName("textures/bakednpctextures/retail-253789_bake9m.blp", file, extra));
    uint32_t id = 0;
    CHECK(ParseRetailPath("textures/bakednpctextures/retail-253789_npc18975.blp", id) && id == 253789);
    CHECK(Retail::ClaimsPath("textures/bakednpctextures/retail-253789_npc18975.blp", AliasTable()));
}

TEST(WarpedBakeNamesCarryTheBakeAndBody) {
    CHECK(Retail::WarpedBakeName(8046191, 1, 0) == "retail-8046191_bake1m.blp");
    CHECK(Retail::WarpedBakeName(1049291, 11, 1) == "retail-1049291_bake11f.blp");

    Retail::WarpedBakeRequest request;
    CHECK(Retail::ParseWarpedBakeName("textures/bakednpctextures/retail-8046191_bake1m.blp", request));
    CHECK(request.fileDataId == 8046191 && request.race == 1 && request.gender == 0);
    CHECK(Retail::ParseWarpedBakeName("textures/bakednpctextures/retail-1049291_bake11f.blp", request));
    CHECK(request.fileDataId == 1049291 && request.race == 11 && request.gender == 1);
    CHECK(Retail::ClaimsPath("textures/bakednpctextures/retail-8046191_bake1m.blp", AliasTable()));
}

TEST(WarpedBakeNamesDoNotReadAsOtherNames) {
    Retail::WarpedBakeRequest request;
    RaceFileRequest race;
    uint32_t id = 0;
    CHECK(!Retail::ParseWarpedBakeName("textures/bakednpctextures/retail-8046191.blp", request));
    CHECK(!Retail::ParseWarpedBakeName("textures/bakednpctextures/retail-8046191_bake1x.blp", request));
    CHECK(!Retail::ParseWarpedBakeName("textures/bakednpctextures/retail-8046191_bakem.blp", request));
    CHECK(!Retail::ParseWarpedBakeName("textures/bakednpctextures/retail-x_bake1m.blp", request));
    CHECK(!ParseRetailPath("textures/bakednpctextures/retail-8046191_bake1m.blp", id));
    CHECK(!ParseRaceFileName("textures/bakednpctextures/retail-8046191_bake1m.blp", race));
}
