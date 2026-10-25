#include "WoW/Common/ComponentCore/ItemGeosets.h"
#include <algorithm>

static const uint32_t GEOSET_GROUP_SIZE = 100;

uint32_t ChooseItemGeoset(uint32_t requested, const std::vector<uint32_t>& modelGeosets) {
    if (std::find(modelGeosets.begin(), modelGeosets.end(), requested) != modelGeosets.end()) {
        return requested;
    }
    return requested / GEOSET_GROUP_SIZE * GEOSET_GROUP_SIZE + 1;
}
