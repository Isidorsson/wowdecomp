-- The drawer's actions on the shown entry and its picked display, each with its grid key: Morph me, Set target model,
-- Add item, Try on, Copy id, Copy display, Copy all, GM command, Favourite, Build SQL.

local RB = RetailBrowser

local slot = RB.drawerActionsSlot
local BUTTON_WIDTH = 133
local BUTTON_HEIGHT = 22

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
    local button = RB.CreateButton(slot, text .. " " .. RB.Key(hotkey), BUTTON_WIDTH, BUTTON_HEIGHT, function()
        Run(key)
        RB.FocusGrid()
    end)
    local n = #buttons
    button:SetPoint("TOPLEFT", (n % 2) * (BUTTON_WIDTH + 4), -math.floor(n / 2) * (BUTTON_HEIGHT + 4))
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
AddAction("favourite", "Favourite", "F", function(d)
    local on = RB.ToggleFavourite(d.kind, d.id, d.name)
    RB.Print((on and "added " or "removed ") .. d.name .. (on and " to" or " from") .. " favourites")
    RB.AfterFavouriteChange()
end)
AddAction("build", "Build SQL", "B", function() RB.ShowBuild("focused") end,
    function(d) return d.kind == "item" or d.kind == "set" or d.kind == "creature" end)

local hint = RB.CreateText(slot, "RetailBrowserLabelFont")
hint:SetPoint("BOTTOMLEFT", 2, 2)
hint:SetPoint("BOTTOMRIGHT", -2, 2)
hint:SetText("Actions use the picked display (Displays tab, Left/Right).")

function RB.UpdateActions()
    local d = RB.details
    for _, button in ipairs(buttons) do
        local action = RB.ACTIONS[button.key]
        button:SetEnabled(d and (not action.enabled or action.enabled(d)))
    end
    for _, button in ipairs(buttons) do
        if button.key == "favourite" then
            button:SetLabel((d and RB.IsFavourite(d.kind, d.id) and "Unfavourite" or "Favourite") .. " " .. RB.Key("F"))
        end
    end
end

function RB.AfterFavouriteChange()
    RB.UpdateActions()
    RB.RefreshTabs()
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
