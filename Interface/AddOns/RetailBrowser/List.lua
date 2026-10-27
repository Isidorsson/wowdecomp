-- A mini grid for the inspector: a header line, rows of cells (columns { { label, width (nil: the rest), align } }) and
-- a hint at the bottom right. RB.CreateList scrolls a tab's whole panel; RB.CreateFixedList does not scroll and sizes
-- itself to its rows, for an Overview section. entries: { { cells = {...}, dim }, ... }; onClick(entry, index).

local RB = RetailBrowser
local T = RB.THEME

local ROW_HEIGHT = 14
local HEADER_HEIGHT = 15
local MAX_COLUMNS = 10
local SCROLL_ROWS = 20
local SCROLLBAR_SPACE = 22
local FIXED_INSET = 2
local MIN_FLEX_WIDTH = 40

local function CreateRow(list, r)
    local row = CreateFrame("Button", nil, list.panel)
    row:SetHeight(ROW_HEIGHT)
    row:SetPoint("TOPLEFT", 2, -HEADER_HEIGHT - (r - 1) * ROW_HEIGHT)
    row:SetPoint("RIGHT", list.panel, "RIGHT", -list.rightInset, 0)
    row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    row.focus = RB.CreateFill(row, "BACKGROUND", T.selected)
    row.cells = {}
    for c = 1, MAX_COLUMNS do
        row.cells[c] = RB.CreateText(row, "RetailBrowserGridFont")
        row.cells[c]:SetHeight(ROW_HEIGHT)
    end
    row:SetScript("OnClick", function(self)
        if list.onClick then list.onClick(list.entries[self.index], self.index) end
    end)
    RB.AttachTip(row, list.tipKey)
    list.rows[r] = row
    return row
end

local function EnsureRows(list, count)
    for r = #list.rows + 1, count do
        CreateRow(list, r)
    end
end

-- Puts headers and cells on the columns for the panel's width.
local function PlaceCell(cell, anchor, x, width, column)
    cell:ClearAllPoints()
    if not column then
        cell:Hide()
        return
    end
    cell:SetPoint(anchor.point, anchor.frame, anchor.point, x + anchor.dx, anchor.dy)
    cell:SetWidth(width - 4)
    cell:SetJustifyH(column[3] or "LEFT")
    cell:Show()
end

local function LayOutColumns(list)
    local columns = list.columns or {}
    local fixed = 0
    for _, column in ipairs(columns) do fixed = fixed + (column[2] or 0) end
    local inner = (list.panel:GetWidth() > 0 and list.panel:GetWidth() or 540) - list.rightInset - 4
    local x = 4
    for c = 1, MAX_COLUMNS do
        local column = columns[c]
        local width = column and (column[2] or math.max(MIN_FLEX_WIDTH, inner - fixed)) or 0
        PlaceCell(list.headers[c], { point = "TOPLEFT", frame = list.panel, dx = 0, dy = -1 }, x, width, column)
        if column then list.headers[c]:SetText(column[1]) end
        for _, row in ipairs(list.rows) do
            PlaceCell(row.cells[c], { point = "LEFT", frame = row, dx = -2, dy = 0 }, x, width, column)
        end
        x = x + width
    end
end

local function NewList(parent, onClick, tipKey, rightInset)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetBackdrop(RB.PANEL_BACKDROP)
    RB.SetBack(panel, T.well)
    RB.SetBorder(panel, T.borderDim)
    local list = { panel = panel, entries = {}, onClick = onClick, tipKey = tipKey, rows = {}, headers = {}, rightInset = rightInset }
    list.title = RB.CreateText(panel, "RetailBrowserLabelFont", "RIGHT")
    list.title:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -rightInset - 2, 3)
    list.title:SetPoint("BOTTOMLEFT", panel, "BOTTOMLEFT", 4, 3)
    for c = 1, MAX_COLUMNS do
        list.headers[c] = RB.CreateText(panel, "RetailBrowserHeadFont")
        list.headers[c]:SetHeight(HEADER_HEIGHT - 1)
    end
    panel:SetScript("OnSizeChanged", function()
        LayOutColumns(list)
        RB.RefreshList(list)
    end)
    return list
end

-- A tab's list: scrolls its entries in a FauxScrollFrame (the panel is parented by AddInspectorTab).
function RB.CreateList(name, onClick, tipKey)
    local list = NewList(nil, onClick, tipKey, SCROLLBAR_SPACE)
    local scroll = CreateFrame("ScrollFrame", "RetailBrowserList" .. name, list.panel, "FauxScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", 1, -HEADER_HEIGHT)
    scroll:SetPoint("BOTTOMRIGHT", -SCROLLBAR_SPACE, 1)
    scroll:SetScript("OnVerticalScroll", function(self, offset)
        FauxScrollFrame_OnVerticalScroll(self, offset, ROW_HEIGHT, function() RB.RefreshList(list) end)
    end)
    list.scroll = scroll
    EnsureRows(list, SCROLL_ROWS)
    return list
end

-- An Overview section's list: every entry shown, the panel as tall as its rows (and the hint line, when there is one).
function RB.CreateFixedList(parent, onClick, tipKey)
    local list = NewList(parent, onClick, tipKey, FIXED_INSET)
    list.fixed = true
    return list
end

function RB.FixedListHeight(count, hasTitle)
    return HEADER_HEIGHT + count * ROW_HEIGHT + (hasTitle and ROW_HEIGHT or 0) + 4
end

local function VisibleRows(list)
    if list.fixed then
        return #list.entries
    end
    return math.max(1, math.min(SCROLL_ROWS, math.floor((list.panel:GetHeight() - HEADER_HEIGHT - 1) / ROW_HEIGHT)))
end

function RB.RefreshList(list)
    local visible = VisibleRows(list)
    local offset = list.scroll and FauxScrollFrame_GetOffset(list.scroll) or 0
    for r, row in ipairs(list.rows) do
        local index = offset + r
        local entry = r <= visible and list.entries[index]
        row.index = index
        if entry then
            for c, cell in ipairs(row.cells) do
                cell:SetText(entry.cells[c] or "")
                RB.SetColor(cell, entry.dim and T.disabled or T.text)
            end
            RB.Shown(row.focus, index == list.focused)
            row:Show()
        else
            row:Hide()
        end
    end
    if list.scroll then
        FauxScrollFrame_Update(list.scroll, #list.entries, visible, ROW_HEIGHT)
    end
end

function RB.ShowListEntries(list, columns, entries, title, focused)
    list.columns, list.entries, list.focused = columns, entries, focused
    list.title:SetText(title or "")
    if list.fixed then
        EnsureRows(list, #entries)
        list.panel:SetHeight(RB.FixedListHeight(#entries, title and title ~= ""))
    end
    LayOutColumns(list)
    RB.RefreshList(list)
end
