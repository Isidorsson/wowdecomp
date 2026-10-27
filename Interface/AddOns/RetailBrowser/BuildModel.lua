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

-- parts: { { d = details, sql = that entry's SQL } } in build order. Returns the queue: a row per statement with its
-- name, "from <id> · display <id>", a note and whether it is ready (retail's row is served, one display).
function RB.BuildQueue(parts)
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
    return queue
end

-- "9000000" for one entry, "9000000–9000002" for a run (nextEntry is the first entry after it).
function RB.EntryRangeText(first, nextEntry)
    if nextEntry - first <= 1 then
        return tostring(first)
    end
    return first .. "–" .. (nextEntry - 1)
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

-- The confirm bar's question before Apply: tables, how many rows, the entry range.
function RB.ApplyConfirmText(tables, first, nextEntry)
    local count = nextEntry - first
    return string.format("Write %d %s %s (%s) to the world DB? Rows with these entries are replaced.", count,
        table.concat(tables, " and "), count == 1 and "row" or "rows", RB.EntryRangeText(first, nextEntry))
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
