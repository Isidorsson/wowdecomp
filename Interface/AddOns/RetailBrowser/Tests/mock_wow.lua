-- Minimal 3.3.5 FrameXML stand-in, enough to execute the addon files that run at load time (Core.lua, Theme.lua).
-- PascalCase keys on a widget return a callable stub, so a frame method the code calls but the mock does not model
-- still succeeds; anything else reads nil.

local M = {}

local widgetMeta = {
    __index = function(_, key)
        if type(key) == "string" and key:match("^%u") then
            return function() end
        end
        return nil
    end,
}

function M.widget()
    return setmetatable({}, widgetMeta)
end

-- A region that records the last colour each setter received: { r, g, b, a }.
function M.recorder()
    local region = M.widget()
    region.calls = {}
    for _, method in ipairs({ "SetTextColor", "SetVertexColor", "SetBackdropColor", "SetBackdropBorderColor" }) do
        region[method] = function(self, ...)
            self.calls[method] = { ... }
        end
    end
    return region
end

function M.install()
    _G.CreateFrame = function()
        local f = M.widget()
        function f:SetScript(name, fn) self[name] = fn end
        return f
    end
    _G.SlashCmdList = {}
    _G.wipe = function(t)
        for k in pairs(t) do t[k] = nil end
        return t
    end
    return M
end

return M
