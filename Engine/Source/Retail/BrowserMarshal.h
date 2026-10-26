#ifndef ENGINE_SOURCE_RETAIL_BROWSERMARSHAL_H
#define ENGINE_SOURCE_RETAIL_BROWSERMARSHAL_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "Engine/Source/Retail/RetailIndex.h"

namespace Retail {

// What the retail browser's Lua functions return, as a plain tree the client glue pushes onto the Lua stack
// (WoW/Source/Ui/RetailBrowser_Scripts.cpp), so the marshalling is tested without a Lua state.
struct BrowserValue {
    enum class Type : uint8_t { Nil, Bool, Number, String, Table };

    Type type = Type::Nil;
    bool flag = false;
    double number = 0;
    std::string text;
    std::vector<std::pair<std::string, BrowserValue>> fields;   // Named keys, in insertion order.
    std::vector<BrowserValue> items;                            // The array part, from 1.

    static BrowserValue Bool(bool value);
    static BrowserValue Number(double value);
    static BrowserValue String(std::string value);
    static BrowserValue Table();

    BrowserValue& Set(const std::string& key, BrowserValue value);
    BrowserValue& Add(BrowserValue value);
    // Null for a key the table lacks.
    const BrowserValue* Get(const std::string& key) const;
};

enum class IndexState : uint8_t { Idle, Building, Ready, Failed };
const char* IndexStateName(IndexState state);

// The index as the Lua functions see it: null unless state is Ready.
struct BrowserIndexView {
    IndexState state = IndexState::Idle;
    const RetailIndex* index = nullptr;
    std::string error;   // Why the build failed.
};

// Whether the installed DBCs have a display id, so their row wins over the retail one ("custom row wins").
struct InstalledDisplays {
    std::function<bool(uint32_t)> creature;
    std::function<bool(uint32_t)> item;
};

// Filter names as the addon sends them: inventoryType, armour, weaponSubclass, quality, minItemLevel,
// maxItemLevel, expansion, creatureType, family, classification, race, humanoid, classId, and the column filters minId,
// maxId, displayId, minCount, maxCount, installed, setSource, modelKind, mountType, sort, descending. Unknown names are
// ignored.
BrowseFilters ParseBrowseFilters(const std::map<std::string, double>& filters);

// RetailSearch(kind, text, filters, page): {status = "building"} until the index is ready, then
// {status = "ok", total, page (from 1), pages, rows = {{id, name, detail, displayId, icon, quality}, ...}}. The
// installed displays answer the installed filter.
// False with error for an unknown kind, which Lua sees as nil, error.
bool MarshalSearch(const BrowserIndexView& view, const std::string& kind, const std::string& text,
    const std::map<std::string, double>& filters, uint32_t page, BrowserValue& out, std::string& error,
    const InstalledDisplays& installed = InstalledDisplays());

// RetailDetails(kind, id): the entry's ids as named fields, its displays (or appearances, pieces, users) with a
// "custom" flag where the installed DBC has the id, and fields = {{label, value}, ...} in display order for the
// UI and plain-text copies. False with error for an unknown kind or id.
bool MarshalDetails(const BrowserIndexView& view, const std::string& kind, uint32_t id, const InstalledDisplays& installed,
    BrowserValue& out, std::string& error);

// RetailBuildSql(kind, id, options): world DB SQL for an item, a set (an item per piece) or a creature (BuildSql.h).
// Numbers: itemLevel (200), startEntry (9000000), baseEntry (0: the suggested stock item, or DEFAULT_BASE_CREATURE),
// requiredLevel (-1 keeps the base's), combo (ComboPrimary), retailStamina (0), retailArmour (1: retail armour and
// weapon damage, 0 keeps the base's), allVariants (0: one piece per slot), appearance (1), display (0: the first),
// otherDisplays (0), scale (0 keeps). Texts: name, subname (else retail's).
// {status = "ok", kind, sql, firstEntry, nextEntry, itemLevel, base, pieces, lines = {{piece, retail, retailValue,
// classic, value, note [, applied]}, ...}, notes = {text, ...}}; false with error for another kind or an unknown id.
struct BuildOptions {
    std::map<std::string, double> numbers;
    std::map<std::string, std::string> texts;
};
bool MarshalBuild(const BrowserIndexView& view, const std::string& kind, uint32_t id, const BuildOptions& options, BrowserValue& out,
    std::string& error);

// RetailBrowserInfo(): status, error, counts per kind and the filter choices the index has names for.
BrowserValue MarshalInfo(const BrowserIndexView& view);

// Shared by the marshallers: the status table of an index that is not ready (false when it is), and the kind of a
// name or the error an unknown one gives.
bool BrowserNotReady(const BrowserIndexView& view, BrowserValue& out);
bool BrowserKindOf(const std::string& name, BrowseKind& kind, std::string& error);

// Every match of a search as CSV: a header row, then one row per match, quoted where needed.
bool SearchCsv(const BrowserIndexView& view, const std::string& kind, const std::string& text,
    const std::map<std::string, double>& filters, std::string& csv, std::string& error,
    const InstalledDisplays& installed = InstalledDisplays());

}

#endif
