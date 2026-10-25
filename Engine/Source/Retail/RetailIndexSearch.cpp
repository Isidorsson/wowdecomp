#include <algorithm>
#include <cstring>
#include "Engine/Source/Retail/RetailIndex.h"

// The retail browser index's search: normalized text, ranking, filters and the rows a page shows.

namespace Retail {

static const char* const KIND_NAMES[] = { "creature", "item", "set", "model", "mount" };

// 3.3.5 InventoryType names, by id.
static const char* const INVENTORY_TYPE_NAMES[] = { "", "Head", "Neck", "Shoulder", "Shirt", "Chest", "Waist", "Legs", "Feet",
    "Wrist", "Hands", "Finger", "Trinket", "One-Hand", "Shield", "Ranged", "Back", "Two-Hand", "Bag", "Tabard", "Robe",
    "Main Hand", "Off Hand", "Held In Off-hand", "Ammo", "Thrown", "Ranged", "Quiver", "Relic" };

constexpr int32_t CHEST = 5;
constexpr int32_t ROBE = 20;
constexpr uint32_t WEAPON_CLASS = 2;
constexpr uint32_t ARMOR_CLASS = 4;

// Latin-1 letters U+00C0 to U+00FF (UTF-8 C3 80 to C3 BF) without their accents; "" keeps the character.
static const char* const LATIN1_FOLDED[64] = { "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "", "o", "u", "u", "u", "u", "y", "th", "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "", "o", "u", "u", "u", "u", "y", "th", "y" };

bool ParseBrowseKind(std::string_view name, BrowseKind& kind) {
    for (size_t i = 0; i < sizeof(KIND_NAMES) / sizeof(KIND_NAMES[0]); i++) {
        if (name == KIND_NAMES[i]) {
            kind = static_cast<BrowseKind>(i);
            return true;
        }
    }
    return false;
}

const char* BrowseKindName(BrowseKind kind) {
    return KIND_NAMES[static_cast<size_t>(kind)];
}

std::string NormalizeSearchText(std::string_view text) {
    std::string normalized;
    bool space = false;
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            space = !normalized.empty();
            continue;
        }
        if (space) {
            normalized += ' ';
            space = false;
        }
        unsigned char next = i + 1 < text.size() ? static_cast<unsigned char>(text[i + 1]) : 0;
        if (c == 0xC3 && next >= 0x80 && next <= 0xBF && LATIN1_FOLDED[next - 0x80][0]) {
            normalized += LATIN1_FOLDED[next - 0x80];
            i++;
            continue;
        }
        normalized += static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    }
    return normalized;
}

ClassificationGroup GroupOfClassification(uint32_t classification) {
    switch (classification) {
        case 1:
            return ClassificationGroup::Elite;
        case 2:
        case 4:
            return ClassificationGroup::Rare;
        case 3:
            return ClassificationGroup::Boss;
        default:
            return ClassificationGroup::Normal;
    }
}

bool BrowseFilters::Any() const {
    const int32_t values[] = { inventoryType, armour, weaponSubclass, quality, minItemLevel, maxItemLevel, expansion, creatureType,
        family, classification, race, humanoid, classId };
    return std::any_of(std::begin(values), std::end(values), [](int32_t value) { return value >= 0; });
}

const std::vector<RetailIndex::Keys>& RetailIndex::KeysOf(BrowseKind kind) const {
    switch (kind) {
        case BrowseKind::Creature:
            return this->m_creatureKeys;
        case BrowseKind::Item:
            return this->m_itemKeys;
        case BrowseKind::Set:
            return this->m_setKeys;
        case BrowseKind::Model:
            return this->m_modelKeys;
        default:
            return this->m_mountKeys;
    }
}

static bool Unset(int32_t filter) {
    return filter < 0;
}

static bool Matches(int32_t filter, uint32_t value) {
    return filter < 0 || static_cast<uint32_t>(filter) == value;
}

