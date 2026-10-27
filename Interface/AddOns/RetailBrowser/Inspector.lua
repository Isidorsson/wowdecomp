-- The inspector, the right column: the shown entry's name (coloured by quality or rank), its facts and the favourite
-- star, the tab strip on one row (Overview, Stats, Displays, Used by, Build SQL, Raw: names with a count bubble, short
-- names when narrow; the keys are in their tooltips), the red band when an installed display row wins, then the open
-- tab under the 3D preview in a scrolling area, and the actions pinned at the bottom. The splitter
-- on its left edge (or Ctrl+Left / Ctrl+Right) sets its width, 560 px by default, at least 420, saved per character.

local RB = RetailBrowser
local T = RB.THEME
local Layout = RB.Layout

local INSET = RB.EDGE_INSET + 6
local HEADER_HEIGHT = 50
local TAB_HEIGHT = 22
local TAB_GAP = 2
local TAB_LINE = 2
local BUBBLE_HEIGHT = 13
local BUBBLE_PADDING = 8
local BUBBLE_GAP = 4
local STAR_SIZE = 18
local STAR_OFF_ALPHA = 0.45
local WARN_PADDING = 4
local GAP = 6
local PREVIEW_HEIGHT = 300
local PANEL_MIN_HEIGHT = 160
local SCROLLBAR_SPACE = 22
local SPLITTER_LINE = 2
local SPLITTER_LINE_INSET = 40

local inspector = RB.CreatePanel(RB.inspectorCol, "panel")
inspector:SetPoint("TOPRIGHT", RB.inspectorCol, "TOPRIGHT")
inspector:SetPoint("BOTTOMRIGHT", RB.inspectorCol, "BOTTOMRIGHT")
inspector:SetWidth(Layout.INSPECTOR_DEFAULT)
inspector:EnableMouse(true)
RB.inspector = inspector

local heading = RB.CreateText(inspector, "GameFontNormalLarge")
-- The favourite star: the raid-target star (FRIZQT has no star glyph), gold when a favourite, else grey.
local star = CreateFrame("Button", nil, inspector)
star:SetSize(STAR_SIZE, STAR_SIZE)
star:SetPoint("TOPRIGHT", -INSET, -INSET)
star.icon = star:CreateTexture(nil, "ARTWORK")
star.icon:SetAllPoints()
star.icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcon_1")
star:SetScript("OnClick", function() RB.RunAction("favourite") end)
RB.AttachTip(star, "favouriteStar")
star:Hide()

function RB.RefreshStar()
    local d = RB.details
    RB.Shown(star, d)
    local on = d and RB.IsFavourite(d.kind, d.id)
    star.icon:SetDesaturated(not on)
    RB.SetVertex(star.icon, on and { 1, 1, 1 } or T.label)
    -- A desaturated texture ignores its vertex colour on this client, so the grey star is the white one, faded.
    star:SetAlpha(on and 1 or STAR_OFF_ALPHA)
end

heading:SetPoint("TOPLEFT", INSET, -INSET - 2)
heading:SetPoint("RIGHT", star, "LEFT", -6, 0)
local subline = RB.CreateText(inspector, "RetailBrowserLabelFont")
subline:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 0, -4)
subline:SetPoint("RIGHT", -INSET, 0)

local strip = CreateFrame("Frame", nil, inspector)
strip:SetPoint("TOPLEFT", INSET, -HEADER_HEIGHT)
strip:SetPoint("TOPRIGHT", -INSET, -HEADER_HEIGHT)
local stripLine = RB.CreateFill(strip, "BORDER", T.borderDim)
stripLine:ClearAllPoints()
stripLine:SetPoint("BOTTOMLEFT")
stripLine:SetPoint("BOTTOMRIGHT")
stripLine:SetHeight(1)

local warning = CreateFrame("Frame", nil, inspector)
warning:SetPoint("TOPLEFT", strip, "BOTTOMLEFT", 0, -2)
warning:SetPoint("TOPRIGHT", strip, "BOTTOMRIGHT", 0, -2)
RB.CreateFill(warning, "BACKGROUND", T.warning)
-- Wraps to the inspector's width; the band grows to fit it (LayOutInspector).
local warningText = RB.CreateText(warning, "RetailBrowserSmallFont")
warningText:SetPoint("TOPLEFT", 6, -WARN_PADDING)
RB.SetColor(warningText, T.warningText)
warning:Hide()

