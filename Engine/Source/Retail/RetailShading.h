#ifndef ENGINE_SOURCE_RETAIL_RETAILSHADING_H
#define ENGINE_SOURCE_RETAIL_RETAILSHADING_H

#include <cstdint>
#include <string>
#include <vector>

namespace Retail {

// Appends the tables every converted batch indexes: the 3.3.5 combiner table with every pair of operations
// (announced by global flag 8; the array that started in its header slot moves to the end), coordinate combos
// with every pair of sources, and layer materials for effect passes after the model's own. All are derived
// from the model alone, so a skin finds them without the other skins.
bool AppendShadingTables(std::vector<uint8_t>& m2, std::string& error);

struct ShadingTables {
    uint32_t coordPairs;        // Where the coordinate pairs start in the model's coordinate combos.
    uint32_t materials;         // The model's own materials, before the layer materials.
    uint32_t textureCombos;
    uint32_t weightCombos;
    uint32_t transformCombos;
};

bool ReadShadingTables(const std::vector<uint8_t>& m2, ShadingTables& tables, std::string& error);

// Points every batch at the combiner and coordinate pairs its retail shader or effect base encodes. Effects
// with textures past their base get one batch per further texture right after the base, on the same submesh.
bool ShadeBatches(std::vector<uint8_t>& skin, const ShadingTables& tables, std::string& error);

// A 3.3.5 two-texture batch draws its second texture with UV2 (Diffuse_T1_T2), so UV1 is copied into UV2 on
// the vertices of batches that sample UV1 twice, unless a batch or pass of any view reads UV2 on them (those
// are kept and warned). skins are the original retail skins of every view.
bool CopyFirstUvSet(std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, std::vector<std::string>& warnings, std::string& error);

// One warning per view and retail effect the client lacks: what its batches draw instead, their passes and
// what is lost, as the downporter reports them. skins are the original retail skins of every view.
bool ReportEffects(const std::vector<uint8_t>& m2, const std::vector<std::vector<uint8_t>>& skins, std::vector<std::string>& warnings, std::string& error);

}

#endif
