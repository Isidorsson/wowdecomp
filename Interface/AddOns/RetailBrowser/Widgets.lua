-- Small widget helpers on the 3.3.5 templates (UIPanelButtonTemplate, InputBoxTemplate, UIDropDownMenuTemplate).

local RB = RetailBrowser

-- Choices per dropdown level before they are grouped into submenus.
local DROPDOWN_GROUP = 20

RB.PANEL_BACKDROP = {
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 },
}
RB.INSET_BACKDROP = {
    bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 16, edgeSize = 14,
    insets = { left = 3, right = 3, top = 3, bottom = 3 },
}

function RB.CreateInset(parent, width, height)
    local inset = CreateFrame("Frame", nil, parent)
    inset:SetSize(width, height)
    inset:SetBackdrop(RB.INSET_BACKDROP)
    inset:SetBackdropColor(0, 0, 0, 0.55)
    inset:SetBackdropBorderColor(0.5, 0.5, 0.5, 0.9)
    return inset
end

function RB.CreateButton(parent, text, width, onClick)
    local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    button:SetSize(width, 22)
    button:SetText(text)
    button:SetScript("OnClick", onClick)
    return button
end

function RB.CreateText(parent, font, justify)
    local text = parent:CreateFontString(nil, "OVERLAY", font or "GameFontHighlightSmall")
    text:SetJustifyH(justify or "LEFT")
    return text
end

function RB.CreateEditBox(parent, name, width, numeric)
    local box = CreateFrame("EditBox", name, parent, "InputBoxTemplate")
    box:SetSize(width, 20)
    box:SetAutoFocus(false)
    box:SetNumeric(numeric and true or false)
    box:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
    return box
end

local function SelectChoice(dropdown, value, text)
    dropdown.value = value
    UIDropDownMenu_SetText(dropdown, text)
    CloseDropDownMenus()
    if dropdown.onChange then
        dropdown.onChange(dropdown, value)
    end
end

local function AddChoice(dropdown, choice, level)
    local info = UIDropDownMenu_CreateInfo()
    info.text = choice[2]
    info.checked = dropdown.value == choice[1]
    info.func = function() SelectChoice(dropdown, choice[1], choice[2]) end
    UIDropDownMenu_AddButton(info, level)
end

-- Up to DROPDOWN_GROUP choices are listed; more are grouped in submenus named by their first and last choice.
local function InitializeDropdown(dropdown, level)
    level = level or 1
    local choices = dropdown.choices
    if level > 1 then
        local group = UIDROPDOWNMENU_MENU_VALUE
        for i = (group - 1) * DROPDOWN_GROUP + 1, math.min(group * DROPDOWN_GROUP, #choices) do
            AddChoice(dropdown, choices[i], level)
        end
        return
    end
    AddChoice(dropdown, { nil, "Any" }, level)
    if #choices <= DROPDOWN_GROUP then
        for _, choice in ipairs(choices) do
            AddChoice(dropdown, choice, level)
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

-- A labelled dropdown of { value, text } choices with "Any" (nil) first.
function RB.CreateDropdown(parent, name, width, onChange)
    local dropdown = CreateFrame("Frame", name, parent, "UIDropDownMenuTemplate")
    UIDropDownMenu_SetWidth(dropdown, width)
    dropdown.label = RB.CreateText(dropdown, "GameFontNormalSmall")
    dropdown.label:SetPoint("BOTTOMLEFT", dropdown, "TOPLEFT", 20, 0)
    dropdown.choices = {}
    dropdown.onChange = onChange
    UIDropDownMenu_Initialize(dropdown, InitializeDropdown)
    return dropdown
end

function RB.SetDropdown(dropdown, label, choices)
    dropdown.label:SetText(label)
    dropdown.choices = choices or {}
    dropdown.value = nil
    UIDropDownMenu_SetText(dropdown, "Any")
end

function RB.QualityColor(quality)
    local color = ITEM_QUALITY_COLORS and ITEM_QUALITY_COLORS[quality or 1]
    if color then
        return color.r, color.g, color.b
    end
    return 1, 1, 1
end

-- A retail icon by FileDataID, which the client serves from CASC.
function RB.IconPath(fdid)
    if fdid and fdid > 0 then
        return "retail\\" .. fdid .. ".blp"
    end
    return "Interface\\Icons\\INV_Misc_QuestionMark"
end
