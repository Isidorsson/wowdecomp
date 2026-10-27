-- The inspector's actions on the shown entry and its picked display, each with its grid key: Morph me, Set target
-- model, Add item, Try on, Copy id, Copy display, Copy all, GM command, Build SQL. They fill the
-- inspector's width in rows (as many columns as fit) and stay pinned under its scrolling content.

local RB = RetailBrowser

local slot = RB.inspectorActionsSlot
local BUTTON_MIN_WIDTH = 160
local BUTTON_HEIGHT = 22
local BUTTON_GAP = 4
local HINT_HEIGHT = 14

RB.ACTIONS = {}
local buttons = {}

local function Run(key)
    local d = RB.details
    local action = RB.ACTIONS[key]
    if d and action and (not action.enabled or action.enabled(d)) then
        action.run(d)
        return true
    end
end
RB.RunAction = Run

local function AddAction(key, text, hotkey, run, enabled)
    RB.ACTIONS[key] = { run = run, enabled = enabled }
    if not text then
        return
    end
    local button = RB.CreateButton(slot, text .. " " .. RB.Key(hotkey), BUTTON_MIN_WIDTH, BUTTON_HEIGHT, function()
        Run(key)
        RB.FocusGrid()
    end)
    button.key = key
    RB.AttachTip(button, "action_" .. key)
    buttons[#buttons + 1] = button
end

local function IsItemKind(d)
    return d.kind == "item" or d.kind == "set"
end

AddAction("morph", "Morph me", "M", function(d) RB.SendCommand(RB.CommandsFor(d).morph) end,
    function(d) return RB.CommandsFor(d).morph ~= nil end)
AddAction("npcModel", "Set target", "T", function(d) RB.SendCommand(RB.CommandsFor(d).npcModel) end,
    function(d) return RB.CommandsFor(d).npcModel ~= nil and UnitExists("target") end)
AddAction("addItem", "Add item", "I", function(d)
    for _, command in ipairs(RB.CommandsFor(d).addItem) do RB.SendCommand(command) end
end, function(d) return #RB.CommandsFor(d).addItem > 0 end)
AddAction("tryOn", "Try on", "Y", function(d) RB.SchedulePreview(true) end, IsItemKind)
AddAction("copyId", "Copy id", "Ctrl+C", function(d) RB.Copy(RB.PrimaryId(d), "id " .. RB.PrimaryId(d)) end)
AddAction("copyDisplay", "Copy display id", "Alt+C", function(d)
    local picked = RB.Picked(d)
    local display = picked and picked.displayId
    if display then RB.Copy(display, "display " .. display) end
end, function(d) return RB.Picked(d) ~= nil end)
AddAction("copyAll", "Copy all", "Ctrl+Shift+C", function(d) RB.Copy(RB.PlainText(d), "all fields") end)
AddAction("copyGm", "GM command", "G", function(d) RB.Copy(RB.GmCommandText(d), "GM command") end,
    function(d) return RB.GmCommandText(d) ~= "" end)
-- No button: the header's star runs it, and F.
AddAction("favourite", nil, "F", function(d)
    local on = RB.ToggleFavourite(d.kind, d.id, d.name)
    RB.Print((on and "added " or "removed ") .. d.name .. (on and " to" or " from") .. " favourites")
    RB.AfterFavouriteChange()
end)
AddAction("build", "Build SQL", "B", function() RB.ShowBuild("focused") end,
    function(d) return RB.BUILD_TABLES[d.kind] ~= nil end)

local hint = RB.CreateText(slot, "RetailBrowserLabelFont")
hint:SetPoint("BOTTOMLEFT", 2, 2)
hint:SetPoint("BOTTOMRIGHT", -2, 2)
hint:SetText("Actions use the picked display (Displays tab, Left/Right).")

-- Places the buttons in rows across the width; returns the slot's height.
function RB.LayOutActions(width)
    local columns, buttonWidth, rows = RB.Layout.ActionGrid(#buttons, width, BUTTON_MIN_WIDTH, BUTTON_GAP)
    for i, button in ipairs(buttons) do
        local n = i - 1
        button:ClearAllPoints()
        button:SetWidth(buttonWidth)
        button:SetPoint("TOPLEFT", (n % columns) * (buttonWidth + BUTTON_GAP), -math.floor(n / columns) * (BUTTON_HEIGHT + BUTTON_GAP))
    end
    return rows * (BUTTON_HEIGHT + BUTTON_GAP) + HINT_HEIGHT
end

-- Each kind's main action, drawn as the primary button.
local PRIMARY = { creature = "morph", mount = "morph", model = "morph", item = "addItem", set = "tryOn" }

function RB.UpdateActions()
    local d = RB.details
    local primary = d and PRIMARY[d.kind]
    for _, button in ipairs(buttons) do
        local action = RB.ACTIONS[button.key]
        button:SetEnabled(d and (not action.enabled or action.enabled(d)))
        button:SetPrimary(button.key == primary)
        if button.key == "tryOn" then
            button:SetLabel((d and d.kind == "set" and "Try on set" or "Try on") .. " " .. RB.Key("Y"))
        end
    end
end

function RB.AfterFavouriteChange()
    RB.RefreshStar()
    RB.RefreshRail()
    for _, row in ipairs(RB.rows) do
        row.cells = nil
    end
    if RB.state.source == "favourites" then
        RB.RefreshSaved()
    else
        RB.RefreshGrid()
    end
end

local events = CreateFrame("Frame", nil, slot)
events:RegisterEvent("PLAYER_TARGET_CHANGED")
events:SetScript("OnEvent", function() RB.UpdateActions() end)
