-- Build SQL: world DB SQL for the focused entry, the checked rows or the page (RetailBuildSql per entry, each starting
-- at the entry the previous one left free), shown on the Build SQL page (BuildPage.lua). Every statement copies a stock
-- row (the base entry) and overrides only the columns it names; an empty start entry asks the server for a free run
-- (FreeEntry.lua). Retail stats are computed at the item level and mapped to 3.3.5 stats as listed. Apply writes the
-- SQL to the world DB after the page's confirm bar.

local RB = RetailBrowser
local T = RB.THEME

local DEFAULT_ITEM_LEVEL = 200
local DEFAULT_START_ENTRY = 9000000
local MAPPING_BYTES = 4096
-- A build pauses after this many entries until the next frame, so hundreds of rows never freeze the client.
local BUILD_STEP = 20

RB.buildScope = "focused"
local selected = 1

-- The running build (a coroutine resumed each frame) and the row-by-row apply (RB.applyJob, BuildModel.lua).
local buildJob

local function ApplyRunning()
    local job = RB.applyJob
    return job and not job.done and not job.failed
end

-- Rebuilds after a pause, so typing, ticking options or stepping through rows builds once at the end. Not while an
-- apply is writing: its rows are the ones on screen.
function RB.ScheduleBuild()
    if RB.page == "build" and not ApplyRunning() then
        RB.buildAt = GetTime() + RB.SEARCH_DELAY
        RB.SetBuildStatus("Building...")
    end
end

-- The base item's armour, damage and delay from its tooltip; nil until the client has the item (it asks the server, so
-- the next build shows it).
local scan = CreateFrame("GameTooltip", "RetailBrowserBuildScan", nil, "GameTooltipTemplate")

local function BaseCombat(entry)
    scan:SetOwner(UIParent, "ANCHOR_NONE")
    scan:SetHyperlink("item:" .. entry)
    if not GetItemInfo(entry) then
        scan:Hide()
        return nil
    end
    local base = {}
    for i = 1, scan:NumLines() do
        for _, side in ipairs({ "Left", "Right" }) do
            local text = _G["RetailBrowserBuildScanText" .. side .. i]:GetText() or ""
            base.armor = base.armor or tonumber(text:match("^(%d+) Armor$"))
            local min, max = text:match("^(%d+) %- (%d+) ")
            if min and not base.dmg_min1 then
                base.dmg_min1, base.dmg_max1 = tonumber(min), tonumber(max)
            end
            local speed = tonumber(text:match("^Speed (%d+%.%d+)"))
            base.delay = base.delay or (speed and math.floor(speed * 1000 + 0.5))
        end
    end
    scan:Hide()
    return base
end

-- The base's value beside a retail armour or damage line (one item built), so the two can be compared.
local COMBAT_COLUMNS = { armor = true, dmg_min1 = true, dmg_max1 = true, delay = true }
local function CombatText(line, base)
    if not base or not COMBAT_COLUMNS[line.classic] then
        return ""
    end
    if base == true then
        return "  " .. RB.Colored("(base: not known yet, Build again)", T.label)
    end
    return "  " .. RB.Colored("(base " .. tostring(base[line.classic] or (line.classic == "armor" and 0) or "?") .. ")", T.combat)
end

