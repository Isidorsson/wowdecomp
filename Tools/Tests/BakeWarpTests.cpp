#include <cmath>
#include "Engine/Source/Retail/BakeBlp.h"
#include "Engine/Source/Retail/BakeWarp.h"
#include "Tools/Tests/Bytes.h"
#include "Tools/Tests/Check.h"

using Retail::BodyMesh;
using Retail::Image;
using Retail::Texel;

// A bake whose every texel says where it is.
static Image Pattern(uint32_t width, uint32_t height) {
    Image image;
    image.width = width;
    image.height = height;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            image.texels.push_back({ static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>((x >> 8) | ((y >> 8) << 4)), 255 });
        }
    }
    return image;
}

// Two triangles over the unit UV square on both bodies; retail u scaled by retailScale.
static void Square(BodyMesh& installed, BodyMesh& retail, float retailScale) {
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } };
    installed.uvs = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
    installed.skinTriangles = { { 0, 1, 2 }, { 1, 3, 2 } };
    retail.positions = installed.positions;
    retail.uvs = { { 0, 0 }, { retailScale, 0 }, { 0, 1 }, { retailScale, 1 } };
}

static bool SameImages(const Image& a, const Image& b) {
    return a.width == b.width && a.height == b.height && a.texels == b.texels;
}

TEST(BakeWarpOfIdenticalUvsCopiesTheBake) {
    BodyMesh installed;
    BodyMesh retail;
    Square(installed, retail, 1.0f);
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, Retail::MatchVertices(installed, retail));
    CHECK(warp.triangles.size() == 2);

    Image bake = Pattern(512, 512);
    CHECK(SameImages(Retail::ApplyBakeWarp(warp, bake), bake));
}

TEST(BakeWarpOfTheBodysDoubledUvTakesTheBakesLeftHalf) {
    BodyMesh installed;
    BodyMesh retail;
    Square(installed, retail, 0.5f);
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, Retail::MatchVertices(installed, retail));

    Image bake = Pattern(1024, 512);
    Image warped = Retail::ApplyBakeWarp(warp, bake);
    CHECK(warped.width == 512 && warped.height == 512);
    bool left = warped.texels.size() == 512 * 512;
    for (uint32_t y = 0; left && y < 512; y += 7) {
        for (uint32_t x = 0; left && x < 512; x += 5) {
            left = warped.At(x, y) == bake.At(x, y);
        }
    }
    CHECK(left);
}

TEST(BakeWarpOutputIs512SquareForAnyBake) {
    BodyMesh installed;
    BodyMesh retail;
    Square(installed, retail, 0.5f);
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, Retail::MatchVertices(installed, retail));
    Image warped = Retail::ApplyBakeWarp(warp, Pattern(512, 256));
    CHECK(warped.width == 512 && warped.height == 512 && warped.texels.size() == 512 * 512);
    // A 512x256 bake at half scale: installed texel (200, 300) samples bake (99.75, 149.75). Texels are BGRA: b holds x.
    CHECK(warped.texels.size() == 512 * 512 && warped.At(200, 300).b == 100 && warped.At(200, 300).g == 150);
}

TEST(BakeWarpMapsAnUnmatchedTriangleThroughItsNearestMatchedNeighbour) {
    BodyMesh installed;
    BodyMesh retail;
    // Matched triangle: installed u 0..0.5 -> retail u 0.5..0.75 (an affine that is not the background's u / 2).
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 7, 7, 7 } };
    installed.uvs = { { 0, 0 }, { 0.5f, 0 }, { 0, 0.5f }, { 0.5f, 0.5f } };
    installed.skinTriangles = { { 0, 1, 2 }, { 1, 3, 2 } };
    retail.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    retail.uvs = { { 0.5f, 0 }, { 0.75f, 0 }, { 0.5f, 0.5f } };

    Retail::VertexMatches matches = Retail::MatchVertices(installed, retail);
    CHECK(matches[3].empty());
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, matches);
    CHECK(warp.triangles.size() == 2);
    // The unmatched corner (0.5, 0.5) lands where the neighbour's affine puts it.
    CHECK(warp.triangles.size() == 2 && warp.triangles[1].retail[1][0] == 0.75f && warp.triangles[1].retail[1][1] == 0.5f);

    Image bake = Pattern(1024, 512);
    Image warped = Retail::ApplyBakeWarp(warp, bake);
    // Installed texel (200, 200) is in the unmatched triangle: retail u = 0.5 + 200.5 / 512 / 2.
    CHECK(warped.At(200, 200) == bake.At(512 + 200, 200));
}

