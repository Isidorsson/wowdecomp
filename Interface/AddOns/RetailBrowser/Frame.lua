-- The browser window: kind tabs, the search box and filters, the status line, and the search requests behind them.

local RB = RetailBrowser

local FRAME_WIDTH = 960
local FRAME_HEIGHT = 620
local MAX_FILTERS = 5
local FILTER_WIDTH = 88
local FILTERS_LEFT = 200

RB.state = { kind = "creature", source = "search", text = "", page = 1, filters = {} }

local frame = CreateFrame("Frame", "RetailBrowserFrame", UIParent)
RB.frame = frame
frame:SetSize(FRAME_WIDTH, FRAME_HEIGHT)
frame:SetPoint("CENTER")
frame:SetBackdrop(RB.PANEL_BACKDROP)
frame:SetToplevel(true)
frame:SetFrameStrata("DIALOG")
frame:EnableMouse(true)
frame:SetMovable(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
frame:Hide()
tinsert(UISpecialFrames, "RetailBrowserFrame")

local title = RB.CreateText(frame, "GameFontNormalLarge")
title:SetPoint("TOPLEFT", 22, -18)
title:SetText("Retail Browser")

local status = RB.CreateText(frame, "GameFontDisableSmall", "RIGHT")
status:SetPoint("TOPRIGHT", -40, -22)
RB.statusText = status

local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -6, -6)

local atLogin = CreateFrame("CheckButton", "RetailBrowserIndexAtLogin", frame, "UICheckButtonTemplate")
atLogin:SetSize(22, 22)
atLogin:SetPoint("TOPLEFT", title, "TOPRIGHT", 12, 4)
_G[atLogin:GetName() .. "Text"]:SetText("Build the index at login")
atLogin:SetScript("OnClick", function(self) RB.SetIndexAtLogin(self:GetChecked()) end)
atLogin:SetScript("OnShow", function(self) self:SetChecked(RB.IndexAtLogin()) end)

function RB.SetStatus(text)
    status:SetText(text or "")
end

-- Tabs: the five kinds, then the saved lists.
local tabs = {}
local function SelectTab(key)
    for _, tab in ipairs(tabs) do
        if tab.key == key then tab:LockHighlight() else tab:UnlockHighlight() end
    end
end

