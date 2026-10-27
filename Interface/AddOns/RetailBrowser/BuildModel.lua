-- The Build SQL page's data, built from RetailBuildSql results without touching a frame (Tests/ runs it): the rows a
-- build writes (each "INSERT INTO t (columns) SELECT values FROM t WHERE entry = base" copies the base row and
-- overrides the values that are not the column itself), the queue the page lists, the columns a row changes, the
-- SQL laid out for reading (whitespace only) and its preview, and the page's labels and hints.

local RB = RetailBrowser

local SEPARATOR = " · "
local MAX_CREATURE_ROWS = 50
local LIST_ITEMS_PER_LINE = 6
local LIST_INDENT = "  "

-- What a creature, model or mount with several displays builds (RetailBuildSql's displays option).
RB.DISPLAY_MODES = { { 1, "One creature each" }, { 0, "Picked only" }, { 2, "modelid1-4 variants" } }

-- Splits a column or SELECT list on its top-level commas; a quoted string or `name` keeps its commas ('' and \'
-- escape a quote).
local function SplitValues(list)
    local values, current, quoted, i = {}, {}, false, 1
    while i <= #list do
        local char = list:sub(i, i)
        if quoted == "'" and (char == "\\" or (char == "'" and list:sub(i + 1, i + 1) == "'")) then
            current[#current + 1] = list:sub(i, i + 1)
            i = i + 1
        elseif char == "'" or char == "`" then
            if not quoted then
                quoted = char
            elseif quoted == char then
                quoted = false
            end
            current[#current + 1] = char
        elseif char == "," and not quoted then
            values[#values + 1] = table.concat(current):match("^%s*(.-)%s*$")
            current = {}
        else
            current[#current + 1] = char
        end
        i = i + 1
    end
    values[#values + 1] = table.concat(current):match("^%s*(.-)%s*$")
    return values
end

local function Unquote(value)
    local inner = value and value:match("^'(.*)'$")
    return inner and (inner:gsub("''", "'"):gsub("\\'", "'")) or value
end

local function ParseRow(block)
    local tableName, columnList = block:match("INSERT INTO `([%w_]+)` %((.-)%)\nSELECT ")
    if not tableName then
        return nil
    end
    local row = { text = block, table = tableName, columns = {}, by = {} }
    for column in columnList:gmatch("`([^`]+)`") do
        row.columns[#row.columns + 1] = column
    end
    row.values = SplitValues(block:match("\nSELECT (.-)\nFROM ") or "")
    for i, column in ipairs(row.columns) do
        row.by[column] = row.values[i]
    end
    row.entry = tonumber(row.by.entry)
    row.base = tonumber(block:match("\nFROM `[%w_]+` WHERE `entry` = (%d+);"))
    row.name = Unquote(row.by.name)
    row.display = tonumber(row.by.modelid1 or row.by.displayid)
    return row
end

-- The rows of a build's SQL (blank lines part them).
function RB.SqlRows(sql)
    local rows = {}
    for block in ((sql or "") .. "\n\n"):gmatch("(.-)\n\n") do
        local row = ParseRow(block)
        if row then
            rows[#rows + 1] = row
        end
    end
    return rows
end

-- The columns a row overrides (its value is not the base row's column): { { column, value } }.
function RB.ChangesVsBase(row)
    local changes = {}
    for i, column in ipairs(row.columns) do
        if row.values[i] ~= "`" .. column .. "`" then
            changes[#changes + 1] = { column = column, value = row.values[i] }
        end
    end
    return changes
end

-- The badge of the display a row writes: its entry in the source's displays, appearances or pieces.
local function RowBadge(d, display)
    local item = d.kind == "item" or d.kind == "set"
    for _, list in ipairs({ d.displays or {}, d.appearances or {}, d.pieces or {} }) do
        for _, entry in ipairs(list) do
            if entry.displayId == display then
                return RB.BadgeOf(entry.custom, display, item)
            end
        end
    end
end

local BADGE_NOTES = { ["3.3.5"] = "stock 3.3.5 row wins", custom = "custom row wins" }

local function DisplayCount(d)
    return d.displayCount or #(d.displays or {})
end

-- Why a row was not built, short enough for the queue's note line.
local function SkipNote(reason)
    if reason:find("no stock 3.3.5", 1, true) then
        return "skipped: no stock item to copy, set a base entry"
    end
    return "skipped: " .. reason
end

-- parts: { { d = details, sql = that entry's SQL } } in build order; skipped: { { d = details, reason = the error } }
-- for entries that could not be built. Returns the queue: a row per statement with its name, "from <id> · display
-- <id>", a note and whether it is ready (retail's row is served, one display), then a row per skipped entry, whose
-- SQL is a comment saying why.
function RB.BuildQueue(parts, skipped)
    local queue = {}
    for _, part in ipairs(parts) do
        local count = DisplayCount(part.d)
        for _, row in ipairs(RB.SqlRows(part.sql)) do
            local note = BADGE_NOTES[RowBadge(part.d, row.display) or ""]
                or (RB.BUILD_TABLES[part.d.kind] == "creature" and count > 1 and count .. " displays") or nil
            queue[#queue + 1] = { name = row.name or "", entry = row.entry, row = row, d = part.d, ready = note == nil,
                note = note or "ready", from = "from " .. RB.PrimaryId(part.d) .. SEPARATOR .. "display " .. (row.display or 0) }
        end
    end
    for _, skip in ipairs(skipped or {}) do
        queue[#queue + 1] = { name = skip.d.name or "", d = skip.d, ready = false, skipped = true, note = SkipNote(skip.reason),
            from = "from " .. RB.PrimaryId(skip.d), row = { text = "-- skipped: " .. skip.reason .. "\n", columns = {}, values = {} } }
    end
    return queue
end

-- "9000000" for one entry, "9000000–9000002" for a run (nextEntry is the first entry after it).
function RB.EntryRangeText(first, nextEntry)
    if nextEntry - first <= 1 then
        return tostring(first)
    end
    return first .. "–" .. (nextEntry - 1)
end

-- What Build SQL builds when it opens: the checked rows when there are some, else the focused row.
function RB.DefaultBuildScope(checked)
    return checked > 0 and "checked" or "focused"
end

-- The scope chosen on the page's switch stands until the checks change (chosen is then nil).
function RB.BuildScopeFor(checked, chosen)
    return chosen or RB.DefaultBuildScope(checked)
end

function RB.BuildScopeLabels(checked, page)
    return { focused = "Focused row", checked = "Checked " .. checked, page = "Whole page " .. page }
end

local MODE_HINTS = {
    [1] = "A creature per display, named <name> (<display>); at most " .. MAX_CREATURE_ROWS .. " rows.",
    [0] = "One creature with the picked display only.",
    [2] = "One creature: the picked display in modelid1, the next three in modelid2-4 (3.3.5 has four); more are left out.",
}

function RB.DisplayModeHint(mode)
    return MODE_HINTS[mode]
end

-- The item options matter when the build has an item or a set.
function RB.ItemOptionsOpen(entries)
    for _, d in ipairs(entries) do
        if d.kind == "item" or d.kind == "set" then
            return true
        end
    end
    return false
end

-- The tables a build writes, in the order its queue names them; rows that were not built name none.
function RB.BuildTables(queue)
    local tables, seen = {}, {}
    for _, item in ipairs(queue) do
        local name = item.row and item.row.table
        if name and not seen[name] then
            seen[name] = true
            tables[#tables + 1] = name
        end
    end
    return tables
end

-- The confirm bar's question before Apply: tables, how many rows and the entry range.
function RB.ApplyConfirmText(tables, first, nextEntry)
    local count = nextEntry - first
    return string.format("Write %d %s %s (%s) to the world DB? Rows with these entries are replaced.", count,
        table.concat(tables, " and "), count == 1 and "row" or "rows", RB.EntryRangeText(first, nextEntry))
        .. (count > 1 and " Written one row at a time, top to bottom." or "")
end

function RB.ApplyProgressText(index, count, entry)
    return string.format("Writing row %d / %d · entry %d...", index, count, entry)
end

function RB.BuildProgressText(done, total)
    return string.format("Building %d / %d...", done, total)
end

-- Apply goes a row at a time. A build's SQL is blocks parted by blank lines, a block per row whose DELETE names its
-- entry; that line stays whole in the readable layout and in the user's edits, so it is what the SQL is split on. A
-- block with no entry (a note the user typed) goes with the row before it, or the first row, so nothing is dropped.
local DELETE_ENTRY = "DELETE FROM `[%w_]+` WHERE `entry` = (%d+);"

-- The SQL as { entry, sql } per row, top to bottom.
function RB.ApplyRows(sql)
    local rows, leading = {}, {}
    for block in ((sql or "") .. "\n\n"):gmatch("(.-)\n\n") do
        if block:match("%S") then
            local entry = tonumber(block:match(DELETE_ENTRY))
            if entry then
                rows[#rows + 1] = { entry = entry, blocks = { block } }
            elseif #rows > 0 then
                table.insert(rows[#rows].blocks, block)
            else
                leading[#leading + 1] = block
            end
        end
    end
    for i, row in ipairs(rows) do
        local blocks = i == 1 and { unpack(leading) } or {}
        for _, block in ipairs(row.blocks) do blocks[#blocks + 1] = block end
        rows[i] = { entry = row.entry, sql = table.concat(blocks, "\n\n") .. "\n" }
    end
    return rows
end

-- An apply job: every row's state (waiting, writing, written, failed) and the row it is at. One row is sent at a
-- time; a failed row stops the job until it is retried, which starts again at that row.
function RB.ApplyJob(rows)
    local job = { rows = rows, index = 1, state = {}, done = #rows == 0 }
    for _, row in ipairs(rows) do job.state[row.entry] = "waiting" end
    return job
end

-- The next row to send, marked writing; nil when the job is done or stopped at a failure.
function RB.ApplyJobNext(job)
    local row = not job.done and not job.failed and job.rows[job.index]
    if not row then
        return nil
    end
    job.state[row.entry] = "writing"
    return row
end

function RB.ApplyJobFinished(job, ok, text)
    job.state[job.rows[job.index].entry] = ok and "written" or "failed"
    if ok then
        job.index = job.index + 1
        job.done = job.index > #job.rows
    else
        job.failed = text or "failed"
    end
end

-- Rows not written yet: the failed one and every later one.
function RB.ApplyJobRemaining(job)
    return #job.rows - job.index + 1
end

function RB.ApplyJobWritten(job)
    return job.index - 1
end

function RB.ApplyJobRetry(job)
    job.failed = nil
    for i = job.index, #job.rows do job.state[job.rows[i].entry] = "waiting" end
end

-- The queue's title: its length, or where an apply is ("Writing 37 / 93").
function RB.ApplyQueueTitle(count, job)
    if not job then
        return "Queue · " .. count
    end
    local verb = job.done and "Written" or job.failed and "Stopped at" or "Writing"
    return string.format("%s %d / %d", verb, job.done and #job.rows or job.index, #job.rows)
end

-- The queue without the rows the job has written (Clear written); waiting, failed and skipped rows stay, in order.
function RB.WithoutWritten(queue, job)
    local kept = {}
    for _, item in ipairs(queue) do
        if not (job and item.entry and job.state[item.entry] == "written") then kept[#kept + 1] = item end
    end
    return kept
end

-- A list's items, LIST_ITEMS_PER_LINE to an indented line.
local function ListLines(list, out)
    local items = SplitValues(list)
    for first = 1, #items, LIST_ITEMS_PER_LINE do
        local last = math.min(first + LIST_ITEMS_PER_LINE - 1, #items)
        out[#out + 1] = LIST_INDENT .. table.concat(items, ", ", first, last) .. (last < #items and "," or "")
    end
end

-- The SQL laid out for reading: an INSERT's column list and its SELECT list a few items to a line. Only whitespace
-- outside quotes and backticks changes; comments, DELETE and the FROM ... WHERE line stay as written (the server's
-- apply check wants those on one line). Formatting twice changes nothing.
function RB.FormatSql(sql)
    local out = {}
    for line in ((sql or "") .. "\n"):gmatch("(.-)\n") do
        local insert, columns = line:match("^(INSERT INTO `[%w_]+` )%((.*)%)$")
        local values = line:match("^SELECT (.*)$")
        if insert then
            out[#out + 1] = insert .. "("
            ListLines(columns, out)
            out[#out + 1] = ")"
        elseif values then
            out[#out + 1] = "SELECT"
            ListLines(values, out)
        else
            out[#out + 1] = line
        end
    end
    out[#out] = nil
    return table.concat(out, "\n") .. ((sql or ""):sub(-1) == "\n" and "\n" or "")
end

-- The SQL box's text: the whole SQL when it is one row within limit, else the selected row's statements (cut at the
-- last line that fits in limit) and a comment on what is not shown. Returns the text and whether it is all of it.
function RB.SqlPreview(rowText, sql, rowCount, limit)
    if rowCount <= 1 and #sql <= limit then
        return sql, true
    end
    local text = rowText
    if #text > limit then
        local cut = text:sub(1, limit):match(".*\n()")
        text = cut and text:sub(1, cut - 1) or text:sub(1, limit)
    end
    if text:sub(-1) ~= "\n" then
        text = text .. "\n"
    end
    local kb = math.ceil(math.max(#sql - #text, 0) / 1024)
    local rest = rowCount - 1
    local what = rest > 0 and string.format("%d more %s, %d KB,", rest, rest == 1 and "row" or "rows", kb) or (kb .. " KB more")
    return text .. "\n-- " .. what .. " not shown: Copy and Export use all of it, Edit full SQL shows it.", false
end
