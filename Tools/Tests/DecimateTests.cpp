#include "Engine/Source/Retail/Decimate.h"
#include "Tools/Tests/Check.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

using Retail::Decimate;
using Retail::Decimated;
using Retail::DecimationVertex;
using Retail::IndexTriangle;

// The downporter's decimator tests (downport/transform/decimate/tests.rs) on meshes built in code.

struct Mesh {
    std::vector<DecimationVertex> vertices;
    std::vector<IndexTriangle> triangles;
};

// A flat columns by rows grid in the plane z = 0, two triangles per cell, UVs equal to the position.
static Mesh Grid(uint32_t columns, uint32_t rows) {
    Mesh mesh;
    for (uint32_t y = 0; y < rows; y++) {
        for (uint32_t x = 0; x < columns; x++) {
            DecimationVertex vertex;
            vertex.position = { double(x), double(y), 0.0 };
            vertex.uv[0] = { float(x), float(y) };
            vertex.normal = { 0.0f, 0.0f, 1.0f };
            mesh.vertices.push_back(vertex);
        }
    }
    auto at = [&](uint32_t x, uint32_t y) { return y * columns + x; };
    for (uint32_t y = 0; y + 1 < rows; y++) {
        for (uint32_t x = 0; x + 1 < columns; x++) {
            mesh.triangles.push_back({ at(x, y), at(x + 1, y), at(x, y + 1) });
            mesh.triangles.push_back({ at(x + 1, y), at(x + 1, y + 1), at(x, y + 1) });
        }
    }
    return mesh;
}

// A closed latitude and longitude sphere.
static Mesh Sphere(uint32_t rings, uint32_t segments) {
    constexpr double PI = 3.14159265358979323846;
    Mesh mesh;
    DecimationVertex north;
    north.position = { 0.0, 0.0, 1.0 };
    north.normal = { 0.0f, 0.0f, 1.0f };
    mesh.vertices.push_back(north);
    for (uint32_t ring = 1; ring < rings; ring++) {
        double phi = PI * ring / rings;
        for (uint32_t segment = 0; segment < segments; segment++) {
            double theta = 2.0 * PI * segment / segments;
            DecimationVertex vertex;
            vertex.position = { std::sin(phi) * std::cos(theta), std::sin(phi) * std::sin(theta), std::cos(phi) };
            mesh.vertices.push_back(vertex);
        }
    }
    DecimationVertex south;
    south.position = { 0.0, 0.0, -1.0 };
    mesh.vertices.push_back(south);
    uint32_t last = static_cast<uint32_t>(mesh.vertices.size() - 1);
    auto at = [&](uint32_t ring, uint32_t segment) { return 1 + (ring - 1) * segments + segment % segments; };
    for (uint32_t segment = 0; segment < segments; segment++) {
        mesh.triangles.push_back({ 0, at(1, segment), at(1, segment + 1) });
        mesh.triangles.push_back({ last, at(rings - 1, segment + 1), at(rings - 1, segment) });
    }
    for (uint32_t ring = 1; ring + 1 < rings; ring++) {
        for (uint32_t segment = 0; segment < segments; segment++) {
            mesh.triangles.push_back({ at(ring, segment), at(ring + 1, segment), at(ring, segment + 1) });
            mesh.triangles.push_back({ at(ring, segment + 1), at(ring + 1, segment), at(ring + 1, segment + 1) });
        }
    }
    return mesh;
}

static double Area(const Mesh& mesh, const IndexTriangle& t) {
    const auto& a = mesh.vertices[t[0]].position;
    const auto& b = mesh.vertices[t[1]].position;
    const auto& c = mesh.vertices[t[2]].position;
    double u[] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    double v[] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    double n[] = { u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
    return std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) / 2.0;
}

