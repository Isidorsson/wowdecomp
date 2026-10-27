-- The Overview tab's content as plain tables, built from a RetailDetails table without touching a frame (Tests/ runs
-- it). RB.OverviewSections(d) returns the sections top to bottom:
--   { title, hint, kind = "kv", rows = { { label, shown value, copied value }, ... } }
--   { title, hint, kind = "list", columns = { { label, width, align } }, entries = { { cells, pick | open, dim } },
--     focused, more }
--   { title, hint, kind = "chips", chips = { { text, on } } }
--   { kind = "tooltip", lines = { { left, right, color, rightColor } } }   (an item drawn the way the game shows one)
-- InspectorOverview.lua draws them.

local RB = RetailBrowser
local T = RB.THEME

local MODEL_SLOTS = 4
local LIST_LIMIT = 15
local SOCKET_NAMES = { "Meta", "Red", "Yellow", "Blue", "Hydraulic", "Cogwheel", "Prismatic" }

local function Named(name, id)
    if not id or id == 0 then
        return "", nil
    end
    return name and name ~= "" and (name .. " (" .. id .. ")") or tostring(id), tostring(id)
end

local function Badge(badge)
    local color = T.badge[badge or ""]
    if not color then
        return ""
    end
    return " " .. RB.Colored(badge, color)
end

local function BadgeCell(custom, displayId, itemDisplay)
    local badge = RB.BadgeOf(custom, displayId, itemDisplay)
    return RB.Colored(badge, T.badge[badge])
end

