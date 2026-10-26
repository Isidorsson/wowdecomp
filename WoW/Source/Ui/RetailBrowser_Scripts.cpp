#include "WoW/Source/Ui/RetailBrowser_Scripts.h"

// Not in the original. The Lua functions of the retail browser addon (Interface\AddOns\RetailBrowser): a search over the
// retail tables of the pinned CASC build, built on a worker on first use (Engine/Source/Retail/RetailIndex.h), its
// details, world DB SQL built from an entry (RetailBuildSql, RetailExportSql), a CSV export and the clipboard. Registered only when retail assets are configured, so the addon can test
// for RetailSearch to know whether it runs on such a client.

#if defined(WOWDECOMP_RETAIL_ASSETS)
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/OsClipboard.h"
#include "Engine/Source/Retail/BrowserMarshal.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/Ui/RetailModelFrame_Scripts.h"

static const char* const EXPORT_FOLDER = "Logs\\RetailBrowser";

template <class Db>
static bool HasStockRow(const Db& db, uint32_t id) {
    int32_t key = static_cast<int32_t>(id);
    return key >= db.m_minID && key <= db.m_maxID && db.m_recordsById[key - db.m_minID];
}

// The installed DBC rows, which the client draws in place of a retail row of the same id.
static Retail::InstalledDisplays InstalledDisplays() {
    return { [](uint32_t id) { return HasStockRow(g_creatureDisplayInfoDB, id); }, [](uint32_t id) { return HasStockRow(g_itemDisplayInfoDB, id); } };
}

static void PushValue(lua_State* L, const Retail::BrowserValue& value) {
    using Type = Retail::BrowserValue::Type;
    switch (value.type) {
        case Type::Bool:
            lua_pushboolean(L, value.flag);
            return;
        case Type::Number:
            lua_pushnumber(L, value.number);
            return;
        case Type::String:
            lua_pushstring(L, value.text.c_str());
            return;
        case Type::Table:
            break;
        default:
            lua_pushnil(L);
            return;
    }
    lua_createtable(L, static_cast<int>(value.items.size()), static_cast<int>(value.fields.size()));
    for (size_t i = 0; i < value.items.size(); i++) {
        PushValue(L, value.items[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    for (const auto& [key, field] : value.fields) {
        PushValue(L, field);
        lua_setfield(L, -2, key.c_str());
    }
}

// nil, error: what a Lua caller checks with "local result, err = ...".
static int32_t PushError(lua_State* L, const std::string& error) {
    lua_pushnil(L);
    lua_pushstring(L, error.c_str());
    return 2;
}

static std::string StringArg(lua_State* L, int index) {
    const char* text = lua_isstring(L, index) ? lua_tolstring(L, index, nullptr) : nullptr;
    return text ? text : "";
}

// The numeric fields of a filter table; other keys and values are ignored.
static std::map<std::string, double> FilterArg(lua_State* L, int index) {
    std::map<std::string, double> filters;
    if (!lua_istable(L, index)) {
        return filters;
    }
    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TNUMBER) {
            filters[lua_tolstring(L, -2, nullptr)] = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
    }
    return filters;
}

// RetailSearch(kind, text [, filters [, page]]): see MarshalSearch.
static int32_t Script_RetailSearch(lua_State* L) {
    uint32_t page = lua_isnumber(L, 4) ? static_cast<uint32_t>(lua_tonumber(L, 4)) : 1;
    Retail::BrowserValue result;
    std::string error;
    if (!Retail::MarshalSearch(Retail::CascSource::Instance().BrowserIndex(), StringArg(L, 1), StringArg(L, 2), FilterArg(L, 3), page, result, error)) {
        return PushError(L, error);
    }
    PushValue(L, result);
    return 1;
}

// RetailDetails(kind, id): see MarshalDetails.
static int32_t Script_RetailDetails(lua_State* L) {
    uint32_t id = lua_isnumber(L, 2) ? static_cast<uint32_t>(lua_tonumber(L, 2)) : 0;
    Retail::BrowserValue result;
    std::string error;
    if (!Retail::MarshalDetails(Retail::CascSource::Instance().BrowserIndex(), StringArg(L, 1), id, InstalledDisplays(), result, error)) {
        return PushError(L, error);
    }
    PushValue(L, result);
    return 1;
}

// RetailBrowserInfo(): see MarshalInfo. Starts the index build.
static int32_t Script_RetailBrowserInfo(lua_State* L) {
    PushValue(L, Retail::MarshalInfo(Retail::CascSource::Instance().BrowserIndex()));
    return 1;
}

// Logs\RetailBrowser\<name>-<time>.<extension>; the name keeps letters, digits, '-' and '_'.
static std::string ExportPath(const std::string& name, const char* extension) {
    std::string safe;
    for (char c : name) {
        bool kept = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        safe += kept ? c : '_';
    }
    std::time_t now = std::time(nullptr);
    std::tm local = {};
    localtime_s(&local, &now);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
    return std::string(EXPORT_FOLDER) + "\\" + (safe.empty() ? "export" : safe) + "-" + stamp + "." + extension;
}

// Pushes the path written, or nil, error.
static int32_t WriteExport(lua_State* L, const std::string& path, const std::string& text) {
    std::error_code created;
    std::filesystem::create_directories(EXPORT_FOLDER, created);
    std::ofstream file(path, std::ios::binary);
    if (!file.write(text.data(), static_cast<std::streamsize>(text.size()))) {
        return PushError(L, "cannot write " + path);
    }
    lua_pushstring(L, path.c_str());
    return 1;
}

// RetailExportCsv(kind, text [, filters]): every match written to Logs\RetailBrowser\<kind>-<time>.csv; the path, or
// nil, error.
static int32_t Script_RetailExportCsv(lua_State* L) {
    std::string kind = StringArg(L, 1);
    std::string csv;
    std::string error;
    if (!Retail::SearchCsv(Retail::CascSource::Instance().BrowserIndex(), kind, StringArg(L, 2), FilterArg(L, 3), csv, error)) {
        return PushError(L, error);
    }
    return WriteExport(L, ExportPath(kind, "csv"), csv);
}

// The number and string fields of an options table.
static Retail::BuildOptions BuildOptionsArg(lua_State* L, int index) {
    Retail::BuildOptions options;
    options.numbers = FilterArg(L, index);
    if (!lua_istable(L, index)) {
        return options;
    }
    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING) {
            options.texts[lua_tolstring(L, -2, nullptr)] = lua_tolstring(L, -1, nullptr);
        }
        lua_pop(L, 1);
    }
    return options;
}

