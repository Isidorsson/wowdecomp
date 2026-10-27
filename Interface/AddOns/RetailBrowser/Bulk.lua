-- The bulk bar under the grid: what it does to the checked rows (of any page). Without checked rows it says how to
-- check some. The grid's letter keys G, I, Y, F act on the checked rows when there are some, else on the focused one.
-- Its buttons wrap into rows beside the count (their keys are in their tooltips), so the bar grows to fit a narrow
-- main column; the grid gives up rows for it.

local RB = RetailBrowser
local L = RB.LAYOUT

local CONFIRM_SECONDS = 5
local CONFIRM_ABOVE = 10

local LABEL_WIDTH = 74
local CHECK_PAGE_WIDTH = 120
local GAP = 4
local BUTTON_MIN_WIDTH = 64
local BUTTON_HEIGHT = 18
local ROW_SPACE = BUTTON_HEIGHT + 4

local bar = RB.CreatePanel(RB.mainCol, "band")
bar:SetPoint("TOPLEFT", RB.grid, "BOTTOMLEFT", 0, -2)
bar:SetPoint("TOPRIGHT", RB.grid, "BOTTOMRIGHT", 0, -2)
bar:SetHeight(L.bulkHeight)
RB.bulk = bar

local label = RB.CreateText(bar, "GameFontNormalSmall")
label:SetPoint("TOPLEFT", 8, -(L.bulkHeight - 12) / 2)
label:SetWidth(LABEL_WIDTH)
local idle = RB.CreateText(bar, "RetailBrowserLabelFont")
idle:SetPoint("LEFT", 8, 0)
idle:SetPoint("RIGHT", -CHECK_PAGE_WIDTH - 12, 0)
idle:SetText("Check rows: click or drag down the boxes, Shift+click a range, Shift+Up/Down, Ctrl+A the page.")

