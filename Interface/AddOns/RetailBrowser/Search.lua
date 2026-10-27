-- The search behind the grid: RetailSearch with the search text, every column filter and the header sort (the index
-- filters and sorts all its entries, then hands back one page of 50), the index status poll, and the saved lists
-- (Favourites, History), which are filtered and sorted here.

local RB = RetailBrowser
local frame = RB.frame

-- filters: the RetailSearch filters the column cells set (by filter key); sort: { key = column key, code, descending }.
RB.state = { kind = "creature", source = "search", text = "", page = 1, filters = {}, sort = nil }

function RB.SetKind(kind)
    local state = RB.state
    state.kind, state.source, state.page, state.filters, state.sort, state.text = kind, "search", 1, {}, nil, ""
    RB.OnColumnsChanged()
    RB.RefreshRail()
    RB.Search(true)
end

function RB.ShowSaved(source)
    local state = RB.state
    state.source, state.page, state.filters, state.sort, state.text = source, 1, {}, nil, ""
    RB.OnColumnsChanged()
    RB.RefreshRail()
    RB.RefreshSaved()
end

-- Searches now, or after the typing pause; a search before the index is ready polls until it is.
function RB.Search(now)
    if RB.state.source ~= "search" then
        RB.RefreshSaved()
        return
    end
    RB.searchAt = GetTime() + (now and 0 or RB.SEARCH_DELAY)
end

-- The RetailSearch filter table: the cells' filters and the sort.
function RB.SearchFilters()
    local state, filters = RB.state, {}
    for key, value in pairs(state.filters) do
        filters[key] = value
    end
    if state.sort and type(state.sort.code) == "number" then
        filters.sort = state.sort.code
        filters.descending = state.sort.descending and 1 or 0
    end
    return filters
end

local function RunSearch()
    local state = RB.state
    if state.source ~= "search" then
        return
    end
    local result, err = RetailSearch(state.kind, state.text, RB.SearchFilters(), state.page)
    if not result then
        RB.SetStatus(RB.Colored(tostring(err), RB.THEME.bad))
        return
    end
    if result.status ~= "ok" then
        RB.SetStatus(result.status == "failed" and RB.Colored("index failed: " .. tostring(result.error), RB.THEME.bad) or "Building the index...")
        RB.pollAt = GetTime() + RB.POLL_SECONDS
        return
    end
    RB.ShowResult(result)
end

-- Each row's RetailDetails (cached) for its cells, and the icons for the prefetch other branches add.
local function AttachDetails(rows, kind)
    local icons = {}
    for _, row in ipairs(rows) do
        row.kind = row.kind or kind
        row.d = RB.DetailsOf(row.kind, row.id)
        if row.icon and row.icon > 0 then
            icons[#icons + 1] = row.icon
        end
    end
    RB.PrefetchIcons(icons)
end

function RB.ShowResult(result)
    RB.result = result
    local started = debugprofilestop()
    AttachDetails(result.rows, RB.state.kind)
    RB.lastPageMs = debugprofilestop() - started
    RB.SetRows(result.rows)
    if result.total > 0 then
        RB.SetPageText(RB.PageText(result.total, result.page, result.pages))
    else
        RB.SetPageText(RB.state.text == "" and not next(RB.state.filters) and "Type a name or id, or set a filter" or "No matches")
    end
end

function RB.TurnPage(step)
    local result = RB.result
    if not result or RB.state.source ~= "search" then
        return
    end
    local page = RB.state.page + step
    if page >= 1 and page <= result.pages then
        RB.state.page = page
        RB.Search(true)
    end
end

-- A saved entry passes the cells' filters (text, kind, id range).
local function SavedPasses(entry, state)
    local f = state.filters
    if state.text ~= "" and not (entry.name or ""):lower():find(state.text:lower(), 1, true) and tostring(entry.id) ~= state.text then
        return false
    end
    return (not f.kind or f.kind == entry.kind) and (not f.minId or entry.id >= f.minId) and (not f.maxId or entry.id <= f.maxId)
end

local function SavedLess(sort)
    return function(a, b)
        local x, y = a[sort.key], b[sort.key]
        if x == y then
            return a.id < b.id
        end
        if sort.descending then return x > y end
        return x < y
    end
end

-- Favourites or History, every kind at once.
function RB.RefreshSaved()
    local state = RB.state
    if state.source == "search" then
        return
    end
    local entries = state.source == "favourites" and RB.Favourites() or RB.History()
    local rows = {}
    for _, entry in ipairs(entries) do
        if SavedPasses(entry, state) then
            rows[#rows + 1] = { id = entry.id, name = entry.name, kind = entry.kind, displayId = 0, icon = 0, quality = 1 }
        end
    end
    if state.sort then
        table.sort(rows, SavedLess(state.sort))
    end
    RB.result = nil
    RB.SetRows(rows)
    RB.SetPageText(#rows .. (state.source == "favourites" and " favourites" or " viewed"))
end

function RB.ExportCsv()
    local state = RB.state
    if state.source ~= "search" then
        RB.Print("export works on search results.")
        return
    end
    local path, err = RetailExportCsv(state.kind, state.text, RB.SearchFilters())
    RB.Print(path and ("exported to " .. path) or ("export failed: " .. tostring(err)))
end

-- Index status: counts once ready; the choices that come from the index are refreshed then.
local function RefreshInfo()
    local info = RetailBrowserInfo()
    if info.status == "ready" then
        local firstReady = not RB.info
        RB.info = info
        local c = info.counts
        RB.SetStatus(string.format("index ready, %d rows", c.creature + c.item + c.set + c.model + c.mount))
        if firstReady then
            RB.RefreshRail()
        end
        return true
    end
    RB.SetStatus(info.status == "failed" and RB.Colored("index failed: " .. tostring(info.error), RB.THEME.bad) or "Building the index...")
    return info.status == "failed"
end

local poller = CreateFrame("Frame", nil, frame)
poller:SetScript("OnUpdate", function()
    local now = GetTime()
    if RB.pollAt and now >= RB.pollAt then
        RB.pollAt = nil
        if RefreshInfo() then
            RB.Search(true)
        else
            RB.pollAt = now + RB.POLL_SECONDS
        end
    end
    if RB.searchAt and now >= RB.searchAt then
        RB.searchAt = nil
        RunSearch()
    end
end)

function RB.OnWindowShown()
    if not RefreshInfo() then
        RB.pollAt = GetTime() + RB.POLL_SECONDS
    end
    RB.RefreshRail()
    RB.RefreshInspector()
end

-- Opens an entry of any kind: its tab, searched by id, with its details in the inspector.
function RB.OpenEntry(kind, id)
    RB.SetKind(kind)
    RB.SetSearchText(tostring(id))
    RB.ShowDetails(kind, id, true)
end
