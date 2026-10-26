-- Keyboard: the grid owns the keys while it has focus (the status line says "Keys: grid"); its letter keys are
-- commands only then. A key the grid does not use goes into the search box (EditBox:Insert), so no press is lost.
-- / focuses the search box, Esc in a cell returns to the grid, Esc in the grid clears the checks, then closes.

local RB = RetailBrowser
local grid = RB.grid

RB.keyOwner = "game"

function RB.SetKeyOwner(owner)
    RB.keyOwner = owner
    if owner ~= "grid" then
        grid:EnableKeyboard(false)
    end
    RB.ShowKeyOwner(owner)
end

function RB.KeyOwnerLost()
    RB.SetKeyOwner("game")
end

function RB.FocusGrid()
    local focus = GetCurrentKeyBoardFocus()
    if focus and focus:GetParent() and RB.frame:IsShown() then
        focus:ClearFocus()
    end
    grid:EnableKeyboard(true)
    RB.SetKeyOwner("grid")
end

function RB.ToggleGridKeys()
    if RB.keyOwner == "grid" then
        RB.SetKeyOwner("game")
    else
        RB.FocusGrid()
    end
end

local function Bulk(bulkAction, action)
    return function()
        if next(RB.checked) then
            bulkAction()
        else
            RB.RunAction(action)
        end
    end
end

local function Move(step)
    return function() RB.MoveFocus(step) end
end

local function Page(step)
    return function() RB.MoveFocus(step * RB.visibleRows) end
end

local function Tab(key)
    return function() RB.ShowDrawerTab(key) end
end

local function ExtendCheck(step)
    local from = RB.focused or 0
    local to = RB.MoveFocus(step)
    if to then
        RB.CheckRange(math.max(1, from), to)
    end
end

-- Keys without modifiers.
local PLAIN = {
    UP = Move(-1), DOWN = Move(1), PAGEUP = Page(-1), PAGEDOWN = Page(1),
    HOME = function() RB.MoveFocus(-#RB.rows) end, END = function() RB.MoveFocus(#RB.rows) end,
    ENTER = function() if RB.focused then RB.FocusRow(RB.focused, true) end end,
    SPACE = function() if RB.focused then RB.ToggleCheck(RB.focused) end end,
    LEFT = function() RB.StepPick(-1) end, RIGHT = function() RB.StepPick(1) end,
    TAB = function() RB.FocusSearch() end, ["/"] = function() RB.FocusSearch() end,
    ESCAPE = function() if next(RB.checked) then RB.ClearChecks() else RB.frame:Hide() end end,
    M = function() RB.RunAction("morph") end, T = function() RB.RunAction("npcModel") end,
    I = Bulk(RB.BulkAdd, "addItem"), Y = Bulk(RB.BulkTryOn, "tryOn"), G = Bulk(RB.BulkGm, "copyGm"),
    F = Bulk(RB.BulkFavourite, "favourite"),
    B = function() RB.ShowBuild(next(RB.checked) and "checked" or "focused") end,
    D = Tab("details"), P = Tab("displays"), U = Tab("usedby"), R = Tab("raw"), S = Tab("stats"),
    E = function() RB.ExportCsv() end,
    ["["] = function() RB.TurnPage(-1) end, ["]"] = function() RB.TurnPage(1) end,
    ["="] = function() RB.ZoomPreview(1) end, ["-"] = function() RB.ZoomPreview(-1) end,
    F8 = function() RB.ShowSaved("favourites") end, F9 = function() RB.ShowSaved("history") end,
}

local SHIFT = {
    UP = function() ExtendCheck(-1) end, DOWN = function() ExtendCheck(1) end,
    E = function() RB.BulkCsv() end,
}

local CTRL = {
    UP = function() RB.StepDrawer(1) end, DOWN = function() RB.StepDrawer(-1) end,
    DELETE = function() RB.ClearHistoryNow() end,
    C = function()
        if IsShiftKeyDown() then
            if next(RB.checked) then RB.BulkCopyRows() else RB.RunAction("copyAll") end
        elseif next(RB.checked) then
            RB.BulkCopyIds()
        else
            RB.RunAction("copyId")
        end
    end,
    A = function() RB.CheckPage() end,
    S = function() RB.ExportBuildSql() end,
    ENTER = function() RB.CopyBuildSql() end,
}
for i, kind in ipairs(RB.KINDS) do
    CTRL[tostring(i)] = function() RB.SetKind(kind.key) end
end

local ALT = { C = function() RB.RunAction("copyDisplay") end }
for i = 1, 4 do
    ALT[tostring(i)] = function() RB.PickDisplay(i) end
end

local function Handler(key)
    if IsControlKeyDown() then
        return CTRL[key]
    elseif IsAltKeyDown() then
        return ALT[key]
    elseif IsShiftKeyDown() and SHIFT[key] then
        return SHIFT[key]
    end
    return PLAIN[key]
end

-- OnKeyDown runs the command; OnChar (which follows for a key that types a character) sends anything else to the
-- search box. Modifier presses alone are ignored.
local MODIFIERS = { LSHIFT = true, RSHIFT = true, LCTRL = true, RCTRL = true, LALT = true, RALT = true }

grid:SetScript("OnKeyDown", function(self, key)
    if MODIFIERS[key] then
        return
    end
    local handler = Handler(key)
    self.lastHandled = handler ~= nil or IsControlKeyDown() or IsAltKeyDown()
    if handler then
        handler()
    elseif not self.hasOnChar and not self.lastHandled and #key == 1 then
        RB.FocusSearch(IsShiftKeyDown() and key or key:lower())
    end
end)

grid.hasOnChar = grid:HasScript("OnChar")
if grid.hasOnChar then
    grid:SetScript("OnChar", function(self, text)
        if not self.lastHandled then
            RB.FocusSearch(text)
        end
        self.lastHandled = false
    end)
end

grid:SetScript("OnHide", function(self)
    self:EnableKeyboard(false)
    RB.SetKeyOwner("game")
end)
RB.ShowKeyOwner("game")
