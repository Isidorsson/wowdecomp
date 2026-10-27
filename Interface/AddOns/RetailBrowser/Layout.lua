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

-- How the player wearing an item is framed: x is the distance (more negative is further back), z the height (more
-- negative frames higher on the body), facing the turn (positive brings the right hand toward you). Measured in game
-- in the player model's fixed box (Layout.ModelBox, PLAYER_ASPECT): the whole body at 0 fills about four fifths of its height; the
-- player model has a camera about 2.5 in front, so distance zooms.
Layout.WHOLE_VIEW = { x = 0, z = -0.05, facing = 0 }
local NEAR = 1
local HEAD_VIEW = { x = NEAR, z = -0.5, facing = 0 }
local CHEST_VIEW = { x = NEAR, z = -0.2, facing = 0 }
local WAIST_VIEW = { x = NEAR, z = 0, facing = 0 }
local RIGHT_HAND_VIEW = { x = NEAR, z = 0, facing = 0.9 }
local LEFT_HAND_VIEW = { x = NEAR, z = 0, facing = -0.9 }
local TWO_HAND_VIEW = { x = Layout.WHOLE_VIEW.x, z = Layout.WHOLE_VIEW.z, facing = 0.6 }

-- By InventoryType. Rings, trinkets, bags, ammo, quivers and relics draw nothing on the body: the whole view.
local ITEM_VIEWS = {
    [1] = HEAD_VIEW, [2] = HEAD_VIEW, [3] = HEAD_VIEW,
    [4] = CHEST_VIEW, [5] = CHEST_VIEW, [19] = CHEST_VIEW, [20] = CHEST_VIEW,
    [6] = WAIST_VIEW, [9] = WAIST_VIEW, [10] = WAIST_VIEW,
    [7] = { x = NEAR, z = 0.25, facing = 0 },
    [8] = { x = NEAR, z = 0.55, facing = 0 },
    [16] = { x = 0.5, z = -0.15, facing = math.pi },
    [13] = RIGHT_HAND_VIEW, [21] = RIGHT_HAND_VIEW, [25] = RIGHT_HAND_VIEW, [26] = RIGHT_HAND_VIEW,
    [14] = LEFT_HAND_VIEW, [22] = LEFT_HAND_VIEW, [23] = LEFT_HAND_VIEW,
    [17] = TWO_HAND_VIEW,
    [15] = { x = Layout.WHOLE_VIEW.x, z = Layout.WHOLE_VIEW.z, facing = -0.6 },
}

function Layout.ItemView(inventoryType)
    return ITEM_VIEWS[inventoryType or 0] or Layout.WHOLE_VIEW
end

-- A creature or mount display has no camera, so distance does nothing and its size is set by scale instead. A
-- humanoid fits whole, with head room, at 0.58 in the default box (262 px of model height; the model stands on the
-- box's bottom, so a bigger scale loses the head first); the scale follows the box's height, and big creature types
-- (their fit value) and big display scales shrink it further.
local DISPLAY_FULL_HEIGHT = 262 / 0.58
local DISPLAY_TYPE_SHRINK = 0.2

function Layout.DisplayScale(boxHeight, typeFit, displayScale)
    if not boxHeight or boxHeight <= 0 then
        return 1
    end
    local scale = math.min(1, boxHeight / DISPLAY_FULL_HEIGHT)
    return scale / (1 + (typeFit or 0) * DISPLAY_TYPE_SHRINK) / math.max(1, displayScale or 1)
end

-- The SQL box's width: the visible width when it wraps, else wide enough for the longest line (it scrolls sideways).
function Layout.SqlBoxWidth(wrap, visible, longest, padding)
    if wrap then
        return visible
    end
    return math.max(visible, longest + padding)
end

-- What a click on a grid row does: "range" (Shift, checkbox included), "toggle" (the checkbox, Ctrl or right-click)
-- or "focus".
function Layout.RowClickAction(mouse, onBox, shift, ctrl)
    if shift then
        return "range"
    elseif mouse == "RightButton" or onBox or ctrl then
        return "toggle"
    end
    return "focus"
end

-- The grid row under the cursor while dragging over the checkboxes, clamped to the visible rows: gridTop and cursorY
-- in the same units, rowsTop the rows' distance below the grid's top, offset the scrolled-past rows.
function Layout.RowUnderCursor(gridTop, cursorY, rowsTop, rowHeight, offset, visible)
    local row = math.floor((gridTop - rowsTop - cursorY) / rowHeight) + 1
    return offset + math.max(1, math.min(visible, row))
end

-- Buttons of at least minWidth in as many columns as fit: columns, button width, rows.
function Layout.ActionGrid(count, width, minWidth, gap)
    local columns = math.min(count, math.max(1, math.floor((width + gap) / (minWidth + gap))))
    return columns, (width - (columns - 1) * gap) / columns, math.ceil(count / columns)
end

-- The row-hover preview: a first hover waits, so sweeping the list does not flash previews; once one has shown (or
-- hid within HOVER_WARM_WINDOW, as moving between rows passes through a leave) the next one swaps almost at once.
Layout.HOVER_DELAY = 0.2
Layout.HOVER_WARM_DELAY = 0.05
local HOVER_WARM_WINDOW = 0.5

function Layout.HoverDelay(now, shown, hiddenAt)
    local warm = shown or (hiddenAt ~= nil and now - hiddenAt < HOVER_WARM_WINDOW)
    return warm and Layout.HOVER_WARM_DELAY or Layout.HOVER_DELAY
end

-- A preview model's box: a fixed shape (width over height), as tall as the preview allows. A model with a camera (the
-- player, humanoid displays) takes its field of view as a diagonal (wowdecomp Gxu.cpp GxuXformCreateProjection_SG), so
-- the vertical view narrows as the box widens; one shape keeps each view framing the body the same in any box. The
-- player's box is a portrait; a display's is square, for mounts and long creatures.
Layout.PLAYER_ASPECT = 0.8
Layout.DISPLAY_ASPECT = 1

function Layout.ModelBox(width, height, aspect)
    if width >= height * aspect then
        return height * aspect, height
    end
    return width, width / aspect
end