// RetailBuildSql(kind, id [, options]): see MarshalBuild.
static int32_t Script_RetailBuildSql(lua_State* L) {
    uint32_t id = lua_isnumber(L, 2) ? static_cast<uint32_t>(lua_tonumber(L, 2)) : 0;
    Retail::BrowserValue result;
    std::string error;
    if (!Retail::MarshalBuild(Retail::CascSource::Instance().BrowserIndex(), StringArg(L, 1), id, BuildOptionsArg(L, 3), result, error)) {
        return PushError(L, error);
    }
    PushValue(L, result);
    return 1;
}

// RetailExportSql(name, sql): sql written to Logs\RetailBrowser\<name>-<time>.sql; the path, or nil, error.
static int32_t Script_RetailExportSql(lua_State* L) {
    if (!lua_isstring(L, 2)) {
        return PushError(L, "Usage: RetailExportSql(name, sql)");
    }
    return WriteExport(L, ExportPath(StringArg(L, 1), "sql"), StringArg(L, 2));
}

// CopyToClipboard(text): true, or nil, error.
static int32_t Script_CopyToClipboard(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        return PushError(L, "Usage: CopyToClipboard(text)");
    }
    if (!OsClipboardPutString(lua_tolstring(L, 1, nullptr))) {
        return PushError(L, "the clipboard is in use");
    }
    lua_pushboolean(L, 1);
    return 1;
}

// GetClipboardText(): the clipboard's text, or nil.
static int32_t Script_GetClipboardText(lua_State* L) {
    char* text = OsClipboardGetString();
    if (!text) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, text);
    OsClipboardFreeString(text);
    return 1;
}

// The result list's icon prefetch (CascSource::PrefetchIcons), so a new page's icons are read off the main thread.

// RetailPrefetchIcons({fdid, ...}): reads those icons on a worker, in order, replacing what an earlier call still had queued.
static int32_t Script_RetailPrefetchIcons(lua_State* L) {
    std::vector<uint32_t> icons;
    int count = lua_istable(L, 1) ? static_cast<int>(lua_objlen(L, 1)) : 0;
    for (int i = 1; i <= count; i++) {
        lua_rawgeti(L, 1, i);
        if (lua_isnumber(L, -1)) {
            icons.push_back(static_cast<uint32_t>(lua_tonumber(L, -1)));
        }
        lua_pop(L, 1);
    }
    Retail::CascSource::Instance().PrefetchIcons(icons);
    return 0;
}

// RetailIconPending(fdid): whether the prefetch still has the icon queued or in flight; a texture set now would wait on it.
static int32_t Script_RetailIconPending(lua_State* L) {
    bool pending = lua_isnumber(L, 1) && Retail::CascSource::Instance().IconPending(static_cast<uint32_t>(lua_tonumber(L, 1)));
    lua_pushboolean(L, pending);
    return 1;
}

static void RetailIconsRegister() {
    FrameScript_RegisterFunction("RetailPrefetchIcons", Script_RetailPrefetchIcons);
    FrameScript_RegisterFunction("RetailIconPending", Script_RetailIconPending);
}

void RetailBrowserRegister() {
    if (!Retail::CascSource::Instance().Configured()) {
        return;
    }
    FrameScript_RegisterFunction("RetailSearch", Script_RetailSearch);
    FrameScript_RegisterFunction("RetailDetails", Script_RetailDetails);
    FrameScript_RegisterFunction("RetailBrowserInfo", Script_RetailBrowserInfo);
    FrameScript_RegisterFunction("RetailExportCsv", Script_RetailExportCsv);
    FrameScript_RegisterFunction("RetailBuildSql", Script_RetailBuildSql);
    FrameScript_RegisterFunction("RetailExportSql", Script_RetailExportSql);
    FrameScript_RegisterFunction("CopyToClipboard", Script_CopyToClipboard);
    FrameScript_RegisterFunction("GetClipboardText", Script_GetClipboardText);
    RetailIconsRegister();
    RetailModelFramesRegister();
}

#else

void RetailBrowserRegister() {
}

#endif
