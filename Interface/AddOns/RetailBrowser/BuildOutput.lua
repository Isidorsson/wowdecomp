-- The Build SQL page's output: tabs SQL (the selected row's statements, at most PREVIEW_BYTES, then a comment on what is
-- not shown; Wrap off keeps each line whole and scrolls sideways, saved per character), Stat mapping (each retail
-- stat and what it became) and Changes vs base (the columns the row overrides;
-- the base row's own values are in the world DB, not the client). The footer has the build's status, then Copy
-- (Ctrl+Enter), Export (Ctrl+S) and Edit full SQL.

local RB = RetailBrowser
local T = RB.THEME

local PAD = 12
local TAB_HEIGHT = 26
local FOOTER_HEIGHT = 64
local BUTTONS_HEIGHT = 32
-- A multi-line edit box lays its text out in time that grows with the square of its length (a 425 KB set froze the
-- client for 40 s), so the box shows the selected row's statements and Copy / Export use the whole SQL.
local PREVIEW_BYTES = 4096
local HUGE_SQL_BYTES = 65536
local CONFIRM_SECONDS = 5
local SQL_PADDING = 24
local HBAR_HEIGHT = 8
local HSCROLL_STEP = 60
local NOTHING_TO_MAP = "Stat mapping applies to items and sets. This build has only creatures, so there is nothing to map."

local col = RB.buildOutputCol

-- Tabs.
local tabs, tabOrder, current = {}, { "sql", "mapping", "changes" }, "sql"
local TAB_LABELS = { sql = "SQL", mapping = "Stat mapping", changes = "Changes vs base" }
local strip = CreateFrame("Frame", nil, col)
strip:SetPoint("TOPLEFT", PAD, -6)
strip:SetPoint("TOPRIGHT", -PAD, -6)
strip:SetHeight(TAB_HEIGHT)
local stripLine = RB.CreateFill(strip, "BORDER", T.borderDim)
stripLine:ClearAllPoints()
stripLine:SetPoint("BOTTOMLEFT")
stripLine:SetPoint("BOTTOMRIGHT")
stripLine:SetHeight(1)
-- Wrap on/off for the SQL tab, at the strip's right end; the choice is read when the page first shows.
local wrap
local wrapButton = RB.CreateButton(strip, "Wrap off", 64, 18, function() RB.ToggleSqlWrap() end)
wrapButton:SetPoint("RIGHT", 0, 2)
RB.AttachTip(wrapButton, "build_wrap")
local selection = RB.CreateText(strip, "RetailBrowserLabelFont", "RIGHT")
selection:SetPoint("RIGHT", wrapButton, "LEFT", -8, -1)

local body = CreateFrame("Frame", nil, col)
body:SetPoint("TOPLEFT", PAD, -6 - TAB_HEIGHT - 6)
body:SetPoint("BOTTOMRIGHT", -PAD, FOOTER_HEIGHT)

local function PaintTabs()
    for key, tab in pairs(tabs) do
        local on = key == current
        RB.Shown(tab.line, on)
        RB.SetColor(tab.text, on and T.accent or T.label)
        RB.Shown(tab.panel, on)
    end
end

local x = 0
for _, key in ipairs(tabOrder) do
    local button = CreateFrame("Button", nil, strip)
    button.text = RB.CreateText(button, "GameFontHighlightSmall")
    button.text:SetPoint("LEFT", 8, 1)
    button.text:SetText(TAB_LABELS[key])
    button:SetSize(button.text:GetStringWidth() + 16, TAB_HEIGHT)
    button:SetPoint("BOTTOMLEFT", x, 0)
    x = x + button:GetWidth() + 2
    button.line = RB.CreateFill(button, "ARTWORK", T.accent)
    button.line:ClearAllPoints()
    button.line:SetPoint("BOTTOMLEFT")
    button.line:SetPoint("BOTTOMRIGHT")
    button.line:SetHeight(2)
    button:SetScript("OnClick", function() RB.ShowBuildOutputTab(key) end)
    button:SetScript("OnEnter", function(self) if current ~= key then RB.SetColor(self.text, T.text) end end)
    button:SetScript("OnLeave", function() PaintTabs() end)
    RB.AttachTip(button, "build_tab_" .. key)
    tabs[key] = button
end
-- The selected row's name after the tabs, cut short when the column is narrow.
selection:SetPoint("LEFT", tabs[tabOrder[#tabOrder]], "RIGHT", 12, 0)

-- A scrolling multi-line edit box: select text in it, or copy it with the buttons.
local function ScrollText(name, parent)
    local back = RB.CreatePanel(parent, "well")
    back:SetAllPoints()
    local scroll = CreateFrame("ScrollFrame", "RetailBrowserBuild" .. name, back, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", 6, -6)
    scroll:SetPoint("BOTTOMRIGHT", -26, 6)
    local box = CreateFrame("EditBox", nil, scroll)
    box:SetMultiLine(true)
    box:SetMaxLetters(0)
    box:SetAutoFocus(false)
    box:SetFontObject(GameFontHighlightSmall)
    box:SetWidth(400)
    box:SetScript("OnEscapePressed", function() RB.FocusPage() end)
    box:SetScript("OnEditFocusGained", function() RB.SetKeyOwner("build") end)
    box:SetScript("OnEditFocusLost", function() RB.KeyOwnerLost() end)
    box:SetScript("OnCursorChanged", ScrollingEdit_OnCursorChanged)
    box:SetScript("OnUpdate", function(self, elapsed) ScrollingEdit_OnUpdate(self, elapsed, scroll) end)
    scroll:SetScrollChild(box)
    scroll:SetScript("OnSizeChanged", function(_, width) box:SetWidth(math.max(100, width)) end)
    return box, back, scroll
end

local sqlBox, sqlScroll
sqlBox, tabs.sql.panel, sqlScroll = ScrollText("Sql", body)

-- Wrap off: the box is as wide as its longest line inside a sideways scroll (a multi-line edit box always wraps at
-- its width), with a thin bar under it and Shift+wheel. Lines are measured in the box's font, within PREVIEW_BYTES.
local measure = UIParent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
measure:Hide()
local longest = 0
local hbar = CreateFrame("Slider", nil, tabs.sql.panel)
hbar:SetOrientation("HORIZONTAL")
hbar:SetPoint("BOTTOMLEFT", 6, 5)
hbar:SetPoint("BOTTOMRIGHT", -26, 5)
hbar:SetHeight(HBAR_HEIGHT)
hbar:SetBackdrop(RB.FLAT_BACKDROP)
RB.SetBack(hbar, T.band)
RB.SetBorder(hbar, T.borderDim)
hbar:SetThumbTexture("Interface\\Buttons\\WHITE8X8")
local thumb = hbar:GetThumbTexture()
thumb:SetHeight(HBAR_HEIGHT)
RB.SetVertex(thumb, T.buttonBorder)
hbar:SetScript("OnValueChanged", function(_, value) sqlScroll:SetHorizontalScroll(value) end)
hbar:Hide()
sqlScroll:SetPoint("BOTTOMRIGHT", -26, 6 + HBAR_HEIGHT + 4)
sqlScroll:SetScript("OnMouseWheel", function(self, delta)
    if IsShiftKeyDown() and hbar:IsShown() then
        hbar:SetValue(hbar:GetValue() - delta * HSCROLL_STEP)
    else
        ScrollFrameTemplate_OnMouseWheel(self, delta)
    end
end)

local function Wrap()
    if wrap == nil then
        wrap = RB.CharSetting("buildWrap", false) and true or false
    end
    return wrap
end

local function LongestLine(text)
    local most = 0
    for line in (text:sub(1, PREVIEW_BYTES) .. "\n"):gmatch("(.-)\n") do
        measure:SetText(line)
        most = math.max(most, measure:GetStringWidth())
    end
    return most
end

local function ApplySqlWidth()
    local visible = sqlScroll:GetWidth()
    if visible <= 0 then
        return
    end
    local width = RB.Layout.SqlBoxWidth(Wrap(), visible, longest, SQL_PADDING)
    sqlBox:SetWidth(width)
    local range = width - visible
    hbar:SetMinMaxValues(0, math.max(0, range))
    thumb:SetWidth(math.max(20, hbar:GetWidth() * visible / width))
    RB.Shown(hbar, range > 0.5)
    if range <= 0.5 then
        hbar:SetValue(0)
        sqlScroll:SetHorizontalScroll(0)
    end
end
sqlScroll:SetScript("OnSizeChanged", ApplySqlWidth)

local function SetSqlText(text)
    longest = Wrap() and 0 or LongestLine(text)
    sqlBox:SetText(text)
    sqlBox:SetCursorPosition(0)
    hbar:SetValue(0)
    ApplySqlWidth()
end

local function PaintWrap()
    wrapButton:SetLabel(Wrap() and "Wrap on" or "Wrap off")
    wrapButton:SetOn(Wrap())
end

function RB.ToggleSqlWrap()
    wrap = not Wrap()
    RB.SetCharSetting("buildWrap", wrap)
    PaintWrap()
    longest = wrap and 0 or LongestLine(sqlBox:GetText())
    ApplySqlWidth()
end
col:HookScript("OnShow", PaintWrap)
local mapping
mapping, tabs.mapping.panel = ScrollText("Mapping", body)
RB.AttachTip(sqlBox, "sql_preview")
RB.AttachTip(mapping, "sql_mapping")
local changes = RB.CreateList("BuildChanges", function(entry)
    if entry then RB.Copy(entry.copy, entry.cells[1] .. " (" .. entry.copy .. ")") end
end, "list_field")
changes.panel:SetParent(body)
changes.panel:SetAllPoints()
tabs.changes.panel = changes.panel
PaintTabs()

-- Footer.
-- The status on its own line over the buttons (the column can be narrow).
local status = RB.CreateText(col, "RetailBrowserSmallFont")
status:SetPoint("BOTTOMLEFT", PAD, BUTTONS_HEIGHT + 4)
status:SetPoint("BOTTOMRIGHT", -PAD, BUTTONS_HEIGHT + 4)
status:SetHeight(FOOTER_HEIGHT - BUTTONS_HEIGHT - 6)
local buttons = {}
local function FooterButton(text, width, onClick, tipKey)
    local button = RB.CreateButton(col, text, width, 24, onClick)
    button:SetPoint("BOTTOMRIGHT", buttons[#buttons] or col, buttons[#buttons] and "BOTTOMLEFT" or "BOTTOMRIGHT",
        buttons[#buttons] and -6 or -PAD, buttons[#buttons] and 0 or 8)
    RB.AttachTip(button, tipKey)
    buttons[#buttons + 1] = button
    return button
end

-- Whether the box holds the whole SQL (then Copy and Export take the box, edits included).
local sqlFull, wholeSql = false, ""
local confirmFullAt

local function EditFullSql()
    if sqlFull or wholeSql == "" then
        return
    end
    if #wholeSql > HUGE_SQL_BYTES and not (confirmFullAt and GetTime() - confirmFullAt < CONFIRM_SECONDS) then
        confirmFullAt = GetTime()
        status:SetText(RB.Colored(string.format("The SQL is %d KB: showing it all can freeze the client for seconds or minutes. "
            .. "Click Edit full SQL again to go ahead.", math.ceil(#wholeSql / 1024)), T.title))
        return
    end
    current = "sql"
    PaintTabs()
    SetSqlText(wholeSql)
    sqlFull, confirmFullAt = true, nil
end

FooterButton("Edit full SQL", 100, EditFullSql, "sql_edit")
FooterButton("Export " .. RB.Key("Ctrl+S"), 110, function() RB.ExportBuildSql() end, "sql_export")
FooterButton("Copy " .. RB.Key("Ctrl+Enter"), 130, function() RB.CopyBuildSql() end, "sql_copy")

function RB.ShowBuildOutputTab(key)
    current = key
    PaintTabs()
end

function RB.SetBuildStatus(text)
    status:SetText(text or "")
end

-- The SQL Copy and Export take: the box when it holds all of it (edits included), else the build's.
function RB.BuildFullSql()
    return sqlFull and sqlBox:GetText() or wholeSql
end

local function ChangeEntries(row)
    local entries = {}
    for i, change in ipairs(row and RB.ChangesVsBase(row) or {}) do
        entries[i] = { copy = change.value, cells = { change.column, change.value } }
    end
    return entries
end

-- out: { sql (formatted), rowCount, item (the selected queue row, or nil), mapping text, creatureOnly }.
function RB.ShowBuildOutput(out)
    wholeSql = out.sql or ""
    local item = out.item
    local text, whole = RB.SqlPreview(item and RB.FormatSql(item.row.text) or "", wholeSql, out.rowCount or 0, PREVIEW_BYTES)
    sqlFull, confirmFullAt = whole, nil
    SetSqlText(text)
    mapping:SetText(out.creatureOnly and NOTHING_TO_MAP or (out.mapping or ""))
    local base = item and item.row.base
    RB.ShowListEntries(changes, { { "Column", 170 }, { "This row" } }, ChangeEntries(item and item.row),
        base and ("base " .. base .. ": its own values are in the world DB, so only what this row changes is listed") or "")
    selection:SetText(item and (item.name .. RB.Colored(" · " .. (item.entry or ""), T.value)) or "")
end
