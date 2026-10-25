#include <cstdio>
#include "Engine/Source/Retail/BrowserMarshal.h"
#include "Engine/Source/Retail/ClassicStats.h"

// RetailDetails(kind, id): every id of an entry as named fields and as the labelled field list the UI shows and
// copies. A display id the installed DBC has is flagged custom: the client draws the installed row, not the retail one.

namespace Retail {

// Users of a model listed in its details; the count says how many there are in all.
constexpr size_t MAX_MODEL_USERS = 200;

namespace {

// The labelled fields of a details table: what the UI lists, "Copy field" copies (copy, else value) and "Copy all"
// joins.
class FieldList {
    public:
    explicit FieldList(BrowserValue& details) : m_details(details) {}
    // Set last: a later Set on the details table would move the list.
    ~FieldList() { this->m_details.Set("fields", std::move(this->m_fields)); }

    void Add(const std::string& label, const std::string& value, const std::string& copy = "") {
        BrowserValue& field = this->m_fields.Add(BrowserValue::Table());
        field.Set("label", BrowserValue::String(label));
        field.Set("value", BrowserValue::String(value));
        if (!copy.empty()) {
            field.Set("copy", BrowserValue::String(copy));
        }
    }

    void AddId(const std::string& label, uint32_t id) { this->Add(label, std::to_string(id)); }

