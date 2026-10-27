local h = require("harness")

-- The window is one size for everyone: nothing may set or read a window scale (an old saved "scale" is ignored).
local FORBIDDEN = { "SetWindowScale", "RefreshScaleButtons", 'CharSetting%("scale"', "SCALES", '"%d+%%"' }

local function TocFiles()
    local files = {}
    for line in io.lines("RetailBrowser.toc") do
        if line:match("%.lua%s*$") then
            files[#files + 1] = line:match("^%s*(.-)%s*$")
        end
    end
    return files
end

h.describe("Window scale", function()
    h.it("no file has scale buttons or reads a window scale", function()
        local found = {}
        for _, file in ipairs(TocFiles()) do
            local n = 0
            for line in io.lines(file) do
                n = n + 1
                for _, pattern in ipairs(FORBIDDEN) do
                    if line:match(pattern) then
                        found[#found + 1] = file .. ":" .. n .. ": " .. pattern
                    end
                end
            end
        end
        h.assertEq(found, {})
    end)
end)
