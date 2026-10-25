-- The result list: one page of RetailSearch rows (or a saved list), sortable by name or id, with keyboard navigation
-- (arrows and page keys move, Enter previews, Escape leaves the list) and paging and CSV export below.

local RB = RetailBrowser
local frame = RB.frame

local LIST_WIDTH = 440
local ROW_HEIGHT = 20
local VISIBLE_ROWS = 21

local list = RB.CreateInset(frame, LIST_WIDTH, ROW_HEIGHT * VISIBLE_ROWS + 30)
list:SetPoint("TOPLEFT", 20, -124)
RB.list = list
RB.rows = {}

local scroll = CreateFrame("ScrollFrame", "RetailBrowserListScroll", list, "FauxScrollFrameTemplate")
scroll:SetPoint("TOPLEFT", 4, -26)
scroll:SetPoint("BOTTOMRIGHT", -26, 4)

local function SortRows(key)
    local sort = RB.sort
    if sort and sort.key == key then
        sort.ascending = not sort.ascending
    else
        RB.sort = { key = key, ascending = true }
    end
    local s = RB.sort
    table.sort(RB.rows, function(a, b)
        local x, y = a[s.key], b[s.key]
        if x == y then
            return a.id < b.id
        end
        if s.ascending then return x < y else return x > y end
    end)
    RB.RefreshList()
end

local function AddHeader(text, key, width, x)
    local header = CreateFrame("Button", nil, list)
    header:SetSize(width, 18)
    header:SetPoint("TOPLEFT", x, -5)
    header:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    local label = RB.CreateText(header, "GameFontNormalSmall")
    label:SetPoint("LEFT", 4, 0)
    label:SetText(text)
    header:SetScript("OnClick", function() SortRows(key) end)
end
AddHeader("Name (click to sort)", "name", 300, 6)
AddHeader("Id", "id", 90, 320)

local function RowClicked(self)
    RB.SelectRow(self.index, true)
    RB.FocusList()
end

local buttons = {}
for i = 1, VISIBLE_ROWS do
    local row = CreateFrame("Button", nil, list)
    row:SetSize(LIST_WIDTH - 34, ROW_HEIGHT)
    row:SetPoint("TOPLEFT", 6, -26 - (i - 1) * ROW_HEIGHT)
    row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    row.selected = row:CreateTexture(nil, "BACKGROUND")
    row.selected:SetAllPoints()
    row.selected:SetTexture(0.2, 0.45, 0.8, 0.45)
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetSize(ROW_HEIGHT - 2, ROW_HEIGHT - 2)
    row.icon:SetPoint("LEFT", 2, 0)
    row.name = RB.CreateText(row, "GameFontHighlightSmall")
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 4, 0)
    row.name:SetSize(170, 12)
    row.detail = RB.CreateText(row, "GameFontDisableSmall")
    row.detail:SetPoint("LEFT", row.name, "RIGHT", 4, 0)
    row.detail:SetSize(130, 12)
    row.id = RB.CreateText(row, "GameFontHighlightSmall", "RIGHT")
    row.id:SetPoint("RIGHT", -2, 0)
    row:SetScript("OnClick", RowClicked)
    buttons[i] = row
end

local pageText = RB.CreateText(frame, "GameFontHighlightSmall", "CENTER")
local prev = RB.CreateButton(frame, "< Prev", 70, function() RB.TurnPage(-1) end)
prev:SetPoint("TOPLEFT", list, "BOTTOMLEFT", 0, -4)
local nextPage = RB.CreateButton(frame, "Next >", 70, function() RB.TurnPage(1) end)
nextPage:SetPoint("LEFT", prev, "RIGHT", 4, 0)
pageText:SetPoint("LEFT", nextPage, "RIGHT", 10, 0)
local export = RB.CreateButton(frame, "Export CSV", 90, function() RB.ExportCsv() end)
export:SetPoint("TOPRIGHT", list, "BOTTOMRIGHT", 0, -4)
local focusHint = RB.CreateText(frame, "GameFontDisableSmall")
focusHint:SetPoint("BOTTOMRIGHT", list, "TOPRIGHT", -4, 2)

