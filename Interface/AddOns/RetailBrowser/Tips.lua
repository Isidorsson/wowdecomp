-- Every tooltip of the Retail Browser: a gold title, one or two white lines on what the control does, the key in grey,
-- and for a disabled control a red line saying why. RB.AttachTip(frame, key) shows RB.TIPS[key]; a function key or
-- tip returns the tip at hover time (cells whose meaning depends on the column).

local RB = RetailBrowser
local T = RB.THEME

local SOURCE_LINES = {
    "served: the client draws the retail row it reads from CASC.",
    "3.3.5: your installed DBC has this id as a stock 3.3.5 row, and that row wins.",
    "custom: your installed DBC has this id as a row your server added, and that row wins.",
}
local COPY = "Click copies the value; Shift+click adds it to the copy buffer."

RB.TIPS = {
    -- Window.
    close = { "Close", "Closes the browser. Esc in the grid does it too (after clearing checked rows).", key = "Esc" },
    scale = { "Window scale", "Draws the whole window at this scale; its top right corner stays put. Saved per character." },
    indexAtLogin = { "Index at login", "Builds the search index on a worker thread when you log in, so the first /rb is ready at once (keeps about 120 MB)." },
    tab_creature = { "Creatures", "Retail creatures: names, ranks, every display and its NPC Extra row.", key = "Ctrl+1" },
    tab_item = { "Items", "The 104k searchable retail items with their appearances and ItemSparse stats.", key = "Ctrl+2" },
    tab_set = { "Sets", "Transmog sets and item sets with their pieces.", key = "Ctrl+3" },
    tab_model = { "Models", "Model FileDataIDs and every creature, item or mount that uses them.", key = "Ctrl+4" },
    tab_mount = { "Mounts", "Retail mounts with their spells and displays.", key = "Ctrl+5" },
    tab_favourites = { "Favourites", "Entries you saved, of every kind (account-wide).", key = "F8" },
    tab_history = { "History", "The last 200 entries you looked at.", key = "F9" },
    chip = { "Active filter", "Click to clear this filter." },
    clearFilters = { "Clear filters", "Clears every column filter and the search text." },
    prevPage = { "Previous page", "Results come 50 a page; turning pages only slices the ranking.", key = "[" },
    nextPage = { "Next page", "Results come 50 a page; turning pages only slices the ranking.", key = "]" },
    exportCsv = { "Export CSV", "Writes every match of this search (all pages) to Logs\\RetailBrowser\\<kind>-<time>.csv.", key = "E" },
    keys = { "Key owner", "Green: the grid has the keys and letter keys are commands. Click to hand the keys back to the game, or to the grid." },
    clearHistory = { "Clear history", "Forgets every entry in History.", key = "Ctrl+Del" },
    grip = { "Resize", "Drag to resize the window (at least 960x600). The size is saved per character." },
    splitter = { "Grid / drawer split", "Drag up for a bigger drawer, down for a longer list. Saved per character.", key = "Ctrl+Up / Ctrl+Down" },

    -- Filter cells.
    filter_search = { "Search", "A name (case and accents ignored) or any id of the entry. Searches the whole index.", key = "/" },
    filter_range = { "Range filter", "200-  -300  100-200  >=5  >5  <=5  <5  =5. Filters the whole index, not the page; 0 is a value too." },
    filter_exact = { "Display filter", "An exact display id: matches any of the entry's displays. Filters the whole index." },
    filter_choice = { "Choice filter", "Click to pick a value (Any turns it off). Filters the whole index." },
    filter_favourites = { "Favourites of this kind", "Opens Favourites filtered to this tab's kind." },

    -- Bulk bar.
    bulk_ids = { "Copy ids", "Copies the ids of every checked row, comma-separated.", key = "Ctrl+C" },
    bulk_rows = { "Copy rows", "Copies the checked rows' visible columns as tab-separated text with a header.", key = "Ctrl+Shift+C" },
    bulk_gm = { "GM commands", "Copies each checked entry's GM command (.morph or .additem), one per line.", key = "G" },
    bulk_add = { "Add items", "Sends .additem for every checked item and set piece; more than 10 lines ask for a second press.", key = "I",
        disabled = "Check items or sets first." },
    bulk_try = { "Try on", "Wears every checked item and set on the preview at once.", key = "Y", disabled = "Check items or sets first." },
    bulk_fav = { "Favourite", "Adds the checked rows to Favourites, or removes them when all are favourites.", key = "F" },
    bulk_csv = { "Copy CSV", "Copies the checked rows as CSV to the clipboard.", key = "Shift+E" },
    bulk_build = { "Build SQL", "Opens Build SQL for the checked rows, entries counting up from Start entry.", key = "B",
        disabled = "SQL is built for items, sets and creatures." },
    bulk_clear = { "Clear checks", "Unchecks every row.", key = "Esc" },
    checkPage = { "Check page", "Checks every row of this page.", key = "Ctrl+A" },

    -- Drawer tabs.
    tab_details = { "Details", "Every id of the entry in groups; click a value to copy it.", key = "D" },
    tab_stats = { "Stats", "The ItemSparse stats at the item's own level, sockets and weapon fields.", key = "S" },
    tab_displays = { "Displays", "Every display (appearance, piece, user). The picked one drives the preview and every action.", key = "P" },
    tab_usedby = { "Used by", "Everything else that uses the picked display's model; click one to open it.", key = "U" },
    tab_build = { "Build SQL", "World DB SQL for the focused row, the checked rows or the page.", key = "B" },
    tab_raw = { "Raw", "Every field RetailDetails labels; click one to copy it.", key = "R" },
    list_displays = { "Display", "Click picks it for the preview and every action (Shift+click opens a model's user).", key = "Left / Right" },
    list_usedby = { "User of the model", "Click opens this entry in its tab." },
    list_field = { "Field", "Click copies the value." },
    group_ids = { "Ids", "Every id of the entry and its picked display; click one to copy it." },
    group_item = { "Item", "The item's quality, levels, slot, class and classes (Item and ItemSparse)." },
    group_creature = { "Creature", "The creature's name, title, type, family, rank and how many displays it has." },
    group_display = { "Display", "The picked display: its probability, scale, NPC race and sex, and source.", key = "Left / Right" },
    group_stats = { "Stats", "Retail stats at the item's own level (Build maps them to 3.3.5 stats)." },
    group_set = { "Set", "The set's armour, expansion and pieces." },
    group_piece = { "Piece", "The picked piece of the set.", key = "Left / Right" },
    group_user = { "User", "The picked user of the model.", key = "Left / Right" },
    group_mount = { "Mount", "The mount's name, type and displays." },

    -- Preview toolbar.
    preview = { "Preview", "Drag to turn it, the mouse wheel zooms. Items and sets are shown on your character." },
    tool_prev = { "Previous display", "Picks the previous display for the preview and every action.", key = "Left" },
    tool_next = { "Next display", "Picks the next display for the preview and every action.", key = "Right" },
    tool_turnLeft = { "Turn left", "Turns the model a sixteenth to the left." },
    tool_turnRight = { "Turn right", "Turns the model a sixteenth to the right." },
    tool_zoomIn = { "Zoom in", "Moves the model nearer.", key = "=" },
    tool_zoomOut = { "Zoom out", "Moves the model further away.", key = "-" },
    tool_fit = { "Fit", "Frames the display by its scale and the creature's type (dragonkin and giants start further back)." },
    tool_reset = { "Reset", "Turns the model to the front and goes back to the fitted zoom." },
    tool_anim = { "Animation", "Cycles Stand, Walk, Run, Ready, Talk and Dance." },
    tool_copy = { "Copy display id", "Copies the picked display id.", key = "Alt+C" },

    -- Actions.
    action_morph = { "Morph me", "Sends .morph with the picked display.", key = "M", disabled = "Pick a creature, mount or creature model with a display first." },
    action_npcModel = { "Set target model", "Sends .npc set model with the picked display to your target.", key = "T",
        disabled = "Target an NPC and pick an entry with a creature display first." },
    action_addItem = { "Add item", "Sends .additem (a set adds every piece). Works for items the server has.", key = "I", disabled = "Select an item or a set first." },
    action_tryOn = { "Try on", "Shows the item or set on your character in the preview.", key = "Y", disabled = "Select an item or a set first." },
    action_copyId = { "Copy id", "Copies the entry's id (a set's own TransmogSet or ItemSet id).", key = "Ctrl+C", disabled = "Select a row first." },
    action_copyDisplay = { "Copy display id", "Copies the picked display id (green in the Displays tab).", key = "Alt+C", disabled = "This entry has no display." },
    action_copyAll = { "Copy all", "Copies every field as plain text.", key = "Ctrl+Shift+C", disabled = "Select a row first." },
    action_copyGm = { "GM command", "Copies .morph for a display, or the .additem lines of an item or set.", key = "G", disabled = "This entry has no GM command." },
    action_favourite = { "Favourite", "Saves the entry to Favourites, or removes it.", key = "F", disabled = "Select a row first." },
    action_build = { "Build SQL", "Opens Build SQL for this entry.", key = "B", disabled = "SQL is built for items, sets and creatures." },

    -- Build SQL.
    scope_focused = { "Focused row", "Builds the SQL of the entry the drawer shows." },
    scope_checked = { "Checked rows", "Builds one entry per checked row (at most 50), counting up from Start entry." },
    scope_page = { "Whole page", "Builds one entry per row of this page (at most 50)." },
    opt_itemLevel = { "Item level", "The level retail stats are computed at and the item's ItemLevel. Empty: each item's own retail level; 0 is allowed." },
    opt_startEntry = { "Start entry", "The first new entry; later rows take the next ones. Empty: the server's first free run that fits the build (100000 and up; a lone item can fill a one-wide hole). Copy and Export reserve it. Typed: used as is, unchecked." },
    opt_baseEntry = { "Base entry", "The stock row every statement copies. Empty: a suggestion per class, subclass and slot (creatures: 15166)." },
    opt_requiredLevel = { "Required level", "Empty keeps the base row's." },
    opt_name = { "Name", "Empty uses the retail name (shown in grey). What you type replaces it in the SQL." },
    opt_subname = { "Subname", "Creatures: empty uses the retail title (shown in grey). What you type replaces it." },
    opt_scale = { "Scale", "Creatures: empty keeps the base row's scale." },
    opt_combo = { "Hybrid primary", "What 'Strength or Intellect' and the other hybrid stats become. Any: the first one named." },
    opt_retailStamina = { "Retail stamina multiplier", "On: retail's StaminaMultByILvl (x9.49 at 200). Off: 3.3.5-sized stamina." },
    opt_allVariants = { "Every set variant", "A row per set piece instead of one per slot." },
    opt_retailArmour = { "Retail armour/damage", "On: armor, damage and delay from the retail formulas at the item level (retail-sized, far below 3.3.5's; the mapping shows the base's beside them). Off: the base row's stay." },
    opt_otherDisplays = { "Other displays as modelid2-4", "Creatures: the other displays after the picked one fill modelid2 to modelid4." },
    sql_build = { "Build now", "Builds at once (changes also rebuild 0.3 s after the last one)." },
    sql_copy = { "Copy SQL", "Copies all the SQL, not just the 4 KB shown.", key = "Ctrl+Enter" },
    sql_export = { "Export SQL", "Writes all the SQL to Logs\\RetailBrowser\\<kind>-<id>-<time>.sql.", key = "Ctrl+S" },
    sql_edit = { "Edit full SQL", "Puts every statement in the box to edit (over 64 KB asks for a second click: the box can freeze the client)." },
    sql_mapping = { "Stat mapping", "Every retail stat, its value, what it became on 3.3.5 and why." },
    sql_preview = { "SQL preview", "The first row's statements (at most 4 KB); Copy and Export use all of it." },
}

