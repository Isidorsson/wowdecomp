# Retail Browser

An in-game search over the retail tables of the pinned build, like wago.tools inside the 3.3.5 client. It needs a
client built with `WOWDECOMP_RETAIL_ASSETS=ON` and a working `Data/Retail/RetailCasc.ini`; on any other client the
addon only prints that retail assets are not configured.

## Opening it

- `/rb` (or `/retailbrowser`) toggles the window. A key can be bound under Key Bindings, "Retail Browser".
- The window is about 1280x800 UI units (40 px inside a smaller screen) and opens at the top right, under the minimap
  and right of the chat. Drag the title bar to move it and the corner grip to resize it (down to 960x600); the
  90% / 100% / 110% buttons scale it. Place, size, scale and drawer height are saved per character
  (`RetailBrowserCharDB`); the window always stays on screen.
- The index is built on a worker thread at login ("Index at login" in the title bar, on by default; it keeps about
  120 MB for the session), so the first open is ready at once. With the option off, the first open builds it (about 1
  second on a local install; the title bar says "Building the index..." until then). It stays in memory for the
  session, `/reload` included.
- Esc in the grid clears the checked rows, then closes the window.

## Layout

From the top: the title bar, the tabs (Creatures, Items, Sets, Models, Mounts with their counts, then Favourites and
History), the query row (a chip per active filter, Clear filters, the pager and Export CSV), the grid, the bulk bar,
the splitter, the drawer, and the status line (key owner, hints, Clear history). Drag the splitter (a thin line that
glows gold on hover) up for a bigger drawer or down for a longer list; Ctrl+Up / Ctrl+Down move the same splitter four
rows a step. It keeps at least 3 grid rows and 200 px of drawer, starts with room for a 360 px preview, and is saved
per character; Build SQL takes at least 330 px while it is shown. Everything reflows when the window is resized.

Every control has a tooltip: what it does, its key in grey, and for a disabled button a red line saying why. Column
headers explain their column (Source explains served, 3.3.5 and custom), drawer values say where they come from, and
the Build SQL options say what an empty field means. All the strings are in `Tips.lua`.

## The grid

- Every tab is a grid of columns. Items: icon, name (quality colour), item level, slot, type (armour or weapon
  subclass), quality, item id, display id, expansion, source, favourite. Creatures: name (rank colour: elite gold,
  rare silver, rare elite pale blue), creature id, display count (gold when above 1), display 1, type, rank, race of
  the NPC Extra row, family, kind (NPC or plain model), source, favourite. Sets: name, set id, source (transmog or item
  set), pieces, armour, classes, expansion, display 1, source. Models: name of the first user, model FileDataID, kind
  (creature or item model), users, display 1. Mounts: name, mount id, spell, type, display count, display 1, source.
- Source says "served" (the client draws the retail row), "custom" (your installed DBC has that display id, a row
  above the stock 3.3.5 ones: it wins) or "3.3.5" (a stock 3.3.5 row of that id wins).
- Click a header to sort, again to reverse. The sort runs over every match in the index, not the page shown (with a
  search text it replaces the text ranking).
- The row under the headers holds one filter cell per column, and every cell filters the whole index (RetailSearch),
  not the 50 rows shown. The name cell is the search box: a name (case and accents ignored; exact names first, then
  names that start with the text, then names that contain it) or a number (any id of the entry: creature or display,
  item or ItemDisplayInfo, set, model FileDataID, mount, display or spell). Id, item level and count cells take a
  range: `200-`, `-300`, `100-200`, `>=5`, `>5`, `<=5`, `<5` or `=5`; 0 is a value too (a red edge means the text is
not a range). Range cells start empty (no filter).
  Display cells take an exact display id (any of the entry's displays). The other cells open a choice menu (Any turns
  them off). Each active filter shows as a chip in the query row; click a chip to clear it.
