local h = require("harness")
local RB = RetailBrowser
local T = RB.THEME

h.describe("Button style", function()
    h.it("a normal button: button fill, hairline border, button text, opaque", function()
        h.assertEq(RB.ButtonStyle("normal", "normal"), { fill = T.button, border = T.buttonBorder, text = T.buttonText, alpha = 1 })
    end)

    h.it("hover turns the border accent", function()
        local style = RB.ButtonStyle("normal", "hover")
        h.assertEq(style.border, T.accent)
        h.assertEq(style.fill, T.button)
    end)

    h.it("pressed darkens the fill", function()
        local style = RB.ButtonStyle("normal", "pressed")
        h.assertEq(style.fill, T.buttonPressed)
        h.assertTrue(style.fill[1] + style.fill[2] + style.fill[3] < T.button[1] + T.button[2] + T.button[3], "darker")
    end)

    h.it("disabled keeps the colours at about 40% alpha", function()
        local style = RB.ButtonStyle("normal", "disabled")
        h.assertEq(style.fill, T.button)
        h.assertTrue(style.alpha > 0.3 and style.alpha < 0.5, tostring(style.alpha))
    end)

    h.it("on: accent text and border on the accent fill", function()
        local style = RB.ButtonStyle("normal", "on")
        h.assertEq(style.text, T.accent)
        h.assertEq(style.border, T.accent)
        h.assertEq(style.fill, T.accentFill)
    end)

    h.it("primary: accent fill, dark text", function()
        local style = RB.ButtonStyle("primary", "normal")
        h.assertEq(style.fill, T.primary)
        h.assertEq(style.text, T.primaryText)
    end)

    h.it("primary pressed darkens its fill, disabled fades it", function()
        h.assertEq(RB.ButtonStyle("primary", "pressed").fill, T.primaryPressed)
        h.assertTrue(RB.ButtonStyle("primary", "disabled").alpha < 0.5)
    end)

    h.it("confirm: the yellow fill with dark text", function()
        local style = RB.ButtonStyle("confirm", "normal")
        h.assertEq(style.fill, T.confirmButton)
        h.assertEq(style.text, T.confirmButtonText)
    end)

    h.it("every state of every variant is a full set of colours", function()
        for _, variant in ipairs({ "normal", "primary", "confirm" }) do
            for _, state in ipairs({ "normal", "hover", "pressed", "disabled", "on" }) do
                local style = RB.ButtonStyle(variant, state)
                h.assertTrue(style.fill and style.border and style.text and style.alpha, variant .. "/" .. state)
            end
        end
    end)
end)

h.describe("Key hints", function()
    h.it("splits a trailing key hint off a label", function()
        local text, hint = RB.SplitKeyHint("Copy id " .. RB.Key("Ctrl+C"))
        h.assertEq(text, "Copy id")
        h.assertEq(hint, "Ctrl+C")
    end)

    h.it("leaves a label without a trailing hint whole", function()
        h.assertEq({ RB.SplitKeyHint("Clear filters") }, { "Clear filters" })
        local prev = RB.Key("[") .. " Prev"
        h.assertEq({ RB.SplitKeyHint(prev) }, { prev })
    end)

    h.it("keeps other colour codes in the text", function()
        local text, hint = RB.SplitKeyHint(RB.Colored("x", T.bad) .. " Fav " .. RB.Key("F"))
        h.assertEq(text, RB.Colored("x", T.bad) .. " Fav")
        h.assertEq(hint, "F")
    end)
end)