-- Header tips by column key; the label is the title.
RB.COLUMN_TIPS = {
    check = "Check rows for the bulk bar: click here, Space, Ctrl+click, right-click or Shift+click a range.",
    icon = "The item's retail icon.",
    name = "Click sorts by name over the whole index; again reverses. Items: quality colour. Creatures: rank colour.",
    ilvl = "Retail item level (ItemSearchName).",
    slot = "Inventory slot; Chest includes robes.",
    type = "Armour type or weapon subclass (items), creature type (creatures), mount type (mounts).",
    quality = "Item quality.",
    id = "The entry's own id: creature, item, TransmogSet or ItemSet, model FileDataID or mount.",
    display = "The first display id (CreatureDisplayInfo or ItemDisplayInfo); its filter matches any display.",
    exp = "The expansion the entry came with.",
    source = table.concat(SOURCE_LINES, "\n"),
    fav = "* marks a favourite.",
    count = "How many displays (pieces, users) the entry has; gold when more than one.",
    rank = "Normal, elite (gold), rare (silver) or rare elite (pale blue), from Creature.Classification.",
    race = "The race of the first NPC Extra row (humanoid NPCs).",
    family = "Creature family (beasts).",
    kind = "NPC: a display with an NPC Extra row (dressed and baked). Model: a plain model.",
    setSource = "Transmog set or item set.",
    armour = "Most pieces' armour type.",
    class = "The classes the set is for.",
    modelKind = "A creature model or an item model.",
    spell = "The mount's spell id (the search box finds it).",
    mountType = "Retail MountTypeID.",
}

