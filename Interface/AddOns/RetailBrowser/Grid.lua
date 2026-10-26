-- The grid: a header per column (click sorts the whole index, again flips it), the filter row (Filters.lua), pooled
-- 16 px rows (as many as the window's height leaves) over a FauxScrollFrame of the page's 50 rows, a check box per
-- row (bulk actions act on the checked rows), the focused row (the drawer shows it) and hover (the hover preview).
-- Columns reflow with the window's width: fixed columns keep theirs, the name column takes the rest.

local RB = RetailBrowser
local L = RB.LAYOUT
local T = RB.THEME
local frame = RB.frame

local MAX_COLUMNS = 13
local MAX_ROWS = 40
local SCROLLBAR = 20
local INSET = RB.EDGE_INSET
local ROWS_TOP = INSET + L.headHeight + L.filterHeight
local SELECTED_EDGE = 2

local grid = RB.CreatePanel(frame, "well")
grid:SetPoint("TOPLEFT", L.pad, -L.gridTop)
grid:SetPoint("TOPRIGHT", -L.pad, -L.gridTop)
RB.grid = grid
RB.rows, RB.checked = {}, {}
RB.visibleRows = 10
RB.GRID_CHROME = ROWS_TOP + INSET

local headerBack = RB.CreateFill(grid, "BORDER", T.band)
headerBack:ClearAllPoints()
headerBack:SetPoint("TOPLEFT", INSET, -INSET)
headerBack:SetPoint("TOPRIGHT", -INSET, -INSET)
headerBack:SetHeight(L.headHeight + L.filterHeight)

local scroll = CreateFrame("ScrollFrame", "RetailBrowserGridScroll", grid, "FauxScrollFrameTemplate")
scroll:SetPoint("TOPLEFT", INSET, -ROWS_TOP)
scroll:SetPoint("BOTTOMRIGHT", -SCROLLBAR - INSET, INSET)

-- Column x offsets and widths: fixed widths, the rest to the name column.
local layout = {}
local function LayOut(columns)
    local inner = grid:GetWidth() - 2 * INSET - SCROLLBAR
    local fixed = 0
    for _, column in ipairs(columns) do
        fixed = fixed + (column.width or 0)
    end
    local x = INSET
    wipe(layout)
    for i, column in ipairs(columns) do
        local width = column.width or math.max(120, inner - fixed)
        layout[i] = { x = x, width = width }
        x = x + width
    end
end

function RB.ColumnLayout()
    return layout
end

-- Headers.
local headers = {}
local function HeaderClicked(self)
    local column = RB.ColumnsNow()[self.index]
    if not column or not column.sort then
        return
    end
    local sort = RB.state.sort
    if sort and sort.key == column.key then
        sort.descending = not sort.descending
    else
        RB.state.sort = { key = column.key, code = column.sort, descending = false }
    end
    RB.state.page = 1
    RB.RefreshHeaders()
    RB.Search(true)
    RB.FocusGrid()
end

for i = 1, MAX_COLUMNS do
    local header = CreateFrame("Button", nil, grid)
    header:SetHeight(L.headHeight)
    RB.SetHoverHighlight(header)
    header.index = i
    header.text = RB.CreateText(header, "RetailBrowserHeadFont")
    header.text:SetPoint("LEFT", 3, 0)
    header.text:SetPoint("RIGHT", -2, 0)
    header.line = RB.CreateFill(header, "BORDER", T.borderDim)
    header.line:ClearAllPoints()
    header.line:SetPoint("TOPRIGHT")
    header.line:SetPoint("BOTTOMRIGHT")
    header.line:SetWidth(1)
    header:SetScript("OnClick", HeaderClicked)
    RB.AttachTip(header, function(self)
        local column = RB.ColumnsNow()[self.index]
        local text = column and (RB.COLUMN_TIPS[column.key] or RB.COLUMN_TIPS[column.filter and column.filter.key or ""])
        return column and { column.label or (column.key == "check" and "Check" or "Icon"), text, column.sort and "Click: sort over the whole index" or nil }
    end)
    headers[i] = header
end

function RB.RefreshHeaders()
    local columns, sort = RB.ColumnsNow(), RB.state.sort
    for i, header in ipairs(headers) do
        local column = columns[i]
        if column then
            local label = column.label or ""
            if sort and sort.key == column.key then
                label = label .. " " .. RB.Colored(sort.descending and "v" or "^", T.value)
            end
            header.text:SetText(label)
            header.text:SetJustifyH(column.align == "RIGHT" and "RIGHT" or column.align == "CENTER" and "CENTER" or "LEFT")
            header:Show()
        else
            header:Hide()
        end
    end
end

-- Rows.
local buttons = {}

local function RowClicked(self, mouse)
    local index = self.index
    if not RB.rows[index] then
        return
    end
    local x = GetCursorPosition() / self:GetEffectiveScale() - self:GetLeft()
    if mouse == "RightButton" or x < 18 or IsControlKeyDown() then
        RB.ToggleCheck(index)
    elseif IsShiftKeyDown() then
        RB.CheckRange(RB.anchor or index, index)
    else
        RB.FocusRow(index, true)
    end
    RB.anchor = index
    RB.FocusGrid()
end

local function CreateRow(i)
    local row = CreateFrame("Button", nil, grid)
    row:SetHeight(L.rowHeight)
    row:SetPoint("TOPLEFT", INSET, -ROWS_TOP - (i - 1) * L.rowHeight)
    row:SetPoint("RIGHT", grid, "RIGHT", -SCROLLBAR - INSET, 0)
    row:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    row.stripe = RB.CreateFill(row, "BACKGROUND", T.stripe)
    row.checkedFill = RB.CreateFill(row, "BORDER", T.checked)
    row.focusFill = RB.CreateFill(row, "ARTWORK", T.selected)
    row.focusEdge = RB.CreateFill(row, "ARTWORK", T.selectedEdge)
    row.focusEdge:ClearAllPoints()
    row.focusEdge:SetPoint("TOPLEFT")
    row.focusEdge:SetPoint("BOTTOMLEFT")
    row.focusEdge:SetWidth(SELECTED_EDGE)
    RB.SetHoverHighlight(row)
    -- One texture: the empty box, or the tick on the checked row's fill (one layer cannot stack two).
    row.box = row:CreateTexture(nil, "OVERLAY")
    row.box:SetSize(16, 16)
    row.icon = row:CreateTexture(nil, "OVERLAY")
    row.icon:SetSize(14, 14)
    row.cells = {}
    for c = 1, MAX_COLUMNS do
        row.cells[c] = RB.CreateText(row, "RetailBrowserGridFont")
        row.cells[c]:SetHeight(L.rowHeight)
    end
    row:SetScript("OnClick", RowClicked)
    row:SetScript("OnEnter", function(self) RB.HoverRow(self) end)
    row:SetScript("OnLeave", function() RB.HoverEnd() end)
    if i % 2 == 1 then row.stripe:Hide() end
    return row
end

for i = 1, MAX_ROWS do
    buttons[i] = CreateRow(i)
end

-- Places the cells of every row for the columns now shown.
local function LayOutRows(columns)
    for _, row in ipairs(buttons) do
        row.box:Hide()
        row.icon:Hide()
        for c, cell in ipairs(row.cells) do
            local column, place = columns[c], layout[c]
            cell:ClearAllPoints()
            if column and column.cell then
                cell:SetPoint("LEFT", row, "LEFT", place.x - INSET + 3, 0)
                cell:SetWidth(place.width - 6)
                cell:SetJustifyH(column.align or "LEFT")
                cell:Show()
            else
                cell:Hide()
            end
            if column and column.special == "check" then
                row.box:SetPoint("LEFT", row, "LEFT", place.x - INSET, 0)
            elseif column and column.special == "icon" then
                row.icon:SetPoint("LEFT", row, "LEFT", place.x - INSET + 1, 0)
            end
        end
    end
end

-- Places headers, cells and filter cells for the columns now shown and the grid's width.
function RB.RelayoutColumns()
    local columns = RB.ColumnsNow()
    LayOut(columns)
    for i, header in ipairs(headers) do
        local place = layout[i]
        if place then
            header:ClearAllPoints()
            header:SetPoint("TOPLEFT", grid, "TOPLEFT", place.x, -INSET)
            header:SetWidth(place.width)
        end
    end
    RB.RefreshHeaders()
    LayOutRows(columns)
    RB.PlaceFilterCells(layout)
    RB.RefreshGrid()
end

function RB.OnColumnsChanged()
    local columns = RB.ColumnsNow()
    LayOut(columns)
    RB.LayOutFilterCells(columns, layout)
    RB.RelayoutColumns()
    RB.checked = {}
    RB.RefreshBulk()
end

-- An icon the prefetch (RetailIconPending, wd-casc-io) is still reading is left blank and set once it is read, so a
-- new page never reads its icons on the frame that shows it. Without the prefetch every icon is set at once.
local function SetIcon(row, entry)
    local icon = entry.icon or 0
    local pending = icon > 0 and type(RetailIconPending) == "function" and RetailIconPending(icon)
    row.iconPending = pending
    row.icon:SetTexture(not pending and icon > 0 and RB.IconPath(icon) or nil)
    return not pending
end

local function FillPendingIcons()
    local waiting = false
    for _, row in ipairs(buttons) do
        local entry = row.iconPending and row:IsShown() and RB.rows[row.index]
        if entry and not SetIcon(row, entry) then
            waiting = true
        end
    end
    if not waiting then
        grid:SetScript("OnUpdate", nil)
    end
end

local function ShowRow(row, entry, columns)
    local cells = RB.CellsOf(entry, columns)
    local hasIcon = columns[2] and columns[2].special == "icon"
    for c, cell in ipairs(row.cells) do
        local value = cells[c]
        if value then
            cell:SetText(value[1])
            cell:SetTextColor(value[2], value[3], value[4])
        end
    end
    local checked = RB.checked[RB.EntryKey(entry.kind, entry.id)] ~= nil
    row.box:SetTexture(checked and "Interface\\Buttons\\UI-CheckBox-Check" or "Interface\\Buttons\\UI-CheckBox-Up")
    row.box:Show()
    RB.Shown(row.checkedFill, checked)
    RB.Shown(row.focusFill, row.index == RB.focused)
    RB.Shown(row.focusEdge, row.index == RB.focused)
    if hasIcon then
        row.icon:Show()
        return SetIcon(row, entry)
    end
    row.iconPending = nil
    return true
end

function RB.RefreshGrid()
    local offset = FauxScrollFrame_GetOffset(scroll)
    local columns = RB.ColumnsNow()
    local waiting = false
    for i, row in ipairs(buttons) do
        local index = offset + i
        local entry = i <= RB.visibleRows and RB.rows[index]
        row.index = index
        if entry then
            waiting = not ShowRow(row, entry, columns) or waiting
            row:Show()
        else
            row:Hide()
        end
    end
    FauxScrollFrame_Update(scroll, #RB.rows, RB.visibleRows, L.rowHeight)
    grid:SetScript("OnUpdate", waiting and FillPendingIcons or nil)
end

scroll:SetScript("OnVerticalScroll", function(self, offset)
    RB.scrolledAt = GetTime()
    RB.HoverEnd()
    FauxScrollFrame_OnVerticalScroll(self, offset, L.rowHeight, RB.RefreshGrid)
end)

function RB.SetRows(rows)
    RB.rows, RB.focused = rows, nil
    FauxScrollFrame_SetOffset(scroll, 0)
    RetailBrowserGridScrollScrollBar:SetValue(0)
    RB.RefreshGrid()
    RB.RefreshBulk()
end

-- The rows the drawer leaves (Drawer.lua sets them from the window's height).
function RB.SetGridRows(count)
    RB.visibleRows = math.max(1, math.min(MAX_ROWS, count))
    grid:SetHeight(RB.GRID_CHROME + RB.visibleRows * L.rowHeight)
    RB.RefreshGrid()
end

-- Keeps a row in view, scrolling by whole rows.
local function ScrollTo(index)
    local offset = FauxScrollFrame_GetOffset(scroll)
    if index <= offset then
        offset = index - 1
    elseif index > offset + RB.visibleRows then
        offset = index - RB.visibleRows
    end
    RetailBrowserGridScrollScrollBar:SetValue(offset * L.rowHeight)
end

-- Focuses a row: the drawer shows it (its preview after a pause, at once when now is set).
function RB.FocusRow(index, now)
    local entry = RB.rows[index]
    if not entry then
        return
    end
    RB.focused = index
    ScrollTo(index)
    RB.RefreshGrid()
    RB.ShowDetails(entry.kind, entry.id, now)
end

function RB.FocusedRow()
    return RB.focused and RB.rows[RB.focused]
end

function RB.MoveFocus(step)
    if #RB.rows == 0 then
        return
    end
    local index = math.max(1, math.min(#RB.rows, (RB.focused or 0) + step))
    RB.FocusRow(index, false)
    return index
end

-- Checked rows, by "kind:id", across pages.
function RB.ToggleCheck(index)
    local entry = RB.rows[index]
    if not entry then
        return
    end
    local key = RB.EntryKey(entry.kind, entry.id)
    RB.checked[key] = not RB.checked[key] and entry or nil
    RB.RefreshGrid()
    RB.RefreshBulk()
end

function RB.CheckRange(from, to)
    for index = math.min(from, to), math.max(from, to) do
        local entry = RB.rows[index]
        if entry then
            RB.checked[RB.EntryKey(entry.kind, entry.id)] = entry
        end
    end
    RB.RefreshGrid()
    RB.RefreshBulk()
end

function RB.CheckPage()
    RB.CheckRange(1, #RB.rows)
end

function RB.ClearChecks()
    RB.checked = {}
    RB.RefreshGrid()
    RB.RefreshBulk()
end

-- The checked rows in page order first, then those of other pages.
function RB.CheckedRows()
    local list, seen = {}, {}
    for _, entry in ipairs(RB.rows) do
        local key = RB.EntryKey(entry.kind, entry.id)
        if RB.checked[key] then
            list[#list + 1], seen[key] = entry, true
        end
    end
    for key, entry in pairs(RB.checked) do
        if not seen[key] then
            list[#list + 1] = entry
        end
    end
    return list
end
