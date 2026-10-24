#include "Engine/Source/Retail/AnimationFallbacks.h"
#include <cstdio>

namespace Retail {

static constexpr size_t FALLBACK_COLUMN = 0;

bool AnimationFallbacks::Load(const Db2Table& table, std::string& error) {
    if (table.LayoutHash() != ANIMATION_DATA_LAYOUT) {
        char message[128];
        std::snprintf(message, sizeof(message), "AnimationData layout %08X is not the known %08X (a newer build?)", table.LayoutHash(), ANIMATION_DATA_LAYOUT);
        error = message;
        return false;
    }

    this->m_fallbacks.clear();

    for (size_t row = 0; row < table.RowCount(); row++) {
        this->m_fallbacks[table.RowId(row)] = table.Value(row, FALLBACK_COLUMN);
    }

    return true;
}

uint32_t AnimationFallbacks::Resolve(uint32_t id) const {
    // Each step visits a new row, so a chain longer than the table must have looped.
    for (size_t steps = 0; steps <= this->m_fallbacks.size(); steps++) {
        if (id <= LEGACY_MAX_ANIMATION) {
            return id;
        }

        auto fallback = this->m_fallbacks.find(id);
        if (fallback == this->m_fallbacks.end()) {
            return 0;
        }

        id = fallback->second;
    }

    return 0;
}

}
