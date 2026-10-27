local h = require("harness")
local F = require("fixtures")
local RB = RetailBrowser
local T = RB.THEME

local function Plain(text)
    return (text:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", ""))
end

h.describe("Header facts", function()
    h.it("creature: kind, id, type, rank, race NPC, source", function()
        h.assertEq(Plain(RB.HeaderFacts(F.Creature(1))), "Creature 203205 · Humanoid · Normal · Human NPC · served")
    end)

    h.it("only the source word takes its badge colour", function()
        local facts = RB.HeaderFacts(F.Creature(1))
        h.assertTrue(facts:find(RB.Colored("served", T.badge.served), 1, true) ~= nil, facts)
        h.assertEq(select(2, facts:gsub("|c", "")), 1)
    end)

    h.it("creature drawn by a plain model says so", function()
        local d = F.Creature(1)
        d.displays[1].extraId = 0
        h.assertEq(Plain(RB.HeaderFacts(d)), "Creature 203205 · Humanoid · Normal · plain model · served")
    end)

    h.it("item: kind, id, slot and type, expansion, source", function()
        h.assertEq(Plain(RB.HeaderFacts(F.Item())), "Item 19019 · One-Hand Sword · Classic · served")
    end)

    h.it("set: source kind, id, armour, expansion, pieces", function()
        h.assertEq(Plain(RB.HeaderFacts(F.Set())), "Item set 1 · Plate · Classic · 2 pieces")
        local d = F.Set()
        d.source, d.pieces = "transmog", { d.pieces[1] }
        h.assertEq(Plain(RB.HeaderFacts(d)), "Transmog set 1 · Plate · Classic · 1 piece")
    end)

    h.it("model: fdid, kind of model, users", function()
        h.assertEq(Plain(RB.HeaderFacts(F.Model())), "Model fdid 1011653 · creature model · 2 users")
    end)

    h.it("mount: id, spell, type", function()
        h.assertEq(Plain(RB.HeaderFacts(F.Mount())), "Mount 6 · spell 458 · Ground")
    end)

    h.it("skips facts the entry does not have", function()
        local d = F.Creature(1)
        d.typeName = ""
        h.assertEq(Plain(RB.HeaderFacts(d)), "Creature 203205 · Normal · Human NPC · served")
    end)
end)

h.describe("Custom row warning", function()
    h.it("says nothing when retail's row is served", function()
        h.assertEq(RB.WarningText(F.Creature(1)), nil)
    end)

    h.it("a stock 3.3.5 row", function()
        local d = F.Creature(1)
        d.displays[1].custom, d.displays[1].displayId = true, 3167
        h.assertEq(Plain(RB.WarningText(d)),
            "Stock 3.3.5 row wins: display 3167 is in your CreatureDisplayInfo.dbc, so the client draws that, not retail's.")
    end)

    h.it("a row the server added, for an item", function()
        local d = F.Item()
        d.appearances[1].custom, d.appearances[1].displayId = true, 200000
        h.assertEq(Plain(RB.WarningText(d)),
            "Custom row wins: display 200000 is a row your server added to ItemDisplayInfo.dbc, so the client draws that, not retail's.")
    end)
end)
