#include "Engine/Source/Retail/CreatureSkins.h"
#include <algorithm>
#include <cstdio>
#include <iterator>

namespace Retail {

// Physical columns of the layouts (WoWDBDefs, as wow-toolbox-editor casc/skins.rs reads them).
constexpr size_t MODEL_DATA_FILE_DATA_ID = 2;
constexpr size_t DISPLAY_MODEL_ID = 1;
constexpr size_t DISPLAY_TEXTURE_VARIATIONS = 27;

// The M2 texture types the variations fill, in order. Retail's fourth fills type 5.
constexpr uint32_t SKIN_TYPES[] = { 11, 12, 13, 5 };

static bool KnownLayout(const Db2Table& table, uint32_t layout, const char* name, std::string& error) {
    if (table.LayoutHash() == layout) {
        return true;
    }

    char message[128];
    std::snprintf(message, sizeof(message), "%s layout %08X is not the known %08X (a newer build?)", name, table.LayoutHash(), layout);
    error = message;
    return false;
}

static std::vector<SkinTexture> Variations(const Db2Table& displayInfo, size_t row) {
    std::vector<uint32_t> files = displayInfo.Values(row, DISPLAY_TEXTURE_VARIATIONS);
    std::vector<SkinTexture> textures;
    for (size_t i = 0; i < files.size() && i < std::size(SKIN_TYPES); i++) {
        if (files[i]) {
            textures.push_back({ SKIN_TYPES[i], files[i] });
        }
    }
    return textures;
}

bool CreatureSkins::Load(const Db2Table& modelData, const Db2Table& displayInfo, std::string& error) {
    if (!KnownLayout(modelData, CREATURE_MODEL_DATA_LAYOUT, "CreatureModelData", error) || !KnownLayout(displayInfo, CREATURE_DISPLAY_INFO_LAYOUT, "CreatureDisplayInfo", error)) {
        return false;
    }

    std::map<uint32_t, uint32_t> fileOf;
    for (size_t row = 0; row < modelData.RowCount(); row++) {
        fileOf[modelData.RowId(row)] = modelData.Value(row, MODEL_DATA_FILE_DATA_ID);
    }

    // Every distinct skin of each model, with the displays showing it.
    std::map<uint32_t, std::vector<CreatureSkin>> skins;
    for (size_t row = 0; row < displayInfo.RowCount(); row++) {
        auto model = fileOf.find(displayInfo.Value(row, DISPLAY_MODEL_ID));
        std::vector<SkinTexture> textures = Variations(displayInfo, row);
        if (model == fileOf.end() || textures.empty()) {
            continue;
        }

        uint32_t display = displayInfo.RowId(row);
        std::vector<CreatureSkin>& variants = skins[model->second];
        auto same = std::find_if(variants.begin(), variants.end(), [&](const CreatureSkin& skin) { return skin.textures == textures; });
        if (same == variants.end()) {
            variants.push_back({ display, 1, 0, textures });
        } else {
            same->displays++;
            same->displayId = std::min(same->displayId, display);
        }
    }

    this->m_defaults.clear();
    for (const auto& [model, variants] : skins) {
        CreatureSkin best = *std::min_element(variants.begin(), variants.end(), [](const CreatureSkin& a, const CreatureSkin& b) {
            return a.displays != b.displays ? a.displays > b.displays : a.displayId < b.displayId;
        });
        best.others = static_cast<uint32_t>(variants.size() - 1);
        this->m_defaults[model] = best;
    }

    return true;
}

const CreatureSkin* CreatureSkins::Default(uint32_t modelFileDataId) const {
    auto skin = this->m_defaults.find(modelFileDataId);
    return skin == this->m_defaults.end() ? nullptr : &skin->second;
}

}