// Every triangle names three distinct vertices of the mesh and has an area.
static bool Clean(const Mesh& mesh, const std::vector<IndexTriangle>& triangles) {
    return std::all_of(triangles.begin(), triangles.end(), [&](const IndexTriangle& t) {
        bool inside = std::all_of(t.begin(), t.end(), [&](uint32_t v) { return v < mesh.vertices.size(); });
        return inside && t[0] != t[1] && t[1] != t[2] && t[0] != t[2] && Area(mesh, t) > 0.0;
    });
}

static size_t Removed(const Decimated& result) {
    return result.stats.collapsed + result.stats.borderCollapsed + result.stats.dropped + result.stats.degenerate;
}

TEST(AMeshWithinTheTargetComesBackUnchanged) {
    Mesh mesh = Grid(5, 5);
    Decimated result = Decimate(mesh.vertices, mesh.triangles, mesh.triangles.size());

    CHECK(result.triangles == mesh.triangles && Removed(result) == 0);
}

TEST(TheTriangleBudgetIsMetAndTheTrianglesAreSound) {
    Mesh mesh = Sphere(24, 32);
    size_t target = mesh.triangles.size() / 3;
    Decimated result = Decimate(mesh.vertices, mesh.triangles, target);

    // A closed mesh collapses nearly to the target.
    CHECK(result.triangles.size() <= target && result.triangles.size() >= target * 9 / 10);
    CHECK(Clean(mesh, result.triangles) && result.stats.dropped == 0);
}

TEST(DecimatingTwiceGivesTheSameTriangles) {
    Mesh mesh = Sphere(16, 20);
    Decimated first = Decimate(mesh.vertices, mesh.triangles, mesh.triangles.size() / 4);
    Decimated second = Decimate(mesh.vertices, mesh.triangles, mesh.triangles.size() / 4);

    CHECK(first.triangles == second.triangles && first.stats.collapsed == second.stats.collapsed);
}

