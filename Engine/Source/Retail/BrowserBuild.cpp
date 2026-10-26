#include <cmath>
#include <cstdio>
#include <map>
#include "Engine/Source/Retail/BrowserMarshal.h"
#include "Engine/Source/Retail/BuildSql.h"

// RetailBuildSql(kind, id, options): the SQL builders (BuildSql.h) fed from the index. Items take their stats from
// ItemSparse at the chosen item level (StatBudget.h) mapped to 3.3.5 (ClassicStats.h); a set writes one item per piece;
// a creature copies a base creature with the retail displays.

namespace Retail {

constexpr double DEFAULT_ITEM_LEVEL = 200;
constexpr uint32_t MAX_CLASSIC_QUALITY = 7;   // Heirloom.
constexpr uint32_t INVENTORY_CHEST = 5;
constexpr uint32_t INVENTORY_ROBE = 20;
constexpr size_t CREATURE_MODELS = 4;

namespace {

double NumberOption(const BuildOptions& options, const char* key, double fallback) {
    auto found = options.numbers.find(key);
    return found != options.numbers.end() ? found->second : fallback;
}

const std::string* TextOption(const BuildOptions& options, const char* key) {
    auto found = options.texts.find(key);
    return found != options.texts.end() ? &found->second : nullptr;
}

uint32_t Unsigned(double value) {
    return value > 0 ? static_cast<uint32_t>(value) : 0;
}

// What one build collects: the statements, the stat lines and the notes, and the next free entry.
struct Build {
    const RetailIndex& index;
    const BuildOptions& options;
    uint32_t itemLevel;
    uint32_t nextEntry;
    std::string sql;
    BrowserValue lines = BrowserValue::Table();
    std::vector<std::string> notes;
    uint32_t pieces = 0;
    uint32_t base = 0;

