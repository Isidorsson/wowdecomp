-- The shell's layout maths, pure so Tests/ can run it: the rail's width (collapsed below 1200 px), the inspector's
-- width (560 by default, at least 420, leaving the grid MAIN_MIN), whole grid rows of a height, the grid columns that
-- fit a width (by drop rank), the inspector's tabs on one row, chips that fit and a grid of buttons.

local RB = RetailBrowser

local Layout = {
    PAD = 10,                 -- Window edge to the outer columns.
    GAP = 8,                  -- Between columns.
    RAIL_WIDTH = 176,
    RAIL_COLLAPSED_WIDTH = 48,
    RAIL_COLLAPSE_BELOW = 1200,
    INSPECTOR_DEFAULT = 560,
    INSPECTOR_MIN = 420,
    INSPECTOR_STEP = 40,
    MAIN_MIN = 400,
}
RB.Layout = Layout

function Layout.RailCollapsed(windowWidth)
    return windowWidth < Layout.RAIL_COLLAPSE_BELOW
end

function Layout.RailWidth(windowWidth)
    return Layout.RailCollapsed(windowWidth) and Layout.RAIL_COLLAPSED_WIDTH or Layout.RAIL_WIDTH
end

-- The window width the rail, the pads and the gaps take.
function Layout.ColumnsChrome(railWidth)
    return 2 * Layout.PAD + railWidth + 2 * Layout.GAP
end

function Layout.InspectorWidth(wanted, windowWidth, railWidth)
    local most = windowWidth - Layout.ColumnsChrome(railWidth) - Layout.MAIN_MIN
    return math.max(Layout.INSPECTOR_MIN, math.min(most, wanted or Layout.INSPECTOR_DEFAULT))
end

function Layout.StepInspector(width, delta, windowWidth, railWidth)
    return Layout.InspectorWidth(width + delta * Layout.INSPECTOR_STEP, windowWidth, railWidth)
end

function Layout.GridRows(height, chrome, rowHeight, maxRows)
    return math.max(1, math.min(maxRows, math.floor((height - chrome) / rowHeight)))
end

-- columns: { width = n } fixed, { flex = true } takes the rest (at least flexMin), and an optional drop rank: while
-- they do not fit, the column with the highest drop leaves first; columns without one always stay.
-- Returns { [i] = { x, width } } for the kept columns (dropped ones have no entry), from x = 0.
local function DropOrder(columns)
    local order = {}
    for i, column in ipairs(columns) do
        if column.drop then
            order[#order + 1] = i
        end
    end
    table.sort(order, function(a, b) return columns[a].drop > columns[b].drop end)
    return order
end

function Layout.FitColumns(columns, inner, flexMin)
    local kept, fixed = {}, 0
    for i, column in ipairs(columns) do
        kept[i] = true
        fixed = fixed + (column.width or 0)
    end
    for _, i in ipairs(DropOrder(columns)) do
        if fixed + flexMin <= inner then
            break
        end
        kept[i] = false
        fixed = fixed - columns[i].width
    end
    local places, x = {}, 0
    for i, column in ipairs(columns) do
        if kept[i] then
            local width = column.flex and math.max(flexMin, inner - fixed) or column.width
            places[i] = { x = x, width = width }
            x = x + width
        end
    end
    return places
end

-- The inspector's tabs on one row: full labels at the widest padding, else short labels, else tighter padding.
-- tabs: { { full = label width, short = short label width or nil, extra = count bubble width } }.
-- Returns { short, padding, fits } (the tightest setting when nothing fits).
Layout.TAB_PADDINGS = { 16, 10, 6 }

local function TabsWidth(tabs, short, padding, gap)
    local width = 0
    for i, tab in ipairs(tabs) do
        width = width + (short and tab.short or tab.full) + tab.extra + padding + (i > 1 and gap or 0)
    end
    return width
end

function Layout.FitTabs(tabs, width, gap)
    local paddings = Layout.TAB_PADDINGS
    if TabsWidth(tabs, false, paddings[1], gap) <= width then
        return { short = false, padding = paddings[1], fits = true }
    end
    for _, padding in ipairs(paddings) do
        if TabsWidth(tabs, true, padding, gap) <= width then
            return { short = true, padding = padding, fits = true }
        end
    end
    return { short = true, padding = paddings[#paddings], fits = false }
end

-- Clear filters' label: it counts the filters when some of their chips are hidden.
function Layout.ClearFiltersLabel(count, shown)
    if shown >= count then
        return "Clear filters"
    end
    return "Clear " .. count .. (count == 1 and " filter" or " filters")
end

-- How many of the chips (their widths, in order, gap between them) fit in room: the rest stay hidden.
function Layout.FitChips(widths, room, gap)
    local used = 0
    for i, width in ipairs(widths) do
        used = used + (i > 1 and gap or 0) + width
        if used > room then
            return i - 1
        end
    end
    return #widths
end

-- The SQL box's width: the visible width when it wraps, else wide enough for the longest line (it scrolls sideways).
function Layout.SqlBoxWidth(wrap, visible, longest, padding)
    if wrap then
        return visible
    end
    return math.max(visible, longest + padding)
end

-- Buttons of at least minWidth in as many columns as fit: columns, button width, rows.
function Layout.ActionGrid(count, width, minWidth, gap)
    local columns = math.min(count, math.max(1, math.floor((width + gap) / (minWidth + gap))))
    return columns, (width - (columns - 1) * gap) / columns, math.ceil(count / columns)
end
