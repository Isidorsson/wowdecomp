-- Widget helpers on the stock 3.3.5 pieces, coloured from Theme.lua: panels with the tooltip edge tinted bronze, the
-- red-and-gold UIPanelButtonTemplate, gold-text tabs (TabButtonTemplate), dark input wells, and the grouped choice menu.

local RB = RetailBrowser
local T = RB.THEME

-- Choices per menu level before they are grouped into submenus (3.3.5 lists 24 buttons a level, Any is one).
local DROPDOWN_GROUP = 23
RB.EDGE_INSET = 4

RB.WINDOW_BACKDROP = {
    bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 16, edgeSize = 24,
    insets = { left = 6, right = 6, top = 6, bottom = 6 },
}
RB.PANEL_BACKDROP = {
    bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 16, edgeSize = 12,
    insets = { left = 3, right = 3, top = 3, bottom = 3 },
}
RB.INPUT_BACKDROP = {
    bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 16, edgeSize = 8,
    insets = { left = 2, right = 2, top = 2, bottom = 2 },
}

local function MakeFont(name, file, size, color)
    local font = CreateFont(name)
    font:SetFont(file, size, "")
    font:SetTextColor(color[1], color[2], color[3])
    return font
end
MakeFont("RetailBrowserGridFont", "Fonts\\ARIALN.TTF", 11, T.text)
MakeFont("RetailBrowserHeadFont", "Fonts\\FRIZQT__.TTF", 10, T.title)
MakeFont("RetailBrowserSmallFont", "Fonts\\ARIALN.TTF", 10, T.text)
MakeFont("RetailBrowserLabelFont", "Fonts\\ARIALN.TTF", 10, T.label)
MakeFont("RetailBrowserValueFont", "Fonts\\ARIALN.TTF", 11, T.value)

-- A panel (style panel, well or band) with the tooltip edge in bronze (dim for wells).
function RB.CreatePanel(parent, style)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetBackdrop(RB.PANEL_BACKDROP)
    RB.SetBack(panel, T[style or "panel"])
    RB.SetBorder(panel, style == "well" and T.borderDim or T.border)
    return panel
end

function RB.CreateText(parent, font, justify)
    local text = parent:CreateFontString(nil, "OVERLAY", font or "RetailBrowserSmallFont")
    text:SetJustifyH(justify or "LEFT")
    return text
end

-- A solid fill in a theme colour (show and hide it; SetAlpha would overwrite the vertex alpha).
function RB.CreateFill(parent, layer, color)
    local fill = parent:CreateTexture(nil, layer or "BACKGROUND")
    fill:SetTexture("Interface\\ChatFrame\\ChatFrameBackground")
    RB.SetVertex(fill, color)
    fill:SetAllPoints()
    return fill
end

-- The additive soft gold highlight of rows and list entries.
function RB.SetHoverHighlight(button)
    local glow = button:CreateTexture(nil, "HIGHLIGHT")
    glow:SetTexture("Interface\\ChatFrame\\ChatFrameBackground")
    RB.SetVertex(glow, T.hover)
    glow:SetBlendMode("ADD")
    glow:SetAllPoints()
    button:SetHighlightTexture(glow)
end

local function SetLabel(self, text)
    self:SetText(text)
end

-- A stock red-and-gold button with small text; SetOn locks its highlight (toggles, scopes).
function RB.CreateButton(parent, text, width, height, onClick)
    local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    button:SetSize(width, height or 20)
    button:SetNormalFontObject(GameFontNormalSmall)
    button:SetHighlightFontObject(GameFontHighlightSmall)
    button:SetDisabledFontObject(GameFontDisableSmall)
    button:SetText(text)
    button.label = button:GetFontString()
    button:SetScript("OnClick", onClick)
    button.SetLabel = SetLabel
    button.SetOn = function(self, on) if on then self:LockHighlight() else self:UnlockHighlight() end end
    button.SetEnabled = function(self, on) if on then self:Enable() else self:Disable() end end
    return button
end

-- A gold-text tab; SetOn selects it.
function RB.CreateTab(parent, name, text, onClick)
    local tab = CreateFrame("Button", name, parent, "TabButtonTemplate")
    tab:SetText(text)
    tab.label = tab:GetFontString()
    tab:SetScript("OnClick", onClick)
    tab.SetLabel = function(self, label)
        self:SetText(label)
        PanelTemplates_TabResize(self, 0)
    end
    tab.SetOn = function(self, on)
        if on then PanelTemplates_SelectTab(self) else PanelTemplates_DeselectTab(self) end
    end
    PanelTemplates_TabResize(tab, 0)
    PanelTemplates_DeselectTab(tab)
    return tab
end

-- A box's placeholder: grey text over an empty box, hidden while it has focus or text (3.3.5 has no placeholder).
function RB.RefreshHint(box)
    RB.Shown(box.hint, box:GetText() == "" and not box:HasFocus())
end

function RB.SetHint(box, text)
    box.hint:SetText(text or "")
    RB.RefreshHint(box)
end

local function WellFocus(self)
    RB.SetBorder(self, T.inputFocus)
    if self.hint then self.hint:Hide() end
end

local function WellBlur(self)
    RB.SetBorder(self, self.edgeColor or T.inputBorder)
    if self.hint then RB.RefreshHint(self) end
