-- The grid's columns per kind: header label, width (nil: the rest), alignment, the RetailSearch sort column, the filter
-- cell (search text, a range or exact number, or a choice; all go to RetailSearch, so they filter the whole index) and
-- the cell text of a row from its search row and its cached RetailDetails.

local RB = RetailBrowser
local S = RB.SORT

local WHITE = RB.THEME.text
local DIM = RB.THEME.label
local GOLD = RB.THEME.title
local VALUE = RB.THEME.value

local function Text(value, color)
    color = color or WHITE
    return tostring(value or ""), color[1], color[2], color[3]
end

local function Number(value)
    if not value or value == 0 then
        return Text("", DIM)
    end
    return Text(value, VALUE)
end

local function Choice(list, value)
    local text = RB.ChoiceText(list, value)
    if text == "" and value and value ~= 0 then
        text = tostring(value)
    end
    return Text(text, DIM)
end

-- A badge cell: served, custom or 3.3.5 for the first display a list has an installed row of, else the first's.
local function BadgeCell(list, itemDisplay)
    if not list or #list == 0 then
        return Text("", DIM)
    end
    for _, entry in ipairs(list) do
        if entry.custom then
            local badge = RB.BadgeOf(true, entry.displayId, itemDisplay)
            return Text(badge, RB.THEME.badge[badge])
        end
    end
    return Text("served", RB.THEME.badge.served)
end

local function FirstExtraRace(d)
    for _, display in ipairs(d.displays or {}) do
        if display.extraId and display.extraId > 0 then
            return display.race
        end
    end
end

