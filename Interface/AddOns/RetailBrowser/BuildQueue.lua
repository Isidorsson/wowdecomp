-- The Build SQL page's queue: a row per entry the build writes (BuildModel.lua's RB.BuildQueue), with its name,
-- "from <source id> · display <id>", a note and the new entry; a teal dot is ready, a yellow one needs a look (an
-- installed display row wins, or the source has several displays). A click shows that row in the output. The
-- next free entry after the build is at the bottom.

local RB = RetailBrowser
local T = RB.THEME

local PAD = 10
local ROW_HEIGHT = 48
local ROW_GAP = 2
local DOT = 8
local FOOTER_HEIGHT = 28
local SCROLLBAR_SPACE = 20

local col = RB.buildQueueCol
local title = RB.CreateText(col, "RetailBrowserHeadFont")
title:SetPoint("TOPLEFT", PAD + 4, -PAD)
RB.SetColor(title, T.section)

local footer = RB.CreateText(col, "RetailBrowserLabelFont")
footer:SetPoint("BOTTOMLEFT", PAD + 4, PAD)
footer:SetPoint("BOTTOMRIGHT", -PAD, PAD)
local footerLine = RB.CreateFill(col, "BORDER", T.borderDim)
footerLine:ClearAllPoints()
footerLine:SetPoint("BOTTOMLEFT", PAD, FOOTER_HEIGHT)
footerLine:SetPoint("BOTTOMRIGHT", -PAD, FOOTER_HEIGHT)
footerLine:SetHeight(1)

local scroll = CreateFrame("ScrollFrame", "RetailBrowserBuildQueue", col, "UIPanelScrollFrameTemplate")
scroll:SetPoint("TOPLEFT", PAD, -PAD - 18)
scroll:SetPoint("BOTTOMRIGHT", -PAD - SCROLLBAR_SPACE, FOOTER_HEIGHT + 4)
local scrollBar = RetailBrowserBuildQueueScrollBar
local content = CreateFrame("Frame", nil, scroll)
content:SetSize(1, 1)
scroll:SetScrollChild(content)
scroll:HookScript("OnScrollRangeChanged", function(_, _, yRange) RB.Shown(scrollBar, (yRange or 0) > 0.5) end)

local rows = {}

local function CreateRow(i)
    local row = CreateFrame("Button", nil, content)
    row:SetHeight(ROW_HEIGHT)
    row:SetPoint("TOPLEFT", 0, -(i - 1) * (ROW_HEIGHT + ROW_GAP))
    row:SetPoint("RIGHT", content, "RIGHT", 0, 0)
    row:SetBackdrop(RB.FLAT_BACKDROP)
    RB.SetHoverHighlight(row)
    row.dot = RB.CreateFill(row, "ARTWORK", T.accent)
    row.dot:ClearAllPoints()
    row.dot:SetSize(DOT, DOT)
    row.dot:SetPoint("TOPLEFT", 8, -10)
    row.name = RB.CreateText(row, "RetailBrowserButtonFont")
    row.name:SetPoint("TOPLEFT", 24, -6)
    row.entry = RB.CreateText(row, "RetailBrowserValueFont", "RIGHT")
    row.entry:SetPoint("TOPRIGHT", -8, -7)
    row.name:SetPoint("RIGHT", row.entry, "LEFT", -6, 0)
    row.from = RB.CreateText(row, "RetailBrowserLabelFont")
    row.from:SetPoint("TOPLEFT", 24, -21)
    row.from:SetPoint("RIGHT", -8, 0)
    row.note = RB.CreateText(row, "RetailBrowserSmallFont")
    row.note:SetPoint("TOPLEFT", 24, -34)
    row.note:SetPoint("RIGHT", -8, 0)
    row:SetScript("OnClick", function(self) RB.SelectBuildRow(self.index) end)
    RB.AttachTip(row, "build_queueRow")
    rows[i] = row
    return row
end

local function PaintRow(row, item, selected)
    RB.SetBack(row, selected and T.accentFill or { 0, 0, 0, 0 })
    RB.SetBorder(row, selected and T.borderDim or { 0, 0, 0, 0 })
    RB.SetVertex(row.dot, item.ready and T.accent or T.pending)
    row.name:SetText(item.name)
    row.entry:SetText(item.entry or "")
    row.from:SetText(item.from)
    row.note:SetText(item.note)
    RB.SetColor(row.note, item.ready and T.label or T.pending)
end

-- queue: RB.BuildQueue's rows; selected: the index shown in the output; nextFree: the entry after the build.
function RB.ShowBuildQueue(queue, selected, nextFree)
    title:SetText("Queue · " .. #queue)
    content:SetSize(scroll:GetWidth() > 0 and scroll:GetWidth() or 260, math.max(1, #queue * (ROW_HEIGHT + ROW_GAP)))
    for i, item in ipairs(queue) do
        local row = rows[i] or CreateRow(i)
        row.index = i
        PaintRow(row, item, i == selected)
        row:Show()
    end
    for i = #queue + 1, #rows do
        rows[i]:Hide()
    end
    footer:SetText(nextFree and ("Next free after this build: " .. RB.Colored(nextFree, T.text)) or "")
    -- The bar only when the rows do not fit (the range-changed hook misses a range that stays 0).
    RB.Shown(scrollBar, content:GetHeight() > scroll:GetHeight() + 0.5)
end

scroll:SetScript("OnSizeChanged", function(self, width) content:SetWidth(width) end)
