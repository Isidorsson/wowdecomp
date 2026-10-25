#include "Engine/Source/Retail/BakeWarp.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>

namespace Retail {

// Installed UV triangles smaller than this cover no texel.
constexpr float DEGENERATE_AREA = 1e-12f;
// Different seam UVs tried per vertex; real seams have two or three.
constexpr size_t MAX_CANDIDATES = 4;
constexpr float FLIPPED = 1e9f;
// How much a matched triangle's retail UVs may grow or shrink it (the square root of the area ratio). The 20 bodies'
// real matches measure 0.27 to 2.7 (0.707 on the body, about 1.2 on faces); a combination of copies from different
// islands spans the bake.
constexpr float MIN_UV_SCALE = 0.2f;
constexpr float MAX_UV_SCALE = 3.0f;
// Barycentric slack, so texels on a shared edge belong to both triangles.
constexpr float EDGE_SLACK = 1e-6f;
// The body's measured relation where no triangle lies: retail u = installed u * BODY_U_SCALE.
constexpr float BODY_U_SCALE = 0.5f;
constexpr int NEIGHBOUR_GRID = 64;
// The face rects of the installed square (CharacterComponent s_sectionInfoRaw FaceUpper from v 320 of 512) and the
// face half of the retail HD layouts.
constexpr float INSTALLED_FACE_U = 0.5f;
constexpr float INSTALLED_FACE_V = 0.625f;
constexpr float RETAIL_FACE_U = 0.5f;
// Triangles a mesh-neighbour search looks through before falling back to the nearest by UV.
constexpr size_t MESH_SEARCH = 256;

namespace {

// to = M * from + t.
struct Affine {
    float a, b, c, d, e, f;

    Vec2 Apply(const Vec2& p) const { return { a * p[0] + b * p[1] + c, d * p[0] + e * p[1] + f }; }
};

float Cross(const Vec2& o, const Vec2& p, const Vec2& q) {
    return (p[0] - o[0]) * (q[1] - o[1]) - (p[1] - o[1]) * (q[0] - o[0]);
}

bool AffineOf(const std::array<Vec2, 3>& from, const std::array<Vec2, 3>& to, Affine& affine) {
    float det = Cross(from[0], from[1], from[2]);
    if (std::fabs(det) < DEGENERATE_AREA) {
        return false;
    }
    // Edges e1 = p1 - p0, e2 = p2 - p0; M = [r1 r2] * inverse([e1 e2]).
    float e1u = from[1][0] - from[0][0], e1v = from[1][1] - from[0][1];
    float e2u = from[2][0] - from[0][0], e2v = from[2][1] - from[0][1];
    float r1u = to[1][0] - to[0][0], r1v = to[1][1] - to[0][1];
    float r2u = to[2][0] - to[0][0], r2v = to[2][1] - to[0][1];
    affine.a = (r1u * e2v - r2u * e1v) / det;
    affine.b = (r2u * e1u - r1u * e2u) / det;
    affine.d = (r1v * e2v - r2v * e1v) / det;
    affine.e = (r2v * e1u - r1v * e2u) / det;
    affine.c = to[0][0] - affine.a * from[0][0] - affine.b * from[0][1];
    affine.f = to[0][1] - affine.d * from[0][0] - affine.e * from[0][1];
    return true;
}

// The ratio of the affine's singular values; a flip, a collapse or a size change past the UV scale limits counts as
// the worst.
float Distortion(const Affine& m) {
    float det = m.a * m.e - m.b * m.d;
    if (det <= 0 || std::sqrt(det) < MIN_UV_SCALE || std::sqrt(det) > MAX_UV_SCALE) {
        return FLIPPED;
    }
    float sum = m.a * m.a + m.b * m.b + m.d * m.d + m.e * m.e;
    float root = std::sqrt(std::max(0.0f, sum * sum - 4 * det * det));
    float smax = std::sqrt((sum + root) / 2);
    float smin = std::sqrt(std::max(0.0f, (sum - root) / 2));
    return smin > 0 ? smax / smin : FLIPPED;
}

Vec2 Centre(const std::array<Vec2, 3>& p) {
    return { (p[0][0] + p[1][0] + p[2][0]) / 3, (p[0][1] + p[1][1] + p[2][1]) / 3 };
}

// The matched triangles' centres and affines on a coarse grid of installed UV space.
class NeighbourGrid {
    public:
    void Add(const Vec2& centre, const Affine& affine) {
        this->m_cells[CellOf(centre)].push_back(this->m_entries.size());
        this->m_entries.push_back({ centre, affine });
    }

