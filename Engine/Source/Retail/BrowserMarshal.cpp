#include "Engine/Source/Retail/BrowserMarshal.h"
#include <algorithm>
#include <cstdio>

// The retail browser's Lua results as BrowserValue trees: search pages, the index status and CSV exports. Details
// are in BrowserDetails.cpp.

namespace Retail {

BrowserValue BrowserValue::Bool(bool value) {
    BrowserValue v;
    v.type = Type::Bool;
    v.flag = value;
    return v;
}

BrowserValue BrowserValue::Number(double value) {
    BrowserValue v;
    v.type = Type::Number;
    v.number = value;
    return v;
}

BrowserValue BrowserValue::String(std::string value) {
    BrowserValue v;
    v.type = Type::String;
    v.text = std::move(value);
    return v;
}

BrowserValue BrowserValue::Table() {
    BrowserValue v;
    v.type = Type::Table;
    return v;
}

BrowserValue& BrowserValue::Set(const std::string& key, BrowserValue value) {
    this->fields.emplace_back(key, std::move(value));
    return this->fields.back().second;
}

BrowserValue& BrowserValue::Add(BrowserValue value) {
    this->items.push_back(std::move(value));
    return this->items.back();
}

const BrowserValue* BrowserValue::Get(const std::string& key) const {
    for (const auto& [name, value] : this->fields) {
        if (name == key) {
            return &value;
        }
    }
    return nullptr;
}

const char* IndexStateName(IndexState state) {
    static const char* const names[] = { "idle", "building", "ready", "failed" };
    return names[static_cast<size_t>(state)];
}

BrowseFilters ParseBrowseFilters(const std::map<std::string, double>& filters) {
    BrowseFilters parsed;
    const std::pair<const char*, int32_t*> names[] = { { "inventoryType", &parsed.inventoryType }, { "armour", &parsed.armour },
        { "weaponSubclass", &parsed.weaponSubclass }, { "quality", &parsed.quality }, { "minItemLevel", &parsed.minItemLevel },
        { "maxItemLevel", &parsed.maxItemLevel }, { "expansion", &parsed.expansion }, { "creatureType", &parsed.creatureType },
        { "family", &parsed.family }, { "classification", &parsed.classification }, { "race", &parsed.race },
        { "humanoid", &parsed.humanoid }, { "classId", &parsed.classId }, { "minId", &parsed.minId }, { "maxId", &parsed.maxId },
        { "displayId", &parsed.displayId }, { "minCount", &parsed.minCount }, { "maxCount", &parsed.maxCount }, { "installed", &parsed.installed },
        { "setSource", &parsed.setSource }, { "modelKind", &parsed.modelKind }, { "mountType", &parsed.mountType }, { "sort", &parsed.sort },
        { "descending", &parsed.descending } };
    for (const auto& [name, field] : names) {
        auto found = filters.find(name);
        if (found != filters.end()) {
            *field = static_cast<int32_t>(found->second);
        }
    }
    return parsed;
}

// The filters of a search, with the installed displays its installed filter asks.
static BrowseFilters SearchFilters(const std::map<std::string, double>& filters, const InstalledDisplays& installed) {
    BrowseFilters parsed = ParseBrowseFilters(filters);
    parsed.installedCreature = installed.creature;
    parsed.installedItem = installed.item;
    return parsed;
}

bool BrowserNotReady(const BrowserIndexView& view, BrowserValue& out) {
    if (view.state == IndexState::Ready && view.index) {
        return false;
    }
    out = BrowserValue::Table();
    bool failed = view.state == IndexState::Failed;
    out.Set("status", BrowserValue::String(failed ? "failed" : "building"));
    if (failed) {
        out.Set("error", BrowserValue::String(view.error));
    }
    return true;
}

bool BrowserKindOf(const std::string& name, BrowseKind& kind, std::string& error) {
    if (ParseBrowseKind(name, kind)) {
        return true;
    }
    error = "unknown kind '" + name + "' (creature, item, set, model or mount)";
    return false;
}

static BrowserValue RowValue(const SearchRow& row) {
    BrowserValue value = BrowserValue::Table();
    value.Set("id", BrowserValue::Number(row.id));
    value.Set("name", BrowserValue::String(row.name));
    value.Set("detail", BrowserValue::String(row.detail));
    value.Set("displayId", BrowserValue::Number(row.displayId));
    value.Set("icon", BrowserValue::Number(row.icon));
    value.Set("quality", BrowserValue::Number(row.quality));
    return value;
}

bool MarshalSearch(const BrowserIndexView& view, const std::string& kind, const std::string& text,
    const std::map<std::string, double>& filters, uint32_t page, BrowserValue& out, std::string& error, const InstalledDisplays& installed) {
    BrowseKind parsed;
    if (!BrowserKindOf(kind, parsed, error)) {
        return false;
    }
    if (BrowserNotReady(view, out)) {
        return true;
    }
    SearchPage result = view.index->Search(parsed, text, SearchFilters(filters, installed), page ? page - 1 : 0);
    out = BrowserValue::Table();
    out.Set("status", BrowserValue::String("ok"));
    out.Set("total", BrowserValue::Number(static_cast<double>(result.total)));
    out.Set("page", BrowserValue::Number(result.page + 1));
    out.Set("pages", BrowserValue::Number(result.pages));
    BrowserValue& rows = out.Set("rows", BrowserValue::Table());
    for (const SearchRow& row : result.rows) {
        rows.Add(RowValue(row));
    }
    return true;
}

static BrowserValue NamedChoices(const std::map<uint32_t, std::string>& names) {
    BrowserValue list = BrowserValue::Table();
    for (const auto& [id, name] : names) {
        if (!name.empty()) {
            BrowserValue& choice = list.Add(BrowserValue::Table());
            choice.Set("id", BrowserValue::Number(id));
            choice.Set("name", BrowserValue::String(name));
        }
    }
    return list;
}

// The subclass names of one item class, by subclass.
static std::map<uint32_t, std::string> SubclassesOf(const RetailIndex& index, uint32_t classId) {
    std::map<uint32_t, std::string> names;
    for (const auto& [key, name] : index.SubclassNames()) {
        if (key >> 8 == classId) {
            names[key & 0xFF] = name;
        }
    }
    return names;
}

BrowserValue MarshalInfo(const BrowserIndexView& view) {
    BrowserValue info = BrowserValue::Table();
    info.Set("status", BrowserValue::String(IndexStateName(view.state)));
    if (view.state == IndexState::Failed) {
        info.Set("error", BrowserValue::String(view.error));
    }
    if (view.state != IndexState::Ready || !view.index) {
        return info;
    }
    const RetailIndex& index = *view.index;
    BrowserValue& counts = info.Set("counts", BrowserValue::Table());
    for (BrowseKind kind : { BrowseKind::Creature, BrowseKind::Item, BrowseKind::Set, BrowseKind::Model, BrowseKind::Mount }) {
        counts.Set(BrowseKindName(kind), BrowserValue::Number(static_cast<double>(index.Count(kind))));
    }
    info.Set("skippedRows", BrowserValue::Number(static_cast<double>(index.SkippedRows())));
    info.Set("creatureTypes", NamedChoices(index.CreatureTypeNames()));
    info.Set("families", NamedChoices(index.FamilyNames()));
    info.Set("weaponSubclasses", NamedChoices(SubclassesOf(index, 2)));
    return info;
}

static std::string CsvCell(const std::string& text) {
    if (text.find_first_of(",\"\n") == std::string::npos) {
        return text;
    }
    std::string quoted = "\"";
    for (char c : text) {
        quoted += c == '"' ? "\"\"" : std::string(1, c);
    }
    return quoted + "\"";
}

bool SearchCsv(const BrowserIndexView& view, const std::string& kind, const std::string& text,
    const std::map<std::string, double>& filters, std::string& csv, std::string& error, const InstalledDisplays& installed) {
    BrowseKind parsed;
    if (!BrowserKindOf(kind, parsed, error)) {
        return false;
    }
    if (view.state != IndexState::Ready || !view.index) {
        error = "the index is not ready";
        return false;
    }
    csv = "id,name,detail,displayId,icon,quality\n";
    for (const SearchRow& row : view.index->SearchAll(parsed, text, SearchFilters(filters, installed))) {
        csv += std::to_string(row.id) + "," + CsvCell(row.name) + "," + CsvCell(row.detail) + "," + std::to_string(row.displayId) + "," +
            std::to_string(row.icon) + "," + std::to_string(row.quality) + "\n";
    }
    return true;
}

}
