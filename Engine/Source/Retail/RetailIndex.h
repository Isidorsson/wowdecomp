#ifndef ENGINE_SOURCE_RETAIL_RETAILINDEX_H
#define ENGINE_SOURCE_RETAIL_RETAILINDEX_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include "Engine/Source/Retail/BrowserTables.h"
#include "Engine/Source/Retail/ItemSparse.h"
#include "Engine/Source/Retail/StatBudget.h"

namespace Retail {

// The in-memory search index of the retail browser (Interface\AddOns\RetailBrowser): creatures, items, sets, models
// and mounts of the pinned build, joined once from BrowserTables. Search is case and accent insensitive; a number
// searches ids. Details carry every id server and DB work needs (the SQL builders, BrowserBuild.cpp, start from them).

enum class BrowseKind : uint8_t { Creature, Item, Set, Model, Mount };

bool ParseBrowseKind(std::string_view name, BrowseKind& kind);
const char* BrowseKindName(BrowseKind kind);

constexpr uint32_t BROWSE_PAGE_SIZE = 50;

// ItemSet ids share a key space with TransmogSet ids in the set kind, above every TransmogSet id.
constexpr uint32_t ITEM_SET_KEY_BASE = 10000000;

// Lower case, accents dropped (Latin-1 letters in UTF-8), runs of white space as one space, trimmed.
std::string NormalizeSearchText(std::string_view text);

// Creature classification as the filter groups it: 3.3.5's ranks. Retail has no boss value (bosses are elites; the old
// world boss value 3 is unused), so there is no boss group.
enum class ClassificationGroup : int32_t { Normal = 0, Elite = 1, Rare = 2, RareElite = 3 };
ClassificationGroup GroupOfClassification(uint32_t classification);

struct BrowseDisplay {
    uint32_t id = 0;
    float probability = 0;
    bool known = false;   // The display loaders have it.
    CreatureDisplayFacts facts;
};

struct BrowseCreature {
    uint32_t id = 0;
    std::string name;
    std::string title;
    uint32_t classification = 0;
    uint32_t type = 0;
    uint32_t family = 0;
    std::vector<BrowseDisplay> displays;   // The nonzero DisplayID slots, in order.
};

struct BrowseAppearance {
    uint32_t modifiedAppearanceId = 0;
    uint32_t modifier = 0;
    uint32_t appearanceId = 0;
    uint32_t displayId = 0;   // ItemDisplayInfo id.
    uint32_t icon = 0;        // ItemAppearance default icon.
    bool known = false;
    ItemDisplayFacts facts;
};

struct BrowseItem {
    uint32_t id = 0;
    std::string name;
    uint32_t quality = 0;
    uint32_t expansion = 0;
    uint32_t itemLevel = 0;
    uint32_t requiredLevel = 0;
    uint32_t allowableClass = 0;
    uint32_t classId = 0;
    uint32_t subclassId = 0;
    uint32_t inventoryType = 0;
    uint32_t material = 0;
    uint32_t sheatheType = 0;
    uint32_t icon = 0;   // Item's own icon, else the first appearance's.
    std::vector<BrowseAppearance> appearances;   // By ItemModifiedAppearance order.
    bool hasSparse = false;   // Its ItemSparse row was read (AttachSparse): stats, sockets, delay.
    SparseItem sparse;
};

enum class SetSource : uint8_t { Transmog, ItemSet };

struct SetPiece {
    uint32_t itemId = 0;
    uint32_t modifiedAppearanceId = 0;   // 0 for an ItemSet piece: its item's first appearance gives the display.
    uint32_t displayId = 0;
    uint32_t inventoryType = 0;
};

struct BrowseSet {
    uint32_t key = 0;   // TransmogSet id, or ITEM_SET_KEY_BASE + ItemSet id.
    uint32_t setId = 0;
    SetSource source = SetSource::Transmog;
    std::string name;
    uint32_t classMask = 0;   // 1 << (class - 1); 0 for every class.
    uint32_t expansion = 0;
    uint32_t armour = 0;      // Most pieces' armour subclass (1 cloth to 4 plate), 0 for none.
    std::vector<SetPiece> pieces;
};

struct ModelUser {
    BrowseKind kind = BrowseKind::Creature;
    uint32_t id = 0;
    uint32_t displayId = 0;
    std::string name;
};

struct BrowseModel {
    uint32_t fdid = 0;
    bool creature = true;   // A creature model, else an item model.
    std::vector<ModelUser> users;
};

struct BrowseMount {
    uint32_t id = 0;
    std::string name;
    uint32_t spellId = 0;
    uint32_t mountType = 0;
    std::vector<BrowseDisplay> displays;
};

// -1 for a filter not set. Items: inventoryType (chest also matches robes), armour (1 cloth to 4 plate),
// weaponSubclass, quality, item level range, expansion. Creatures: creatureType, family, classification (a
// ClassificationGroup), race (of the Extra row), humanoid (1 any display has an Extra row, 0 none). Sets: classId,
// armour, expansion.
struct BrowseFilters {
    int32_t inventoryType = -1;
    int32_t armour = -1;
    int32_t weaponSubclass = -1;
    int32_t quality = -1;
    int32_t minItemLevel = -1;
    int32_t maxItemLevel = -1;
    int32_t expansion = -1;
    int32_t creatureType = -1;
    int32_t family = -1;
    int32_t classification = -1;
    int32_t race = -1;
    int32_t humanoid = -1;
    int32_t classId = -1;