    private:
    BrowserValue& m_details;
    BrowserValue m_fields = BrowserValue::Table();
};

std::string Number(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

std::string Named(const std::string& name, uint32_t id) {
    return name.empty() ? std::to_string(id) : name + " (" + std::to_string(id) + ")";
}

std::string NameIn(const std::map<uint32_t, std::string>& names, uint32_t id) {
    auto found = names.find(id);
    return found != names.end() ? found->second : std::string();
}

const char* Served(bool custom) {
    return custom ? "custom row wins" : "served";
}

bool Installed(const std::function<bool(uint32_t)>& installed, uint32_t id) {
    return id && installed && installed(id);
}

BrowserValue& Begin(BrowserValue& out, BrowseKind kind, uint32_t id, const std::string& name) {
    out = BrowserValue::Table();
    out.Set("status", BrowserValue::String("ok"));
    out.Set("kind", BrowserValue::String(BrowseKindName(kind)));
    out.Set("id", BrowserValue::Number(id));
    out.Set("name", BrowserValue::String(name));
    return out;
}

void SetNumber(BrowserValue& table, const char* key, double value) {
    table.Set(key, BrowserValue::Number(value));
}

// A creature or mount display: its table under displays and its labelled lines.
void AddDisplay(BrowserValue& displays, FieldList& fields, size_t slot, const BrowseDisplay& display, const InstalledDisplays& installed) {
    bool custom = Installed(installed.creature, display.id);
    const CreatureDisplayFacts& facts = display.facts;
    BrowserValue& value = displays.Add(BrowserValue::Table());
    SetNumber(value, "displayId", display.id);
    SetNumber(value, "probability", display.probability);
    value.Set("known", BrowserValue::Bool(display.known));
    value.Set("custom", BrowserValue::Bool(custom));
    SetNumber(value, "modelId", facts.modelId);
    SetNumber(value, "modelFdid", facts.modelFdid);
    SetNumber(value, "scale", facts.scale);
    SetNumber(value, "extraId", facts.extraId);
    SetNumber(value, "race", facts.race);
    SetNumber(value, "sex", facts.sex);
    SetNumber(value, "sdBake", facts.sdBake);
    SetNumber(value, "hdBake", facts.hdBake);

    std::string label = "Display " + std::to_string(slot + 1);
    std::string state = display.known ? Served(custom) : custom ? "custom row wins" : "not in the retail tables";
    fields.Add(label, std::to_string(display.id) + ", " + Number(display.probability * 100) + "%, " + state, std::to_string(display.id));
    if (display.known) {
        fields.Add(label + " model", "fdid " + std::to_string(facts.modelFdid) + ", model " + std::to_string(facts.modelId) + ", scale " + Number(facts.scale),
            std::to_string(facts.modelFdid));
    }
    if (facts.extraId) {
        fields.Add(label + " NPC", "Extra " + std::to_string(facts.extraId) + ", race " + std::to_string(facts.race) + ", sex " + std::to_string(facts.sex) +
            ", bakes " + std::to_string(facts.sdBake) + " / " + std::to_string(facts.hdBake), std::to_string(facts.extraId));
    }
}

void CreatureDetails(const RetailIndex& index, const BrowseCreature& c, const InstalledDisplays& installed, BrowserValue& out) {
    BrowserValue& details = Begin(out, BrowseKind::Creature, c.id, c.name);
    SetNumber(details, "creatureId", c.id);
    details.Set("title", BrowserValue::String(c.title));
    SetNumber(details, "type", c.type);
    details.Set("typeName", BrowserValue::String(NameIn(index.CreatureTypeNames(), c.type)));
    SetNumber(details, "family", c.family);
    details.Set("familyName", BrowserValue::String(NameIn(index.FamilyNames(), c.family)));
    SetNumber(details, "classification", c.classification);
    SetNumber(details, "displayId", c.displays.empty() ? 0 : c.displays[0].id);
    FieldList fields(details);
    fields.AddId("Creature id", c.id);
    fields.Add("Name", c.name);
    if (!c.title.empty()) {
        fields.Add("Title", c.title);
    }
    fields.Add("Type", Named(NameIn(index.CreatureTypeNames(), c.type), c.type), std::to_string(c.type));
    if (c.family) {
        fields.Add("Family", Named(NameIn(index.FamilyNames(), c.family), c.family), std::to_string(c.family));
    }
    fields.AddId("Classification", c.classification);
    BrowserValue& displays = details.Set("displays", BrowserValue::Table());
    for (size_t slot = 0; slot < c.displays.size(); slot++) {
        AddDisplay(displays, fields, slot, c.displays[slot], installed);
    }
}

void AddAppearance(BrowserValue& looks, FieldList& fields, size_t slot, const BrowseAppearance& look, const InstalledDisplays& installed) {
    bool custom = Installed(installed.item, look.displayId);
    BrowserValue& value = looks.Add(BrowserValue::Table());
    SetNumber(value, "modifiedAppearanceId", look.modifiedAppearanceId);
    SetNumber(value, "modifier", look.modifier);
    SetNumber(value, "appearanceId", look.appearanceId);
    SetNumber(value, "displayId", look.displayId);
    SetNumber(value, "icon", look.icon);
    value.Set("known", BrowserValue::Bool(look.known));
    value.Set("custom", BrowserValue::Bool(custom));
    BrowserValue& models = value.Set("models", BrowserValue::Table());
    for (uint32_t model : look.facts.models) {
        models.Add(BrowserValue::Number(model));
    }
    std::string label = "Appearance " + std::to_string(slot + 1);
    fields.Add(label, "IDI " + std::to_string(look.displayId) + ", appearance " + std::to_string(look.appearanceId) + ", IMA " +
        std::to_string(look.modifiedAppearanceId) + ", modifier " + std::to_string(look.modifier) + ", " + Served(custom), std::to_string(look.displayId));
    fields.Add(label + " models", std::to_string(look.facts.models[0]) + ", " + std::to_string(look.facts.models[1]), std::to_string(look.facts.models[0]));
}

std::string SocketList(const SparseItem& sparse) {
    std::string list;
    for (uint32_t socket : sparse.sockets) {
        if (socket) {
            list += (list.empty() ? "" : ", ") + std::to_string(socket);
        }
    }
    return list.empty() ? "none" : "types " + list + (sparse.socketBonus ? ", bonus enchant " + std::to_string(sparse.socketBonus) : "");
}

// ItemSparse: each stat with its budget share and its value at the item's own level, sockets and weapon fields as stored.
void AddSparse(const RetailIndex& index, const BrowseItem& item, BrowserValue& details, FieldList& fields) {
    if (!item.hasSparse) {
        fields.Add("Stats", "no ItemSparse row (still locked, or the table did not load)");
        return;
    }
    const SparseItem& sparse = item.sparse;
    StatRequest request;
    request.itemLevel = sparse.itemLevel;
    request.subclass = item.subclassId;
    std::vector<RetailStat> values = ComputeRetailStats(sparse, index.Budget(), request);
    BrowserValue& stats = details.Set("stats", BrowserValue::Table());
    size_t computed = 0;
    for (size_t slot = 0; slot < ITEM_STAT_SLOTS; slot++) {
        if (sparse.statTypes[slot] < 0 || sparse.statPercents[slot] == 0) {
            continue;
        }
        int32_t type = sparse.statTypes[slot];
        bool known = computed < values.size() && values[computed].type == type;
        BrowserValue& stat = stats.Add(BrowserValue::Table());
        SetNumber(stat, "type", type);
        stat.Set("name", BrowserValue::String(RetailStatName(type)));
        SetNumber(stat, "percent", sparse.statPercents[slot]);
        SetNumber(stat, "socketCost", sparse.socketCosts[slot]);
        char share[96];
        std::snprintf(share, sizeof(share), "%.2f%%", sparse.statPercents[slot] / 100.0);
        std::string value = RetailStatName(type) + ", " + share;
        if (known) {
            SetNumber(stat, "value", values[computed].value);
            value += ", " + std::to_string(values[computed].value) + " at item level " + std::to_string(sparse.itemLevel);
            computed++;
        }
        fields.Add("Stat " + std::to_string(stats.items.size()), value, std::to_string(type));
    }
    SetNumber(details, "sparseItemLevel", sparse.itemLevel);
    SetNumber(details, "requiredLevel", sparse.requiredLevel);
    fields.Add("Sockets", SocketList(sparse));
    if (sparse.delay) {
        fields.Add("Weapon", "delay " + std::to_string(sparse.delay) + " ms, damage variance " + Number(sparse.damageVariance) + ", damage type " +
            std::to_string(sparse.damageType), std::to_string(sparse.delay));
    }
    fields.AddId("Required level", sparse.requiredLevel);
    if (sparse.levelCurve) {
        fields.Add("Scales", "with the player's level (curve " + std::to_string(sparse.levelCurve) + ")", std::to_string(sparse.levelCurve));
    }
}

void ItemDetails(const RetailIndex& index, const BrowseItem& i, const InstalledDisplays& installed, BrowserValue& out) {
    BrowserValue& details = Begin(out, BrowseKind::Item, i.id, i.name);
    SetNumber(details, "itemId", i.id);
    SetNumber(details, "quality", i.quality);
    SetNumber(details, "itemLevel", i.itemLevel);
    SetNumber(details, "expansion", i.expansion);
    SetNumber(details, "classId", i.classId);
    SetNumber(details, "subclassId", i.subclassId);
    details.Set("subclassName", BrowserValue::String(index.SubclassName(i.classId, i.subclassId)));
    SetNumber(details, "inventoryType", i.inventoryType);
    SetNumber(details, "material", i.material);
    SetNumber(details, "sheatheType", i.sheatheType);
    SetNumber(details, "allowableClass", i.allowableClass);
    SetNumber(details, "icon", i.icon);
    SetNumber(details, "displayId", i.appearances.empty() ? 0 : i.appearances[0].displayId);
    FieldList fields(details);
    fields.AddId("Item id", i.id);
    fields.Add("Name", i.name);
    fields.AddId("Quality", i.quality);
    fields.AddId("Item level", i.itemLevel);
    fields.AddId("Inventory type", i.inventoryType);
    fields.Add("Class / subclass", std::to_string(i.classId) + " / " + std::to_string(i.subclassId) + " " + index.SubclassName(i.classId, i.subclassId),
        std::to_string(i.subclassId));
    fields.AddId("Material", i.material);
    fields.AddId("Allowable classes", i.allowableClass);
    fields.AddId("Expansion", i.expansion);
    fields.AddId("Icon fdid", i.icon);
    AddSparse(index, i, details, fields);
    BrowserValue& looks = details.Set("appearances", BrowserValue::Table());
    for (size_t slot = 0; slot < i.appearances.size(); slot++) {
        AddAppearance(looks, fields, slot, i.appearances[slot], installed);
    }
}

void SetDetails(const RetailIndex& index, const BrowseSet& s, const InstalledDisplays& installed, BrowserValue& out) {
    BrowserValue& details = Begin(out, BrowseKind::Set, s.key, s.name);
    SetNumber(details, "setId", s.setId);
    details.Set("source", BrowserValue::String(s.source == SetSource::ItemSet ? "itemset" : "transmog"));
    SetNumber(details, "classMask", s.classMask);
    SetNumber(details, "expansion", s.expansion);
    SetNumber(details, "armour", s.armour);
    FieldList fields(details);
    fields.AddId(s.source == SetSource::ItemSet ? "ItemSet id" : "TransmogSet id", s.setId);
    fields.Add("Name", s.name);
    fields.AddId("Class mask", s.classMask);
    fields.AddId("Armour type", s.armour);
    fields.AddId("Expansion", s.expansion);
    BrowserValue& pieces = details.Set("pieces", BrowserValue::Table());
    for (size_t slot = 0; slot < s.pieces.size(); slot++) {
        const SetPiece& piece = s.pieces[slot];
        bool custom = Installed(installed.item, piece.displayId);
        BrowserValue& value = pieces.Add(BrowserValue::Table());
        SetNumber(value, "itemId", piece.itemId);
        SetNumber(value, "modifiedAppearanceId", piece.modifiedAppearanceId);
        SetNumber(value, "displayId", piece.displayId);
        SetNumber(value, "inventoryType", piece.inventoryType);
        value.Set("custom", BrowserValue::Bool(custom));
        const BrowseItem* item = index.Item(piece.itemId);
        value.Set("name", BrowserValue::String(item ? item->name : ""));
        fields.Add("Piece " + std::to_string(slot + 1), "item " + std::to_string(piece.itemId) + ", IDI " + std::to_string(piece.displayId) +
            ", slot " + std::to_string(piece.inventoryType) + ", " + Served(custom), std::to_string(piece.itemId));
    }
}

void ModelDetails(const BrowseModel& m, BrowserValue& out) {
    BrowserValue& details = Begin(out, BrowseKind::Model, m.fdid, m.users.empty() ? std::to_string(m.fdid) : m.users[0].name);
    SetNumber(details, "fdid", m.fdid);
    details.Set("creature", BrowserValue::Bool(m.creature));
    SetNumber(details, "userCount", static_cast<double>(m.users.size()));
    SetNumber(details, "displayId", m.users.empty() ? 0 : m.users[0].displayId);
    FieldList fields(details);
    fields.AddId("Model fdid", m.fdid);
    fields.Add("Kind", m.creature ? "Creature model" : "Item model");
    fields.Add("Users", std::to_string(m.users.size()));
    BrowserValue& users = details.Set("users", BrowserValue::Table());
    for (size_t slot = 0; slot < m.users.size() && slot < MAX_MODEL_USERS; slot++) {
        const ModelUser& user = m.users[slot];
        BrowserValue& value = users.Add(BrowserValue::Table());
        value.Set("kind", BrowserValue::String(BrowseKindName(user.kind)));
        SetNumber(value, "id", user.id);
        SetNumber(value, "displayId", user.displayId);
        value.Set("name", BrowserValue::String(user.name));
        fields.Add("User " + std::to_string(slot + 1), std::string(BrowseKindName(user.kind)) + " " + std::to_string(user.id) + " " + user.name +
            ", display " + std::to_string(user.displayId), std::to_string(user.id));
    }
}

void MountDetails(const BrowseMount& m, const InstalledDisplays& installed, BrowserValue& out) {
    BrowserValue& details = Begin(out, BrowseKind::Mount, m.id, m.name);
    SetNumber(details, "mountId", m.id);
    SetNumber(details, "spellId", m.spellId);
    SetNumber(details, "mountType", m.mountType);
    SetNumber(details, "displayId", m.displays.empty() ? 0 : m.displays[0].id);
    FieldList fields(details);
    fields.AddId("Mount id", m.id);
    fields.Add("Name", m.name);
    fields.AddId("Spell id", m.spellId);
    fields.AddId("Mount type", m.mountType);
    BrowserValue& displays = details.Set("displays", BrowserValue::Table());
    for (size_t slot = 0; slot < m.displays.size(); slot++) {
        AddDisplay(displays, fields, slot, m.displays[slot], installed);
    }
}

}

bool MarshalDetails(const BrowserIndexView& view, const std::string& kind, uint32_t id, const InstalledDisplays& installed,
    BrowserValue& out, std::string& error) {
    BrowseKind parsed;
    if (!BrowserKindOf(kind, parsed, error)) {
        return false;
    }
    if (BrowserNotReady(view, out)) {
        return true;
    }
    const RetailIndex& index = *view.index;
    bool found = true;
    if (parsed == BrowseKind::Creature) {
        const BrowseCreature* entry = index.Creature(id);
        found = entry && (CreatureDetails(index, *entry, installed, out), true);
    } else if (parsed == BrowseKind::Item) {
        const BrowseItem* entry = index.Item(id);
        found = entry && (ItemDetails(index, *entry, installed, out), true);
    } else if (parsed == BrowseKind::Set) {
        const BrowseSet* entry = index.Set(id);
        found = entry && (SetDetails(index, *entry, installed, out), true);
    } else if (parsed == BrowseKind::Model) {
        const BrowseModel* entry = index.Model(id);
        found = entry && (ModelDetails(*entry, out), true);
    } else {
        const BrowseMount* entry = index.Mount(id);
        found = entry && (MountDetails(*entry, installed, out), true);
    }
    if (!found) {
        error = "no " + kind + " " + std::to_string(id);
    }
    return found;
}

}
