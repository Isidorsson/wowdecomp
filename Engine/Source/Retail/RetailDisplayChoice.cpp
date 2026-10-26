#include "Engine/Source/Retail/RetailDisplayChoice.h"

namespace Retail {

bool RetailDisplayWins(CreatureDisplays mode, uint32_t id, StockDisplayKind kind, const RetailDisplay* retail) {
    if (mode == CreatureDisplays::Stock || kind == StockDisplayKind::PlayerBody || !retail || id == 0 || id > STOCK_CREATURE_DISPLAY_MAX) {
        return false;
    }
    bool humanoid = kind == StockDisplayKind::NpcHumanoid || retail->extendedDisplayId != 0;
    return mode == CreatureDisplays::All || !humanoid;
}

DisplayRow OverlayRetailDisplay(const DisplayRow& stock, const RetailDisplay& retail) {
    DisplayRow row = stock;
    row.modelId = static_cast<int32_t>(SyntheticModelId(retail.modelId));
    row.extendedDisplayId = 0;
    row.scale = retail.scale;
    row.alpha = static_cast<int32_t>(retail.alpha);
    for (size_t i = 0; i < row.variations.size(); i++) {
        row.variations[i] = VariationName(retail.variations[i]);
    }
    row.fourthVariation = VariationName(retail.fourthVariation);
    row.geosetData = static_cast<int32_t>(retail.geosetData);
    return row;
}

}