end

-- The state colour of a well's border (dim, active, invalid); focus draws it gold meanwhile.
function RB.SetWellEdge(well, color)
    well.edgeColor = color
    if not (well.HasFocus and well:HasFocus()) then
        RB.SetBorder(well, color)
    end
end

local function MakeWell(frame)
    frame:SetBackdrop(RB.INPUT_BACKDROP)
    RB.SetBack(frame, T.input)
    RB.SetBorder(frame, T.inputBorder)
end

-- A dark input well for the grid's filter row and the Build options; HookScript its focus events.
function RB.CreateCellBox(parent, name)
    local box = CreateFrame("EditBox", name, parent)
    box:SetHeight(16)
    box:SetAutoFocus(false)
    box:SetFontObject(RetailBrowserSmallFont)
    box:SetTextInsets(4, 4, 0, 0)
    MakeWell(box)
    box:SetScript("OnEditFocusGained", WellFocus)
    box:SetScript("OnEditFocusLost", WellBlur)
    box.hint = RB.CreateText(box, "RetailBrowserSmallFont")
    box.hint:SetPoint("LEFT", 5, 0)
    box.hint:SetPoint("RIGHT", -4, 0)
    box.hint:SetHeight(14)
    RB.SetColor(box.hint, T.disabled)
    return box
end

-- An input-well button for choices (it opens the choice menu).
function RB.CreateChoiceButton(parent, width, onClick)
    local button = CreateFrame("Button", nil, parent)
    button:SetSize(width, 16)
    MakeWell(button)
    button.label = RB.CreateText(button, "RetailBrowserSmallFont")
    button.label:SetPoint("LEFT", 5, 0)
    button.label:SetPoint("RIGHT", -4, 0)
    RB.SetHoverHighlight(button)
    button:SetScript("OnClick", onClick)
    button.SetLabel = function(self, text) self.label:SetText(text) end
    return button
end

local function AddChoice(menu, choice, level)
    local info = UIDropDownMenu_CreateInfo()
    info.text = choice[2]
    info.checked = menu.value == choice[1] and (choice[3] == nil or choice[3] == menu.valueKey)
    info.func = function()
        CloseDropDownMenus()
        menu.onPick(choice[1], choice[2], choice[3])
    end
    UIDropDownMenu_AddButton(info, level)
end

-- Up to DROPDOWN_GROUP choices are listed; more are grouped in submenus named by their first and last choice. Any
-- (nil) comes first. A choice may name its own filter key as its third value.
local function InitializeMenu(menu, level)
    level = level or 1
    local choices = menu.choices or {}
    if level > 1 then
        local group = UIDROPDOWNMENU_MENU_VALUE
        for i = (group - 1) * DROPDOWN_GROUP + 1, math.min(group * DROPDOWN_GROUP, #choices) do
            AddChoice(menu, choices[i], level)
        end
        return
    end
    AddChoice(menu, { nil, "Any" }, level)
    if #choices <= DROPDOWN_GROUP then
        for _, choice in ipairs(choices) do
            AddChoice(menu, choice, level)
        end
        return
    end
    for group = 1, math.ceil(#choices / DROPDOWN_GROUP) do
        local first = choices[(group - 1) * DROPDOWN_GROUP + 1]
        local last = choices[math.min(group * DROPDOWN_GROUP, #choices)]
        local info = UIDropDownMenu_CreateInfo()
        info.text = first[2] .. " - " .. last[2]
        info.hasArrow = true
        info.notCheckable = true
        info.value = group
        UIDropDownMenu_AddButton(info, level)
    end
end

-- One shared menu for every choice cell: onPick(value, text, key) gets the choice.
local choiceMenu = CreateFrame("Frame", "RetailBrowserChoiceMenu", UIParent, "UIDropDownMenuTemplate")
choiceMenu.displayMode = "MENU"
UIDropDownMenu_Initialize(choiceMenu, InitializeMenu, "MENU")

function RB.ShowChoiceMenu(anchor, choices, value, valueKey, onPick)
    choiceMenu.choices, choiceMenu.value, choiceMenu.valueKey, choiceMenu.onPick = choices, value, valueKey, onPick
    ToggleDropDownMenu(1, nil, choiceMenu, anchor, 0, 0)
end

function RB.QualityColor(quality)
    local color = T.quality[quality or 1] or T.quality[1]
    return color[1], color[2], color[3]
end

-- Creature rank groups as the filter has them: 0 normal, 1 elite, 2 rare, 3 rare elite (retail 1 and 3 are elite,
-- 2 rare elite, 4 rare).
function RB.RankGroup(classification)
    if classification == 1 or classification == 3 then
        return 1
    elseif classification == 2 then
        return 3
    elseif classification == 4 then
        return 2
    end
    return 0
end

function RB.RankColor(group)
    local color = T.rank[group or 0] or T.rank[0]
    return color[1], color[2], color[3]
end

-- A retail icon by FileDataID, which the client serves from CASC.
function RB.IconPath(fdid)
    if fdid and fdid > 0 then
        return "retail\\" .. fdid .. ".blp"
    end
    return "Interface\\Icons\\INV_Misc_QuestionMark"
end

function RB.Shown(region, shown)
    if shown then region:Show() else region:Hide() end
end
