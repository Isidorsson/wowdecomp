#ifndef ENGINE_SOURCE_RETAIL_RETAILPHYSICS_H
#define ENGINE_SOURCE_RETAIL_RETAILPHYSICS_H

#include <cstdint>
#include <string>
#include <vector>

// Retail physics (a model's PFDC chunk, or the .phys file its PFID names): rigid bodies on bones, hung from each
// other by joints, that retail simulates for plumes, tails and flaps (bones flagged 0x400). 3.3.5 has no physics,
// so those bones stood in their bind pose, a plume sticking straight out of a helm.

namespace Retail {

// Gives the bones of m2's dynamic bodies the pose gravity settles them in, as one rotation key per sequence: each
// shoulder joint swings its body toward straight down (model -z), as far as its cone allows, and a spherical one all
// the way; welded and other joints hold their body to its parent. Bones that are animated keep their keys. Physics
// in a layout this does not read is warned about and changes nothing.
bool BakePhysicsPose(std::vector<uint8_t>& m2, const std::vector<uint8_t>& physics, std::vector<std::string>& warnings, std::string& error);

}

#endif
