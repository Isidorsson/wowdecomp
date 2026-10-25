-- The detail pane: every field RetailDetails labels (click one to copy it; a display line also picks that display
-- for the preview and the GM commands), and the action buttons below it.

local RB = RetailBrowser
local frame = RB.frame

local PANE_WIDTH = 468
local FIELD_HEIGHT = 18
local VISIBLE_FIELDS = 11

local pane = RB.CreateInset(frame, PANE_WIDTH, FIELD_HEIGHT * VISIBLE_FIELDS + 44)
pane:SetPoint("TOPLEFT", RB.list, "TOPRIGHT", 12, 0)
RB.pane = pane

local heading = RB.CreateText(pane, "GameFontNormal")
heading:SetPoint("TOPLEFT", 10, -8)
heading:SetWidth(PANE_WIDTH - 20)
local subheading = RB.CreateText(pane, "GameFontDisableSmall")
subheading:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 0, -2)
subheading:SetText("Pick a result. Click a field to copy it.")

local scroll = CreateFrame("ScrollFrame", "RetailBrowserFieldScroll", pane, "FauxScrollFrameTemplate")
scroll:SetPoint("TOPLEFT", 4, -40)
scroll:SetPoint("BOTTOMRIGHT", -26, 4)

local function DisplayOfField(details, field)
    local slot = field.label:match("^Display (%d+)$")
    local display = slot and details.displays and details.displays[tonumber(slot)]
    if display then
        return display.displayId
    end
    local user = field.label:match("^User (%d+)$")
    user = user and details.users and details.users[tonumber(user)]
    return user and user.kind ~= "item" and user.displayId or nil
end

local function FieldClicked(self)
    local details, field = RB.details, self.field
    if not field then
        return
    end
    RB.Copy(field.copy or field.value, field.label .. " (" .. (field.copy or field.value) .. ")")
    local display = DisplayOfField(details, field)
    if display then
        RB.pickedDisplay = { owner = details, id = display }
        RB.Preview(details)
    end
end

local rows = {}
for i = 1, VISIBLE_FIELDS do
    local row = CreateFrame("Button", nil, pane)
    row:SetSize(PANE_WIDTH - 34, FIELD_HEIGHT)
    row:SetPoint("TOPLEFT", 8, -40 - (i - 1) * FIELD_HEIGHT)
    row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    row.label = RB.CreateText(row, "GameFontNormalSmall")
    row.label:SetPoint("LEFT", 2, 0)
    row.label:SetWidth(130)
    row.value = RB.CreateText(row, "GameFontHighlightSmall")
    row.value:SetPoint("LEFT", row.label, "RIGHT", 6, 0)
    row.value:SetWidth(PANE_WIDTH - 180)
    row:SetScript("OnClick", FieldClicked)
    rows[i] = row
end

local function RefreshFields()
    local fields = RB.details and RB.details.fields or {}
    local offset = FauxScrollFrame_GetOffset(scroll)
    for i, row in ipairs(rows) do
        local field = fields[offset + i]
        row.field = field
        if field then
            row.label:SetText(field.label)
            row.value:SetText(field.value)
            row:Show()
        else
            row:Hide()
        end
    end
    FauxScrollFrame_Update(scroll, #fields, VISIBLE_FIELDS, FIELD_HEIGHT)
end
scroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, FIELD_HEIGHT, RefreshFields)
end)

-- Action buttons, in two columns to the right of the preview.
local actions = CreateFrame("Frame", nil, frame)
actions:SetSize(250, 150)
actions:SetPoint("TOPLEFT", pane, "BOTTOMLEFT", 222, -10)
local buttons = {}
local function AddAction(key, text, onClick)
    local button = RB.CreateButton(actions, text, 122, function()
        if RB.details then onClick(RB.details) end
    end)
    local n = #buttons
    button:SetPoint("TOPLEFT", (n % 2) * 126, -math.floor(n / 2) * 26)
    buttons[key] = button
    buttons[#buttons + 1] = button
end

AddAction("morph", "Morph me", function(d) RB.SendCommand(RB.CommandsFor(d).morph) end)
AddAction("npcModel", "Set target model", function(d) RB.SendCommand(RB.CommandsFor(d).npcModel) end)
AddAction("addItem", "Add item", function(d)
    for _, command in ipairs(RB.CommandsFor(d).addItem) do RB.SendCommand(command) end
end)
AddAction("trySet", "Try whole set", function(d) RB.Preview(d) end)
AddAction("copyId", "Copy id", function(d) RB.Copy(RB.PrimaryId(d), "id " .. RB.PrimaryId(d)) end)
AddAction("copyAll", "Copy all", function(d) RB.Copy(RB.PlainText(d), "all fields") end)
AddAction("copyGm", "Copy GM command", function(d) RB.Copy(RB.GmCommandText(d), "GM command") end)
AddAction("favourite", "Favourite", function(d)
    local on = RB.ToggleFavourite(d.kind, d.id, d.name)
    RB.UpdateActions()
    if RB.state.source == "favourites" then RB.ShowSaved("favourites") end
    RB.Print((on and "added " or "removed ") .. d.name .. (on and " to" or " from") .. " favourites")
end)
AddAction("build", "Build SQL", function() RB.ToggleBuild() end)

local function Enable(button, enabled)
    if enabled then button:Enable() else button:Disable() end
end

function RB.UpdateActions()
    local d = RB.details
    local commands = d and RB.CommandsFor(d) or { addItem = {} }
    Enable(buttons.morph, commands.morph)
    Enable(buttons.npcModel, commands.npcModel and UnitExists("target"))
    Enable(buttons.addItem, #commands.addItem > 0)
    Enable(buttons.trySet, d and d.kind == "set")
    Enable(buttons.build, d and (d.kind == "item" or d.kind == "set" or d.kind == "creature"))
    for _, key in ipairs({ "copyId", "copyAll", "copyGm", "favourite" }) do
        Enable(buttons[key], d)
    end
    buttons.favourite:SetText(d and RB.IsFavourite(d.kind, d.id) and "Unfavourite" or "Favourite")
end
actions:RegisterEvent("PLAYER_TARGET_CHANGED")
actions:SetScript("OnEvent", RB.UpdateActions)

function RB.ShowDetails(kind, id, preview)
    local details, err = RetailDetails(kind, id)
    if not details or details.status ~= "ok" then
        RB.SetStatus("|cffff4040" .. tostring(err or (details and details.status)) .. "|r")
        return
    end
    RB.details, RB.pickedDisplay = details, nil
    RB.RememberViewed(kind, id, details.name)
    heading:SetText(details.name)
    heading:SetTextColor(RB.QualityColor(details.quality))
    subheading:SetText(RB.KindLabel(kind) .. "  |  id " .. RB.PrimaryId(details))
    RetailBrowserFieldScrollScrollBar:SetValue(0)
    RefreshFields()
    RB.UpdateActions()
    if preview then
        RB.Preview(details)
    end
end
RB.UpdateActions()
