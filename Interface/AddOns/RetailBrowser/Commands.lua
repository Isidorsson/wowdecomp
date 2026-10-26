-- GM commands and plain-text copies built from a RetailDetails table. The SQL of the Build panel (Build.lua) comes
-- from RetailBuildSql, which reads the same entry.

local RB = RetailBrowser

-- The display picked in the details of this entry, nil before one is picked.
function RB.PickedDisplay(details)
    local picked = RB.pickedDisplay
    return details and picked and picked.owner == details and picked.id or nil
end

-- The creature display an action uses: the one picked in the details, else the entry's first.
function RB.ActionDisplay(details)
    if not details then
        return nil
    end
    if RB.PickedDisplay(details) then
        return RB.PickedDisplay(details)
    end
    if details.kind == "model" and not details.creature then
        return nil
    end
    if details.displayId and details.displayId > 0 then
        return details.displayId
    end
end

-- The displays an entry can preview and morph: a creature's (up to four), a mount's, or every display of a model.
function RB.DisplaysOf(details)
    return details and details.displays or {}
end

-- Picks a display of the shown entry for the preview, the GM commands, Copy id and the Build panel.
function RB.PickDisplay(details, id)
    RB.pickedDisplay = { owner = details, id = id }
    RB.Preview(details)
    RB.RefreshFields()
    RB.UpdateActions()
    RB.ScheduleBuild()
end

-- Moves the picked display delta places through the entry's displays, wrapping around.
function RB.StepDisplay(details, delta)
    local displays = RB.DisplaysOf(details)
    if #displays == 0 then
        return
    end
    local current, at = RB.ActionDisplay(details), 1
    for i, display in ipairs(displays) do
        if display.displayId == current then
            at = i
        end
    end
    RB.PickDisplay(details, displays[(at - 1 + delta) % #displays + 1].displayId)
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

-- The id "Copy id" copies: a picked display, else the entry's own id.
function RB.CopyId(details)
    return RB.PickedDisplay(details) or RB.PrimaryId(details)
end

-- The entry's own id: the original set id for an item set, not its search key.
function RB.PrimaryId(details)
    return details.setId or details.itemId or details.creatureId or details.mountId or details.fdid or details.id
end
