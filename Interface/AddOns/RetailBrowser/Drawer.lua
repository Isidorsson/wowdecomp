-- The bottom drawer (in place of a side pane): tabs Details, Stats, Displays (Appearances, Pieces, Users), Used by,
-- Build SQL and Raw on the left, the 3D preview (at least 360 px tall when the window has the room) and the actions on
-- the right. The splitter above it (at the end of this file, or Ctrl+Up / Ctrl+Down) sets its height between 200 px and what
-- leaves 3 grid rows; Build SQL takes at least 330 px while it is shown, so it always stays in the window.

local RB = RetailBrowser
local L = RB.LAYOUT
local T = RB.THEME
local frame = RB.frame

local TAB_HEIGHT = 32
local WARN_HEIGHT = 16
local PREVIEW_MIN_HEIGHT = 360
local PREVIEW_TOOLBAR = 30
local MIN_GRID_ROWS = 3
local ACTIONS_WIDTH = 270
local PREVIEW_MIN_WIDTH, PREVIEW_MAX_WIDTH = 230, 420
local BULK_SPACE = L.bulkHeight + 4
local STATUS_SPACE = L.statusHeight + 10
local SPLITTER_SPACE = 6
local DRAWER_MIN = 200
local BUILD_MIN_HEIGHT = 330
local KEY_STEP_ROWS = 4

local drawer = RB.CreatePanel(frame, "panel")
drawer:SetPoint("TOPLEFT", RB.bulk, "BOTTOMLEFT", 0, -2 - SPLITTER_SPACE)
drawer:SetPoint("TOPRIGHT", RB.bulk, "BOTTOMRIGHT", 0, -2 - SPLITTER_SPACE)
drawer:SetPoint("BOTTOM", frame, "BOTTOM", 0, STATUS_SPACE)
RB.drawer = drawer

local strip = RB.CreateFill(drawer, "BORDER", T.band)
strip:ClearAllPoints()
strip:SetPoint("TOPLEFT", RB.EDGE_INSET, -RB.EDGE_INSET)
strip:SetPoint("TOPRIGHT", -RB.EDGE_INSET, -RB.EDGE_INSET)
strip:SetHeight(TAB_HEIGHT - 4)

local heading = RB.CreateText(drawer, "GameFontNormal", "RIGHT")
heading:SetPoint("TOPRIGHT", -160, -12)
heading:SetWidth(360)
local sizeHint = RB.CreateText(drawer, "RetailBrowserLabelFont", "RIGHT")
sizeHint:SetPoint("TOPRIGHT", -10, -13)
sizeHint:SetText(RB.Colored("Ctrl+Up/Dn", T.value) .. " drawer size")

local warning = CreateFrame("Frame", nil, drawer)
warning:SetPoint("TOPLEFT", RB.EDGE_INSET, -TAB_HEIGHT)
warning:SetPoint("TOPRIGHT", -RB.EDGE_INSET, -TAB_HEIGHT)
warning:SetHeight(WARN_HEIGHT)
RB.CreateFill(warning, "BACKGROUND", T.warning)
local warningText = RB.CreateText(warning, "RetailBrowserSmallFont")
warningText:SetPoint("LEFT", 6, 0)
warningText:SetPoint("RIGHT", -6, 0)
warning:Hide()

-- Tab content on the left; the preview and the actions on the right (Preview.lua, Actions.lua fill them).
local content = CreateFrame("Frame", nil, drawer)
local side = CreateFrame("Frame", nil, drawer)
RB.drawerPreviewSlot = CreateFrame("Frame", nil, side)
RB.drawerPreviewSlot:SetPoint("TOPLEFT", 0, 0)
RB.drawerPreviewSlot:SetPoint("BOTTOMLEFT", 0, 0)
RB.drawerActionsSlot = CreateFrame("Frame", nil, side)
RB.drawerActionsSlot:SetPoint("TOPRIGHT", 0, 0)
RB.drawerActionsSlot:SetPoint("BOTTOMRIGHT", 0, 0)
RB.drawerActionsSlot:SetWidth(ACTIONS_WIDTH)

local tabs, tabOrder = {}, {}
RB.drawerTab = "details"

-- The preview's width: a share of the drawer's, within its bounds.
local function PreviewWidth()
    local width = (drawer:GetWidth() - ACTIONS_WIDTH) * 0.32
    return math.max(PREVIEW_MIN_WIDTH, math.min(PREVIEW_MAX_WIDTH, width))
end

