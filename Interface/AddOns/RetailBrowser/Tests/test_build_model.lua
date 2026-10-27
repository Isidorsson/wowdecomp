local h = require("harness")
local F = require("fixtures")
local RB = RetailBrowser
local T = RB.THEME

-- Two rows the way RetailBuildSql writes them: a comment, DELETE, then INSERT ... SELECT from the base row.
local function CreatureRow(entry, display, name)
    return "-- retail creature 203206 Stormwind Guard, copied from creature 15166\n"
        .. "DELETE FROM `creature_template` WHERE `entry` = " .. entry .. ";\n"
        .. "INSERT INTO `creature_template` (`entry`, `KillCredit1`, `modelid1`, `name`, `subname`, `scale`)\n"
        .. "SELECT " .. entry .. ", `KillCredit1`, " .. display .. ", '" .. name .. "', '', `scale`\n"
        .. "FROM `creature_template` WHERE `entry` = 15166;\n"
end

local ITEM_ROW = "-- retail item 19019 Thunderfury, copied from item 37401\n"
    .. "DELETE FROM `item_template` WHERE `entry` = 9100000;\n"
    .. "INSERT INTO `item_template` (`entry`, `class`, `name`, `displayid`, `BuyCount`)\n"
    .. "SELECT 9100000, 2, 'Thunderfury, Blessed Blade of the Windseeker', 30606, `BuyCount`\n"
    .. "FROM `item_template` WHERE `entry` = 37401;\n"