local function AddTab(key, label, onClick)
    local tab = RB.CreateButton(frame, label, 92, onClick)
    tab.key = key
    tab:SetPoint("TOPLEFT", 20 + (#tabs * 96) + (#tabs >= #RB.KINDS and 24 or 0), -44)
    tabs[#tabs + 1] = tab
end

for _, kind in ipairs(RB.KINDS) do
    AddTab(kind.key, kind.label, function() RB.SetKind(kind.key) end)
end
AddTab("favourites", "Favourites", function() RB.ShowSaved("favourites") end)
AddTab("history", "History", function() RB.ShowSaved("history") end)

-- Search box and filters.
local search = RB.CreateEditBox(frame, "RetailBrowserSearchBox", 170)
search:SetPoint("TOPLEFT", 30, -92)
RB.searchBox = search
local searchLabel = RB.CreateText(frame, "GameFontNormalSmall")
searchLabel:SetPoint("BOTTOMLEFT", search, "TOPLEFT", -4, 2)
searchLabel:SetText("Search (name or id)")

local dropdowns = {}
for i = 1, MAX_FILTERS do
    local dropdown = RB.CreateDropdown(frame, "RetailBrowserFilter" .. i, FILTER_WIDTH, function(self, value)
        RB.state.filters[self.filterKey] = value
        RB.Search(true)
    end)
    dropdown:SetPoint("TOPLEFT", FILTERS_LEFT + (i - 1) * (FILTER_WIDTH + 30), -86)
    dropdowns[i] = dropdown
end

local levelMin = RB.CreateEditBox(frame, "RetailBrowserLevelMin", 34, true)
local levelMax = RB.CreateEditBox(frame, "RetailBrowserLevelMax", 34, true)
levelMin:SetPoint("TOPLEFT", FILTERS_LEFT + MAX_FILTERS * (FILTER_WIDTH + 30) + 4, -92)
levelMax:SetPoint("LEFT", levelMin, "RIGHT", 10, 0)
local levelLabel = RB.CreateText(frame, "GameFontNormalSmall")
levelLabel:SetPoint("BOTTOMLEFT", levelMin, "TOPLEFT", -4, 2)
levelLabel:SetText("Item level")
local function LevelChanged()
    RB.state.filters.minItemLevel = tonumber(levelMin:GetText())
    RB.state.filters.maxItemLevel = tonumber(levelMax:GetText())
    RB.Search(false)
end
levelMin:SetScript("OnTextChanged", LevelChanged)
levelMax:SetScript("OnTextChanged", LevelChanged)

local function ShowLevelRange(shown)
    for _, region in ipairs({ levelMin, levelMax, levelLabel }) do
        if shown then region:Show() else region:Hide() end
    end
end

-- The dropdowns of a kind, with the choices the index has names for.
local function ConfigureFilters(kind)
    local filters = RB.FILTERS[kind] or {}
    for i, dropdown in ipairs(dropdowns) do
        local filter = filters[i]
        if filter then
            local choices = filter.choices
            if type(choices) == "function" then
                choices = choices(RB.info or {})
            end
            dropdown.filterKey = filter.key
            RB.SetDropdown(dropdown, filter.label, choices)
            dropdown:Show()
        else
            dropdown:Hide()
        end
    end
    levelMin:SetText("")
    levelMax:SetText("")
    ShowLevelRange(kind == "item")
end

function RB.SetKind(kind)
    local state = RB.state
    state.kind, state.source, state.page, state.filters = kind, "search", 1, {}
    SelectTab(kind)
    ConfigureFilters(kind)
    RB.Search(true)
end

function RB.ShowSaved(source)
    RB.state.source = source
    SelectTab(source)
    RB.ShowRows(source == "favourites" and RB.Favourites() or RB.History(), source)
end

-- Searches now, or after the typing pause when later is set; a search before the index is ready polls until it is.
function RB.Search(now)
    if RB.state.source ~= "search" then
        return
    end
    RB.searchAt = GetTime() + (now and 0 or RB.SEARCH_DELAY)
end

local function RunSearch()
    local state = RB.state
    if state.source ~= "search" then
        return
    end
    local result, err = RetailSearch(state.kind, state.text, state.filters, state.page)
    if not result then
        RB.SetStatus("|cffff4040" .. tostring(err) .. "|r")
        return
    end
    if result.status ~= "ok" then
        RB.SetStatus(result.status == "failed" and ("|cffff4040index failed: " .. tostring(result.error) .. "|r") or "Building the index...")
        RB.pollAt = GetTime() + RB.POLL_SECONDS
        return
    end
    RB.ShowResult(result)
end

search:SetScript("OnTextChanged", function(self)
    RB.state.text = self:GetText()
    RB.state.page = 1
    RB.Search(false)
end)
search:SetScript("OnEnterPressed", function(self)
    self:ClearFocus()
    RB.Search(true)
    RB.FocusList()
end)

-- Index status: counts once ready; the filter choices that come from the index are refreshed then.
local function RefreshInfo()
    local info = RetailBrowserInfo()
    if info.status == "ready" then
        local firstReady = not RB.info
        RB.info = info
        local c = info.counts
        RB.SetStatus(string.format("%d creatures, %d items, %d sets, %d models, %d mounts", c.creature, c.item, c.set, c.model, c.mount))
        if firstReady and RB.state.source == "search" then
            ConfigureFilters(RB.state.kind)
        end
        return true
    end
    RB.SetStatus(info.status == "failed" and ("|cffff4040index failed: " .. tostring(info.error) .. "|r") or "Building the index...")
    return info.status == "failed"
end

frame:SetScript("OnUpdate", function()
    local now = GetTime()
    if RB.pollAt and now >= RB.pollAt then
        RB.pollAt = nil
        if RefreshInfo() then
            RB.Search(true)
        else
            RB.pollAt = now + RB.POLL_SECONDS
        end
    end
    if RB.searchAt and now >= RB.searchAt then
        RB.searchAt = nil
        RunSearch()
    end
end)

frame:SetScript("OnShow", function()
    if not RefreshInfo() then
        RB.pollAt = GetTime() + RB.POLL_SECONDS
    end
end)

function RB.Toggle()
    if frame:IsShown() then
        frame:Hide()
        return
    end
    frame:Show()
    if not RB.started then
        RB.started = true
        RB.SetKind("creature")
    end
end

function RetailBrowser_Toggle()
    if RB.Available() then
        RB.Toggle()
    end
end
