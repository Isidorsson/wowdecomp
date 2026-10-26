-- The browser window: about 1280x800 (40 px inside a smaller screen), resizable from its corner grip down to 960x600,
-- moved by its title bar, its place, size and scale saved per character and kept on screen. Title bar, kind tabs,
-- the query row (active filter chips, pager, CSV) and the status line that says who owns the keys.

local RB = RetailBrowser
local T = RB.THEME

RB.LAYOUT = {
    pad = 10, gridTop = 92, rowHeight = 16, headHeight = 17, filterHeight = 19, bulkHeight = 24, statusHeight = 20,
}
local L = RB.LAYOUT
local DEFAULT_WIDTH, DEFAULT_HEIGHT = 1280, 800
local MIN_WIDTH, MIN_HEIGHT = 960, 600
local SCREEN_MARGIN = 40
local DEFAULT_X, DEFAULT_TOP = -14, 134
local SCALES = { 0.9, 1, 1.1 }
local MAX_CHIPS = 8

local frame = CreateFrame("Frame", "RetailBrowserFrame", UIParent)
RB.frame = frame
frame:SetSize(DEFAULT_WIDTH, DEFAULT_HEIGHT)
frame:SetBackdrop(RB.WINDOW_BACKDROP)
RB.SetBack(frame, T.panel)
RB.SetBorder(frame, T.border)
frame:SetToplevel(true)
frame:SetFrameStrata("DIALOG")
frame:EnableMouse(true)
frame:SetMovable(true)
frame:SetResizable(true)
frame:SetMinResize(MIN_WIDTH, MIN_HEIGHT)
frame:SetClampedToScreen(true)
frame:Hide()
tinsert(UISpecialFrames, "RetailBrowserFrame")

-- Title bar: drag it to move the window.
local titleBar = CreateFrame("Frame", nil, frame)
titleBar:SetPoint("TOPLEFT", 6, -6)
titleBar:SetPoint("TOPRIGHT", -6, -6)
titleBar:SetHeight(24)
RB.CreateFill(titleBar, "BACKGROUND", T.band)
titleBar:EnableMouse(true)
titleBar:RegisterForDrag("LeftButton")
titleBar:SetScript("OnDragStart", function() frame:StartMoving() end)
titleBar:SetScript("OnDragStop", function()
    frame:StopMovingOrSizing()
    RB.SaveWindowPlace()
end)

local title = RB.CreateText(titleBar, "GameFontNormal")
title:SetPoint("LEFT", 8, 0)
title:SetText("Retail Browser")
local status = RB.CreateText(titleBar, "RetailBrowserLabelFont")
status:SetPoint("LEFT", title, "RIGHT", 12, 0)
status:SetWidth(330)
RB.statusText = status

function RB.SetStatus(text)
    status:SetText(text or "")
end

local close = CreateFrame("Button", nil, titleBar, "UIPanelCloseButton")
close:SetSize(26, 26)
close:SetPoint("RIGHT", 2, 0)
-- The template hides its parent, which here is the title bar, not the window.
close:SetScript("OnClick", function() frame:Hide() end)
RB.AttachTip(close, "close")

