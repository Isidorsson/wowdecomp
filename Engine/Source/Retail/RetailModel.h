#ifndef ENGINE_SOURCE_RETAIL_RETAILMODEL_H
#define ENGINE_SOURCE_RETAIL_RETAILMODEL_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "Engine/Source/Retail/AnimationFallbacks.h"
#include "Engine/Source/Retail/ModelSkins.h"

namespace Retail {

using FileReader = std::function<bool(uint32_t fileDataId, std::vector<uint8_t>& bytes, std::string& error)>;

// What a conversion reads besides the model itself.
struct ModelSources {
    const AnimationFallbacks* fallbacks = nullptr;  // Without them, animation ids past 505 are refused.
    FileReader read;                                 // AFID .anim files and SKID .skel files. Without it, sequences needing
                                                     // an .anim are emptied and models on a skeleton refused.
    const ModelSkin* skin = nullptr;              // The model's default skin. Without it, skin slots stay unnamed.
};

// A sequence whose retail id 3.3.5 does not know, renumbered to its fallback.
struct RemappedSequence {
    uint16_t index;
    uint16_t originalId;
    uint16_t originalVariation;
    uint16_t id;
};

// What a model on a shared skeleton (SKID) took from it; fileDataId 0 for a model with sequences of its own.
struct ModelSkeleton {
    uint32_t fileDataId = 0;
    std::vector<uint32_t> parents;      // Nearest first.
    uint32_t own = 0;                   // Sequences of the model's skeleton: the runs the model's own tracks have.
    uint32_t parentCount = 0;           // Sequences of the parent's merged list.
    std::vector<int32_t> parentIndex;   // Per merged sequence: its index in the parent's list when its keys came from it, else -1.
    std::vector<uint8_t> afid;          // The chain's AFID rows, nearest skeleton first.
};

// A retail model as the 3.3.5 loader reads it: an MD20 version 264 blob, plus the skins its views use.
struct ConvertedModel {
    std::vector<uint8_t> m2;
    std::vector<uint32_t> skins;                // SFID FileDataIDs of views 0 to numSkinProfiles - 1.
    std::vector<std::vector<uint8_t>> skinBytes; // Per view, its skin as ConvertSkin converts it: empty for a view that
                                                 // does not convert, and none when a view's skin was not read.
    std::vector<RemappedSequence> remapped;
    ModelSkeleton skeleton;
    std::vector<std::string> warnings;          // Data dropped on the way, such as a missing .anim file.
};

// MD21 versions 272 to 274. Records keep their bytes except the fields 3.3.5 reads differently; added data
// (texture names, inlined animations, converted records) is appended and pointed at, so every untouched offset
// stays valid. Features later slices convert are refused with a reason naming them.
bool ConvertModel(const std::vector<uint8_t>& file, const ModelSources& sources, ConvertedModel& model, std::string& error);

// A retail .skin for view of a retail model file, which supplies vertex weights, the view count and, by SFID, the
// other views' skins.
struct SkinInput {
    const std::vector<uint8_t>& skin;
    const std::vector<uint8_t>& modelFile;
    uint32_t view;
    FileReader read;   // The other views' original skins, which place this view's bone windows. Without it a
                       // model of several views has no submesh split.
};

bool ConvertSkin(const SkinInput& input, std::vector<uint8_t>& out, std::string& error);

}

#endif