-- The actions (Actions.lua fills the slot and says how tall its rows are), above a hairline.
RB.inspectorActionsSlot = CreateFrame("Frame", nil, inspector)
RB.inspectorActionsSlot:SetPoint("BOTTOMLEFT", INSET, INSET)
RB.inspectorActionsSlot:SetPoint("BOTTOMRIGHT", -INSET, INSET)
local actionsLine = RB.CreateFill(inspector, "BORDER", T.borderDim)
actionsLine:ClearAllPoints()
actionsLine:SetPoint("BOTTOMLEFT", RB.inspectorActionsSlot, "TOPLEFT", 0, GAP / 2)
actionsLine:SetPoint("BOTTOMRIGHT", RB.inspectorActionsSlot, "TOPRIGHT", 0, GAP / 2)
actionsLine:SetHeight(1)

-- The content: the preview (Preview.lua fills it) on top, the open tab's panel scrolling under it. The preview stays
-- out of the scroll frame: a model inside one draws at the wrong place on this client.
RB.inspectorPreviewSlot = CreateFrame("Frame", nil, inspector)
RB.inspectorPreviewSlot:SetHeight(PREVIEW_HEIGHT)
local scroll = CreateFrame("ScrollFrame", "RetailBrowserInspectorScroll", inspector, "UIPanelScrollFrameTemplate")
local scrollBar = RetailBrowserInspectorScrollScrollBar
local content = CreateFrame("Frame", nil, scroll)
content:SetSize(1, 1)
scroll:SetScrollChild(content)
scroll:HookScript("OnScrollRangeChanged", function(_, _, yRange)
    RB.Shown(scrollBar, (yRange or 0) > 0.5)
end)

local tabs, tabOrder = {}, {}
RB.inspectorTab = "overview"

-- Active: accent text and the underline; under the mouse: body text; else the label colour.
local function PaintTab(button)
    RB.Shown(button.line, button.on)
    RB.SetColor(button.text, button.on and T.accent or (button.hovered and T.text or T.label))
end

local function CreateTabButton(key)
    local button = CreateFrame("Button", nil, strip)
    button:SetHeight(TAB_HEIGHT)
    button.text = RB.CreateText(button, "GameFontHighlightSmall")
    button.bubble = CreateFrame("Frame", nil, button)
    button.bubble:SetHeight(BUBBLE_HEIGHT)
    button.bubble:SetBackdrop(RB.FLAT_BACKDROP)
    RB.SetBack(button.bubble, T.well)
    RB.SetBorder(button.bubble, T.borderDim)
    button.bubble.text = RB.CreateText(button.bubble, "RetailBrowserLabelFont", "CENTER")
    button.bubble.text:SetPoint("CENTER", 0, 0)
    button.line = RB.CreateFill(button, "ARTWORK", T.accent)
    button.line:ClearAllPoints()
    button.line:SetPoint("BOTTOMLEFT")
    button.line:SetPoint("BOTTOMRIGHT")
    button.line:SetHeight(TAB_LINE)
    button:SetScript("OnEnter", function(self) self.hovered = true; PaintTab(self) end)
    button:SetScript("OnLeave", function(self) self.hovered = false; PaintTab(self) end)
    button:SetScript("OnClick", function() RB.ShowInspectorTab(key) end)
    RB.AttachTip(button, "tab_" .. key)
    return button
end

