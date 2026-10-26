-- Retail Browser: searches the retail tables the wowdecomp client serves from CASC (RetailSearch, RetailDetails,
-- RetailBrowserInfo, RetailExportCsv, RetailBuildSql, RetailExportSql and CopyToClipboard exist only on a client with
-- retail assets configured). Constants, saved variables, favourites and history.

RetailBrowser = RetailBrowser or {}
local RB = RetailBrowser

RB.KINDS = {
    { key = "creature", label = "Creatures" },
    { key = "item", label = "Items" },
    { key = "set", label = "Sets" },
    { key = "model", label = "Models" },
    { key = "mount", label = "Mounts" },
}
RB.HISTORY_SIZE = 200
RB.POLL_SECONDS = 0.5
RB.SEARCH_DELAY = 0.3
RB.BULK_LIMIT = 50

-- The highest stock 3.3.5a ids of the display DBCs: an installed row at or below them is the game's own ("3.3.5"),
-- above them a row the server added ("custom").
RB.STOCK_MAX_CREATURE_DISPLAY = 32754
RB.STOCK_MAX_ITEM_DISPLAY = 68742

-- RetailSearch's sort columns (Engine/Source/Retail/RetailIndex.h SortColumn).
RB.SORT = {
    name = 0, id = 1, itemLevel = 2, quality = 3, slot = 4, subclass = 5, expansion = 6, display = 7, count = 8,
    type = 9, rank = 10, race = 11, family = 12, kind = 13, installed = 14,
}

RB.INVENTORY_TYPES = {
    { 1, "Head" }, { 2, "Neck" }, { 3, "Shoulder" }, { 4, "Shirt" }, { 5, "Chest" }, { 6, "Waist" }, { 7, "Legs" },
    { 8, "Feet" }, { 9, "Wrist" }, { 10, "Hands" }, { 11, "Finger" }, { 12, "Trinket" }, { 13, "One-Hand" },
    { 14, "Shield" }, { 15, "Ranged" }, { 16, "Back" }, { 17, "Two-Hand" }, { 19, "Tabard" }, { 20, "Robe" },
    { 21, "Main Hand" }, { 22, "Off Hand" }, { 23, "Held In Off-hand" }, { 26, "Wand / Gun" },
}
RB.ARMOUR_TYPES = { { 1, "Cloth" }, { 2, "Leather" }, { 3, "Mail" }, { 4, "Plate" } }
RB.QUALITIES = {
    { 0, "Poor" }, { 1, "Common" }, { 2, "Uncommon" }, { 3, "Rare" }, { 4, "Epic" }, { 5, "Legendary" },
    { 6, "Artifact" }, { 7, "Heirloom" },
}
RB.EXPANSIONS = {
    { 0, "Classic" }, { 1, "TBC" }, { 2, "WotLK" }, { 3, "Cata" }, { 4, "MoP" }, { 5, "WoD" }, { 6, "Legion" },
    { 7, "BfA" }, { 8, "SL" }, { 9, "DF" }, { 10, "TWW" }, { 11, "MID" },
}
-- Retail has no boss classification (bosses are elites), so ranks are 3.3.5's without one.
RB.CLASSIFICATIONS = { { 0, "Normal" }, { 1, "Elite" }, { 3, "Rare elite" }, { 2, "Rare" } }
RB.RACES = {
    { 1, "Human" }, { 2, "Orc" }, { 3, "Dwarf" }, { 4, "Night Elf" }, { 5, "Undead" }, { 6, "Tauren" }, { 7, "Gnome" },
    { 8, "Troll" }, { 9, "Goblin" }, { 10, "Blood Elf" }, { 11, "Draenei" }, { 22, "Worgen" }, { 24, "Pandaren" },
    { 27, "Nightborne" }, { 28, "Highmountain" }, { 29, "Void Elf" }, { 30, "Lightforged" }, { 31, "Zandalari" },
    { 32, "Kul Tiran" }, { 34, "Dark Iron" }, { 35, "Vulpera" }, { 36, "Mag'har" }, { 37, "Mechagnome" },
    { 52, "Dracthyr" }, { 84, "Earthen" },
}
RB.NPC_KINDS = { { 1, "NPC" }, { 0, "Model" } }
RB.CLASSES = {
    { 1, "Warrior" }, { 2, "Paladin" }, { 3, "Hunter" }, { 4, "Rogue" }, { 5, "Priest" }, { 6, "Death Knight" },
    { 7, "Shaman" }, { 8, "Mage" }, { 9, "Warlock" }, { 10, "Monk" }, { 11, "Druid" }, { 12, "Demon Hunter" },
    { 13, "Evoker" },
}
RB.INSTALLED = { { 0, "Served" }, { 1, "Custom / 3.3.5" } }
RB.SET_SOURCES = { { 0, "Transmog" }, { 1, "Item set" } }
RB.MODEL_KINDS = { { 1, "Creature" }, { 0, "Item" } }
RB.MOUNT_TYPES = {
    { 230, "Ground" }, { 231, "Turtle" }, { 232, "Vashj'ir" }, { 241, "Qiraji" }, { 248, "Flying" },
    { 254, "Underwater" }, { 269, "Strider" }, { 284, "Chauffeur" }, { 402, "Skyriding" }, { 407, "Fly + swim" },
    { 424, "Sky + fly" },
}
RB.SAVED_KINDS = { { "creature", "Creature" }, { "item", "Item" }, { "set", "Set" }, { "model", "Model" }, { "mount", "Mount" } }