- Slot Chest includes robes. Rank follows retail's Creature.Classification: 0 normal (5 trivial and 6 minus mobs too),
  1 elite, 2 rare elite, 4 rare; retail has no boss value, so there is no boss filter. A filter with an empty search
  box lists everything it passes; an empty box without filters lists nothing.
- Results come 50 a page (`[` / `]` or Prev / Next). Turning pages only slices the ranking the search made.
- Hover a row for 0.4 s to see it on a floating 3D preview.
- Favourites and History list saved entries of every kind (name, kind, id), filtered and sorted in Lua. The * cell on
  a kind's grid opens Favourites filtered to that kind.

## Keyboard

The grid owns the keys while it has focus: the status line says "Keys: grid". Click the grid (or the Keys button) to
give it the keys; the Keys button hands them back to the game. Letter keys are commands only while the grid has
focus, and any key the grid does not use goes into the search box, so no press is lost.

| Key | Does |
|---|---|
| Up / Down, PgUp / PgDn, Home / End | Move the focused row (the drawer shows it; the preview follows after 0.25 s) |
| Enter | Shows the focused row at once |
| Space, Shift+Up / Shift+Down, Ctrl+A | Check the row, extend the checked range, check the page |
| Left / Right, Alt+1 to Alt+4 | Pick the previous / next display (or appearance, piece, user), or one directly |
| `/`, Tab | Focus the search box (Tab in a cell goes to the next cell; Enter or Esc returns to the grid) |
| M, T | Morph me, Set target model (the picked display) |
| I, Y, G, F | Add item, Try on, copy the GM command, Favourite: on the checked rows when there are some, else the focused one |
| B | Build SQL (for the checked rows when there are some) |
| D, P, U, S, R | Drawer tabs: Details, Displays, Used by, Stats, Raw |
| E, Shift+E | Export CSV of the search, copy the checked rows as CSV |
| Ctrl+C, Ctrl+Shift+C | Copy the ids (checked rows, else the focused id), copy the rows as text (else all fields) |
| Alt+C | Copy the picked display id |
| `=` / `-` | Zoom the preview |
| Ctrl+1 to Ctrl+5, F8, F9 | Tabs: the five kinds, Favourites, History |
| Ctrl+Up / Ctrl+Down | Drawer height |
| Ctrl+Del | Clear history |
| Ctrl+Enter, Ctrl+S | Copy SQL, Export SQL |
| Esc | Clear the checked rows, else close |

## Bulk bar

Check rows with Space, Ctrl+click, right-click or the check box; Shift+click checks a range. Checked rows stay checked
across pages until the tab changes. The bar acts on all of them: Copy ids (comma-separated), Rows (the visible
columns as tab-separated text with a header), GM (each entry's command, one per line), Add (every `.additem`; more
than 10 lines ask for a second press, more than 50 refuse), Try on (wears them all on the preview), Fav (adds them,
or removes them when all are favourites), CSV (the rows as CSV, to the clipboard), Build SQL xN, Clear.

## Drawer

