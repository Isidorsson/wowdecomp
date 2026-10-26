-- The drawer's Details tab: the entry's ids and facts in three groups (ids first). Click a value to copy it;
-- Shift+click adds it to the copy buffer and copies the buffer (comma-separated).

local RB = RetailBrowser

local GROUPS = 3
local ROWS = 8
local ROW_HEIGHT = 13
local LABEL_WIDTH = 78
local LABEL_SHARE = 0.45
local HINT_MIN_WIDTH = 200
local STAT_NAME_LENGTH = 15

local panel = CreateFrame("Frame")
local groups = {}
local copyBuffer = {}

local function ValueClicked(self)
    if not self.copy then
        return
    end
    if IsShiftKeyDown() then
        copyBuffer[#copyBuffer + 1] = self.copy
        RB.Copy(table.concat(copyBuffer, ", "), #copyBuffer .. " values: " .. table.concat(copyBuffer, ", "))
        return
    end
    wipe(copyBuffer)
    RB.Copy(self.copy, self.labelText .. " (" .. self.copy .. ")")
end

-- A value's tip: where it comes from (a stat line otherwise), and that a click copies it.
local function FieldTip(self)
    local where = RB.FIELD_TIPS[self.labelText or ""] or RB.FIELD_TIPS.stat
    return { self.labelText, { where, "Click copies the value; Shift+click adds it to the copy buffer." } }
end

local function CreateGroup(i)
    local group = RB.CreatePanel(panel, "well")
    group.title = RB.CreateText(group, "RetailBrowserHeadFont")
    group.title:SetPoint("TOPLEFT", 5, -3)
    group.hint = RB.CreateText(group, "RetailBrowserLabelFont", "RIGHT")
    group.hint:SetPoint("TOPRIGHT", -5, -3)
    local titleArea = CreateFrame("Button", nil, group)
    titleArea:SetPoint("TOPLEFT", 2, -1)
    titleArea:SetPoint("TOPRIGHT", -2, -1)
    titleArea:SetHeight(14)
    RB.AttachTip(titleArea, function()
        local first = (group.title:GetText() or ""):match("^(%a+)")
        return first and RB.TIPS["group_" .. first:lower()]
    end)
    group.rows = {}
    for r = 1, ROWS do
        local row = CreateFrame("Button", nil, group)
        row:SetHeight(ROW_HEIGHT)
        row:SetPoint("TOPLEFT", 3, -15 - (r - 1) * ROW_HEIGHT)
        row:SetPoint("RIGHT", -3, 0)
        row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
        row.label = RB.CreateText(row, "RetailBrowserLabelFont")
        row.label:SetPoint("LEFT", 2, 0)
        row.label:SetWidth(LABEL_WIDTH)
        row.label:SetHeight(ROW_HEIGHT)
        row.value = RB.CreateText(row, "RetailBrowserValueFont")
        row.value:SetPoint("LEFT", row.label, "RIGHT", 2, 0)
        row.value:SetPoint("RIGHT", -2, 0)
        row.value:SetHeight(ROW_HEIGHT)
        row:SetScript("OnClick", ValueClicked)
        RB.AttachTip(row, FieldTip)
        group.rows[r] = row
    end
    return group
end

for i = 1, GROUPS do
    groups[i] = CreateGroup(i)
    if i == 1 then
        groups[i]:SetPoint("TOPLEFT", panel, "TOPLEFT", 0, 0)
    else
        groups[i]:SetPoint("TOPLEFT", groups[i - 1], "TOPRIGHT", 4, 0)
    end
    groups[i]:SetPoint("BOTTOM", panel, "BOTTOM", 0, 0)
end

panel:SetScript("OnSizeChanged", function(self, width)
    local each = (width - 4 * (GROUPS - 1)) / GROUPS
    for _, group in ipairs(groups) do
        group:SetWidth(each)
        RB.Shown(group.hint, each >= HINT_MIN_WIDTH)
        for _, row in ipairs(group.rows) do
            row.label:SetWidth(math.min(LABEL_WIDTH, each * LABEL_SHARE))
        end
    end
end)

local function Named(name, id)
    if not id or id == 0 then
        return "", nil
    end
    return name and name ~= "" and (name .. " (" .. id .. ")") or tostring(id), tostring(id)
end

local function Badge(badge)
    local color = RB.THEME.badge[badge or ""]
    if not color then
        return ""
    end
    return " " .. RB.Colored(badge, color)
end

-- A row: label, shown value (may hold colour codes) and the text a click copies (the value when omitted).
local function Row(label, value, copy)
    value = value ~= nil and tostring(value) or ""
    return { label, value, copy or (value ~= "" and value:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", "") or nil) }
end

local function DisplayRows(d, p)
    local count = #(d.displays or {})
    local _, index = RB.Picked(d)
    local rows = {
        Row("Picked", count > 0 and (index .. " / " .. count) or "none"),
        Row("Probability", p and string.format("%g%%", (p.probability or 0) * 100) or ""),
        Row("Scale", p and string.format("%g", p.scale or 1) or ""),
        Row("Race / sex", p and p.extraId > 0 and (RB.ChoiceText(RB.RACES, p.race) .. " " .. (p.sex == 1 and "F" or "M")) or "plain model"),
        Row("Source", p and Badge(RB.PickedBadge(d)) or "", p and RB.PickedBadge(d)),
        Row("Known", p and (p.known and "in the retail tables" or RB.Colored("not in the retail tables", RB.THEME.bad)) or ""),
    }
    return rows
end

local function CreatureGroups(d)
    local p = RB.Picked(d) or {}
    return {
        { "Ids", "click: copy", {
            Row("Creature id", d.creatureId), Row("Display", RB.Colored(p.displayId or 0, RB.THEME.picked) .. Badge(RB.PickedBadge(d)), tostring(p.displayId or 0)),
            Row("Model fdid", p.modelFdid), Row("Model id", p.modelId), Row("NPC Extra", p.extraId ~= 0 and p.extraId or "none"),
            Row("Bakes sd / hd", p.extraId and p.extraId > 0 and (p.sdBake .. " / " .. p.hdBake) or ""),
        } },
        { "Creature", "", {
            Row("Name", d.name), Row("Title", d.title), Row("Type", Named(d.typeName, d.type)), Row("Family", Named(d.familyName, d.family)),
            Row("Rank", RB.ChoiceText(RB.CLASSIFICATIONS, RB.RankGroup(d.classification)) .. " (" .. d.classification .. ")", tostring(d.classification)),
            Row("Displays", #(d.displays or {})),
        } },
        { "Display", "Left/Right picks", DisplayRows(d, RB.Picked(d)) },
    }
end

-- Retail armour or weapon damage at the item's own level (the details' Armour and Damage fields), else that Build
-- keeps the base row's.
local function CombatRow(d)
    for _, field in ipairs(d.fields or {}) do
        if field.label == "Armour" or field.label == "Damage" then
            return Row(field.label, RB.Colored(field.value, RB.THEME.combat), field.copy)
        end
    end
    return Row("Armour, damage", RB.Colored("from the base row (Build)", RB.THEME.label), "")
end

local function StatRows(d)
    local rows = {}
    for i, stat in ipairs(d.stats or {}) do
        if i > 6 then break end
        local name = #stat.name > STAT_NAME_LENGTH and (stat.name:sub(1, STAT_NAME_LENGTH - 1) .. ".") or stat.name
        rows[#rows + 1] = Row(name, string.format("%.1f%%%s", stat.percent / 100, stat.value and (", " .. stat.value) or ""), tostring(stat.value or stat.type))
    end
    if #rows == 0 then
        rows[1] = Row("Stats", "none in ItemSparse")
    end
    rows[#rows + 1] = CombatRow(d)
    return rows
end

local function ItemGroups(d)
    local a = RB.Picked(d) or {}
    local models = a.models or {}
    return {
        { "Ids", "click: copy", {
            Row("Item id", d.itemId), Row("Display", (a.displayId or 0) .. Badge(RB.PickedBadge(d)), tostring(a.displayId or 0)),
            Row("Appearance", a.appearanceId), Row("Mod. appear.", a.modifiedAppearanceId), Row("Icon fdid", d.icon),
            Row("Model fdid", models[1] and (models[1] .. (models[2] and models[2] > 0 and (", " .. models[2]) or "")) or "", tostring(models[1] or "")),
        } },
        { "Item", "", {
            Row("Quality", d.quality .. " " .. RB.ChoiceText(RB.QUALITIES, d.quality), tostring(d.quality)),
            Row("Level / req", d.itemLevel .. " / " .. (d.requiredLevel or "?"), tostring(d.itemLevel)),
            Row("Slot", d.inventoryType .. " " .. RB.ChoiceText(RB.INVENTORY_TYPES, d.inventoryType), tostring(d.inventoryType)),
            Row("Class / sub", d.classId .. " / " .. d.subclassId .. " " .. (d.subclassName or ""), tostring(d.subclassId)),
            Row("Material / exp", d.material .. " / " .. d.expansion), Row("Classes", d.allowableClass),
        } },
        { "Stats at " .. (d.sparseItemLevel or d.itemLevel), "retail values", StatRows(d) },
    }
end

local function SetGroups(d)
    local p = RB.Picked(d) or {}
    return {
        { "Ids", "click: copy", {
            Row(d.source == "itemset" and "ItemSet id" or "TransmogSet id", d.setId), Row("Search key", d.id), Row("Class mask", d.classMask),
        } },
        { "Set", "", {
            Row("Name", d.name), Row("Armour", RB.ChoiceText(RB.ARMOUR_TYPES, d.armour), tostring(d.armour)),
            Row("Expansion", RB.ChoiceText(RB.EXPANSIONS, d.expansion), tostring(d.expansion)), Row("Pieces", #(d.pieces or {})),
        } },
        { "Piece", "Left/Right picks", {
            Row("Item id", p.itemId), Row("Name", p.name), Row("Slot", RB.ChoiceText(RB.INVENTORY_TYPES, p.inventoryType), tostring(p.inventoryType or "")),
            Row("Display", (p.displayId or 0) .. Badge(RB.PickedBadge(d)), tostring(p.displayId or 0)), Row("Mod. appear.", p.modifiedAppearanceId),
        } },
    }
end

-- A model's picked row: one of the creature displays drawn with it, else one of its users.
local function ModelGroups(d)
    local u = RB.Picked(d) or {}
    local ids = { Row("Model fdid", d.fdid), Row("Kind", d.creature and "creature model" or "item model"), Row("Users", d.userCount),
        Row("Displays", d.displayCount or #(d.displays or {})) }
    local action = d.creature and RB.ActionDisplay(d)
    if action and action ~= u.displayId then
        -- Before a pick: the first creature user's display, which may be past the listed ones.
        ids[#ids + 1] = Row("Display", RB.Colored(action, RB.THEME.picked) .. " " .. RB.Colored("(first user's)", RB.THEME.label), tostring(action))
        return { { "Ids", "click: copy", ids }, { "Display", "Left/Right picks", { Row("Picked", "none: the first user's display") } }, { "", "", {} } }
    end
    if RB.IsDisplayRow(d, RB.Picked(d)) then
        ids[#ids + 1] = Row("Display", RB.Colored(u.displayId, RB.THEME.picked) .. Badge(RB.PickedBadge(d)), tostring(u.displayId))
        return { { "Ids", "click: copy", ids }, { "Display", "Left/Right picks", DisplayRows(d, u) }, { "", "", {} } }
    end
    return {
        { "Ids", "click: copy", ids },
        { "User", "Left/Right picks", { Row("Kind", u.kind), Row("Id", u.id), Row("Name", u.name), Row("Display", u.displayId) } },
        { "", "", {} },
    }
end

local function MountGroups(d)
    local p = RB.Picked(d) or {}
    return {
        { "Ids", "click: copy", {
            Row("Mount id", d.mountId), Row("Spell id", d.spellId), Row("Display", RB.Colored(p.displayId or 0, RB.THEME.picked) .. Badge(RB.PickedBadge(d)), tostring(p.displayId or 0)),
            Row("Model fdid", p.modelFdid), Row("Model id", p.modelId),
        } },
        { "Mount", "", { Row("Name", d.name), Row("Type", Named(RB.ChoiceText(RB.MOUNT_TYPES, d.mountType), d.mountType)), Row("Displays", #(d.displays or {})) } },
        { "Display", "Left/Right picks", DisplayRows(d, RB.Picked(d)) },
    }
end

local BUILDERS = { creature = CreatureGroups, item = ItemGroups, set = SetGroups, model = ModelGroups, mount = MountGroups }

local function Refresh(d)
    local list = d and BUILDERS[d.kind] and BUILDERS[d.kind](d) or {}
    for g, group in ipairs(groups) do
        local entry = list[g]
        group.title:SetText(entry and entry[1] or (g == 1 and "Pick a row" or ""))
        group.hint:SetText(entry and entry[2] or "")
        for r, row in ipairs(group.rows) do
            local value = entry and entry[3][r]
            if value then
                row.labelText, row.copy = value[1], value[3]
                row.label:SetText(value[1])
                row.value:SetText(value[2])
                row:Show()
            else
                row:Hide()
            end
        end
    end
end

RB.AddDrawerTab("details", "Details", panel, Refresh, { hotkey = "D", order = 1 })
