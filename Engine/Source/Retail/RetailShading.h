#ifndef ENGINE_SOURCE_RETAIL_RETAILSHADING_H
#define ENGINE_SOURCE_RETAIL_RETAILSHADING_H

#include <cstdint>
#include <string>
#include <vector>

namespace Retail {

// Appends the tables every converted batch indexes: the 3.3.5 combiner table with every pair of operations
// (announced by global flag 8; the array that started in its header slot moves to the end) and coordinate
// combos with every pair of sources. Both are the same for every model, so a skin finds them without its model.
bool AppendShadingTables(std::vector<uint8_t>& m2, std::string& error);

struct ShadingTables {
    uint32_t coordPairs;    // Where the coordinate pairs start in the model's coordinate combos.
};

bool ReadShadingTables(const std::vector<uint8_t>& m2, ShadingTables& tables, std::string& error);

// Points a batch at the combiner and coordinate pairs its retail shader encodes. Effects 0 to 2 become the
// client's own, reading UV1 and the environment map; later effects are refused (converter slice C5b).
bool ShadeBatch(std::vector<uint8_t>& skin, size_t batch, const ShadingTables& tables, std::string& error);

// A 3.3.5 two-texture batch draws its second texture with UV2 (Diffuse_T1_T2), so UV1 is copied into UV2 on
// the vertices of batches that sample UV1 twice, unless a batch of any view reads UV2 on them (those are kept
// and warned). skins are the original retail skins of every view.
bool CopyFirstUvSet(std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, std::vector<std::string>& warnings, std::string& error);

}

#endif
