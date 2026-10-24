#ifndef ENGINE_SOURCE_RETAIL_RETAILRECORDS_H
#define ENGINE_SOURCE_RETAIL_RETAILRECORDS_H

#include <cstdint>
#include <string>
#include <vector>

namespace Retail {

// Rewrites 116-byte retail cameras (FoV track) as 100-byte 3.3.5 ones (constant FoV).
bool ConvertCameras(std::vector<uint8_t>& m2, std::string& error);

// Rewrites 492-byte retail particle emitters as 476-byte 3.3.5 ones. compressed gets the emitters whose
// gravity keys are packed vectors, for DecodeParticleGravity once their tracks are inlined.
bool ConvertParticles(std::vector<uint8_t>& m2, std::vector<uint32_t>& compressed, std::vector<std::string>& warnings, std::string& error);

// Rewrites the packed gravity vectors of those emitters as the float 3.3.5 reads: the downward pull.
bool DecodeParticleGravity(std::vector<uint8_t>& m2, const std::vector<uint32_t>& compressed, std::string& error);

}

#endif