TEST(BakeWarpPrefersTheMatchedNeighbourOnTheSameMeshIsland) {
    BodyMesh installed;
    BodyMesh retail;
    // Island A: a matched triangle (0, 1, 2) mapped u / 2, and an unmatched one (1, 4, 2) sharing its edge.
    // Island B: a matched triangle (5, 6, 3) whose UV centre is nearer the unmatched one's, mapped into the right half.
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 6, 1, 0 }, { 9, 9, 9 }, { 5, 1, 0 }, { 6, 0, 0 } };
    installed.uvs = { { 0, 0 }, { 0.2f, 0 }, { 0, 0.2f }, { 0.2f, 0.3f }, { 0.3f, 0.3f }, { 0.2f, 0.2f }, { 0.3f, 0.2f } };
    installed.skinTriangles = { { 0, 1, 2 }, { 1, 4, 2 }, { 5, 6, 3 } };
    retail.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 5, 1, 0 }, { 6, 0, 0 }, { 6, 1, 0 } };
    retail.uvs = { { 0, 0 }, { 0.1f, 0 }, { 0, 0.2f }, { 0.9f, 0.35f }, { 0.95f, 0.35f }, { 0.9f, 0.45f } };

    Retail::VertexMatches matches = Retail::MatchVertices(installed, retail);
    CHECK(matches[4].empty() && !matches[3].empty());
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, matches);
    CHECK(warp.triangles.size() == 3);
    // Vertex 4 (0.3, 0.3) through island A's affine: (0.15, 0.3), not into the right half.
    CHECK(warp.triangles.size() == 3 && std::fabs(warp.triangles[1].retail[1][0] - 0.15f) < 1e-5f && std::fabs(warp.triangles[1].retail[1][1] - 0.3f) < 1e-5f);
}

TEST(BakeWarpSkipsANeighbourWhoseAffineCarriesTheTriangleOffTheBake) {
    BodyMesh installed;
    BodyMesh retail;
    // An unmatched triangle (0, 1, 2) between two matched ones: the nearer (0, 3, 1) is stretched six times in v
    // (NE male's forehead under the scalp), the farther (1, 4, 5) is mapped as is. Vertex 1 has a seam copy for each.
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 9, 9, 9 }, { 0, -1, 0 }, { 2, 0, 0 }, { 2, 1, 0 } };
    installed.uvs = { { 0.8f, 0.1f }, { 0.9f, 0.1f }, { 0.85f, 0.2f }, { 0.85f, 0.02f }, { 0.98f, 0.1f }, { 0.94f, 0.18f } };
    installed.skinTriangles = { { 0, 1, 2 }, { 0, 3, 1 }, { 1, 4, 5 } };
    retail.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 2, 0, 0 }, { 2, 1, 0 } };
    retail.uvs = { { 0.8f, 0.48f }, { 0.9f, 0.48f }, { 0.9f, 0.1f }, { 0.85f, 0.0f }, { 0.98f, 0.1f }, { 0.94f, 0.18f } };

    Retail::VertexMatches matches = Retail::MatchVertices(installed, retail);
    CHECK(matches[2].empty());
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, matches);
    // Through the nearer neighbour vertex 2 would land at v 1.08; through the farther one at (0.85, 0.2).
    CHECK(warp.triangles.size() == 3 && std::fabs(warp.triangles[0].retail[2][0] - 0.85f) < 1e-5f && std::fabs(warp.triangles[0].retail[2][1] - 0.2f) < 1e-5f);
}

TEST(BakeWarpTakesTheInstalledSkinWhereRetailDrawsNoBake) {
    BodyMesh installed;
    BodyMesh retail;
    Square(installed, retail, 0.5f);
    // Retail draws these vertices with another texture (the HD eyes), so the bake holds nothing for them.
    retail.skinned = { 0, 0, 0, 0 };
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, Retail::MatchVertices(installed, retail));
    CHECK(warp.triangles.size() == 2 && warp.triangles[0].fromBase && warp.triangles[1].fromBase);

    Image base = Pattern(512, 512);
    for (Texel& texel : base.texels) {
        texel.a = 7;
    }
    Image warped = Retail::ApplyBakeWarp(warp, Pattern(1024, 512), &base);
    CHECK(warped.At(300, 100) == base.At(300, 100));
    // Without the installed skin, the background stays.
    CHECK(Retail::ApplyBakeWarp(warp, Pattern(1024, 512)).At(300, 100).a == 255);
}