-- Drawer value tips by field label.
RB.FIELD_TIPS = {
    ["Creature id"] = "Creature.db2 id.",
    ["Display"] = "CreatureDisplayInfo (or ItemDisplayInfo) id of the picked display. " .. "served = delivered from retail CASC; 3.3.5 or custom = your installed DBC row wins.",
    ["Model fdid"] = "The model's FileDataID in CASC.",
    ["Model id"] = "CreatureModelData id.",
    ["NPC Extra"] = "CreatureDisplayInfoExtra id: the dressed humanoid NPC look.",
    ["Bakes sd / hd"] = "The baked face textures (FileDataIDs) of the NPC Extra row.",
    ["Picked"] = "Which display the preview and the actions use.",
    ["Probability"] = "How often the creature spawns with this display.",
    ["Scale"] = "CreatureDisplayInfo scale.",
    ["Race / sex"] = "From the NPC Extra row.",
    ["Source"] = table.concat(SOURCE_LINES, "\n"),
    ["Known"] = "Whether the display loaders know this display.",
    ["Item id"] = "Item.db2 id.",
    ["Appearance"] = "ItemAppearance id.",
    ["Mod. appear."] = "ItemModifiedAppearance id.",
    ["Icon fdid"] = "The icon's FileDataID.",
    ["Level / req"] = "Item level and required level (ItemSparse).",
    ["Class / sub"] = "Item class and subclass.",
    ["Material / exp"] = "Material and expansion.",
    ["Classes"] = "AllowableClass mask (-1: every class).",
    ["Armour, damage"] = "No retail armour or damage tables for this item: Build keeps the base row's.",
    ["Armour"] = "Retail armour at the item's own level (ItemArmorQuality, ItemArmorTotal, ArmorLocation); Build computes it at its item level.",
    ["Damage"] = "Retail weapon damage and dps at the item's own level (ItemDamage tables and ItemDelay); Build computes it at its item level.",
    ["Search key"] = "The set's key in the browser (item sets are offset by 10000000).",
    ["Class mask"] = "1 << (class - 1) per class; 0 for every class.",
    ["Name"] = "The retail name (Creature, ItemSearchName, TransmogSet or ItemSet, Mount) of the entry, piece or user.",
    ["Title"] = "The creature's title, shown under its name (Creature.db2).",
    ["Type"] = "Creatures: CreatureType (Beast, Humanoid...). Mounts: MountTypeID, which says where it can ride or fly.",
    ["Family"] = "CreatureFamily: the pet family of a beast.",
    ["Rank"] = "Creature.Classification: normal, elite, rare elite or rare.",
    ["Displays"] = "How many creature displays the entry has (a model: how many draw with it). Left/Right picks one.",
    ["Quality"] = "Item quality (ItemSearchName): 0 poor to 7 heirloom.",
    ["Slot"] = "The inventory type the item is worn in.",
    ["Kind"] = "Models: a creature or an item model. Users: what uses the model (creature, item or mount).",
    ["Users"] = "How many creatures, items and mounts use the model.",
    ["Id"] = "The id of the picked user: a creature, item or mount id.",
    ["Mount id"] = "Mount.db2 id.",
    ["Spell id"] = "The spell that summons the mount.",
    ["Expansion"] = "The set's expansion.",
    ["Pieces"] = "How many items the set lists (several per slot for a transmog set's looks).",
    ["ItemSet id"] = "ItemSet.db2 id: the set's own id, which Copy id copies.",
    ["TransmogSet id"] = "TransmogSet.db2 id: the set's own id, which Copy id copies.",
    ["Stats"] = "ItemSparse lists no stat allocations for this item.",
    stat = "A retail stat (ItemSparse): its budget share, then its value at the item's own level.",
}

