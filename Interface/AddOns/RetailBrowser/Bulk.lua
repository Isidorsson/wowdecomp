-- The bulk bar under the grid: what it does to the checked rows (of any page). Without checked rows it says how to
-- check some. The grid's letter keys G, I, Y, F act on the checked rows when there are some, else on the focused one.

local RB = RetailBrowser
local L = RB.LAYOUT
local frame = RB.frame

local CONFIRM_SECONDS = 5
local CONFIRM_ABOVE = 10

local LABEL_WIDTH = 74
local CHECK_PAGE_WIDTH = 120
local GAP = 4

local bar = RB.CreatePanel(frame, "band")
bar:SetPoint("TOPLEFT", RB.grid, "BOTTOMLEFT", 0, -2)
bar:SetPoint("TOPRIGHT", RB.grid, "BOTTOMRIGHT", 0, -2)
bar:SetHeight(L.bulkHeight)
RB.bulk = bar

local label = RB.CreateText(bar, "GameFontNormalSmall")
label:SetPoint("LEFT", 8, 0)
label:SetWidth(LABEL_WIDTH)
local idle = RB.CreateText(bar, "RetailBrowserLabelFont")
idle:SetPoint("LEFT", 8, 0)
idle:SetText("Check rows for bulk actions: Space, Ctrl+click or right-click a row, Shift+click a range, Ctrl+A the page.")

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
AddButton("ids", "Copy ids " .. RB.Key("Ctrl+C"), RB.BulkCopyIds)
AddButton("rows", "Rows " .. RB.Key("Ctrl+Shift+C"), RB.BulkCopyRows)
AddButton("gm", "GM " .. RB.Key("G"), RB.BulkGm)
AddButton("add", "Add " .. RB.Key("I"), RB.BulkAdd, ITEMS)
AddButton("try", "Try on " .. RB.Key("Y"), RB.BulkTryOn, ITEMS)
AddButton("fav", "Fav " .. RB.Key("F"), RB.BulkFavourite)
AddButton("csv", "CSV " .. RB.Key("Shift+E"), RB.BulkCsv)
AddButton("build", "Build SQL x1 " .. RB.Key("B"), function() RB.ShowBuild("checked") end, { item = true, set = true, creature = true })
AddButton("clear", "Clear " .. RB.Key("Esc"), RB.ClearChecks)

local checkPage = RB.CreateButton(bar, "Check page " .. RB.Key("Ctrl+A"), CHECK_PAGE_WIDTH, 18, function() RB.CheckPage() end)
checkPage:SetPoint("RIGHT", -6, 0)
RB.AttachTip(checkPage, "checkPage")

-- The buttons share the bar's width between the label and Check page.
local function SpreadButtons()
    local width = (bar:GetWidth() - 16 - LABEL_WIDTH - CHECK_PAGE_WIDTH - GAP * (#buttons + 1)) / #buttons
    for i, button in ipairs(buttons) do
        button:SetWidth(math.max(60, width))
        button:ClearAllPoints()
        button:SetPoint("LEFT", bar, "LEFT", 8 + LABEL_WIDTH + (i - 1) * (math.max(60, width) + GAP), 0)
    end
end
bar:SetScript("OnSizeChanged", SpreadButtons)

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
    buttons.build:SetLabel("Build SQL x" .. count .. " " .. RB.Key("B"))
    RB.Shown(checkPage, #RB.rows > 0)
    if RB.OnChecksChanged then
        RB.OnChecksChanged(count)
    end
end
