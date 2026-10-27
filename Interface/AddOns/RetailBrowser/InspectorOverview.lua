-- The inspector's Overview tab: the sections Overview.lua builds for the shown entry, stacked, each as tall as its
-- content: key-value rows (click a value to copy it; Shift+click adds it to the copy buffer and copies the buffer,
-- comma-separated), a fixed list (click picks a display or opens a user), class chips, or an item's tooltip.

local RB = RetailBrowser
local T = RB.THEME

local PAD = 6
local SECTION_GAP = 6
local TITLE_SPACE = 18
local KV_ROW = 14
local LABEL_WIDTH = 104
local CHIP_HEIGHT = 18
local CHIP_GAP = 4
local CHIP_PADDING = 14
local TOOLTIP_LINE = 15
local FALLBACK_WIDTH = 500

local panel = CreateFrame("Frame")
panel.contentHeight = 0
local sectionFrames = {}
local copyBuffer = {}
local shown, shownWidth

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

local function ListClicked(entry)
    if entry and entry.pick then
        RB.PickDisplay(entry.pick)
        RB.FocusGrid()
    elseif entry and entry.open then
        RB.OpenEntry(entry.open.kind, entry.open.id)
    end
end

local function CreateSection()
    local frame = RB.CreatePanel(panel, "well")
    frame.title = RB.CreateText(frame, "RetailBrowserHeadFont")
    frame.title:SetPoint("TOPLEFT", PAD, -PAD)
    RB.SetColor(frame.title, T.section)
    frame.hint = RB.CreateText(frame, "RetailBrowserLabelFont", "RIGHT")
    frame.hint:SetPoint("TOPRIGHT", -PAD, -PAD)
    frame.rows, frame.chips, frame.lines = {}, {}, {}
    frame.list = RB.CreateFixedList(frame, ListClicked, "list_overview")
    return frame
end

-- Key-value rows.
local function KvRow(frame, r)
    local row = frame.rows[r]
    if row then
        return row
    end
    row = CreateFrame("Button", nil, frame)
    row:SetHeight(KV_ROW)
    row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    row.label = RB.CreateText(row, "RetailBrowserLabelFont")
    row.label:SetPoint("LEFT", 2, 0)
    row.label:SetWidth(LABEL_WIDTH)
    row.value = RB.CreateText(row, "RetailBrowserValueFont")
    row.value:SetPoint("LEFT", row.label, "RIGHT", 4, 0)
    row.value:SetPoint("RIGHT", -2, 0)
    row:SetScript("OnClick", ValueClicked)
    RB.AttachTip(row, FieldTip)
    frame.rows[r] = row
    return row
end

local function RenderKv(frame, section, top)
    for r, data in ipairs(section.rows) do
        local row = KvRow(frame, r)
        row:SetPoint("TOPLEFT", PAD - 2, -top - (r - 1) * KV_ROW)
        row:SetPoint("RIGHT", -PAD, 0)
        row.labelText, row.copy = data[1], data[3]
        row.label:SetText(data[1])
        row.value:SetText(data[2])
        row:Show()
    end
    return #section.rows * KV_ROW
end

local function RenderList(frame, section, top)
    local list = frame.list
    list.panel:ClearAllPoints()
    list.panel:SetPoint("TOPLEFT", PAD, -top)
    list.panel:SetPoint("RIGHT", -PAD, 0)
    RB.ShowListEntries(list, section.columns, section.entries, section.more, section.focused)
    list.panel:Show()
    return list.panel:GetHeight()
end

-- Class chips, wrapping to the section's width.
local function Chip(frame, i)
    local chip = frame.chips[i]
    if not chip then
        chip = CreateFrame("Frame", nil, frame)
        chip:SetHeight(CHIP_HEIGHT)
        chip.fill = RB.CreateFill(chip, "BACKGROUND", T.accentFill)
        chip.text = RB.CreateText(chip, "RetailBrowserSmallFont", "CENTER")
        chip.text:SetPoint("CENTER")
        frame.chips[i] = chip
    end
    return chip
