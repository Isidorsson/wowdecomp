#ifndef ENGINE_SOURCE_RETAIL_RETAILBONEWINDOWS_H
#define ENGINE_SOURCE_RETAIL_RETAILBONEWINDOWS_H

#include <cstdint>
#include <string>
#include <vector>

// Bone windows: submeshes split so that none draws with more bones than the client assumes for its view
// (wow-toolbox-editor downport/transform/bone_windows.rs). The client picks the view it draws by position, never
// by boneCountMax, and uploads three vertex shader constants per bone of a submesh's window.

namespace Retail {

constexpr uint32_t MAX_VIEWS = 4;   // The views the client takes.

// The bone palette the client assumes for view of a model with views views (Wow.exe 12340 0xA45644, M2Shared.cpp).
uint32_t ViewBonePalette(uint32_t view, uint32_t views);

// The bones one draw of a submesh piece uploads, and the triangles (numbered in source order) drawn with them.
struct TriangleWindow {
    std::vector<uint16_t> bones;
    std::vector<uint32_t> triangles;

    bool operator==(const TriangleWindow& other) const { return bones == other.bones && triangles == other.triangles; }
};

// Triangles, each as the sorted model bones it weighs, grouped into windows of at most palette bones, largest
// first: each window takes, one at a time, the bone that brings the remaining triangles closest to complete
// (the downporter's partition).
std::vector<TriangleWindow> PartitionTriangles(const std::vector<std::vector<uint16_t>>& triangles, uint32_t palette);

// A run of triangles of one source submesh drawn with one window.
struct SectionPiece {
    uint32_t parent;
    std::vector<uint16_t> window;     // Model bones. Empty keeps the parent's window.
    std::vector<uint32_t> triangles;  // Within the parent, in source order.
};

// How every view is split, derived from the model and every view's original skin alone, so the model and each
// skin compute the same plan.
struct BoneWindowPlan {
    uint32_t combos = 0;                             // The model's own bone combos, which the windows follow.
    std::vector<std::vector<SectionPiece>> views;    // Every submesh's pieces, for views with one to split.
};

// Views whose palette the client skins on the CPU (256), and views whose skin is broken (refused when it is
// converted), get no pieces.
bool PlanBoneWindows(const std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, BoneWindowPlan& plan, std::string& error);

// Every piece's window, view after view, after the model's own bone combos.
bool AppendBoneWindows(std::vector<uint8_t>& m2, const BoneWindowPlan& plan, std::string& error);

// The skin of view with one submesh per piece, each with its own vertex and index range and the vertices it
// shares with other pieces copied, and the batches of a split submesh repeated per piece.
bool SplitSections(std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2, const BoneWindowPlan& plan, uint32_t view, std::string& error);

// One warning per split view, as the downporter reports it. skins are the original retail skins of every view.
void ReportBoneWindows(const std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, const BoneWindowPlan& plan, std::vector<std::string>& warnings);

}

#endif