    // The affine of the matched triangle nearest to centre, searched ring by ring.
    std::optional<Affine> Nearest(const Vec2& centre) const {
        if (this->m_entries.empty()) {
            return std::nullopt;
        }
        int cx = CellX(centre[0]), cy = CellX(centre[1]);
        float best = std::numeric_limits<float>::max();
        const Affine* found = nullptr;
        for (int ring = 0; ring <= NEIGHBOUR_GRID * 2; ring++) {
            if (found && (ring - 1) * (1.0f / NEIGHBOUR_GRID) > std::sqrt(best)) {
                break;
            }
            this->ScanRing(cx, cy, ring, centre, best, found);
        }
        return *found;
    }

    private:
    struct Entry {
        Vec2 centre;
        Affine affine;
    };

    static int CellX(float u) { return std::clamp(static_cast<int>(std::floor(u * NEIGHBOUR_GRID)), -NEIGHBOUR_GRID, 2 * NEIGHBOUR_GRID); }
    static int CellOf(const Vec2& p) { return (CellX(p[1]) + NEIGHBOUR_GRID) * (4 * NEIGHBOUR_GRID) + CellX(p[0]) + NEIGHBOUR_GRID; }

    void ScanRing(int cx, int cy, int ring, const Vec2& centre, float& best, const Affine*& found) const {
        for (int y = cy - ring; y <= cy + ring; y++) {
            for (int x = cx - ring; x <= cx + ring; x++) {
                if (std::max(std::abs(x - cx), std::abs(y - cy)) != ring) {
                    continue;
                }
                auto cell = this->m_cells.find((y + NEIGHBOUR_GRID) * (4 * NEIGHBOUR_GRID) + x + NEIGHBOUR_GRID);
                if (cell == this->m_cells.end()) {
                    continue;
                }
                for (size_t i : cell->second) {
                    const Entry& entry = this->m_entries[i];
                    float du = entry.centre[0] - centre[0], dv = entry.centre[1] - centre[1];
                    if (du * du + dv * dv < best) {
                        best = du * du + dv * dv;
                        found = &entry.affine;
                    }
                }
            }
        }
    }

