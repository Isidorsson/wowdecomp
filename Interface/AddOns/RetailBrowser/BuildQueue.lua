-- The Build SQL page's queue: a row per entry the build writes (BuildModel.lua's RB.BuildQueue), with its name,
-- "from <source id> · display <id>", a note and the new entry; a teal dot is ready, a yellow one needs a look (an
-- installed display row wins, or the source has several displays). A click shows that row in the output. The
-- next free entry after the build is at the bottom. While Apply writes, a strip under the title fills row by row, the
-- row being written pulses and the list follows it; the trash button then drops the written rows from the list.

local RB = RetailBrowser
local T = RB.THEME

local PAD = 10
local ROW_HEIGHT = 48
local ROW_GAP = 2
local DOT = 8
local FOOTER_HEIGHT = 28
local SCROLLBAR_SPACE = 20
local STRIP_TOP = PAD + 20
local STRIP_HEIGHT = 3
local LIST_TOP = STRIP_TOP + STRIP_HEIGHT + 5
local CLEAR_WIDTH = 74
local PULSE_SPEED = 5
local WRITING_FILL = { T.pending[1], T.pending[2], T.pending[3], 0.10 }

local col = RB.buildQueueCol
local title = RB.CreateText(col, "RetailBrowserHeadFont")
title:SetPoint("TOPLEFT", PAD + 4, -PAD)
RB.SetColor(title, T.section)

-- The apply's progress: written rows over the job's rows, red once it stops at a failed row.
local strip = RB.CreateFill(col, "ARTWORK", T.borderDim)
strip:ClearAllPoints()
strip:SetPoint("TOPLEFT", PAD + 4, -STRIP_TOP)
strip:SetPoint("TOPRIGHT", -PAD, -STRIP_TOP)
strip:SetHeight(STRIP_HEIGHT)
local stripFill = RB.CreateFill(col, "OVERLAY", T.accent)
stripFill:ClearAllPoints()
stripFill:SetPoint("TOPLEFT", strip, "TOPLEFT")
stripFill:SetHeight(STRIP_HEIGHT)

-- The trash button: a can drawn from fills (3.3.5 has no trash art) and how many written rows it clears.
local clear = RB.CreateButton(col, "", CLEAR_WIDTH, 18, function() RB.ClearWrittenRows() end)
clear:SetPoint("TOPRIGHT", -PAD, -PAD + 3)
RB.AttachTip(clear, "build_clearWritten")
local function CanPart(width, height, x, y, color)
    local part = RB.CreateFill(clear, "OVERLAY", color or T.buttonText)
    part:ClearAllPoints()
    part:SetSize(width, height)
    part:SetPoint("TOPLEFT", x, y)
end
CanPart(4, 1, 10, -3)
CanPart(10, 2, 7, -4)
CanPart(8, 9, 8, -7)
CanPart(1, 6, 10, -8, T.button)
CanPart(1, 6, 13, -8, T.button)

local footer = RB.CreateText(col, "RetailBrowserLabelFont")
footer:SetPoint("BOTTOMLEFT", PAD + 4, PAD)
footer:SetPoint("BOTTOMRIGHT", -PAD, PAD)
local footerLine = RB.CreateFill(col, "BORDER", T.borderDim)
footerLine:ClearAllPoints()
footerLine:SetPoint("BOTTOMLEFT", PAD, FOOTER_HEIGHT)
footerLine:SetPoint("BOTTOMRIGHT", -PAD, FOOTER_HEIGHT)
footerLine:SetHeight(1)

local scroll = CreateFrame("ScrollFrame", "RetailBrowserBuildQueue", col, "UIPanelScrollFrameTemplate")
scroll:SetPoint("TOPLEFT", PAD, -LIST_TOP)
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

