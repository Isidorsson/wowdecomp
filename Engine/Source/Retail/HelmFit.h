#ifndef ENGINE_SOURCE_RETAIL_HELMFIT_H
#define ENGINE_SOURCE_RETAIL_HELMFIT_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Retail {

// How retail refit one race and gender's helms for the HD heads: retail = scale * stock + shift, the shift along
// x (forward) and z (up). A helm is drawn in the frame of the body's helm attachment, which the HD bodies moved
// into the middle of the head, so a retail helm on a 3.3.5 body floats up and forward (and, for tauren and gnomes,
// is too big). The table is measured by the wow-toolbox downporter (helm_fit.rs) on the helms both clients ship;
// DeriveHelmFit reads the fit off the installed body instead when that body is an HD port.
struct HelmFit {
    float scale;
    float shiftX;
    float shiftZ;
};

// The measured fit of a stock race (1 to 11) and gender (0 male, 1 female) on the 3.3.5 bodies; null otherwise.
const HelmFit* HelmFitFor(uint32_t race, uint32_t gender);

// The race and gender of a race-named helm read, "item/objectcomponents/head/retail-<fdid>_<prefix><m|f>.m2";
// false for any other path, a weapon-style "retail-<fdid>" name included.
bool HelmRaceForPath(const std::string& normalized, uint32_t& race, uint32_t& gender);

// The body the client draws a stock race and gender with ("Character\Human\Female\HumanFemale.m2"), and the
// retail body of the same name the retail helms were made for; empty and 0 otherwise.
std::string StockBodyPath(uint32_t race, uint32_t gender);
uint32_t RetailBodyFileDataId(uint32_t race, uint32_t gender);

// Where a body hangs its helms: the helm attachment (id 11) at rest, and the pivots of its head bone (key bone 6)
// and every bone above it to the root.
struct HeadFrame {
    std::array<float, 3> attachment;
    std::vector<std::array<float, 3>> chain;
};

// The frame of a 3.3.5 (MD20) or retail (MD21) body; false when it has no helm attachment under a head bone.
bool ReadHeadFrame(const std::vector<uint8_t>& model, HeadFrame& frame, std::string& error);

// A body with the retail skeleton up to its head (an HD body ported to 3.3.5) has the retail head, so retail helms
// fit it unscaled, moved only as far as its helm attachment moved. False for any other body: its head differs, and
// the table measured on the 3.3.5 bodies fits it.
bool DeriveHelmFit(const HeadFrame& stock, const HeadFrame& retail, HelmFit& fit);

// Moves a converted (MD20 version 264) helm onto the 3.3.5 head: stock = (retail - shift) / scale for every
// point (vertices, bone pivots, attachments, events, lights, ribbons, particles, camera bases, collision,
// bounds), and / scale for every offset (bone translation keys, particle splines, camera keys).
bool FitHelmToStockHead(std::vector<uint8_t>& m2, const HelmFit& fit, std::string& error);

}

#endif
