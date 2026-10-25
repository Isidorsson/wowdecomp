#ifndef ENGINE_SOURCE_RETAIL_BAKEWARP_H
#define ENGINE_SOURCE_RETAIL_BAKEWARP_H

#include <array>
#include <functional>
#include <cstdint>
#include <vector>
#include "Engine/Source/Retail/BakeBlp.h"
#include "Engine/Source/Retail/BodyMatch.h"

namespace Retail {

// A retail HD bake is laid out for the retail body's UVs (2:1, body in the left half, face in the right). The installed
// HD ports re-laid their UVs into the classic square, so the bake is resampled triangle by triangle: each installed
// skin triangle takes the bake texels under the retail UVs of the vertices it matched.

// The installed composite size (componentTextureLevel 9).
constexpr uint32_t WARPED_BAKE_SIZE = 512;

struct UvTriangle {
    std::array<Vec2, 3> installed;
    std::array<Vec2, 3> retail;
    bool fromBase = false;   // Retail draws it with another texture (the HD eyes): it keeps the installed skin.
};

struct BakeWarp {
    std::vector<UvTriangle> triangles;
};

// One triangle per installed skin triangle with a non-degenerate UV. Of a matched vertex's retail candidates' different
// UVs, the combination whose installed-to-retail UV affine is least distorted wins (a seam copy lies on another island,
// which skews or flips the triangle, or blows it up). A triangle with an unmatched vertex, or with no such
// combination, maps through the UV affine of the matched triangle fewest shared-vertex steps away (installed seams
// split vertices, so this keeps to the UV island) that keeps it on the bake, else of the nearest matched triangle by
// installed UV centre. A triangle whose vertices retail draws only with other textures is marked fromBase.
//
// With regions, a triangle maps only where its region of the texture lies (a candidate combination or a neighbour's
// affine that would land it elsewhere is not taken; a triangle nothing lands right is left out).
using RegionCheck = std::function<bool(const Vec2& installedCentre, const Vec2& retailCentre)>;
BakeWarp BuildBakeWarp(const BodyMesh& installed, const BodyMesh& retail, const VertexMatches& matches, const RegionCheck& regions = nullptr);

// The player bodies' regions: the installed classic square has the face in its FaceUpper and FaceLower rects (u below
// 0.5, v from 0.625), the retail HD layouts 103 to 124 in their right half; the body is everything else.
bool PlayerBodyRegions(const Vec2& installedCentre, const Vec2& retailCentre);

// The warped bake, size by size: texels inside a triangle sample the bake bilinearly at the retail UV (a fromBase one
// the installed base skin at its own UV, when given); the rest take the body's measured relation (retail u =
// installed u / 2), so filtering at island edges finds plausible colour.
Image ApplyBakeWarp(const BakeWarp& warp, const Image& bake, const Image* base = nullptr, uint32_t size = WARPED_BAKE_SIZE);

}

#endif