-- While an apply runs (Build.lua's RB.applyJob), a row's note is where its write is.
local APPLY_NOTES = {
    waiting = { "waiting to write", T.label }, writing = { "writing...", T.pending },
    written = { "written", T.accent }, failed = { "not written", T.bad },
}

local function PaintRow(row, item, selected)
    local job = RB.applyJob
    local state = job and item.entry and job.state[item.entry]
    RB.SetBack(row, selected and T.accentFill or state == "writing" and WRITING_FILL or { 0, 0, 0, 0 })
    RB.SetBorder(row, selected and T.borderDim or { 0, 0, 0, 0 })
    -- Teal ready, yellow needs a look, red could not be built (it has no entry and writes nothing).
    local flag = item.skipped and T.bad or (item.ready and T.accent or T.pending)
    local note, noteColor = item.note, item.ready and T.label or flag
    local apply = APPLY_NOTES[state or ""]
    if apply then
        note, noteColor = apply[1], apply[2]
        if state == "failed" then note = note .. ": " .. tostring(job.failed) end
        flag = noteColor == T.label and flag or noteColor
    end
    RB.SetVertex(row.dot, flag)
    row.dot:SetAlpha(1)
    row.name:SetText(item.name)
    row.entry:SetText(item.entry or "")
    row.from:SetText(item.from)
    row.note:SetText(note)
    RB.SetColor(row.note, noteColor)
    return state
end

-- The row being written pulses until its answer comes.
local writingRow
col:SetScript("OnUpdate", function()
    if writingRow then
        writingRow.dot:SetAlpha(0.35 + 0.65 * math.abs(math.sin(GetTime() * PULSE_SPEED)))
    end
end)

-- Keeps the row being written in the middle of the list.
local function Follow(index)
    scroll:UpdateScrollChildRect()
    local top = (index - 1) * (ROW_HEIGHT + ROW_GAP) - (scroll:GetHeight() - ROW_HEIGHT) / 2
    scroll:SetVerticalScroll(math.max(0, math.min(top, scroll:GetVerticalScrollRange())))
end

local function ShowProgress(job, queue)
    local written = job and RB.ApplyJobWritten(job) or 0
    RB.Shown(strip, job ~= nil)
    RB.Shown(stripFill, written > 0)
    if written > 0 then
        -- The column is a fixed width; the strip (a texture between two anchors) can report a stale one.
        local stripWidth = col:GetWidth() - (PAD + 4) - PAD
        stripFill:SetWidth(math.max(1, stripWidth * written / #job.rows))
        RB.SetVertex(stripFill, job.failed and T.bad or T.accent)
    end
    local running = job and not job.done and not job.failed
    local shown = #queue - #RB.WithoutWritten(queue, job)
    clear:SetLabel("Clear " .. shown)
    clear.label:ClearAllPoints()
    clear.label:SetPoint("LEFT", 22, 0)
    clear.label:SetPoint("RIGHT", -6, 0)
    RB.Shown(clear, shown > 0)
    clear:SetEnabled(not running)
end

-- queue: RB.BuildQueue's rows; selected: the index shown in the output; nextFree: the entry after the build.
function RB.ShowBuildQueue(queue, selected, nextFree)
    title:SetText(RB.ApplyQueueTitle(#queue, RB.applyJob))
    content:SetSize(scroll:GetWidth() > 0 and scroll:GetWidth() or 260, math.max(1, #queue * (ROW_HEIGHT + ROW_GAP)))
    writingRow = nil
    local writingIndex
    for i, item in ipairs(queue) do
        local row = rows[i] or CreateRow(i)
        row.index = i
        if PaintRow(row, item, i == selected) == "writing" then
            writingRow, writingIndex = row, i
        end
        row:Show()
    end
    for i = #queue + 1, #rows do
        rows[i]:Hide()
    end
    footer:SetText(nextFree and ("Next free after this build: " .. RB.Colored(nextFree, T.text)) or "")
    -- The bar only when the rows do not fit (the range-changed hook misses a range that stays 0).
    RB.Shown(scrollBar, content:GetHeight() > scroll:GetHeight() + 0.5)
    ShowProgress(RB.applyJob, queue)
    if writingIndex then Follow(writingIndex) end
end

scroll:SetScript("OnSizeChanged", function(self, width) content:SetWidth(width) end)
