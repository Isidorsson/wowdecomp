-- GM commands, plain-text copies and the picked display of a RetailDetails table. The SQL of the Build tab (Build.lua)
-- comes from RetailBuildSql, which reads the same entry.

local RB = RetailBrowser

-- What the Displays tab lists and picks from: displays (creatures, mounts, and every creature display of a model),
-- appearances (items), pieces (sets), or the users of a model no creature display draws.
function RB.PickList(details)
    if not details then
        return {}
    end
    if details.kind == "model" then
        return details.displays and #details.displays > 0 and details.displays or details.users or {}
    end
    return details.displays or details.appearances or details.pieces or details.users or {}
end

-- Whether a pick list row is a creature display (a model lists displays or users).
function RB.IsDisplayRow(details, row)
    return row ~= nil and details.kind ~= "item" and details.kind ~= "set" and row.kind == nil
end

-- Until one is picked: the row of the entry's own display (a model's first creature user's), else the first.
local function DefaultPick(details, list)
    local own = details and details.displayId
    if own and own > 0 and details.kind ~= "item" and details.kind ~= "set" then
        for i, row in ipairs(list) do
            if row.displayId == own and RB.IsDisplayRow(details, row) then
                return i
            end
        end
    end
    return 1
end

-- The picked row of the Displays tab, and its place in the list.
function RB.Picked(details)
    local list = RB.PickList(details)
    local index = RB.pick and RB.pick.owner == details and RB.pick.index or DefaultPick(details, list)
    return list[index], index
end

function RB.Pick(details, index)
    local list = RB.PickList(details)
    if details and list[index] then
        RB.pick = { owner = details, index = index }
        return true
    end
end

-- The creature display an action uses: the picked one (a model's creature users count), nil for items and sets.
function RB.ActionDisplay(details)
    if not details or details.kind == "item" or details.kind == "set" then
        return nil
    end
    local picked = RB.Picked(details)
    if details.kind == "model" then
        if not details.creature then
            return nil
        end
        -- Before a pick, the first creature user's display, also when it is past the listed displays.
        local own = details.displayId
        if own and own > 0 and not (RB.pick and RB.pick.owner == details) and not (picked and picked.displayId == own) then
            return own
        end
        return picked and picked.kind ~= "item" and picked.displayId or nil
    end
    local display = picked and picked.displayId or details.displayId
    if display and display > 0 then
        return display
    end
end

-- The badge of the picked display: served, custom or 3.3.5.
function RB.PickedBadge(details)
    local picked = RB.Picked(details)
    local model = details.kind == "model"
    if not picked or (model and (not RB.IsDisplayRow(details, picked) or RB.ActionDisplay(details) ~= picked.displayId)) then
        return nil
    end
    local itemDisplay = details.kind == "item" or details.kind == "set"
    return RB.BadgeOf(picked.custom, picked.displayId, itemDisplay)
end

local function ItemIds(details)
    if details.kind == "item" then
        return { details.itemId }
    end
    local ids = {}
    if details.kind == "set" then
        for _, piece in ipairs(details.pieces or {}) do
            ids[#ids + 1] = piece.itemId
        end
    end
    return ids
end

-- Every command of an entry: morph and npcModel for creature displays, addItem lines for items and set pieces.
function RB.CommandsFor(details)
    local commands = { addItem = {} }
    local display = RB.ActionDisplay(details)
    if display then
        commands.morph = ".morph " .. display
        commands.npcModel = ".npc set model " .. display
    end
    for _, id in ipairs(ItemIds(details)) do
        commands.addItem[#commands.addItem + 1] = ".additem " .. id
    end
    return commands
end

-- The command "Copy GM command" copies: the morph of a display, else the item lines.
function RB.GmCommandText(details)
    local commands = RB.CommandsFor(details)
    if commands.morph then
        return commands.morph
    end
    return table.concat(commands.addItem, "\n")
end

function RB.SendCommand(command)
    if command then
        SendChatMessage(command, "SAY")
    end
end

function RB.PlainText(details)
    local lines = { details.name or "", RB.KindLabel(details.kind) }
    for _, field in ipairs(details.fields or {}) do
        lines[#lines + 1] = field.label .. ": " .. field.value
    end
    return table.concat(lines, "\n")
end

function RB.KindLabel(kind)
    for _, entry in ipairs(RB.KINDS) do
        if entry.key == kind then
            return entry.label
        end
    end
    return kind or ""
end

-- The id "Copy id" copies: the original set id for an item set, not its search key.
function RB.PrimaryId(details)
    return details.setId or details.itemId or details.creatureId or details.mountId or details.fdid or details.id
end

-- RetailDetails, cached for the grid's cells and the inspector (the index does not change while the client runs).
local detailsCache, cached = {}, 0
local CACHE_LIMIT = 600

function RB.DetailsOf(kind, id)
    local key = kind .. ":" .. id
    local details = detailsCache[key]
    if details then
        return details
    end
    local err
    details, err = RetailDetails(kind, id)
    if not details or details.status ~= "ok" then
        return nil, err or (details and details.status)
    end
    if cached >= CACHE_LIMIT then
        wipe(detailsCache)
        cached = 0
    end
    detailsCache[key], cached = details, cached + 1
    return details
end
