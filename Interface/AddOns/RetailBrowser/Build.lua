-- The Build panel: world DB SQL for the shown item, set or creature (RetailBuildSql). Every statement copies a stock
-- row (the base entry) and overrides only the columns it names; new entries count up from the start entry, which the
-- client cannot check is free. Retail stats are computed at the item level and mapped to 3.3.5 stats as listed.

local RB = RetailBrowser
local frame = RB.frame

local PANEL_WIDTH = 480
local PANEL_HEIGHT = 620
local TEXT_WIDTH = PANEL_WIDTH - 56
local MAPPING_HEIGHT = 150
local SQL_HEIGHT = 175
local DEFAULT_ITEM_LEVEL = 200
local DEFAULT_START_ENTRY = 9000000
local BUILD_KINDS = { item = true, set = true, creature = true }
local COMBOS = { { 1, "Strength" }, { 2, "Agility" }, { 3, "Intellect" } }

local panel = CreateFrame("Frame", "RetailBrowserBuildFrame", frame)
panel:SetSize(PANEL_WIDTH, PANEL_HEIGHT)
panel:SetPoint("TOPLEFT", frame, "TOPRIGHT", -4, 0)
panel:SetBackdrop(RB.PANEL_BACKDROP)
panel:EnableMouse(true)
panel:Hide()
RB.buildPanel = panel

local title = RB.CreateText(panel, "GameFontNormalLarge")
title:SetPoint("TOPLEFT", 22, -18)
title:SetText("Build SQL")
local subtitle = RB.CreateText(panel, "GameFontHighlightSmall")
subtitle:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -4)
subtitle:SetWidth(PANEL_WIDTH - 44)
local close = CreateFrame("Button", nil, panel, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -6, -6)

local function LabelledBox(name, label, width, numeric, x, y)
    local box = RB.CreateEditBox(panel, "RetailBrowserBuild" .. name, width, numeric)
    box:SetPoint("TOPLEFT", x, y)
    box.label = RB.CreateText(panel, "GameFontNormalSmall")
    box.label:SetPoint("BOTTOMLEFT", box, "TOPLEFT", -4, 2)
    box.label:SetText(label)
    box:SetScript("OnEnterPressed", function(self)
        self:ClearFocus()
        RB.BuildSql()
    end)
    return box
end

local itemLevel = LabelledBox("ItemLevel", "Item level", 50, true, 30, -70)
local startEntry = LabelledBox("StartEntry", "Start entry", 80, true, 100, -70)
local baseEntry = LabelledBox("BaseEntry", "Base entry", 70, true, 200, -70)
local requiredLevel = LabelledBox("RequiredLevel", "Required level", 40, true, 290, -70)
local scale = LabelledBox("Scale", "Scale", 40, false, 380, -70)
local nameBox = LabelledBox("Name", "Name (blank: retail)", 200, false, 30, -112)
local subnameBox = LabelledBox("Subname", "Subname (creature)", 180, false, 250, -112)
itemLevel:SetText(DEFAULT_ITEM_LEVEL)
startEntry:SetText(DEFAULT_START_ENTRY)

local combo = RB.CreateDropdown(panel, "RetailBrowserBuildCombo", 110, function() RB.BuildSql() end)
combo:SetPoint("TOPLEFT", 10, -150)
RB.SetDropdown(combo, "Hybrid primary (Any: first named)", COMBOS)

local function Check(name, label, x, y, checked)
    local check = CreateFrame("CheckButton", "RetailBrowserBuild" .. name, panel, "UICheckButtonTemplate")
    check:SetSize(24, 24)
    check:SetPoint("TOPLEFT", x, y)
    _G[check:GetName() .. "Text"]:SetText(label)
    check:SetChecked(checked)
    check:SetScript("OnClick", function() RB.BuildSql() end)
    return check
end

local retailStamina = Check("RetailStamina", "Retail stamina multiplier", 170, -150, false)
local allVariants = Check("AllVariants", "Every set variant", 330, -150, false)
local otherDisplays = Check("OtherDisplays", "Other displays as modelid2-4", 170, -174, false)

local status = RB.CreateText(panel, "GameFontDisableSmall")
status:SetPoint("TOPLEFT", 26, -194)
status:SetWidth(PANEL_WIDTH - 52)

