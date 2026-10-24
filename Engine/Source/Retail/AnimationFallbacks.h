#ifndef ENGINE_SOURCE_RETAIL_ANIMATIONFALLBACKS_H
#define ENGINE_SOURCE_RETAIL_ANIMATIONFALLBACKS_H

#include <cstdint>
#include <map>
#include <string>
#include "Engine/Source/Retail/Db2Table.h"

namespace Retail {

// DBFilesClient/AnimationData.db2 and the WoWDBDefs layout whose physical column 0 is Fallback.
constexpr uint32_t ANIMATION_DATA_FDID = 1375431;
constexpr uint32_t ANIMATION_DATA_LAYOUT = 0xBBF66A3C;

// Highest id in the 3.3.5 AnimationData.dbc (ANIMATION_COUNT - 1 in the client).
constexpr uint32_t LEGACY_MAX_ANIMATION = 505;

// Read from the same pinned build as the models, so new expansions bring their own mapping.
class AnimationFallbacks {
    public:
    bool Load(const Db2Table& table, std::string& error);
    size_t Size() const { return this->m_fallbacks.size(); }

    // id itself when 3.3.5 knows it, else the first 3.3.5 id on its Fallback chain. 0 (Stand) when the
    // chain loops or reaches a missing row.
    uint32_t Resolve(uint32_t id) const;

    private:
    std::map<uint32_t, uint32_t> m_fallbacks;
};

}

#endif
