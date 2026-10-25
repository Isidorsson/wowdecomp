#include "Engine/Source/Retail/Decimate.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <tuple>

// A port of wow-toolbox-editor downport/transform/decimate (geometry.rs, simplifier.rs): every order, tie break and
// floating point step is kept, so both pick the same collapses.

namespace Retail {

namespace {

constexpr double BONE_MISMATCH = 1.0;     // Cost of changing the moved vertex's main bone, per squared edge length.
constexpr double LENGTH_EPSILON = 1e-4;   // Breaks ties among collapses with no geometric error: shorter edges first.
constexpr double BORDER_WEIGHT = 64.0;    // Weight of the planes that keep a border where it is.
constexpr double MIN_NORMAL_COS = 0.25;   // A collapse may not turn a surviving triangle's normal past this cosine.

using Vec3 = std::array<double, 3>;

Vec3 Sub(const Vec3& a, const Vec3& b) { return { a[0] - b[0], a[1] - b[1], a[2] - b[2] }; }
double Dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 Cross(const Vec3& a, const Vec3& b) { return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] }; }
double LengthSquared(const Vec3& a) { return Dot(a, a); }

uint64_t Bits(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

uint32_t Bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Rust's f64::total_cmp as a key.
int64_t TotalOrder(double value) {
    int64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits ^ static_cast<int64_t>(static_cast<uint64_t>(bits >> 63) >> 1);
}

// The unit normal and the area of a triangle; the normal is zero for a triangle with no area.
std::pair<Vec3, double> NormalAndArea(const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 n = Cross(Sub(b, a), Sub(c, a));
    double twiceArea = std::sqrt(LengthSquared(n));
    if (twiceArea == 0.0) {
        return { Vec3{}, 0.0 };
    }
    return { Vec3{ n[0] / twiceArea, n[1] / twiceArea, n[2] / twiceArea }, twiceArea / 2.0 };
}

// A symmetric 4x4 matrix of squared distances to planes: a2 ab ac ad b2 bc bd c2 cd d2 for ax + by + cz + d = 0.
struct Quadric {
    std::array<double, 10> q = {};

    static Quadric Plane(const Vec3& n, double d, double weight) {
        double a = n[0], b = n[1], c = n[2];
        Quadric plane;
        plane.q = { a * a, a * b, a * c, a * d, b * b, b * c, b * d, c * c, c * d, d * d };
        for (double& value : plane.q) {
            value *= weight;
        }
        return plane;
    }

    void Add(const Quadric& other) {
        for (size_t i = 0; i < q.size(); i++) {
            q[i] += other.q[i];
        }
    }

    double Error(const Vec3& p) const {
        double x = p[0], y = p[1], z = p[2];
        return q[0] * x * x + 2.0 * q[1] * x * y + 2.0 * q[2] * x * z + 2.0 * q[3] * x + q[4] * y * y + 2.0 * q[5] * y * z + 2.0 * q[6] * y + q[7] * z * z
            + 2.0 * q[8] * z + q[9];
    }
};

double BoundingDiagonalSquared(const std::vector<DecimationVertex>& vertices) {
    Vec3 low = { std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };
    Vec3 high = { std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest() };
    for (const DecimationVertex& vertex : vertices) {
        for (size_t axis = 0; axis < 3; axis++) {
            low[axis] = std::min(low[axis], vertex.position[axis]);
            high[axis] = std::max(high[axis], vertex.position[axis]);
        }
    }
    double diagonal = LengthSquared(Sub(high, low));
    return std::isfinite(diagonal) && diagonal > 0.0 ? diagonal : 1.0;
}

// The vertices of one position.
struct Site {
    std::vector<uint32_t> members;
    Quadric quadric;
    std::set<uint32_t> neighbors;
    std::set<uint32_t> triangles;
    bool alive = false;
    uint32_t stamp = 0;   // Bumped whenever the site's cost changes, so queued entries go stale.
};

// A queued collapse: cost bits, site to remove, site to keep, and their stamps. Cheapest first.
using Entry = std::tuple<uint64_t, uint32_t, uint32_t, uint32_t, uint32_t>;
using Queue = std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>>;

// How each vertex of the removed site is re-pointed.
struct Plan {
    std::map<uint32_t, uint32_t> moves;
    bool alongBorder;
};

// A vertex's place in texture space, bit for bit.
std::array<uint32_t, 4> UvKey(const DecimationVertex& vertex) {
    return { Bits(vertex.uv[0][0] + 0.0f), Bits(vertex.uv[0][1] + 0.0f), Bits(vertex.uv[1][0] + 0.0f), Bits(vertex.uv[1][1] + 0.0f) };
}

class Simplifier {
    public:
    Simplifier(const std::vector<DecimationVertex>& vertices, const std::vector<IndexTriangle>& triangles);
    Decimated Run(size_t target);

    private:
    const std::vector<DecimationVertex>& m_vertices;
    std::vector<IndexTriangle> m_corners;
    std::vector<bool> m_alive;
    size_t m_live = 0;
    std::vector<uint32_t> m_siteOf;
    std::vector<Site> m_sites;
    double m_scale2 = 1.0;   // Squared diagonal of the bounding box, to make costs unit free.
    DecimationStats m_stats;

    void Weld();
    const Vec3& Position(uint32_t site) const { return m_vertices[m_sites[site].members[0]].position; }
    std::array<uint32_t, 3> TriangleSites(uint32_t triangle) const;
    std::array<Vec3, 3> TrianglePositions(uint32_t triangle) const;
    void BuildQuadrics();
    void Collapse(size_t target, bool allowBorders);
    bool Fresh(uint32_t site, uint32_t stamp) const { return m_sites[site].alive && m_sites[site].stamp == stamp; }
    Entry MakeEntry(uint32_t from, uint32_t to) const;
    double Cost(uint32_t from, uint32_t to) const;
    uint8_t MainBone(uint32_t site) const { return m_vertices[m_sites[site].members[0]].mainBone; }
    bool IsBorder(uint32_t site) const;
    std::vector<uint32_t> SharedTriangles(uint32_t a, uint32_t b) const;
    std::optional<Plan> MakePlan(uint32_t from, uint32_t to, bool allowBorders) const;
    std::optional<std::map<uint32_t, uint32_t>> Moves(uint32_t from, uint32_t to, const std::vector<uint32_t>& shared) const;
    std::map<uint32_t, uint32_t> Partners(uint32_t from, uint32_t to, const std::vector<uint32_t>& shared) const;
    double AttributeDistance(uint32_t a, uint32_t b) const;
    bool WouldFlip(uint32_t from, uint32_t to, const std::vector<uint32_t>& shared) const;
    void Apply(uint32_t from, uint32_t to, const Plan& plan, Queue& queue);
    void Relink(uint32_t site);
    void Kill(uint32_t triangle);
    void DropSmallest(size_t target);
};

// The site of each vertex, and the sites, one per distinct position.
void Simplifier::Weld() {
    std::map<std::array<uint64_t, 3>, uint32_t> byPosition;
    for (uint32_t index = 0; index < m_vertices.size(); index++) {
        const Vec3& p = m_vertices[index].position;
        // Adding zero turns -0.0 into 0.0, which would otherwise be another site.
        std::array<uint64_t, 3> key = { Bits(p[0] + 0.0), Bits(p[1] + 0.0), Bits(p[2] + 0.0) };
        auto found = byPosition.find(key);
        if (found == byPosition.end()) {
            m_sites.emplace_back();
            m_sites.back().alive = true;
            found = byPosition.emplace(key, static_cast<uint32_t>(m_sites.size() - 1)).first;
        }
        m_sites[found->second].members.push_back(index);
        m_siteOf.push_back(found->second);
    }
}

Simplifier::Simplifier(const std::vector<DecimationVertex>& vertices, const std::vector<IndexTriangle>& triangles) : m_vertices(vertices) {
    Weld();
    std::set<IndexTriangle> seen;
    for (const IndexTriangle& triangle : triangles) {
        uint32_t a = m_siteOf[triangle[0]], b = m_siteOf[triangle[1]], c = m_siteOf[triangle[2]];
        IndexTriangle key = triangle;
        std::sort(key.begin(), key.end());
        if (a == b || b == c || a == c || !seen.insert(key).second) {
            m_stats.degenerate++;
            continue;
        }
        m_corners.push_back(triangle);
    }

    for (uint32_t index = 0; index < m_corners.size(); index++) {
        std::array<uint32_t, 3> at = TriangleSites(index);
        for (size_t position = 0; position < 3; position++) {
            m_sites[at[position]].triangles.insert(index);
            for (size_t other = position + 1; other < 3; other++) {
                m_sites[at[position]].neighbors.insert(at[other]);
                m_sites[at[other]].neighbors.insert(at[position]);
            }
        }
    }

    m_live = m_corners.size();
    m_alive.assign(m_live, true);
    m_scale2 = BoundingDiagonalSquared(vertices);
    BuildQuadrics();
}

std::array<uint32_t, 3> Simplifier::TriangleSites(uint32_t triangle) const {
    const IndexTriangle& corner = m_corners[triangle];
    return { m_siteOf[corner[0]], m_siteOf[corner[1]], m_siteOf[corner[2]] };
}

std::array<Vec3, 3> Simplifier::TrianglePositions(uint32_t triangle) const {
    std::array<uint32_t, 3> sites = TriangleSites(triangle);
    return { Position(sites[0]), Position(sites[1]), Position(sites[2]) };
}

// Every triangle's plane, weighted by its area, goes to its three sites; every border edge adds a plane through it
// that stands on its triangle.
void Simplifier::BuildQuadrics() {
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> edges;
    for (uint32_t triangle = 0; triangle < m_corners.size(); triangle++) {
        std::array<uint32_t, 3> sites = TriangleSites(triangle);
        std::array<Vec3, 3> p = TrianglePositions(triangle);
        auto [n, area] = NormalAndArea(p[0], p[1], p[2]);
        Quadric plane = Quadric::Plane(n, -Dot(n, p[0]), area);
        for (uint32_t site : sites) {
            m_sites[site].quadric.Add(plane);
        }
        for (auto [from, to] : { std::make_pair(0, 1), std::make_pair(1, 2), std::make_pair(2, 0) }) {
            edges[{ std::min(sites[from], sites[to]), std::max(sites[from], sites[to]) }].push_back(triangle);
        }
    }

    for (const auto& [edge, triangles] : edges) {
        if (triangles.size() != 1) {
            continue;
        }
        Vec3 pa = Position(edge.first);
        Vec3 pb = Position(edge.second);
        std::array<Vec3, 3> p = TrianglePositions(triangles[0]);
        Vec3 n = NormalAndArea(p[0], p[1], p[2]).first;
        Vec3 side = Cross(Sub(pb, pa), n);
        double reach = std::sqrt(LengthSquared(side));
        if (reach == 0.0) {
            continue;
        }
        side = { side[0] / reach, side[1] / reach, side[2] / reach };
        Quadric plane = Quadric::Plane(side, -Dot(side, pa), BORDER_WEIGHT * std::max(reach, 1e-12));
        m_sites[edge.first].quadric.Add(plane);
        m_sites[edge.second].quadric.Add(plane);
    }
}

Decimated Simplifier::Run(size_t target) {
    if (m_live > target) {
        Collapse(target, false);
    }
    if (m_live > target) {
        Collapse(target, true);
    }
    if (m_live > target) {
        DropSmallest(target);
    }

    Decimated result;
    for (size_t t = 0; t < m_corners.size(); t++) {
        if (m_alive[t]) {
            result.triangles.push_back(m_corners[t]);
        }
    }
    result.stats = m_stats;
    return result;
}

// Collapse the cheapest valid edges until target triangles are left or a whole pass finds none that is valid.
void Simplifier::Collapse(size_t target, bool allowBorders) {
    while (true) {
        size_t before = m_live;
        Queue queue;
        for (uint32_t from = 0; from < m_sites.size(); from++) {
            for (uint32_t to : m_sites[from].neighbors) {
                queue.push(MakeEntry(from, to));
            }
        }

        while (m_live > target && !queue.empty()) {
            auto [cost, from, to, fromStamp, toStamp] = queue.top();
            queue.pop();
            if (!Fresh(from, fromStamp) || !Fresh(to, toStamp)) {
                continue;
            }
            if (std::optional<Plan> plan = MakePlan(from, to, allowBorders)) {
                Apply(from, to, *plan, queue);
            }
        }

        if (m_live <= target || m_live == before) {
            return;
        }
    }
}

Entry Simplifier::MakeEntry(uint32_t from, uint32_t to) const {
    return { Bits(Cost(from, to)), from, to, m_sites[from].stamp, m_sites[to].stamp };
}

// The error of moving from onto to, unit free and never negative, so its bits order like the number.
double Simplifier::Cost(uint32_t from, uint32_t to) const {
    const Vec3& target = Position(to);
    double geometric = m_sites[from].quadric.Error(target) + m_sites[to].quadric.Error(target);
    double edge = LengthSquared(Sub(Position(from), target));
    double bones = MainBone(from) == MainBone(to) ? 0.0 : BONE_MISMATCH;
    return (std::fmax(geometric, 0.0) + (LENGTH_EPSILON + bones) * edge) / m_scale2;
}

// Whether the edge between site and some neighbour has other than two triangles.
bool Simplifier::IsBorder(uint32_t site) const {
    return std::any_of(m_sites[site].neighbors.begin(), m_sites[site].neighbors.end(), [&](uint32_t other) { return SharedTriangles(site, other).size() != 2; });
}

std::vector<uint32_t> Simplifier::SharedTriangles(uint32_t a, uint32_t b) const {
    std::vector<uint32_t> shared;
    for (uint32_t triangle : m_sites[a].triangles) {
        std::array<uint32_t, 3> sites = TriangleSites(triangle);
        if (std::find(sites.begin(), sites.end(), b) != sites.end()) {
            shared.push_back(triangle);
        }
    }
    return shared;
}

// How to collapse from onto to, if that keeps the surface valid.
std::optional<Plan> Simplifier::MakePlan(uint32_t from, uint32_t to, bool allowBorders) const {
    std::vector<uint32_t> shared = SharedTriangles(from, to);
    if (shared.empty() || shared.size() > 2) {
        return std::nullopt;
    }

    bool alongBorder = IsBorder(from);
    if (alongBorder && !(allowBorders && shared.size() == 1)) {
        return std::nullopt;
    }

    std::set<uint32_t> apexes;
    for (uint32_t triangle : shared) {
        for (uint32_t site : TriangleSites(triangle)) {
            if (site != from && site != to) {
                apexes.insert(site);
            }
        }
    }
    std::set<uint32_t> common;
    const std::set<uint32_t>& mine = m_sites[from].neighbors;
    const std::set<uint32_t>& theirs = m_sites[to].neighbors;
    std::set_intersection(mine.begin(), mine.end(), theirs.begin(), theirs.end(), std::inserter(common, common.end()));
    if (common != apexes) {
        return std::nullopt;
    }

    std::optional<std::map<uint32_t, uint32_t>> moves = Moves(from, to, shared);
    if (!moves || WouldFlip(from, to, shared)) {
        return std::nullopt;
    }
    return Plan{ std::move(*moves), alongBorder };
}

// For each vertex of from that a surviving triangle uses, the vertex of to it moves to: the one it shares a triangle
// with (the closest in UV and normal if several), or else the one a vertex at the same place in the texture, differing
// only in its normal, moves to. None when some vertex has no partner, which would pull a triangle across a seam.
std::optional<std::map<uint32_t, uint32_t>> Simplifier::Moves(uint32_t from, uint32_t to, const std::vector<uint32_t>& shared) const {
    std::map<uint32_t, uint32_t> partners = Partners(from, to, shared);
    std::map<uint32_t, uint32_t> moves;
    for (uint32_t triangle : m_sites[from].triangles) {
        if (std::find(shared.begin(), shared.end(), triangle) != shared.end()) {
            continue;
        }

        const IndexTriangle& corner = m_corners[triangle];
        auto on = std::find_if(corner.begin(), corner.end(), [&](uint32_t v) { return m_siteOf[v] == from; });
        if (on == corner.end()) {
            return std::nullopt;
        }
        uint32_t vertex = *on;
        if (moves.count(vertex)) {
            continue;
        }

        auto partner = partners.find(vertex);
        if (partner == partners.end()) {
            std::array<uint32_t, 4> uv = UvKey(m_vertices[vertex]);
            for (uint32_t mate : m_sites[from].members) {
                if (UvKey(m_vertices[mate]) == uv && partners.count(mate)) {
                    partner = partners.find(mate);
                    break;
                }
            }
        }
        if (partner == partners.end()) {
            return std::nullopt;
        }
        moves[vertex] = partner->second;
    }
    return moves;
}

// The vertex of to that each vertex of from shares a triangle with among the shared ones; the closest in UV and normal
// if several.
std::map<uint32_t, uint32_t> Simplifier::Partners(uint32_t from, uint32_t to, const std::vector<uint32_t>& shared) const {
    std::map<uint32_t, std::vector<uint32_t>> found;
    for (uint32_t triangle : shared) {
        const IndexTriangle& corner = m_corners[triangle];
        auto on = [&](uint32_t site) { return std::find_if(corner.begin(), corner.end(), [&](uint32_t v) { return m_siteOf[v] == site; }); };
        auto a = on(from);
        auto b = on(to);
        if (a != corner.end() && b != corner.end()) {
            found[*a].push_back(*b);
        }
    }

    std::map<uint32_t, uint32_t> partners;
    for (const auto& [vertex, candidates] : found) {
        uint32_t best = candidates[0];
        for (size_t i = 1; i < candidates.size(); i++) {
            int64_t x = TotalOrder(AttributeDistance(vertex, candidates[i]));
            int64_t y = TotalOrder(AttributeDistance(vertex, best));
            if (x < y || (x == y && candidates[i] < best)) {
                best = candidates[i];
            }
        }
        partners[vertex] = best;
    }
    return partners;
}

double Simplifier::AttributeDistance(uint32_t a, uint32_t b) const {
    const DecimationVertex& p = m_vertices[a];
    const DecimationVertex& q = m_vertices[b];
    double uv = 0.0;
    for (size_t set = 0; set < 2; set++) {
        float du = p.uv[set][0] - q.uv[set][0];
        float dv = p.uv[set][1] - q.uv[set][1];
        uv += static_cast<double>(du * du + dv * dv);
    }
    double normal = 0.0;
    for (size_t axis = 0; axis < 3; axis++) {
        float d = p.normal[axis] - q.normal[axis];
        normal += static_cast<double>(d * d);
    }
    return uv + normal;
}

// Whether moving from to to turns a surviving triangle over or squashes it flat.
bool Simplifier::WouldFlip(uint32_t from, uint32_t to, const std::vector<uint32_t>& shared) const {
    const Vec3& moved = Position(from);
    const Vec3& target = Position(to);
    double tinyArea = m_scale2 * 1e-14;
    for (uint32_t triangle : m_sites[from].triangles) {
        if (std::find(shared.begin(), shared.end(), triangle) != shared.end()) {
            continue;
        }

        std::vector<Vec3> others;
        for (uint32_t site : TriangleSites(triangle)) {
            if (site != from) {
                others.push_back(Position(site));
            }
        }
        auto [before, areaBefore] = NormalAndArea(moved, others[0], others[1]);
        auto [after, areaAfter] = NormalAndArea(target, others[0], others[1]);
        if (areaBefore > tinyArea && (areaAfter <= tinyArea || Dot(before, after) < MIN_NORMAL_COS)) {
            return true;
        }
    }
    return false;
}

void Simplifier::Apply(uint32_t from, uint32_t to, const Plan& plan, Queue& queue) {
    std::vector<uint32_t> triangles(m_sites[from].triangles.begin(), m_sites[from].triangles.end());
    for (uint32_t triangle : triangles) {
        std::array<uint32_t, 3> sites = TriangleSites(triangle);
        if (std::find(sites.begin(), sites.end(), to) != sites.end()) {
            Kill(triangle);
            continue;
        }
        for (uint32_t& corner : m_corners[triangle]) {
            if (m_siteOf[corner] == from) {
                corner = plan.moves.at(corner);
            }
        }
        m_sites[from].triangles.erase(triangle);
        m_sites[to].triangles.insert(triangle);
    }
    (plan.alongBorder ? m_stats.borderCollapsed : m_stats.collapsed)++;

    for (uint32_t other : std::vector<uint32_t>(m_sites[from].neighbors.begin(), m_sites[from].neighbors.end())) {
        m_sites[other].neighbors.erase(from);
    }
    Site removed = std::move(m_sites[from]);
    m_sites[from] = Site();
    m_sites[to].quadric.Add(removed.quadric);
    m_sites[to].stamp++;
    Relink(to);
    for (uint32_t other : std::vector<uint32_t>(m_sites[to].neighbors.begin(), m_sites[to].neighbors.end())) {
        queue.push(MakeEntry(to, other));
        queue.push(MakeEntry(other, to));
    }
}

// Recompute the neighbours of site from its triangles, on both sides.
void Simplifier::Relink(uint32_t site) {
    std::set<uint32_t> after;
    for (uint32_t triangle : m_sites[site].triangles) {
        for (uint32_t other : TriangleSites(triangle)) {
            if (other != site) {
                after.insert(other);
            }
        }
    }
    for (uint32_t other : std::vector<uint32_t>(m_sites[site].neighbors.begin(), m_sites[site].neighbors.end())) {
        if (!after.count(other)) {
            m_sites[other].neighbors.erase(site);
        }
    }
    for (uint32_t other : after) {
        m_sites[other].neighbors.insert(site);
    }
    m_sites[site].neighbors = std::move(after);
}

void Simplifier::Kill(uint32_t triangle) {
    for (uint32_t site : TriangleSites(triangle)) {
        m_sites[site].triangles.erase(triangle);
    }
    m_alive[triangle] = false;
    m_live--;
}

// The last resort: drop the triangles of least area, ties by number.
void Simplifier::DropSmallest(size_t target) {
    std::vector<std::pair<uint64_t, uint32_t>> byArea;
    for (uint32_t triangle = 0; triangle < m_corners.size(); triangle++) {
        if (m_alive[triangle]) {
            std::array<Vec3, 3> p = TrianglePositions(triangle);
            byArea.emplace_back(Bits(NormalAndArea(p[0], p[1], p[2]).second), triangle);
        }
    }
    std::sort(byArea.begin(), byArea.end());
    size_t drop = m_live - target;
    for (size_t i = 0; i < drop; i++) {
        m_alive[byArea[i].second] = false;
        m_live--;
        m_stats.dropped++;
    }
}

}

Decimated Decimate(const std::vector<DecimationVertex>& vertices, const std::vector<IndexTriangle>& triangles, size_t target) {
    return Simplifier(vertices, triangles).Run(target);
}

}