    bool Any() const;
    bool operator==(const BrowseFilters& other) const;
};

struct SearchRow {
    uint32_t id = 0;
    std::string name;
    std::string detail;       // One line under the name.
    uint32_t displayId = 0;   // What the preview shows: a creature display, an item display.
    uint32_t icon = 0;
    uint32_t quality = 0;
};

struct SearchPage {
    std::vector<SearchRow> rows;
    size_t total = 0;
    uint32_t page = 0;    // From 0.
    uint32_t pages = 0;
};

// The item joins Build reads once for items and sets (RetailIndex.cpp).
struct ItemJoins;

class RetailIndex {
    public:
    bool Build(const BrowserTables& tables, const BrowserLookups& lookups, std::string& error);
    // After Build: each indexed item's ItemSparse row (parsed with ItemSparseStrings()), and the budget tables that
    // turn its allocations into values. Without them items have no stats and the SQL builders no stat lines.
    bool AttachSparse(const Db2Table& itemSparse, std::string& error);
    void SetBudget(StatBudget budget) { this->m_budget = std::move(budget); }
    const StatBudget& Budget() const { return this->m_budget; }

    // Exact name or id first, then prefix, then substring; ties by name, then id. No text and no filter finds nothing;
    // a filter alone lists everything it passes. Only the page's rows are built; the ranking of the last search is
    // kept, so turning its pages only slices it.
    SearchPage Search(BrowseKind kind, std::string_view text, const BrowseFilters& filters, uint32_t page) const;
    // Every match, ranked as Search ranks them.
    std::vector<SearchRow> SearchAll(BrowseKind kind, std::string_view text, const BrowseFilters& filters) const;

    // Null for an unknown id.
    const BrowseCreature* Creature(uint32_t id) const;
    const BrowseItem* Item(uint32_t id) const;
    const BrowseSet* Set(uint32_t key) const;
    const BrowseModel* Model(uint32_t fdid) const;
    const BrowseMount* Mount(uint32_t id) const;

    size_t Count(BrowseKind kind) const;
    // Rows of still-locked sections the tables skipped.
    size_t SkippedRows() const { return this->m_skippedRows; }

    const std::map<uint32_t, std::string>& CreatureTypeNames() const { return this->m_typeNames; }
    const std::map<uint32_t, std::string>& FamilyNames() const { return this->m_familyNames; }
    // By class << 8 | subclass.
    const std::map<uint32_t, std::string>& SubclassNames() const { return this->m_subclassNames; }
    std::string SubclassName(uint32_t classId, uint32_t subclassId) const;

    // What a search compares: normalized names and the ids a number finds, the first id the entry's own.
    struct Keys {
        std::vector<std::string> names;
        std::vector<uint32_t> ids;
        uint32_t nameOrder = 0;   // The entry's place among its kind by first name, then id: what ties compare.
    };

    private:
    void BuildCreatures(const BrowserTables& tables, const BrowserLookups& lookups);
    void BuildItems(const BrowserTables& tables, const ItemJoins& joins, const BrowserLookups& lookups);
    void BuildSets(const BrowserTables& tables, const ItemJoins& joins);
    void BuildTransmogSets(const BrowserTables& tables, const ItemJoins& joins);
    void BuildItemSets(const BrowserTables& tables, const ItemJoins& joins);
    void BuildMounts(const BrowserTables& tables, const BrowserLookups& lookups);
    void BuildModels();
    void BuildNames(const BrowserTables& tables);
    void BuildNameOrders();
    bool Passes(BrowseKind kind, size_t entry, const BrowseFilters& filters) const;
    SearchRow RowOf(BrowseKind kind, size_t entry) const;
    const std::vector<Keys>& KeysOf(BrowseKind kind) const;
    std::vector<Keys>& KeysOf(BrowseKind kind);

    // Search's last ranking: (rank << 32 | nameOrder, entry) of every match, sorted once a second page is asked.
    struct RankedSearch {
        bool valid = false;
        BrowseKind kind = BrowseKind::Creature;
        std::string query;
        BrowseFilters filters;
        bool sorted = false;
        std::vector<std::pair<uint64_t, uint32_t>> ranked;
    };
    // A copy or a rebuilt index starts without one.
    struct SearchCache {
        SearchCache() = default;
        SearchCache(const SearchCache&) {}
        SearchCache& operator=(const SearchCache&) {
            this->last = RankedSearch();
            return *this;
        }
        std::mutex lock;
        RankedSearch last;
    };
    std::vector<std::pair<uint64_t, uint32_t>> Rank(BrowseKind kind, const std::string& query, const BrowseFilters& filters) const;

    std::vector<BrowseCreature> m_creatures;   // By id, each vector.
    std::vector<BrowseItem> m_items;
    std::vector<BrowseSet> m_sets;
    std::vector<BrowseModel> m_models;
    std::vector<BrowseMount> m_mounts;
    std::vector<Keys> m_creatureKeys;          // Parallel to the entries.
    std::vector<Keys> m_itemKeys;
    std::vector<Keys> m_setKeys;
    std::vector<Keys> m_modelKeys;
    std::vector<Keys> m_mountKeys;
    std::map<uint32_t, std::string> m_typeNames;
    std::map<uint32_t, std::string> m_familyNames;
    std::map<uint32_t, std::string> m_subclassNames;
    size_t m_skippedRows = 0;
    StatBudget m_budget;
    mutable SearchCache m_searchCache;
};

}

#endif
