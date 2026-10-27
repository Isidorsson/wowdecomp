-- The Build SQL drawer tab: world DB SQL for the focused entry, the checked rows or the page (RetailBuildSql per
-- entry, each starting at the entry the previous one left free). Every statement copies a stock row (the base entry)
-- and overrides only the columns it names; an empty start entry asks the server for a free run (FreeEntry.lua). Retail stats are
-- computed at the item level and mapped to 3.3.5 stats as listed. The tab uses the drawer's tall step.

local RB = RetailBrowser

local OPTIONS_WIDTH = 270
local MAPPING_WIDTH = 300
local DEFAULT_ITEM_LEVEL = 200
local DEFAULT_START_ENTRY = 9000000
local COMBOS = { { 1, "Strength" }, { 2, "Agility" }, { 3, "Intellect" } }
-- A multi-line edit box lays its text out in time that grows with the square of its length (a 425 KB set froze the
-- client for 40 s), so the box shows the first row's statements and Copy / Export use the whole SQL.
local PREVIEW_BYTES = 4096
local HUGE_SQL_BYTES = 65536
local CONFIRM_SECONDS = 5

local panel = CreateFrame("Frame", "RetailBrowserBuildFrame")
RB.buildPanel = panel

-- Rebuilds after a pause, so typing, ticking options or stepping through rows builds once at the end.
function RB.ScheduleBuild()
    if panel:IsVisible() then
        RB.buildAt = GetTime() + RB.SEARCH_DELAY
    end
end

local options = CreateFrame("Frame", nil, panel)
options:SetPoint("TOPLEFT")
options:SetPoint("BOTTOMLEFT")
options:SetWidth(OPTIONS_WIDTH)

local function Heading(parent, text, x, y)
    local heading = RB.CreateText(parent, "RetailBrowserHeadFont")
    heading:SetPoint("TOPLEFT", x, y)
    heading:SetText(text)
    return heading
end
Heading(options, "Options", 2, -1)

-- Scope: what the SQL is built for.
RB.buildScope = "focused"
local scopeButtons = {}
local SCOPES = { { "focused", "Focused row" }, { "checked", "Checked" }, { "page", "Whole page" } }
for i, scope in ipairs(SCOPES) do
    local button = RB.CreateButton(options, scope[2], 86, 18, function() RB.SetBuildScope(scope[1]) end)
    button:SetPoint("TOPLEFT", 2 + (i - 1) * 89, -14)
    button.scope = scope[1]
    RB.AttachTip(button, "scope_" .. scope[1])
    scopeButtons[i] = button
end

