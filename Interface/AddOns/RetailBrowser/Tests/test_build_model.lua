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
            "Write 3 creature_template rows (9000000–9000002) to the world DB? Rows with these entries are replaced.")
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