end

local function RenderChips(frame, section, top, width)
    local x, y = 0, 0
    for i, data in ipairs(section.chips) do
        local chip = Chip(frame, i)
        chip.text:SetText(data.text)
        RB.SetColor(chip.text, data.on and T.accent or T.disabled)
        RB.Shown(chip.fill, data.on)
        chip:SetWidth(chip.text:GetStringWidth() + CHIP_PADDING)
        if x > 0 and x + chip:GetWidth() > width then
            x, y = 0, y + CHIP_HEIGHT + CHIP_GAP
        end
        chip:SetPoint("TOPLEFT", PAD + x, -top - y)
        chip:Show()
        x = x + chip:GetWidth() + CHIP_GAP
    end
    return y + CHIP_HEIGHT
end

-- An item tooltip: a left and a right string per line.
local function TooltipLine(frame, i)
    local line = frame.lines[i]
    if not line then
        line = { left = RB.CreateText(frame, "GameTooltipText"), right = RB.CreateText(frame, "GameTooltipText", "RIGHT") }
        frame.lines[i] = line
    end
    return line
end

local function RenderTooltip(frame, section, top)
    for i, data in ipairs(section.lines) do
        local line = TooltipLine(frame, i)
        line.left:SetFontObject(i == 1 and GameTooltipHeaderText or GameTooltipText)
        line.left:SetPoint("TOPLEFT", PAD + 2, -top - (i - 1) * TOOLTIP_LINE - (i > 1 and 3 or 0))
        line.right:SetPoint("TOPRIGHT", -PAD - 2, -top - (i - 1) * TOOLTIP_LINE - (i > 1 and 3 or 0))
        line.left:SetText(data.left or "")
        line.right:SetText(data.right or "")
        RB.SetColor(line.left, data.color)
        RB.SetColor(line.right, data.rightColor)
        line.left:Show()
        line.right:Show()
    end
    return #section.lines * TOOLTIP_LINE + 3
end

local RENDERERS = { kv = RenderKv, list = RenderList, chips = RenderChips, tooltip = RenderTooltip }

local function HideParts(frame)
    for _, row in ipairs(frame.rows) do row:Hide() end
    for _, chip in ipairs(frame.chips) do chip:Hide() end
    for _, line in ipairs(frame.lines) do line.left:Hide(); line.right:Hide() end
    frame.list.panel:Hide()
end

-- Draws one section at y = -top in the panel; returns its height.
local function RenderSection(frame, section, top, width)
    HideParts(frame)
    frame:ClearAllPoints()
    frame:SetPoint("TOPLEFT", 0, -top)
    frame:SetPoint("RIGHT", 0, 0)
    frame.title:SetText(section.title or "")
    frame.hint:SetText(section.hint or "")
    local contentTop = section.title and TITLE_SPACE + PAD or PAD
    local height = contentTop + RENDERERS[section.kind](frame, section, contentTop, width - 2 * PAD) + PAD
    frame:SetHeight(height)
    frame:Show()
    return height
end

local function Render(sections)
    local width = panel:GetWidth() > 0 and panel:GetWidth() or FALLBACK_WIDTH
    shown, shownWidth = sections, width
    local top = 0
    for i, section in ipairs(sections) do
        sectionFrames[i] = sectionFrames[i] or CreateSection()
        top = top + RenderSection(sectionFrames[i], section, top, width) + SECTION_GAP
    end
    for i = #sections + 1, #sectionFrames do
        sectionFrames[i]:Hide()
    end
    if top ~= panel.contentHeight then
        panel.contentHeight = top
        RB.LayOutInspector()
    end
end

-- A new width rewraps the chips and the list columns.
panel:SetScript("OnSizeChanged", function(self, width)
    if shown and width ~= shownWidth then
        Render(shown)
    end
end)

local function Refresh(d)
    Render(RB.OverviewSections(d))
end

RB.AddInspectorTab("overview", "Overview", panel, Refresh,
    { order = 1, minHeight = function() return panel.contentHeight end })
