#include "Engine/Source/Retail/BodyMatch.h"
#include "Tools/Tests/BodyFixtures.h"
#include "Tools/Tests/Check.h"

using Retail::BodyMesh;

// Installed: three vertices on retail ones, one off them in a skin triangle, one off them under the hair texture
// only. Retail: the three, the second twice (a UV seam).
static std::vector<BodyVertex> InstalledVertices() {
    return { { 0, 0, 0, 0, 0 }, { 1, 0, 0, 1, 0 }, { 0, 1, 0, 0, 1 }, { 5, 5, 5, 1, 1 }, { 9, 9, 9, 0.5f, 0.5f } };
}

static std::vector<BodyVertex> RetailVertices() {
    return { { 0, 0, 0, 0, 0 }, { 1, 0, 0, 0.5f, 0 }, { 0, 1, 0, 0, 1 }, { 1, 0, 0, 0.9f, 0.9f } };
}

TEST(BodyMeshReadsVerticesAndOnlyTheSkinTriangles) {
    std::vector<uint8_t> skin = BodySkin({ { { 0, 1, 2 }, { 1, 2, 3 } }, { { 2, 3, 4 } } });
    BodyMesh mesh;
    std::string error;
    CHECK(Retail::ReadBodyMesh(BodyModel(InstalledVertices(), { 1, 6 }), &skin, mesh, error));
    CHECK(mesh.positions.size() == 5 && mesh.uvs.size() == 5);
    CHECK(mesh.positions.size() == 5 && mesh.positions[3][0] == 5 && mesh.uvs[1][0] == 1);
    CHECK(mesh.skinTriangles.size() == 2);
    CHECK(mesh.skinTriangles.size() == 2 && mesh.skinTriangles[1][2] == 3);
}

TEST(BodyMeshReadsAnMd21ModelWithoutASkin) {
    BodyMesh mesh;
    std::string error;
    CHECK(Retail::ReadBodyMesh(BodyModelMd21(RetailVertices()), nullptr, mesh, error));
    CHECK(mesh.positions.size() == 4 && mesh.skinTriangles.empty());
    CHECK(!Retail::ReadBodyMesh(std::vector<uint8_t>(16, 0), nullptr, mesh, error));
}

TEST(BodyMatchFindsEverySeamCopy) {
    std::vector<uint8_t> skin = BodySkin({ { { 0, 1, 2 }, { 1, 2, 3 } }, { { 2, 3, 4 } } });
    BodyMesh installed;
    BodyMesh retail;
    std::string error;
    CHECK(Retail::ReadBodyMesh(BodyModel(InstalledVertices(), { 1, 6 }), &skin, installed, error));
    CHECK(Retail::ReadBodyMesh(BodyModelMd21(RetailVertices()), nullptr, retail, error));

    Retail::VertexMatches matches = Retail::MatchVertices(installed, retail);
    CHECK(matches.size() == 5);
    CHECK(matches.size() == 5 && matches[0].size() == 1 && matches[1].size() == 2 && matches[3].empty() && matches[4].empty());
}

TEST(BodyMatchRatioCountsTheSkinVerticesOnly) {
    std::vector<uint8_t> skin = BodySkin({ { { 0, 1, 2 }, { 1, 2, 3 } }, { { 2, 3, 4 } } });
    BodyMesh installed;
    BodyMesh retail;
    std::string error;
    CHECK(Retail::ReadBodyMesh(BodyModel(InstalledVertices(), { 1, 6 }), &skin, installed, error));
    CHECK(Retail::ReadBodyMesh(BodyModelMd21(RetailVertices()), nullptr, retail, error));

    CHECK(Retail::BodyMatchRatio(installed, Retail::MatchVertices(installed, retail)) == 0.75);
    BodyMesh bare = installed;
    bare.skinTriangles.clear();
    CHECK(Retail::BodyMatchRatio(bare, Retail::MatchVertices(bare, retail)) == 0.0);
}

TEST(BodyMatchToleratesRoundingWithinTheTolerance) {
    BodyMesh installed;
    installed.positions = { { 1.0f, 2.0f, 3.0f }, { 1.0f, 2.0f, 3.1f } };
    installed.uvs = { { 0, 0 }, { 0, 0 } };
    BodyMesh retail;
    retail.positions = { { 1.000004f, 2.0f, 2.999996f } };
    retail.uvs = { { 0, 0 } };
    Retail::VertexMatches matches = Retail::MatchVertices(installed, retail);
    CHECK(matches[0].size() == 1 && matches[1].empty());
}

TEST(BodyMeshReadsEachVertexsGeoset) {
    std::vector<uint8_t> skin = GeosetSkin({ { 0, 0, 2 }, { 3201, 2, 3 } });
    BodyMesh mesh;
    std::string error;
    CHECK(Retail::ReadBodyMesh(BodyModelMd21(InstalledVertices()), &skin, mesh, error));
    CHECK(mesh.geosets.size() == 5 && mesh.geosets[1] == 0 && mesh.geosets[2] == 3201 && mesh.geosets[4] == 3201);
}

TEST(BodyMeshGeosetsIgnoreTheIndexLevel) {
    std::vector<uint8_t> skin = GeosetSkin({ { 0, 0, 2 }, { 3201, 2, 3 } });
    PutU16(skin, GetU32(skin, SkinAt::SECTIONS + 4) + 0x30 + 0x02, 1);   // The second section's indices start past 65,535.
    BodyMesh mesh;
    std::string error;
    CHECK(Retail::ReadBodyMesh(BodyModelMd21(InstalledVertices()), &skin, mesh, error));
    CHECK(mesh.geosets.size() == 5 && mesh.geosets[2] == 3201 && mesh.geosets[4] == 3201);
}

