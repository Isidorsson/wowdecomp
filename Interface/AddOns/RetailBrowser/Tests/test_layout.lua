local h = require("harness")
local Layout = RetailBrowser.Layout

h.describe("Rail", function()
    h.it("collapses below 1200 px", function()
        h.assertTrue(Layout.RailCollapsed(1199))
        h.assertTrue(not Layout.RailCollapsed(1200))
    end)

    h.it("is 48 px collapsed, wider expanded", function()
        h.assertEq(Layout.RailWidth(1100), 48)
        h.assertEq(Layout.RailWidth(1280), Layout.RAIL_WIDTH)
        h.assertTrue(Layout.RAIL_WIDTH > 48)
    end)
end)

h.describe("Inspector width", function()
    h.it("defaults to 560", function()
        h.assertEq(Layout.InspectorWidth(nil, 1280, Layout.RailWidth(1280)), 560)
    end)

    h.it("never goes below 420", function()
        h.assertEq(Layout.InspectorWidth(100, 1280, Layout.RAIL_WIDTH), 420)
    end)

    h.it("leaves the grid its minimum", function()
        local width = Layout.InspectorWidth(5000, 1280, Layout.RAIL_WIDTH)
        h.assertEq(1280 - Layout.ColumnsChrome(Layout.RAIL_WIDTH) - width, Layout.MAIN_MIN)
    end)

    h.it("keeps 420 when even that crowds the grid", function()
        h.assertEq(Layout.InspectorWidth(560, 700, 48), 420)
    end)

    h.it("fits 560 at the 1100 minimum window", function()
        h.assertEq(Layout.InspectorWidth(560, 1100, Layout.RailWidth(1100)), 560)
    end)

    h.it("steps by a fixed amount and clamps", function()
        h.assertEq(Layout.StepInspector(560, 1, 1280, Layout.RAIL_WIDTH), 560 + Layout.INSPECTOR_STEP)
        h.assertEq(Layout.StepInspector(440, -1, 1280, Layout.RAIL_WIDTH), 420)
    end)

end)

h.describe("Grid rows", function()
    h.it("takes whole rows of the height", function()
        h.assertEq(Layout.GridRows(200, 40, 16, 40), 10)
        h.assertEq(Layout.GridRows(215, 40, 16, 40), 10)
    end)

    h.it("clamps to 1 and the pool size", function()
        h.assertEq(Layout.GridRows(10, 40, 16, 40), 1)
        h.assertEq(Layout.GridRows(5000, 40, 16, 40), 40)
    end)
end)

h.describe("Columns that fit", function()
    -- check 16, name (flex), id 58, type 74 (drop 2), rank 62 (drop 1); higher drop leaves first.
    local columns = { { width = 16 }, { flex = true }, { width = 58 }, { width = 74, drop = 2 }, { width = 62, drop = 1 } }

    local function Count(places)
        local n = 0
        for _ in pairs(places) do n = n + 1 end
        return n
    end

    h.it("gives the flex column the rest when all fit", function()
        local places = Layout.FitColumns(columns, 500, 120)
        h.assertEq(Count(places), 5)
        h.assertEq(places[2], { x = 16, width = 500 - 210 })
        h.assertEq(places[5], { x = 500 - 62, width = 62 })
    end)

    h.it("drops the highest drop rank first, wherever it sits", function()
        local places = Layout.FitColumns(columns, 300, 120)
        h.assertEq(places[4], nil)
        h.assertEq(places[5], { x = 300 - 62, width = 62 })
        h.assertEq(places[2].width, 300 - 16 - 58 - 62)
    end)

    h.it("drops in rank order until it fits", function()
        local places = Layout.FitColumns(columns, 250, 120)
        h.assertEq(Count(places), 3)
        h.assertEq(places[2].width, 250 - 16 - 58)
    end)

    h.it("never drops a column without a rank", function()
        h.assertEq(Count(Layout.FitColumns(columns, 10, 120)), 3)
    end)

    h.it("keeps the flex column at its minimum", function()
        h.assertEq(Layout.FitColumns(columns, 10, 120)[2].width, 120)
    end)
end)

h.describe("Chips that fit", function()
    h.it("shows every chip that fits, in order", function()
        h.assertEq(Layout.FitChips({ 50, 60, 70 }, 200, 4), 3)
    end)

    h.it("stops at the first chip past the room", function()
        h.assertEq(Layout.FitChips({ 50, 60, 70 }, 120, 4), 2)
        h.assertEq(Layout.FitChips({ 50, 60, 70 }, 40, 4), 0)
    end)

    h.it("counts the gap after each chip", function()
        h.assertEq(Layout.FitChips({ 50, 50 }, 104, 4), 2)
        h.assertEq(Layout.FitChips({ 50, 50 }, 103, 4), 1)
    end)
end)

