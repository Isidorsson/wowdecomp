-- The Build SQL page's settings, in wells: Entries (start, base), Identity (name, subname, scale), Displays (one creature
-- each, picked only or modelid1-4 variants, with a hint), and Item options (item level, required level, hybrid
-- primary, retail stamina, every set variant, retail armour), which fold shut while the build has no item or set.
-- Every change rebuilds after a pause. RB.BuildOptionsFor(d, start) turns them into RetailBuildSql's options.

local RB = RetailBrowser
local T = RB.THEME

local PAD = 12
local WELL_GAP = 10
local TITLE_SPACE = 22
local FIELD_HEIGHT = 20
local FIELD_SPACE = 24
local HINT_SPACE = 14
local LABEL_WIDTH = 104
local COMBOS = { { 1, "Strength" }, { 2, "Agility" }, { 3, "Intellect" } }

local col = RB.buildOptionsCol
local boxes = {}

local function Well(title)
    local well = RB.CreatePanel(col, "well")
    well:SetPoint("LEFT", PAD, 0)
    well:SetPoint("RIGHT", -PAD, 0)
    well.title = RB.CreateText(well, "RetailBrowserHeadFont")
    well.title:SetPoint("TOPLEFT", 10, -8)
    well.title:SetText(title)
    RB.SetColor(well.title, T.section)
    well.y = TITLE_SPACE + 6
    return well
end

local function Label(well, text)
    local label = RB.CreateText(well, "RetailBrowserLabelFont")
    label:SetPoint("TOPLEFT", 10, -well.y - 4)
    label:SetText(text)
    return label
end

local function Hint(well, text)
    local hint = RB.CreateText(well, "RetailBrowserLabelFont")
    hint:SetPoint("TOPLEFT", 10 + LABEL_WIDTH, -well.y)
    hint:SetText(text)
    well.y = well.y + HINT_SPACE
    return hint
end