h.describe("SQL rows", function()
    h.it("splits a build into its rows", function()
        local rows = RB.SqlRows(CreatureRow(9000000, 111840, "A") .. "\n" .. CreatureRow(9000001, 111841, "B"))
        h.assertEq(#rows, 2)
        h.assertEq(rows[2].entry, 9000001)
        h.assertEq(rows[2].table, "creature_template")
        h.assertEq(rows[2].base, 15166)
    end)

    h.it("keeps quoted commas inside one value", function()
        local row = RB.SqlRows(ITEM_ROW)[1]
        h.assertEq(#row.values, #row.columns)
        h.assertEq(row.values[3], "'Thunderfury, Blessed Blade of the Windseeker'")
    end)

    h.it("unquotes doubled quotes in a string value", function()
        h.assertEq(RB.SqlRows(CreatureRow(1, 2, "Hogger''s Pup"))[1].name, "Hogger's Pup")
    end)
end)

h.describe("Changes vs base", function()
    h.it("lists the columns a row overrides, not those it copies", function()
        local changes = RB.ChangesVsBase(RB.SqlRows(CreatureRow(9000000, 111840, "A"))[1])
        h.assertEq(changes, {
            { column = "entry", value = "9000000" }, { column = "modelid1", value = "111840" },
            { column = "name", value = "'A'" }, { column = "subname", value = "''" },
        })
    end)
end)

h.describe("Build queue", function()
    local function Part(d, sql) return { d = d, sql = sql } end

    h.it("rows that could not be built follow the built ones, not ready, with why", function()
        local d, bad = F.Creature(1), F.Creature(1)
        bad.name = "Engine of Mecha-Perfection"
        local reason = "no stock 3.3.5 item of class 0, subclass 8, slot 0 to copy for Engine of Mecha-Perfection: enter a base entry"
        local rows = RB.BuildQueue({ Part(d, CreatureRow(9000000, 111839, "Stormwind Guard")) }, { { d = bad, reason = reason } })
        h.assertEq(#rows, 2)
        h.assertEq(rows[1].name, "Stormwind Guard")
        local skipped = rows[2]
        h.assertEq(skipped.name, "Engine of Mecha-Perfection")
        h.assertTrue(not skipped.ready and skipped.skipped)
        h.assertEq(skipped.entry, nil)
        h.assertEq(skipped.note, "skipped: no stock item to copy, set a base entry")
        h.assertTrue(skipped.row.text:find(reason, 1, true) ~= nil, skipped.row.text)
        h.assertEq(RB.ChangesVsBase(skipped.row), {})
    end)

    h.it("another build error is shown as it is", function()
        local rows = RB.BuildQueue({}, { { d = F.Creature(1), reason = "no display" } })
        h.assertEq(rows[1].note, "skipped: no display")
    end)

    h.it("a row per statement, with its source, display and entry", function()
        local d = F.Creature(1)
        local rows = RB.BuildQueue({ Part(d, CreatureRow(9000000, 111839, "Stormwind Guard")) })
        h.assertEq(#rows, 1)
        h.assertEq(rows[1].name, "Stormwind Guard")
        h.assertEq(rows[1].from, "from 203205 · display 111839")
        h.assertEq(rows[1].entry, 9000000)
        h.assertTrue(rows[1].ready, "served display")
        h.assertEq(rows[1].note, "ready")
    end)

    h.it("a stock 3.3.5 row winning is not ready", function()
        local d = F.Creature(1)
        d.displays[1].custom, d.displays[1].displayId = true, 3167
        local row = RB.BuildQueue({ Part(d, CreatureRow(9000000, 3167, "Stormwind Guard")) })[1]
        h.assertTrue(not row.ready)
        h.assertEq(row.note, "stock 3.3.5 row wins")
    end)

    h.it("a row the server added winning says so", function()
        local d = F.Creature(1)
        d.displays[1].custom, d.displays[1].displayId = true, 900000
        h.assertEq(RB.BuildQueue({ Part(d, CreatureRow(9000000, 900000, "X")) })[1].note, "custom row wins")
    end)

    h.it("a source with several displays is flagged", function()
        local d = F.Creature(2)
        local rows = RB.BuildQueue({ Part(d, CreatureRow(9000000, 111839, "A") .. "\n" .. CreatureRow(9000001, 111840, "B")) })
        h.assertEq(#rows, 2)
        h.assertEq(rows[2].note, "2 displays")
        h.assertTrue(not rows[2].ready)
    end)

    h.it("an item row uses its displayid and appearance", function()
        local row = RB.BuildQueue({ Part(F.Item(), ITEM_ROW) })[1]
        h.assertEq(row.from, "from 19019 · display 30606")
        h.assertEq(row.name, "Thunderfury, Blessed Blade of the Windseeker")
    end)
end)

h.describe("Default build scope", function()
    h.it("builds the checked rows when there are some", function()
        h.assertEq(RB.DefaultBuildScope(50), "checked")
    end)

    h.it("builds the focused row when nothing is checked", function()
        h.assertEq(RB.DefaultBuildScope(0), "focused")
    end)

    h.it("a scope chosen on the page stands until the checks change", function()
        h.assertEq(RB.BuildScopeFor(50, "focused"), "focused")
        h.assertEq(RB.BuildScopeFor(50, nil), "checked")
        h.assertEq(RB.BuildScopeFor(0, "page"), "page")
    end)
end)

h.describe("Build page labels", function()
    h.it("entry range: one entry, or first to last", function()
        h.assertEq(RB.EntryRangeText(9000000, 9000001), "9000000")
        h.assertEq(RB.EntryRangeText(9000000, 9000003), "9000000–9000002")
    end)

    h.it("scope labels count the checked rows and the page", function()
        h.assertEq(RB.BuildScopeLabels(3, 6), { focused = "Focused row", checked = "Checked 3", page = "Whole page 6" })
    end)

    h.it("every display mode has a one-line hint", function()
        for _, mode in ipairs(RB.DISPLAY_MODES) do
            local hint = RB.DisplayModeHint(mode[1])
            h.assertTrue(hint and hint ~= "" and not hint:find("\n"), mode[2])
        end
    end)

    h.it("the variants hint says displays past four are left out", function()
        h.assertTrue(RB.DisplayModeHint(2):find("four") ~= nil and RB.DisplayModeHint(2):find("left out") ~= nil)
    end)

    h.it("item options open only for item and set builds", function()
        h.assertTrue(not RB.ItemOptionsOpen({ F.Creature(1), F.Mount() }))
        h.assertTrue(RB.ItemOptionsOpen({ F.Creature(1), F.Item() }))
        h.assertTrue(RB.ItemOptionsOpen({ F.Set() }))
    end)

    h.it("the apply confirm names the rows, tables and range", function()
        h.assertEq(RB.ApplyConfirmText({ "creature_template" }, 9000000, 9000003),
            "Write 3 creature_template rows (9000000–9000002) to the world DB? Rows with these entries are replaced. Written one row at a time, top to bottom.")
        h.assertEq(RB.ApplyConfirmText({ "item_template" }, 9100000, 9100001),
            "Write 1 item_template row (9100000) to the world DB? Rows with these entries are replaced.")
    end)
end)

-- Whitespace outside quotes and backticks dropped: what formatting may change.
local function Content(sql)
    local out, quote = {}, nil
    for i = 1, #sql do
        local char = sql:sub(i, i)
        if quote then
            out[#out + 1] = char
            if char == quote then quote = nil end
        elseif char == "'" or char == "`" then
            quote = char
            out[#out + 1] = char
        elseif not char:match("%s") then
            out[#out + 1] = char
        end
    end
    return table.concat(out)
end

local function Lines(text)
    local lines = {}
    for line in (text .. "\n"):gmatch("(.-)\n") do lines[#lines + 1] = line end
    return lines
end

h.describe("SQL formatting", function()
    local raw = CreatureRow(9000000, 111840, "Hogger''s, Pup") .. "\n" .. ITEM_ROW

    h.it("changes only whitespace outside quotes and backticks", function()
        h.assertEq(Content(RB.FormatSql(raw)), Content(raw))
    end)

    h.it("keeps a quoted name with a comma and a quote whole on one line", function()
        local found = false
        for _, line in ipairs(Lines(RB.FormatSql(raw))) do
            if line:find("'Hogger''s, Pup'", 1, true) then found = true end
        end
        h.assertTrue(found)
    end)

    h.it("breaks the column and value lists, SELECT on its own line", function()
        local lines = Lines(RB.FormatSql(CreatureRow(9000000, 111840, "A")))
        h.assertEq(lines[3], "INSERT INTO `creature_template` (")
        h.assertEq(lines[4], "  `entry`, `KillCredit1`, `modelid1`, `name`, `subname`, `scale`")
        h.assertEq(lines[5], ")")
        h.assertEq(lines[6], "SELECT")
        h.assertEq(lines[7], "  9000000, `KillCredit1`, 111840, 'A', '', `scale`")
        h.assertEq(lines[8], "FROM `creature_template` WHERE `entry` = 15166;")
    end)

    h.it("leaves comments and DELETE lines as they are", function()
        local lines = Lines(RB.FormatSql(ITEM_ROW))
        h.assertEq(lines[1], "-- retail item 19019 Thunderfury, copied from item 37401")
        h.assertEq(lines[2], "DELETE FROM `item_template` WHERE `entry` = 9100000;")
    end)

    h.it("puts a few items on each list line", function()
        local columns, values = {}, {}
        for i = 1, 20 do columns[i] = "`c" .. i .. "`" values[i] = tostring(i) end
        local sql = "INSERT INTO `item_template` (" .. table.concat(columns, ", ") .. ")\nSELECT " .. table.concat(values, ", ")
            .. "\nFROM `item_template` WHERE `entry` = 1;\n"
        local lines = Lines(RB.FormatSql(sql))
        h.assertEq(lines[2], "  `c1`, `c2`, `c3`, `c4`, `c5`, `c6`,")
        h.assertEq(#lines, 1 + 4 + 1 + 1 + 4 + 1 + 1)
    end)

    h.it("is unchanged when formatted twice", function()
        h.assertEq(RB.FormatSql(RB.FormatSql(raw)), RB.FormatSql(raw))
    end)
end)

h.describe("SQL preview", function()
    h.it("a short one-row build shows all of it", function()
        h.assertEq({ RB.SqlPreview("abc\n", "abc\n", 1, 4096) }, { "abc\n", true })
    end)

    h.it("cuts a long row at a line boundary within the limit", function()
        local text = RB.SqlPreview("aaaa\nbbbb\ncccc\n", "aaaa\nbbbb\ncccc\n", 1, 12)
        h.assertEq(text:sub(1, 10), "aaaa\nbbbb\n")
        h.assertTrue(text:sub(11):match("^\n%-%- ") ~= nil, text)
    end)

    h.it("says how many rows are not shown", function()
        local text, whole = RB.SqlPreview("row1\n", "row1\n\nrow2\n\nrow3\n", 3, 4096)
        h.assertTrue(not whole)
        h.assertTrue(text:find("2 more rows", 1, true) ~= nil, text)
    end)
end)

h.describe("Apply rows", function()
    local function Rows(first, count)
        local blocks = {}
        for i = 0, count - 1 do
            blocks[#blocks + 1] = CreatureRow(first + i, 111839, "Guard " .. i)
        end
        return table.concat(blocks, "\n")
    end

    h.it("one apply row per entry, top to bottom", function()
        local rows = RB.ApplyRows(Rows(9000000, 3))
        h.assertEq(#rows, 3)
        h.assertEq({ rows[1].entry, rows[3].entry }, { 9000000, 9000002 })
    end)

    h.it("a row's SQL holds its own statements and no others", function()
        local row = RB.ApplyRows(Rows(9000000, 3))[2]
        h.assertTrue(row.sql:find("`entry` = 9000001;", 1, true) ~= nil)
        h.assertTrue(row.sql:find("`entry` = 9000000;", 1, true) == nil)
        h.assertTrue(row.sql:find("`entry` = 9000002;", 1, true) == nil)
    end)

    h.it("splits the readable (formatted) SQL the same way", function()
        local rows = RB.ApplyRows(RB.FormatSql(Rows(9000000, 3)))
        h.assertEq(#rows, 3)
        h.assertEq(rows[2].entry, 9000001)
    end)

    h.it("a block with no entry stays with the row before it, so nothing is dropped", function()
        local rows = RB.ApplyRows(Rows(9000000, 2) .. "\n-- a note the user typed\n")
        h.assertEq(#rows, 2)
        h.assertTrue(rows[2].sql:find("a note the user typed", 1, true) ~= nil)
    end)

    h.it("no SQL, no rows", function()
        h.assertEq(#RB.ApplyRows(""), 0)
    end)
end)

h.describe("Apply job", function()
    local function Job()
        return RB.ApplyJob({ { entry = 1, sql = "a" }, { entry = 2, sql = "b" }, { entry = 3, sql = "c" } })
    end

    h.it("every row waits until it is sent", function()
        local job = Job()
        h.assertEq(job.state[1], "waiting")
        h.assertEq(job.state[3], "waiting")
    end)

    h.it("sends one row at a time, top to bottom", function()
        local job = Job()
        h.assertEq(RB.ApplyJobNext(job).sql, "a")
        h.assertEq(job.state[1], "writing")
        h.assertEq(job.state[2], "waiting")
        RB.ApplyJobFinished(job, true)
        h.assertEq(job.state[1], "written")
        h.assertEq(RB.ApplyJobNext(job).sql, "b")
    end)

    h.it("is done after the last row is written", function()
        local job = Job()
        for _ = 1, 3 do
            RB.ApplyJobNext(job)
            RB.ApplyJobFinished(job, true)
        end
        h.assertTrue(job.done)
        h.assertEq(RB.ApplyJobNext(job), nil)
    end)

    h.it("stops at a failed row: later ones still waiting, nothing more sent", function()
        local job = Job()
        RB.ApplyJobNext(job)
        RB.ApplyJobFinished(job, true)
        RB.ApplyJobNext(job)
        RB.ApplyJobFinished(job, false, "0/1 written")
        h.assertEq(job.state[2], "failed")
        h.assertEq(job.state[3], "waiting")
        h.assertEq(job.failed, "0/1 written")
        h.assertEq(RB.ApplyJobNext(job), nil)
        h.assertEq(RB.ApplyJobRemaining(job), 2)
    end)

    h.it("retry resumes at the failed row and never resends written ones", function()
        local job = Job()
        RB.ApplyJobNext(job)
        RB.ApplyJobFinished(job, true)
        RB.ApplyJobNext(job)
        RB.ApplyJobFinished(job, false, "timeout")
        RB.ApplyJobRetry(job)
        h.assertEq(job.failed, nil)
        h.assertEq(job.state[1], "written")
        h.assertEq(job.state[2], "waiting")
        h.assertEq(RB.ApplyJobNext(job).sql, "b")
    end)

    h.it("counts written rows", function()
        local job = Job()
        RB.ApplyJobNext(job)
        RB.ApplyJobFinished(job, true)
        h.assertEq(RB.ApplyJobWritten(job), 1)
    end)
end)

h.describe("Queue title", function()
    local function Job(results)
        local job = RB.ApplyJob({ { entry = 1, sql = "a" }, { entry = 2, sql = "b" }, { entry = 3, sql = "c" } })
        for _, ok in ipairs(results) do
            RB.ApplyJobNext(job)
            RB.ApplyJobFinished(job, ok, "refused")
        end
        return job
    end

    h.it("counts the queue with no apply", function()
        h.assertEq(RB.ApplyQueueTitle(93, nil), "Queue · 93")
    end)

    h.it("names the row being written", function()
        local job = Job({ true })
        RB.ApplyJobNext(job)
        h.assertEq(RB.ApplyQueueTitle(3, job), "Writing 2 / 3")
    end)

    h.it("says where a failed apply stopped", function()
        h.assertEq(RB.ApplyQueueTitle(3, Job({ true, false })), "Stopped at 2 / 3")
    end)

    h.it("says when every row is written", function()
        h.assertEq(RB.ApplyQueueTitle(3, Job({ true, true, true })), "Written 3 / 3")
    end)
end)

h.describe("Clear written", function()
    h.it("drops written rows and keeps waiting, failed and skipped ones in order", function()
        local job = RB.ApplyJob({ { entry = 1, sql = "a" }, { entry = 2, sql = "b" }, { entry = 3, sql = "c" } })
        RB.ApplyJobNext(job)
        RB.ApplyJobFinished(job, true)
        RB.ApplyJobNext(job)
        RB.ApplyJobFinished(job, false, "refused")
        local queue = { { entry = 1 }, { entry = 2 }, { skipped = true }, { entry = 3 } }
        local kept = RB.WithoutWritten(queue, job)
        h.assertEq(#kept, 3)
        h.assertEq({ kept[1].entry, kept[2].skipped, kept[3].entry }, { 2, true, 3 })
    end)

    h.it("keeps everything with no apply", function()
        h.assertEq(#RB.WithoutWritten({ { entry = 1 } }, nil), 1)
    end)
end)

h.describe("Apply texts", function()
    h.it("the confirm says several rows go one at a time", function()
        local text = RB.ApplyConfirmText({ "item_template" }, 100000, 100149)
        h.assertTrue(text:find("149 item_template rows", 1, true) ~= nil, text)
        h.assertTrue(text:find("one row at a time", 1, true) ~= nil, text)
    end)

    h.it("one row is not said to go one at a time", function()
        h.assertTrue(RB.ApplyConfirmText({ "item_template" }, 100000, 100001):find("at a time", 1, true) == nil)
    end)

    h.it("progress names the row and its entry", function()
        h.assertEq(RB.ApplyProgressText(37, 93, 100094), "Writing row 37 / 93 · entry 100094...")
    end)

    h.it("build progress counts rows", function()
        h.assertEq(RB.BuildProgressText(60, 149), "Building 60 / 149...")
    end)
end)

h.describe("Tables a build writes", function()
    h.it("lists each table once, in queue order, and skips rows that were not built", function()
        local d, bad = F.Creature(1), F.Creature(1)
        local queue = RB.BuildQueue({ { d = d, sql = CreatureRow(9000000, 111839, "A") .. "\n" .. ITEM_ROW } },
            { { d = bad, reason = "no display" } })
        h.assertEq(RB.BuildTables(queue), { "creature_template", "item_template" })
    end)
end)
