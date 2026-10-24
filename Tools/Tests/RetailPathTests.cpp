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