-- Other files add their tab: key, its name (a string or function(kind, d)), its frame and refresh.
-- opts: kinds (where it shows), order (place in the strip), short (the
-- name when the strip is narrow, a string or function), count (function(kind, d): the bubble's number), minHeight
-- (a number or a function; the panel scrolls rather than shrink below it). Keys are in the tab's tooltip.
function RB.AddInspectorTab(key, label, panel, refresh, opts)
    opts = opts or {}
    tabs[key] = { button = CreateTabButton(key), label = label, short = opts.short, count = opts.count, panel = panel,
        refresh = refresh, kinds = opts.kinds, order = opts.order or 99, minHeight = opts.minHeight or PANEL_MIN_HEIGHT }
    tabOrder[#tabOrder + 1] = key
    table.sort(tabOrder, function(a, b) return tabs[a].order < tabs[b].order end)
    panel:SetParent(content)
    panel:Hide()
end

local function TabShown(tab, kind)
    return not tab.kinds or (kind and tab.kinds[kind])
end

local function Resolve(value, kind)
    if type(value) == "function" then
        return value(kind, RB.details)
    end
    return value
end

-- A tab's names and count bubble, measured: { tab, name, short, full, short width, extra } for Layout.FitTabs.
local function MeasureTab(tab, kind)
    local button = tab.button
    local name, short = Resolve(tab.label, kind), Resolve(tab.short, kind)
    local count = tab.count and tab.count(kind, RB.details) or 0
    button.text:SetText(name)
    local item = { tab = tab, name = name, short = short, full = button.text:GetStringWidth(), extra = 0 }
    if short then
        button.text:SetText(short)
        item.shortWidth = button.text:GetStringWidth()
    end
    button.bubble.text:SetText(count)
    RB.Shown(button.bubble, count > 0)
    if count > 0 then
        button.bubble:SetWidth(button.bubble.text:GetStringWidth() + BUBBLE_PADDING)
        item.extra = button.bubble:GetWidth() + BUBBLE_GAP
    end
    return item
end

-- Places a measured tab at x with the fit's name and padding; returns its width.
local function PlaceTab(item, key, x, fit)
    local button = item.tab.button
    local useShort = fit.short and item.short
    button.text:SetText(useShort and item.short or item.name)
    local textWidth = useShort and item.shortWidth or item.full
    button.text:ClearAllPoints()
    button.text:SetPoint("LEFT", fit.padding / 2, 1)
    button.bubble:ClearAllPoints()
    button.bubble:SetPoint("LEFT", button.text, "RIGHT", BUBBLE_GAP, 0)
    button:SetWidth(textWidth + item.extra + fit.padding)
    button:ClearAllPoints()
    button:SetPoint("TOPLEFT", strip, "TOPLEFT", x, 0)
    button.on = key == RB.inspectorTab
    PaintTab(button)
    return button:GetWidth()
end

-- The shown kind's tabs on one row: full names, else short ones, else tighter padding (Layout.FitTabs). Returns
-- the strip's height.
local function LayOutTabs(width)
    local kind = RB.details and RB.details.kind
    local items, keys = {}, {}
    for _, key in ipairs(tabOrder) do
        local tab = tabs[key]
        RB.Shown(tab.button, TabShown(tab, kind))
        if tab.button:IsShown() then
            local item = MeasureTab(tab, kind)
            items[#items + 1], keys[#items + 1] = item, key
        end
    end
    local measured = {}
    for i, item in ipairs(items) do
        measured[i] = { full = item.full, short = item.shortWidth, extra = item.extra }
    end
    local fit = Layout.FitTabs(measured, width, TAB_GAP)
    local x = 0
    for i, item in ipairs(items) do
        x = x + PlaceTab(item, keys[i], x, fit) + TAB_GAP
    end
    strip:SetHeight(TAB_HEIGHT + 1)
    return TAB_HEIGHT + 1
end

-- The content area between the strip (and warning) and the actions: the preview, then the
-- scrolling panel, which fills what is left or scrolls past its minimum height.
local function LayOutContent(tab, top, bottom, width)
    local preview = RB.inspectorPreviewSlot
    local previewSpace = PREVIEW_HEIGHT + GAP
    preview:ClearAllPoints()
    preview:SetPoint("TOPLEFT", INSET, -top)
    preview:SetPoint("TOPRIGHT", -INSET, -top)
    scroll:ClearAllPoints()
    scroll:SetPoint("TOPLEFT", INSET, -top - previewSpace)
    scroll:SetPoint("BOTTOMRIGHT", -INSET - SCROLLBAR_SPACE, bottom)
    local viewWidth = width - 2 * INSET - SCROLLBAR_SPACE
    local viewHeight = RB.ColumnHeight() - top - previewSpace - bottom
    local minHeight = tab and tab.minHeight or PANEL_MIN_HEIGHT
    if type(minHeight) == "function" then
        minHeight = minHeight()
    end
    local panelHeight = math.max(viewHeight, minHeight)
    content:SetSize(viewWidth, panelHeight)
    for _, other in pairs(tabs) do
        other.panel:ClearAllPoints()
        other.panel:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0)
        other.panel:SetSize(viewWidth, panelHeight)
    end
end

local function LayOutInspector()
    local tab = tabs[RB.inspectorTab]
    local width = RB.InspectorWidth()
    inspector:SetWidth(width)
    local top = HEADER_HEIGHT + LayOutTabs(width - 2 * INSET) + GAP
    if warning:IsShown() then
        warningText:SetWidth(width - 2 * INSET - 12)
        warning:SetHeight(warningText:GetStringHeight() + 2 * WARN_PADDING)
        top = top + warning:GetHeight() + 2
    end
    local actionsHeight = RB.LayOutActions and RB.LayOutActions(width - 2 * INSET) or 0
    RB.inspectorActionsSlot:SetHeight(math.max(1, actionsHeight))
    LayOutContent(tab, top, INSET + actionsHeight + GAP, width)
end

function RB.ShowInspectorTab(key)
    local tab = tabs[key]
    if not tab or not TabShown(tab, RB.details and RB.details.kind) then
        return
    end
    RB.inspectorTab = key
    for otherKey, other in pairs(tabs) do
        RB.Shown(other.panel, otherKey == key)
    end
    scrollBar:SetValue(0)
    LayOutInspector()
    if tab.refresh then
        tab.refresh(RB.details)
    end
end

-- The red band when the picked display's installed DBC row wins over the retail one.
local function RefreshWarning(d)
    local text = RB.WarningText(d)
    warningText:SetText(text or "")
    RB.Shown(warning, text)
end

-- Everything that shows the picked display: the warning, the open tab, the actions, the preview and Build.
function RB.RefreshInspector()
    local d = RB.details
    RefreshWarning(d)
    LayOutInspector()
    local tab = tabs[RB.inspectorTab]
    if tab and tab.refresh then
        tab.refresh(d)
    end
    if RB.UpdateActions then
        RB.UpdateActions()
    end
end

function RB.PickDisplay(index)
    local d = RB.details
    if d and RB.Pick(d, index) then
        RB.RefreshInspector()
        RB.SchedulePreview(true)
        RB.ScheduleBuild()
    end
end

function RB.StepPick(step)
    local d = RB.details
    local _, index = RB.Picked(d)
    local count = #RB.PickList(d)
    if count > 0 then
        RB.PickDisplay((index - 1 + step) % count + 1)
    end
end

local function HeadingColor(d)
    if d.kind == "item" then
        return RB.QualityColor(d.quality)
    elseif d.kind == "creature" then
        return RB.RankColor(RB.RankGroup(d.classification))
    end
    return T.text[1], T.text[2], T.text[3]
end

subline:SetText("Pick a row in the grid.")

-- Shows an entry: its tab stays when the kind has it, else Overview.
function RB.ShowDetails(kind, id, now)
    local d, err = RB.DetailsOf(kind, id)
    if not d then
        RB.SetStatus(RB.Colored(tostring(err), T.bad))
        return
    end
    -- Another kind lays out other sections: start them from the top.
    if not RB.details or RB.details.kind ~= d.kind then
        scrollBar:SetValue(0)
    end
    RB.details = d
    if not (RB.pick and RB.pick.owner == d) then
        RB.pick = nil
    end
    heading:SetText(d.name or "")
    heading:SetTextColor(HeadingColor(d))
    subline:SetText(RB.HeaderFacts(d))
    local tab = tabs[RB.inspectorTab]
    if not tab or not TabShown(tab, kind) then
        RB.inspectorTab = "overview"
    end
    for key, other in pairs(tabs) do
        RB.Shown(other.panel, key == RB.inspectorTab)
    end
    RB.RefreshStar()
    RB.RefreshInspector()
    RB.Prefetch(kind, id)
    RB.SchedulePreview(now)
    RB.ScheduleBuild()
end

RB.LayOutInspector = LayOutInspector
RB.OnColumnsResized(LayOutInspector)

-- The splitter: a grip in the gap left of the inspector; dragging it left widens the inspector.
local splitter = CreateFrame("Frame", nil, RB.inspectorCol)
splitter:SetFrameLevel(inspector:GetFrameLevel() + 5)
splitter:SetPoint("TOPRIGHT", inspector, "TOPLEFT", 0, 0)
splitter:SetPoint("BOTTOMRIGHT", inspector, "BOTTOMLEFT", 0, 0)
splitter:SetWidth(Layout.GAP)
splitter:EnableMouse(true)
RB.splitter = splitter

local line = splitter:CreateTexture(nil, "ARTWORK")
line:SetTexture("Interface\\ChatFrame\\ChatFrameBackground")
line:SetPoint("TOP", 0, -SPLITTER_LINE_INSET)
line:SetPoint("BOTTOM", 0, SPLITTER_LINE_INSET)
line:SetWidth(SPLITTER_LINE)
RB.SetVertex(line, T.borderDim)

local function CursorX()
    local x = GetCursorPosition()
    return x / splitter:GetEffectiveScale()
end

splitter:SetScript("OnEnter", function()
    RB.SetVertex(line, T.accent)
    SetCursor("UI-Cursor-Size")
end)
splitter:SetScript("OnLeave", function(self)
    if not self.dragging then
        RB.SetVertex(line, T.borderDim)
    end
    ResetCursor()
end)
RB.AttachTip(splitter, "splitter")

splitter:SetScript("OnMouseDown", function(self)
    self.dragging = { x = CursorX(), width = RB.InspectorWidth() }
end)
splitter:SetScript("OnMouseUp", function(self)
    self.dragging = nil
    RB.SetInspectorWidth(RB.InspectorWidth())
    if not MouseIsOver(self) then
        RB.SetVertex(line, T.borderDim)
    end
end)
splitter:SetScript("OnUpdate", function(self)
    local drag = self.dragging
    if drag then
        RB.SetInspectorWidth(drag.width + (drag.x - CursorX()), true)
    end
end)
