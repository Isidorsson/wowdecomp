-- The browser window, the shell of the Ledger layout: about 1280x800 (40 px inside a smaller screen), resizable from
-- its corner grip down to 1100x600, moved by its title bar, its place and size saved per character and kept on
-- screen. It is drawn at scale 1 for everyone (an old saved scale is ignored). Under the title bar the rail (Rail.lua)
-- and a page: browse (the main column with the query row, the grid, the bulk bar and the status line that says who
-- owns the keys, and the inspector, Inspector.lua) or build (the Build SQL page, BuildPage.lua). Layout.lua does
-- the maths.

local RB = RetailBrowser
local T = RB.THEME
local Layout = RB.Layout

RB.LAYOUT = {
    rowHeight = 16, headHeight = 17, filterHeight = 19, searchHeight = 24, queryHeight = 22, bulkHeight = 24, statusHeight = 20,
}
local L = RB.LAYOUT
local TITLE_SPACE = 34
local BOTTOM_SPACE = 6
local DEFAULT_WIDTH, DEFAULT_HEIGHT = 1280, 800
local MIN_WIDTH, MIN_HEIGHT = 1100, 600
local SCREEN_MARGIN = 40
local DEFAULT_X, DEFAULT_TOP = -14, 134
local MAX_CHIPS = 8
local CHIP_GAP = 4

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
status:SetPoint("RIGHT", -40, 0)
RB.statusText = status

function RB.SetStatus(text)
    status:SetText(text or "")
end

-- A flat x: muted, teal under the mouse.
local close = CreateFrame("Button", nil, titleBar)
close:SetSize(22, 20)
close:SetPoint("RIGHT", -2, 0)
close.text = RB.CreateText(close, "GameFontHighlight", "CENTER")
close.text:SetPoint("CENTER", 0, 1)
close.text:SetText("x")
RB.SetColor(close.text, T.label)
close:SetScript("OnEnter", function(self) RB.SetColor(self.text, T.accent) end)
close:SetScript("OnLeave", function(self) RB.SetColor(self.text, T.label) end)
close:SetScript("OnClick", function() frame:Hide() end)
RB.AttachTip(close, "close")

-- The three columns. The rail and the inspector take their widths from Layout; the main column takes the rest.
local function Column()
    local column = CreateFrame("Frame", nil, frame)
    column:SetPoint("TOP", 0, -TITLE_SPACE)
    column:SetPoint("BOTTOM", 0, BOTTOM_SPACE)
    return column
end
RB.railCol, RB.mainCol, RB.inspectorCol = Column(), Column(), Column()
RB.railCol:SetPoint("LEFT", Layout.PAD, 0)
RB.inspectorCol:SetPoint("RIGHT", -Layout.PAD, 0)
RB.mainCol:SetPoint("LEFT", RB.railCol, "RIGHT", Layout.GAP, 0)
RB.mainCol:SetPoint("RIGHT", RB.inspectorCol, "LEFT", -Layout.GAP, 0)
-- The build page takes the place of the main column and the inspector.
RB.buildCol = Column()
RB.buildCol:SetPoint("LEFT", RB.railCol, "RIGHT", Layout.GAP, 0)
RB.buildCol:SetPoint("RIGHT", -Layout.PAD, 0)
RB.buildCol:Hide()
RB.page = "browse"

-- page: "browse" (the grid and the inspector) or "build" (the Build SQL page).
function RB.ShowPage(page)
    RB.page = page
    local build = page == "build"
    RB.Shown(RB.mainCol, not build)
    RB.Shown(RB.inspectorCol, not build)
    RB.Shown(RB.buildCol, build)
    if RB.RefreshRail then
        RB.RefreshRail()
    end
end

-- The columns' height, from the window's (anchored sizes can lag a frame behind a resize).
function RB.ColumnHeight()
    return frame:GetHeight() - TITLE_SPACE - BOTTOM_SPACE
end

function RB.RailWidth()
    return Layout.RailWidth(frame:GetWidth())
end

-- The inspector's set width (the splitter, saved per character), within what this window allows.
function RB.InspectorWidth()
    return Layout.InspectorWidth(RB.inspectorWidth or RB.CharSetting("inspectorWidth"), frame:GetWidth(), RB.RailWidth())
end

-- Sets the splitter (saved unless dragging, which saves on release).
function RB.SetInspectorWidth(width, unsaved)
    RB.inspectorWidth = Layout.InspectorWidth(width, frame:GetWidth(), RB.RailWidth())
    if not unsaved then
        RB.SetCharSetting("inspectorWidth", RB.inspectorWidth)
    end
    RB.LayOutColumns()
end

function RB.StepInspector(delta)
    RB.SetInspectorWidth(Layout.StepInspector(RB.InspectorWidth(), delta, frame:GetWidth(), RB.RailWidth()))
end

