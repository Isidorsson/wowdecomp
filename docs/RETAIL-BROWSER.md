# Retail Browser

An in-game search over the retail tables of the pinned build, like wago.tools inside the 3.3.5 client. It needs a
client built with `WOWDECOMP_RETAIL_ASSETS=ON` and a working `Data/Retail/RetailCasc.ini`; on any other client the
addon only prints that retail assets are not configured.

## Opening it

- `/rb` (or `/retailbrowser`) toggles the window. A key can be bound under Key Bindings, "Retail Browser".
- The first open builds the index on a worker thread (about 2 to 4 seconds on a local install; the status line says
  "Building the index..." until then). It stays in memory for the session, `/reload` included.
- Escape closes the window.

## Searching

- Tabs: Creatures, Items, Sets, Models, Mounts. Favourites and History list saved entries of every kind.
- Type a name: case and accents are ignored. Exact names come first, then names that start with the text, then names
  that contain it. Results come 50 a page (Prev / Next).
- Type a number to search ids: creature id or display id, item id or ItemDisplayInfo id, set id, model FileDataID,
  mount id, display id or spell id.
- Filters (Any = off): Creatures by type, family, rank (normal, elite, rare, boss), race of the NPC's Extra row, and
  kind (humanoid NPC or plain model). Items by slot (Chest includes robes), armour, weapon type, quality, expansion
  and an item level range. Sets by class, armour and expansion. A filter with an empty search box lists everything
  it passes; an empty box without filters lists nothing.
- Click a column header to sort the page by name or id (click again to reverse).
- Keyboard: press Enter in the search box (or click a row) to give the list the keys: Up/Down and PgUp/PgDn move
  and show details, Enter previews, Esc or any other key hands the keys back.

## Details and preview

- The right pane lists every id of the entry: creature id, each display with its probability, model FileDataID,
  scale and NPC Extra row (race, sex, bakes); item id, quality, item level, slot, class/subclass, icon, and per
  appearance the ItemDisplayInfo id, appearance and ItemModifiedAppearance ids; set pieces with item and display ids;
  what uses a model.
- Each display says "served" (the client draws the retail row) or "custom row wins" (the installed DBC has that id,
  so the client draws your row instead).
- Click any field to copy it. Clicking a display line also picks that display for the preview and the commands.
- The preview shows creatures and mounts by display (humanoid NPCs dressed and baked), and items and sets on your
  character. Drag to turn it, use the mouse wheel to move it nearer or further.

## Buttons

| Button | Does |
|---|---|
| Morph me | `.morph <display>` |
| Set target model | `.npc set model <display>` on your target |
| Add item | `.additem <id>` (a set adds every piece). Works only for items that exist on the server |
| Try whole set | Wears one piece per slot of the set in the preview |
| Copy id / Copy all / Copy GM command | Copies the id, every field as plain text, or the command above |
| Favourite | Saves the entry (kept in `WTF\...\SavedVariables\RetailBrowser.lua`) |
| Export CSV | Writes every match of the current search to `Logs\RetailBrowser\<kind>-<time>.csv` |

## Lua functions (for other addons)

`RetailSearch(kind, text [, filters [, page]])`, `RetailDetails(kind, id)`, `RetailBrowserInfo()`,
`RetailExportCsv(kind, text [, filters])`, `CopyToClipboard(text)`, `GetClipboardText()`, and the model methods
`model:SetDisplayInfo(displayId)` and `dressUpModel:TryOnDisplay(displayId, inventoryType [, sheathe, class, subclass])`.
Kinds are `creature`, `item`, `set`, `model`, `mount`. A bad kind or id returns `nil, error`; before the index is
ready a search returns `{ status = "building" }`.

## Known limits

- Retail item stats are not shown yet: they live in ItemSparse, a sparse table the client's db2 reader does not read.
- Item names, quality and item level come from ItemSearchName, which lists the 104k searchable items, not every item.
- Rows still encrypted in the build (unreleased content) are skipped; `Logs/RetailAssets.log` says how many.
