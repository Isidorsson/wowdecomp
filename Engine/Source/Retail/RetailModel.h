#ifndef ENGINE_SOURCE_RETAIL_RETAILMODEL_H
#define ENGINE_SOURCE_RETAIL_RETAILMODEL_H

#include <cstdint>
#include <string>
#include <vector>

namespace Retail {

// A retail model as the 3.3.5 loader reads it: an MD20 version 264 blob, plus the skins its views use.
struct ConvertedModel {
    std::vector<uint8_t> m2;
    std::vector<uint32_t> skins;    // SFID FileDataIDs of views 0 to numSkinProfiles - 1.
};

// MD21 versions 272 to 274. Records keep their bytes except the fields 3.3.5 reads differently; added data
// (texture names) is appended and pointed at, so every untouched offset stays valid. Features later slices
// convert are refused with a reason naming them.
bool ConvertModel(const std::vector<uint8_t>& file, ConvertedModel& model, std::string& error);

// A retail .skin for view of the converted m2 (which supplies vertex weights and the view count).
bool ConvertSkin(const std::vector<uint8_t>& skin, const std::vector<uint8_t>& m2, uint32_t view, std::vector<uint8_t>& out, std::string& error);

}

#endif
