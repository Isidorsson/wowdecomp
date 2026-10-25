-- GM commands and plain-text copies built from a RetailDetails table. The SQL builders of a later slice start from
-- the same details: every id they need is a named field there.

local RB = RetailBrowser

-- The creature display an action uses: the one picked in the details, else the entry's first.
function RB.ActionDisplay(details)
    if not details then
        return nil
    end
    if RB.pickedDisplay and RB.pickedDisplay.owner == details then
        return RB.pickedDisplay.id
    end
    if details.kind == "model" and not details.creature then
        return nil
    end
    if details.displayId and details.displayId > 0 then
        return details.displayId
    end
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
    if details.kind ~= "item" and details.kind ~= "set" then
        local display = RB.ActionDisplay(details)
        if display then
            commands.morph = ".morph " .. display
            commands.npcModel = ".npc set model " .. display
        end
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
