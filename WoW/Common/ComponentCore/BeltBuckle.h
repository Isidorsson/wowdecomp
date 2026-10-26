#ifndef WOW_COMMON_COMPONENTCORE_BELTBUCKLE_H
#define WOW_COMMON_COMPONENTCORE_BELTBUCKLE_H

#include <string>
#include "WoW/Common/ComponentCore/Types.h"

// Not in the original. Retail belts hang a rigid buckle model on the body's ATTACH_BELT_BUCKLE point (every NPC belt
// with a model is one: one file for all races, never a per-race collection). 3.3.5 belts name no model, so they are
// unchanged. The model and its skin live in Item\ObjectComponents\Waist\, as shoulders do in ...\Shoulder\.
struct BucklePaths {
    std::string model;
    std::string texture;
};

// False when the belt names no model or no skin (AddLink drops a model whose skin does not load).
bool BuckleItemPaths(const char* modelName, const char* modelTexture, BucklePaths& paths);

#endif
