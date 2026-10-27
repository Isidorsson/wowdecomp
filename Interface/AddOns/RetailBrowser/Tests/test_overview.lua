local h = require("harness")
local RB = RetailBrowser
local T = RB.THEME

local F = require("fixtures")
local Creature, Item, Set, Model, Mount = F.Creature, F.Item, F.Set, F.Model, F.Mount

local function Titles(sections)
    local titles = {}
    for i, section in ipairs(sections) do titles[i] = section.title end
    return titles
end

local function Section(sections, title)
    for _, section in ipairs(sections) do
        if section.title == title then return section end
    end
end

local function LineWith(section, pattern)
    for _, line in ipairs(section.lines) do
        if (line.left or ""):match(pattern) or (line.right or ""):match(pattern) then return line end
    end
end

h.describe("Overview sections", function()
    h.it("is empty without details", function()
        h.assertEq(RB.OverviewSections(nil), {})
    end)

    h.it("creature: ids, model slots, creature, picked display", function()
        h.assertEq(Titles(RB.OverviewSections(Creature(1))), { "Ids", "Model slots", "Creature", "Picked display" })
    end)

    h.it("creature model slots list modelid1-4, empty ones greyed", function()
        local slots = Section(RB.OverviewSections(Creature(2)), "Model slots")
        h.assertEq(slots.kind, "list")
        h.assertEq(#slots.entries, 4)
        h.assertEq(slots.entries[1].cells[1], "modelid1")
        h.assertEq(slots.entries[1].pick, 1)
        h.assertTrue(not slots.entries[2].dim, "slot 2 holds a display")
        h.assertTrue(slots.entries[3].dim, "slot 3 is empty")
        h.assertEq(slots.more, nil)
    end)

    h.it("creature with more than 4 displays says how many more", function()
        local slots = Section(RB.OverviewSections(Creature(6)), "Model slots")
        h.assertEq(#slots.entries, 4)
        h.assertTrue(slots.more and slots.more:match("^%+2 more") ~= nil, tostring(slots.more))
    end)

    h.it("item: tooltip, ids, item", function()
        local sections = RB.OverviewSections(Item())
        h.assertEq(sections[1].kind, "tooltip")
        h.assertEq(Titles(sections)[2], "Ids")
        h.assertEq(Titles(sections)[3], "Item")
    end)

    h.it("item tooltip: name in quality colour, item level in gold last", function()
        local lines = RB.OverviewSections(Item())[1].lines
        h.assertEq(lines[1].left, "Thunderfury")
        h.assertEq(lines[1].color, T.quality[5])
        h.assertEq(lines[#lines].left, "Item Level 80")
        h.assertEq(lines[#lines].color, T.itemLevel)
    end)

    h.it("item tooltip: slot and type, damage and speed, dps", function()
        local tooltip = RB.OverviewSections(Item())[1]
        local slot = LineWith(tooltip, "^One%-Hand$")
        h.assertEq(slot and slot.right, "Sword")
        local damage = LineWith(tooltip, "^100 %- 200 Damage$")
        h.assertEq(damage and damage.right, "Speed 1.90")
        h.assertTrue(LineWith(tooltip, "78%.9 damage per second") ~= nil, "dps line")
    end)

    h.it("item tooltip: every stat, sockets, required level", function()
        local tooltip = RB.OverviewSections(Item())[1]
        for i = 1, 8 do
            h.assertTrue(LineWith(tooltip, "^%+" .. (10 * i) .. " Stat" .. i .. "$") ~= nil, "stat " .. i)
        end
        h.assertTrue(LineWith(tooltip, "^Meta Socket$") ~= nil, "meta socket")
        h.assertTrue(LineWith(tooltip, "^Red Socket$") ~= nil, "red socket")
        h.assertTrue(LineWith(tooltip, "^Requires Level 60$") ~= nil, "required level")
    end)

    h.it("item without ItemSparse says so instead of stats", function()
        local d = Item()
        d.stats, d.fields, d.requiredLevel = nil, {}, nil
        h.assertTrue(LineWith(RB.OverviewSections(d)[1], "no ItemSparse") ~= nil)
    end)

    h.it("set: ids, set, classes, pieces", function()
        h.assertEq(Titles(RB.OverviewSections(Set())), { "Ids", "Set", "Classes", "Pieces" })
    end)

    h.it("set classes light the mask's classes", function()
        local chips = Section(RB.OverviewSections(Set()), "Classes").chips
        h.assertEq(#chips, #RB.CLASSES)
        for i, chip in ipairs(chips) do
            h.assertEq(chip.on, i == 1 or i == 11, chip.text)
        end
    end)

    h.it("set with no class mask lights every class", function()
        local d = Set()
        d.classMask = 0
        for _, chip in ipairs(Section(RB.OverviewSections(d), "Classes").chips) do
            h.assertTrue(chip.on, chip.text)
        end
    end)

    h.it("set pieces list each piece, click picks it", function()
        local pieces = Section(RB.OverviewSections(Set()), "Pieces")
        h.assertEq(#pieces.entries, 2)
        h.assertEq(pieces.entries[2].cells[1], 16964)
        h.assertEq(pieces.entries[2].pick, 2)
    end)

    h.it("model: ids and users, click opens a user", function()
        local sections = RB.OverviewSections(Model())
        h.assertEq(Titles(sections), { "Ids", "Users" })
        h.assertEq(sections[2].entries[2].open, { kind = "item", id = 2 })
    end)

    h.it("mount: ids, mount, displays", function()
        local sections = RB.OverviewSections(Mount())
        h.assertEq(Titles(sections), { "Ids", "Mount", "Displays" })
        h.assertEq(#sections[3].entries, 2)
    end)

    h.it("key-value rows copy the plain value", function()
        local ids = RB.OverviewSections(Creature(1))[1]
        h.assertEq(ids.kind, "kv")
        h.assertEq(ids.rows[1], { "Creature id", "203205", "203205" })
    end)
end)
