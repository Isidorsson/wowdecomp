#include "WoW/Common/ComponentCore/BeltBuckle.h"

static const char* const WAIST_FOLDER = "Item\\ObjectComponents\\Waist\\";

bool BuckleItemPaths(const char* modelName, const char* modelTexture, BucklePaths& paths) {
    if (!modelName || !*modelName || !modelTexture || !*modelTexture) {
        return false;
    }

    paths.model = std::string(WAIST_FOLDER) + modelName;
    paths.texture = std::string(WAIST_FOLDER) + modelTexture + ".blp";
    return true;
}