-- A row: label, shown value (may hold colour codes) and the text a click copies (the value when omitted).
local function Row(label, value, copy)
    value = value ~= nil and tostring(value) or ""
    return { label, value, copy or (value ~= "" and value:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", "") or nil) }
end

local function Kv(title, hint, rows)
    return { title = title, hint = hint, kind = "kv", rows = rows }
end

local function Field(d, label)
    for _, field in ipairs(d.fields or {}) do
        if field.label == label then
            return field
        end
    end
end

local function RaceSex(p)
    return p.extraId and p.extraId > 0 and (RB.ChoiceText(RB.RACES, p.race) .. " " .. (p.sex == 1 and "F" or "M")) or "plain model"
end

local function Percent(probability)
    return string.format("%g%%", (probability or 0) * 100)
end

-- Creature, mount and model displays: the picked one's facts.
local function PickedDisplayRows(d, p)
    local count = #(d.displays or {})
    local _, index = RB.Picked(d)
    return {
        Row("Picked", count > 0 and (index .. " / " .. count) or "none"),
        Row("Probability", p and Percent(p.probability) or ""),
        Row("Scale", p and string.format("%g", p.scale or 1) or ""),
        Row("Race / sex", p and RaceSex(p) or ""),
        Row("Source", p and Badge(RB.PickedBadge(d)) or "", p and RB.PickedBadge(d)),
        Row("Known", p and (p.known and "in the retail tables" or RB.Colored("not in the retail tables", T.bad)) or ""),
    }
end

-- A list section of at most LIST_LIMIT entries; the rest is a "+N more" line pointing at the tab that has them all.
local function List(title, hint, columns, entries, focused, tab)
    local more
    if #entries > LIST_LIMIT then
        more = string.format("+%d more, see %s", #entries - LIST_LIMIT, tab)
        for i = #entries, LIST_LIMIT + 1, -1 do
            entries[i] = nil
        end
    end
    return { title = title, hint = hint, kind = "list", columns = columns, entries = entries, focused = focused, more = more }
end

-- Creature.

local SLOT_COLUMNS = { { "Slot", 62 }, { "Display", 56, "RIGHT" }, { "Prob.", 42, "RIGHT" }, { "Scale", 38, "RIGHT" },
    { "Race / sex" }, { "Source", 50 }, { "Known", 48 } }

local function ModelSlots(d)
    local displays = d.displays or {}
    local _, picked = RB.Picked(d)
    local entries = {}
    for slot = 1, MODEL_SLOTS do
        local e = displays[slot]
        if e then
            entries[slot] = { pick = slot, cells = { "modelid" .. slot, e.displayId, Percent(e.probability), string.format("%g", e.scale or 1),
                RaceSex(e), BadgeCell(e.custom, e.displayId, false), e.known and "yes" or RB.Colored("no", T.bad) } }
        else
            entries[slot] = { dim = true, cells = { "modelid" .. slot, "-", "", "", "", "", "empty" } }
        end
    end
    local section = List("Model slots", "click picks", SLOT_COLUMNS, entries, picked <= MODEL_SLOTS and picked or nil)
    if #displays > MODEL_SLOTS then
        section.more = string.format("+%d more, see Displays. 3.3.5 has four slots: Build's modelid1-4 variants keep the "
            .. "picked display and the next three; One creature each writes a row per display.", #displays - MODEL_SLOTS)
    end
    return section
end

local function CreatureSections(d)
    local p = RB.Picked(d) or {}
    return {
        Kv("Ids", "click: copy", {
            Row("Creature id", d.creatureId),
            Row("Display", RB.Colored(p.displayId or 0, T.picked) .. Badge(RB.PickedBadge(d)), tostring(p.displayId or 0)),
            Row("Model fdid", p.modelFdid), Row("Model id", p.modelId), Row("NPC Extra", p.extraId ~= 0 and p.extraId or "none"),
            Row("Bakes sd / hd", p.extraId and p.extraId > 0 and (p.sdBake .. " / " .. p.hdBake) or ""),
        }),
        ModelSlots(d),
        Kv("Creature", nil, {
            Row("Name", d.name), Row("Title", d.title), Row("Type", Named(d.typeName, d.type)), Row("Family", Named(d.familyName, d.family)),
            Row("Rank", RB.ChoiceText(RB.CLASSIFICATIONS, RB.RankGroup(d.classification)) .. " (" .. d.classification .. ")", tostring(d.classification)),
        }),
        Kv("Picked display", "Left/Right picks", PickedDisplayRows(d, RB.Picked(d))),
    }
end

-- Item: the tooltip.

local function Line(left, color, right, rightColor)
    return { left = left, color = color or T.text, right = right, rightColor = rightColor or T.text }
end

local function CombatLines(d, lines)
    local damage, weapon, armour = Field(d, "Damage"), Field(d, "Weapon"), Field(d, "Armour")
    if damage then
        local low, high, dps = damage.value:match("^(%d+) %- (%d+) %(([%d%.]+) dps%)")
        local delay = weapon and tonumber(weapon.copy)
        lines[#lines + 1] = Line((low or "?") .. " - " .. (high or "?") .. " Damage", nil, delay and string.format("Speed %.2f", delay / 1000))
        lines[#lines + 1] = Line("(" .. (dps or "?") .. " damage per second)")
    elseif armour then
        lines[#lines + 1] = Line(armour.copy .. " Armor")
    end
end

local function StatLines(d, lines)
    if not d.stats then
        lines[#lines + 1] = Line("no ItemSparse row: stats unknown", T.label)
        return
    end
    for _, stat in ipairs(d.stats) do
        if stat.value then
            lines[#lines + 1] = Line("+" .. stat.value .. " " .. stat.name)
        else
            lines[#lines + 1] = Line(string.format("%s (%.1f%% of the budget)", stat.name, (stat.percent or 0) / 100), T.label)
        end
    end
end

local function SocketLines(d, lines)
    local sockets = Field(d, "Sockets")
    local list = sockets and sockets.value:match("^types ([%d, ]+)")
    for socket in (list or ""):gmatch("%d+") do
        local name = SOCKET_NAMES[tonumber(socket)]
        lines[#lines + 1] = Line(name and (name .. " Socket") or ("Socket (type " .. socket .. ")"), T.label)
    end
end

local function TooltipLines(d)
    local lines = { Line(d.name or "", T.quality[d.quality] or T.quality[1]) }
    local slot = RB.ChoiceText(RB.INVENTORY_TYPES, d.inventoryType)
    if slot ~= "" or (d.subclassName or "") ~= "" then
        lines[#lines + 1] = Line(slot, nil, d.subclassName)
    end
    CombatLines(d, lines)
    StatLines(d, lines)
    SocketLines(d, lines)
    if d.requiredLevel and d.requiredLevel > 1 then
        lines[#lines + 1] = Line("Requires Level " .. d.requiredLevel)
    end
    lines[#lines + 1] = Line("Item Level " .. (d.sparseItemLevel or d.itemLevel or 0), T.itemLevel)
    return lines
end

local function ItemSections(d)
    local a = RB.Picked(d) or {}
    local models = a.models or {}
    local weapon, sockets, scales = Field(d, "Weapon"), Field(d, "Sockets"), Field(d, "Scales")
    return {
        { kind = "tooltip", lines = TooltipLines(d) },
        Kv("Ids", "click: copy", {
            Row("Item id", d.itemId), Row("IDI", (a.displayId or 0) .. Badge(RB.PickedBadge(d)), tostring(a.displayId or 0)),
            Row("Appearance", a.appearanceId), Row("IMA", a.modifiedAppearanceId), Row("Icon fdid", d.icon),
            Row("Model fdid 1 / 2", (models[1] or 0) .. " / " .. (models[2] or 0), tostring(models[1] or "")),
        }),
        Kv("Item", nil, {
            Row("Quality", d.quality .. " " .. RB.ChoiceText(RB.QUALITIES, d.quality), tostring(d.quality)),
            Row("Level / req", d.itemLevel .. " / " .. (d.requiredLevel or "?"), tostring(d.itemLevel)),
            Row("Slot", d.inventoryType .. " " .. RB.ChoiceText(RB.INVENTORY_TYPES, d.inventoryType), tostring(d.inventoryType)),
            Row("Class / sub", d.classId .. " / " .. d.subclassId .. " " .. (d.subclassName or ""), tostring(d.subclassId)),
            Row("Material", d.material), Row("Expansion", RB.ChoiceText(RB.EXPANSIONS, d.expansion), tostring(d.expansion)),
            Row("Classes", d.allowableClass), Row("Sheathe", d.sheatheType),
            Row("Weapon delay", weapon and (weapon.copy .. " ms") or "", weapon and weapon.copy),
            Row("Sockets", sockets and sockets.value or ""), Row("Scales", scales and scales.copy or "no"),
        }),
    }
end

-- Set.

local PIECE_COLUMNS = { { "Item", 54, "RIGHT" }, { "Name" }, { "Slot", 64 }, { "IDI", 52, "RIGHT" }, { "IMA", 52, "RIGHT" }, { "Source", 50 } }

-- Whether class id's bit is set; a mask of 0 or less means every class.
local function HasClass(mask, classId)
    return not mask or mask <= 0 or math.floor(mask / 2 ^ (classId - 1)) % 2 == 1
end

local function ClassChips(d)
    local chips = {}
    for i, entry in ipairs(RB.CLASSES) do
        chips[i] = { text = entry[2], on = HasClass(d.classMask, entry[1]) }
    end
    return { title = "Classes", hint = (d.classMask or 0) <= 0 and "any class" or nil, kind = "chips", chips = chips }
end

local function SetSections(d)
    local _, picked = RB.Picked(d)
    local entries = {}
    for i, p in ipairs(d.pieces or {}) do
        entries[i] = { pick = i, cells = { p.itemId, p.name, RB.ChoiceText(RB.INVENTORY_TYPES, p.inventoryType), p.displayId,
            p.modifiedAppearanceId, BadgeCell(p.custom, p.displayId, true) } }
    end
    return {
        Kv("Ids", "click: copy", {
            Row(d.source == "itemset" and "ItemSet id" or "TransmogSet id", d.setId), Row("Search key", d.id), Row("Class mask", d.classMask),
        }),
        Kv("Set", nil, {
            Row("Name", d.name), Row("Source", d.source == "itemset" and "Item set" or "Transmog", d.source),
            Row("Armour", RB.ChoiceText(RB.ARMOUR_TYPES, d.armour), tostring(d.armour)),
            Row("Expansion", RB.ChoiceText(RB.EXPANSIONS, d.expansion), tostring(d.expansion)), Row("Pieces", #(d.pieces or {})),
        }),
        ClassChips(d),
        List("Pieces", "click picks", PIECE_COLUMNS, entries, picked, "Pieces"),
    }
end

-- Model.

local USER_COLUMNS = { { "Kind", 60 }, { "Id", 64, "RIGHT" }, { "Name" }, { "Display", 64, "RIGHT" } }

local function ModelSections(d)
    local ids = { Row("Model fdid", d.fdid), Row("Kind", d.creature and "creature model" or "item model"), Row("Users", d.userCount),
        Row("Displays", d.displayCount or #(d.displays or {})) }
    local action = d.creature and RB.ActionDisplay(d)
    if action then
        ids[#ids + 1] = Row("Display", RB.Colored(action, T.picked), tostring(action))
    end
    local entries = {}
    for i, user in ipairs(d.users or {}) do
        entries[i] = { open = { kind = user.kind, id = user.id }, cells = { RB.KindLabel(user.kind), user.id, user.name, user.displayId } }
    end
    local listed = d.userCount and d.userCount > #entries and string.format("%d of %d listed; ", #entries, d.userCount) or ""
    return {
        Kv("Ids", "click: copy", ids),
        List("Users", listed .. "click opens one", USER_COLUMNS, entries, nil, "Users"),
    }
end

-- Mount.

local DISPLAY_COLUMNS = { { "#", 22 }, { "Display", 56, "RIGHT" }, { "Prob.", 42, "RIGHT" }, { "Model fdid", 70, "RIGHT" },
    { "Scale", 38, "RIGHT" }, { "Race / sex" }, { "Source", 50 } }

local function MountSections(d)
    local p, picked = RB.Picked(d)
    p = p or {}
    local entries = {}
    for i, e in ipairs(d.displays or {}) do
        entries[i] = { pick = i, cells = { i, e.displayId, Percent(e.probability), e.modelFdid, string.format("%g", e.scale or 1), RaceSex(e),
            BadgeCell(e.custom, e.displayId, false) } }
    end
    return {
        Kv("Ids", "click: copy", {
            Row("Mount id", d.mountId), Row("Spell id", d.spellId),
            Row("Display", RB.Colored(p.displayId or 0, T.picked) .. Badge(RB.PickedBadge(d)), tostring(p.displayId or 0)),
            Row("Model fdid", p.modelFdid), Row("Model id", p.modelId),
        }),
        Kv("Mount", nil, { Row("Name", d.name), Row("Type", Named(RB.ChoiceText(RB.MOUNT_TYPES, d.mountType), d.mountType)),
            Row("Displays", #(d.displays or {})) }),
        List("Displays", "click picks", DISPLAY_COLUMNS, entries, picked, "Displays"),
    }
end

local BUILDERS = { creature = CreatureSections, item = ItemSections, set = SetSections, model = ModelSections, mount = MountSections }

function RB.OverviewSections(d)
    local build = d and BUILDERS[d.kind]
    return build and build(d) or {}
end

-- The header's sub-line: the singular kind and id, then its facts, joined with " · " (only the source is coloured).
local function Plural(count, word)
    return count .. " " .. word .. (count == 1 and "" or "s")
end

local function Source(d)
    local badge = RB.PickedBadge(d)
    return badge and RB.Colored(badge, T.badge[badge])
end

local function CreatureFacts(d)
    local p = RB.Picked(d)
    return { "Creature " .. d.creatureId, d.typeName, RB.ChoiceText(RB.CLASSIFICATIONS, RB.RankGroup(d.classification)),
        p and (p.extraId > 0 and RB.ChoiceText(RB.RACES, p.race) .. " NPC" or "plain model"), Source(d) }
end

local function ItemFacts(d)
    local slot = RB.ChoiceText(RB.INVENTORY_TYPES, d.inventoryType)
    local look = (slot .. " " .. (d.subclassName or "")):match("^%s*(.-)%s*$")
    return { "Item " .. d.itemId, look, RB.ChoiceText(RB.EXPANSIONS, d.expansion), Source(d) }
end

local function SetFacts(d)
    return { (d.source == "itemset" and "Item set " or "Transmog set ") .. d.setId, RB.ChoiceText(RB.ARMOUR_TYPES, d.armour),
        RB.ChoiceText(RB.EXPANSIONS, d.expansion), Plural(#(d.pieces or {}), "piece") }
end

local function ModelFacts(d)
    return { "Model fdid " .. d.fdid, d.creature and "creature model" or "item model", Plural(d.userCount or 0, "user") }
end

local function MountFacts(d)
    return { "Mount " .. d.mountId, "spell " .. d.spellId, RB.ChoiceText(RB.MOUNT_TYPES, d.mountType) }
end

local FACTS = { creature = CreatureFacts, item = ItemFacts, set = SetFacts, model = ModelFacts, mount = MountFacts }

function RB.HeaderFacts(d)
    local facts = d and FACTS[d.kind]
    if not facts then
        return ""
    end
    local parts = {}
    -- A fixed count: a missing fact is a nil hole, which ipairs would stop at.
    local list = facts(d)
    for i = 1, 5 do
        if list[i] and list[i] ~= "" then
            parts[#parts + 1] = list[i]
        end
    end
    return table.concat(parts, " · ")
end

-- The band's text when the picked display's installed DBC row wins over retail's; nil when retail's is served.
function RB.WarningText(d)
    local badge = d and RB.PickedBadge(d)
    if badge ~= "custom" and badge ~= "3.3.5" then
        return nil
    end
    local picked = RB.Picked(d)
    local dbc = (d.kind == "item" or d.kind == "set") and "ItemDisplayInfo.dbc" or "CreatureDisplayInfo.dbc"
    if badge == "3.3.5" then
        return string.format("Stock 3.3.5 row wins: display %d is in your %s, so the client draws that, not retail's.",
            picked.displayId or 0, dbc)
    end
    return string.format("Custom row wins: display %d is a row your server added to %s, so the client draws that, not "
        .. "retail's.", picked.displayId or 0, dbc)
end
