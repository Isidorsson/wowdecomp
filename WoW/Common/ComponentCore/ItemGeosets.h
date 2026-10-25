#ifndef WOW_COMMON_COMPONENTCORE_ITEMGEOSETS_H
#define WOW_COMMON_COMPONENTCORE_ITEMGEOSETS_H

#include <cstdint>
#include <vector>

// Not in the original. The mesh drawn for an item's geoset (group * 100 + 1 + the item's value): the item's own if the
// model has it, else the group's default x01, which draws as without the item (the bare shin for boots, or nothing).
// Retail item displays name meshes the installed bodies lack (boots 506 to 510).
uint32_t ChooseItemGeoset(uint32_t requested, const std::vector<uint32_t>& modelGeosets);

#endif