local function Lines(tip)
    if type(tip[2]) == "table" then
        return tip[2]
    end
    return { tip[2], tip[3] }
end

-- Opens toward the middle of the screen: the window sits against the right edge, where ANCHOR_RIGHT runs off it.
local function TipAnchor(owner)
    local x = owner:GetCenter()
    local onRight = x and x * owner:GetEffectiveScale() > UIParent:GetRight() * UIParent:GetEffectiveScale() / 2
    return onRight and "ANCHOR_LEFT" or "ANCHOR_RIGHT"
end

-- Shows a tip on GameTooltip; disabledWhy (or the tip's own disabled line) is added in red.
function RB.ShowTip(owner, tip, disabledWhy)
    if not tip then
        return
    end
    GameTooltip:SetOwner(owner, TipAnchor(owner))
    GameTooltip:SetClampedToScreen(true)
    GameTooltip:SetText(tip[1] or "", T.title[1], T.title[2], T.title[3])
    for _, line in ipairs(Lines(tip)) do
        if line then
            GameTooltip:AddLine(line, T.text[1], T.text[2], T.text[3], 1)
        end
    end
    if tip.key then
        GameTooltip:AddLine(tip.key, T.label[1], T.label[2], T.label[3])
    end
    if disabledWhy then
        GameTooltip:AddLine(disabledWhy, T.bad[1], T.bad[2], T.bad[3], 1)
    end
    GameTooltip:Show()
end

-- key: a RB.TIPS key, a tip table or function(frame) returning one. A disabled button adds its disabled line.
function RB.AttachTip(frame, key)
    frame:HookScript("OnEnter", function(self)
        local tip = type(key) == "function" and key(self) or type(key) == "table" and key or RB.TIPS[key]
        -- 3.3.5 IsEnabled returns 1 or 0 (0 is true in Lua).
        local enabled = self.IsEnabled and self:IsEnabled()
        local disabled = self.IsEnabled and (not enabled or enabled == 0)
        RB.ShowTip(self, tip, disabled and tip and (self.disabledWhy or tip.disabled) or nil)
    end)
    frame:HookScript("OnLeave", function() GameTooltip:Hide() end)
end
