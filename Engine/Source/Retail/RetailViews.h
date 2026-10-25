#ifndef ENGINE_SOURCE_RETAIL_RETAILVIEWS_H
#define ENGINE_SOURCE_RETAIL_RETAILVIEWS_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Retail/RetailBoneWindows.h"
#include "Engine/Source/Retail/RetailModel.h"

// Which retail skins a converted model keeps as its views. SFID lists the views, then the LOD skins
// (<model>_lodNN.skin). A LOD skin counts its vertices from where its band starts in the model, which it keeps in
// the field a view uses for boneCountMax (wow-toolbox-editor downport/read/skin_files.rs rebase_lod_vertices).

namespace Retail {

struct ViewSkins {
    std::vector<uint32_t> fileDataIds;
    std::vector<std::vector<uint8_t>> originals;   // A LOD skin's lookup counts from the model's first vertex.
    std::vector<std::string> warnings;
    BoneWindowPlan plan;                           // The kept views' bone windows, as PlanBoneWindows plans them,
    bool planned = false;                          // unless the model's bone combos cannot be read.
};

// The views, when the client can draw every one once split by bone (SectionsFit). Otherwise the views and LOD skins
// that it can draw, in SFID order and at most four; when none can, the first four of them, which LayOutSections
// reduces. False with readError when a view's skin is not read; chosen then keeps the views unread and no plan.
bool ChooseViews(const std::vector<uint8_t>& m2, const std::vector<uint32_t>& sfid, const FileReader& read, ViewSkins& chosen, std::string& readError);

}

#endif
