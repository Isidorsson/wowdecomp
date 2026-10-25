-- Retail Browser: searches the retail tables the wowdecomp client serves from CASC (RetailSearch, RetailDetails,
-- RetailBrowserInfo, RetailExportCsv and CopyToClipboard exist only on a client with retail assets configured).

RetailBrowser = RetailBrowser or {}
local RB = RetailBrowser

RB.KINDS = {
    { key = "creature", label = "Creatures" },
    { key = "item", label = "Items" },
    { key = "set", label = "Sets" },
    { key = "model", label = "Models" },
    { key = "mount", label = "Mounts" },
}
RB.HISTORY_SIZE = 50
RB.POLL_SECONDS = 0.5
RB.SEARCH_DELAY = 0.3

RB.INVENTORY_TYPES = {
    { 1, "Head" }, { 2, "Neck" }, { 3, "Shoulder" }, { 4, "Shirt" }, { 5, "Chest" }, { 6, "Waist" }, { 7, "Legs" },
    { 8, "Feet" }, { 9, "Wrist" }, { 10, "Hands" }, { 11, "Finger" }, { 12, "Trinket" }, { 13, "One-Hand" },
    { 14, "Shield" }, { 15, "Ranged" }, { 16, "Back" }, { 17, "Two-Hand" }, { 19, "Tabard" }, { 21, "Main Hand" },
    { 22, "Off Hand" }, { 23, "Held In Off-hand" }, { 25, "Thrown" }, { 26, "Wand / Gun" }, { 28, "Relic" },
}
RB.ARMOUR_TYPES = { { 1, "Cloth" }, { 2, "Leather" }, { 3, "Mail" }, { 4, "Plate" } }
RB.QUALITIES = {
    { 0, "Poor" }, { 1, "Common" }, { 2, "Uncommon" }, { 3, "Rare" }, { 4, "Epic" }, { 5, "Legendary" },
    { 6, "Artifact" }, { 7, "Heirloom" },
}
RB.EXPANSIONS = {
    { 0, "Classic" }, { 1, "Burning Crusade" }, { 2, "Wrath" }, { 3, "Cataclysm" }, { 4, "Pandaria" }, { 5, "Draenor" },
    { 6, "Legion" }, { 7, "Battle for Azeroth" }, { 8, "Shadowlands" }, { 9, "Dragonflight" }, { 10, "War Within" },
    { 11, "Midnight" },
}
-- Retail has no boss classification (bosses are elites), so ranks are 3.3.5's without one.
RB.CLASSIFICATIONS = { { 0, "Normal" }, { 1, "Elite" }, { 3, "Rare elite" }, { 2, "Rare" } }
RB.RACES = {
    { 1, "Human" }, { 2, "Orc" }, { 3, "Dwarf" }, { 4, "Night Elf" }, { 5, "Undead" }, { 6, "Tauren" }, { 7, "Gnome" },
    { 8, "Troll" }, { 9, "Goblin" }, { 10, "Blood Elf" }, { 11, "Draenei" }, { 22, "Worgen" }, { 24, "Pandaren" },
}
RB.NPC_KINDS = { { 1, "Humanoid NPC" }, { 0, "Plain model" } }
RB.CLASSES = {
    { 1, "Warrior" }, { 2, "Paladin" }, { 3, "Hunter" }, { 4, "Rogue" }, { 5, "Priest" }, { 6, "Death Knight" },
    { 7, "Shaman" }, { 8, "Mage" }, { 9, "Warlock" }, { 10, "Monk" }, { 11, "Druid" }, { 12, "Demon Hunter" },
    { 13, "Evoker" },
}

-- Filters per kind: the RetailSearch filter name, its label and its choices (a list, or a function of the index info).
RB.FILTERS = {
    creature = {
        { key = "creatureType", label = "Type", choices = function(info) return RB.NamedChoices(info.creatureTypes) end },
        { key = "family", label = "Family", choices = function(info) return RB.NamedChoices(info.families) end },
        { key = "classification", label = "Rank", choices = RB.CLASSIFICATIONS },
        { key = "race", label = "Race", choices = RB.RACES },
        { key = "humanoid", label = "Kind", choices = RB.NPC_KINDS },
    },
    item = {
        { key = "inventoryType", label = "Slot", choices = RB.INVENTORY_TYPES },
        { key = "armour", label = "Armour", choices = RB.ARMOUR_TYPES },
        { key = "weaponSubclass", label = "Weapon", choices = function(info) return RB.NamedChoices(info.weaponSubclasses) end },
        { key = "quality", label = "Quality", choices = RB.QUALITIES },
        { key = "expansion", label = "Expansion", choices = RB.EXPANSIONS },
    },
    set = {
        { key = "classId", label = "Class", choices = RB.CLASSES },
        { key = "armour", label = "Armour", choices = RB.ARMOUR_TYPES },
        { key = "expansion", label = "Expansion", choices = RB.EXPANSIONS },
    },
    model = {},
    mount = {},
}

function RB.NamedChoices(list)
    local choices = {}
    for _, entry in ipairs(list or {}) do
        choices[#choices + 1] = { entry.id, entry.name }
    end
    table.sort(choices, function(a, b) return a[2] < b[2] end)
    return choices
end

function RB.Available()
    return type(RetailSearch) == "function"
end

function RB.Print(message)
    DEFAULT_CHAT_FRAME:AddMessage("|cff66bbffRetail Browser:|r " .. message)
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

-- Saved variables: favourites by "kind:id", and the most recent details first.
local function InitDB()
    RetailBrowserDB = RetailBrowserDB or {}
    RetailBrowserDB.favourites = RetailBrowserDB.favourites or {}
    RetailBrowserDB.history = RetailBrowserDB.history or {}
    if RetailBrowserDB.indexAtLogin == nil then
        RetailBrowserDB.indexAtLogin = true
    end
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

function RB.IsFavourite(kind, id)
    return RetailBrowserDB.favourites[EntryKey(kind, id)] ~= nil
end

function RB.ToggleFavourite(kind, id, name)
    local key = EntryKey(kind, id)
    if RetailBrowserDB.favourites[key] then
        RetailBrowserDB.favourites[key] = nil
    else
        RetailBrowserDB.favourites[key] = { kind = kind, id = id, name = name }
    end
    return RetailBrowserDB.favourites[key] ~= nil
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
