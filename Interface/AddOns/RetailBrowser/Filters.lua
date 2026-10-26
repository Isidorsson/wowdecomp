-- The grid's filter row: one cell on top of each column. The name cell is the search box (/ focuses it); a range
-- cell takes 200-, -300, 100-200, >=5, >5, <=5, <5 or =5; an exact cell a number; a choice cell opens a menu. Every
-- cell sets a RetailSearch filter, so the index filters all its entries, not the page shown. Active filters show as
-- chips in the query row.

local RB = RetailBrowser
local L = RB.LAYOUT
local T = RB.THEME
local grid = RB.grid

local MAX_COLUMNS = 13
local CELL_TOP = -(RB.EDGE_INSET + L.headHeight + 1)

local boxes, choiceButtons, favButtons = {}, {}, {}
local columnsNow = {}
local suppressText = false

local function Trim(text)
    return (text or ""):match("^%s*(.-)%s*$")
end

-- min, max of a range text; ok is false for text that is not a range.
local function ParseRange(text)
    text = Trim(text)
    if text == "" then
        return nil, nil, true
    end
    local a, b = text:match("^(%d+)%s*%-%s*(%d+)$")
    if a then return tonumber(a), tonumber(b), true end
    a = text:match("^(%d+)%s*%-$") or text:match("^>=%s*(%d+)$")
    if a then return tonumber(a), nil, true end
    a = text:match("^%-%s*(%d+)$") or text:match("^<=%s*(%d+)$")
    if a then return nil, tonumber(a), true end
    a = text:match("^>%s*(%d+)$")
    if a then return tonumber(a) + 1, nil, true end
    a = text:match("^<%s*(%d+)$")
    if a then return nil, math.max(0, tonumber(a) - 1), true end
    a = text:match("^=?%s*(%d+)$")
    if a then return tonumber(a), tonumber(a), true end
    return nil, nil, false
end
RB.ParseRange = ParseRange

local function FiltersChanged()
    RB.state.page = 1
    RB.Search(false)
    RB.RefreshChips()
end

local function BoxChanged(self)
    if suppressText then
        return
    end
    local column = columnsNow[self.index]
    local filter = column and column.filter
    if not filter then
        return
    end
    local text = self:GetText()
    RB.RefreshHint(self)
    if filter.type == "search" then
        RB.state.text = text
        RB.SetWellEdge(self, text ~= "" and T.inputActive or T.inputBorder)
        FiltersChanged()
        return
    end
    local min, max, ok
    if filter.type == "exact" then
        min = tonumber(Trim(text):match("^=?%s*(%d+)$"))
        ok = Trim(text) == "" or min ~= nil
        RB.state.filters[filter.key] = min
    else
        min, max, ok = ParseRange(text)
        RB.state.filters[filter.min], RB.state.filters[filter.max] = min, max
    end
    RB.SetWellEdge(self, not ok and T.bad or (Trim(text) ~= "" and T.inputActive or T.inputBorder))
    FiltersChanged()
end

-- A cell's tip: its filter type's, titled with the column.
local function CellTip(self)
    local column = columnsNow[self.index]
    local tip = column and column.filter and RB.TIPS["filter_" .. column.filter.type]
    return tip and { (column.label or "") .. ": " .. tip[1], tip[2], key = tip.key }
end

-- The next (or previous) text cell after index, for Tab.
local function NextBox(index, step)
    for i = index + step, step > 0 and #columnsNow or 1, step do
        if boxes[i]:IsShown() then
            return boxes[i]
        end
    end
end

local function CreateBox(i)
    local box = RB.CreateCellBox(grid, "RetailBrowserFilterCell" .. i)
    box.index = i
    box:SetScript("OnTextChanged", BoxChanged)
    box:SetScript("OnEnterPressed", function(self) self:ClearFocus() RB.FocusGrid() end)
    box:SetScript("OnEscapePressed", function(self) self:ClearFocus() RB.FocusGrid() end)
    box:SetScript("OnTabPressed", function(self)
        local nextBox = NextBox(self.index, IsShiftKeyDown() and -1 or 1)
        if nextBox then nextBox:SetFocus() end
    end)
    box:HookScript("OnEditFocusGained", function(self)
        RB.SetKeyOwner(columnsNow[self.index] and columnsNow[self.index].filter.type == "search" and "search" or "filter")
    end)
    box:HookScript("OnEditFocusLost", function() RB.KeyOwnerLost() end)
    RB.AttachTip(box, CellTip)
    return box
end

local function ChoiceLabel(button)
    return (button.text and RB.Colored(button.text, T.value) or RB.Colored("Any", T.disabled)) .. " " .. RB.Colored("v", T.title)
end

local function ChoiceClicked(self)
    local column = columnsNow[self.index]
    local filter = column and column.filter
    if not filter then
        return
    end
    local choices = filter.choices
    if type(choices) == "function" then
        choices = choices(RB.info or {})
    end
    RB.ShowChoiceMenu(self, choices, self.value, self.valueKey, function(value, text, key)
        key = key or filter.key
        RB.state.filters[filter.key] = nil
        if self.valueKey then RB.state.filters[self.valueKey] = nil end
        if value ~= nil then
            RB.state.filters[key] = value
        end
        self.value, self.valueKey, self.text = value, value ~= nil and key or nil, value ~= nil and text or nil
        self:SetLabel(ChoiceLabel(self))
        RB.SetWellEdge(self, self.text and T.inputActive or T.inputBorder)
        FiltersChanged()
        RB.FocusGrid()
    end)