local scaleButtons = {}
for i = #SCALES, 1, -1 do
    local scale = SCALES[i]
    local button = RB.CreateButton(titleBar, math.floor(scale * 100 + 0.5) .. "%", 44, 18, function() RB.SetWindowScale(scale) end)
    button:SetPoint("RIGHT", i == #SCALES and close or scaleButtons[i + 1], "LEFT", -3, 0)
    button.scale = scale
    RB.AttachTip(button, "scale")
    scaleButtons[i] = button
end

local atLogin = CreateFrame("CheckButton", "RetailBrowserIndexAtLogin", titleBar, "UICheckButtonTemplate")
atLogin:SetSize(20, 20)
atLogin:SetPoint("RIGHT", scaleButtons[1], "LEFT", -96, 0)
_G[atLogin:GetName() .. "Text"]:SetFontObject(RetailBrowserLabelFont)
_G[atLogin:GetName() .. "Text"]:SetText("Index at login")
atLogin:SetScript("OnClick", function(self) RB.SetIndexAtLogin(self:GetChecked()) end)
atLogin:SetScript("OnShow", function(self) self:SetChecked(RB.IndexAtLogin()) end)
RB.AttachTip(atLogin, "indexAtLogin")

-- Size and place: the saved size (default about 1280x800) within the screen less its margins; offsets of TOPRIGHT
-- from UIParent's TOPRIGHT, in UIParent units.
local function ScreenSize()
    local scale = RB.CharSetting("scale", 1)
    return UIParent:GetWidth() / scale, UIParent:GetHeight() / scale
end

local function WindowSize()
    local screenWidth, screenHeight = ScreenSize()
    local maxWidth, maxHeight = screenWidth - 2 * SCREEN_MARGIN, screenHeight - 2 * SCREEN_MARGIN
    local width = math.min(RB.CharSetting("width", DEFAULT_WIDTH), maxWidth)
    local height = math.min(RB.CharSetting("height", DEFAULT_HEIGHT), maxHeight)
    return math.max(width, math.min(MIN_WIDTH, maxWidth)), math.max(height, math.min(MIN_HEIGHT, maxHeight))
end

function RB.SaveWindowPlace()
    local ratio = frame:GetEffectiveScale() / UIParent:GetEffectiveScale()
    RB.SetCharSetting("x", frame:GetRight() * ratio - UIParent:GetRight())
    RB.SetCharSetting("y", frame:GetTop() * ratio - UIParent:GetTop())
end

local function DefaultY(height)
    local _, screenHeight = ScreenSize()
    return -math.min(DEFAULT_TOP, math.max(SCREEN_MARGIN, screenHeight - height - SCREEN_MARGIN))
end

local function PlaceWindow()
    local scale = RB.CharSetting("scale", 1)
    frame:SetScale(scale)
    for _, button in ipairs(scaleButtons) do
        button:SetOn(button.scale == scale)
    end
    local width, height = WindowSize()
    frame:SetSize(width, height)
    frame:SetMaxResize(ScreenSize())
    frame:ClearAllPoints()
    frame:SetPoint("TOPRIGHT", UIParent, "TOPRIGHT", RB.CharSetting("x", DEFAULT_X) / scale, RB.CharSetting("y", DefaultY(height)) / scale)
end

function RB.SetWindowScale(scale)
    if frame:IsShown() and frame:GetRight() then
        RB.SaveWindowPlace()
    end
    RB.SetCharSetting("scale", scale)
    PlaceWindow()
end

function RB.ResetWindowPlace()
    for _, key in ipairs({ "x", "y", "width", "height" }) do
        RB.SetCharSetting(key, nil)
    end
    PlaceWindow()
end

-- The corner grip.
local grip = CreateFrame("Button", nil, frame)
grip:SetSize(16, 16)
grip:SetPoint("BOTTOMRIGHT", -4, 4)
grip:SetNormalTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Up")
grip:SetHighlightTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Highlight")
grip:SetPushedTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Down")
grip:SetScript("OnMouseDown", function() frame:StartSizing("BOTTOMRIGHT") end)
RB.AttachTip(grip, "grip")
grip:SetScript("OnMouseUp", function()
    frame:StopMovingOrSizing()
    RB.SetCharSetting("width", math.floor(frame:GetWidth() + 0.5))
    RB.SetCharSetting("height", math.floor(frame:GetHeight() + 0.5))
    RB.SaveWindowPlace()
end)

frame:SetScript("OnSizeChanged", function()
    if RB.OnWindowResized then
        RB.OnWindowResized()
    end
end)

-- Kind tabs (Ctrl+1 to Ctrl+5), then the saved lists (F8, F9), and Clear history.
local tabs = {}
local function AddTab(key, label, hotkey, onClick)
    local tab = RB.CreateTab(frame, "RetailBrowserTab" .. (#tabs + 1), label, onClick)
    tab.key, tab.baseLabel, tab.hotkey = key, label, hotkey
    RB.AttachTip(tab, "tab_" .. key)
    local previous = tabs[#tabs]
    if previous then
        tab:SetPoint("LEFT", previous, "RIGHT", key == "favourites" and 12 or -4, 0)
    else
        tab:SetPoint("TOPLEFT", L.pad, -32)
    end
    tabs[#tabs + 1] = tab
end

for i, kind in ipairs(RB.KINDS) do
    AddTab(kind.key, kind.label, "Ctrl+" .. i, function() RB.SetKind(kind.key) end)
end
AddTab("favourites", "Favourites", "F8", function() RB.ShowSaved("favourites") end)
AddTab("history", "History", "F9", function() RB.ShowSaved("history") end)


function RB.ClearHistoryNow()
    RB.ClearHistory()
    if RB.state.source == "history" then
        RB.ShowSaved("history")
    end
    RB.RefreshTabs()
end

-- Tab labels: the hotkey and, once the index is ready, how many entries the kind has.
function RB.RefreshTabs()
    local counts = RB.info and RB.info.counts or {}
    local selected = RB.state.source == "search" and RB.state.kind or RB.state.source
    for _, tab in ipairs(tabs) do
        local count = counts[tab.key]
        if tab.key == "favourites" then
            count = #RB.Favourites()
        elseif tab.key == "history" then
            count = #RB.History()
        end
        tab:SetLabel(string.format("%s %s %s", tab.baseLabel, RB.Colored(count and tostring(count) or "", T.value), RB.Key(tab.hotkey)))
        tab:SetOn(tab.key == selected)
    end
end

local tabLine = frame:CreateTexture(nil, "ARTWORK")
tabLine:SetTexture("Interface\\ChatFrame\\ChatFrameBackground")
RB.SetVertex(tabLine, T.borderDim)
tabLine:SetPoint("TOPLEFT", L.pad, -62)
tabLine:SetPoint("TOPRIGHT", -L.pad, -62)
tabLine:SetHeight(1)

-- Query row: a chip per active filter (click clears it), Clear filters, the pager and Export CSV.
local queryRow = CreateFrame("Frame", nil, frame)
queryRow:SetPoint("TOPLEFT", L.pad, -66)
queryRow:SetPoint("TOPRIGHT", -L.pad, -66)
queryRow:SetHeight(20)

local chips = {}
for i = 1, MAX_CHIPS do
    local chip = RB.CreateButton(queryRow, "", 60, 18, function(self) RB.ClearColumnFilter(self.column) end)
    chip:SetPoint("LEFT", i == 1 and queryRow or chips[i - 1], i == 1 and "LEFT" or "RIGHT", i == 1 and 0 or 4, 0)
    chip:Hide()
    RB.AttachTip(chip, "chip")
    chips[i] = chip
end
local clearFilters = RB.CreateButton(queryRow, "Clear filters", 84, 18, function() RB.ClearAllFilters() end)
RB.AttachTip(clearFilters, "clearFilters")
local noFilters = RB.CreateText(queryRow, "RetailBrowserLabelFont")
noFilters:SetPoint("LEFT", 2, 0)
noFilters:SetText("Filters sit on top of their column; they search the whole index.")

local export = RB.CreateButton(queryRow, "Export CSV " .. RB.Key("E"), 100, 18, function() RB.ExportCsv() end)
export:SetPoint("RIGHT", 0, 0)
RB.AttachTip(export, "exportCsv")
local nextPage = RB.CreateButton(queryRow, "Next " .. RB.Key("]"), 60, 18, function() RB.TurnPage(1) end)
nextPage:SetPoint("RIGHT", export, "LEFT", -10, 0)
RB.AttachTip(nextPage, "nextPage")
local pageText = RB.CreateText(queryRow, "RetailBrowserValueFont", "RIGHT")
pageText:SetPoint("RIGHT", nextPage, "LEFT", -6, 0)
local prevPage = RB.CreateButton(queryRow, RB.Key("[") .. " Prev", 60, 18, function() RB.TurnPage(-1) end)
prevPage:SetPoint("RIGHT", pageText, "LEFT", -6, 0)
RB.AttachTip(prevPage, "prevPage")

function RB.SetPageText(text)
    pageText:SetText(text or "")
end

-- chipList: { { column = column, text = "Slot = Chest" }, ... }
function RB.ShowChips(chipList)
    local last
    for i, chip in ipairs(chips) do
        local entry = chipList[i]
        if entry then
            chip.column = entry.column
            chip:SetLabel(entry.text .. " " .. RB.Colored("x", T.bad))
            chip:SetWidth(chip.label:GetStringWidth() + 20)
            chip:Show()
            last = chip
        else
            chip:Hide()
        end
    end
    RB.Shown(noFilters, not last)
    clearFilters:ClearAllPoints()
    if last then
        clearFilters:SetPoint("LEFT", last, "RIGHT", 8, 0)
    end
    RB.Shown(clearFilters, last)
end

-- Status line: the key owner, then the key hints.
local statusLine = CreateFrame("Frame", nil, frame)
statusLine:SetPoint("BOTTOMLEFT", L.pad, 6)
statusLine:SetPoint("BOTTOMRIGHT", -L.pad - 14, 6)
statusLine:SetHeight(L.statusHeight)
local keysButton = RB.CreateButton(statusLine, "", 110, 18, function() RB.ToggleGridKeys() end)
keysButton:SetPoint("LEFT", 0, 0)
RB.AttachTip(keysButton, "keys")
-- Clear history sits here, not by the tabs: the tab strip needs the whole width of a small window.
local clearHistory = RB.CreateButton(statusLine, "Clear history " .. RB.Key("Ctrl+Del"), 130, 18, function() RB.ClearHistoryNow() end)
clearHistory:SetPoint("RIGHT", 0, 0)
RB.AttachTip(clearHistory, "clearHistory")
local hints = RB.CreateText(statusLine, "RetailBrowserLabelFont")
hints:SetPoint("LEFT", keysButton, "RIGHT", 8, 0)
hints:SetPoint("RIGHT", clearHistory, "LEFT", -8, 0)

local OWNER_TEXT = {
    grid = RB.Colored("Keys: grid", T.good),
    search = RB.Colored("Keys: search", T.title),
    filter = RB.Colored("Keys: filter", T.title),
    build = RB.Colored("Keys: Build", T.title),
    game = RB.Colored("Keys: game", T.disabled),
}
local OWNER_HINTS = {
    grid = "Up/Dn move  Enter details  Space check  Left/Right display  / search  Tab filters  M morph  T target  I add  Y try on"
        .. "  G GM  F fav  B build  D details  P displays  Ctrl+Up/Dn drawer  Esc clear/close",
    search = "Typing searches the whole index.  Enter or Esc: back to the grid.",
    filter = "Range: 200-  -300  100-200  >=5  =5.  Tab next filter, Enter or Esc: back to the grid.",
    build = "Changes rebuild after 0.3 s.  Esc: back to the grid.",
    game = "Click the grid or this button to give it the keys.",
}

function RB.ShowKeyOwner(owner)
    keysButton:SetLabel(OWNER_TEXT[owner] or OWNER_TEXT.game)
    hints:SetText(OWNER_HINTS[owner] or OWNER_HINTS.game)
end

frame:SetScript("OnShow", function()
    PlaceWindow()
    if RB.OnWindowShown then
        RB.OnWindowShown()
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
    RB.FocusGrid()
end

function RetailBrowser_Toggle()
    if RB.Available() then
        RB.Toggle()
    end
end
