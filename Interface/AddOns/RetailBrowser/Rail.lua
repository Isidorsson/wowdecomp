-- The rail, the left column: the datasets (Creatures to Mounts, Ctrl+1 to Ctrl+5) with their counts, a divider, the
-- saved lists (Favourites F8, History F9), Build SQL (B; its count is the rows it builds), and Index at login at the
-- bottom. Below 1200 px of
-- window width it collapses to 48 px of short labels; the tooltips keep the full names and keys.

local RB = RetailBrowser
local T = RB.THEME

local ENTRY_HEIGHT = 34
local COLLAPSED_ENTRY_HEIGHT = 28
local ENTRY_GAP = 2
local EDGE_WIDTH = 2
local DIVIDER_SPACE = 13

local rail = RB.CreatePanel(RB.railCol, "rail")
rail:SetAllPoints()
RB.rail = rail

local heading = RB.CreateText(rail, "RetailBrowserHeadFont")
heading:SetPoint("TOPLEFT", 10, -9)
heading:SetText("DATASETS")
RB.SetColor(heading, T.section)

local entries = {}

local function CreateEntry(key, label, short, hotkey, onClick)
    local entry = CreateFrame("Button", nil, rail)
    entry.key, entry.baseLabel, entry.short, entry.hotkey = key, label, short, hotkey
    entry.fill = RB.CreateFill(entry, "BACKGROUND", T.accentFill)
    entry.edge = RB.CreateFill(entry, "BORDER", T.accent)
    entry.edge:ClearAllPoints()
    entry.edge:SetPoint("TOPLEFT")
    entry.edge:SetPoint("BOTTOMLEFT")
    entry.edge:SetWidth(EDGE_WIDTH)
    RB.SetHoverHighlight(entry)
    entry.label = RB.CreateText(entry, "GameFontHighlightSmall")
    entry.count = RB.CreateText(entry, "RetailBrowserValueFont", "RIGHT")
    entry.count:SetPoint("TOPRIGHT", -8, -6)
    entry.hint = RB.CreateText(entry, "RetailBrowserLabelFont")
    entry.hint:SetPoint("BOTTOMLEFT", 10, 5)
    entry.hint:SetText(hotkey)
    entry:SetScript("OnClick", onClick)
    RB.AttachTip(entry, "tab_" .. key)
    entries[#entries + 1] = entry
    return entry
end

for i, kind in ipairs(RB.KINDS) do
    CreateEntry(kind.key, kind.label, kind.label:sub(1, 3), "Ctrl+" .. i, function() RB.SetKind(kind.key) end)
end
CreateEntry("favourites", "Favourites", "Fav", "F8", function() RB.ShowSaved("favourites") end)
CreateEntry("history", "History", "His", "F9", function() RB.ShowSaved("history") end)
CreateEntry("build", "Build SQL", "SQL", "B", function() RB.ShowBuild() end)

-- A divider above the saved lists and one above Build SQL.
local dividers = {}
for i = 1, 2 do
    dividers[i] = RB.CreateFill(rail, "ARTWORK", T.borderDim)
    dividers[i]:ClearAllPoints()
    dividers[i]:SetHeight(1)
end
local DIVIDER_BEFORE = { favourites = 1, build = 2 }

-- Bottom: Index at login.
local atLogin = CreateFrame("CheckButton", "RetailBrowserIndexAtLogin", rail, "UICheckButtonTemplate")
atLogin:SetSize(20, 20)
local atLoginText = _G[atLogin:GetName() .. "Text"]
atLoginText:SetFontObject(RetailBrowserLabelFont)
atLoginText:SetText("Index at login")
atLogin:SetScript("OnClick", function(self) RB.SetIndexAtLogin(self:GetChecked()) end)
atLogin:SetScript("OnShow", function(self) self:SetChecked(RB.IndexAtLogin()) end)
RB.AttachTip(atLogin, "indexAtLogin")

-- Expanded: the full label and count on top, the key under them. Collapsed: the short label alone.
local function LayOutEntry(entry, collapsed)
    entry.label:ClearAllPoints()
    if collapsed then
        entry.label:SetPoint("CENTER", 1, 0)
        entry.label:SetText(entry.short)
    else
        entry.label:SetPoint("TOPLEFT", 10, -6)
        entry.label:SetText(entry.baseLabel)
    end
    RB.Shown(entry.count, not collapsed)
    RB.Shown(entry.hint, not collapsed)
    entry:SetHeight(collapsed and COLLAPSED_ENTRY_HEIGHT or ENTRY_HEIGHT)
end

local function PlaceEntries(collapsed)
    local y = collapsed and -6 or -26
    for i, entry in ipairs(entries) do
        LayOutEntry(entry, collapsed)
        local divider = dividers[DIVIDER_BEFORE[entry.key] or 0]
        if divider then
            divider:SetPoint("TOPLEFT", 8, y - 6)
            divider:SetPoint("TOPRIGHT", -8, y - 6)
            y = y - DIVIDER_SPACE
        end
        entry:ClearAllPoints()
        entry:SetPoint("TOPLEFT", 4, y)
        entry:SetPoint("TOPRIGHT", -4, y)
        y = y - entry:GetHeight() - ENTRY_GAP
    end
end

-- Collapsed, Index at login keeps its box only.
local function PlaceBottom(collapsed)
    atLogin:ClearAllPoints()
    if collapsed then
        atLogin:SetPoint("BOTTOM", 0, 6)
    else
        atLogin:SetPoint("BOTTOMLEFT", 6, 6)
    end
    RB.Shown(atLoginText, not collapsed)
end

local collapsedNow
local function LayOutRail()
    local collapsed = RB.Layout.RailCollapsed(RB.frame:GetWidth())
    if collapsed == collapsedNow then
        return
    end
    collapsedNow = collapsed
    RB.Shown(heading, not collapsed)
    PlaceEntries(collapsed)
    PlaceBottom(collapsed)
end
RB.OnColumnsResized(LayOutRail)

function RB.ClearHistoryNow()
    RB.ClearHistory()
    if RB.state.source == "history" then
        RB.ShowSaved("history")
    end
    RB.RefreshRail()
end

-- Counts (once the index is ready) and the selected entry.
function RB.RefreshRail()
    local counts = RB.info and RB.info.counts or {}
    local selected = RB.state.source == "search" and RB.state.kind or RB.state.source
    if RB.page == "build" then
        selected = "build"
    end
    for _, entry in ipairs(entries) do
        local count = counts[entry.key]
        if entry.key == "favourites" then
            count = #RB.Favourites()
        elseif entry.key == "history" then
            count = #RB.History()
        elseif entry.key == "build" then
            count = RB.BuildScopeCount and RB.BuildScopeCount()
        end
        entry.count:SetText(count and tostring(count) or "")
        local on = entry.key == selected
        RB.Shown(entry.fill, on)
        RB.Shown(entry.edge, on)
        RB.SetColor(entry.label, on and T.accent or T.value)
    end
end
