#ifndef ENGINE_SOURCE_RETAIL_DECIMATE_H
#define ENGINE_SOURCE_RETAIL_DECIMATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Triangle decimation of one submesh by quadric error half edge collapse (wow-toolbox-editor
// downport/transform/decimate). A collapse only re-points triangle corners from one existing vertex to another, so the
// result is a subset of the input's triangles with some corners swapped. Vertices at one position are a site; a site
// collapses only when each of its vertices has a partner on the target site, which keeps UV seams closed. Collapses run
// inside the surface, then along its border too, then the smallest triangles are dropped, deterministically.

namespace Retail {

struct DecimationVertex {
    std::array<double, 3> position = {};
    std::array<std::array<float, 2>, 2> uv = {};
    std::array<float, 3> normal = {};
    uint8_t mainBone = 0;   // The bone that carries most of the vertex's weight.
};

struct DecimationStats {
    size_t degenerate = 0;        // Two corners at one position, or an exact duplicate, removed first.
    size_t collapsed = 0;         // Collapses inside the surface.
    size_t borderCollapsed = 0;   // Collapses along the border.
    size_t dropped = 0;           // Triangles dropped, smallest first, once no collapse was left.
};

using IndexTriangle = std::array<uint32_t, 3>;

struct Decimated {
    std::vector<IndexTriangle> triangles;   // The survivors in input order, as indices into the input vertices.
    DecimationStats stats;
};

// triangles (indices into vertices) reduced to at most target.
Decimated Decimate(const std::vector<DecimationVertex>& vertices, const std::vector<IndexTriangle>& triangles, size_t target);

}

#endif
