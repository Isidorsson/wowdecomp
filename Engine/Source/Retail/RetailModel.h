#ifndef ENGINE_SOURCE_RETAIL_RETAILMODEL_H
#define ENGINE_SOURCE_RETAIL_RETAILMODEL_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "Engine/Source/Retail/AnimationFallbacks.h"

namespace Retail {

using FileReader = std::function<bool(uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error)>;

// What a conversion reads besides the model itself.
struct ModelSources {
    const AnimationFallbacks* fallbacks = nullptr;  // Without them, animation ids past 505 are refused.
    FileReader read;                                 // AFID .anim files. Without it, sequences needing one are emptied.
};

// A sequence whose retail id 3.3.5 does not know, renumbered to its fallback.
struct RemappedSequence {
    uint16_t index;
    uint16_t originalId;
    uint16_t originalVariation;
    uint16_t id;
};

// A retail model as the 3.3.5 loader reads it: an MD20 version 264 blob, plus the skins its views use.
struct ConvertedModel {
    std::vector<uint8_t> m2;
    std::vector<uint32_t> skins;                // SFID FileDataIDs of views 0 to numSkinProfiles - 1.
    std::vector<RemappedSequence> remapped;
    std::vector<std::string> warnings;          // Data dropped on the way, such as a missing .anim file.
};

// MD21 versions 272 to 274. Records keep their bytes except the fields 3.3.5 reads differently; added data
// (texture names, inlined animations, converted records) is appended and pointed at, so every untouched offset
// stays valid. Features later slices convert are refused with a reason naming them.
bool ConvertModel(const std::vector<uint8_t>& file, const ModelSources& sources, ConvertedModel& model, std::string& error);

// A retail .skin for view of a retail model file (which supplies vertex weights and the view count).
bool ConvertSkin(const std::vector<uint8_t>& skin, const std::vector<uint8_t>& modelFile, uint32_t view, std::vector<uint8_t>& out, std::string& error);

}

#endif
