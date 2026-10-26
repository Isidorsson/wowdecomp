# Retail Browser

An in-game search over the retail tables of the pinned build, like wago.tools inside the 3.3.5 client. It needs a
client built with `WOWDECOMP_RETAIL_ASSETS=ON` and a working `Data/Retail/RetailCasc.ini`; on any other client the
addon only prints that retail assets are not configured.

## Opening it

- `/rb` (or `/retailbrowser`) toggles the window. A key can be bound under Key Bindings, "Retail Browser".
- The index is built on a worker thread at login ("Build the index at login", next to the window title, on by
  default; it keeps about 120 MB for the session), so the first open is ready at once. With the option off, the first
  open builds it (about 1 second on a local install; the status line says "Building the index..." until then). It
  stays in memory for the session, `/reload` included.
- Escape closes the window.

## Searching

- Tabs: Creatures, Items, Sets, Models, Mounts. Favourites and History list saved entries of every kind.
- Type a name: case and accents are ignored. Exact names come first, then names that start with the text, then names
  that contain it. Results come 50 a page (Prev / Next).
- Type a number to search ids: creature id or display id, item id or ItemDisplayInfo id, set id, model FileDataID,
  mount id, display id or spell id.
- Filters (Any = off): Creatures by type, family, rank (normal, elite, rare elite, rare), race of the NPC's Extra row, and
  kind (humanoid NPC or plain model). Items by slot (Chest includes robes), armour, weapon type, quality, expansion
  and an item level range. Sets by class, armour and expansion. A filter with an empty search box lists everything
  it passes; an empty box without filters lists nothing.
- Rank follows retail's Creature.Classification: 0 normal (5 trivial and 6 minus mobs too), 1 elite, 2 rare elite,
  4 rare. Retail has no boss value (no row uses 3, the old world boss value; bosses such as Ragnaros are elite), so
  there is no boss filter.
- Turning pages of a search only slices the ranking the search made, so Prev / Next are instant.
- Click a column header to sort the page by name or id (click again to reverse).
- Keyboard: press Enter in the search box (or click a row) to give the list the keys: Up/Down and PgUp/PgDn move
  and show details, Enter previews, Esc or any other key hands the keys back.

## Details and preview

- The right pane lists every id of the entry: creature id, each display with its probability, model FileDataID,
  scale and NPC Extra row (race, sex, bakes); item id, quality, item level, slot, class/subclass, icon, and per
  appearance the ItemDisplayInfo id, appearance and ItemModifiedAppearance ids; set pieces with item and display ids;
  what uses a model, and every creature display drawn with the model (the first 200), also displays no creature or
  mount names.
- Items also list their ItemSparse stats: each stat type with its budget share (StatPercentEditor / 100) and its value
  at the item's own level, sockets (retail SocketType numbers and the socket bonus enchantment), required level,
  weapon delay, damage variance and damage type as stored, retail armour or weapon damage at the item's own level, and
  whether the item scales with the player's level (its ContentTuning or level curve).
- Each display says "served" (the client draws the retail row) or "custom row wins" (the installed DBC has that id,
  so the client draws your row instead).
- Click any field to copy it. Clicking a display's line (its model and NPC lines too, or a model's creature user) also
  picks that display for the preview, Morph me, Set target model, Copy id, Copy GM command and Build SQL. The lines of
  the display they use are labelled green; before a pick that is the entry's first display.
- With more than one display (a creature's up to four, a mount's MountXDisplay rows, a model's displays) the arrows
  under the preview step through them; the caption shows the display and its place, "Display 126134 (2/8)".
- The preview shows creatures and mounts by display (humanoid NPCs dressed and baked), and items and sets on your
  character. Drag to turn it, use the mouse wheel to move it nearer or further.

## Buttons

| Button | Does |
|---|---|
| Morph me | `.morph <display>` |
| Set target model | `.npc set model <display>` on your target |
| Add item | `.additem <id>` (a set adds every piece). Works only for items that exist on the server |
| Try whole set | Wears one piece per slot of the set in the preview |
| Copy id / Copy all / Copy GM command | Copies the id (the picked display once one is picked: "Copy display id"), every field as plain text, or the command above |
| Favourite | Saves the entry (kept in `WTF\...\SavedVariables\RetailBrowser.lua`) |
| Export CSV | Writes every match of the current search to `Logs\RetailBrowser\<kind>-<time>.csv` |
| Build SQL | Opens the Build panel (items, sets and creatures) |

## Build: world DB SQL

The Build panel writes TrinityCore 3.3.5 world DB SQL for the shown item, set or creature. It never writes a row by
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

- **Item level** (default 200): the level the retail stats are computed at, and the item's `ItemLevel`.
- **Start entry** (default 9000000): the first new entry; a set's pieces take the next ones. The client cannot see
  which entries are free, so pick a range you know is empty (the status line says the next free one after this build).
  Creature entries must stay below 16777216 (the GUID keeps 24 bits of the entry).
- **Base entry**: the stock row to copy. Blank uses a suggestion per class, subclass and slot (an epic of item level 200
  to 245 without spells, set or sockets, taken from the world DB; `SuggestedBaseItem` in `BuildSql.cpp`), or creature
  15166 (a plain level 60 humanoid, friendly to all, no AI). A set always uses the suggestion per piece.
- **Required level** (blank keeps the base's), **Name** (blank: the retail name), **Subname** and **Scale** (creatures).
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

The panel rebuilds 0.3 seconds after the last change (an option, a field, a new entry in the details), so stepping
through results builds once; Build or Enter builds at once.

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
Kinds are `creature`, `item`, `set`, `model`, `mount`. A bad kind or id returns `nil, error`; before the index is
ready a search returns `{ status = "building" }`.

## Known limits

- Items that scale with the player's level (a ContentTuning or level curve) show their stored item level; the Build
  panel's item level is what counts. Their level range (ContentTuning) is not read.
- Retail armour and damage are retail-sized, far below 3.3.5's at the same item level; shield block value stays the
  base's.
- Item names, quality and item level come from ItemSearchName, which lists the 104k searchable items, not every item.
- Rows still encrypted in the build (unreleased content) are skipped; `Logs/RetailAssets.log` says how many.
