#include "Engine/Source/Retail/CreatureTables.h"
#include <algorithm>
#include <cstdio>

namespace Retail {

// Physical columns of the CreatureDisplayInfoGeosetData layout.
constexpr size_t GEOSET_INDEX = 0;
constexpr size_t GEOSET_VALUE = 1;

bool KnownLayout(const Db2Table& table, uint32_t layout, const char* name, std::string& error) {
    if (table.LayoutHash() == layout) {
        return true;
    }

    char message[128];
    std::snprintf(message, sizeof(message), "%s layout %08X is not the known %08X (a newer build?)", name, table.LayoutHash(), layout);
    error = message;
    return false;
}

std::map<uint32_t, std::vector<Geoset>> GeosetsByDisplay(const Db2Table& geosets) {
    std::map<uint32_t, std::vector<Geoset>> byDisplay;
    for (size_t row = 0; row < geosets.RowCount(); row++) {
        if (uint32_t display = geosets.Relation(row)) {
            byDisplay[display].push_back({ geosets.Value(row, GEOSET_INDEX), geosets.Value(row, GEOSET_VALUE) });
        }
    }

    for (auto& [display, shown] : byDisplay) {
        std::sort(shown.begin(), shown.end());
        shown.erase(std::unique(shown.begin(), shown.end()), shown.end());
    }
    return byDisplay;
}

}