function RB.RefreshList()
    local offset = FauxScrollFrame_GetOffset(scroll)
    for i, row in ipairs(buttons) do
        local index = offset + i
        local entry = RB.rows[index]
        if entry then
            row.index = index
            row.icon:SetTexture(entry.icon > 0 and RB.IconPath(entry.icon) or nil)
            row.name:SetText(entry.name)
            row.name:SetTextColor(RB.QualityColor(entry.quality))
            row.detail:SetText(entry.detail)
            row.id:SetText(entry.id)
            if index == RB.selected then row.selected:Show() else row.selected:Hide() end
            row:Show()
        else
            row:Hide()
        end
    end
    FauxScrollFrame_Update(scroll, #RB.rows, VISIBLE_ROWS, ROW_HEIGHT)
end
scroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, ROW_HEIGHT, RB.RefreshList)
end)

local function SetRows(rows, kind)
    for _, row in ipairs(rows) do
        row.kind = row.kind or kind
    end
    RB.rows, RB.selected, RB.sort = rows, nil, nil
    FauxScrollFrame_SetOffset(scroll, 0)
    RetailBrowserListScrollScrollBar:SetValue(0)
    RB.RefreshList()
end

function RB.ShowResult(result)
    RB.result = result
    SetRows(result.rows, RB.state.kind)
    pageText:SetText(result.total > 0 and string.format("Page %d / %d, %d matches", result.page, result.pages, result.total) or "No matches")
end

-- A saved list: favourites or history, of every kind.
function RB.ShowRows(entries, source)
    local rows = {}
    for _, entry in ipairs(entries) do
        rows[#rows + 1] = { id = entry.id, name = entry.name, detail = RB.KindLabel(entry.kind), kind = entry.kind, displayId = 0, icon = 0, quality = 1 }
    end
    RB.result = nil
    SetRows(rows)
    pageText:SetText(#rows .. (source == "favourites" and " favourites" or " viewed"))
end

function RB.TurnPage(step)
    local result = RB.result
    if not result or RB.state.source ~= "search" then
        return
    end
    local page = RB.state.page + step
    if page >= 1 and page <= result.pages then
        RB.state.page = page
        RB.Search(true)
    end
end

-- Keeps the selected row in view, scrolling by whole rows.
local function ScrollTo(index)
    local offset = FauxScrollFrame_GetOffset(scroll)
    if index <= offset then
        offset = index - 1
    elseif index > offset + VISIBLE_ROWS then
        offset = index - VISIBLE_ROWS
    end
    RetailBrowserListScrollScrollBar:SetValue(offset * ROW_HEIGHT)
end

function RB.SelectRow(index, preview)
    local entry = RB.rows[index]
    if not entry then
        return
    end
    RB.selected = index
    ScrollTo(index)
    RB.RefreshList()
    RB.ShowDetails(entry.kind, entry.id, preview)
end

-- Keys go to the list until Escape or a key it does not use (3.3.5 frames cannot pass keys on, so the list lets go).
function RB.FocusList()
    list:EnableKeyboard(true)
    focusHint:SetText("Keys: arrows, PgUp/PgDn, Enter, Esc")
end

local function ReleaseList()
    list:EnableKeyboard(false)
    focusHint:SetText("")
end

local KEY_STEPS = { UP = -1, DOWN = 1, PAGEUP = -VISIBLE_ROWS, PAGEDOWN = VISIBLE_ROWS }

list:SetScript("OnKeyDown", function(self, key)
    local step = KEY_STEPS[key]
    if step then
        local index = math.max(1, math.min(#RB.rows, (RB.selected or 0) + step))
        RB.SelectRow(index, false)
    elseif key == "ENTER" then
        if RB.selected then RB.SelectRow(RB.selected, true) end
    else
        ReleaseList()
    end
end)
list:SetScript("OnHide", ReleaseList)

function RB.ExportCsv()
    local state = RB.state
    if state.source ~= "search" then
        RB.Print("export works on search results.")
        return
    end
    local path, err = RetailExportCsv(state.kind, state.text, state.filters)
    RB.Print(path and ("exported to " .. path) or ("export failed: " .. tostring(err)))
end