    std::vector<Entry> m_entries;
    std::map<int, std::vector<size_t>> m_cells;
};

// Whether retail draws every candidate of every corner with another texture than the skin, so the bake has no texels
// for the triangle. Unknown without the retail skins.
bool DrawnWithoutBake(const std::array<const std::vector<uint32_t>*, 3>& candidates, const BodyMesh& retail) {
    if (retail.skinned.size() != retail.positions.size()) {
        return false;
    }
    for (const std::vector<uint32_t>* corner : candidates) {
        for (uint32_t r : *corner) {
            if (retail.skinned[r]) {
                return false;
            }
        }
    }
    return true;
}

// The candidates' different UVs, up to MAX_CANDIDATES: retail repeats an overlay's vertex once per LOD, and those
// repeats must not crowd out the copy on the vertex's own island.
std::vector<Vec2> DistinctUvs(const std::vector<uint32_t>& candidates, const BodyMesh& retail) {
    std::vector<Vec2> uvs;
    for (uint32_t r : candidates) {
        if (uvs.size() == MAX_CANDIDATES) {
            break;
        }
        if (std::find(uvs.begin(), uvs.end(), retail.uvs[r]) == uvs.end()) {
            uvs.push_back(retail.uvs[r]);
        }
    }
    return uvs;
}

// The retail UVs of the least distorted combination of the corners' candidates.
bool BestRetailUvs(const std::array<Vec2, 3>& installed, const std::array<const std::vector<uint32_t>*, 3>& candidates, const BodyMesh& retail,
    const RegionCheck& regions, std::array<Vec2, 3>& best, Affine& bestAffine) {
    float lowest = std::numeric_limits<float>::max();
    std::array<std::vector<Vec2>, 3> corners;
    for (size_t c = 0; c < 3; c++) {
        corners[c] = DistinctUvs(*candidates[c], retail);
    }
    for (const Vec2& first : corners[0]) {
        for (const Vec2& second : corners[1]) {
            for (const Vec2& third : corners[2]) {
                std::array<Vec2, 3> uvs = { first, second, third };
                Affine affine;
                bool inRegion = !regions || regions(Centre(installed), Centre(uvs));
                float distortion = inRegion && AffineOf(installed, uvs, affine) ? Distortion(affine) : FLIPPED;
                if (distortion < lowest) {
                    lowest = distortion;
                    best = uvs;
                    bestAffine = affine;
                }
            }
        }
    }
    return lowest < FLIPPED;
}

}

namespace {

// A skin triangle on its way into the warp: its UVs and, once matched, the affine it maps through.
struct Placed {
    UvTriangle triangle;
    std::array<uint32_t, 3> corners;
    std::optional<Affine> affine;
};

// Whether an affine lands a triangle on the bake and in its own region. A neighbour across an unmatched seam (the
// forehead under the scalp) can stretch it off the bake, whose clamped edge texels then streak.
bool LandsInRegion(const Affine& affine, const std::array<Vec2, 3>& installed, const RegionCheck& regions) {
    for (const Vec2& corner : installed) {
        Vec2 uv = affine.Apply(corner);
        if (uv[0] < -EDGE_SLACK || uv[0] > 1 + EDGE_SLACK || uv[1] < -EDGE_SLACK || uv[1] > 1 + EDGE_SLACK) {
            return false;
        }
    }
    Vec2 centre = Centre(installed);
    return !regions || regions(centre, affine.Apply(centre));
}

// The affine of the matched triangle fewest shared-vertex steps away (the nearest by UV centre among equals), so a
// triangle keeps to its own UV island; none within MESH_SEARCH triangles.
std::optional<Affine> MeshNeighbourAffine(const std::vector<Placed>& placed, const std::vector<std::vector<size_t>>& trianglesOf, size_t start,
    const RegionCheck& regions) {
    std::vector<size_t> ring = { start };
    std::vector<bool> seen(placed.size(), false);
    seen[start] = true;
    Vec2 centre = Centre(placed[start].triangle.installed);
    for (size_t visited = 0; !ring.empty() && visited < MESH_SEARCH;) {
        std::vector<size_t> next;
        const Affine* best = nullptr;
        float bestDistance = std::numeric_limits<float>::max();
        for (size_t index : ring) {
            for (uint32_t corner : placed[index].corners) {
                for (size_t neighbour : trianglesOf[corner]) {
                    if (seen[neighbour]) {
                        continue;
                    }
                    seen[neighbour] = true;
                    visited++;
                    next.push_back(neighbour);
                    if (!placed[neighbour].affine || !LandsInRegion(*placed[neighbour].affine, placed[start].triangle.installed, regions)) {
                        continue;
                    }
                    Vec2 other = Centre(placed[neighbour].triangle.installed);
                    float distance = (other[0] - centre[0]) * (other[0] - centre[0]) + (other[1] - centre[1]) * (other[1] - centre[1]);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        best = &*placed[neighbour].affine;
                    }
                }
            }
        }
        if (best) {
            return *best;
        }
        ring = std::move(next);
    }
    return std::nullopt;
}

}

BakeWarp BuildBakeWarp(const BodyMesh& installed, const BodyMesh& retail, const VertexMatches& matches, const RegionCheck& regions) {
    std::vector<Placed> placed;
    NeighbourGrid grid;
    for (const auto& corners : installed.skinTriangles) {
        Placed entry;
        entry.corners = corners;
        bool matched = true;
        std::array<const std::vector<uint32_t>*, 3> candidates;
        for (size_t c = 0; c < 3; c++) {
            entry.triangle.installed[c] = installed.uvs[corners[c]];
            candidates[c] = &matches[corners[c]];
            matched = matched && !candidates[c]->empty();
        }
        const auto& uv = entry.triangle.installed;
        if (std::fabs(Cross(uv[0], uv[1], uv[2])) < DEGENERATE_AREA) {
            continue;
        }
        Affine affine;
        if (matched && DrawnWithoutBake(candidates, retail)) {
            entry.triangle.fromBase = true;
            entry.triangle.retail = uv;
        } else if (matched && BestRetailUvs(uv, candidates, retail, regions, entry.triangle.retail, affine)) {
            entry.affine = affine;
            grid.Add(Centre(uv), affine);
        }
        placed.push_back(entry);
    }

    std::vector<std::vector<size_t>> trianglesOf(installed.positions.size());
    for (size_t i = 0; i < placed.size(); i++) {
        for (uint32_t corner : placed[i].corners) {
            trianglesOf[corner].push_back(i);
        }
    }

    BakeWarp warp;
    for (size_t i = 0; i < placed.size(); i++) {
        UvTriangle triangle = placed[i].triangle;
        if (!placed[i].affine && !triangle.fromBase) {
            std::optional<Affine> affine = MeshNeighbourAffine(placed, trianglesOf, i, regions);
            affine = affine ? affine : grid.Nearest(Centre(triangle.installed));
            if (!affine || !LandsInRegion(*affine, triangle.installed, regions)) {
                continue;
            }
            for (size_t c = 0; c < 3; c++) {
                triangle.retail[c] = affine->Apply(triangle.installed[c]);
            }
        }
        warp.triangles.push_back(triangle);
    }
    return warp;
}