local function ClassText(mask)
    if not mask or mask == 0 then
        return "All"
    end
    local names = {}
    for _, class in ipairs(RB.CLASSES) do
        if bit.band(mask, bit.lshift(1, class[1] - 1)) ~= 0 then
            names[#names + 1] = class[2]
        end
    end
    return #names <= 2 and table.concat(names, ", ") or (#names .. " classes")
end

local function TypeChoices(info)
    local choices = {}
    for _, armour in ipairs(RB.ARMOUR_TYPES) do
        choices[#choices + 1] = { armour[1], armour[2], "armour" }
    end
    for _, weapon in ipairs(RB.NamedChoices(info.weaponSubclasses)) do
        choices[#choices + 1] = { weapon[1], weapon[2], "weaponSubclass" }
    end
    return choices
end

local CHECK = { key = "check", width = 16, special = "check" }
local FAV = { key = "fav", label = "*", width = 16, align = "CENTER", filter = { type = "favourites" },
    cell = function(row) return Text(RB.IsFavourite(row.kind, row.id) and "*" or "", GOLD) end }
local function NameColumn(cell)
    return { key = "name", label = "Name", align = "LEFT", sort = S.name, filter = { type = "search" }, cell = cell }
end
local function IdColumn(label, width, value)
    return { key = "id", label = label, width = width or 58, align = "RIGHT", sort = S.id, filter = { type = "range", min = "minId", max = "maxId" },
        cell = function(row, d) return Text(value and value(row, d) or row.id, VALUE) end }
end
local function CountColumn(label, list)
    return { key = "count", label = label, width = 38, align = "RIGHT", sort = S.count, filter = { type = "range", min = "minCount", max = "maxCount" },
        cell = function(row, d)
            local count = #(d[list] or {})
            return Text(count, count > 1 and GOLD or VALUE)
        end }
end
local function DisplayColumn(label)
    return { key = "display", label = label or "Display", width = 62, align = "RIGHT", sort = S.display, filter = { type = "exact", key = "displayId" },
        cell = function(row) return Number(row.displayId) end }
end
local function ChoiceColumn(key, label, width, sort, filterKey, choices, cell)
    return { key = key, label = label, width = width, sort = sort, filter = { type = "choice", key = filterKey, choices = choices }, cell = cell }
end
local function SourceColumn(list, itemDisplay)
    return ChoiceColumn("source", "Source", 52, S.installed, "installed", RB.INSTALLED, function(row, d) return BadgeCell(d[list], itemDisplay) end)
end

RB.COLUMNS = {}

RB.COLUMNS.item = {
    CHECK,
    { key = "icon", width = 16, special = "icon" },
    NameColumn(function(row) return row.name, RB.QualityColor(row.quality) end),
    { key = "ilvl", label = "iLvl", width = 40, align = "RIGHT", sort = S.itemLevel, filter = { type = "range", min = "minItemLevel", max = "maxItemLevel" },
        cell = function(row, d) return Number(d.itemLevel) end },
    ChoiceColumn("slot", "Slot", 74, S.slot, "inventoryType", RB.INVENTORY_TYPES, function(row, d) return Choice(RB.INVENTORY_TYPES, d.inventoryType) end),
    ChoiceColumn("type", "Type", 70, S.subclass, "armour", TypeChoices, function(row, d) return Text(d.subclassName, DIM) end),
    ChoiceColumn("quality", "Quality", 62, S.quality, "quality", RB.QUALITIES, function(row)
        return RB.ChoiceText(RB.QUALITIES, row.quality), RB.QualityColor(row.quality)
    end),
    IdColumn("Item id"),
    DisplayColumn(),
    ChoiceColumn("exp", "Exp", 48, S.expansion, "expansion", RB.EXPANSIONS, function(row, d) return Choice(RB.EXPANSIONS, d.expansion) end),
    SourceColumn("appearances", true),
    FAV,
}

RB.COLUMNS.creature = {
    CHECK,
    NameColumn(function(row, d) return row.name, RB.RankColor(RB.RankGroup(d.classification)) end),
    IdColumn("Creature id", 68),
    CountColumn("Disp", "displays"),
    DisplayColumn("Display 1"),
    ChoiceColumn("type", "Type", 74, S.type, "creatureType", function(info) return RB.NamedChoices(info.creatureTypes) end,
        function(row, d) return Text(d.typeName, DIM) end),
    ChoiceColumn("rank", "Rank", 62, S.rank, "classification", RB.CLASSIFICATIONS, function(row, d)
        local group = RB.RankGroup(d.classification)
        return RB.ChoiceText(RB.CLASSIFICATIONS, group), RB.RankColor(group)
    end),
    ChoiceColumn("race", "Race", 70, S.race, "race", RB.RACES, function(row, d) return Choice(RB.RACES, FirstExtraRace(d)) end),
    ChoiceColumn("family", "Family", 70, S.family, "family", function(info) return RB.NamedChoices(info.families) end,
        function(row, d) return Text(d.familyName, DIM) end),
    ChoiceColumn("kind", "Kind", 46, S.kind, "humanoid", RB.NPC_KINDS, function(row, d) return Text(FirstExtraRace(d) and "NPC" or "Model", DIM) end),
    SourceColumn("displays", false),
    FAV,
}

RB.COLUMNS.set = {
    CHECK,
    NameColumn(function(row) return Text(row.name) end),
    IdColumn("Set id", 58, function(row, d) return d.setId end),
    ChoiceColumn("setSource", "Source", 64, S.kind, "setSource", RB.SET_SOURCES, function(row, d)
        return Text(d.source == "itemset" and "Item set" or "Transmog", DIM)
    end),
    CountColumn("Pieces", "pieces"),
    ChoiceColumn("armour", "Armour", 62, S.subclass, "armour", RB.ARMOUR_TYPES, function(row, d) return Choice(RB.ARMOUR_TYPES, d.armour) end),
    ChoiceColumn("class", "Class", 110, nil, "classId", RB.CLASSES, function(row, d) return Text(ClassText(d.classMask), DIM) end),
    ChoiceColumn("exp", "Exp", 48, S.expansion, "expansion", RB.EXPANSIONS, function(row, d) return Choice(RB.EXPANSIONS, d.expansion) end),
    DisplayColumn("Display 1"),
    SourceColumn("pieces", true),
    FAV,
}

RB.COLUMNS.model = {
    CHECK,
    NameColumn(function(row) return Text(row.name) end),
    IdColumn("Model fdid", 74),
    ChoiceColumn("modelKind", "Kind", 64, S.kind, "modelKind", RB.MODEL_KINDS, function(row, d)
        return Text(d.creature and "Creature" or "Item", DIM)
    end),
    { key = "count", label = "Users", width = 46, align = "RIGHT", sort = S.count, filter = { type = "range", min = "minCount", max = "maxCount" },
        cell = function(row, d) return Text(d.userCount, (d.userCount or 0) > 1 and GOLD or VALUE) end },
    DisplayColumn("Display 1"),
    FAV,
}

RB.COLUMNS.mount = {
    CHECK,
    NameColumn(function(row) return Text(row.name) end),
    IdColumn("Mount id"),
    { key = "spell", label = "Spell", width = 62, align = "RIGHT", cell = function(row, d) return Number(d.spellId) end },
    ChoiceColumn("mountType", "Type", 74, S.kind, "mountType", RB.MOUNT_TYPES, function(row, d) return Choice(RB.MOUNT_TYPES, d.mountType) end),
    CountColumn("Disp", "displays"),
    DisplayColumn("Display 1"),
    SourceColumn("displays", false),
    FAV,
}

-- Favourites and History: every kind at once, filtered and sorted in Lua (the saved lists are small and complete).
RB.COLUMNS.saved = {
    CHECK,
    { key = "name", label = "Name", align = "LEFT", sort = "name", filter = { type = "search" }, cell = function(row) return Text(row.name) end },
    { key = "kind", label = "Kind", width = 80, sort = "kind", filter = { type = "choice", key = "kind", choices = RB.SAVED_KINDS },
        cell = function(row) return Text(RB.KindLabel(row.kind), DIM) end },
    { key = "id", label = "Id", width = 80, align = "RIGHT", sort = "id", filter = { type = "range", min = "minId", max = "maxId" },
        cell = function(row) return Text(row.id, VALUE) end },
    FAV,
}

-- The columns the grid shows now.
function RB.ColumnsNow()
    if RB.state.source ~= "search" then
        return RB.COLUMNS.saved
    end
    return RB.COLUMNS[RB.state.kind]
end

-- The cell texts and colours of a row, computed once (scrolling only sets them).
function RB.CellsOf(row, columns)
    if row.cells and row.cellColumns == columns then
        return row.cells
    end
    local d = row.d or {}
    local cells = {}
    for i, column in ipairs(columns) do
        if column.cell then
            local text, r, g, b = column.cell(row, d)
            cells[i] = { text or "", r or WHITE[1], g or WHITE[2], b or WHITE[3] }
        end
    end
    row.cells, row.cellColumns = cells, columns
    return cells
end