- Details: the ids in three groups (ids first, then the entry, then the picked display or, for items, the stats at
  the item's own level). Click a value to copy it; Shift+click adds it to a copy buffer and copies the buffer.
- Displays (Appearances for items, Pieces for sets): every display with its probability, model FileDataID and id,
  scale, NPC Extra row (race, sex, bakes) and source. A model lists every creature display drawn with it (the first
  200, also displays no creature or mount names), or its Users when no creature display draws it. Click one (or
  Left / Right, or the preview's `<` `>`) to pick it: the preview, Morph me, Set target, Copy display id, the GM command and
  Build SQL all use the picked one, which is labelled green. Before a pick it is the entry's own display (a model's
  first creature user's). A red band says when its installed DBC row wins over retail's.
- Used by (creatures, items, mounts, models): everything else that uses the picked display's model (first 200); click
  one to open it.
- Stats (items): the ItemSparse stats (type, budget share, value at the item's level), sockets, weapon fields,
  required level and level curve.
- Build SQL: see below. Raw: every field RetailDetails labels; click to copy.
- The preview fills the drawer's height. Its toolbar: `<` `>` pick the display, `<<` `>>` turn it, `+` `-` zoom, Fit
  (frames the display by its scale and the creature's type: dragonkin and giants start further back), Reset, the
  animation (Stand, Walk, Run, Ready, Talk, Dance; played on a clock with SetSequenceTime) and Copy (the display id). Drag to turn it, the wheel zooms.
  The caption names the display and its place, "Display 126134 (2/8)".
- Actions: Morph me (`.morph <display>`), Set target (`.npc set model <display>` on your target), Add item
  (`.additem <id>`, a set adds every piece; only items the server has), Try on, Copy id, Copy display id, Copy all,
  GM command, Favourite (kept in `WTF\...\SavedVariables\RetailBrowser.lua`), Build SQL.

## Colours

Every colour comes from one palette, `Theme.lua` (`RB.THEME`), after the retail UI: near-black panels with a cool
tint, darker wells for lists and inputs, the stock dialog and tooltip borders tinted bronze, gold titles and headers,
white text, grey labels, pale gold ids and numbers, the item quality colours (artifact pale gold, heirloom light blue),
green for served, orange for custom rows, red for errors. Rows stripe faintly, glow soft gold on hover, and the focused
row is gold with a 2 px gold edge. Buttons are the stock red-and-gold UIPanelButtonTemplate and tabs the gold-text
TabButtonTemplate. Inputs are dark wells with a dim bronze edge, pale gold while active and gold while focused.

## Build: world DB SQL

The Build SQL drawer tab writes TrinityCore 3.3.5 world DB SQL for the focused row, the checked rows or the whole
page (the scope buttons; at most 50 entries, each starting at the entry the previous one left free). It uses the
drawer's tallest step, so it stays inside the window. It never writes a row by
hand: every statement copies a stock row (the base) and overrides only the columns it names, listing every column of
the table so a different schema fails instead of shifting values. Each statement first deletes its new entry, so
running an export again replaces the rows it wrote:

```sql
-- retail item 263851 Breastplate of Voidwrath at item level 200, copied from item 39239
DELETE FROM `item_template` WHERE `entry` = 9000000;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, ..., `VerifiedBuild`)
SELECT 9000000, 4, 4, `SoundOverrideSubclass`, 'Breastplate of Voidwrath', 737435, 4, `Flags`, ..., `VerifiedBuild`
FROM `item_template` WHERE `entry` = 39239;
```

- **Item level** (empty: each item's own retail level, shown in grey; sets and creatures 200; 0 is allowed): the level
  the retail stats are computed at, and the item's `ItemLevel`.
- **Start entry**: the first new entry; a set's pieces take the next ones. Empty, the tab builds once to learn how
  many entries the scope takes, asks the server (lua_scripts `CloticScripts/RetailBrowser/RetailBrowser_FreeEntry.lua`,
  an addon whisper with prefix `RBFE`, GM accounts only) for the first run of that many free entries at 100000 or above,
  and builds again there: a lone item can fill a one-wide hole, a set takes the first hole as long as its pieces, a
  mixed scope one free in both `item_template` and `creature_template`. Copy SQL and Export SQL reserve the run on the
  server until it restarts, so the next build skips SQL not applied yet. With no answer in 2 seconds it falls back to
  9000000 and says so. A typed start is used as is.
  Creature entries must stay below 16777216 (the GUID keeps 24 bits of the entry).
- **Base entry**: the stock row to copy. Blank uses a suggestion per class, subclass and slot (an epic of item level 200
  to 245 without spells, set or sockets, taken from the world DB; `SuggestedBaseItem` in `BuildSql.cpp`), or creature
  15166 (a plain level 60 humanoid, friendly to all, no AI). A set always uses the suggestion per piece.
- **Required level** (blank keeps the base's), **Name** (blank: the retail name), **Subname** (blank: the retail
  title) and **Scale** (creatures, blank keeps the base's). An empty override field shows the retail value it stands
  for in grey; what you type replaces it in the SQL, and an empty field uses the retail value.
- **Hybrid primary**: what "Strength or Intellect" and the other hybrid stats become (Any: the first named).
- **Retail stamina multiplier** (off): retail multiplies stamina by StaminaMultByILvl (x9.49 at 200, so a plate chest
  gets 824 stamina instead of 135); leave it off for 3.3.5-sized stamina, turn it on for retail's exact values.
- **Retail armour/damage** (on): writes retail armour (`armor`), or a weapon's damage, school and speed (`dmg_min1`,
  `dmg_max1`, `dmg_type1`, `delay`; the second damage cleared). Off keeps the base's. Retail values are retail-sized: at
  item level 200 a plate chest has 181 armour (stock 39239: 2241), a shield 529 (40266: 7722) and a two-hand axe
  95 - 198 at 3.6 s (37852: 507 - 761 at 3.4 s), so the mapping shows the base's beside each (read from its tooltip;
  "not known yet" until the client has the item, then Build again).
- **Every set variant**: a row per set piece instead of one per slot (a slot prefers the piece with stat allocations,
  since sets also list stat-less transmog copies).
- **Other displays as modelid2-4**: a creature's other displays after the picked one (else 0).

The mapping box lists every retail stat, its value, what it became and why (the first 4 KB of them, then how many
more). The SQL box shows the first row's statements (at most 4 KB) and a comment line saying how much more there is:
a multi-line edit box lays its text out in time that grows with the square of its length, and a whole set of every
variant (430 KB) froze the client for 40 seconds. Copy SQL copies all the SQL and Export SQL writes all of it to
`Logs\RetailBrowser\<kind>-<id>-<time>.sql`. Edit full SQL puts every statement in the box to edit before copying
(over 64 KB it asks for a second click, since the box may freeze the client); once the box holds everything, Copy
and Export take the box, edits included.

The tab rebuilds 0.3 seconds after the last change (an option, a field, a new focused row or picked display), so
stepping through results builds once; Build now builds at once. Option names (`itemLevel`, `startEntry`, `baseEntry`,
`requiredLevel`, `scale`, `combo`, `retailStamina`, `allVariants`, `otherDisplays`, `display`, `appearance`, `name`,
`subname`) are RetailBuildSql's.

What an item row overrides: `entry`, `class`, `subclass`, `name`, `displayid` (the served retail ItemDisplayInfo of
the first appearance), `Quality` (heirloom at most), `InventoryType`, `ItemLevel`, `RequiredLevel` when set,
`StatsCount`, `stat_type1..10` / `stat_value1..10`, `ScalingStatDistribution` / `ScalingStatValue` (0), the six
resistance columns, `socketColor_1..3` and `socketBonus` (0), and with Retail armour/damage `armor`, or `dmg_min1`,
`dmg_max1`, `dmg_type1`, `dmg_min2` / `dmg_max2` / `dmg_type2` (0) and `delay`. Bonding, flags, spells, block and price
stay the base's. A creature row overrides `entry`, `difficulty_entry_1..3` (0), `modelid1..4`, `name`, `subname` and
`scale` when set; level, faction, flags, AI and loot stay the base's. `creature_model_info` already has rows for the
retail displays. Sets write item rows only: no `ItemSet.dbc` row or set bonus yet.

New `item_template` and `creature_template` rows load when the worldserver starts (TrinityCore 3.3.5 has no reload for
new entries), so restart it after applying the SQL.

### Retail stat values

ItemSparse stores allocations, not values. They are computed the way the retail client does (TrinityCore master
`Item::GetItemStatValue` and `GetRandomPropertyPoints`, with the pinned build's own tables):

```
points     = RandPropPoints[itemLevel].{Good,Superior,Epic}F[cell]
budget     = StatPercentEditor * points / 10000
budget    -= floor(StatPercentageOfSocket * ItemSocketCostPerLevel[max(itemLevel, the item's own level)] + 0.5)
value      = floor(budget * multiplier + 0.5)        resistances: floor(budget)
           a stat its sockets take to 0 or less is left out
multiplier = StaminaMultByILvl for stamina, CombatRatingsMultByILvl for ratings, else 1
```

The cell is the slot's: 0 head, chest, legs, two-hand and ranged; 1 shoulders, waist, feet, hands and trinkets; 2 neck,
wrists, fingers, back, shields and off-hand items; 3 one-hand weapons and wands; 4 relics. The multiplier column is the
slot's too: weapon for weapons, shields and off-hand items, trinket, jewellery for neck and finger, else armour.
Uncommon items read Good, rare and heirloom Superior, epic to artifact Epic; poor and common items get no stats.

Tables: RandPropPoints (1310245), `gametables\combatratingsmultbyilvl.txt` (1391670), `staminamultbyilvl.txt`
(1980632) and `itemsocketcostperlevel.txt` (1391643). Checked against wowhead's tooltips at the item level wowhead
shows: 14 items without sockets match on every stat (legacy and Midnight armour, weapons, cloaks, rings and necks, item
levels 13 to 684; `Tools/Tests/StatBudgetTests.cpp`). Sockets: TrinityCore truncates the socket share
(`int(share * cost)`), which left socketed items 1 or 2 points high; retail rounds it, and charges the socket cost of
the item's own level when the item is shown below it (wowhead shows Gladiator's Scaled Helm, item level 37, at 15, where
the table says 0: it still pays the 2 points of 37). With both, 201 of 206 socketed stats of 93 random socketed items
match wowhead; the 5 left are a heroic variant (bonus ids) and one tooltip that adds two allocations of one stat.

### Retail armour and weapon damage

As TrinityCore master's `ItemTemplate::GetArmor`, `GetDPS` and `GetDamage` read the pinned build's tables
(`ItemArmour.cpp`):

```
armour = int(ItemArmorQuality[ilvl].Qualitymod[q] * ItemArmorTotal[ilvl].<type> * ArmorLocation[slot].<type> + 0.5)
shield = int(ItemArmorShield[ilvl].Quality[q] + 0.5)
dps    = ItemDamage<table>[ilvl].Quality[q]
avg    = dps * ItemDelay / 1000
min    = floor(avg * (1 - DmgVariance / 2)), at least 1        max = floor(avg * (1 + DmgVariance / 2) + 0.5)
```

q is the quality (heirlooms read rare; past artifact none), the type cloth, leather, mail or plate (subclass 1 to 4), a
robe reads the chest's ArmorLocation row. The damage table: ammo ItemDamageAmmo; two-hand ItemDamageTwoHand; bows,
guns and crossbows ItemDamageTwoHand; wands ItemDamageOneHandCaster; one-hand, main and off hand ItemDamageOneHand; an
item with ITEM_FLAG2_CASTER_WEAPON (Flags[1] 0x200) reads the caster table of its kind. Tables: ItemArmorQuality
(1283021; one row, 0.9 poor, 0.95 common, else 1, copied to every level), ItemArmorTotal (1283022), ArmorLocation
(1284818), ItemArmorShield (1277741), ItemDamageAmmo (1277740), ItemDamageOneHand (1277743), ItemDamageOneHandCaster
(1277739), ItemDamageTwoHand (1277738), ItemDamageTwoHandCaster (1277742). Checked against wowhead at the item level
wowhead shows: 20 armour pieces and shields (cloth to plate, every slot kind, robes, item levels 1 to 321) and 23
weapons (one- and two-hand, caster staves and daggers, bows, guns, crossbows, wands, heirlooms) all match
(`Tools/Tests/ItemArmourTests.cpp`); wowhead's dps is the tooltip's (min + max) / 2 / speed, not the table's.

### Items that scale with the player's level

Items with a ContentTuningID (about 8000: quest rewards and dungeon loot that follow the player through a level range)
or a PlayerLevelToItemLevelCurveID (heirlooms) have no fixed level in retail. Their details say so ("Scales"), and the
Build panel notes that their stats, armour and damage are computed at its item level instead.

### Stat mapping

| Retail | 3.3.5 |
|---|---|
| Stats both have: mana, health, primaries, stamina, ratings 12 to 37, attack power, spell power, regeneration, spell penetration, block value | Same type and value |
| Versatility (40) | Stamina, 1:1 (40 is feral attack power on 3.3.5) |
| Agility or Strength or Intellect (71), Agility or Strength (72), Agility or Intellect (73), Strength or Intellect (74) | One primary: the first named, or the chosen hybrid primary when it is one of them |
| Fire, frost, holy, shadow, nature, arcane resistance (51 to 56) | `fire_res` and the other resistance columns, at most 255 |
| Mastery, bonus armour, PvP power, multistrike, readiness, speed, leech, avoidance, indestructible, cleave, corruption, anything else | Dropped |

Equal 3.3.5 types are summed (an item's two intellect allocations, versatility into stamina); at most 10 stat slots,
values up to 32767 (`stat_value` is a smallint). Retail socket types map to colours: 1 meta, 2 red, 3 yellow, 4 blue,
7 prismatic (red, yellow and blue); the retail-only kinds (cogwheel, domination...) are left empty.

## Lua functions (for other addons)

`RetailSearch(kind, text [, filters [, page]])`, `RetailDetails(kind, id)`, `RetailBrowserInfo()`,
`RetailExportCsv(kind, text [, filters])`, `RetailBuildSql(kind, id [, options])` (the Build panel's options,
`MarshalBuild` in `BrowserMarshal.h`), `RetailExportSql(name, sql)`, `CopyToClipboard(text)`, `GetClipboardText()`, and the model methods
`model:SetDisplayInfo(displayId)` and `dressUpModel:TryOnDisplay(displayId, inventoryType [, sheathe, class, subclass])`.
On a `PlayerModel`, `SetDisplayInfo` of a customizable creature (the dragonriding drakes) draws its customization look
as a mount does; NPCs on an installed body need a `DressUpModel` (RETAIL-ASSETS.md, Customizable creatures).
Kinds are `creature`, `item`, `set`, `model`, `mount`. A bad kind or id returns `nil, error`; before the index is
ready a search returns `{ status = "building" }`.

Search filters (numbers; unknown names are ignored): `inventoryType`, `armour`, `weaponSubclass`, `quality`,
`minItemLevel`, `maxItemLevel`, `expansion`, `creatureType`, `family`, `classification`, `race`, `humanoid`, `classId`,
and for every kind `minId` / `maxId` (a set's own TransmogSet or ItemSet id, a model's FileDataID), `displayId` (any of
its displays, appearances, pieces or users), `minCount` / `maxCount` (how many of those), `installed` (1: one has an
installed DBC row, 0: none), `setSource` (0 transmog, 1 item set), `modelKind` (1 creature, 0 item), `mountType`.
`sort` (0 name, 1 id, 2 item level, 3 quality, 4 slot, 5 subclass or armour, 6 expansion, 7 first display, 8 count,
9 type, 10 rank, 11 race, 12 family, 13 kind, 14 installed) with `descending = 1` orders every match by that column
instead of the text ranking (`SortColumn` in `RetailIndex.h`). A focused or hovered row calls
`RetailPrefetch("display", id)` with the display its actions would use, so its preview is prepared on a worker; a page
calls `RetailPrefetchIcons(list)` with its icons, and the grid leaves an icon blank while `RetailIconPending(fdid)`, setting
it once it is read. The addon calls them only when a client has them.

## Known limits

- Items that scale with the player's level (a level curve) show their stored item level; the Build panel's item level
  is what counts.
- The mount Spell column has no filter or sort (the search box finds spell ids). Rows are fetched with RetailDetails
  per page row (cached) for the grid's columns.
- Item names, quality and item level come from ItemSearchName, which lists the 104k searchable items, not every item.
- Rows still encrypted in the build (unreleased content) are skipped; `Logs/RetailAssets.log` says how many.
