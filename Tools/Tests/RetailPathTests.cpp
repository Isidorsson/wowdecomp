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
