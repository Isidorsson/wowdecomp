-- The Build SQL page (RB.ShowPage("build")): a header with "< <Kind>" back (Esc), the title, the scope switch
-- (Focused row / Checked N / Whole page N), the entry range and Apply to world DB; a hint when rows are checked but
-- only the focused one is built; the yellow confirm bar Apply opens;
-- then three columns: the queue (BuildQueue.lua), the settings (BuildOptions.lua) and the output (BuildOutput.lua).
-- While it is open the page owns the keys: Esc goes back, Ctrl+Enter copies the SQL, Ctrl+S exports it.

local RB = RetailBrowser
local T = RB.THEME

local PAD = 12
local HEADER_HEIGHT = 44
local CONFIRM_HEIGHT = 32
local HINT_HEIGHT = 24
local QUEUE_WIDTH = 300
local OPTIONS_WIDTH = 380
local SCOPE_WIDTH = 108
local SCOPES = { "focused", "checked", "page" }

local page = RB.CreatePanel(RB.buildCol, "panel")
page:SetAllPoints()
RB.buildPage = page

local function Line(parent, vertical)
    local line = RB.CreateFill(parent, "BORDER", T.borderDim)
    line:ClearAllPoints()
    if vertical then line:SetWidth(1) else line:SetHeight(1) end
    return line
end

-- Header.
local header = CreateFrame("Frame", nil, page)
header:SetPoint("TOPLEFT", RB.EDGE_INSET, -RB.EDGE_INSET)
header:SetPoint("TOPRIGHT", -RB.EDGE_INSET, -RB.EDGE_INSET)
header:SetHeight(HEADER_HEIGHT)
local headerLine = Line(header)
headerLine:SetPoint("BOTTOMLEFT")
headerLine:SetPoint("BOTTOMRIGHT")

local back = RB.CreateButton(header, "< Creatures", 104, 22, function() RB.ShowPage("browse") RB.FocusPage() end)
back:SetPoint("LEFT", PAD, 0)
RB.AttachTip(back, "build_back")
local title = RB.CreateText(header, "GameFontNormalLarge")
title:SetPoint("LEFT", back, "RIGHT", 14, 0)
title:SetText("Build SQL")
RB.SetColor(title, T.text)

local scopeButtons = {}
for i, scope in ipairs(SCOPES) do
    local button = RB.CreateButton(header, "", SCOPE_WIDTH, 24, function() RB.ChooseBuildScope(scope) end)
    button:SetPoint("LEFT", i == 1 and title or scopeButtons[i - 1], "RIGHT", i == 1 and 18 or 2, 0)
    button.scope = scope
    RB.AttachTip(button, "scope_" .. scope)
    scopeButtons[i] = button
end

local apply = RB.CreateButton(header, "Apply to world DB", 150, 26, function() RB.AskApply() end)
apply:SetPoint("RIGHT", -PAD, 0)
apply:SetPrimary(true)
RB.AttachTip(apply, "sql_apply")
local range = RB.CreateText(header, "RetailBrowserValueFont", "RIGHT")
range:SetPoint("RIGHT", apply, "LEFT", -14, 0)

-- The Apply button through a row-by-row apply: "Writing..." while it runs, "Retry N rows" after a failed row.
function RB.SetApplyButton(label, enabled)
    apply:SetLabel(label)
    apply:SetEnabled(enabled)
end

-- Under the header while rows are checked but only the focused row is built: "50 rows checked · Build all 50".
local hint = CreateFrame("Frame", nil, page)
hint:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 0, 0)
hint:SetPoint("TOPRIGHT", header, "BOTTOMRIGHT", 0, 0)
hint:SetHeight(HINT_HEIGHT)
RB.CreateFill(hint, "BACKGROUND", T.band)
hint.text = RB.CreateText(hint, "RetailBrowserButtonFont")
hint.text:SetPoint("LEFT", PAD, 0)
RB.SetColor(hint.text, T.pending)
hint.all = RB.CreateButton(hint, "", 110, 18, function() RB.ChooseBuildScope("checked") end)
hint.all:SetPoint("LEFT", hint.text, "RIGHT", 10, 0)
RB.AttachTip(hint.all, "scope_checked")
hint:Hide()

local LayOutColumns

-- labels: { focused, checked, page } texts; scope: the one that is on; rangeText: "9000000–9000002" or nil;
-- checked: how many rows are checked.
function RB.RefreshBuildHeader(labels, scope, rangeText, checked)
    for _, button in ipairs(scopeButtons) do
        button:SetLabel(labels[button.scope])
        button:SetOn(button.scope == scope)
    end
    range:SetText(rangeText and (RB.Colored("Entries ", T.label) .. rangeText) or "")
    apply:SetEnabled(rangeText ~= nil)
    local list = RB.state.source == "search" and RB.KindLabel(RB.state.kind) or RB.KindLabel(RB.state.source)
    back:SetLabel("< " .. list:gsub("^%l", string.upper))
    local showHint = scope == "focused" and (checked or 0) > 0
    if showHint then
        hint.text:SetText(checked .. (checked == 1 and " row checked" or " rows checked") .. " · only the focused row is built")
        hint.all:SetLabel(checked == 1 and "Build it" or ("Build all " .. checked))
    end
    if hint:IsShown() ~= (showHint and true or false) then
        RB.Shown(hint, showHint)
        LayOutColumns()
    end