-- The details of each checked row (fetched for rows of other pages too).
local function CheckedDetails()
    local list = {}
    for _, row in ipairs(RB.CheckedRows()) do
        local d = row.d or RB.DetailsOf(row.kind, row.id)
        if d then
            list[#list + 1] = { row = row, d = d }
        end
    end
    return list
end

function RB.BulkCopyIds()
    local ids = {}
    for _, entry in ipairs(CheckedDetails()) do
        ids[#ids + 1] = RB.PrimaryId(entry.d)
    end
    RB.Copy(table.concat(ids, ", "), #ids .. " ids: " .. table.concat(ids, ", "))
end

-- Rows as text: the visible columns, tab- or comma-separated, with a header line.
local function RowsText(separator)
    local columns = RB.ColumnsNow()
    local lines, header = {}, {}
    for _, column in ipairs(columns) do
        if column.cell then header[#header + 1] = column.label or column.key end
    end
    lines[1] = table.concat(header, separator)
    for _, row in ipairs(RB.CheckedRows()) do
        row.d = row.d or RB.DetailsOf(row.kind, row.id)
        local cells, values = RB.CellsOf(row, columns), {}
        for i, column in ipairs(columns) do
            if column.cell then
                local text = tostring(cells[i] and cells[i][1] or "")
                if separator == "," and text:find("[,\"]") then
                    text = "\"" .. text:gsub("\"", "\"\"") .. "\""
                end
                values[#values + 1] = text
            end
        end
        lines[#lines + 1] = table.concat(values, separator)
    end
    return lines
end

function RB.BulkCopyRows()
    local lines = RowsText("\t")
    RB.Copy(table.concat(lines, "\n"), (#lines - 1) .. " rows")
end

function RB.BulkCsv()
    local lines = RowsText(",")
    RB.Copy(table.concat(lines, "\n"), "CSV of " .. (#lines - 1) .. " rows")
end

function RB.BulkGm()
    local lines = {}
    for _, entry in ipairs(CheckedDetails()) do
        local text = RB.GmCommandText(entry.d)
        if text ~= "" then lines[#lines + 1] = text end
    end
    RB.Copy(table.concat(lines, "\n"), #lines .. " GM commands")
end

-- Sends every .additem line of the checked items and sets; many lines ask for a second press first.
function RB.BulkAdd()
    local commands = {}
    for _, entry in ipairs(CheckedDetails()) do
        for _, command in ipairs(RB.CommandsFor(entry.d).addItem) do
            commands[#commands + 1] = command
        end
    end
    if #commands > RB.BULK_LIMIT then
        RB.Print(string.format("%d .additem lines is more than %d: check fewer rows.", #commands, RB.BULK_LIMIT))
        return
    end
    if #commands > CONFIRM_ABOVE and not (RB.addConfirmAt and GetTime() - RB.addConfirmAt < CONFIRM_SECONDS) then
        RB.addConfirmAt = GetTime()
        RB.Print(string.format("this sends %d .additem lines: press Add again within %d s to go ahead.", #commands, CONFIRM_SECONDS))
        return
    end
    RB.addConfirmAt = nil
    for _, command in ipairs(commands) do
        RB.SendCommand(command)
    end
end

function RB.BulkTryOn()
    local list = {}
    for _, entry in ipairs(CheckedDetails()) do
        list[#list + 1] = entry.d
    end
    RB.TryOnAll(list)
end

function RB.BulkFavourite()
    local list = CheckedDetails()
    local all = #list > 0
    for _, entry in ipairs(list) do
        all = all and RB.IsFavourite(entry.d.kind, entry.d.id)
    end
    for _, entry in ipairs(list) do
        RB.SetFavourite(entry.d.kind, entry.d.id, entry.d.name, not all)
    end
    RB.Print((all and "removed " or "added ") .. #list .. (all and " favourites" or " to favourites"))
    RB.AfterFavouriteChange()
end

local buttons = {}
local function AddButton(key, text, onClick, kinds)
    local button = RB.CreateButton(bar, text, 80, 18, onClick)
    button.kinds = kinds
    RB.AttachTip(button, "bulk_" .. key)
    buttons[#buttons + 1] = button
    buttons[key] = button
end

local ITEMS = { item = true, set = true }
AddButton("ids", "Copy ids", RB.BulkCopyIds)
AddButton("rows", "Rows", RB.BulkCopyRows)
AddButton("gm", "GM", RB.BulkGm)
AddButton("add", "Add", RB.BulkAdd, ITEMS)
AddButton("try", "Try on", RB.BulkTryOn, ITEMS)
AddButton("fav", "Fav", RB.BulkFavourite)
AddButton("csv", "CSV", RB.BulkCsv)
AddButton("build", "Build SQL x1", function() RB.ShowBuild() end, RB.BUILD_TABLES)
AddButton("clear", "Clear", RB.ClearChecks)

local checkPage = RB.CreateButton(bar, "Check page", CHECK_PAGE_WIDTH, BUTTON_HEIGHT, function() RB.CheckPage() end)
RB.AttachTip(checkPage, "checkPage")

-- Idle: the hint and Check page on one line. With checked rows: the count, then the buttons and Check page in as
-- many rows as the bar's width needs. Returns the bar's height.
local function LayOutBar(checking)
    checkPage:ClearAllPoints()
    if not checking then
        checkPage:SetWidth(CHECK_PAGE_WIDTH)
        checkPage:SetPoint("RIGHT", -6, 0)
        return L.bulkHeight
    end
    local items = { unpack(buttons) }
    items[#items + 1] = checkPage
    local width = bar:GetWidth() - 16 - LABEL_WIDTH
    local columns, buttonWidth, rows = RB.Layout.ActionGrid(#items, width, BUTTON_MIN_WIDTH, GAP)
    for i, item in ipairs(items) do
        local n = i - 1
        item:ClearAllPoints()
        item:SetWidth(buttonWidth)
        item:SetPoint("TOPLEFT", 8 + LABEL_WIDTH + (n % columns) * (buttonWidth + GAP), -3 - math.floor(n / columns) * ROW_SPACE)
    end
    return rows * ROW_SPACE + 2
end

local function ApplyHeight(height)
    if math.abs(bar:GetHeight() - height) > 0.5 then
        bar:SetHeight(height)
        RB.FitGridRows()
    end
end
bar:SetScript("OnSizeChanged", function() ApplyHeight(LayOutBar(#RB.CheckedRows() > 0)) end)

function RB.RefreshBulk()
    local rows = RB.CheckedRows()
    local count = #rows
    local kind = rows[1] and rows[1].kind
    RB.Shown(label, count > 0)
    RB.Shown(idle, count == 0)
    label:SetText(count .. " checked")
    for _, button in ipairs(buttons) do
        RB.Shown(button, count > 0)
        button:SetEnabled(not button.kinds or (kind and button.kinds[kind]))
    end
    buttons.build:SetLabel("Build SQL x" .. count)
    RB.Shown(checkPage, #RB.rows > 0)
    ApplyHeight(LayOutBar(count > 0))
    if RB.OnChecksChanged then
        RB.OnChecksChanged(count)
    end
end