static bool PassesCreature(const BrowseCreature& creature, const BrowseFilters& f) {
    if (!Matches(f.creatureType, creature.type) || !Matches(f.family, creature.family) ||
        !Matches(f.classification, static_cast<uint32_t>(GroupOfClassification(creature.classification)))) {
        return false;
    }
    bool humanoid = std::any_of(creature.displays.begin(), creature.displays.end(), [](const BrowseDisplay& d) { return d.facts.extraId != 0; });
    bool race = std::any_of(creature.displays.begin(), creature.displays.end(),
        [&](const BrowseDisplay& d) { return d.facts.extraId && static_cast<int32_t>(d.facts.race) == f.race; });
    return Matches(f.humanoid, humanoid ? 1 : 0) && (Unset(f.race) || race);
}

static bool PassesItem(const BrowseItem& item, const BrowseFilters& f) {
    int32_t slot = static_cast<int32_t>(item.inventoryType);
    bool chestLike = (f.inventoryType == CHEST || f.inventoryType == ROBE) && (slot == CHEST || slot == ROBE);
    if (!Unset(f.inventoryType) && slot != f.inventoryType && !chestLike) {
        return false;
    }
    if (!Unset(f.armour) && (item.classId != ARMOR_CLASS || !Matches(f.armour, item.subclassId))) {
        return false;
    }
    if (!Unset(f.weaponSubclass) && (item.classId != WEAPON_CLASS || !Matches(f.weaponSubclass, item.subclassId))) {
        return false;
    }
    bool levelInRange = (Unset(f.minItemLevel) || item.itemLevel >= static_cast<uint32_t>(f.minItemLevel)) &&
        (Unset(f.maxItemLevel) || item.itemLevel <= static_cast<uint32_t>(f.maxItemLevel));
    return levelInRange && Matches(f.quality, item.quality) && Matches(f.expansion, item.expansion);
}

static bool PassesSet(const BrowseSet& set, const BrowseFilters& f) {
    bool forClass = Unset(f.classId) || !set.classMask || (f.classId >= 1 && f.classId <= 32 && (set.classMask >> (f.classId - 1)) & 1);
    return forClass && Matches(f.armour, set.armour) && Matches(f.expansion, set.expansion);
}

bool RetailIndex::Passes(BrowseKind kind, size_t entry, const BrowseFilters& filters) const {
    switch (kind) {
        case BrowseKind::Creature:
            return PassesCreature(this->m_creatures[entry], filters);
        case BrowseKind::Item:
            return PassesItem(this->m_items[entry], filters);
        case BrowseKind::Set:
            return PassesSet(this->m_sets[entry], filters);
        default:
            return true;
    }
}

// 0 for an exact name or the entry's own id, 1 for a prefix or another of its ids, 2 for a substring; false for no match.
static bool RankOf(const RetailIndex::Keys& keys, const std::string& query, bool numeric, uint32_t number, uint32_t& rank) {
    if (query.empty()) {
        rank = 0;
        return true;
    }
    if (numeric) {
        auto found = std::find(keys.ids.begin(), keys.ids.end(), number);
        rank = found == keys.ids.begin() ? 0 : 1;
        return found != keys.ids.end();
    }
    bool matched = false;
    rank = 3;
    for (const std::string& name : keys.names) {
        size_t at = name.find(query);
        if (at != std::string::npos) {
            uint32_t found = name.size() == query.size() ? 0 : at == 0 ? 1 : 2;
            rank = std::min(rank, found);
            matched = true;
        }
    }
    return matched;
}

static bool ParseNumber(const std::string& text, uint32_t& number) {
    if (text.empty() || text.size() > 10 || !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        return false;
    }
    uint64_t value = std::stoull(text);
    number = static_cast<uint32_t>(value);
    return value <= UINT32_MAX;
}

