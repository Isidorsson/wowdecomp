#include <cstring>
#include "Engine/Source/Model2/M2Limits.h"
#include "Tools/Tests/Check.h"

TEST(LegacyEffectsMatchTheOriginalSwitch) {
    const M2LegacyEffect* opaque = M2FindLegacyEffect(1);
    CHECK(opaque && !std::strcmp(opaque->vertexShader, "Diffuse_T1_Env"));
    CHECK(opaque && !std::strcmp(opaque->pixelShader, "Combiners_Opaque_Mod2xNA_Alpha"));
    CHECK(opaque && opaque->colorOp == GxTexOp_Mod && opaque->alphaOp == GxTexOp_PassThru);

    const M2LegacyEffect* add = M2FindLegacyEffect(2);
    CHECK(add && !std::strcmp(add->pixelShader, "Combiners_Opaque_AddAlpha"));

    const M2LegacyEffect* addAlpha = M2FindLegacyEffect(3);
    CHECK(addAlpha && !std::strcmp(addAlpha->pixelShader, "Combiners_Opaque_AddAlpha_Alpha"));
}

TEST(UnknownLegacyEffectsHaveNoShaders) {
    CHECK(M2FindLegacyEffect(0) == nullptr);
    CHECK(M2FindLegacyEffect(4) == nullptr);
    CHECK(M2FindLegacyEffect(0x7FFF) == nullptr);
}

TEST(SkinSectionsWithinTheBoneWindowFit) {
    CHECK(M2SkinSectionFits(1, 0, 1, M2_MAX_SECTION_BONES));
    CHECK(M2SkinSectionFits(M2_MAX_SECTION_BONES, 0, M2_MAX_SECTION_BONES, M2_MAX_SECTION_BONES));
    CHECK(M2SkinSectionFits(10, 90, 100, M2_MAX_SECTION_BONES));
}

TEST(SkinSectionsOutsideTheBoneWindowAreRefused) {
    CHECK(!M2SkinSectionFits(M2_MAX_SECTION_BONES + 1, 0, 200, M2_MAX_SECTION_BONES));
}

TEST(CpuSkinnedSectionsMayExceedTheShaderWindow) {
    CHECK(M2SkinSectionFits(200, 0, 256, 256));
}

TEST(BrokenSkinSectionsAreRefusedOnEveryPath) {
    CHECK(!M2SkinSectionFits(0, 0, 10, 256));
    CHECK(!M2SkinSectionFits(10, 91, 100, 256));
    CHECK(!M2SkinSectionFits(1, 0xFFFFFFFF, 10, 256));
}

TEST(BoneWindowMatchesTheVertexShaderRegisters) {
    CHECK(M2_MAX_SECTION_BONES == (256 - 31) / 3);
}
