-- /rb seed: exports the retail asset seed players ship with (lua_scripts docs/WOWDECOMP-RETAIL-ASSET-DELIVERY.md).
-- Asks the server for every display id its templates use (CloticScripts/RetailBrowser/RetailBrowser_SeedIds.lua,
-- GM accounts only), collects the pages of both kinds, then RetailExportSeed writes the seed. The server pushes the
-- pages itself; a request still unanswered after RB.SEED_WAIT seconds can be sent again.

local RB = RetailBrowser
local Seed = {}
RB.Seed = Seed

RB.SEED_WAIT = 30
local AIO_NAME = "RetailBrowser"
local KINDS = { "creature", "item" }

-- kind -> { pages = { [page] = ids }, count = pageCount }, while a request is open.
local request

local function Complete(kind)
    local pages = request[kind]
    if not pages or not pages.count then
        return false
    end
    for page = 1, pages.count do
        if not pages[page] then
            return false
        end
    end
    return true
end

local function Joined(kind)
    local ids = {}
    for page = 1, request[kind].count do
        for _, id in ipairs(request[kind][page]) do
            ids[#ids + 1] = id
        end
    end
    return ids
end

local function Export()
    local creatures, items = Joined("creature"), Joined("item")
    request = nil
    RB.Print(("exporting the seed for %d creature and %d item display ids..."):format(#creatures, #items))
    local folder, files, skipped, err = RetailExportSeed(creatures, items)
    RB.Print(("seed written to %s: %d files, %d displays skipped."):format(folder, files, skipped))
    if err then
        RB.Print("first error: " .. err)
    end
end

local function OnSeedIds(_, kind, page, pageCount, ids)
    if not request or not request[kind] then
        return
    end
    request[kind].count = pageCount
    request[kind][page] = ids
    for _, each in ipairs(KINDS) do
        if not Complete(each) then
            return
        end
    end
    Export()
end

function Seed.Request()
    if not AIO then
        RB.Print("the seed needs AIO_Client.")
        return
    end
    request = { creature = {}, item = {}, asked = GetTime() }
    AIO.Handle(AIO_NAME, "RequestSeedIds")
    RB.Print("asked the server for its display ids.")
end

-- Called by the slash command handler; true when the message was a seed command.
function Seed.Command(message)
    if strtrim(message or "") ~= "seed" then
        return false
    end
    if request and GetTime() - request.asked < RB.SEED_WAIT then
        RB.Print("a seed request is already waiting for the server.")
        return true
    end
    Seed.Request()
    return true
end

if AIO then
    AIO.AddHandlers(AIO_NAME, { SeedIds = OnSeedIds })
end