h.describe("Tabs on one row", function()
    -- About 6 px a character in the tab font, an 18 px count bubble.
    local function Tab(full, short, count)
        return { full = #full * 6, short = short and #short * 6 or nil, extra = count and 18 or 0 }
    end
    local OVERVIEW, RAW, USED, BUILD = Tab("Overview"), Tab("Raw", nil, true), Tab("Used by"), Tab("Build SQL", "SQL")
    local SETS = {
        creature = { OVERVIEW, Tab("Displays", "Disp.", true), USED, BUILD, RAW },
        item = { OVERVIEW, Tab("Stats", nil, true), Tab("Appearances", "Looks", true), USED, BUILD, RAW },
        set = { OVERVIEW, Tab("Pieces", nil, true), BUILD, RAW },
        model = { OVERVIEW, Tab("Displays", "Disp.", true), BUILD, RAW },
        mount = { OVERVIEW, Tab("Displays", "Disp.", true), USED, BUILD, RAW },
    }
    local STRIP_AT_MIN = Layout.INSPECTOR_MIN - 20

    h.it("keeps full labels and padding when they fit", function()
        h.assertEq(Layout.FitTabs(SETS.set, 560, 2), { short = false, padding = 16, fits = true })
    end)

    h.it("every kind's tabs fit one row at the 420 px minimum", function()
        for kind, tabs in pairs(SETS) do
            h.assertTrue(Layout.FitTabs(tabs, STRIP_AT_MIN, 2).fits, kind)
        end
    end)

    h.it("shortens labels before tightening the padding", function()
        local fit = Layout.FitTabs(SETS.item, STRIP_AT_MIN, 2)
        h.assertEq(fit, { short = true, padding = 16, fits = true })
    end)

    h.it("tightens the padding when short labels are not enough", function()
        local fit = Layout.FitTabs(SETS.item, 340, 2)
        h.assertTrue(fit.short and fit.padding < 16 and fit.fits, tostring(fit.padding))
    end)

    h.it("says when nothing fits, at the tightest setting", function()
        h.assertEq(Layout.FitTabs(SETS.item, 100, 2), { short = true, padding = Layout.TAB_PADDINGS[#Layout.TAB_PADDINGS], fits = false })
    end)
end)

h.describe("SQL box width", function()
    h.it("wrap on: the visible width", function()
        h.assertEq(Layout.SqlBoxWidth(true, 400, 900, 24), 400)
    end)

    h.it("wrap off: the longest line plus padding", function()
        h.assertEq(Layout.SqlBoxWidth(false, 400, 900, 24), 924)
    end)

    h.it("wrap off never narrower than the visible width", function()
        h.assertEq(Layout.SqlBoxWidth(false, 400, 100, 24), 400)
    end)
end)

h.describe("Item preview framing", function()
    local HEAD, CHEST, LEGS, FEET, ONE_HAND, SHIELD, TWO_HAND, MAIN_HAND, OFF_HAND, HOLDABLE, FINGER, BACK =
        1, 5, 7, 8, 13, 14, 17, 21, 22, 23, 11, 16

    h.it("a slot's view is closer than the whole body", function()
        for _, slot in ipairs({ HEAD, CHEST, LEGS, FEET, MAIN_HAND, SHIELD }) do
            h.assertTrue(Layout.ItemView(slot).x > Layout.WHOLE_VIEW.x, "slot " .. slot)
        end
    end)

    h.it("the head is framed above the chest, the chest above the legs, the legs above the feet", function()
        h.assertTrue(Layout.ItemView(HEAD).z < Layout.ItemView(CHEST).z)
        h.assertTrue(Layout.ItemView(CHEST).z < Layout.ItemView(LEGS).z)
        h.assertTrue(Layout.ItemView(LEGS).z < Layout.ItemView(FEET).z)
    end)

    h.it("main-hand and one-hand weapons turn the right side toward you", function()
        h.assertTrue(Layout.ItemView(MAIN_HAND).facing > 0)
        h.assertEq(Layout.ItemView(ONE_HAND).facing, Layout.ItemView(MAIN_HAND).facing)
    end)

    h.it("off-hands, shields and held-in-off-hand items turn the left side toward you", function()
        for _, slot in ipairs({ OFF_HAND, SHIELD, HOLDABLE }) do
            h.assertTrue(Layout.ItemView(slot).facing < 0, "slot " .. slot)
        end
    end)

    h.it("a cloak is seen from behind", function()
        h.assertTrue(math.abs(Layout.ItemView(BACK).facing) > 3)
    end)

    h.it("two-handers show the whole body", function()
        h.assertEq(Layout.ItemView(TWO_HAND).x, Layout.WHOLE_VIEW.x)
    end)

    h.it("slots with no model, and unknown slots, show the whole body", function()
        h.assertEq(Layout.ItemView(FINGER), Layout.WHOLE_VIEW)
        h.assertEq(Layout.ItemView(nil), Layout.WHOLE_VIEW)
        h.assertEq(Layout.ItemView(99), Layout.WHOLE_VIEW)
    end)
end)

h.describe("Display preview scale", function()
    h.it("the default inspector box shows a humanoid at the measured scale", function()
        local scale = Layout.DisplayScale(262, 0, 1)
        h.assertTrue(scale > 0.55 and scale < 0.62, tostring(scale))
    end)

    h.it("a shorter box shrinks the model, a taller one never grows it past 1", function()
        h.assertTrue(Layout.DisplayScale(200, 0, 1) < Layout.DisplayScale(262, 0, 1))
        h.assertEq(Layout.DisplayScale(2000, 0, 1), 1)
    end)

    h.it("big creature types and big display scales shrink it further", function()
        h.assertTrue(Layout.DisplayScale(262, 4.5, 1) < Layout.DisplayScale(262, 0, 1))
        h.assertTrue(Layout.DisplayScale(262, 0, 2) < Layout.DisplayScale(262, 0, 1))
    end)

    h.it("a box with no height yet keeps the model's own size", function()
        h.assertEq(Layout.DisplayScale(0, 0, 1), 1)
    end)
end)

h.describe("Pager", function()
    h.it("one match is singular", function()
        h.assertEq(RetailBrowser.PageText(1, 1, 1), "1 match, page 1 / 1")
    end)

    h.it("more matches are plural", function()
        h.assertEq(RetailBrowser.PageText(360, 2, 8), "360 matches, page 2 / 8")
    end)
end)

h.describe("Clear filters label", function()
    h.it("is plain while every chip shows", function()
        h.assertEq(Layout.ClearFiltersLabel(2, 2), "Clear filters")
    end)

    h.it("counts hidden filters, singular and plural", function()
        h.assertEq(Layout.ClearFiltersLabel(1, 0), "Clear 1 filter")
        h.assertEq(Layout.ClearFiltersLabel(3, 1), "Clear 3 filters")
    end)
end)

h.describe("Action grid", function()
    h.it("fits as many columns as the minimum width allows", function()
        local cols, width, rows = Layout.ActionGrid(10, 520, 160, 6)
        h.assertEq(cols, 3)
        h.assertEq(rows, 4)
        h.assertEq(width, (520 - 2 * 6) / 3)
    end)

    h.it("has at least one column", function()
        local cols, width = Layout.ActionGrid(10, 100, 160, 6)
        h.assertEq(cols, 1)
        h.assertEq(width, 100)
    end)
end)

h.describe("Row click", function()
    h.it("Shift+click checks a range, even on the checkbox", function()
        h.assertEq(Layout.RowClickAction("LeftButton", true, true, false), "range")
        h.assertEq(Layout.RowClickAction("LeftButton", false, true, false), "range")
    end)

    h.it("the checkbox, Ctrl+click and right-click toggle one row", function()
        h.assertEq(Layout.RowClickAction("LeftButton", true, false, false), "toggle")
        h.assertEq(Layout.RowClickAction("LeftButton", false, false, true), "toggle")
        h.assertEq(Layout.RowClickAction("RightButton", false, false, false), "toggle")
    end)

    h.it("a plain click elsewhere on the row focuses it", function()
        h.assertEq(Layout.RowClickAction("LeftButton", false, false, false), "focus")
    end)
end)

h.describe("Row under the cursor", function()
    -- Rows start 40 px below the grid's top at y = 500, 16 px each; the list is scrolled 10 rows; 20 rows show.
    local function At(y) return Layout.RowUnderCursor(500, y, 40, 16, 10, 20) end

    h.it("the first visible row is the scroll offset plus one", function()
        h.assertEq(At(459), 11)
    end)

    h.it("each row height down is the next row", function()
        h.assertEq(At(459 - 16), 12)
        h.assertEq(At(460 - 40), 13)
    end)

    h.it("above the rows or below the last visible one is clamped to the edge rows", function()
        h.assertEq(At(480), 11)
        h.assertEq(At(0), 30)
    end)
end)
