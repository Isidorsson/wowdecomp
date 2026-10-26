-- The photo booth: one creature display over the whole screen on a flat backdrop, UI hidden and pose frozen, so
-- wowctl (lua_scripts tools/wowctl booth) can shoot it over black, then white, and matte the pair into a transparent
-- PNG. Show loads a display once; SetBackdrop only recolours, so both shots hold the same pose.
--   /rb booth <display> [preset]   opens it on black      /rb booth   closes it (so does a click)

local RB = RetailBrowser
local Booth = {}
RB.Booth = Booth

local BACKDROPS = { black = { 0, 0, 0 }, white = { 1, 1, 1 } }
-- Facing in radians; position as SetPosition takes it: x toward the camera, z up.
local PRESETS = {
    front = { facing = 0, x = 0, y = 0, z = 0 },
    threequarter = { facing = math.pi / 5, x = 0, y = 0, z = 0 },
    head = { facing = 0, x = 2.4, y = 0, z = -0.8 },
}
Booth.PRESETS = PRESETS
-- A fully transparent texture: there is no HideCursor, and a cursor in both shots mattes as opaque.
local NO_CURSOR = "Interface\\AddOns\\RetailBrowser\\BoothNoCursor"

-- No parent: UIParent is hidden while the booth is open.
local frame = CreateFrame("Frame", "RetailBrowserBooth")
frame:SetAllPoints(WorldFrame)
frame:SetFrameStrata("FULLSCREEN_DIALOG")
frame:EnableMouse(true)
frame:Hide()

local back = frame:CreateTexture(nil, "BACKGROUND")
back:SetAllPoints()

local model = CreateFrame("DressUpModel", nil, frame)
model:SetAllPoints()

-- Stand at time 0 every frame: the same bones in both shots. Particles keep their own clock. Frames reset the
-- cursor as the mouse crosses them, so the blank one is set again each frame.
model:SetScript("OnUpdate", function(self)
    self:SetSequenceTime(0, 0)
    SetCursor(NO_CURSOR)
end)

-- ActionStatus ("Screen Captured") hangs off WorldFrame, so hiding UIParent leaves it in the next shot, and it
-- fades itself (setting its own alpha), so it is kept from hearing the screenshots instead.
local SCREENSHOT_EVENTS = { "SCREENSHOT_SUCCEEDED", "SCREENSHOT_FAILED" }
local uiWasShown
function Booth.Open()
    if frame:IsShown() then
        return
    end
    uiWasShown = UIParent:IsShown()
    UIParent:Hide()
    ActionStatus:Hide()
    for _, event in ipairs(SCREENSHOT_EVENTS) do
        ActionStatus:UnregisterEvent(event)
    end
    Booth.SetBackdrop("black")
    frame:Show()
end

function Booth.Close()
    if not frame:IsShown() then
        return
    end
    frame:Hide()
    ResetCursor()
    for _, event in ipairs(SCREENSHOT_EVENTS) do
        ActionStatus:RegisterEvent(event)
    end
    if uiWasShown then
        UIParent:Show()
    end
end

frame:SetScript("OnMouseUp", Booth.Close)

function Booth.SetBackdrop(name)
    local color = BACKDROPS[name]
    if not color then
        error("backdrop must be black or white, not " .. tostring(name))
    end
    back:SetTexture(color[1], color[2], color[3], 1)
    return true
end

-- Opens the booth if needed and shows a display at a preset (default front).
function Booth.Show(display, presetName)
    local preset = PRESETS[presetName or "front"]
    if not preset then
        error("unknown preset " .. tostring(presetName))
    end
    Booth.Open()
    model:SetDisplayInfo(display)
    model:SetFacing(preset.facing)
    model:SetPosition(preset.x, preset.y, preset.z)
    return true
end

-- Whether the model has a file yet, which wowctl waits for before its settle delay.
function Booth.Ready()
    local file = model:GetModel()
    return type(file) == "string" and file ~= ""
end

-- "/rb booth 128688 head" shows; "/rb booth" closes. False for a message that is not a booth command.
function Booth.Command(message)
    local verb, display, preset = strsplit(" ", strtrim(message or ""))
    if verb ~= "booth" then
        return false
    end
    if not tonumber(display) then
        Booth.Close()
        return true
    end
    if preset and not PRESETS[preset] then
        RB.Print("presets: front, threequarter, head")
        return true
    end
    Booth.Show(tonumber(display), preset)
    return true
end
