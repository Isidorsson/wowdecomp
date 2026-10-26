#ifndef ENGINE_SOURCE_RETAIL_RETAILINDEXJOINS_H
#define ENGINE_SOURCE_RETAIL_RETAILINDEXJOINS_H

#include <algorithm>
#include <cstdint>
#include <vector>
#include "Engine/Source/Retail/RetailIndex.h"

// What RetailIndex.cpp and RetailIndexSets.cpp share while building: the item joins and the lookups by id.

namespace Retail {

// The item facts the Item table adds to a name.
struct ItemFacts {
    uint32_t id = 0;
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
    uint32_t id = 0;
    uint32_t display = 0;
    uint32_t icon = 0;
};

// Each by id (FindById), the last row of an id kept; byItem by item id, then OrderIndex, then id.
struct ItemJoins {
    std::vector<ItemFacts> facts;
    std::vector<AppearanceRow> appearances;
    std::vector<ModifiedAppearance> modified;
    std::vector<ModifiedAppearance> byItem;
};

ItemJoins ReadItemJoins(const BrowserTables& tables);

// An item's ItemModifiedAppearance rows, in OrderIndex order (a range of joins.byItem).
struct ItemLooks {
    const ModifiedAppearance* first = nullptr;
    const ModifiedAppearance* last = nullptr;
    const ModifiedAppearance* begin() const { return this->first; }
    const ModifiedAppearance* end() const { return this->last; }
    bool empty() const { return this->first == this->last; }
};

ItemLooks LooksOf(const ItemJoins& joins, uint32_t item);

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

// Sorted by id with one row per id, the last of the rows that had it (as assigning each row to a map by id keeps).
template <typename Row>
void KeepLastById(std::vector<Row>& rows) {
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.id < b.id; });
    size_t kept = 0;
    for (size_t r = 0; r < rows.size(); r++) {
        if (r + 1 < rows.size() && rows[r + 1].id == rows[r].id) {
            continue;
        }
        rows[kept++] = rows[r];
    }
    rows.resize(kept);
}

BrowseDisplay CreatureDisplayOf(uint32_t id, float probability, const BrowserLookups& lookups);

}

#endif