bool PlayerBodyRegions(const Vec2& installedCentre, const Vec2& retailCentre) {
    bool installedFace = installedCentre[0] < INSTALLED_FACE_U && installedCentre[1] >= INSTALLED_FACE_V;
    return installedFace == (retailCentre[0] >= RETAIL_FACE_U);
}

static uint8_t Mix(uint8_t p00, uint8_t p10, uint8_t p01, uint8_t p11, float fx, float fy) {
    float top = p00 + (p10 - p00) * fx;
    float bottom = p01 + (p11 - p01) * fx;
    return static_cast<uint8_t>(std::lround(std::clamp(top + (bottom - top) * fy, 0.0f, 255.0f)));
}

// Bilinear, clamped at the edges, at a UV of the bake.
static Texel Sample(const Image& bake, const Vec2& uv) {
    float x = std::clamp(uv[0] * bake.width - 0.5f, 0.0f, float(bake.width - 1));
    float y = std::clamp(uv[1] * bake.height - 0.5f, 0.0f, float(bake.height - 1));
    uint32_t x0 = static_cast<uint32_t>(x), y0 = static_cast<uint32_t>(y);
    uint32_t x1 = std::min(x0 + 1, bake.width - 1), y1 = std::min(y0 + 1, bake.height - 1);
    float fx = x - x0, fy = y - y0;
    const Texel& a = bake.At(x0, y0);
    const Texel& b = bake.At(x1, y0);
    const Texel& c = bake.At(x0, y1);
    const Texel& d = bake.At(x1, y1);
    return { Mix(a.b, b.b, c.b, d.b, fx, fy), Mix(a.g, b.g, c.g, d.g, fx, fy), Mix(a.r, b.r, c.r, d.r, fx, fy), Mix(a.a, b.a, c.a, d.a, fx, fy) };
}

static void Rasterize(const UvTriangle& triangle, const Image& bake, const Image* base, Image& out) {
    if (triangle.fromBase && !base) {
        return;
    }
    const auto& p = triangle.installed;
    float area = Cross(p[0], p[1], p[2]);
    float size = float(out.width);
    int x0 = std::max(0, static_cast<int>(std::floor(std::min({ p[0][0], p[1][0], p[2][0] }) * size)));
    int x1 = std::min(int(out.width) - 1, static_cast<int>(std::ceil(std::max({ p[0][0], p[1][0], p[2][0] }) * size)));
    int y0 = std::max(0, static_cast<int>(std::floor(std::min({ p[0][1], p[1][1], p[2][1] }) * size)));
    int y1 = std::min(int(out.height) - 1, static_cast<int>(std::ceil(std::max({ p[0][1], p[1][1], p[2][1] }) * size)));
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            Vec2 centre = { (x + 0.5f) / size, (y + 0.5f) / size };
            float w0 = Cross(p[1], p[2], centre) / area;
            float w1 = Cross(p[2], p[0], centre) / area;
            float w2 = 1.0f - w0 - w1;
            if (w0 < -EDGE_SLACK || w1 < -EDGE_SLACK || w2 < -EDGE_SLACK) {
                continue;
            }
            const auto& r = triangle.retail;
            Vec2 uv = { w0 * r[0][0] + w1 * r[1][0] + w2 * r[2][0], w0 * r[0][1] + w1 * r[1][1] + w2 * r[2][1] };
            out.texels[size_t(y) * out.width + x] = Sample(triangle.fromBase ? *base : bake, uv);
        }
    }
}

Image ApplyBakeWarp(const BakeWarp& warp, const Image& bake, const Image* base, uint32_t size) {
    Image out;
    out.width = size;
    out.height = size;
    out.texels.resize(size_t(size) * size);
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            out.texels[size_t(y) * size + x] = Sample(bake, { (x + 0.5f) / size * BODY_U_SCALE, (y + 0.5f) / size });
        }
    }
    for (const UvTriangle& triangle : warp.triangles) {
        Rasterize(triangle, bake, base, out);
    }
    return out;
}

}