end

local function CreateChoice(i)
    local button = RB.CreateChoiceButton(grid, 40, ChoiceClicked)
    button.index = i
    RB.AttachTip(button, CellTip)
    return button
end

local function CreateFav(i)
    local button = RB.CreateChoiceButton(grid, 14, function()
        RB.ShowSaved("favourites")
        RB.SetSavedKindFilter(RB.lastKind)
    end)
    button:SetLabel(RB.Colored("*", T.title))
    button.index = i
    RB.AttachTip(button, "filter_favourites")
    return button
end

for i = 1, MAX_COLUMNS do
    boxes[i], choiceButtons[i], favButtons[i] = CreateBox(i), CreateChoice(i), CreateFav(i)
end

local HINTS = { search = "name or id", range = "min-max", exact = "=" }

local function ResetBox(box, filterType)
    suppressText = true
    box:SetText("")
    suppressText = false
    RB.SetHint(box, HINTS[filterType])
    RB.SetWellEdge(box, T.inputBorder)
end

-- Which cell each column shows, all cleared (a kind change starts without filters).
function RB.LayOutFilterCells(columns)
    columnsNow = columns
    RB.lastKind = RB.state.source == "search" and RB.state.kind or RB.lastKind
    for i = 1, MAX_COLUMNS do
        local box, choice, fav = boxes[i], choiceButtons[i], favButtons[i]
        box:Hide()
        choice:Hide()
        fav:Hide()
        local filter = columns[i] and columns[i].filter
        if filter and (filter.type == "search" or filter.type == "range" or filter.type == "exact") then
            box:ClearFocus()
            ResetBox(box, filter.type)
            box:Show()
        elseif filter and filter.type == "choice" then
            choice.value, choice.valueKey, choice.text = nil, nil, nil
            choice:SetLabel(ChoiceLabel(choice))
            RB.SetWellEdge(choice, T.inputBorder)
            choice:Show()
        elseif filter and filter.type == "favourites" and RB.state.source == "search" then
            fav:Show()
        end
    end
    RB.RefreshChips()
end

-- Puts the shown cells on top of their columns.
function RB.PlaceFilterCells(layout)
    for i = 1, MAX_COLUMNS do
        local place = layout[i]
        for _, cell in ipairs({ boxes[i], choiceButtons[i], favButtons[i] }) do
            if place and cell:IsShown() then
                cell:ClearAllPoints()
                cell:SetPoint("TOPLEFT", grid, "TOPLEFT", place.x + 1, CELL_TOP)
                cell:SetWidth(place.width - 2)
            end
        end
    end
end

-- The search box: the name cell.
function RB.SearchBox()
    for i, column in ipairs(columnsNow) do
        if column.filter and column.filter.type == "search" then
            return boxes[i]
        end
    end
end

function RB.SetSearchText(text)
    local box = RB.SearchBox()
    if box then
        box:SetText(text or "")
    end
end

function RB.FocusSearch(insert)
    local box = RB.SearchBox()
    if not box then
        return
    end
    box:SetFocus()
    if insert then
        box:Insert(insert)
    end
end

function RB.SetSavedKindFilter(kind)
    for i, column in ipairs(columnsNow) do
        if column.key == "kind" and kind then
            local button = choiceButtons[i]
            RB.state.filters.kind = kind
            button.value, button.valueKey, button.text = kind, "kind", RB.KindLabel(kind)
            button:SetLabel(ChoiceLabel(button))
            FiltersChanged()
        end
    end
end

-- Chips: one per active cell.
function RB.RefreshChips()
    local chips = {}
    for i, column in ipairs(columnsNow) do
        local filter = column.filter
        local box, choice = boxes[i], choiceButtons[i]
        if filter and box:IsShown() and Trim(box:GetText()) ~= "" then
            chips[#chips + 1] = { column = i, text = (column.label or "") .. (filter.type == "search" and " ~ " or " ") .. Trim(box:GetText()) }
        elseif filter and choice:IsShown() and choice.text then
            chips[#chips + 1] = { column = i, text = (column.label or "") .. " = " .. choice.text }
        end
    end
    RB.ShowChips(chips)
end

function RB.ClearColumnFilter(index)
    local column = columnsNow[index]
    local filter = column and column.filter
    -- The favourites cell is a button, not a filter: nothing to clear.
    if not filter or filter.type == "favourites" then
        return
    end
    if boxes[index]:IsShown() then
        boxes[index]:SetText("")
        return
    end
    local choice = choiceButtons[index]
    RB.state.filters[filter.key] = nil
    if choice.valueKey then RB.state.filters[choice.valueKey] = nil end
    choice.value, choice.valueKey, choice.text = nil, nil, nil
    choice:SetLabel(ChoiceLabel(choice))
    RB.SetWellEdge(choice, T.inputBorder)
    FiltersChanged()
end

function RB.ClearAllFilters()
    for i = 1, #columnsNow do
        RB.ClearColumnFilter(i)
    end
    RB.FocusGrid()
end