end

-- The confirm bar: Apply asks here before anything is written.
local confirm = CreateFrame("Frame", nil, page)
confirm:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 0, 0)
confirm:SetPoint("TOPRIGHT", header, "BOTTOMRIGHT", 0, 0)
confirm:SetHeight(CONFIRM_HEIGHT)
confirm:SetBackdrop(RB.FLAT_BACKDROP)
RB.SetBack(confirm, T.confirm)
RB.SetBorder(confirm, T.confirmBorder)
confirm:Hide()
local go = RB.CreateButton(confirm, "Apply", 120, 22, function() RB.ApplyBuildSql() end)
go:SetPoint("RIGHT", -PAD, 0)
go:SetVariant("confirm")
local cancel = RB.CreateButton(confirm, "Cancel", 70, 22, function() RB.HideApplyConfirm() end)
cancel:SetPoint("RIGHT", go, "LEFT", -6, 0)
-- The count first and large, so a build of 50 rows never reads like one.
local count = RB.CreateText(confirm, "GameFontNormalLarge")
count:SetPoint("LEFT", PAD, 0)
RB.SetColor(count, T.confirmButton)
local question = RB.CreateText(confirm, "RetailBrowserButtonFont")
question:SetPoint("LEFT", count, "RIGHT", 10, 0)
question:SetPoint("RIGHT", cancel, "LEFT", -10, 0)
RB.SetColor(question, T.confirmText)

-- The three columns, under the header and the confirm bar when it shows.
local function Column(width)
    local column = CreateFrame("Frame", nil, page)
    if width then column:SetWidth(width) end
    column:SetPoint("BOTTOM", 0, RB.EDGE_INSET)
    return column
end
RB.buildQueueCol, RB.buildOptionsCol, RB.buildOutputCol = Column(QUEUE_WIDTH), Column(OPTIONS_WIDTH), Column()
local queueLine, optionsLine = Line(page, true), Line(page, true)

function LayOutColumns()
    local above = hint:IsShown() and hint or header
    confirm:SetPoint("TOPLEFT", above, "BOTTOMLEFT", 0, 0)
    confirm:SetPoint("TOPRIGHT", above, "BOTTOMRIGHT", 0, 0)
    local top = RB.EDGE_INSET + HEADER_HEIGHT + (confirm:IsShown() and CONFIRM_HEIGHT or 0) + (hint:IsShown() and HINT_HEIGHT or 0)
    RB.buildQueueCol:SetPoint("TOPLEFT", RB.EDGE_INSET, -top)
    RB.buildOptionsCol:SetPoint("TOPLEFT", RB.buildQueueCol, "TOPRIGHT", 1, 0)
    RB.buildOutputCol:SetPoint("TOPLEFT", RB.buildOptionsCol, "TOPRIGHT", 1, 0)
    RB.buildOutputCol:SetPoint("RIGHT", -RB.EDGE_INSET, 0)
    for _, pair in ipairs({ { queueLine, RB.buildQueueCol }, { optionsLine, RB.buildOptionsCol } }) do
        pair[1]:SetPoint("TOPLEFT", pair[2], "TOPRIGHT", 0, 0)
        pair[1]:SetPoint("BOTTOMLEFT", pair[2], "BOTTOMRIGHT", 0, 0)
    end
end
LayOutColumns()

function RB.ShowApplyConfirm(text, rows)
    count:SetText(rows .. (rows == 1 and " row" or " rows"))
    question:SetText(text)
    go:SetLabel("Apply " .. rows .. (rows == 1 and " row" or " rows"))
    confirm:Show()
    LayOutColumns()
end

function RB.HideApplyConfirm()
    confirm:Hide()
    LayOutColumns()
end

-- Keys: the page has them while it is open and no box has focus.
function RB.FocusPage()
    if RB.page ~= "build" then
        RB.FocusGrid()
        return
    end
    local focus = GetCurrentKeyBoardFocus()
    if focus then
        focus:ClearFocus()
    end
    page:EnableKeyboard(true)
    RB.SetKeyOwner("build")
end

page:SetScript("OnKeyDown", function(_, key)
    if key == "ESCAPE" then
        if confirm:IsShown() then
            RB.HideApplyConfirm()
        else
            RB.ShowPage("browse")
            RB.FocusPage()
        end
    elseif IsControlKeyDown() and key == "ENTER" then
        RB.CopyBuildSql()
    elseif IsControlKeyDown() and key == "S" then
        RB.ExportBuildSql()
    end
end)
page:SetScript("OnShow", function()
    RB.FocusPage()
    if RB.OnBuildPageShown then RB.OnBuildPageShown() end
end)
page:SetScript("OnHide", function(self)
    self:EnableKeyboard(false)
    RB.HideApplyConfirm()
end)
