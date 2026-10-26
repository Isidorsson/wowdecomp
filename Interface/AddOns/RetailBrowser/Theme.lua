-- Every colour the Retail Browser draws, after the retail WoW UI: near-black panels with a cool tint, bronze metal
-- borders (the stock dialog and tooltip edges, tinted), gold titles, white body text, grey labels, pale gold ids and
-- numbers, the item quality colours. Frames read colours only from here: { r, g, b [, a] } in 0..1.

local RB = RetailBrowser

local GOLD = { 1, 0.82, 0 }
local PALE_GOLD = { 0.90, 0.80, 0.50 }

RB.THEME = {
    panel = { 0.05, 0.05, 0.06, 0.92 },
    well = { 0.02, 0.02, 0.03, 0.95 },
    band = { 0.10, 0.09, 0.07, 0.9 },
    border = { 0.55, 0.45, 0.25, 1 },
    borderDim = { 0.30, 0.25, 0.15, 1 },

    title = GOLD,
    text = { 1, 1, 1 },
    label = { 0.75, 0.75, 0.75 },
    disabled = { 0.5, 0.5, 0.5 },
    value = PALE_GOLD,
    good = { 0, 1, 0 },
    bad = { 1, 0.13, 0.13 },
    picked = { 0.25, 1, 0.25 },     -- The display the preview and the actions use.
    combat = { 0.50, 0.75, 1 },     -- The base item's armour and damage beside the retail ones (Build).

    quality = {
        [0] = { 0.62, 0.62, 0.62 }, [1] = { 1, 1, 1 }, [2] = { 0.12, 1, 0 }, [3] = { 0, 0.44, 0.87 },
        [4] = { 0.64, 0.21, 0.93 }, [5] = { 1, 0.50, 0 }, [6] = PALE_GOLD, [7] = { 0, 0.80, 1 },
    },
    -- By rank group: 0 normal, 1 elite, 2 rare, 3 rare elite.
    rank = { [0] = { 1, 1, 1 }, [1] = GOLD, [2] = { 0.78, 0.78, 0.85 }, [3] = { 0.60, 0.80, 1 } },
    badge = { served = { 0, 1, 0 }, custom = { 1, 0.50, 0 }, ["3.3.5"] = { 0.75, 0.75, 0.75 } },

    stripe = { 1, 1, 1, 0.03 },
    hover = { 1, 0.82, 0, 0.12 },
    selected = { 1, 0.82, 0, 0.25 },
    selectedEdge = { 1, 0.82, 0, 1 },
    checked = { 0.90, 0.80, 0.50, 0.10 },

    input = { 0, 0, 0, 0.6 },
    inputBorder = { 0.30, 0.25, 0.15, 1 },
    inputFocus = { 1, 0.82, 0, 1 },
    inputActive = PALE_GOLD,

    warning = { 0.40, 0.03, 0.03, 0.9 },
    preview = { 0, 0, 0, 0.6 },
}

-- "|cffrrggbb" of a theme colour, for text that mixes colours.
function RB.Hex(color)
    return string.format("|cff%02x%02x%02x", math.floor(color[1] * 255 + 0.5), math.floor(color[2] * 255 + 0.5), math.floor(color[3] * 255 + 0.5))
end

function RB.Colored(text, color)
    return RB.Hex(color) .. tostring(text) .. "|r"
end

-- A key hint after a label ("Copy ids Ctrl+C").
function RB.Key(text)
    return RB.Colored(text, RB.THEME.label)
end

function RB.SetColor(region, color)
    region:SetTextColor(color[1], color[2], color[3], color[4] or 1)
end

function RB.SetVertex(texture, color)
    texture:SetVertexColor(color[1], color[2], color[3], color[4] or 1)
end

function RB.SetBorder(frame, color)
    frame:SetBackdropBorderColor(color[1], color[2], color[3], color[4] or 1)
end

function RB.SetBack(frame, color)
    frame:SetBackdropColor(color[1], color[2], color[3], color[4] or 1)
end
