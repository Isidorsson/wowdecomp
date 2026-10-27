-- RetailDetails fixtures with the field names BrowserDetails.cpp sets.
local F = {}

function F.Display(id, extra)
    return { displayId = id, probability = 0.5, scale = 1, modelFdid = 1011653, modelId = 7661, extraId = extra or 0, race = 1,
        sex = 0, sdBake = 11, hdBake = 12, custom = false, known = true }
end

function F.Creature(count)
    local displays = {}
    for i = 1, count do
        displays[i] = F.Display(111838 + i, i == 1 and 130617 or 0)
    end
    return { kind = "creature", id = 203205, creatureId = 203205, name = "Stormwind Guard", title = "", type = 7,
        typeName = "Humanoid", family = 0, familyName = "", classification = 0, displayId = 111839, displays = displays, fields = {} }
end

function F.Item()
    local stats = {}
    for i = 1, 8 do
        stats[i] = { type = i, name = "Stat" .. i, percent = 1000, socketCost = 0, value = 10 * i }
    end
    return { kind = "item", id = 19019, itemId = 19019, name = "Thunderfury", quality = 5, itemLevel = 80, sparseItemLevel = 80,
        requiredLevel = 60, expansion = 0, classId = 2, subclassId = 7, subclassName = "Sword", inventoryType = 13, material = 1,
        sheatheType = 3, allowableClass = -1, icon = 135349, displayId = 30606, stats = stats,
        appearances = { { displayId = 30606, appearanceId = 1, modifiedAppearanceId = 2, modifier = 0, models = { 3, 4 }, custom = false, known = true } },
        fields = {
            { label = "Sockets", value = "types 1, 2, bonus enchant 3", copy = "types 1, 2, bonus enchant 3" },
            { label = "Weapon", value = "delay 1900 ms, damage variance 0.4, damage type 0", copy = "1900" },
            { label = "Damage", value = "100 - 200 (78.9 dps) at item level 80", copy = "200" },
        } }
end

function F.Set()
    return { kind = "set", id = 9001, setId = 1, source = "itemset", classMask = 1 + 1024, expansion = 0, armour = 4, name = "Battlegear",
        pieces = {
            { itemId = 16963, name = "Helm", inventoryType = 1, displayId = 29000, modifiedAppearanceId = 5, custom = false },
            { itemId = 16964, name = "Gauntlets", inventoryType = 10, displayId = 29001, modifiedAppearanceId = 6, custom = true },
        }, fields = {} }
end

function F.Model()
    return { kind = "model", id = 1011653, fdid = 1011653, name = "humanmale.m2", creature = true, userCount = 2, displayCount = 0,
        displays = {}, users = { { kind = "creature", id = 1, name = "A", displayId = 3 }, { kind = "item", id = 2, name = "B", displayId = 4 } },
        fields = {} }
end

function F.Mount()
    return { kind = "mount", id = 6, mountId = 6, spellId = 458, name = "Brown Horse", mountType = 230, displayId = 2404,
        displays = { F.Display(2404), F.Display(2405) }, fields = {} }
end

return F
