-- The inspector's list tabs, each a scrolling List.lua mini grid: Displays (every display, appearance, piece or user;
-- the picked one drives the preview and every action), Used by (what else uses the picked display's model; click opens
-- it), Stats (items: every stat line) and Raw (every field RetailDetails labels; click copies).

local RB = RetailBrowser

local function Badge(custom, displayId, itemDisplay)
    local badge = RB.BadgeOf(custom, displayId, itemDisplay)
    return RB.Colored(badge, RB.THEME.badge[badge])
end

-- Displays tab.
local DISPLAY_COLUMNS = {
    creature = { { "#", 22 }, { "Display", 58, "RIGHT" }, { "Prob", 40, "RIGHT" }, { "Model fdid", 70, "RIGHT" }, { "Model", 46, "RIGHT" },
        { "Scale", 38, "RIGHT" }, { "NPC Extra", 62, "RIGHT" }, { "Race / sex" }, { "Source", 52 } },
    item = { { "#", 22 }, { "Display", 58, "RIGHT" }, { "Appear.", 54, "RIGHT" }, { "Mod. app.", 58, "RIGHT" }, { "Modifier", 50, "RIGHT" },
        { "Model 1", 64, "RIGHT" }, { "Model 2", 64, "RIGHT" }, { "Icon" }, { "Source", 52 } },
    set = { { "#", 22 }, { "Item id", 58, "RIGHT" }, { "Name" }, { "Slot", 70 }, { "Display", 58, "RIGHT" }, { "Mod. app.", 58, "RIGHT" }, { "Source", 52 } },
    model = { { "#", 22 }, { "Kind", 60 }, { "Id", 64, "RIGHT" }, { "Name" }, { "Display", 64, "RIGHT" } },
}
DISPLAY_COLUMNS.mount = DISPLAY_COLUMNS.creature

local function DisplayCells(kind, index, e)
    if kind == "item" then
        local models = e.models or {}
        return { index, e.displayId, e.appearanceId, e.modifiedAppearanceId, e.modifier, models[1] or "", models[2] or "", e.icon, Badge(e.custom, e.displayId, true) }
    elseif kind == "set" then
        return { index, e.itemId, e.name, RB.ChoiceText(RB.INVENTORY_TYPES, e.inventoryType), e.displayId, e.modifiedAppearanceId, Badge(e.custom, e.displayId, true) }
    elseif kind == "model" then
        return { index, e.kind, e.id, e.name, e.displayId }
    end
    local race = e.extraId > 0 and (RB.ChoiceText(RB.RACES, e.race) .. (e.sex == 1 and " F" or " M") .. "  bakes " .. e.sdBake .. "/" .. e.hdBake) or "plain model"
    return { index, RB.Colored(e.displayId, RB.THEME.value), string.format("%g%%", e.probability * 100), e.modelFdid, e.modelId,
        string.format("%.2f", e.scale), e.extraId > 0 and e.extraId or "", race, Badge(e.custom, e.displayId, false) }
end

local displays = RB.CreateList("Displays", function(entry, index)
    if entry and IsShiftKeyDown() and entry.open then
        RB.OpenEntry(entry.open.kind, entry.open.id)
    elseif entry then
        RB.PickDisplay(index)
        RB.FocusGrid()
    end
end, "list_displays")

-- The cell that holds a row's display id, labelled green on the picked row.
local DISPLAY_CELL = { creature = 2, mount = 2, item = 2, set = 5, model = 5 }

local function RefreshDisplays(d)
    if not d then
        return
    end
    local list = RB.PickList(d)
    -- A model lists every creature display drawn with it (as a creature does), or its users when none is.
    local kind = d.kind == "model" and RB.IsDisplayRow(d, list[1]) and "creature" or d.kind
    local _, picked = RB.Picked(d)
    local entries = {}
    for i, e in ipairs(list) do
        local cells = DisplayCells(kind, i, e)
        if i == picked and (d.kind ~= "model" or not d.creature or e.displayId == RB.ActionDisplay(d)) then
            local at = DISPLAY_CELL[kind]
            cells[1], cells[at] = RB.Colored(i, RB.THEME.picked), RB.Colored(e.displayId or 0, RB.THEME.picked)
        end
        entries[i] = { cells = cells, open = e.kind and { kind = e.kind, id = e.id } or nil }
    end
    local hint = kind == "model" and "click picks, Shift+click opens" or "Left/Right or Alt+1..4 pick; the green one drives the preview and every action"
    RB.ShowListEntries(displays, DISPLAY_COLUMNS[kind], entries, hint, picked)
