#include "Engine/Source/Retail/BodyMatch.h"
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include "Engine/Source/Retail/RetailModelData.h"

namespace Retail {

constexpr size_t VERTEX_UV = 0x20;
constexpr size_t TEXTURE_TYPE = 0;
constexpr uint32_t SKIN_TEXTURE = 1;
constexpr size_t SECTION_ID = 0x00;
constexpr size_t SECTION_LEVEL = 0x02;
constexpr size_t SECTION_VERTEX_START = 0x04;
constexpr size_t SECTION_VERTEX_COUNT = 0x06;
constexpr size_t SECTION_INDEX_START = 0x08;
constexpr size_t SECTION_INDEX_COUNT = 0x0A;
constexpr size_t BATCH_SECTION = 0x04;
constexpr size_t BATCH_TEXTURE_COMBO = 0x10;
constexpr uint16_t UNKNOWN_GEOSET = 0xFFFF;
constexpr uint16_t GEOSET_GROUP = 100;

// Bodies of races 1 to 11 (Goblin, 9, has none: its installed body is no port), male then female.
static const char* const BODY_NAMES[] = { "Human", "Orc", "Dwarf", "NightElf", "Scourge", "Tauren", "Gnome", "Troll", "", "BloodElf", "Draenei" };
constexpr uint32_t BODY_RACES = sizeof(BODY_NAMES) / sizeof(BODY_NAMES[0]);
// Retail ChrModel 1 to 22's HD models (FACTS.md Q3).
static const uint32_t RETAIL_HD_BODIES[BODY_RACES][2] = {
    { 1011653, 1000764 }, { 917116, 949470 }, { 878772, 950080 }, { 974343, 921844 }, { 959310, 997378 }, { 968705, 986648 },
    { 900914, 940356 }, { 1022938, 1018060 }, { 0, 0 }, { 1100087, 1100258 }, { 1005887, 1022598 },
};

static bool HasBody(uint32_t race, uint32_t sex) {
    return race >= 1 && race <= BODY_RACES && sex <= 1 && RETAIL_HD_BODIES[race - 1][sex];
}

static std::string BodyStem(uint32_t race, uint32_t sex) {
    std::string name = BODY_NAMES[race - 1];
    std::string gender = sex ? "Female" : "Male";
    return "Character\\" + name + "\\" + gender + "\\" + name + gender;
}

std::string InstalledBodyPath(uint32_t race, uint32_t sex) {
    return HasBody(race, sex) ? BodyStem(race, sex) + ".m2" : std::string();
}

std::string InstalledBodySkinPath(uint32_t race, uint32_t sex) {
    return HasBody(race, sex) ? BodyStem(race, sex) + "00.skin" : std::string();
}

std::string InstalledBaseSkinPath(uint32_t race, uint32_t sex) {
    return HasBody(race, sex) ? BodyStem(race, sex) + "Skin00_00.blp" : std::string();
}

uint32_t RetailHdBodyFileDataId(uint32_t race, uint32_t sex) {
    return HasBody(race, sex) ? RETAIL_HD_BODIES[race - 1][sex] : 0;
}

// The MD20 blob of an MD20 file, or of an MD21 file's MD21 chunk.
static bool Md20Of(const std::vector<uint8_t>& model, std::vector<uint8_t>& md20, std::string& error) {
    if (model.size() >= 8 && std::memcmp(model.data(), "MD21", 4) == 0) {
        md20 = ReadChunks(model)["MD21"];
    } else {
        md20 = model;
    }
    if (md20.size() < M2At::HEADER || std::memcmp(md20.data(), "MD20", 4) != 0) {
        return Fail(error, "a body mesh needs an M2 model");
    }
    return true;
}

// The textures each texture combo names, by type.
static bool ComboTypes(const std::vector<uint8_t>& md20, std::vector<uint32_t>& types, std::string& error) {
    Span textures;
    Span combos;
    if (!ReadSpan(md20, M2At::TEXTURES, TEXTURE_SIZE, "textures", textures, error) || !ReadSpan(md20, M2At::TEXTURE_COMBOS, 2, "texture combos", combos, error)) {
        return false;
    }
    types.clear();
    for (uint32_t i = 0; i < combos.count; i++) {
        uint16_t texture = Get<uint16_t>(md20, combos.At(i, 2));
        types.push_back(texture < textures.count ? Get<uint32_t>(md20, textures.At(texture, TEXTURE_SIZE) + TEXTURE_TYPE) : 0);
    }
    return true;
}

// Every section whose batch draws the skin texture first.
static std::set<uint16_t> SkinSections(const std::vector<uint8_t>& skin, const std::vector<uint32_t>& comboTypes, std::string& error) {
    std::set<uint16_t> sections;
    Span batches;
    if (!ReadSpan(skin, SkinAt::BATCHES, BATCH_SIZE, "batches", batches, error)) {
        return sections;
    }
    for (uint32_t b = 0; b < batches.count; b++) {
        size_t at = batches.At(b, BATCH_SIZE);
        uint16_t combo = Get<uint16_t>(skin, at + BATCH_TEXTURE_COMBO);
        if (combo < comboTypes.size() && comboTypes[combo] == SKIN_TEXTURE) {
            sections.insert(Get<uint16_t>(skin, at + BATCH_SECTION));
        }
    }
    return sections;
}

// The model vertex of skin vertex k: retail skins count their lookup from a first vertex (the field 3.3.5 calls
// boneCountMax), so LOD skins can name vertices past 65,535.
struct SkinLookup {
    Span lookup;
    uint32_t first = 0;