std::vector<SearchRow> RetailIndex::SearchAll(BrowseKind kind, std::string_view text, const BrowseFilters& filters) const {
    std::string query = NormalizeSearchText(text);
    if (query.empty() && !filters.Any()) {
        return {};
    }
    uint32_t number = 0;
    bool numeric = ParseNumber(query, number);
    const std::vector<Keys>& keys = this->KeysOf(kind);
    std::vector<std::pair<uint32_t, size_t>> ranked;
    for (size_t entry = 0; entry < keys.size(); entry++) {
        uint32_t rank;
        if (this->Passes(kind, entry, filters) && RankOf(keys[entry], query, numeric, number, rank)) {
            ranked.push_back({ rank, entry });
        }
    }
    auto nameOf = [&](size_t entry) -> const std::string& {
        static const std::string none;
        return keys[entry].names.empty() ? none : keys[entry].names[0];
    };
    std::stable_sort(ranked.begin(), ranked.end(), [&](const auto& a, const auto& b) {
        return a.first != b.first ? a.first < b.first : nameOf(a.second) < nameOf(b.second);
    });
    std::vector<SearchRow> rows;
    rows.reserve(ranked.size());
    for (const auto& [rank, entry] : ranked) {
        rows.push_back(this->RowOf(kind, entry));
    }
    return rows;
}

SearchPage RetailIndex::Search(BrowseKind kind, std::string_view text, const BrowseFilters& filters, uint32_t page) const {
    std::vector<SearchRow> all = this->SearchAll(kind, text, filters);
    SearchPage result;
    result.total = all.size();
    result.page = page;
    result.pages = static_cast<uint32_t>((all.size() + BROWSE_PAGE_SIZE - 1) / BROWSE_PAGE_SIZE);
    size_t first = static_cast<size_t>(page) * BROWSE_PAGE_SIZE;
    for (size_t i = first; i < all.size() && i < first + BROWSE_PAGE_SIZE; i++) {
        result.rows.push_back(std::move(all[i]));
    }
    return result;
}

static std::string Joined(const std::vector<std::string>& parts) {
    std::string text;
    for (const std::string& part : parts) {
        if (!part.empty()) {
            text += (text.empty() ? "" : ", ") + part;
        }
    }
    return text;
}

static std::string InventoryTypeName(uint32_t type) {
    return type < sizeof(INVENTORY_TYPE_NAMES) / sizeof(INVENTORY_TYPE_NAMES[0]) ? INVENTORY_TYPE_NAMES[type] : "";
}

static const char* const GROUP_NAMES[] = { "", "Elite", "Rare", "Boss" };
static const char* const ARMOUR_NAMES[] = { "", "Cloth", "Leather", "Mail", "Plate" };

SearchRow RetailIndex::RowOf(BrowseKind kind, size_t entry) const {
    SearchRow row;
    if (kind == BrowseKind::Creature) {
        const BrowseCreature& c = this->m_creatures[entry];
        auto type = this->m_typeNames.find(c.type);
        row = { c.id, c.name, Joined({ c.title, type != this->m_typeNames.end() ? type->second : "",
            GROUP_NAMES[static_cast<size_t>(GroupOfClassification(c.classification))] }), c.displays.empty() ? 0 : c.displays[0].id, 0, 0 };
    } else if (kind == BrowseKind::Item) {
        const BrowseItem& i = this->m_items[entry];
        row = { i.id, i.name, Joined({ "ilvl " + std::to_string(i.itemLevel), InventoryTypeName(i.inventoryType), this->SubclassName(i.classId, i.subclassId) }),
            i.appearances.empty() ? 0 : i.appearances[0].displayId, i.icon, i.quality };
    } else if (kind == BrowseKind::Set) {
        const BrowseSet& s = this->m_sets[entry];
        row = { s.key, s.name, Joined({ s.source == SetSource::ItemSet ? "Item set" : "Transmog set", std::to_string(s.pieces.size()) + (s.pieces.size() == 1 ? " piece" : " pieces"),
            ARMOUR_NAMES[s.armour] }), s.pieces.empty() ? 0 : s.pieces[0].displayId, 0, 0 };
    } else if (kind == BrowseKind::Model) {
        const BrowseModel& m = this->m_models[entry];
        row = { m.fdid, m.users.empty() ? std::to_string(m.fdid) : m.users[0].name,
            Joined({ m.creature ? "Creature model" : "Item model", std::to_string(m.users.size()) + (m.users.size() == 1 ? " user" : " users") }), m.users.empty() ? 0 : m.users[0].displayId, 0, 0 };
    } else {
        const BrowseMount& m = this->m_mounts[entry];
        row = { m.id, m.name, "Spell " + std::to_string(m.spellId), m.displays.empty() ? 0 : m.displays[0].id, 0, 0 };
    }
    return row;
}

}
