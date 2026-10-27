local h = require("harness")
local mock = require("mock_wow")
local RB = RetailBrowser
local THEME = RB.THEME

-- Groups hold colours by key (quality[3], rank[1], badge.served) rather than being colours themselves.
local GROUPS = { quality = true, rank = true, badge = true }

-- Tokens later phases use that no file reads yet.
local NEW_TOKENS = { "rail", "accent", "accentFill", "section", "button", "buttonBorder", "buttonText", "buttonPressed",
    "primary", "primaryText", "primaryPressed", "pending", "confirm", "confirmBorder", "confirmText", "confirmButton",
    "confirmButtonText" }

-- The Ledger palette (approved mockups), as RB.Hex renders it.
local LEDGER = {
    panel = "12161c", well = "0f1318", band = "0b0e12", border = "2a313b", borderDim = "1e242c",
    title = "4fd1c5", accent = "4fd1c5", section = "4fd1c5", text = "e6e9ee", label = "7d8696", value = "b4bcc8",
    selectedEdge = "4fd1c5", input = "151a21", inputBorder = "2f3a44", inputFocus = "4fd1c5",
    rail = "0b0e12", accentFill = "16211f",
    button = "151a21", buttonBorder = "2a313b", buttonText = "d5dae2", primary = "4fd1c5", primaryText = "0b0e12",
}

local function IsColour(value)
    if type(value) ~= "table" or (#value ~= 3 and #value ~= 4) then
        return false
    end
    local count = 0
    for _ in pairs(value) do count = count + 1 end
    if count ~= #value then
        return false
    end
    for _, channel in ipairs(value) do
        if type(channel) ~= "number" or channel < 0 or channel > 1 then
            return false
        end
    end
    return true
end

local function TocFiles()
    local files = {}
    for line in io.lines("RetailBrowser.toc") do
        if line:match("%.lua%s*$") then
            files[#files + 1] = line:match("^%s*(.-)%s*$")
        end
    end
    return files
end

-- Every "T.key" / "THEME.key" (and "THEME.group.sub") the addon reads, as { token, sub, where }.
local function ThemeReads(files)
    local reads = {}
    for _, file in ipairs(files) do
        local n = 0
        for line in io.lines(file) do
            n = n + 1
            for token, sub in line:gmatch("%f[%w_]T%.([%a_][%w_]*)%.?([%a_]?[%w_]*)") do
                reads[#reads + 1] = { token = token, sub = sub, where = file .. ":" .. n }
            end
            for token, sub in line:gmatch("%f[%w_]THEME%.([%a_][%w_]*)%.?([%a_]?[%w_]*)") do
                reads[#reads + 1] = { token = token, sub = sub, where = file .. ":" .. n }
            end
        end
    end
    return reads
end

-- The reads the theme cannot satisfy with a valid colour, as "where: token" strings.
local function BadReads(theme, reads)
    local bad = {}
    for _, read in ipairs(reads) do
        local value = theme[read.token]
        local ok
        if GROUPS[read.token] then
            ok = type(value) == "table" and (read.sub == "" or IsColour(value[read.sub]))
        else
            ok = IsColour(value)
        end
        if not ok then
            bad[#bad + 1] = read.where .. ": " .. read.token .. (read.sub ~= "" and "." .. read.sub or "")
        end
    end
    return bad
end

local function Without(theme, key)
    local copy = {}
    for k, v in pairs(theme) do copy[k] = v end
    copy[key] = nil
    return copy
end

h.describe("Theme helpers", function()
    h.it("Hex rounds each channel to two hex digits", function()
        h.assertEq(RB.Hex({ 1, 0.5, 0 }), "|cffff8000")
        h.assertEq(RB.Hex({ 0, 0, 0, 0.3 }), "|cff000000")
    end)

    h.it("Colored wraps text in the colour and a reset", function()
        h.assertEq(RB.Colored("Hi", { 1, 1, 1 }), "|cffffffffHi|r")
        h.assertEq(RB.Colored(42, { 0, 0, 1 }), "|cff0000ff42|r")
    end)

    local setters = {
        { "SetColor", "SetTextColor" }, { "SetVertex", "SetVertexColor" },
        { "SetBorder", "SetBackdropBorderColor" }, { "SetBack", "SetBackdropColor" },
    }
    for _, pair in ipairs(setters) do
        local helper, method = pair[1], pair[2]
        h.it(helper .. " passes rgb and defaults alpha to 1", function()
            local region = mock.recorder()
            RB[helper](region, { 0.1, 0.2, 0.3 })
            h.assertEq(region.calls[method], { 0.1, 0.2, 0.3, 1 })
        end)
        h.it(helper .. " keeps an explicit alpha", function()
            local region = mock.recorder()
            RB[helper](region, { 0.1, 0.2, 0.3, 0.4 })
            h.assertEq(region.calls[method], { 0.1, 0.2, 0.3, 0.4 })
        end)
    end
end)

h.describe("Theme tokens", function()
    local reads = ThemeReads(TocFiles())

    h.it("the addon reads theme tokens (scan sanity)", function()
        h.assertTrue(#reads > 50, "found " .. #reads .. " reads")
    end)

    h.it("every token the addon reads is a valid colour", function()
        h.assertEq(BadReads(THEME, reads), {})
    end)

    h.it("a missing token is reported", function()
        h.assertTrue(#BadReads(Without(THEME, "label"), reads) > 0)
        h.assertTrue(#BadReads(Without(THEME, "badge"), reads) > 0)
    end)

    h.it("every non-group token is a colour and every group entry is one", function()
        for key, value in pairs(THEME) do
            if GROUPS[key] then
                for sub, colour in pairs(value) do
                    h.assertTrue(IsColour(colour), key .. "." .. tostring(sub))
                end
            else
                h.assertTrue(IsColour(value), key)
            end
        end
    end)

    h.it("has the Ledger tokens later phases use", function()
        for _, key in ipairs(NEW_TOKENS) do
            h.assertTrue(IsColour(THEME[key]), key)
        end
    end)
end)

h.describe("Ledger palette", function()
    local keys = {}
    for key in pairs(LEDGER) do keys[#keys + 1] = key end
    table.sort(keys)
    for _, key in ipairs(keys) do
        local hex = LEDGER[key]
        h.it(key .. " is #" .. hex, function()
            h.assertTrue(IsColour(THEME[key]), key)
            h.assertEq(RB.Hex(THEME[key]), "|cff" .. hex)
        end)
    end

    h.it("selection and hover are the accent, faint", function()
        h.assertEq(RB.Hex(THEME.selected), "|cff4fd1c5")
        h.assertEq(RB.Hex(THEME.hover), "|cff4fd1c5")
        h.assertTrue(THEME.selected[4] > 0.1 and THEME.selected[4] < 0.25, "selected alpha")
        h.assertTrue(THEME.hover[4] > 0.04 and THEME.hover[4] < THEME.selected[4], "hover alpha")
    end)

    h.it("a served badge is the accent", function()
        h.assertEq(RB.Hex(THEME.badge.served), "|cff4fd1c5")
    end)
end)