TEST(PlayerBodyRegionsKeepTheFaceInTheRetailRightHalf) {
    CHECK(Retail::PlayerBodyRegions({ 0.2f, 0.8f }, { 0.7f, 0.4f }));
    CHECK(!Retail::PlayerBodyRegions({ 0.2f, 0.8f }, { 0.3f, 0.4f }));
    CHECK(Retail::PlayerBodyRegions({ 0.7f, 0.8f }, { 0.35f, 0.8f }));
    CHECK(!Retail::PlayerBodyRegions({ 0.2f, 0.3f }, { 0.6f, 0.3f }));
}

TEST(BakeWarpWithRegionsSkipsACopyInTheWrongRegion) {
    BodyMesh installed;
    BodyMesh retail;
    // A face triangle whose least distorted copy lies in the body half, and a more distorted one in the face half.
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    installed.uvs = { { 0.1f, 0.7f }, { 0.2f, 0.7f }, { 0.1f, 0.8f } };
    installed.skinTriangles = { { 0, 1, 2 } };
    retail.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    retail.uvs = { { 0.05f, 0.7f }, { 0.10f, 0.7f }, { 0.05f, 0.8f }, { 0.6f, 0.4f }, { 0.75f, 0.4f }, { 0.6f, 0.45f } };
    Retail::VertexMatches matches = Retail::MatchVertices(installed, retail);

    Retail::BakeWarp loose = Retail::BuildBakeWarp(installed, retail, matches);
    CHECK(loose.triangles.size() == 1 && loose.triangles[0].retail[0][0] == 0.05f);
    Retail::BakeWarp regioned = Retail::BuildBakeWarp(installed, retail, matches, Retail::PlayerBodyRegions);
    CHECK(regioned.triangles.size() == 1 && regioned.triangles[0].retail[0][0] == 0.6f);
}

TEST(BakeWarpPicksTheSeamCopyOnTheSameIsland) {
    BodyMesh installed;
    BodyMesh retail;
    Square(installed, retail, 0.5f);
    // A second retail copy of vertex 1 far away on another island.
    retail.positions.push_back({ 1, 0, 0 });
    retail.uvs.push_back({ 0.95f, 0.9f });
    std::swap(retail.positions[1], retail.positions[4]);
    std::swap(retail.uvs[1], retail.uvs[4]);
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, Retail::MatchVertices(installed, retail));
    CHECK(warp.triangles.size() == 2 && warp.triangles[0].retail[1][0] == 0.5f);
    CHECK(warp.triangles.size() == 2 && warp.triangles[1].retail[0][0] == 0.5f);
}

TEST(BakeWarpFindsTheSeamCopyBehindRepeatsOfAnotherIsland) {
    BodyMesh installed;
    BodyMesh square;
    Square(installed, square, 0.5f);
    // Retail repeats an overlay's vertex once per LOD (NE male geoset 1902 over the face, 7 copies), all ahead of
    // the one copy on the vertex's own island.
    BodyMesh retail;
    for (int copy = 0; copy < 7; copy++) {
        retail.positions.push_back({ 1, 0, 0 });
        retail.uvs.push_back({ 0.95f, 0.9f });
    }
    retail.positions.insert(retail.positions.end(), square.positions.begin(), square.positions.end());
    retail.uvs.insert(retail.uvs.end(), square.uvs.begin(), square.uvs.end());
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, Retail::MatchVertices(installed, retail));
    CHECK(warp.triangles.size() == 2 && warp.triangles[0].retail[1][0] == 0.5f && warp.triangles[0].retail[1][1] == 0.0f);
    CHECK(warp.triangles.size() == 2 && warp.triangles[1].retail[0][0] == 0.5f && warp.triangles[1].retail[0][1] == 0.0f);
}

TEST(BakeWarpRefusesACopyCombinationThatBlowsTheTriangleUp) {
    BodyMesh installed;
    BodyMesh retail;
    // Each corner has its own copy (u halved: distortion 2) and one on other islands whose triangle has the same
    // shape at five times the size (distortion 1): Draenei female's hand-to-face seam.
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    installed.uvs = { { 0.1f, 0.1f }, { 0.2f, 0.1f }, { 0.1f, 0.2f } };
    installed.skinTriangles = { { 0, 1, 2 } };
    retail.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    retail.uvs = { { 0.0f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.5f }, { 0.05f, 0.1f }, { 0.1f, 0.1f }, { 0.05f, 0.2f } };
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, Retail::MatchVertices(installed, retail));
    CHECK(warp.triangles.size() == 1 && warp.triangles[0].retail[1][0] == 0.1f && warp.triangles[0].retail[2][1] == 0.2f);
}