-- A scrolling multi-line edit box: select text in it, or copy it with the buttons.
local function ScrollText(name, top, height)
    local scroll = CreateFrame("ScrollFrame", "RetailBrowserBuild" .. name, panel, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", 24, top)
    scroll:SetSize(TEXT_WIDTH, height)
    local back = RB.CreateInset(panel, TEXT_WIDTH + 30, height + 8)
    back:SetPoint("TOPLEFT", scroll, "TOPLEFT", -6, 4)
    back:SetFrameLevel(scroll:GetFrameLevel() - 1)
    local box = CreateFrame("EditBox", nil, scroll)
    box:SetMultiLine(true)
    box:SetMaxLetters(0)
    box:SetAutoFocus(false)
    box:SetFontObject(GameFontHighlightSmall)
    box:SetWidth(TEXT_WIDTH)
    box:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
    box:SetScript("OnCursorChanged", ScrollingEdit_OnCursorChanged)
    box:SetScript("OnUpdate", function(self, elapsed) ScrollingEdit_OnUpdate(self, elapsed, scroll) end)
    scroll:SetScrollChild(box)
    return box
end

local mapping = ScrollText("Mapping", -228, MAPPING_HEIGHT)
local sqlBox = ScrollText("Sql", -228 - MAPPING_HEIGHT - 22, SQL_HEIGHT)
local sqlLabel = RB.CreateText(panel, "GameFontNormalSmall")
sqlLabel:SetPoint("BOTTOMLEFT", sqlBox:GetParent(), "TOPLEFT", -2, 6)
sqlLabel:SetText("SQL (edit before copying if you like)")

local function Options()
    local options = {
        itemLevel = tonumber(itemLevel:GetText()) or DEFAULT_ITEM_LEVEL,
        startEntry = tonumber(startEntry:GetText()) or DEFAULT_START_ENTRY,
        baseEntry = tonumber(baseEntry:GetText()),
        requiredLevel = tonumber(requiredLevel:GetText()),
        scale = tonumber(scale:GetText()),
        combo = combo.value,
        retailStamina = retailStamina:GetChecked() and 1 or 0,
        allVariants = allVariants:GetChecked() and 1 or 0,
        otherDisplays = otherDisplays:GetChecked() and 1 or 0,
        display = RB.ActionDisplay(RB.details),
    }
    if nameBox:GetText() ~= "" then options.name = nameBox:GetText() end
    if subnameBox:GetText() ~= "" then options.subname = subnameBox:GetText() end
    return options
end

local function MappingText(result)
    local lines = {}
    for _, line in ipairs(result.lines or {}) do
        local text = string.format("%s: %s %d -> %s %d", line.piece, line.retail, line.retailValue, line.classic, line.value)
        if line.note ~= "" then
            text = text .. "  |cffaaaaaa(" .. line.note .. ")|r"
        end
        lines[#lines + 1] = text
    end
    if #lines == 0 then
        lines[1] = "No stats to map."
    end
    for _, note in ipairs(result.notes or {}) do
        lines[#lines + 1] = "|cffffd100Note:|r " .. note
    end
    return table.concat(lines, "\n")
end

-- Rebuilds the SQL of the shown entry with the panel's options.
function RB.BuildSql()
    local d = RB.details
    if not panel:IsShown() or not d then
        return
    end
    subtitle:SetText(RB.KindLabel(d.kind) .. ": " .. (d.name or "") .. " (" .. RB.PrimaryId(d) .. ")")
    if not BUILD_KINDS[d.kind] then
        status:SetText("|cffff4040SQL is built for items, sets and creatures.|r")
        mapping:SetText("")
        sqlBox:SetText("")
        return
    end
    local result, err = RetailBuildSql(d.kind, d.id, Options())
    if not result or result.status ~= "ok" then
        status:SetText("|cffff4040" .. tostring(err or (result and result.status)) .. "|r")
        mapping:SetText("")
        sqlBox:SetText("")
        return
    end
    RB.buildResult = result
    status:SetText(string.format("Entries %d to %d, base %d%s, item level %d. Next free: %d.", result.firstEntry, result.nextEntry - 1,
        result.base, baseEntry:GetText() == "" and " (suggested)" or "", result.itemLevel, result.nextEntry))
    mapping:SetText(MappingText(result))
    sqlBox:SetText(result.sql)
    sqlBox:SetCursorPosition(0)
end

local function ExportName(d)
    return d.kind .. "-" .. RB.PrimaryId(d)
end

local buildButton = RB.CreateButton(panel, "Build", 100, function() RB.BuildSql() end)
buildButton:SetPoint("BOTTOMLEFT", 24, 20)
local copyButton = RB.CreateButton(panel, "Copy SQL", 100, function()
    RB.Copy(sqlBox:GetText(), "SQL")
end)
copyButton:SetPoint("LEFT", buildButton, "RIGHT", 8, 0)
local exportButton = RB.CreateButton(panel, "Export SQL", 100, function()
    local d = RB.details
    if not d or sqlBox:GetText() == "" then
        return
    end
    local path, err = RetailExportSql(ExportName(d), sqlBox:GetText())
    RB.Print(path and ("wrote " .. path) or ("could not export: " .. tostring(err)))
end)
exportButton:SetPoint("LEFT", copyButton, "RIGHT", 8, 0)

panel:SetScript("OnShow", function() RB.BuildSql() end)

function RB.ToggleBuild()
    if panel:IsShown() then
        panel:Hide()
    else
        panel:Show()
    end
end

-- A new entry in the details rebuilds the open panel.
local showDetails = RB.ShowDetails
function RB.ShowDetails(...)
    showDetails(...)
    RB.BuildSql()
end
