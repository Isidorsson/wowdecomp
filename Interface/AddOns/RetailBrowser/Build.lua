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

RB.buildScope = "focused"
local selected = 1

-- Rebuilds after a pause, so typing, ticking options or stepping through rows builds once at the end.
function RB.ScheduleBuild()
    if RB.page == "build" then
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

-- The entries the scope names, of the kinds SQL is built for, at most RB.BULK_LIMIT.
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
        if d and RB.BUILD_TABLES[d.kind] and #entries < RB.BULK_LIMIT then
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

local function BuildAll(entries, start)
    local all = { sql = {}, parts = {}, lines = {}, notes = {}, firstEntry = start, tables = RB.EntryTables(entries) }
    for _, d in ipairs(entries) do
        local result, err = RetailBuildSql(d.kind, d.id, RB.BuildOptionsFor(d, start))
        if not result or result.status ~= "ok" then
            return nil, (d.name or "") .. ": " .. tostring(err or (result and result.status))
        end
        local sql = result.sql:sub(-1) == "\n" and result.sql or (result.sql .. "\n")
        all.sql[#all.sql + 1] = sql
        all.parts[#all.parts + 1] = { d = d, sql = sql }
        for _, line in ipairs(result.lines or {}) do all.lines[#all.lines + 1] = line end
        for _, note in ipairs(result.notes or {}) do all.notes[#all.notes + 1] = note end
        all.base, all.itemLevel, all.nextEntry = all.base or result.base, result.itemLevel, result.nextEntry
        start = result.nextEntry
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

local function RefreshHeader()
    local result = RB.buildResult
    RB.RefreshBuildHeader(RB.BuildScopeLabels(#RB.CheckedRows(), #RB.rows), RB.buildScope,
        result and RB.EntryRangeText(result.firstEntry, result.nextEntry) or nil)
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
    local skipped = asked - #entries
    return RB.Colored("Built " .. date("%H:%M:%S"), T.accent) .. RB.Colored(string.format(" · %d %s from %d %s · base %d%s · item level %d",
        rows, rows == 1 and "row" or "rows", #entries, #entries == 1 and "entry" or "entries", result.base,
        RB.BuildBaseTyped() and "" or " (suggested)", result.itemLevel), T.label)
        .. (skipped > 0 and " " .. RB.Colored(string.format("%d skipped (kind, or over %d).", skipped, RB.BULK_LIMIT), T.bad) or "")
end

-- Rebuilds the SQL of the scope's entries with the page's options.
function RB.BuildSql()
    if RB.page ~= "build" then
        return
    end
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
    -- What the box shows and Copy, Export and Apply write: the same SQL laid out for reading (the server's apply
    -- check passes it; only whitespace changes).
    RB.buildSqlText = RB.FormatSql(result.sql)
    RB.buildQueue = RB.BuildQueue(result.parts)
    selected = math.max(1, math.min(selected, #RB.buildQueue))
    local lone = #entries == 1 and entries[1].kind == "item"
    RB.buildMapping = MappingText(result.lines, result.notes, lone and result.base or nil)
    ShowOutput()
    RefreshHeader()
    RB.SetBuildStatus(StatusText(result, entries, asked))
    RB.RefreshRail()
end

function RB.SelectBuildRow(index)
    selected = index
    ShowOutput()
end

-- Builds when a scheduled build is due (a change paused, or the server's free entry is in).
local timer = CreateFrame("Frame", nil, RB.buildPage)
timer:SetScript("OnUpdate", function()
    if RB.buildAt and GetTime() >= RB.buildAt then
        RB.buildAt = nil
        RB.BuildSql()
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

-- The tables the build writes, in the order its rows name them.
local function BuildTables()
    local tables, seen = {}, {}
    for _, item in ipairs(RB.buildQueue or {}) do
        if not seen[item.row.table] then
            seen[item.row.table] = true
            tables[#tables + 1] = item.row.table
        end
    end
    return tables
end

-- Writes to the live world DB, so Apply first asks in the page's confirm bar.
function RB.AskApply()
    local result = RB.buildResult
    if not result or RB.BuildFullSql() == "" then
        return
    end
    RB.ShowApplyConfirm(RB.ApplyConfirmText(BuildTables(), result.firstEntry, result.nextEntry), result.nextEntry - result.firstEntry)
end

function RB.ApplyBuildSql()
    RB.HideApplyConfirm()
    local sql, result = RB.BuildFullSql(), RB.buildResult
    if sql == "" or not result then
        return
    end
    local started = RB.ApplySql(result.firstEntry, result.nextEntry - 1, sql, function(ok, text)
        RB.SetBuildStatus(RB.Colored(ok and "Applied: " or "Not applied: ", ok and T.good or T.bad) .. text)
        RB.Print((ok and "applied: " or "apply failed: ") .. text)
    end)
    RB.SetBuildStatus(started and string.format("Applying %.1f KB to the world DB...", #sql / 1024)
        or RB.Colored("An apply is still running.", T.bad))
end

function RB.SetBuildScope(scope)
    RB.buildScope = scope
    RefreshHeader()
    RB.ScheduleBuild()
    RB.RefreshRail()
end

function RB.OnChecksChanged()
    RefreshHeader()
    RB.RefreshRail()
    if RB.buildScope ~= "focused" then
        RB.ScheduleBuild()
    end
end

-- Opens the page for a scope (focused, checked, page).
function RB.ShowBuild(scope)
    RB.SetBuildScope(scope or RB.buildScope)
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
