#include "Tools/RetailServerDisplays/ServerDisplays.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_set>

namespace ServerDisplays {

// Server columns (DBCStructure.h).
constexpr size_t DISPLAY_MODEL = 1;
constexpr size_t DISPLAY_SCALE = 4;
constexpr size_t MODEL_NAME = 2;
constexpr size_t MODEL_SCALE = 4;
constexpr size_t MODEL_COLLISION_WIDTH = 14;
constexpr size_t MODEL_COLLISION_HEIGHT = 15;
constexpr size_t MODEL_MOUNT_HEIGHT = 16;
constexpr size_t MODEL_GEO_BOX = 17;

constexpr size_t SQL_ROWS_PER_STATEMENT = 500;

static uint32_t Bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

ModelInfo ModelInfoOf(const Retail::RetailDisplay& display, const Retail::RetailCreatureModel& model) {
    float scale = display.scale * model.modelScale;
    return { display.id, 0.5f * model.collisionWidth * scale, STOCK_COMBAT_REACH * scale };
}

std::vector<uint32_t> DisplayRow(uint32_t displayId, uint32_t serverModelId, float scale) {
    std::vector<uint32_t> row(DISPLAY_FIELDS, 0);
    row[0] = displayId;
    row[DISPLAY_MODEL] = serverModelId;
    row[DISPLAY_SCALE] = Bits(scale);
    return row;
}

std::vector<uint32_t> ModelRow(uint32_t serverModelId, uint32_t nameOffset, const Retail::RetailCreatureModel& model) {
    std::vector<uint32_t> row(MODEL_FIELDS, 0);
    row[0] = serverModelId;
    row[MODEL_NAME] = nameOffset;
    row[MODEL_SCALE] = Bits(model.modelScale);
    row[MODEL_COLLISION_WIDTH] = Bits(model.collisionWidth);
    row[MODEL_COLLISION_HEIGHT] = Bits(model.collisionHeight);
    row[MODEL_MOUNT_HEIGHT] = Bits(model.mountHeight);
    for (size_t i = 0; i < model.geoBox.size(); i++) {
        row[MODEL_GEO_BOX + i] = Bits(model.geoBox[i]);
    }
    return row;
}

static bool HasFields(const Wdbc& table, uint32_t fields, const char* name, std::string& error) {
    if (table.FieldCount() == fields) {
        return true;
    }
    error = std::string(name) + " has " + std::to_string(table.FieldCount()) + " fields, the server reads " + std::to_string(fields);
    return false;
}

bool KnownLayouts(const Wdbc& displays, const Wdbc& models, std::string& error) {
    return HasFields(displays, DISPLAY_FIELDS, "CreatureDisplayInfo", error) && HasFields(models, MODEL_FIELDS, "CreatureModelData", error);
}

static std::unordered_set<uint32_t> IdsOf(const Wdbc& table) {
    std::unordered_set<uint32_t> ids;
    for (size_t row = 0; row < table.RowCount(); row++) {
        ids.insert(table.Id(row));
    }
    return ids;
}

// The placeholder of a retail model, appended on first use.
static uint32_t PlaceholderFor(Wdbc& models, const Retail::RetailCreatureModel& model, std::map<uint32_t, uint32_t>& placeholders, Added& added) {
    auto known = placeholders.find(model.id);
    if (known != placeholders.end()) {
        return known->second;
    }

    uint32_t id = models.MaxId() + 1;
    models.AppendRow(ModelRow(id, models.AddString(Retail::CreatureModelName(model.fileDataId)), model));
    placeholders[model.id] = id;
    added.models++;
    return id;
}

Added AddRetailDisplays(Wdbc& displays, Wdbc& models, const Retail::RetailDisplays& retail, std::vector<uint32_t> ids) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    std::unordered_set<uint32_t> stock = IdsOf(displays);
    std::map<uint32_t, uint32_t> placeholders;
    Added added;
    for (uint32_t id : ids) {
        const Retail::RetailDisplay* display = retail.Display(id);
        if (stock.count(id)) {
            added.stock.push_back(id);
            continue;
        }
        if (!display) {
            added.unknown.push_back(id);
            continue;
        }

        const Retail::RetailCreatureModel& model = *retail.Model(display->modelId);
        displays.AppendRow(DisplayRow(id, PlaceholderFor(models, model, placeholders, added), display->scale));
        added.displays.push_back(id);
        added.modelInfo.push_back(ModelInfoOf(*display, model));
    }
    return added;
}

std::vector<uint32_t> MissingDisplayIds(const Wdbc& displays, const Retail::RetailDisplays& retail) {
    std::unordered_set<uint32_t> stock = IdsOf(displays);
    std::vector<uint32_t> missing;
    for (const Retail::RetailDisplay& display : retail.All()) {
        if (!stock.count(display.id)) {
            missing.push_back(display.id);
        }
    }
    return missing;
}

std::vector<uint32_t> WithoutIdsOf(const Wdbc& table, const std::vector<uint32_t>& ids, std::vector<uint32_t>& excluded) {
    std::unordered_set<uint32_t> taken = IdsOf(table);
    std::vector<uint32_t> kept;
    for (uint32_t id : ids) {
        (taken.count(id) ? excluded : kept).push_back(id);
    }
    return kept;
}

static std::string Number(float value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%g", value);
    return text;
}

std::string ModelInfoSql(const std::vector<ModelInfo>& rows) {
    std::string sql;
    for (size_t i = 0; i < rows.size(); i++) {
        if (i % SQL_ROWS_PER_STATEMENT == 0) {
            sql += "REPLACE INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`) VALUES\n";
        }
        const ModelInfo& row = rows[i];
        sql += "(" + std::to_string(row.displayId) + ", " + Number(row.boundingRadius) + ", " + Number(row.combatReach) + ", 2, 0)";
        bool lastOfStatement = i + 1 == rows.size() || (i + 1) % SQL_ROWS_PER_STATEMENT == 0;
        sql += lastOfStatement ? ";\n" : ",\n";
    }
    return sql;
}

}
