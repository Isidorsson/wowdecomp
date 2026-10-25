#ifndef ENGINE_SOURCE_RETAIL_RETAILINDEXJOINS_H
#define ENGINE_SOURCE_RETAIL_RETAILINDEXJOINS_H

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "Engine/Source/Retail/RetailIndex.h"

// What RetailIndex.cpp and RetailIndexSets.cpp share while building: the item joins and the lookups by id.

namespace Retail {

// The item facts the Item table adds to a name.
struct ItemFacts {
    uint32_t classId = 0;
    uint32_t subclassId = 0;
    uint32_t material = 0;
    uint32_t inventoryType = 0;
    uint32_t icon = 0;
    uint32_t sheatheType = 0;
};

struct ModifiedAppearance {
    uint32_t id = 0;
    uint32_t item = 0;
    uint32_t modifier = 0;
    uint32_t appearance = 0;
    uint32_t order = 0;
};

struct AppearanceRow {
    uint32_t display = 0;
    uint32_t icon = 0;
};

struct ItemJoins {
    std::unordered_map<uint32_t, ItemFacts> facts;                          // By item id.
    std::unordered_map<uint32_t, AppearanceRow> appearances;                // By ItemAppearance id.
    std::unordered_map<uint32_t, ModifiedAppearance> modified;              // By ItemModifiedAppearance id.
    std::unordered_map<uint32_t, std::vector<ModifiedAppearance>> ofItem;   // By item id, in OrderIndex order.
};

ItemJoins ReadItemJoins(const BrowserTables& tables);

template <typename Row, typename KeyOf>
const Row* FindByKey(const std::vector<Row>& rows, uint32_t key, KeyOf keyOf) {
    auto row = std::lower_bound(rows.begin(), rows.end(), key, [&](const Row& r, uint32_t k) { return keyOf(r) < k; });
    return row != rows.end() && keyOf(*row) == key ? &*row : nullptr;
}

template <typename Row>
const Row* FindById(const std::vector<Row>& rows, uint32_t id) {
    return FindByKey(rows, id, [](const Row& r) { return r.id; });
}

template <typename Row>
void SortById(std::vector<Row>& rows) {
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.id < b.id; });
}

BrowseDisplay CreatureDisplayOf(uint32_t id, float probability, const BrowserLookups& lookups);

}

#endif
