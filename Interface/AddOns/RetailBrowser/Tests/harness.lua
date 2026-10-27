-- Tiny pure-Lua test harness (after lua_scripts/AIO_Server/WayOfLeveling/Tests/wol_harness.lua).

local M = {}
local groups = {}
local currentGroup = nil

function M.describe(name, fn)
    local group = { name = name, tests = {} }
    groups[#groups + 1] = group
    local prev = currentGroup
    currentGroup = group
    fn()
    currentGroup = prev
end

function M.it(name, fn)
    local group = currentGroup
    if not group then
        group = { name = "(no group)", tests = {} }
        groups[#groups + 1] = group
    end
    group.tests[#group.tests + 1] = { name = name, fn = fn }
end

local function deepEq(a, b)
    if a == b then return true end
    if type(a) ~= "table" or type(b) ~= "table" then return false end
    for k, v in pairs(a) do
        if not deepEq(v, b[k]) then return false end
    end
    for k in pairs(b) do
        if a[k] == nil then return false end
    end
    return true
end

local function fmt(v)
    if type(v) == "table" then
        local parts = {}
        for i, x in ipairs(v) do parts[i] = fmt(x) end
        return "{" .. table.concat(parts, ", ") .. "}"
    end
    if type(v) == "string" then return string.format("%q", v) end
    return tostring(v)
end

function M.assertEq(a, b, msg)
    if not deepEq(a, b) then
        error((msg or "assertEq") .. ": expected " .. fmt(b) .. ", got " .. fmt(a), 2)
    end
end

function M.assertTrue(v, msg)
    if not v then error((msg or "assertTrue") .. ": expected truthy, got " .. fmt(v), 2) end
end

function M.run()
    local passed, failed = 0, 0
    for _, group in ipairs(groups) do
        for _, test in ipairs(group.tests) do
            local ok, err = pcall(test.fn)
            if ok then
                passed = passed + 1
                print("  PASS  " .. group.name .. " / " .. test.name)
            else
                failed = failed + 1
                print("  FAIL  " .. group.name .. " / " .. test.name)
                print("        " .. tostring(err))
            end
        end
    end
    print(string.format("\n%d passed, %d failed, %d total", passed, failed, passed + failed))
    return failed == 0 and 0 or 1
end

return M
