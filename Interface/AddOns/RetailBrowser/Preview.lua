-- The 3D preview: a DressUpModel showing a creature display (SetDisplayInfo, dressed and baked for humanoid NPCs)
-- or the player wearing an item display or a whole set (TryOnDisplay). Drag to turn it.

local RB = RetailBrowser
local frame = RB.frame

local model = CreateFrame("DressUpModel", "RetailBrowserPreview", frame)
model:SetSize(210, 196)
model:SetPoint("TOPLEFT", RB.pane, "BOTTOMLEFT", 0, -8)
model:SetBackdrop(RB.INSET_BACKDROP)
model:SetBackdropColor(0, 0, 0, 0.6)
model:SetBackdropBorderColor(0.5, 0.5, 0.5, 0.9)
model:EnableMouse(true)
RB.preview = model

local caption = RB.CreateText(model, "GameFontDisableSmall", "CENTER")
caption:SetPoint("BOTTOM", 0, 6)
caption:SetWidth(200)

model:SetScript("OnMouseDown", function(self)
    self.dragX = GetCursorPosition()
    self.dragFacing = self:GetFacing()
end)
model:SetScript("OnMouseUp", function(self) self.dragX = nil end)
-- The wheel moves the model toward or away from the camera, for giants the paperdoll camera frames too close.
model:EnableMouseWheel(true)
model:SetScript("OnMouseWheel", function(self, delta)
    local x, y, z = self:GetPosition()
    self:SetPosition(x + delta * 2, y, z)
end)
model:SetScript("OnUpdate", function(self)
    if self.dragX then
        local x = GetCursorPosition()
        self:SetFacing(self.dragFacing + (x - self.dragX) / 80)
    end
end)

-- Arrows that step the preview, the GM commands and the Build panel through an entry's displays.
local function StepButton(text, delta, point)
    local button = RB.CreateButton(model, text, 22, function()
        if RB.details then RB.StepDisplay(RB.details, delta) end
    end)
    button:SetPoint(point, delta < 0 and 4 or -4, 4)
    return button
end
local previous = StepButton("<", -1, "BOTTOMLEFT")
local following = StepButton(">", 1, "BOTTOMRIGHT")

local function ShowSteps(count)
    if count > 1 then
        previous:Show()
        following:Show()
    else
        previous:Hide()
        following:Hide()
    end
end

-- "Display 129156", with its place among the entry's displays when there are several.
local function DisplayCaption(details, display)
    local displays = RB.DisplaysOf(details)
    for i, entry in ipairs(displays) do
        if entry.displayId == display and #displays > 1 then
            return "Display " .. display .. " (" .. i .. "/" .. #displays .. ")"
        end
    end
    return "Display " .. display
end

local function ShowDisplay(details, display)
    model:SetDisplayInfo(display)
    model:SetPosition(0, 0, 0)
    model:SetFacing(0)
    caption:SetText(DisplayCaption(details, display))
end

local function Wear(displayId, inventoryType, sheathe, classId, subclassId)
    return displayId and displayId > 0 and model:TryOnDisplay(displayId, inventoryType, sheathe or 0, classId or 0, subclassId or 0)
end

local function WearItem(item)
    model:SetUnit("player")
    model:SetFacing(0)
    local look = item.appearances and item.appearances[1]
    local worn = look and Wear(look.displayId, item.inventoryType, item.sheatheType, item.classId, item.subclassId)
    caption:SetText(worn and ("Item display " .. look.displayId) or "This slot has no model")
end

local function WearSet(set)
    model:SetUnit("player")
    model:Undress()
    model:SetFacing(0)
    -- A transmog set lists every item of each look (often several per slot): the first of each slot is worn.
    local worn, slots = 0, {}
    for _, piece in ipairs(set.pieces or {}) do
        local slot = piece.inventoryType == 20 and 5 or piece.inventoryType
        if not slots[slot] and Wear(piece.displayId, piece.inventoryType) then
            slots[slot] = true
            worn = worn + 1
        end
    end
    caption:SetText(worn .. " slots worn, " .. #(set.pieces or {}) .. " pieces")
end

function RB.Preview(details)
    ShowSteps(#RB.DisplaysOf(details))
    if details.kind == "item" then
        WearItem(details)
    elseif details.kind == "set" then
        WearSet(details)
    elseif details.kind == "model" and not details.creature then
        local user = details.users and details.users[1]
        local item = user and RetailDetails("item", user.id)
        if item and item.status == "ok" then WearItem(item) end
    else
        local display = RB.ActionDisplay(details)
        if display then ShowDisplay(details, display) end
    end
end

-- The arrows follow the shown entry, previewed or not.
local updateActions = RB.UpdateActions
function RB.UpdateActions()
    updateActions()
    ShowSteps(#RB.DisplaysOf(RB.details))
end
