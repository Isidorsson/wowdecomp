-- The 3D previews. The drawer's: a DressUpModel filling the drawer's height, showing a creature display
-- (SetDisplayInfo, dressed and baked for humanoid NPCs) or the player wearing an item display or a whole set
-- (TryOnDisplay). Its toolbar picks the display, turns, zooms, fits, resets, plays an animation and copies the
-- display id; drag turns it too and the wheel zooms. Fit frames it by the display's scale and the creature's type.
-- The hover preview: one shared model shown 0.4 s after the mouse rests on a grid row.

local RB = RetailBrowser
local T = RB.THEME

local PREVIEW_DELAY = 0.25
local HOVER_DELAY = 0.4
local SCROLL_QUIET = 0.2
local ZOOM_STEP = 1
local TURN_STEP = math.pi / 8
local TOOLBAR_HEIGHT = 30
-- How far back fit puts a display, per retail creature type (Beast 1, Dragonkin 2, Elemental 4, Giant 5,
-- Mechanical 9), before the display's own scale; mounts sit a little further back.
local FIT_BY_TYPE = { [1] = 1, [2] = 4.5, [4] = 1.5, [5] = 3.5, [9] = 1.5 }
local FIT_MOUNT = 2.5
-- Animations the toolbar cycles through (AnimationData ids).
local ANIMATIONS = { { 0, "Stand" }, { 4, "Walk" }, { 5, "Run" }, { 26, "Ready" }, { 60, "Talk" }, { 69, "Dance" } }

local slot = RB.drawerPreviewSlot
local back = RB.CreatePanel(slot, "well")
back:SetPoint("TOPLEFT")
back:SetPoint("BOTTOMRIGHT", 0, TOOLBAR_HEIGHT)
RB.SetBack(back, T.preview)

local model = CreateFrame("DressUpModel", "RetailBrowserPreview", slot)
model:SetPoint("TOPLEFT", 4, -4)
model:SetPoint("BOTTOMRIGHT", -4, TOOLBAR_HEIGHT + 4)
model:EnableMouse(true)
model:EnableMouseWheel(true)
RB.preview = model

local caption = RB.CreateText(slot, "RetailBrowserLabelFont", "CENTER")
caption:SetPoint("BOTTOMLEFT", 6, TOOLBAR_HEIGHT + 6)
caption:SetPoint("BOTTOMRIGHT", -6, TOOLBAR_HEIGHT + 6)

local zoom, fitZoom, animation = 0, 0, 1
local function ApplyZoom()
    local _, y, z = model:GetPosition()
    model:SetPosition(zoom, y or 0, z or 0)
end

-- The animation runs on a clock the model's OnUpdate advances (SetSequence alone did not move retail displays);
-- Stand hands the model back to its own idle.
local animationClock = 0
local function PlayAnimation()
    animationClock = 0
end