local boxes = {}
local function Labelled(name, label, width, x, y, tipKey)
    local text = RB.CreateText(options, "RetailBrowserLabelFont")
    text:SetPoint("TOPLEFT", x, y - 2)
    text:SetWidth(78)
    text:SetText(label)
    local box = RB.CreateCellBox(options, "RetailBrowserBuild" .. name)
    RB.AttachTip(box, tipKey)
    box:SetPoint("TOPLEFT", x + 80, y)
    box:SetWidth(width)
    box:SetScript("OnTextChanged", function(self)
        RB.RefreshHint(self)
        RB.ScheduleBuild()
    end)
    box:SetScript("OnEnterPressed", function(self) self:ClearFocus() RB.FocusGrid() end)
    box:SetScript("OnEscapePressed", function(self) self:ClearFocus() RB.FocusGrid() end)
    box:HookScript("OnEditFocusGained", function() RB.SetKeyOwner("build") end)
    box:HookScript("OnEditFocusLost", function() RB.KeyOwnerLost() end)
    boxes[#boxes + 1] = box
    local index = #boxes
    box:SetScript("OnTabPressed", function()
        local nextBox = boxes[index % #boxes + 1]
        nextBox:SetFocus()
    end)
    return box
end

local ROW = 19
local function RowY(n) return -38 - (n - 1) * ROW end
local itemLevel = Labelled("ItemLevel", "Item level", 60, 2, RowY(1), "opt_itemLevel")
local startEntry = Labelled("StartEntry", "Start entry", 90, 2, RowY(2), "opt_startEntry")
local baseEntry = Labelled("BaseEntry", "Base entry", 90, 2, RowY(3), "opt_baseEntry")
local requiredLevel = Labelled("RequiredLevel", "Required level", 60, 2, RowY(4), "opt_requiredLevel")
local nameBox = Labelled("Name", "Name", 180, 2, RowY(5), "opt_name")
local subnameBox = Labelled("Subname", "Subname", 140, 2, RowY(6), "opt_subname")
local scale = Labelled("Scale", "Scale", 60, 2, RowY(7), "opt_scale")
RB.SetHint(startEntry, "free from server")
RB.SetHint(requiredLevel, "0 (any level)")
RB.SetHint(scale, "base's")

-- Empty override fields show the retail value they stand for (the SQL uses it); typed text replaces it.
local function RetailHints(entries, result)
    local d = entries[1]
    local many = #entries > 1
    RB.SetHint(itemLevel, many and "each own" or tostring(d and (d.sparseItemLevel or d.itemLevel) or DEFAULT_ITEM_LEVEL))
    RB.SetHint(nameBox, many and ("retail names (" .. #entries .. " rows)") or (d and d.name or "retail name"))
    RB.SetHint(subnameBox, d and RB.BUILD_TABLES[d.kind] == "creature" and (many and "retail titles" or (d.title ~= "" and d.title or "none")) or "creatures only")
    RB.SetHint(baseEntry, result and result.base and ("suggested " .. result.base) or "suggested")
end

local comboLabel = RB.CreateText(options, "RetailBrowserLabelFont")
comboLabel:SetPoint("TOPLEFT", 2, RowY(8) - 2)
comboLabel:SetText("Hybrid primary")
local combo = RB.CreateChoiceButton(options, 140, nil)
combo:SetLabel("Any: first named")
combo:SetPoint("TOPLEFT", 82, RowY(8))
RB.AttachTip(combo, "opt_combo")
combo:SetScript("OnClick", function(self)
    RB.ShowChoiceMenu(self, COMBOS, self.value, nil, function(value, text)
        self.value = value
        self:SetLabel(value and text or "Any: first named")
        RB.ScheduleBuild()
    end)
end)

local function Check(name, label, y, checked, tipKey)
    local check = CreateFrame("CheckButton", "RetailBrowserBuild" .. name, options, "UICheckButtonTemplate")
    check:SetSize(18, 18)
    check:SetPoint("TOPLEFT", 0, y)
    local text = _G[check:GetName() .. "Text"]
    text:SetFontObject(RetailBrowserSmallFont)
    text:SetText(label)
    check:SetChecked(checked)
    check:SetScript("OnClick", function() RB.ScheduleBuild() end)
    RB.AttachTip(check, tipKey)
    return check
end

local retailStamina = Check("RetailStamina", "Retail stamina multiplier", RowY(9), false, "opt_retailStamina")
local allVariants = Check("AllVariants", "Every set variant", RowY(10), false, "opt_allVariants")

-- What a creature, model or mount with several displays builds; a click steps to the next way.
local DISPLAY_MODES = { { 1, "One creature each" }, { 0, "Picked only" }, { 2, "As modelid1-4 variants" } }
local displaysLabel = RB.CreateText(options, "RetailBrowserLabelFont")
displaysLabel:SetPoint("TOPLEFT", 2, RowY(11) - 2)
displaysLabel:SetText("Displays")
local displays
displays = RB.CreateButton(options, DISPLAY_MODES[1][2], 160, 18, function()
    displays.mode = displays.mode % #DISPLAY_MODES + 1
    displays:SetLabel(DISPLAY_MODES[displays.mode][2])
    RB.ScheduleBuild()
end)
displays.mode = 1
displays:SetPoint("TOPLEFT", 82, RowY(11))
RB.AttachTip(displays, "opt_displays")
local retailArmour = Check("RetailArmour", "Retail armour/damage", RowY(12), true, "opt_retailArmour")
local CHECK_BOTTOM = RowY(13) - 4

local status = RB.CreateText(options, "RetailBrowserSmallFont")
status:SetPoint("TOPLEFT", 2, CHECK_BOTTOM)
status:SetWidth(OPTIONS_WIDTH - 4)
status:SetHeight(40)
status:SetJustifyV("TOP")

-- A scrolling multi-line edit box: select text in it, or copy it with the buttons.
local function ScrollText(name, parent, x, width, bottom)
    local back = RB.CreatePanel(parent, "well")
    back:SetPoint("TOPLEFT", x, -14)
    back:SetPoint("BOTTOM", parent, "BOTTOM", 0, bottom)
    back:SetWidth(width)
    local scroll = CreateFrame("ScrollFrame", "RetailBrowserBuild" .. name, back, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", 4, -4)
    scroll:SetPoint("BOTTOMRIGHT", -24, 4)
    local box = CreateFrame("EditBox", nil, scroll)
    box:SetMultiLine(true)
    box:SetMaxLetters(0)
    box:SetAutoFocus(false)
    box:SetFontObject(GameFontHighlightSmall)
    box:SetWidth(math.max(100, width - 30))
    box:SetScript("OnEscapePressed", function(self) self:ClearFocus() RB.FocusGrid() end)
    box:SetScript("OnEditFocusGained", function() RB.SetKeyOwner("build") end)
    box:SetScript("OnEditFocusLost", function() RB.KeyOwnerLost() end)
    box:SetScript("OnCursorChanged", ScrollingEdit_OnCursorChanged)
    box:SetScript("OnUpdate", function(self, elapsed) ScrollingEdit_OnUpdate(self, elapsed, scroll) end)
    scroll:SetScrollChild(box)
    return box
end

Heading(panel, "Stat mapping", OPTIONS_WIDTH + 8, -1)
local mapping = ScrollText("Mapping", panel, OPTIONS_WIDTH + 8, MAPPING_WIDTH, 2)
local sqlHeading = Heading(panel, "SQL preview", OPTIONS_WIDTH + MAPPING_WIDTH + 16, -1)
local sqlBox = ScrollText("Sql", panel, OPTIONS_WIDTH + MAPPING_WIDTH + 16, 1, 24)
RB.AttachTip(mapping, "sql_mapping")
RB.AttachTip(sqlBox, "sql_preview")
local sqlBack = sqlBox:GetParent():GetParent()
sqlBack:SetPoint("RIGHT", panel, "RIGHT", 0, 0)
panel:SetScript("OnSizeChanged", function(self, width)
    sqlBox:SetWidth(math.max(100, width - OPTIONS_WIDTH - MAPPING_WIDTH - 16 - 30))
    if RB.SpreadSqlButtons then RB.SpreadSqlButtons(width) end
end)

-- Whether the box holds the whole SQL (then Copy and Export take the box, edits included).
local sqlFull = false
local confirmFullAt

-- The first row's statements (at most PREVIEW_BYTES), then a comment line saying what is not shown.
local function PreviewOf(sql)
    local cut = sql:find("\n\n", 1, true)
    local first = cut and sql:sub(1, cut + 1) or sql
    if #first > PREVIEW_BYTES then
        first = sql:sub(1, PREVIEW_BYTES) .. "\n"
    end
    if #first >= #sql then
        return sql, true
    end
    local _, rows = sql:gsub("\n\n", "")
    return first .. string.format("-- %d KB more not shown (%d rows in all): Copy SQL and Export SQL use all of it, Edit full SQL shows it.",
        math.ceil((#sql - #first) / 1024), math.max(rows, 1)), false
end

local function ShowSql(sql)
    local text, whole = PreviewOf(sql)
    sqlFull = whole
    confirmFullAt = nil
    sqlBox:SetText(text)
    sqlBox:SetCursorPosition(0)
    sqlHeading:SetText("SQL preview " .. RB.Colored(string.format("%s shown of %.1f KB", whole and "all" or "4 KB", #sql / 1024), RB.THEME.label))
end

local function FullSql()
    if sqlFull then
        return sqlBox:GetText()
    end
    return RB.buildResult and RB.buildResult.sql or ""
end

-- The RetailBuildSql options.
local function Options(d, start)
    local options = {
        itemLevel = tonumber(itemLevel:GetText()) or (d.kind == "item" and (d.sparseItemLevel or d.itemLevel)) or nil,
        startEntry = start,
        baseEntry = tonumber(baseEntry:GetText()),
        requiredLevel = tonumber(requiredLevel:GetText()),
        scale = tonumber(scale:GetText()),
        combo = combo.value,
        retailStamina = retailStamina:GetChecked() and 1 or 0,
        allVariants = allVariants:GetChecked() and 1 or 0,
        displays = DISPLAY_MODES[displays.mode][1],
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

-- The base item's armour, damage and delay from its tooltip; nil until the client has the item (it asks the server, so
-- the next build shows it).
local scan = CreateFrame("GameTooltip", "RetailBrowserBuildScan", nil, "GameTooltipTemplate")

local function BaseCombat(entry)
    scan:SetOwner(UIParent, "ANCHOR_NONE")
    scan:SetHyperlink("item:" .. entry)
    if not GetItemInfo(entry) then
        scan:Hide()
        return nil
    end
    local base = {}
    for i = 1, scan:NumLines() do
        for _, side in ipairs({ "Left", "Right" }) do
            local text = _G["RetailBrowserBuildScanText" .. side .. i]:GetText() or ""
            base.armor = base.armor or tonumber(text:match("^(%d+) Armor$"))
            local min, max = text:match("^(%d+) %- (%d+) ")
            if min and not base.dmg_min1 then
                base.dmg_min1, base.dmg_max1 = tonumber(min), tonumber(max)
            end
            local speed = tonumber(text:match("^Speed (%d+%.%d+)"))
            base.delay = base.delay or (speed and math.floor(speed * 1000 + 0.5))
        end
    end
    scan:Hide()
    return base
end

-- The base's value beside a retail armour or damage line (one item built), so the two can be compared.
local COMBAT_COLUMNS = { armor = true, dmg_min1 = true, dmg_max1 = true, delay = true }
local function CombatText(line, base)
    if not base or not COMBAT_COLUMNS[line.classic] then
        return ""
    end
    if base == true then
        return "  " .. RB.Colored("(base: not known yet, Build again)", RB.THEME.label)
    end
    return "  " .. RB.Colored("(base " .. tostring(base[line.classic] or (line.classic == "armor" and 0) or "?") .. ")", RB.THEME.combat)
end

-- The stat lines up to PREVIEW_BYTES (a set of every variant has hundreds), then the notes. baseEntry: the one
-- item's base, whose armour and damage the combat lines are shown beside.
local function MappingText(lines, notes, baseEntry)
    local out, bytes = {}, 0
    local base = baseEntry and baseEntry > 0 and (BaseCombat(baseEntry) or true)
    for i, line in ipairs(lines) do
        local text = string.format("%s: %s %d -> %s %d", line.piece, line.retail, line.retailValue, line.classic, line.value) .. CombatText(line, base)
        if line.note ~= "" then
            text = text .. "  " .. RB.Colored("(" .. line.note .. ")", RB.THEME.label)
        end
        bytes = bytes + #text + 1
        if bytes > PREVIEW_BYTES then
            out[#out + 1] = RB.Colored(string.format("... %d more stat lines (the SQL has them all).", #lines - i + 1), RB.THEME.label)
            break
        end
        out[#out + 1] = text
    end
    if #out == 0 then
        out[1] = "No stats to map."
    end
    for _, note in ipairs(notes) do
        out[#out + 1] = RB.Colored("Note:", RB.THEME.title) .. " " .. note
    end
    return table.concat(out, "\n")
end

-- The entries the scope names, of the kinds SQL is built for, at most RB.BULK_LIMIT.
local function ScopeEntries()
    local list = {}
    if RB.buildScope == "focused" then
        list[1] = RB.details
    else
        for _, row in ipairs(RB.buildScope == "checked" and RB.CheckedRows() or RB.rows) do
            list[#list + 1] = row.d or RB.DetailsOf(row.kind, row.id)
        end
    end
    local entries = {}
    for _, d in ipairs(list) do
        if d and RB.BUILD_TABLES[d.kind] and #entries < RB.BULK_LIMIT then
            entries[#entries + 1] = d
        end
    end
    return entries, #list
end

local function BuildAll(entries, start)
    local all = { sql = {}, lines = {}, notes = {}, firstEntry = start, tables = RB.EntryTables(entries) }
    for _, d in ipairs(entries) do
        local result, err = RetailBuildSql(d.kind, d.id, Options(d, start))
        if not result or result.status ~= "ok" then
            return nil, (d.name or "") .. ": " .. tostring(err or (result and result.status))
        end
        all.sql[#all.sql + 1] = result.sql:sub(-1) == "\n" and result.sql or (result.sql .. "\n")
        for _, line in ipairs(result.lines or {}) do all.lines[#all.lines + 1] = line end
        for _, note in ipairs(result.notes or {}) do all.notes[#all.notes + 1] = note end
        all.base, all.itemLevel, all.nextEntry = all.base or result.base, result.itemLevel, result.nextEntry
        start = result.nextEntry
    end
    all.sql = table.concat(all.sql, "\n")
    return all
end

-- An empty Start entry builds once to learn how many entries the scope takes, then again at the server's first free
-- run of that length. While the server is asking (or silent) the first build's fallback start stands.
local function BuildAtFree(entries)
    local typed = tonumber(startEntry:GetText())
    local first, err = BuildAll(entries, typed or DEFAULT_START_ENTRY)
    if typed or not first then
        return first, err
    end
    local free, hint = RB.FreeStart(first, DEFAULT_START_ENTRY)
    RB.SetHint(startEntry, hint)
    if free and free ~= first.firstEntry then return BuildAll(entries, free) end
    return first
end

-- Build now reads "Building..." while a build waits (the pause after a change, or the server's free entry), then
-- "Built" for a moment after one succeeds.
local BUILT_SECONDS = 1.5
local builtAt
local buildButton

local function BuildLabel()
    if RB.buildAt then
        return "Building" .. string.rep(".", math.floor(GetTime() * 3) % 3 + 1)
    end
    if builtAt and GetTime() - builtAt < BUILT_SECONDS then
        return RB.Colored("Built", RB.THEME.good)
    end
    return "Build now"
end

local function Fail(text)
    builtAt = nil
    status:SetText(RB.Colored(text, RB.THEME.bad))
    mapping:SetText("")
    RB.buildResult = nil
    ShowSql("")
end

-- " (5 displays)" for a lone creature, model or mount with several, so a build of one of them says why it wrote more rows.
local function DisplaysText(entries)
    local d = entries[1]
    if #entries ~= 1 or RB.BUILD_TABLES[d.kind] ~= "creature" then
        return ""
    end
    local count = d.displayCount or #(d.displays or {})
    return count > 1 and string.format(" (%d displays)", count) or ""
end

-- Rebuilds the SQL of the scope's entries with the tab's options.
function RB.BuildSql()
    if not panel:IsVisible() then
        return
    end
    local entries, asked = ScopeEntries()
    if #entries == 0 then
        Fail(asked == 0 and "Nothing to build: focus a row or check some." or "SQL is built for items, sets, creatures, models and mounts.")
        return
    end
    local result, err = BuildAtFree(entries)
    RetailHints(entries, result)
    if not result then
        Fail(err)
        return
    end
    RB.buildResult = result
    local skipped = asked - #entries
    local rows = result.nextEntry - result.firstEntry
    -- The time shows a rebuild happened even when its SQL is the same as before.
    status:SetText(RB.Colored("Built " .. date("%H:%M:%S") .. ".", RB.THEME.good) .. string.format(" Entries %d to %d: %d %s from %d %s%s, base %d%s, item level %d. Next free: %d.%s",
        result.firstEntry, result.nextEntry - 1, rows, rows == 1 and "row" or "rows", #entries, #entries == 1 and "entry" or "entries", DisplaysText(entries),
        result.base, baseEntry:GetText() == "" and " (suggested)" or "", result.itemLevel,
        result.nextEntry, skipped > 0 and " " .. RB.Colored(string.format("%d skipped (kind, or over %d).", skipped, RB.BULK_LIMIT), RB.THEME.bad) or ""))
    local lone = #entries == 1 and entries[1].kind == "item"
    mapping:SetText(MappingText(result.lines, result.notes, lone and result.base or nil))
    ShowSql(result.sql)
    builtAt = GetTime()
end

panel:SetScript("OnUpdate", function()
    if RB.buildAt and GetTime() >= RB.buildAt then
        RB.buildAt = nil
        RB.BuildSql()
    end
    local label = BuildLabel()
    if buildButton and label ~= buildButton.text then
        buildButton.text = label
        buildButton:SetLabel(label)
    end
end)

-- Puts every statement in the box; a huge SQL asks for a second click first.
local function EditFullSql()
    local sql = RB.buildResult and RB.buildResult.sql
    if not sql or sqlFull then
        return
    end
    if #sql > HUGE_SQL_BYTES and not (confirmFullAt and GetTime() - confirmFullAt < CONFIRM_SECONDS) then
        confirmFullAt = GetTime()
        status:SetText(RB.Colored(string.format("The SQL is %d KB: showing it all can freeze the client for seconds or minutes. Click Edit full SQL again to go ahead.",
            math.ceil(#sql / 1024)), RB.THEME.title))
        return
    end
    sqlBox:SetText(sql)
    sqlBox:SetCursorPosition(0)
    sqlFull = true
    confirmFullAt = nil
end

local function ExportName()
    local d = RB.details
    if RB.buildScope ~= "focused" or not d then
        return "build-" .. RB.state.kind
    end
    return d.kind .. "-" .. RB.PrimaryId(d)
end

function RB.CopyBuildSql()
    local sql = FullSql()
    RB.Copy(sql, "SQL")
    if sql ~= "" then RB.ReserveBuilt(RB.buildResult) end
end

function RB.ExportBuildSql()
    local sql = FullSql()
    if sql == "" then
        return
    end
    local path, err = RetailExportSql(ExportName(), sql)
    RB.Print(path and ("wrote " .. path) or ("could not export: " .. tostring(err)))
    if path then RB.ReserveBuilt(RB.buildResult) end
end

-- The SQL buttons share the SQL column's width.
local sqlButtons = {}
local KEY_HINT_WIDTH = 140
local function SqlButton(text, hotkey, onClick, tipKey)
    local button = RB.CreateButton(panel, text, 90, 20, onClick)
    RB.AttachTip(button, tipKey)
    button.text, button.hotkey = text, hotkey
    sqlButtons[#sqlButtons + 1] = button
    return button
end
buildButton = SqlButton("Build now", nil, function() RB.buildAt = nil RB.BuildSql() end, "sql_build")
SqlButton("Copy SQL", "Ctrl+Enter", function() RB.CopyBuildSql() end, "sql_copy")
SqlButton("Export SQL", "Ctrl+S", function() RB.ExportBuildSql() end, "sql_export")
SqlButton("Edit full SQL", nil, EditFullSql, "sql_edit")

local function SpreadSqlButtons(width)
    local left = OPTIONS_WIDTH + MAPPING_WIDTH + 16
    local each = (width - left - 4 * (#sqlButtons - 1)) / #sqlButtons
    for i, button in ipairs(sqlButtons) do
        button:SetWidth(each)
        button:SetLabel(button.hotkey and each >= KEY_HINT_WIDTH and (button.text .. " " .. RB.Key(button.hotkey)) or button.text)
        button:ClearAllPoints()
        button:SetPoint("BOTTOMLEFT", panel, "BOTTOMLEFT", left + (i - 1) * (each + 4), 0)
    end
end

RB.SpreadSqlButtons = SpreadSqlButtons

function RB.SetBuildScope(scope)
    RB.buildScope = scope
    for _, button in ipairs(scopeButtons) do
        button:SetOn(button.scope == scope)
    end
    RB.ScheduleBuild()
end

function RB.OnChecksChanged(count)
    scopeButtons[2]:SetLabel("Checked " .. count)
    if RB.buildScope ~= "focused" then
        RB.ScheduleBuild()
    end
end

-- Opens the tab for a scope (focused, checked, page).
function RB.ShowBuild(scope)
    RB.SetBuildScope(scope or RB.buildScope)
    local d = RB.details
    local first = RB.CheckedRows()[1]
    if (not d or not RB.BUILD_TABLES[d.kind]) and first then
        RB.ShowDetails(first.kind, first.id, false)
    end
    RB.ShowDrawerTab("build")
end

panel:SetScript("OnShow", function()
    RB.ForgetFreeEntries()
    RB.buildAt = nil
    RB.BuildSql()
end)

RB.AddDrawerTab("build", "Build SQL", panel, function() RB.ScheduleBuild() end,
    { hotkey = "B", order = 5, wide = true, kinds = RB.BUILD_TABLES })
RB.SetBuildScope("focused")