TEST(TrianglesKeepTheirWinding) {
    Mesh mesh = Sphere(16, 20);
    Decimated result = Decimate(mesh.vertices, mesh.triangles, mesh.triangles.size() / 3);

    bool outward = true;
    for (const IndexTriangle& t : result.triangles) {
        const auto& a = mesh.vertices[t[0]].position;
        const auto& b = mesh.vertices[t[1]].position;
        const auto& c = mesh.vertices[t[2]].position;
        double u[] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
        double v[] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
        double n[] = { u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
        outward &= n[0] * (a[0] + b[0] + c[0]) + n[1] * (a[1] + b[1] + c[1]) + n[2] * (a[2] + b[2] + c[2]) > 0.0;
    }
    CHECK(outward);
}

TEST(AFlatGridKeepsItsBorderWhileTheInsideCollapses) {
    Mesh mesh = Grid(12, 12);
    Decimated result = Decimate(mesh.vertices, mesh.triangles, 100);

    std::set<uint32_t> used;
    for (const IndexTriangle& t : result.triangles) {
        used.insert(t.begin(), t.end());
    }
    bool border = true;
    for (uint32_t v = 0; v < 144; v++) {
        uint32_t x = v % 12, y = v / 12;
        border &= !(x == 0 || y == 0 || x == 11 || y == 11) || used.count(v);
    }
    CHECK(result.triangles.size() <= 100 && result.stats.borderCollapsed == 0);
    CHECK(border && Clean(mesh, result.triangles));
}

TEST(AFlatGridCollapsesItsBorderForATargetItsInsideCannotReach) {
    Mesh mesh = Grid(8, 8);
    Decimated result = Decimate(mesh.vertices, mesh.triangles, 6);

    CHECK(result.triangles.size() <= 6 && result.stats.borderCollapsed > 0 && Clean(mesh, result.triangles));
}

TEST(RepeatedPositionsAndExactDuplicatesAreRemovedFirst) {
    Mesh mesh = Grid(4, 4);
    mesh.vertices.push_back(mesh.vertices[0]);
    uint32_t copy = static_cast<uint32_t>(mesh.vertices.size() - 1);
    mesh.triangles.push_back({ 0, copy, 1 });
    mesh.triangles.push_back(mesh.triangles[0]);
    Decimated result = Decimate(mesh.vertices, mesh.triangles, mesh.triangles.size());

    CHECK(result.stats.degenerate == 2 && result.triangles.size() == mesh.triangles.size() - 2);
}

// Three triangles that share nothing, of area 0.5, 2 and 4.5.
static Mesh SeparateTriangles() {
    Mesh mesh;
    for (uint32_t index = 0; index < 3; index++) {
        double size = index + 1.0;
        double x = index * 10.0;
        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (std::array<double, 3> position : { std::array<double, 3>{ x, 0, 0 }, std::array<double, 3>{ x + size, 0, 0 }, std::array<double, 3>{ x, size, 0 } }) {
            DecimationVertex vertex;
            vertex.position = position;
            mesh.vertices.push_back(vertex);
        }
        mesh.triangles.push_back({ base, base + 1, base + 2 });
    }
    return mesh;
}

TEST(ATriangleWithNoNeighbourGoesSmallestFirst) {
    Mesh mesh = SeparateTriangles();
    Decimated result = Decimate(mesh.vertices, mesh.triangles, 2);

    CHECK(result.triangles == std::vector<IndexTriangle>({ mesh.triangles[1], mesh.triangles[2] }) && Removed(result) == 1);
    CHECK(Decimate(mesh.vertices, mesh.triangles, 1).triangles == std::vector<IndexTriangle>({ mesh.triangles[2] }));
}

TEST(ATargetOfZeroRemovesEverything) {
    Mesh mesh = Grid(4, 4);
    CHECK(Decimate(mesh.vertices, mesh.triangles, 0).triangles.empty());
}

// Two halves of a grid that share their border positions: each position of column 4 has two vertices (the right
// half's with another normal), and the right half's U is shift more than the left half's.
static Mesh SplitGrid(float shift) {
    Mesh mesh = Grid(9, 5);
    for (uint32_t v = 0; v < mesh.vertices.size(); v++) {
        if (v % 9 > 4) {
            mesh.vertices[v].uv[0][0] += shift;
        }
    }
    std::map<uint32_t, uint32_t> copyOf;
    for (uint32_t v = 0; v < 45; v++) {
        if (v % 9 == 4) {
            DecimationVertex vertex = mesh.vertices[v];
            vertex.uv[0][0] += shift;
            vertex.normal = { 0.0f, 0.6f, 0.8f };
            mesh.vertices.push_back(vertex);
            copyOf[v] = static_cast<uint32_t>(mesh.vertices.size() - 1);
        }
    }
    for (IndexTriangle& t : mesh.triangles) {
        double centre = (mesh.vertices[t[0]].position[0] + mesh.vertices[t[1]].position[0] + mesh.vertices[t[2]].position[0]) / 3.0;
        for (uint32_t& corner : t) {
            if (centre > 4.0 && copyOf.count(corner)) {
                corner = copyOf[corner];
            }
        }
    }
    return mesh;
}

TEST(AHardEdgeThatKeepsTheUvsDoesNotStopTheCollapses) {
    Mesh mesh = SplitGrid(0.0f);
    Decimated result = Decimate(mesh.vertices, mesh.triangles, 24);

    CHECK(result.triangles.size() <= 24 && result.stats.dropped == 0 && Clean(mesh, result.triangles));
}

TEST(ASeamStaysClosedEveryTriangleKeepsItsUvIsland) {
    Mesh mesh = SplitGrid(100.0f);
    Decimated result = Decimate(mesh.vertices, mesh.triangles, 24);

    bool kept = true;
    for (const IndexTriangle& t : result.triangles) {
        std::set<bool> islands;
        for (uint32_t v : t) {
            islands.insert(mesh.vertices[v].uv[0][0] > 50.0f);
        }
        kept &= islands.size() == 1;
    }
    CHECK(result.triangles.size() < mesh.triangles.size() && Clean(mesh, result.triangles) && kept);
}
