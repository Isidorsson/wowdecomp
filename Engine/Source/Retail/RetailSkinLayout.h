#ifndef ENGINE_SOURCE_RETAIL_RETAILSKINLAYOUT_H
#define ENGINE_SOURCE_RETAIL_RETAILSKINLAYOUT_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/Decimate.h"

// Submesh index ranges as the 3.3.5 client draws them. It reads each range from the 16-bit indexStart, never from
// Level (the high half retail skins past 65,535 corners use), then copies the ranges into one index buffer in submesh
// order and keeps where each lands in 16 bits again (CM2Shared::SetIndices). So every submesh but the last may start
// at most 65,535 corners into that buffer, whatever order the file holds the ranges in.

namespace Retail {

// The first index of the submesh record at, Level included.
uint32_t SectionIndexStart(const std::vector<uint8_t>& skin, size_t at);

// The corner count of every submesh of skin, in order. Empty for a skin whose submeshes cannot be read.
std::vector<uint32_t> SectionCounts(const std::vector<uint8_t>& skin);

// Whether submeshes of these corner counts can be laid out for the client: in their order, or else smallest first and
// the largest last (the downporter's layout_order), which fits whenever the others hold 65,535 corners together.
bool SectionsFit(const std::vector<uint32_t>& counts);

struct SectionReduction {
    uint32_t section;   // As numbered before the layout.
    uint32_t before;    // Triangles.
    uint32_t after;
};

struct SkinLayout {
    uint32_t leveled = 0;     // Submeshes that started past 65,535 in the file.
    bool reordered = false;   // Submeshes moved, the largest last, and their batches with them.
    std::vector<SectionReduction> reduced;
    DecimationStats stats;
    bool changed = false;
};

// The skin with its submeshes' ranges laid out one after another, Level 0, in an order SectionsFit accepts. When none
// fits, the submeshes other than the largest first lose triangles by edge collapse (Decimate) until they hold 65,535
// corners together, the largest keeping all of its own (the downporter's skin_layout fit); m2 supplies the vertices it
// weighs. A skin without Level whose ranges already fit is left as it is.
bool LayOutSections(std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2, SkinLayout& layout, std::string& error);

// The warning for view when LayOutSections changed its skin (split first), which skin now is.
void ReportLayout(uint32_t view, const std::vector<uint8_t>& skin, const SkinLayout& layout, std::vector<std::string>& warnings);

}

#endif