TEST(BakeWarpSkipsDegenerateTriangles) {
    BodyMesh installed;
    BodyMesh retail;
    Square(installed, retail, 1.0f);
    installed.uvs[2] = installed.uvs[0];
    Retail::BakeWarp warp = Retail::BuildBakeWarp(installed, retail, Retail::MatchVertices(installed, retail));
    CHECK(warp.triangles.size() == 1);
}

// --- BLP ----------------------------------------------------------------------------------------------------

TEST(RawBlpRoundTripsWithEveryMip) {
    Image image = Pattern(8, 4);
    std::vector<uint8_t> blp = Retail::EncodeRawBlp(image);
    CHECK(blp.size() > 1172);
    CHECK(GetU32(blp, 0) == 0x32504C42 && blp[8] == 3 && blp[9] == 8 && blp[11] == 1);
    CHECK(GetU32(blp, 12) == 8 && GetU32(blp, 16) == 4);
    CHECK(GetU32(blp, 0x54 + 3 * 4) == 4);   // Mip 3, 1x1.
    CHECK(GetU32(blp, 0x54 + 4 * 4) == 0);

    Image decoded;
    std::string error;
    CHECK(Retail::DecodeBakeBlp(blp, decoded, error));
    CHECK(SameImages(decoded, image));
}

static std::vector<uint8_t> DxtBlp(uint8_t alphaType, const std::vector<uint8_t>& block) {
    std::vector<uint8_t> blp(1172, 0);
    PutU32(blp, 0, 0x32504C42);
    PutU32(blp, 4, 1);
    blp[8] = 2;
    blp[9] = alphaType == 7 ? 8 : 0;
    blp[10] = alphaType;
    PutU32(blp, 12, 4);
    PutU32(blp, 16, 4);
    PutU32(blp, 0x14, 1172);
    PutU32(blp, 0x54, static_cast<uint32_t>(block.size()));
    blp.insert(blp.end(), block.begin(), block.end());
    return blp;
}

TEST(Dxt1BlpDecodes) {
    // Colour 0 pure red (0xF800), colour 1 pure blue (0x001F); texels: row 0 all 0, row 1 all 1, row 2 all 2 (2/3 red),
    // row 3 all 3 (1/3 red).
    std::vector<uint8_t> block = { 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x55, 0xAA, 0xFF };
    Image image;
    std::string error;
    CHECK(Retail::DecodeBakeBlp(DxtBlp(0, block), image, error));
    CHECK(image.width == 4 && image.height == 4 && image.texels.size() == 16);
    if (image.texels.size() != 16) {
        return;
    }
    CHECK(image.At(0, 0).r == 255 && image.At(0, 0).b == 0 && image.At(0, 0).a == 255);
    CHECK(image.At(3, 1).r == 0 && image.At(3, 1).b == 255);
    CHECK(image.At(1, 2).r == 170 && image.At(1, 2).b == 85);
    CHECK(image.At(2, 3).r == 85 && image.At(2, 3).b == 170);
}

TEST(PalettizedBlpDecodesWithItsAlphaPlane) {
    std::vector<uint8_t> blp(1172, 0);
    PutU32(blp, 0, 0x32504C42);
    PutU32(blp, 4, 1);
    blp[8] = 1;
    blp[9] = 8;
    PutU32(blp, 12, 2);
    PutU32(blp, 16, 1);
    PutU32(blp, 0x14, 1172);
    PutU32(blp, 0x54, 4);
    PutU32(blp, 0x94 + 5 * 4, 0x00332211);   // Palette entry 5: b 0x11, g 0x22, r 0x33.
    blp.insert(blp.end(), { 5, 0, 200, 9 });   // Indices, then the 8-bit alpha plane.
    Image image;
    std::string error;
    CHECK(Retail::DecodeBakeBlp(blp, image, error));
    CHECK(image.texels.size() == 2 && image.At(0, 0) == Texel({ 0x11, 0x22, 0x33, 200 }) && image.At(1, 0).a == 9);

    blp[9] = 0;
    CHECK(Retail::DecodeBakeBlp(blp, image, error));
    CHECK(image.texels.size() == 2 && image.At(0, 0).a == 255);
}

TEST(BakeBlpRefusesJpegAndDamagedFiles) {
    Image image;
    std::string error;
    std::vector<uint8_t> blp = DxtBlp(0, { 0, 0, 0, 0, 0, 0, 0, 0 });
    blp[8] = 0;
    CHECK(!Retail::DecodeBakeBlp(blp, image, error));
    CHECK(!Retail::DecodeBakeBlp(std::vector<uint8_t>(20, 0), image, error));
    std::vector<uint8_t> truncated = DxtBlp(0, { 0, 0, 0, 0 });
    CHECK(!Retail::DecodeBakeBlp(truncated, image, error));
}
