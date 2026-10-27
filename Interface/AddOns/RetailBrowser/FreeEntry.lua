-- Free template entries from the server (lua_scripts CloticScripts/RetailBrowser/RetailBrowser_FreeEntry.lua): the
-- first run a build fits in, so a lone item can take a one-wide hole and a set a hole as long as its pieces. Answers
-- are cached per tables and count until the Build tab reopens or a run is reserved. The server answers GM accounts
-- only; with no answer in RB.FREE_ENTRY_WAIT seconds the caller falls back to its own start.

local RB = RetailBrowser

local REQUEST_PREFIX = "RBFE"
local REPLY_PREFIX = "RBFS"
RB.FREE_ENTRY_WAIT = 2

-- "<tables> <count>" -> { entry = n } once answered, { asked = time } while asking, { failed = true } after an error.
local answers = {}
-- request id -> answers key
local pending = {}
local lastId = 0

local function Send(text)
    lastId = lastId + 1
    SendAddonMessage(REQUEST_PREFIX, lastId .. " " .. text, "WHISPER", UnitName("player"))
    return lastId
end

-- item, creature or both: the template tables the entries' SQL writes (a mixed build needs a run free in both).
function RB.EntryTables(entries)
    local item, creature = false, false
    for _, d in ipairs(entries) do
        if RB.BUILD_TABLES[d.kind] == "creature" then creature = true else item = true end
    end
    return item and creature and "both" or (creature and "creature" or "item")
end

-- The first free entry for count entries in a row; nil while asking, false when the server did not answer.
function RB.FreeEntry(tables, count)
    local key = tables .. " " .. count
    local answer = answers[key]
    if not answer then
        answers[key] = { asked = GetTime() }
        pending[Send("find " .. key)] = key
        return nil
    end
    if answer.entry then
        return answer.entry
    end
    if answer.failed or GetTime() - answer.asked >= RB.FREE_ENTRY_WAIT then
        return false
    end
    return nil
end

-- The free start for a build's tables and length (result from a first build), and the Start entry hint. While the
-- server is asking, a rebuild is scheduled for when the wait runs out.
function RB.FreeStart(result, fallback)
    local free = RB.FreeEntry(result.tables, result.nextEntry - result.firstEntry)
    if free then
        return free, "free " .. free .. " (server)"
    end
    if free == nil then
        RB.buildAt = GetTime() + RB.FREE_ENTRY_WAIT
        return nil, "asking server..."
    end
    return nil, fallback .. " (no server answer)"
end

-- Marks a build's entries taken on the server (SQL copied or exported but maybe not applied), so later builds skip them.
function RB.ReserveBuilt(result)
    if not result then
        return
    end
    Send(string.format("reserve %s %d %d", result.tables, result.firstEntry, result.nextEntry - 1))
    RB.ForgetFreeEntries()
end

-- The database may have changed (SQL applied), so the next build asks again.
function RB.ForgetFreeEntries()
    wipe(answers)
    wipe(pending)
end

-- Apply to DB (lua_scripts RetailBrowser_Apply.lua): "<id> apply <first> <last> <parts>", then the SQL in parts of
-- APPLY_PART_BYTES (an addon message holds 255 bytes with its prefix and header), APPLY_PARTS_PER_SEND every
-- APPLY_SEND_EVERY seconds to stay under the server's addon flood limit of 100 a second.
local APPLY_PART_BYTES = 200
local APPLY_PARTS_PER_SEND = 40
local APPLY_SEND_EVERY = 0.5
local APPLY_WAIT = 30
-- { id, sql, parts, nextPart, sendAt, onDone } while an apply is sending or waiting for its answer.
local applying

local function FinishApply(ok, text)
    local onDone = applying.onDone
    applying = nil
    if ok then RB.ForgetFreeEntries() end
    onDone(ok, text)
end

local applySender = CreateFrame("Frame")
applySender:Hide()
applySender:SetScript("OnUpdate", function(self)
    if GetTime() < applying.sendAt then
        return
    end
    if applying.nextPart > applying.parts then
        self:Hide()
        return FinishApply(false, "no answer from the server in " .. APPLY_WAIT .. " s (GM account? RetailBrowser_Apply.lua loaded?)")
    end
    local last = math.min(applying.nextPart + APPLY_PARTS_PER_SEND - 1, applying.parts)
    for n = applying.nextPart, last do
        local text = applying.sql:sub((n - 1) * APPLY_PART_BYTES + 1, n * APPLY_PART_BYTES)
        SendAddonMessage(REQUEST_PREFIX, string.format("%d part %d %s", applying.id, n, text), "WHISPER", UnitName("player"))
    end
    applying.nextPart = last + 1
    applying.sendAt = GetTime() + (applying.nextPart > applying.parts and APPLY_WAIT or APPLY_SEND_EVERY)
end)

-- Runs sql on the server's world DB for entries first to last; onDone(ok, text) gets the server's answer. False when
-- an apply is still running.
function RB.ApplySql(first, last, sql, onDone)
    if applying then
        return false
    end
    local parts = math.ceil(#sql / APPLY_PART_BYTES)
    applying = { id = Send(string.format("apply %d %d %d", first, last, parts)), sql = sql, parts = parts, nextPart = 1,
        sendAt = 0, onDone = onDone }
    applySender:Show()
    return true
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:SetScript("OnEvent", function(_, _, prefix, message)
    if prefix ~= REPLY_PREFIX then
        return
    end
    local id, status, payload = message:match("^(%d+) (%a+) (.*)$")
    if applying and tonumber(id) == applying.id then
        applySender:Hide()
        return FinishApply(status == "ok", payload)
    end
    local key = pending[tonumber(id)]
    if not key then
        return
    end
    pending[tonumber(id)] = nil
    if status == "ok" and tonumber(payload) then
        answers[key] = { entry = tonumber(payload) }
    else
        answers[key] = { failed = true }
        RB.Print("free entry lookup failed: " .. tostring(payload))
    end
    RB.ScheduleBuild()
end)
