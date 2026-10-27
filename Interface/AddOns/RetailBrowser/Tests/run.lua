-- Offline tests for the Retail Browser. Not in RetailBrowser.toc, so the game never loads them. From the addon folder:
--   "C:\Program Files (x86)\Lua\5.1\lua.exe" Tests/run.lua

package.path = "./?.lua;./Tests/?.lua;" .. package.path

local h = require("harness")
require("mock_wow").install()

dofile("Core.lua")
dofile("Theme.lua")
dofile("Layout.lua")
dofile("Commands.lua")
dofile("Overview.lua")
dofile("BuildModel.lua")

require("test_theme")
require("test_layout")
require("test_overview")
require("test_no_scale")
require("test_buttons")
require("test_header")
require("test_build_model")

os.exit(h.run())