-- The stat lines up to MAPPING_BYTES (a set of every variant has hundreds), then the notes. baseEntry: the one
-- item's base, whose armour and damage the combat lines are shown beside.
local function MappingText(lines, notes, baseEntry)
    local out, bytes = {}, 0
    local base = baseEntry and baseEntry > 0 and (BaseCombat(baseEntry) or true)
    for i, line in ipairs(lines) do
        local text = string.format("%s: %s %d -> %s %d", line.piece, line.retail, line.retailValue, line.classic, line.value) .. CombatText(line, base)
        if line.note ~= "" then
            text = text .. "  " .. RB.Colored("(" .. line.note .. ")", T.label)
        end
        bytes = bytes + #text + 1
        if bytes > MAPPING_BYTES then
            out[#out + 1] = RB.Colored(string.format("... %d more stat lines (the SQL has them all).", #lines - i + 1), T.label)
            break
        end
        out[#out + 1] = text
    end
    if #out == 0 then
        out[1] = "No stats to map."
    end
    for _, note in ipairs(notes) do
        out[#out + 1] = RB.Colored("Note:", T.title) .. " " .. note
    end
    return table.concat(out, "\n")
end

-- The entries the scope names, of the kinds SQL is built for.
local function ScopeEntries()
    local list = {}
    if RB.buildScope == "focused" then
        list[1] = RB.details
    else
        for _, row in ipairs(RB.buildScope == "checked" and RB.CheckedRows() or RB.rows) do
            list[#list + 1] = row.d or RB.DetailsOf(row.kind, row.id)
        end
    end
    local entries = {}
    for _, d in ipairs(list) do
        if d and RB.BUILD_TABLES[d.kind] then
            entries[#entries + 1] = d
        end
    end
    return entries, #list
end

-- The rail's count: the rows the scope names.
function RB.BuildScopeCount()
    if RB.buildScope == "focused" then
        return RB.details and RB.BUILD_TABLES[RB.details.kind] and 1 or 0
    end
    return RB.buildScope == "checked" and #RB.CheckedRows() or #RB.rows
end

-- Adds one entry's built SQL to the whole build.
local function AddPart(all, d, result)
    local sql = result.sql:sub(-1) == "\n" and result.sql or (result.sql .. "\n")
    all.sql[#all.sql + 1] = sql
    all.parts[#all.parts + 1] = { d = d, sql = sql }
    for _, line in ipairs(result.lines or {}) do all.lines[#all.lines + 1] = line end
    for _, note in ipairs(result.notes or {}) do all.notes[#all.notes + 1] = note end
    all.base, all.itemLevel, all.nextEntry = all.base or result.base, result.itemLevel, result.nextEntry
end

-- Builds each entry from start on. An entry that cannot be built (no stock row to copy, no display) is skipped and
-- listed with why, so one odd row does not stop a build of many; a lone entry, or a build where none can be built,
-- fails with the first error instead.
local function BuildAll(entries, start)
    local all = { sql = {}, parts = {}, lines = {}, notes = {}, skipped = {}, firstEntry = start, tables = RB.EntryTables(entries) }
    local firstError
    for i, d in ipairs(entries) do
        if i % BUILD_STEP == 0 and coroutine.running() then
            RB.SetBuildStatus(RB.BuildProgressText(i, #entries))
            coroutine.yield()
        end
        local result, err = RetailBuildSql(d.kind, d.id, RB.BuildOptionsFor(d, start))
        if result and result.status == "ok" then
            AddPart(all, d, result)
            start = result.nextEntry
        else
            local reason = tostring(err or (result and result.status))
            firstError = firstError or ((d.name or "") .. ": " .. reason)
            if #entries == 1 then
                return nil, firstError
            end
            all.skipped[#all.skipped + 1] = { d = d, reason = reason }
        end
    end
    if #all.parts == 0 then
        return nil, firstError
    end
    all.sql = table.concat(all.sql, "\n")
    return all
end

-- An empty Start entry builds once to learn how many entries the scope takes, then again at the server's first free
-- run of that length. While the server is asking (or silent) the first build's fallback start stands.
local function BuildAtFree(entries)
    local typed = RB.BuildTypedStart()
    local first, err = BuildAll(entries, typed or DEFAULT_START_ENTRY)
    if typed or not first then
        return first, err
    end
    local free, hint = RB.FreeStart(first, DEFAULT_START_ENTRY)
    RB.SetStartEntryHint(hint)
    if free and free ~= first.firstEntry then return BuildAll(entries, free) end
    return first
end

-- The Apply button: "Writing..." while rows go out, "Retry N rows" after a failed row.
local function RefreshApplyButton()
    local job = RB.applyJob
    if ApplyRunning() then
        RB.SetApplyButton("Writing...", false)
    elseif job and job.failed then
        RB.SetApplyButton("Retry " .. RB.ApplyJobRemaining(job) .. " rows", true)
    else
        RB.SetApplyButton("Apply to world DB", RB.buildResult ~= nil)
    end
end

local function RefreshHeader()
    local result = RB.buildResult
    local checked = #RB.CheckedRows()
    RB.RefreshBuildHeader(RB.BuildScopeLabels(checked, #RB.rows), RB.buildScope,
        result and RB.EntryRangeText(result.firstEntry, result.nextEntry) or nil, checked)
    RefreshApplyButton()
end

-- The output for the selected queue row.
local function ShowOutput()
    local result = RB.buildResult
    local queue = RB.buildQueue or {}
    RB.ShowBuildQueue(queue, selected, result and result.nextEntry)
    RB.ShowBuildOutput({ sql = RB.buildSqlText, rowCount = #queue, item = queue[selected], mapping = RB.buildMapping,
        creatureOnly = result and result.tables == "creature" })
end

local function Fail(text)
    RB.buildResult, RB.buildQueue, RB.buildMapping, RB.buildSqlText = nil, {}, "", nil
    ShowOutput()
    RefreshHeader()
    RB.SetBuildStatus(RB.Colored(text, T.bad))
end

-- "3 rows from 3 entries · base 15166 (suggested) · item level 200", then what was skipped.
local function StatusText(result, entries, asked)
    local rows = result.nextEntry - result.firstEntry
    local built = #result.parts
    local skipped = asked - #entries
    local failed = #result.skipped
    return RB.Colored("Built " .. date("%H:%M:%S"), T.accent) .. RB.Colored(string.format(" · %d %s from %d %s · base %d%s · item level %d",
        rows, rows == 1 and "row" or "rows", built, built == 1 and "entry" or "entries", result.base,
        RB.BuildBaseTyped() and "" or " (suggested)", result.itemLevel), T.label)
        .. (failed > 0 and " " .. RB.Colored(string.format("%d could not be built (end of the queue).", failed), T.pending) or "")
        .. (skipped > 0 and " " .. RB.Colored(string.format("%d skipped (no SQL for their kind).", skipped), T.bad) or "")
end

-- Builds the SQL of the scope's entries with the page's options; runs as a coroutine (BuildAll yields every
-- BUILD_STEP entries). A new SQL ends any finished or failed apply job: its rows are not these.
local function RunBuild()
    local entries, asked = ScopeEntries()
    RB.SetItemOptionsOpen(RB.ItemOptionsOpen(entries))
    if #entries == 0 then
        Fail(asked == 0 and "Nothing to build: focus a row or check some." or "SQL is built for items, sets, creatures, models and mounts.")
        return
    end
    local result, err = BuildAtFree(entries)
    RB.BuildRetailHints(entries, result, DEFAULT_ITEM_LEVEL)
    if not result then
        Fail(err)
        return
    end
    RB.buildResult = result
    RB.applyJob = nil
    -- What the box shows and Copy, Export and Apply write: the same SQL laid out for reading (the server's apply
    -- check passes it; only whitespace changes).
    RB.buildSqlText = RB.FormatSql(result.sql)
    RB.buildQueue = RB.BuildQueue(result.parts, result.skipped)
    selected = math.max(1, math.min(selected, #RB.buildQueue))
    local lone = #entries == 1 and entries[1].kind == "item"
    RB.buildMapping = MappingText(result.lines, result.notes, lone and result.base or nil)
    ShowOutput()
    RefreshHeader()
    RB.SetBuildStatus(StatusText(result, entries, asked))
    RB.RefreshRail()
end

local function StepBuild()
    local ok, err = coroutine.resume(buildJob)
    if not ok then
        buildJob = nil
        Fail("Build failed: " .. tostring(err))
        RB.Print("build failed: " .. tostring(err))
    elseif coroutine.status(buildJob) == "dead" then
        buildJob = nil
    end
end

-- Starts a build (replacing one still running); a short build finishes in this call, a long one over a few frames.
function RB.BuildSql()
    if RB.page ~= "build" or ApplyRunning() then
        return
    end
    buildJob = coroutine.create(RunBuild)
    StepBuild()
end

function RB.SelectBuildRow(index)
    selected = index
    ShowOutput()
end

-- Builds when a scheduled build is due (a change paused, or the server's free entry is in), and steps a running one.
local timer = CreateFrame("Frame", nil, RB.buildPage)
timer:SetScript("OnUpdate", function()
    if RB.buildAt and GetTime() >= RB.buildAt then
        RB.buildAt = nil
        RB.BuildSql()
    elseif buildJob then
        StepBuild()
    end
end)

local function ExportName()
    local d = RB.details
    if RB.buildScope ~= "focused" or not d then
        return "build-" .. RB.state.kind
    end
    return d.kind .. "-" .. RB.PrimaryId(d)
end

function RB.CopyBuildSql()
    local sql = RB.BuildFullSql()
    RB.Copy(sql, "SQL")
    if sql ~= "" then RB.ReserveBuilt(RB.buildResult) end
end

function RB.ExportBuildSql()
    local sql = RB.BuildFullSql()
    if sql == "" then
        return
    end
    local path, err = RetailExportSql(ExportName(), sql)
    RB.Print(path and ("wrote " .. path) or ("could not export: " .. tostring(err)))
    if path then RB.ReserveBuilt(RB.buildResult) end
end


-- Writes to the live world DB, so Apply first asks in the page's confirm bar: the whole build, or after a failed
-- row the rows not written yet.
local pendingRows

function RB.AskApply()
    local job, result = RB.applyJob, RB.buildResult
    if job and job.failed then
        local remaining = RB.ApplyJobRemaining(job)
        RB.ShowApplyConfirm(string.format("Retry the %d rows not written, from row %d / %d? Rows already written are not sent again.",
            remaining, job.index, #job.rows), remaining)
        return
    end
    if not result or ApplyRunning() then
        return
    end
    pendingRows = RB.ApplyRows(RB.BuildFullSql())
    if #pendingRows == 0 then
        return
    end
    RB.ShowApplyConfirm(RB.ApplyConfirmText(RB.BuildTables(RB.buildQueue or {}), result.firstEntry, result.nextEntry),
        result.nextEntry - result.firstEntry)
end

local function ApplyDone(job)
    local text = string.format("%d %s written.", #job.rows, #job.rows == 1 and "row" or "rows")
    RB.SetBuildStatus(RB.Colored("Applied: ", T.good) .. text)
    RB.Print("applied: " .. text)
end

local function ApplyStopped(job, row, text)
    RB.SetBuildStatus(RB.Colored(string.format("Row %d (entry %d) not written: ", job.index, row.entry), T.bad) .. text
        .. RB.Colored(string.format("  %d rows not written: Retry sends them.", RB.ApplyJobRemaining(job)), T.label))
    RB.Print(string.format("row %d / %d (entry %d) failed: %s", job.index, #job.rows, row.entry, text))
end

-- Sends the job's next row; the server's answer sends the one after, until the last is written or one fails.
local function SendNextRow()
    local job = RB.applyJob
    local row = RB.ApplyJobNext(job)
    if not row then
        if job.done then ApplyDone(job) end
        RefreshHeader()
        ShowOutput()
        return
    end
    RB.SetBuildStatus(RB.ApplyProgressText(job.index, #job.rows, row.entry))
    local started = RB.ApplySql(row.entry, row.entry, row.sql, function(ok, text)
        RB.ApplyJobFinished(job, ok, text)
        if ok then
            return SendNextRow()
        end
        ApplyStopped(job, row, text)
        RefreshHeader()
        ShowOutput()
    end)
    if not started then
        RB.ApplyJobFinished(job, false, "another apply is still running")
    end
    RefreshHeader()
    ShowOutput()
end

function RB.ApplyBuildSql()
    RB.HideApplyConfirm()
    local job = RB.applyJob
    if job and job.failed then
        RB.ApplyJobRetry(job)
    elseif pendingRows and #pendingRows > 0 and not ApplyRunning() then
        RB.applyJob, pendingRows = RB.ApplyJob(pendingRows), nil
    else
        return
    end
    SendNextRow()
end

-- Drops the written rows from the queue (the trash button); the SQL, Copy, Export and the apply job keep them.
function RB.ClearWrittenRows()
    if ApplyRunning() then
        return
    end
    RB.buildQueue = RB.WithoutWritten(RB.buildQueue or {}, RB.applyJob)
    selected = math.max(1, math.min(selected, #RB.buildQueue))
    ShowOutput()
end

function RB.SetBuildScope(scope)
    RB.buildScope = scope
    RefreshHeader()
    RB.ScheduleBuild()
    RB.RefreshRail()
end

-- A scope picked on the page's switch: it stands until the checks change.
function RB.ChooseBuildScope(scope)
    RB.buildScopeChosen = scope
    RB.SetBuildScope(scope)
end

-- New checks drop a chosen scope: the build follows them again (RB.DefaultBuildScope).
function RB.OnChecksChanged()
    RB.buildScopeChosen = nil
    RB.SetBuildScope(RB.BuildScopeFor(#RB.CheckedRows(), nil))
end

-- Opens the page: the checked rows when there are some, else the focused row, unless a scope was chosen on the
-- page since the checks last changed. The B key, the action, the bulk bar and the rail all come here.
function RB.ShowBuild()
    RB.SetBuildScope(RB.BuildScopeFor(#RB.CheckedRows(), RB.buildScopeChosen))
    local d = RB.details
    local first = RB.CheckedRows()[1]
    if (not d or not RB.BUILD_TABLES[d.kind]) and first then
        RB.ShowDetails(first.kind, first.id, false)
    end
    RB.ShowPage("build")
end

function RB.OnBuildPageShown()
    RB.ForgetFreeEntries()
    RB.buildAt = nil
    RB.BuildSql()
end