    uint32_t Vertex(const std::vector<uint8_t>& skin, size_t k) const { return k < lookup.count ? first + Get<uint16_t>(skin, lookup.At(k, 2)) : UINT32_MAX; }
};

// Each vertex's geoset (the id of the first section whose vertex range holds it), and whether a skin-textured section
// draws it.
static void ReadGeosets(const std::vector<uint8_t>& skin, const SkinLookup& lookup, const Span& sections, const std::set<uint16_t>& skinSections, BodyMesh& mesh) {
    mesh.geosets.resize(mesh.positions.size(), UNKNOWN_GEOSET);
    mesh.skinned.resize(mesh.positions.size(), 0);
    for (uint32_t s = 0; s < sections.count; s++) {
        size_t at = sections.At(s, SECTION_SIZE);
        // Level extends the index start only: lookups stay within 16 bits.
        size_t start = Get<uint16_t>(skin, at + SECTION_VERTEX_START);
        size_t count = Get<uint16_t>(skin, at + SECTION_VERTEX_COUNT);
        for (size_t k = start; k < start + count; k++) {
            uint32_t vertex = lookup.Vertex(skin, k);
            if (vertex >= mesh.geosets.size()) {
                continue;
            }
            if (mesh.geosets[vertex] == UNKNOWN_GEOSET) {
                mesh.geosets[vertex] = Get<uint16_t>(skin, at + SECTION_ID);
            }
            mesh.skinned[vertex] |= skinSections.count(static_cast<uint16_t>(s)) ? 1 : 0;
        }
    }
}

static void ReadTriangles(const std::vector<uint8_t>& skin, const SkinLookup& lookup, const Span& indices, const Span& sections,
    const std::set<uint16_t>& skinSections, BodyMesh& mesh) {
    for (uint16_t s : skinSections) {
        if (s >= sections.count) {
            continue;
        }
        size_t at = sections.At(s, SECTION_SIZE);
        size_t start = Get<uint16_t>(skin, at + SECTION_INDEX_START) + (size_t(Get<uint16_t>(skin, at + SECTION_LEVEL)) << 16);
        size_t count = Get<uint16_t>(skin, at + SECTION_INDEX_COUNT);
        for (size_t i = start; i + 3 <= start + count && i + 3 <= indices.count; i += 3) {
            std::array<uint32_t, 3> triangle;
            bool inside = true;
            for (size_t corner = 0; corner < 3; corner++) {
                triangle[corner] = lookup.Vertex(skin, Get<uint16_t>(skin, indices.At(i + corner, 2)));
                inside = inside && triangle[corner] < mesh.positions.size();
            }
            if (inside) {
                mesh.skinTriangles.push_back(triangle);
            }
        }
    }
}

static bool IsMd21(const std::vector<uint8_t>& model) {
    return model.size() >= 4 && std::memcmp(model.data(), "MD21", 4) == 0;
}

static bool ReadSkin(const std::vector<uint8_t>& md20, const std::vector<uint8_t>& skin, bool retail, bool triangles, BodyMesh& mesh, std::string& error) {
    std::vector<uint32_t> comboTypes;
    SkinLookup lookup;
    Span indices;
    Span sections;
    if (skin.size() < SkinAt::HEADER || !ComboTypes(md20, comboTypes, error) || !ReadSpan(skin, SkinAt::VERTICES, 2, "vertex lookup", lookup.lookup, error)
        || !ReadSpan(skin, SkinAt::INDICES, 2, "indices", indices, error) || !ReadSpan(skin, SkinAt::SECTIONS, SECTION_SIZE, "sections", sections, error)) {
        return error.empty() ? Fail(error, "the body's skin is truncated") : false;
    }
    lookup.first = retail ? Get<uint32_t>(skin, SkinAt::BONE_COUNT_MAX) : 0;
    std::set<uint16_t> skinSections = SkinSections(skin, comboTypes, error);
    ReadGeosets(skin, lookup, sections, skinSections, mesh);
    if (triangles) {
        ReadTriangles(skin, lookup, indices, sections, skinSections, mesh);
    }
    return true;
}

bool AddSkinGeosets(const std::vector<uint8_t>& model, const std::vector<uint8_t>& skin, BodyMesh& mesh, std::string& error) {
    std::vector<uint8_t> md20;
    return Md20Of(model, md20, error) && ReadSkin(md20, skin, IsMd21(model), false, mesh, error);
}

bool ReadBodyMesh(const std::vector<uint8_t>& model, const std::vector<uint8_t>* skin, BodyMesh& mesh, std::string& error) {
    mesh = BodyMesh();
    std::vector<uint8_t> md20;
    Span vertices;
    if (!Md20Of(model, md20, error) || !ReadSpan(md20, M2At::VERTICES, VERTEX_SIZE, "vertices", vertices, error)) {
        return false;
    }
    for (uint32_t v = 0; v < vertices.count; v++) {
        size_t at = vertices.At(v, VERTEX_SIZE);
        mesh.positions.push_back({ Get<float>(md20, at), Get<float>(md20, at + 4), Get<float>(md20, at + 8) });
        mesh.uvs.push_back({ Get<float>(md20, at + VERTEX_UV), Get<float>(md20, at + VERTEX_UV + 4) });
    }
    return !skin || ReadSkin(md20, *skin, IsMd21(model), true, mesh, error);
}

void ShrinkBody(BodyMesh& mesh, float scale) {
    for (Vec3& position : mesh.positions) {
        for (float& axis : position) {
            axis /= scale;
        }
    }
}

namespace {

// Positions on a grid of SAME_POSITION cells; a lookup checks the neighbouring cells too, so points on either side of
// a cell edge still meet.
struct Cell {
    int64_t x, y, z;
    bool operator==(const Cell& other) const { return x == other.x && y == other.y && z == other.z; }
};

struct CellHash {
    size_t operator()(const Cell& c) const { return std::hash<int64_t>()(c.x * 73856093 ^ c.y * 19349663 ^ c.z * 83492791); }
};

Cell CellOf(const Vec3& p) {
    return { static_cast<int64_t>(std::floor(p[0] / SAME_POSITION)), static_cast<int64_t>(std::floor(p[1] / SAME_POSITION)),
        static_cast<int64_t>(std::floor(p[2] / SAME_POSITION)) };
}

bool Near(const Vec3& a, const Vec3& b) {
    return std::fabs(a[0] - b[0]) <= SAME_POSITION && std::fabs(a[1] - b[1]) <= SAME_POSITION && std::fabs(a[2] - b[2]) <= SAME_POSITION;
}

}

// The installed vertex's own geoset, 0 (the body) when its skin gave none.
static uint16_t InstalledGeoset(const BodyMesh& installed, size_t vertex) {
    uint16_t geoset = vertex < installed.geosets.size() ? installed.geosets[vertex] : 0;
    return geoset == UNKNOWN_GEOSET ? 0 : geoset;
}

// The variant of each geoset group the most installed body vertices touch. Group 0 is the body (0) and the hair
// styles, which the skin texture never shows, so it keeps the body.
static std::map<uint16_t, uint16_t> DominantVariants(const VertexMatches& matches, const BodyMesh& installed, const BodyMesh& retail) {
    std::map<uint16_t, size_t> touched;
    for (size_t i = 0; i < matches.size(); i++) {
        if (InstalledGeoset(installed, i)) {
            continue;
        }
        std::set<uint16_t> geosets;
        for (uint32_t r : matches[i]) {
            geosets.insert(retail.geosets[r]);
        }
        for (uint16_t geoset : geosets) {
            touched[geoset]++;
        }
    }
    std::map<uint16_t, uint16_t> dominant = { { 0, 0 } };
    std::map<uint16_t, size_t> best;
    for (const auto& [geoset, count] : touched) {
        uint16_t group = geoset / GEOSET_GROUP;
        if (geoset != UNKNOWN_GEOSET && group && count > best[group]) {
            best[group] = count;
            dominant[group] = geoset;
        }
    }
    return dominant;
}

static std::vector<uint32_t> Kept(const std::vector<uint32_t>& candidates, const BodyMesh& retail, const std::function<bool(uint16_t)>& keep) {
    std::vector<uint32_t> kept;
    for (uint32_t r : candidates) {
        if (keep(retail.geosets[r])) {
            kept.push_back(r);
        }
    }
    return kept;
}

// Copies the skin texture draws come first: beards and scalps overlay the jaw and head with hair-textured meshes.
// Then a vertex of a geoset of its own (ears, eyes) keeps the retail copies of that geoset; a body vertex the dominant
// variants'. A vertex neither rule leaves anything keeps every candidate.
static void KeepVariants(VertexMatches& matches, const BodyMesh& installed, const BodyMesh& retail) {
    if (retail.skinned.size() == retail.positions.size()) {
        for (std::vector<uint32_t>& candidates : matches) {
            std::vector<uint32_t> skinned;
            for (uint32_t r : candidates) {
                if (retail.skinned[r]) {
                    skinned.push_back(r);
                }
            }
            if (!skinned.empty()) {
                candidates = std::move(skinned);
            }
        }
    }
    std::map<uint16_t, uint16_t> dominant = DominantVariants(matches, installed, retail);
    auto isDominant = [&dominant](uint16_t geoset) {
        auto variant = dominant.find(geoset / GEOSET_GROUP);
        return variant != dominant.end() && variant->second == geoset;
    };
    for (size_t i = 0; i < matches.size(); i++) {
        uint16_t own = InstalledGeoset(installed, i);
        std::vector<uint32_t> kept = own ? Kept(matches[i], retail, [own](uint16_t geoset) { return geoset == own; }) : std::vector<uint32_t>();
        if (kept.empty()) {
            kept = Kept(matches[i], retail, isDominant);
        }
        if (!kept.empty()) {
            matches[i] = std::move(kept);
        }
    }
}

VertexMatches MatchVertices(const BodyMesh& installed, const BodyMesh& retail) {
    std::unordered_map<Cell, std::vector<uint32_t>, CellHash> grid;
    for (uint32_t r = 0; r < retail.positions.size(); r++) {
        grid[CellOf(retail.positions[r])].push_back(r);
    }

    VertexMatches matches(installed.positions.size());
    for (size_t i = 0; i < installed.positions.size(); i++) {
        Cell cell = CellOf(installed.positions[i]);
        for (int64_t dx = -1; dx <= 1; dx++) {
            for (int64_t dy = -1; dy <= 1; dy++) {
                for (int64_t dz = -1; dz <= 1; dz++) {
                    auto found = grid.find({ cell.x + dx, cell.y + dy, cell.z + dz });
                    if (found == grid.end()) {
                        continue;
                    }
                    for (uint32_t r : found->second) {
                        if (Near(installed.positions[i], retail.positions[r])) {
                            matches[i].push_back(r);
                        }
                    }
                }
            }
        }
    }
    if (retail.geosets.size() == retail.positions.size()) {
        KeepVariants(matches, installed, retail);
    }
    return matches;
}

double BodyMatchRatio(const BodyMesh& installed, const VertexMatches& matches) {
    std::set<uint32_t> used;
    for (const auto& triangle : installed.skinTriangles) {
        used.insert(triangle.begin(), triangle.end());
    }
    if (used.empty()) {
        return 0.0;
    }
    size_t matched = 0;
    for (uint32_t vertex : used) {
        matched += vertex < matches.size() && !matches[vertex].empty() ? 1 : 0;
    }
    return double(matched) / double(used.size());
}

}