-- Other files follow the columns (rail labels, grid rows and columns, the inspector), in load order.
local columnListeners = {}
function RB.OnColumnsResized(listener)
    columnListeners[#columnListeners + 1] = listener
end

-- Column widths for the window's width.
function RB.LayOutColumns()
    RB.railCol:SetWidth(RB.RailWidth())
    RB.inspectorCol:SetWidth(RB.InspectorWidth())
    for _, listener in ipairs(columnListeners) do
        listener()
    end
end

-- Size and place: the saved size (default about 1280x800) within the screen less its margins; offsets of TOPRIGHT
-- from UIParent's TOPRIGHT, in UIParent units.
local function ScreenSize()
    return UIParent:GetWidth(), UIParent:GetHeight()
end

local function WindowSize()
    local screenWidth, screenHeight = ScreenSize()
    local maxWidth, maxHeight = screenWidth - 2 * SCREEN_MARGIN, screenHeight - 2 * SCREEN_MARGIN
    local width = math.min(RB.CharSetting("width", DEFAULT_WIDTH), maxWidth)
    local height = math.min(RB.CharSetting("height", DEFAULT_HEIGHT), maxHeight)
    return math.max(width, math.min(MIN_WIDTH, maxWidth)), math.max(height, math.min(MIN_HEIGHT, maxHeight))
end

function RB.SaveWindowPlace()
    RB.SetCharSetting("x", frame:GetRight() - UIParent:GetRight())
    RB.SetCharSetting("y", frame:GetTop() - UIParent:GetTop())
end

local function DefaultY(height)
    local _, screenHeight = ScreenSize()
    return -math.min(DEFAULT_TOP, math.max(SCREEN_MARGIN, screenHeight - height - SCREEN_MARGIN))
end

local function PlaceWindow()
    frame:SetScale(1)
    local width, height = WindowSize()
    frame:SetSize(width, height)
    frame:SetMaxResize(ScreenSize())
    frame:ClearAllPoints()
    frame:SetPoint("TOPRIGHT", UIParent, "TOPRIGHT", RB.CharSetting("x", DEFAULT_X), RB.CharSetting("y", DefaultY(height)))
end

function RB.ResetWindowPlace()
    for _, key in ipairs({ "x", "y", "width", "height" }) do
        RB.SetCharSetting(key, nil)
    end
    PlaceWindow()
end

-- The corner grip, above the inspector.
local grip = CreateFrame("Button", nil, frame)
grip:SetSize(16, 16)
grip:SetPoint("BOTTOMRIGHT", -4, 4)
grip:SetFrameLevel(frame:GetFrameLevel() + 60)
grip:SetNormalTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Up")
grip:SetHighlightTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Highlight")
grip:SetPushedTexture("Interface\\ChatFrame\\UI-ChatIM-SizeGrabber-Down")
RB.SetVertex(grip:GetNormalTexture(), T.label)
RB.SetVertex(grip:GetHighlightTexture(), T.accent)
RB.SetVertex(grip:GetPushedTexture(), T.accent)
grip:SetScript("OnMouseDown", function() frame:StartSizing("BOTTOMRIGHT") end)
RB.AttachTip(grip, "grip")
grip:SetScript("OnMouseUp", function()
    frame:StopMovingOrSizing()
    RB.SetCharSetting("width", math.floor(frame:GetWidth() + 0.5))
    RB.SetCharSetting("height", math.floor(frame:GetHeight() + 0.5))
    RB.SaveWindowPlace()
end)

frame:SetScript("OnSizeChanged", function() RB.LayOutColumns() end)

-- Query row (under the search box, Filters.lua): a chip per active filter (click clears it), Clear filters, the
-- pager and Export CSV.
local queryRow = CreateFrame("Frame", nil, RB.mainCol)
queryRow:SetPoint("TOPLEFT", 0, -(L.searchHeight + 4))
queryRow:SetPoint("TOPRIGHT", 0, -(L.searchHeight + 4))
queryRow:SetHeight(L.queryHeight)

local export = RB.CreateButton(queryRow, "Export CSV " .. RB.Key("E"), 88, 18, function() RB.ExportCsv() end)
export:SetPoint("RIGHT", 0, 0)
RB.AttachTip(export, "exportCsv")
local nextPage = RB.CreateButton(queryRow, "Next " .. RB.Key("]"), 52, 18, function() RB.TurnPage(1) end)
nextPage:SetPoint("RIGHT", export, "LEFT", -10, 0)
RB.AttachTip(nextPage, "nextPage")
local pageText = RB.CreateText(queryRow, "RetailBrowserValueFont", "RIGHT")
pageText:SetPoint("RIGHT", nextPage, "LEFT", -6, 0)
local prevPage = RB.CreateButton(queryRow, RB.Key("[") .. " Prev", 52, 18, function() RB.TurnPage(-1) end)
prevPage:SetPoint("RIGHT", pageText, "LEFT", -6, 0)
RB.AttachTip(prevPage, "prevPage")

local chips = {}
for i = 1, MAX_CHIPS do
    local chip = RB.CreateButton(queryRow, "", 60, 18, function(self) RB.ClearColumnFilter(self.column) end)
    chip:SetPoint("LEFT", i == 1 and queryRow or chips[i - 1], i == 1 and "LEFT" or "RIGHT", i == 1 and 0 or CHIP_GAP, 0)
    chip:Hide()
    RB.AttachTip(chip, "chip")
    chips[i] = chip
end
local clearFilters = RB.CreateButton(queryRow, "Clear filters", 84, 18, function() RB.ClearAllFilters() end)
RB.AttachTip(clearFilters, "clearFilters")
local noFilters = RB.CreateText(queryRow, "RetailBrowserLabelFont")
noFilters:SetPoint("LEFT", 2, 0)
noFilters:SetPoint("RIGHT", prevPage, "LEFT", -8, 0)
noFilters:SetText("Filters sit on top of their column; they search the whole index.")

function RB.SetPageText(text)
    pageText:SetText(text or "")
end

-- The query row's width left of the pager, from the window's width (anchored sizes lag a frame behind a resize).
local function ChipRoom()
    local main = frame:GetWidth() - Layout.ColumnsChrome(RB.RailWidth()) - Layout.GAP - RB.InspectorWidth()
    local pager = export:GetWidth() + 10 + nextPage:GetWidth() + 6 + pageText:GetStringWidth() + 6 + prevPage:GetWidth()
    return main - pager - 8
end

-- chipList: { { column = column, text = "Slot = Chest" }, ... }. Each chip clears its filter; Clear filters shows when
-- there are several, and counts them all when some chips do not fit (their filters stay).
local chipsNow = {}
function RB.ShowChips(chipList)
    chipsNow = chipList
    local widths = {}
    for i, chip in ipairs(chips) do
        local entry = chipList[i]
        if entry then
            chip.column = entry.column
            chip:SetLabel(entry.text .. "  " .. RB.Colored("x", T.label))
            chip:SetWidth(chip.label:GetStringWidth() + 20)
            chip:SetOn(true)
            widths[i] = chip:GetWidth()
        end
    end
    local room = ChipRoom()
    local shown = Layout.FitChips(widths, room, CHIP_GAP)
    local clear = #chipList > 1 or shown < #chipList
    if clear then
        shown = Layout.FitChips(widths, room - clearFilters:GetWidth() - 8, CHIP_GAP)
    end
    for i, chip in ipairs(chips) do
        RB.Shown(chip, i <= shown)
    end
    local count = #chipList
    clearFilters:SetLabel(Layout.ClearFiltersLabel(count, shown))
    RB.Shown(noFilters, count == 0)
    clearFilters:ClearAllPoints()
    local last = chips[shown]
    clearFilters:SetPoint("LEFT", last or queryRow, last and "RIGHT" or "LEFT", last and 8 or 0, 0)
    RB.Shown(clearFilters, clear)
end

RB.OnColumnsResized(function() RB.ShowChips(chipsNow) end)

-- Status line (bottom of the main column): the key owner, then the key hints.
local statusLine = CreateFrame("Frame", nil, RB.mainCol)
statusLine:SetPoint("BOTTOMLEFT", 0, 0)
statusLine:SetPoint("BOTTOMRIGHT", 0, 0)
statusLine:SetHeight(L.statusHeight)
local keysButton = RB.CreateButton(statusLine, "", 110, 18, function() RB.ToggleGridKeys() end)
keysButton:SetPoint("LEFT", 0, 0)
RB.AttachTip(keysButton, "keys")
local clearHistory = RB.CreateButton(statusLine, "Clear history " .. RB.Key("Ctrl+Del"), 130, 18, function() RB.ClearHistoryNow() end)
clearHistory:SetPoint("RIGHT", 0, 0)
RB.AttachTip(clearHistory, "clearHistory")
local hints = RB.CreateText(statusLine, "RetailBrowserLabelFont")
hints:SetPoint("LEFT", keysButton, "RIGHT", 8, 0)
hints:SetPoint("RIGHT", clearHistory, "LEFT", -8, 0)

-- The key owner badge: teal while the browser has the keys, muted when the game has them.
local OWNER_TEXT = {
    grid = "Keys: grid", search = "Keys: search", filter = "Keys: filter", build = "Keys: Build", game = "Keys: game",
}
local OWNER_HINTS = {
    grid = "Up/Dn move   Left/Right display   Space check   / search   Tab filters   M morph   T target   I add   Y try on"
        .. "   G GM   F fav   B SQL   D overview   P displays   U used by   S stats   R raw   Ctrl+Left/Right inspector"
        .. "   Esc clear/close",
    search = "Typing searches the whole index.   Enter or Esc: back to the grid.",
    filter = "Range: 200-  -300  100-200  >=5  =5.   Tab next filter, Enter or Esc: back to the grid.",
    build = "Changes rebuild after 0.3 s.   Esc: back to the grid.",
    game = "Click the grid or this button to give it the keys.",
}

function RB.ShowKeyOwner(owner)
    keysButton:SetLabel(OWNER_TEXT[owner] or OWNER_TEXT.game)
    keysButton:SetOn(owner ~= "game" and OWNER_TEXT[owner] ~= nil)
    hints:SetText(OWNER_HINTS[owner] or OWNER_HINTS.game)
end

frame:SetScript("OnShow", function()
    PlaceWindow()
    RB.LayOutColumns()
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
