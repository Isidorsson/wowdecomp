-- Every colour the Retail Browser draws, in the "Ledger" look: flat blue-slate panels a step apart (rail and bands
-- darkest, wells just below the panel), thin cool-grey hairline borders, one teal accent for titles, section heads,
-- focus and the selection (a faint teal wash with a solid teal edge), off-white body text, muted grey labels, pale
-- steel ids and numbers, the item quality colours. Buttons are flat: a dark fill with a hairline border that turns
-- teal on hover, a darker fill pressed, faded when disabled; the primary action is solid teal with dark text.
-- Frames read colours only from here: { r, g, b [, a] } in 0..1.

local RB = RetailBrowser

-- { r, g, b, a } in 0..1 from "rrggbb", so the palette reads as the mockups write it.
local function FromHex(hex, alpha)
    return {
        tonumber(hex:sub(1, 2), 16) / 255, tonumber(hex:sub(3, 4), 16) / 255, tonumber(hex:sub(5, 6), 16) / 255,
        alpha or 1,
    }
end

local ACCENT = FromHex("4fd1c5")
local GOLD = { 1, 0.82, 0 }
local PALE_GOLD = { 0.90, 0.80, 0.50 }
local DISABLED_ALPHA = 0.4

RB.THEME = {
    panel = FromHex("12161c", 0.97),
    well = FromHex("0f1318"),
    band = FromHex("0b0e12"),
    rail = FromHex("0b0e12"),
    border = FromHex("2a313b"),
    borderDim = FromHex("1e242c"),

    accent = ACCENT,
    accentFill = FromHex("16211f"), -- A solid teal-tinted fill: the active rail entry, chips.
    title = ACCENT,
    section = ACCENT,               -- Inspector section heads.
    text = FromHex("e6e9ee"),
    label = FromHex("7d8696"),
    disabled = FromHex("4e5663"),
    value = FromHex("b4bcc8"),
    good = { 0, 1, 0 },
    bad = { 1, 0.13, 0.13 },
    picked = { 0.25, 1, 0.25 },     -- The display the preview and the actions use.
    combat = { 0.50, 0.75, 1 },     -- The base item's armour and damage beside the retail ones (Build).
    itemLevel = GOLD,               -- "Item Level" in the Overview's item tooltip, as the game draws it.

    quality = {
        [0] = { 0.62, 0.62, 0.62 }, [1] = { 1, 1, 1 }, [2] = { 0.12, 1, 0 }, [3] = { 0, 0.44, 0.87 },
        [4] = { 0.64, 0.21, 0.93 }, [5] = { 1, 0.50, 0 }, [6] = PALE_GOLD, [7] = { 0, 0.80, 1 },
    },
    -- By rank group: 0 normal, 1 elite, 2 rare, 3 rare elite.
    rank = { [0] = { 1, 1, 1 }, [1] = GOLD, [2] = { 0.78, 0.78, 0.85 }, [3] = { 0.60, 0.80, 1 } },
    badge = { served = ACCENT, custom = { 1, 0.50, 0 }, ["3.3.5"] = { 0.75, 0.75, 0.75 } },

    stripe = { 1, 1, 1, 0.02 },
    hover = FromHex("4fd1c5", 0.08),
    selected = FromHex("4fd1c5", 0.18),
    selectedEdge = ACCENT,
    checked = FromHex("4fd1c5", 0.05),

    input = FromHex("151a21"),
    inputBorder = FromHex("2f3a44"),
    inputFocus = ACCENT,
    inputActive = FromHex("4fd1c5", 0.55), -- A filter that holds a value.

    warning = FromHex("3a1517", 0.95),
    warningText = FromHex("f2b3b3"),
    preview = { 0, 0, 0, 0.6 },

    button = FromHex("151a21"),
    buttonBorder = FromHex("2a313b"),
    buttonText = FromHex("d5dae2"),
    buttonPressed = FromHex("0f1318"),
    primary = ACCENT,
    primaryText = FromHex("0b0e12"),
    primaryPressed = FromHex("3aa89e"),

    pending = FromHex("f6c453"),            -- A build row that needs a look (an installed row wins, several displays).
    confirm = FromHex("2a1f0d"),            -- The yellow confirm bar before writing to the world DB.
    confirmBorder = FromHex("5a4316"),
    confirmText = FromHex("f6d38a"),
    confirmButton = FromHex("f6c453"),
    confirmButtonText = FromHex("1a1306"),
    confirmButtonPressed = FromHex("d9a838"),
}

-- A button's colours: variant normal or primary, state normal, hover, pressed, disabled or on (a toggle that is set).
-- Returns { fill, border, text, alpha }.
-- Solid variants: the fill, its text and its pressed fill (primary: the teal main action; confirm: the yellow go-ahead).
local SOLID = {
    primary = { "primary", "primaryText", "primaryPressed" },
    confirm = { "confirmButton", "confirmButtonText", "confirmButtonPressed" },
}

function RB.ButtonStyle(variant, state)
    local T = RB.THEME
    local solid = SOLID[variant]
    local fill = solid and T[solid[1]] or T.button
    local style = { fill = fill, border = solid and fill or T.buttonBorder, text = solid and T[solid[2]] or T.buttonText, alpha = 1 }
    if state == "hover" then
        style.border = solid and T.buttonText or T.accent
    elseif state == "pressed" then
        style.fill = solid and T[solid[3]] or T.buttonPressed
        style.border = solid and style.fill or T.accent
    elseif state == "on" and not solid then
        style.text, style.border, style.fill = T.accent, T.accent, T.accentFill
    elseif state == "disabled" then
        style.alpha = DISABLED_ALPHA
    end
    return style
end

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

-- A label ending in an RB.Key hint, split so a button can draw the hint at its right edge: text, hint (nil without).
function RB.SplitKeyHint(label)
    local marker = " " .. RB.Hex(RB.THEME.label)
    local at, from = nil, 1
    while true do
        local found = label:find(marker, from, true)
        if not found then break end
        at, from = found, found + 1
    end
    local hint = at and label:sub(at + #marker):match("^([^|]+)|r$")
    if not hint then
        return label
    end
    return label:sub(1, at - 1), hint
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