end

local PICK_LABELS = { creature = "Displays", mount = "Displays", item = "Appearances", set = "Pieces" }
local SHORT_LABELS = { Displays = "Disp.", Appearances = "Looks" }

local function PickLabel(kind, d)
    return PICK_LABELS[kind] or (d and not RB.IsDisplayRow(d, RB.PickList(d)[1]) and "Users") or "Displays"
end

RB.AddInspectorTab("displays", PickLabel, displays.panel, RefreshDisplays, { order = 3,
    short = function(kind, d) return SHORT_LABELS[PickLabel(kind, d)] end,
    count = function(kind, d) return #RB.PickList(d) end })

-- Used by: the users of the picked display's model.
local function PickedModel(d)
    local picked = RB.Picked(d)
    if not picked then
        return nil
    end
    if d.kind == "item" then
        return picked.models and picked.models[1]
    elseif d.kind == "model" then
        return d.fdid
    end
    return picked.modelFdid
end

local usedBy = RB.CreateList("UsedBy", function(entry)
    if entry then RB.OpenEntry(entry.kind, entry.id) end
end, "list_usedby")

local function RefreshUsedBy(d)
    local fdid = d and PickedModel(d)
    local model = fdid and fdid > 0 and RB.DetailsOf("model", fdid)
    local entries = {}
    for i, user in ipairs(model and model.users or {}) do
        entries[i] = { kind = user.kind, id = user.id, cells = { RB.KindLabel(user.kind), user.id, user.name, user.displayId } }
    end
    local title = model and string.format("model %d: %d users%s, click opens one", fdid, model.userCount, model.userCount > #entries and (" (" .. #entries .. " listed)") or "")
        or "the picked display has no retail model"
    RB.ShowListEntries(usedBy, { { "Kind", 70 }, { "Id", 70, "RIGHT" }, { "Name" }, { "Display", 70, "RIGHT" } }, entries, title)
end

RB.AddInspectorTab("usedby", "Used by", usedBy.panel, RefreshUsedBy, { order = 4, kinds = { creature = true, item = true, mount = true } })

-- Stats (items): the ItemSparse stat lines, sockets, weapon and level fields.
local stats = RB.CreateList("Stats", function(entry)
    if entry then RB.Copy(entry.copy, entry.cells[1] .. " (" .. entry.copy .. ")") end
end, "list_field")

local STAT_LABELS = { Sockets = true, Weapon = true, ["Required level"] = true, Scales = true, Stats = true, Armour = true, Damage = true }

local function FieldEntries(d, keep)
    local entries = {}
    for _, field in ipairs(d and d.fields or {}) do
        if not keep or keep(field.label) then
            entries[#entries + 1] = { copy = field.copy or field.value, cells = { field.label, field.value } }
        end
    end
    return entries
end

RB.AddInspectorTab("stats", "Stats", stats.panel, function(d)
    local entries = FieldEntries(d, function(label) return STAT_LABELS[label] or label:match("^Stat %d+$") end)
    RB.ShowListEntries(stats, { { "Field", 120 }, { "Value" } }, entries, "retail values at the item's own level; click copies")
end, { order = 2, kinds = { item = true }, count = function(kind, d) return #(d and d.stats or {}) end })

-- Raw: every labelled field.
local raw = RB.CreateList("Raw", function(entry)
    if entry then RB.Copy(entry.copy, entry.cells[1] .. " (" .. entry.copy .. ")") end
end, "list_field")

RB.AddInspectorTab("raw", "Raw", raw.panel, function(d)
    RB.ShowListEntries(raw, { { "Field", 150 }, { "Value" } }, FieldEntries(d), "click copies a field; Ctrl+Shift+C in the grid copies them all")
end, { order = 6, count = function(kind, d) return #(d and d.fields or {}) end })