-- The toolbar: its buttons share the preview's width.
local toolbar = CreateFrame("Frame", nil, slot)
toolbar:SetPoint("BOTTOMLEFT")
toolbar:SetPoint("BOTTOMRIGHT")
toolbar:SetHeight(TOOLBAR_HEIGHT)
local tools = {}
local function Tool(text, weight, onClick)
    local button = RB.CreateButton(toolbar, text, 20, 22, onClick)
    button.weight = weight
    tools[#tools + 1] = button
    return button
end

Tool("<", 1, function() RB.StepPick(-1) end)
Tool(">", 1, function() RB.StepPick(1) end)
Tool("<<", 1.2, function() model:SetFacing(model:GetFacing() - TURN_STEP) end)
Tool(">>", 1.2, function() model:SetFacing(model:GetFacing() + TURN_STEP) end)
Tool("+", 1, function() RB.ZoomPreview(1) end)
Tool("-", 1, function() RB.ZoomPreview(-1) end)
Tool("Fit", 1.3, function() RB.FitPreview() end)
Tool("Reset", 1.7, function()
    model:SetFacing(0)
    zoom = fitZoom
    ApplyZoom()
end)
Tool("Stand", 1.9, function(self)
    animation = animation % #ANIMATIONS + 1
    self:SetLabel(ANIMATIONS[animation][2])
    PlayAnimation()
end)
Tool("Copy", 1.6, function() RB.RunAction("copyDisplay") end)
local TOOL_TIPS = { "tool_prev", "tool_next", "tool_turnLeft", "tool_turnRight", "tool_zoomIn", "tool_zoomOut", "tool_fit", "tool_reset",
    "tool_anim", "tool_copy" }
for i, button in ipairs(tools) do
    RB.AttachTip(button, TOOL_TIPS[i])
end
RB.AttachTip(model, "preview")

toolbar:SetScript("OnSizeChanged", function(self, width)
    local weights = 0
    for _, button in ipairs(tools) do weights = weights + button.weight end
    local unit = (width - 2 * (#tools - 1)) / weights
    local x = 0
    for _, button in ipairs(tools) do
        button:SetWidth(unit * button.weight)
        button:ClearAllPoints()
        button:SetPoint("LEFT", toolbar, "LEFT", x, 0)
        x = x + unit * button.weight + 2
    end
end)

model:SetScript("OnMouseDown", function(self)
    self.dragX = GetCursorPosition()
    self.dragFacing = self:GetFacing()
end)
model:SetScript("OnMouseUp", function(self) self.dragX = nil end)
model:SetScript("OnMouseWheel", function(self, delta) RB.ZoomPreview(delta) end)
model:SetScript("OnUpdate", function(self, elapsed)
    if self.dragX then
        local x = GetCursorPosition()
        self:SetFacing(self.dragFacing + (x - self.dragX) / 80)
    end
    if animation > 1 then
        animationClock = animationClock + elapsed * 1000
        self:SetSequenceTime(ANIMATIONS[animation][1], animationClock)
    end
end)

-- The fit distance of a creature or mount display: by type, then by the display's scale.
local function FitOf(d, picked)
    local base = d.kind == "mount" and FIT_MOUNT or FIT_BY_TYPE[d.type or 0] or 0
    local scale = picked and picked.scale or 1
    local back = base + math.max(0, scale - 1) * 3
    return back > 0 and -back or 0
end

local function Wear(target, displayId, inventoryType, sheathe, classId, subclassId)
    return displayId and displayId > 0 and target:TryOnDisplay(displayId, inventoryType or 0, sheathe or 0, classId or 0, subclassId or 0)
end

local function WearItem(target, item, look)
    target:SetUnit("player")
    target:SetFacing(0)
    look = look or (item.appearances and item.appearances[1])
    return look and Wear(target, look.displayId, item.inventoryType, item.sheatheType, item.classId, item.subclassId)
end

-- A transmog set lists every item of each look (often several per slot): the first of each slot is worn.
local function WearPieces(target, pieces, slots)
    local worn = 0
    for _, piece in ipairs(pieces) do
        local slotId = piece.inventoryType == 20 and 5 or piece.inventoryType
        if not slots[slotId] and Wear(target, piece.displayId, piece.inventoryType) then
            slots[slotId] = true
            worn = worn + 1
        end
    end
    return worn
end

local function WearSet(target, set)
    target:SetUnit("player")
    target:Undress()
    target:SetFacing(0)
    return WearPieces(target, set.pieces or {}, {})
end

local function ShowDisplay(target, display, fit)
    target:SetDisplayInfo(display)
    target:SetFacing(0)
    target:SetPosition(fit or 0, 0, 0)
end

-- "Display 129156", with its place among the entry's displays when there are several: "Display 126134 (2/8)".
local function DisplayCaption(d, display)
    local list = RB.PickList(d)
    local _, index = RB.Picked(d)
    if #list > 1 and list[index] and list[index].displayId == display then
        return string.format("Display %d (%d/%d)", display, index, #list)
    end
    return "Display " .. display
end

-- Shows an entry on a model; the caption text.
local function ShowOn(target, d, picked)
    if d.kind == "item" then
        local worn = WearItem(target, d, picked)
        return worn and ("on you, display " .. picked.displayId) or "this slot has no model"
    elseif d.kind == "set" then
        return WearSet(target, d) .. " slots worn, " .. #(d.pieces or {}) .. " pieces"
    elseif d.kind == "model" and not d.creature then
        local user = picked or (d.users and d.users[1])
        local item = user and RB.DetailsOf("item", user.id)
        return item and WearItem(target, item) and ("on you, item " .. user.id) or "no item to wear"
    end
    local display = RB.ActionDisplay(d)
    if not display then
        return "no display"
    end
    local fit = d.kind == "model" and 0 or FitOf(d, picked)
    ShowDisplay(target, display, fit)
    return string.format("%s, fit %g", DisplayCaption(d, display), fit), fit
end

function RB.Preview(d)
    if not d then
        return
    end
    local picked = RB.Picked(d)
    local text, fit = ShowOn(model, d, picked)
    zoom, fitZoom = fit or 0, fit or 0
    caption:SetText(text)
    if animation > 1 then
        PlayAnimation()
    end
end

function RB.FitPreview()
    local d = RB.details
    if d and d.kind ~= "item" and d.kind ~= "set" then
        zoom = FitOf(d, RB.Picked(d))
        fitZoom = zoom
        ApplyZoom()
    end
end

function RB.ZoomPreview(steps)
    zoom = zoom + steps * ZOOM_STEP
    ApplyZoom()
end

-- Wears every checked item and set at once (the bulk bar's Try on).
function RB.TryOnAll(list)
    model:SetUnit("player")
    model:Undress()
    model:SetFacing(0)
    local slots, worn = {}, 0
    for _, d in ipairs(list) do
        if d.kind == "item" then
            local look = d.appearances and d.appearances[1]
            worn = worn + WearPieces(model, look and { { inventoryType = d.inventoryType, displayId = look.displayId } } or {}, slots)
        elseif d.kind == "set" then
            worn = worn + WearPieces(model, d.pieces or {}, slots)
        end
    end
    caption:SetText(worn .. " slots worn from " .. #list .. " rows")
end

-- The drawer preview follows the focused row after a pause (arrowing through rows loads one model at the end).
function RB.SchedulePreview(now)
    if now then
        RB.previewAt = nil
        RB.Preview(RB.details)
        if RB.details then RB.RememberViewed(RB.details.kind, RB.details.id, RB.details.name) end
        return
    end
    RB.previewAt = GetTime() + PREVIEW_DELAY
end

-- Hover preview: one shared model on a tooltip frame.
local hover = CreateFrame("Frame", "RetailBrowserHover", UIParent)
hover:SetSize(170, 210)
hover:SetFrameStrata("TOOLTIP")
hover:SetBackdrop(RB.PANEL_BACKDROP)
RB.SetBack(hover, T.panel)
RB.SetBorder(hover, T.border)
hover:Hide()
local hoverModel = CreateFrame("DressUpModel", "RetailBrowserHoverModel", hover)
hoverModel:SetPoint("TOPLEFT", 4, -4)
hoverModel:SetPoint("BOTTOMRIGHT", -4, 16)
local hoverCaption = RB.CreateText(hover, "RetailBrowserLabelFont", "CENTER")
hoverCaption:SetPoint("BOTTOMLEFT", 4, 4)
hoverCaption:SetPoint("BOTTOMRIGHT", -4, 4)

function RB.HoverRow(row)
    local entry = RB.rows[row.index]
    RB.hoverRow, RB.hoverEntry = row, entry
    RB.hoverAt = entry and GetTime() + HOVER_DELAY
    if entry then
        RB.Prefetch(entry.kind, entry.id)
    end
end

function RB.HoverEnd()
    RB.hoverRow, RB.hoverEntry, RB.hoverAt = nil, nil, nil
    hover:Hide()
end

local function ShowHover()
    local row, entry = RB.hoverRow, RB.hoverEntry
    local d = entry and (entry.d or RB.DetailsOf(entry.kind, entry.id))
    if not d or not row:IsVisible() then
        return
    end
    local scale = row:GetEffectiveScale() / UIParent:GetEffectiveScale()
    local x = GetCursorPosition() / UIParent:GetEffectiveScale()
    hover:ClearAllPoints()
    hover:SetPoint("BOTTOMLEFT", UIParent, "BOTTOMLEFT", x + 24, row:GetBottom() * scale - 60)
    hover:SetClampedToScreen(true)
    hover:Show()
    local list = RB.PickList(d)
    local text = ShowOn(hoverModel, d, list[1])
    hoverCaption:SetText(text or "")
end

local timer = CreateFrame("Frame", nil, RB.frame)
timer:SetScript("OnUpdate", function()
    local now = GetTime()
    if RB.previewAt and now >= RB.previewAt then
        RB.SchedulePreview(true)
    end
    if RB.hoverAt and now >= RB.hoverAt then
        if RB.scrolledAt and now - RB.scrolledAt < SCROLL_QUIET then
            RB.hoverAt = now + SCROLL_QUIET
        else
            RB.hoverAt = nil
            ShowHover()
        end
    end
end)
RB.frame:HookScript("OnHide", RB.HoverEnd)