TEST(LodSkinsFillTheGeosetsViewZeroLeftUnknown) {
    std::vector<uint8_t> view = GeosetSkin({ { 0, 0, 2 } });
    std::vector<uint8_t> lod = GeosetSkin({ { 3201, 0, 5 } });
    std::vector<uint8_t> model = BodyModelMd21(InstalledVertices());
    BodyMesh mesh;
    std::string error;
    CHECK(Retail::ReadBodyMesh(model, &view, mesh, error));
    CHECK(mesh.geosets.size() == 5 && mesh.geosets[1] == 0 && mesh.geosets[2] == 0xFFFF);
    CHECK(Retail::AddSkinGeosets(model, lod, mesh, error));
    CHECK(mesh.geosets.size() == 5 && mesh.geosets[1] == 0 && mesh.geosets[2] == 3201 && mesh.geosets[4] == 3201);
}

// Retail overlays two faces (3201, 3202) on the same three positions with their own UVs, and geoset 0 on one of them.
static BodyMesh OverlaidRetail(std::vector<Retail::Vec2> face2Uvs) {
    BodyMesh retail;
    retail.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 0 } };
    retail.uvs = { { 0.6f, 0.1f }, { 0.7f, 0.1f }, { 0.6f, 0.2f }, face2Uvs[0], face2Uvs[1], { 0.1f, 0.1f } };
    retail.geosets = { 3201, 3201, 3201, 3202, 3202, 0 };
    return retail;
}

TEST(BodyMatchKeepsToTheFaceVariantThePortTook) {
    BodyMesh installed;
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    installed.uvs = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
    Retail::VertexMatches matches = Retail::MatchVertices(installed, OverlaidRetail({ { 0.8f, 0.5f }, { 0.9f, 0.5f } }));
    CHECK(matches[0].size() == 2 && matches[1].size() == 1 && matches[2].size() == 1);
    CHECK(matches[1].size() == 1 && matches[1][0] == 1);
    CHECK(matches[0].size() == 2 && matches[0][0] == 0 && matches[0][1] == 5);
}

TEST(BodyMatchKeepsAnInstalledGeosetsOwnCopies) {
    BodyMesh installed;
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    installed.uvs = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
    installed.geosets = { 0, 3202, 0 };
    Retail::VertexMatches matches = Retail::MatchVertices(installed, OverlaidRetail({ { 0.8f, 0.5f }, { 0.9f, 0.5f } }));
    // Vertex 1 is the installed 3202's own, though the body vertices make 3201 dominant.
    CHECK(matches[1].size() == 1 && matches[1][0] == 4);
    CHECK(matches[0].size() == 2 && matches[0][0] == 0 && matches[0][1] == 5);
}

TEST(BodyMatchPrefersCopiesTheSkinTextureDraws) {
    BodyMesh installed;
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    installed.uvs = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
    BodyMesh retail = OverlaidRetail({ { 0.8f, 0.5f }, { 0.9f, 0.5f } });
    retail.skinned = { 0, 0, 1, 1, 1, 0 };   // Only face 3202 (and vertex 2 of 3201) is skin-textured.
    Retail::VertexMatches matches = Retail::MatchVertices(installed, retail);
    CHECK(matches[0].size() == 1 && matches[0][0] == 3);
    CHECK(matches[1].size() == 1 && matches[1][0] == 4);
}

TEST(BodyMeshMarksTheVerticesSkinBatchesDraw) {
    std::vector<uint8_t> skin = BodySkin({ { { 0, 1, 2 } }, { { 2, 3, 4 } } });
    BodyMesh mesh;
    std::string error;
    CHECK(Retail::ReadBodyMesh(BodyModel(InstalledVertices(), { 1, 6 }), &skin, mesh, error));
    // Both fixture sections span every vertex, so all are drawn by the skin batch's section.
    CHECK(mesh.skinned.size() == 5 && mesh.skinned[4] == 1);
}

TEST(BodyMatchWithoutRetailGeosetsKeepsEveryCandidate) {
    BodyMesh installed;
    installed.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    installed.uvs = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
    BodyMesh retail = OverlaidRetail({ { 0.8f, 0.5f }, { 0.9f, 0.5f } });
    retail.geosets.clear();
    Retail::VertexMatches matches = Retail::MatchVertices(installed, retail);
    CHECK(matches[0].size() == 3 && matches[1].size() == 2);
}

TEST(ShrunkRetailBodiesMatchTheScaledPorts) {
    BodyMesh installed;
    installed.positions = { { 1, 2, 3 } };
    installed.uvs = { { 0, 0 } };
    BodyMesh retail;
    retail.positions = { { 1.35f, 2.7f, 4.05f } };
    retail.uvs = { { 0, 0 } };
    CHECK(Retail::MatchVertices(installed, retail)[0].empty());
    Retail::ShrinkBody(retail, 1.35f);
    CHECK(Retail::MatchVertices(installed, retail)[0].size() == 1);
}

TEST(InstalledBodiesAreTheHdPortRaces) {
    CHECK(Retail::InstalledBodyPath(1, 0) == "Character\\Human\\Male\\HumanMale.m2");
    CHECK(Retail::InstalledBodySkinPath(11, 1) == "Character\\Draenei\\Female\\DraeneiFemale00.skin");
    CHECK(Retail::RetailHdBodyFileDataId(1, 0) == 1011653 && Retail::RetailHdBodyFileDataId(11, 1) == 1022598);
    CHECK(Retail::RetailHdBodyFileDataId(9, 0) == 0 && Retail::InstalledBodyPath(9, 0).empty());
}