local function Field(well, name, label, width, tipKey)
    Label(well, label)
    local box = RB.CreateCellBox(well, "RetailBrowserBuild" .. name)
    box:SetPoint("TOPLEFT", 10 + LABEL_WIDTH, -well.y)
    box:SetSize(width, FIELD_HEIGHT)
    RB.AttachTip(box, tipKey)
    box:SetScript("OnTextChanged", function(self)
        RB.RefreshHint(self)
        RB.ScheduleBuild()
    end)
    box:SetScript("OnEnterPressed", function() RB.FocusPage() end)
    box:SetScript("OnEscapePressed", function() RB.FocusPage() end)
    box:HookScript("OnEditFocusGained", function() RB.SetKeyOwner("build") end)
    box:HookScript("OnEditFocusLost", function() RB.KeyOwnerLost() end)
    boxes[#boxes + 1] = box
    local index = #boxes
    box:SetScript("OnTabPressed", function() boxes[index % #boxes + 1]:SetFocus() end)
    well.y = well.y + FIELD_SPACE
    return box
end

local function Check(well, name, label, checked, tipKey)
    local check = CreateFrame("CheckButton", "RetailBrowserBuild" .. name, well, "UICheckButtonTemplate")
    check:SetSize(18, 18)
    check:SetPoint("TOPLEFT", 6, -well.y)
    local text = _G[check:GetName() .. "Text"]
    text:SetFontObject(RetailBrowserButtonFont)
    text:SetText(label)
    check:SetChecked(checked)
    check:SetScript("OnClick", function() RB.ScheduleBuild() end)
    RB.AttachTip(check, tipKey)
    well.y = well.y + FIELD_SPACE - 2
    return check
end

local function Finish(well)
    well:SetHeight(well.y + 6)
end

-- Entries.
local entries = Well("Entries")
entries:SetPoint("TOP", 0, -PAD)
local startEntry = Field(entries, "StartEntry", "Start entry", 120, "opt_startEntry")
Hint(entries, "Empty = ask the server for a free run")
local baseEntry = Field(entries, "BaseEntry", "Base entry", 120, "opt_baseEntry")
Hint(entries, "Every row copies this stock row, then overrides")
Finish(entries)
RB.SetHint(startEntry, "free from server")

-- Identity.
local identity = Well("Identity")
identity:SetPoint("TOP", entries, "BOTTOM", 0, -WELL_GAP)
local nameBox = Field(identity, "Name", "Name", 220, "opt_name")
local subnameBox = Field(identity, "Subname", "Subname", 180, "opt_subname")
local scale = Field(identity, "Scale", "Scale", 70, "opt_scale")
Finish(identity)
RB.SetHint(scale, "base's")

-- Displays: a three-way switch with a line on what the chosen way builds.
local displays = Well("Displays")
displays:SetPoint("TOP", identity, "BOTTOM", 0, -WELL_GAP)
local modeButtons = {}
local modeHint
local displayMode = RB.DISPLAY_MODES[1][1]

local function PaintModes()
    for _, button in ipairs(modeButtons) do
        button:SetOn(button.mode == displayMode)
    end
    modeHint:SetText(RB.DisplayModeHint(displayMode))
end

local function SetDisplayMode(mode)
    displayMode = mode
    PaintModes()
    RB.ScheduleBuild()
end

for i, mode in ipairs(RB.DISPLAY_MODES) do
    local button = RB.CreateButton(displays, mode[2], 112, FIELD_HEIGHT + 2, function() SetDisplayMode(mode[1]) end)
    button:SetPoint("TOPLEFT", 10 + (i - 1) * 114, -displays.y)
    button.mode = mode[1]
    RB.AttachTip(button, "opt_displays")
    modeButtons[i] = button
end
displays.y = displays.y + FIELD_SPACE + 2
modeHint = RB.CreateText(displays, "RetailBrowserLabelFont")
modeHint:SetPoint("TOPLEFT", 10, -displays.y)
modeHint:SetPoint("RIGHT", -10, 0)
displays.y = displays.y + HINT_SPACE * 2
Finish(displays)

-- Item options: folded shut while the build has no item or set.
local items = Well("")
items:SetPoint("TOP", displays, "BOTTOM", 0, -WELL_GAP)
local body = CreateFrame("Frame", nil, items)
body:SetPoint("TOPLEFT")
body:SetPoint("BOTTOMRIGHT")
body.y = items.y
local itemLevel = Field(body, "ItemLevel", "Item level", 70, "opt_itemLevel")
local requiredLevel = Field(body, "RequiredLevel", "Required level", 70, "opt_requiredLevel")
RB.SetHint(requiredLevel, "0 (any level)")
Label(body, "Hybrid primary")
local combo = RB.CreateChoiceButton(body, 150, nil)
combo:SetPoint("TOPLEFT", 10 + LABEL_WIDTH, -body.y)
combo:SetHeight(FIELD_HEIGHT)
combo:SetLabel("Any: first named")
RB.AttachTip(combo, "opt_combo")
combo:SetScript("OnClick", function(self)
    RB.ShowChoiceMenu(self, COMBOS, self.value, nil, function(value, text)
        self.value = value
        self:SetLabel(value and text or "Any: first named")
        RB.ScheduleBuild()
    end)
end)
body.y = body.y + FIELD_SPACE
local retailStamina = Check(body, "RetailStamina", "Retail stamina multiplier", false, "opt_retailStamina")
local allVariants = Check(body, "AllVariants", "Every set variant", false, "opt_allVariants")
local retailArmour = Check(body, "RetailArmour", "Retail armour/damage", true, "opt_retailArmour")
local openHeight = body.y + 6

local fold = CreateFrame("Button", nil, items)
fold:SetPoint("TOPLEFT", 4, -2)
fold:SetPoint("TOPRIGHT", -4, -2)
fold:SetHeight(TITLE_SPACE)
local itemsOpen, foldForced
local function ShowItemOptions(open)
    itemsOpen = open
    RB.Shown(body, open)
    items:SetHeight(open and openHeight or TITLE_SPACE + 6)
    items.title:SetText(open and "Item options" or ("Item options " .. RB.Colored("· not used by creatures", T.label)))
end
fold:SetScript("OnClick", function()
    foldForced = not itemsOpen
    ShowItemOptions(foldForced)
end)
RB.AttachTip(fold, "build_itemOptions")

-- Opens the item options for a build with an item or a set and folds them for one without (a click overrides it
-- until the build's kinds change).
function RB.SetItemOptionsOpen(open)
    if foldForced ~= nil and foldForced ~= open then
        return
    end
    foldForced = nil
    ShowItemOptions(open)
end
ShowItemOptions(false)
PaintModes()

function RB.BuildTypedStart()
    return tonumber(startEntry:GetText())
end

function RB.BuildBaseTyped()
    return baseEntry:GetText() ~= ""
end

function RB.SetStartEntryHint(hint)
    RB.SetHint(startEntry, hint)
end

-- Empty override fields show the retail value they stand for (the SQL uses it); typed text replaces it.
function RB.BuildRetailHints(list, result, defaultItemLevel)
    local d = list[1]
    local many = #list > 1
    RB.SetHint(itemLevel, many and "each own" or tostring(d and (d.sparseItemLevel or d.itemLevel) or defaultItemLevel))
    RB.SetHint(nameBox, many and ("retail names (" .. #list .. " rows)") or (d and d.name or "retail name"))
    RB.SetHint(subnameBox, d and RB.BUILD_TABLES[d.kind] == "creature" and (many and "retail titles" or (d.title ~= "" and d.title or "none")) or "creatures only")
    RB.SetHint(baseEntry, result and result.base and ("suggested " .. result.base) or "suggested")
end

-- The RetailBuildSql options for one entry, its rows starting at start.
function RB.BuildOptionsFor(d, start)
    local options = {
        itemLevel = tonumber(itemLevel:GetText()) or (d.kind == "item" and (d.sparseItemLevel or d.itemLevel)) or nil,
        startEntry = start,
        baseEntry = tonumber(baseEntry:GetText()),
        requiredLevel = tonumber(requiredLevel:GetText()),
        scale = tonumber(scale:GetText()),
        combo = combo.value,
        retailStamina = retailStamina:GetChecked() and 1 or 0,
        allVariants = allVariants:GetChecked() and 1 or 0,
        displays = displayMode,
        retailArmour = retailArmour:GetChecked() and 1 or 0,
        display = RB.ActionDisplay(d),
    }
    if d.kind == "item" then
        local _, index = RB.Picked(d)
        options.appearance = index
    end
    if nameBox:GetText() ~= "" then options.name = nameBox:GetText() end
    if subnameBox:GetText() ~= "" then options.subname = subnameBox:GetText() end
    return options
end
