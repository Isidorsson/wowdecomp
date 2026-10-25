#ifndef ENGINE_SOURCE_RETAIL_BODYMATCH_H
#define ENGINE_SOURCE_RETAIL_BODYMATCH_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Retail {

// The installed player bodies are decimated HD ports: most of their vertices sit exactly on a vertex of the retail HD
// body, in the same frame, with the UV re-laid into the classic square (FACTS.md Q3). Matching the vertices tells
// where each installed texel's retail bake texel is.

using Vec2 = std::array<float, 2>;
using Vec3 = std::array<float, 3>;

// A body's vertices (position and first UV), the geoset each belongs to, and the triangles its skin texture (type 1)
// is drawn on.
struct BodyMesh {
    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<uint16_t> geosets;   // By vertex, from the skin's submesh ids; empty without a skin.
    std::vector<uint8_t> skinned;    // By vertex, 1 when a batch with the skin texture draws it; empty without a skin.
    std::vector<std::array<uint32_t, 3>> skinTriangles;
};

// The vertices of an M2 (MD20, or MD21 with its MD20 chunk), and with a .skin of it each vertex's geoset and the
// triangles of the batches whose first texture is the skin. Without a skin, no geosets or triangles.
bool ReadBodyMesh(const std::vector<uint8_t>& model, const std::vector<uint8_t>* skin, BodyMesh& mesh, std::string& error);

// The geosets another skin of the model (a retail LOD skin) gives the vertices no earlier skin placed: the HD ports
// were made from LOD meshes, whose vertices view 0 does not name.
bool AddSkinGeosets(const std::vector<uint8_t>& model, const std::vector<uint8_t>& skin, BodyMesh& mesh, std::string& error);

// Every position divided by scale: the Tauren and Gnome ports are the retail HD body made smaller (by HelmFitFor's
// scale, 1.35 / 1.25 and 1.15), the others are retail's size.
void ShrinkBody(BodyMesh& mesh, float scale);

// Positions this close are the same vertex: the HD ports copy retail's to the last digit.
constexpr float SAME_POSITION = 1e-5f;
// The share of skin vertices that must match for an installed body to count as the HD port of a retail body. The 20
// installed ports measure 67% (Blood Elf male) to 99.7% (Troll); their pre-HD retail namesakes 0 to 1%. The ports
// re-shaped parts of some bodies (faces, hands), whose vertices map through their matched neighbours.
constexpr double BODY_MATCH_THRESHOLD = 0.6;

// For each installed vertex, the retail vertices at its position (several at a UV seam), none for an unmatched one.
using VertexMatches = std::vector<std::vector<uint32_t>>;

// With retail geosets known, the HD body's overlaid alternatives (faces, jaws, ears, scalps at the same positions with
// their own UVs) are narrowed to the one the port took: copies a skin-textured batch draws first (beards and scalps
// are hair-textured), then a vertex of an installed geoset other than the body keeps the
// retail copies of that geoset; a body vertex keeps geoset 0 and, of every other group (id / 100), the variant the
// most installed body vertices touch. A vertex no kept candidate fits keeps them all.
VertexMatches MatchVertices(const BodyMesh& installed, const BodyMesh& retail);

// The share of the installed vertices the skin triangles use that have a match: 0 without skin triangles.
double BodyMatchRatio(const BodyMesh& installed, const VertexMatches& matches);

// The installed and retail HD bodies of a race (1 to 8, 10, 11) and sex: the client's path, the retail FileDataID.
std::string InstalledBodyPath(uint32_t race, uint32_t sex);
std::string InstalledBodySkinPath(uint32_t race, uint32_t sex);
// The installed base skin (CharSections skin 0), whose eyes the warp keeps.
std::string InstalledBaseSkinPath(uint32_t race, uint32_t sex);
uint32_t RetailHdBodyFileDataId(uint32_t race, uint32_t sex);
// The race (1 to 8, 10, 11) whose installed body of that sex is a port of the retail model, 0 for none: Mag'har
// NPCs on OrcMale_HD are orcs to the installed client.
uint32_t HdPortRace(uint32_t fileDataId, uint32_t sex);

}

#endif