function RB.NamedChoices(list)
    local choices = {}
    for _, entry in ipairs(list or {}) do
        choices[#choices + 1] = { entry.id, entry.name }
    end
    table.sort(choices, function(a, b) return a[2] < b[2] end)
    return choices
end

-- The text of a { value, text } choice list, or "" for a value it lacks.
function RB.ChoiceText(choices, value)
    for _, choice in ipairs(choices or {}) do
        if choice[1] == value then
            return choice[2]
        end
    end
    return ""
end

function RB.Available()
    return type(RetailSearch) == "function"
end

function RB.Print(message)
    DEFAULT_CHAT_FRAME:AddMessage(RB.Colored("Retail Browser:", RB.THEME.title) .. " " .. message)
end

function RB.Copy(text, what)
    text = tostring(text or "")
    if text == "" then
        return
    end
    local ok, err = CopyToClipboard(text)
    if ok then
        RB.Print("copied " .. (what or text))
    else
        RB.Print("could not copy: " .. tostring(err))
    end
end

-- The model and icon prefetch (wd-npc-preview, wd-casc-io): called only when the client has them. RetailPrefetch takes
-- a creature display: the one the entry's actions would use, so the preview that may follow finds it prepared.
function RB.Prefetch(kind, id)
    if type(RetailPrefetch) ~= "function" or not kind or not id then
        return
    end
    local d = RB.DetailsOf(kind, id)
    local display = d and RB.ActionDisplay(d)
    if display then
        RetailPrefetch("display", display)
    end
end

function RB.PrefetchIcons(list)
    if type(RetailPrefetchIcons) == "function" and #list > 0 then
        RetailPrefetchIcons(list)
    end
end

-- "served" (the client draws the retail row), "3.3.5" (a stock DBC row of that id wins) or "custom" (a row the
-- server added wins).
function RB.BadgeOf(custom, displayId, itemDisplay)
    if not custom then
        return "served"
    end
    local stock = itemDisplay and RB.STOCK_MAX_ITEM_DISPLAY or RB.STOCK_MAX_CREATURE_DISPLAY
    return (displayId or 0) <= stock and "3.3.5" or "custom"
end

-- Account-wide: favourites by "kind:id", the most recent details first, the index-at-login choice. Per character
-- (RetailBrowserCharDB): the window's place, scale and drawer height.
local function InitDB()
    RetailBrowserDB = RetailBrowserDB or {}
    RetailBrowserDB.favourites = RetailBrowserDB.favourites or {}
    RetailBrowserDB.history = RetailBrowserDB.history or {}
    if RetailBrowserDB.indexAtLogin == nil then
        RetailBrowserDB.indexAtLogin = true
    end
    RetailBrowserCharDB = RetailBrowserCharDB or {}
end

function RB.CharSetting(key, default)
    local db = RetailBrowserCharDB
    if db and db[key] ~= nil then
        return db[key]
    end
    return default
end

function RB.SetCharSetting(key, value)
    RetailBrowserCharDB = RetailBrowserCharDB or {}
    RetailBrowserCharDB[key] = value
end

-- Building the index at login (on a worker) makes the first /rb ready at once, for about 120 MB kept all session.
function RB.IndexAtLogin()
    return RetailBrowserDB.indexAtLogin
end

function RB.SetIndexAtLogin(on)
    RetailBrowserDB.indexAtLogin = on and true or false
end

local function EntryKey(kind, id)
    return kind .. ":" .. id
end
RB.EntryKey = EntryKey

function RB.IsFavourite(kind, id)
    return RetailBrowserDB.favourites[EntryKey(kind, id)] ~= nil
end

function RB.SetFavourite(kind, id, name, on)
    RetailBrowserDB.favourites[EntryKey(kind, id)] = on and { kind = kind, id = id, name = name } or nil
end

function RB.ToggleFavourite(kind, id, name)
    local on = not RB.IsFavourite(kind, id)
    RB.SetFavourite(kind, id, name, on)
    return on
end

function RB.Favourites()
    local list = {}
    for _, entry in pairs(RetailBrowserDB.favourites) do
        list[#list + 1] = entry
    end
    table.sort(list, function(a, b) return a.name < b.name end)
    return list
end

function RB.RememberViewed(kind, id, name)
    local history = RetailBrowserDB.history
    for i = #history, 1, -1 do
        if history[i].kind == kind and history[i].id == id then
            table.remove(history, i)
        end
    end
    table.insert(history, 1, { kind = kind, id = id, name = name })
    while #history > RB.HISTORY_SIZE do
        table.remove(history)
    end
end

function RB.History()
    return RetailBrowserDB.history
end

function RB.ClearHistory()
    wipe(RetailBrowserDB.history)
    RB.Print("history cleared")
end

local events = CreateFrame("Frame")
events:RegisterEvent("ADDON_LOADED")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:SetScript("OnEvent", function(self, event, name)
    if event == "ADDON_LOADED" and name == "RetailBrowser" then
        InitDB()
        self:UnregisterEvent("ADDON_LOADED")
    elseif event == "PLAYER_ENTERING_WORLD" then
        self:UnregisterEvent("PLAYER_ENTERING_WORLD")
        if RB.Available() and RetailBrowserDB and RB.IndexAtLogin() then
            RetailBrowserInfo()
        end
    end
end)

SLASH_RETAILBROWSER1 = "/rb"
SLASH_RETAILBROWSER2 = "/retailbrowser"
SlashCmdList["RETAILBROWSER"] = function()
    if not RB.Available() then
        RB.Print("this client has no retail assets configured.")
        return
    end
    RB.Toggle()
end

BINDING_HEADER_RETAILBROWSER = "Retail Browser"
BINDING_NAME_RETAILBROWSER_TOGGLE = "Toggle Retail Browser"