    void Note(const std::string& note) {
        for (const std::string& known : this->notes) {
            if (known == note) {
                return;
            }
        }
        this->notes.push_back(note);
    }
};

std::string ClassicTarget(const StatLine& line) {
    if (line.classicType >= 0) {
        return ClassicStatName(line.classicType);
    }
    return line.resistance >= 0 ? ResistanceColumn(static_cast<Resistance>(line.resistance)) : "dropped";
}

void AddLines(Build& build, const std::string& piece, const ClassicStats& stats) {
    for (const StatLine& line : stats.lines) {
        BrowserValue& value = build.lines.Add(BrowserValue::Table());
        value.Set("piece", BrowserValue::String(piece));
        value.Set("retail", BrowserValue::String(RetailStatName(line.retailType)));
        value.Set("retailValue", BrowserValue::Number(line.retailValue));
        value.Set("classic", BrowserValue::String(ClassicTarget(line)));
        value.Set("value", BrowserValue::Number(line.value));
        value.Set("note", BrowserValue::String(line.note));
    }
}

// A retail armour or damage column, applied to the row or not (the base's stays).
struct CombatLine {
    const char* retail;
    double value;
    const char* classic;
    std::string note;
};

void AddCombatLines(Build& build, const std::string& piece, const std::vector<CombatLine>& lines, bool applied) {
    for (const CombatLine& line : lines) {
        BrowserValue& value = build.lines.Add(BrowserValue::Table());
        value.Set("piece", BrowserValue::String(piece));
        value.Set("retail", BrowserValue::String(line.retail));
        value.Set("retailValue", BrowserValue::Number(line.value));
        value.Set("classic", BrowserValue::String(line.classic));
        value.Set("value", BrowserValue::Number(line.value));
        value.Set("applied", BrowserValue::Number(applied ? 1 : 0));
        value.Set("note", BrowserValue::String(applied ? line.note : "not applied: the base's stays (Retail armour/damage is off)"));
    }
}

// Retail armour and weapon damage at the item level (ItemArmour.h), set on the row unless retailArmour is 0.
std::vector<CombatLine> AddCombat(Build& build, const BrowseItem& item, ItemSql& row, bool apply) {
    const ArmourTables& tables = build.index.Budget().armour;
    SparseItem sparse;
    if (!build.index.SparseOf(item, sparse) || !tables.Ready()) {
        build.Note("no retail armour and damage tables: armor, damage and delay stay the base's");
        return {};
    }
    sparse.quality = item.quality;
    ItemClass kind{ item.classId, item.subclassId };
    std::string level = "retail at item level " + std::to_string(build.itemLevel);
    std::vector<CombatLine> lines;
    uint32_t armour = ComputeRetailArmour(tables, sparse, kind, build.itemLevel);
    if (armour) {
        row.armor = apply ? static_cast<int32_t>(armour) : -1;
        lines.push_back({ "Armour", static_cast<double>(armour), "armor", level });
    }
    RetailDamage damage = ComputeRetailDamage(tables, sparse, kind, build.itemLevel);
    if (damage.max) {
        row.weapon = apply;
        row.damageMin = damage.min;
        row.damageMax = damage.max;
        row.damageType = sparse.damageType;
        row.delay = sparse.delay;
        char dps[96];
        std::snprintf(dps, sizeof(dps), "%s, %.1f dps (ItemDelay %.2f s)", level.c_str(), (damage.min + damage.max) * 500.0 / sparse.delay,
            sparse.delay / 1000.0);
        lines.push_back({ "Damage min", static_cast<double>(damage.min), "dmg_min1", dps });
        lines.push_back({ "Damage max", static_cast<double>(damage.max), "dmg_max1", dps });
        lines.push_back({ "Damage type", static_cast<double>(sparse.damageType), "dmg_type1", "the retail school; the second damage is cleared" });
        lines.push_back({ "Delay", static_cast<double>(sparse.delay), "delay", "ItemSparse ItemDelay, ms" });
    }
    if (apply && !lines.empty()) {
        build.Note("retail armour and damage are retail-sized, far below 3.3.5's at the same item level: compare them with the base's in "
            "the mapping before using them, or turn Retail armour/damage off");
    }
    return lines;
}

void NoteScaling(Build& build, const BrowseItem& item) {
    SparseItem sparse;
    std::string how = build.index.SparseOf(item, sparse) ? LevelScaling(sparse) : std::string();
    if (!how.empty()) {
        build.Note(std::string(item.name) + " scales with the player's level in retail (" + how + "): its stats, armour and damage are computed at item level " +
            std::to_string(build.itemLevel) + " instead");
    }
}

bool HasAllocations(const SparseItem& sparse) {
    for (size_t slot = 0; slot < ITEM_STAT_SLOTS; slot++) {
        if (sparse.statTypes[slot] >= 0 && sparse.statPercents[slot] != 0) {
            return true;
        }
    }
    return false;
}

bool HasAllocations(const RetailIndex& index, uint32_t itemId) {
    const BrowseItem* item = index.Item(itemId);
    SparseItem sparse;
    return item && index.SparseOf(*item, sparse) && HasAllocations(sparse);
}

ClassicStats StatsOf(Build& build, const BrowseItem& item) {
    SparseItem sparse;
    if (!build.index.SparseOf(item, sparse)) {
        build.Note(std::string(item.name) + ": no ItemSparse row, so no stats");
        return {};
    }
    if (!HasAllocations(sparse)) {
        build.Note(std::string(item.name) + " (" + std::to_string(item.id) + "): ItemSparse lists no stat allocations, so no stats");
        return {};
    }
    StatRequest request;
    request.itemLevel = build.itemLevel;
    request.subclass = item.subclassId;
    request.retailStamina = NumberOption(build.options, "retailStamina", 0) != 0;
    sparse.quality = item.quality;
    std::vector<RetailStat> stats = ComputeRetailStats(sparse, build.index.Budget(), request);
    if (stats.empty() && build.index.Budget().randProp.find(build.itemLevel) == build.index.Budget().randProp.end()) {
        build.Note("no retail stat budget at item level " + std::to_string(build.itemLevel) + " (RandPropPoints has 1 to " +
            std::to_string(build.index.Budget().MaxItemLevel()) + "): no stats");
    }
    for (const RetailStat& stat : stats) {
        if (stat.type == 7 && stat.multiplier != 1) {
            char note[160];
            std::snprintf(note, sizeof(note), "stamina includes retail's StaminaMultByILvl x%.2f at item level %u (retailStamina = 0 leaves it out)",
                stat.multiplier, build.itemLevel);
            build.Note(note);
        }
    }
    auto combo = static_cast<ComboPrimary>(Unsigned(NumberOption(build.options, "combo", 0)) & 3);
    return MapToClassic(stats, combo);
}

uint32_t DisplayOf(Build& build, const BrowseItem& item) {
    size_t slot = Unsigned(NumberOption(build.options, "appearance", 1));
    slot = slot ? slot - 1 : 0;
    if (slot >= item.appearances.size()) {
        build.Note(std::string(item.name) + ": no appearance " + std::to_string(slot + 1) + ", displayid 0");
        return 0;
    }
    return item.appearances[slot].displayId;
}

void AddSockets(Build& build, const BrowseItem& item, ItemSql& row) {
    SparseItem sparse;
    if (!build.index.SparseOf(item, sparse)) {
        return;
    }
    for (size_t s = 0; s < ITEM_SOCKETS; s++) {
        row.socketColors[s] = ClassicSocketColor(sparse.sockets[s]);
        if (sparse.sockets[s] && !row.socketColors[s]) {
            build.Note(std::string(item.name) + ": retail socket type " + std::to_string(sparse.sockets[s]) + " has no 3.3.5 colour, left empty");
        }
    }
    if (sparse.socketBonus) {
        build.Note("retail socket bonuses are retail enchantments: socketBonus is 0");
    }
}

// One item row: the base is the option for a lone item, else the stock item suggested for its slot.
bool AddItem(Build& build, const BrowseItem& item, uint32_t displayId, bool lone, std::string& error) {
    ItemSql row;
    row.classId = item.classId;
    row.subclass = item.subclassId;
    std::string classNote;
    ClassicItemClass(row.classId, row.subclass, classNote);
    if (!classNote.empty()) {
        build.Note(std::string(item.name) + ": " + classNote);
    }
    uint32_t chosen = lone ? Unsigned(NumberOption(build.options, "baseEntry", 0)) : 0;
    row.base = chosen ? chosen : SuggestedBaseItem(row.classId, row.subclass, item.inventoryType);
    if (!row.base) {
        error = "no stock 3.3.5 item of class " + std::to_string(row.classId) + ", subclass " + std::to_string(row.subclass) + ", slot " +
            std::to_string(item.inventoryType) + " to copy for " + std::string(item.name) + ": enter a base entry";
        return false;
    }
    const std::string* name = lone ? TextOption(build.options, "name") : nullptr;
    row.entry = build.nextEntry++;
    row.name = name && !name->empty() ? *name : std::string(item.name);
    row.displayId = displayId;
    row.quality = item.quality > MAX_CLASSIC_QUALITY ? MAX_CLASSIC_QUALITY : item.quality;
    row.inventoryType = item.inventoryType;
    row.itemLevel = build.itemLevel;
    row.requiredLevel = static_cast<int32_t>(NumberOption(build.options, "requiredLevel", -1));
    row.stats = StatsOf(build, item);
    AddSockets(build, item, row);
    bool retailCombat = NumberOption(build.options, "retailArmour", 1) != 0;
    std::vector<CombatLine> combat = AddCombat(build, item, row, retailCombat);
    NoteScaling(build, item);
    row.comment = "retail item " + std::to_string(item.id) + " " + std::string(item.name) + " at item level " + std::to_string(build.itemLevel) +
        ", copied from item " + std::to_string(row.base);
    std::string sql = BuildItemSql(row, error);
    if (sql.empty()) {
        return false;
    }
    build.sql += sql + "\n";
    build.base = row.base;
    build.pieces++;
    AddLines(build, std::string(item.name), row.stats);
    AddCombatLines(build, std::string(item.name), combat, retailCombat);
    return true;
}

uint32_t SlotOf(uint32_t inventoryType) {
    return inventoryType == INVENTORY_ROBE ? INVENTORY_CHEST : inventoryType;
}

// One piece per slot, in the order the slots first come: the first with stat allocations, else the first. Sets list
// stat-less transmog copies of their pieces too.
std::vector<const SetPiece*> PiecesPerSlot(const Build& build, const BrowseSet& set) {
    std::vector<uint32_t> order;
    std::map<uint32_t, const SetPiece*> chosen;
    for (const SetPiece& piece : set.pieces) {
        uint32_t slot = SlotOf(piece.inventoryType);
        auto known = chosen.find(slot);
        if (known == chosen.end()) {
            order.push_back(slot);
            chosen[slot] = &piece;
        } else if (HasAllocations(build.index, piece.itemId) && !HasAllocations(build.index, known->second->itemId)) {
            known->second = &piece;
        }
    }
    std::vector<const SetPiece*> pieces;
    for (uint32_t slot : order) {
        pieces.push_back(chosen[slot]);
    }
    return pieces;
}

bool AddSet(Build& build, const BrowseSet& set, std::string& error) {
    std::vector<const SetPiece*> pieces;
    if (NumberOption(build.options, "allVariants", 0) != 0) {
        for (const SetPiece& piece : set.pieces) {
            pieces.push_back(&piece);
        }
    } else {
        pieces = PiecesPerSlot(build, set);
    }
    for (const SetPiece* piece : pieces) {
        const BrowseItem* item = build.index.Item(piece->itemId);
        if (!item) {
            build.Note("piece item " + std::to_string(piece->itemId) + " is not indexed: skipped");
            continue;
        }
        if (!AddItem(build, *item, piece->displayId, false, error)) {
            return false;
        }
    }
    build.Note("each piece copies the stock item suggested for its slot; no ItemSet row or set bonus is written");
    return true;
}

// modelid1 is the picked display (else the first); with otherDisplays the creature's other displays follow.
std::vector<uint32_t> CreatureModels(const Build& build, const BrowseCreature& creature) {
    std::vector<uint32_t> models;
    uint32_t picked = Unsigned(NumberOption(build.options, "display", 0));
    if (picked) {
        models.push_back(picked);
    }
    bool others = NumberOption(build.options, "otherDisplays", 0) != 0;
    for (const BrowseDisplay& display : creature.displays) {
        if (models.size() < CREATURE_MODELS && (models.empty() || (others && display.id != models[0]))) {
            models.push_back(display.id);
        }
    }
    return models;
}

bool AddCreature(Build& build, const BrowseCreature& creature, std::string& error) {
    std::vector<uint32_t> models = CreatureModels(build, creature);
    if (models.empty()) {
        error = std::string(creature.name) + " has no display";
        return false;
    }
    CreatureSql row;
    row.entry = build.nextEntry++;
    uint32_t chosen = Unsigned(NumberOption(build.options, "baseEntry", 0));
    row.base = chosen ? chosen : DEFAULT_BASE_CREATURE;
    const std::string* name = TextOption(build.options, "name");
    const std::string* subname = TextOption(build.options, "subname");
    row.name = name && !name->empty() ? *name : std::string(creature.name);
    row.subname = subname ? *subname : std::string(creature.title);
    for (size_t slot = 0; slot < models.size(); slot++) {
        row.models[slot] = models[slot];
    }
    row.scale = static_cast<float>(NumberOption(build.options, "scale", 0));
    row.comment = "retail creature " + std::to_string(creature.id) + " " + std::string(creature.name) + ", copied from creature " + std::to_string(row.base) +
        "; creature_model_info already has rows for the retail displays";
    std::string sql = BuildCreatureSql(row, error);
    if (sql.empty()) {
        return false;
    }
    build.sql += sql + "\n";
    build.base = row.base;
    build.pieces = 1;
    build.Note("level, faction, flags, AI and loot come from the base creature " + std::to_string(row.base) + "; spawn it with .npc add " +
        std::to_string(row.entry));
    return true;
}

void Finish(Build& build, const std::string& kind, uint32_t first, BrowserValue& out) {
    out = BrowserValue::Table();
    out.Set("status", BrowserValue::String("ok"));
    out.Set("kind", BrowserValue::String(kind));
    out.Set("sql", BrowserValue::String(build.sql));
    out.Set("firstEntry", BrowserValue::Number(first));
    out.Set("nextEntry", BrowserValue::Number(build.nextEntry));
    out.Set("itemLevel", BrowserValue::Number(build.itemLevel));
    out.Set("base", BrowserValue::Number(build.base));
    out.Set("pieces", BrowserValue::Number(build.pieces));
    out.Set("lines", std::move(build.lines));
    BrowserValue& notes = out.Set("notes", BrowserValue::Table());
    for (const std::string& note : build.notes) {
        notes.Add(BrowserValue::String(note));
    }
}

bool BuildEntry(Build& build, BrowseKind kind, uint32_t id, std::string& error) {
    const RetailIndex& index = build.index;
    if (kind == BrowseKind::Item) {
        const BrowseItem* item = index.Item(id);
        if (!item || !AddItem(build, *item, DisplayOf(build, *item), true, error)) {
            return false;
        }
        bool retailCombat = NumberOption(build.options, "retailArmour", 1) != 0;
        build.Note(std::string(retailCombat ? "" : "damage, speed, armour, ") + "bonding, flags, spells and price come from the base item " +
            std::to_string(build.base));
        return true;
    }
    if (kind == BrowseKind::Set) {
        const BrowseSet* set = index.Set(id);
        return set && AddSet(build, *set, error);
    }
    const BrowseCreature* creature = index.Creature(id);
    return creature && AddCreature(build, *creature, error);
}

}

bool MarshalBuild(const BrowserIndexView& view, const std::string& kind, uint32_t id, const BuildOptions& options, BrowserValue& out,
    std::string& error) {
    BrowseKind parsed;
    if (!BrowserKindOf(kind, parsed, error)) {
        return false;
    }
    if (parsed != BrowseKind::Item && parsed != BrowseKind::Set && parsed != BrowseKind::Creature) {
        error = "SQL is built for items, sets and creatures, not " + kind;
        return false;
    }
    if (BrowserNotReady(view, out)) {
        return true;
    }
    uint32_t first = Unsigned(NumberOption(options, "startEntry", DEFAULT_START_ENTRY));
    Build build{ *view.index, options, Unsigned(std::floor(NumberOption(options, "itemLevel", DEFAULT_ITEM_LEVEL))), first };
    error.clear();
    if (!BuildEntry(build, parsed, id, error)) {
        if (error.empty()) {
            error = "no " + kind + " " + std::to_string(id);
        }
        return false;
    }
    Finish(build, kind, first, out);
    return true;
}

}