local function LayOutContent()
    local top = -(TAB_HEIGHT + 2 + (warning:IsShown() and WARN_HEIGHT or 0))
    local tab = tabs[RB.drawerTab]
    local inset = RB.EDGE_INSET + 2
    content:ClearAllPoints()
    content:SetPoint("TOPLEFT", inset, top)
    content:SetPoint("BOTTOMLEFT", inset, inset)
    side:ClearAllPoints()
    side:SetPoint("TOPRIGHT", -inset, top)
    side:SetPoint("BOTTOMRIGHT", -inset, inset)
    local previewWidth = PreviewWidth()
    side:SetWidth(previewWidth + ACTIONS_WIDTH + 6)
    RB.drawerPreviewSlot:SetWidth(previewWidth)
    if tab and tab.wide then
        content:SetPoint("RIGHT", drawer, "RIGHT", -inset, 0)
        side:Hide()
    else
        content:SetPoint("RIGHT", side, "LEFT", -6, 0)
        side:Show()
    end
end

-- Other files add their tab: key, button label per kind (a string or function(kind)), its frame and refresh.
-- opts: kinds (where it shows), wide (no preview and actions), hotkey, order (place in the strip).
function RB.AddDrawerTab(key, label, panel, refresh, opts)
    opts = opts or {}
    local button = RB.CreateTab(drawer, "RetailBrowserDrawerTab" .. (#tabOrder + 1), "", function() RB.ShowDrawerTab(key) end)
    RB.AttachTip(button, "tab_" .. key)
    tabs[key] = { button = button, label = label, panel = panel, refresh = refresh, kinds = opts.kinds, wide = opts.wide, hotkey = opts.hotkey,
        order = opts.order or 99 }
    tabOrder[#tabOrder + 1] = key
    table.sort(tabOrder, function(a, b) return tabs[a].order < tabs[b].order end)
    panel:SetParent(content)
    panel:SetAllPoints(content)
    panel:Hide()
end

local function TabShown(tab, kind)
    return not tab.kinds or (kind and tab.kinds[kind])
end

local function RefreshTabButtons()
    local kind = RB.details and RB.details.kind
    local previous
    for _, key in ipairs(tabOrder) do
        local tab = tabs[key]
        local button = tab.button
        if TabShown(tab, kind) then
            local text = type(tab.label) == "function" and tab.label(kind, RB.details) or tab.label
            button:SetLabel(text .. (tab.hotkey and (" " .. RB.Key(tab.hotkey)) or ""))
            button:ClearAllPoints()
            if previous then
                button:SetPoint("LEFT", previous, "RIGHT", -4, 0)
            else
                button:SetPoint("BOTTOMLEFT", drawer, "TOPLEFT", 6, -TAB_HEIGHT + 2)
            end
            button:SetOn(key == RB.drawerTab)
            button:Show()
            previous = button
        else
            button:Hide()
        end
    end
end

-- The splitter's value is the drawer's wanted height: between DRAWER_MIN and what leaves 3 grid rows, by default
-- the room for the 360 px preview. The grid takes whole rows of the rest; the drawer gets what they leave.
local function DrawerBounds()
    local avail = frame:GetHeight() - L.gridTop - BULK_SPACE - SPLITTER_SPACE - STATUS_SPACE - RB.GRID_CHROME - 2
    local most = math.max(DRAWER_MIN, avail - MIN_GRID_ROWS * L.rowHeight)
    local default = math.min(TAB_HEIGHT + PREVIEW_MIN_HEIGHT + PREVIEW_TOOLBAR + 12, most)
    return avail, DRAWER_MIN, most, default
end

function RB.DrawerHeight()
    local _, least, most, default = DrawerBounds()
    return math.max(least, math.min(most, RB.drawerHeight or RB.CharSetting("drawerHeight", default)))
end

-- Sets the splitter (saved unless dragging, which saves on release).
function RB.SetDrawerHeight(height, unsaved)
    local _, least, most = DrawerBounds()
    RB.drawerHeight = math.max(least, math.min(most, height))
    if not unsaved then
        RB.SetCharSetting("drawerHeight", RB.drawerHeight)
    end
    RB.ApplyDrawerStep()
end

function RB.ApplyDrawerStep()
    local avail, _, most = DrawerBounds()
    local height = RB.DrawerHeight()
    local tab = tabs[RB.drawerTab]
    if tab and tab.wide then
        height = math.max(height, math.min(BUILD_MIN_HEIGHT, most))
    end
    RB.SetGridRows(math.floor((avail - height) / L.rowHeight))
end

-- Ctrl+Up / Ctrl+Down move the same splitter, a few rows a step.
function RB.StepDrawer(delta)
    RB.SetDrawerHeight(RB.DrawerHeight() + delta * KEY_STEP_ROWS * L.rowHeight)
end

-- The window was resized: rows, columns, drawer split and the preview's width follow.
function RB.OnWindowResized()
    RB.ApplyDrawerStep()
    RB.RelayoutColumns()
    LayOutContent()
end

function RB.ShowDrawerTab(key)
    local tab = tabs[key]
    if not tab or not TabShown(tab, RB.details and RB.details.kind) then
        return
    end
    RB.drawerTab = key
    for otherKey, other in pairs(tabs) do
        RB.Shown(other.panel, otherKey == key)
    end
    RefreshTabButtons()
    LayOutContent()
    RB.ApplyDrawerStep()
    if tab.refresh then
        tab.refresh(RB.details)
    end
end

-- The red band when the picked display's installed DBC row wins over the retail one.
local function RefreshWarning(d)
    local badge = d and RB.PickedBadge(d)
    local picked = d and RB.Picked(d)
    if badge == "custom" or badge == "3.3.5" then
        local dbc = (d.kind == "item" or d.kind == "set") and "ItemDisplayInfo.dbc" or "CreatureDisplayInfo.dbc"
        warningText:SetText(string.format("%s display %d exists in your %s (%s), so the client draws that row, not retail's.",
            RB.Colored("custom row wins:", T.title), picked.displayId or 0, dbc, badge == "custom" and "a row your server added" or "the stock 3.3.5 row"))
        warning:Show()
    else
        warning:Hide()
    end
end

-- Everything that shows the picked display: the warning, the open tab, the actions, the preview and Build.
function RB.RefreshDrawer()
    local d = RB.details
    RefreshWarning(d)
    RefreshTabButtons()
    LayOutContent()
    local tab = tabs[RB.drawerTab]
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
        RB.RefreshDrawer()
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
    return T.title[1], T.title[2], T.title[3]
end

-- Shows an entry: its tab stays when the kind has it, else Details.
function RB.ShowDetails(kind, id, now)
    local d, err = RB.DetailsOf(kind, id)
    if not d then
        RB.SetStatus(RB.Colored(tostring(err), T.bad))
        return
    end
    RB.details = d
    if not (RB.pick and RB.pick.owner == d) then
        RB.pick = nil
    end
    heading:SetText(d.name or "")
    heading:SetTextColor(HeadingColor(d))
    local tab = tabs[RB.drawerTab]
    if not tab or not TabShown(tab, kind) then
        RB.drawerTab = "details"
    end
    for key, other in pairs(tabs) do
        RB.Shown(other.panel, key == RB.drawerTab)
    end
    RB.RefreshDrawer()
    RB.ApplyDrawerStep()
    RB.Prefetch(kind, id)
    RB.SchedulePreview(now)
    RB.ScheduleBuild()
end

drawer:SetScript("OnShow", function() RB.ApplyDrawerStep() end)

-- The splitter (6 px grip above the drawer).
local SPLITTER_LINE_HEIGHT = 6
local SPLITTER_LINE = 2

local splitter = CreateFrame("Frame", nil, RB.frame)
splitter:SetPoint("TOPLEFT", RB.bulk, "BOTTOMLEFT", 0, -1)
splitter:SetPoint("TOPRIGHT", RB.bulk, "BOTTOMRIGHT", 0, -1)
splitter:SetHeight(SPLITTER_LINE_HEIGHT)
splitter:EnableMouse(true)
RB.splitter = splitter

local line = splitter:CreateTexture(nil, "ARTWORK")
line:SetTexture("Interface\\ChatFrame\\ChatFrameBackground")
line:SetPoint("LEFT", 40, 0)
line:SetPoint("RIGHT", -40, 0)
line:SetHeight(SPLITTER_LINE)
RB.SetVertex(line, T.borderDim)

local function CursorY()
    local _, y = GetCursorPosition()
    return y / splitter:GetEffectiveScale()
end

splitter:SetScript("OnEnter", function()
    RB.SetVertex(line, T.title)
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
    self.dragging = { y = CursorY(), height = RB.DrawerHeight() }
end)
splitter:SetScript("OnMouseUp", function(self)
    self.dragging = nil
    RB.SetDrawerHeight(RB.DrawerHeight())
    if not MouseIsOver(self) then
        RB.SetVertex(line, T.borderDim)
    end
end)
splitter:SetScript("OnUpdate", function(self)
    local drag = self.dragging
    if drag then
        RB.SetDrawerHeight(drag.height + (CursorY() - drag.y), true)
    end
end)
