# Retail assets

The `dev` branch of this fork can draw retail World of Warcraft creatures, mounts and gear in the 3.3.5a client. It
reads them straight from a retail CASC storage, either a local retail install or Blizzard's CDN, and converts them
in memory into files the stock 3.3.5 engine accepts. It has been tested against retail 12.x (Midnight) builds, for
example 12.1.5.70529.

Nothing here is part of the original client. It is all behind a CMake option that is off by default, and stock data
always wins: a retail row or file is only used where the 3.3.5 client has none.

## Enabling it

Configure with the option on. CascLib 3.0 (MIT) is downloaded at configure time, pinned by hash, and patched to fetch
single files by HTTP range (`cmake/casclib-range-fetch.patch`).

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64 -DWOWDECOMP_RETAIL_ASSETS=ON
cmake --build build/x64 --config Release --target WowClient --parallel
```

| CMake option | Default | Effect |
|---|---|---|
| `WOWDECOMP_RETAIL_ASSETS` | `OFF` | Build the CASC source, the converter and the retail tools into the client |
| `WOWDECOMP_RETAIL_SOURCE_HASH` | `ON` | Key the converted cache by a hash of the converter's sources too, so an edited converter never serves a stale entry |
| `WOWDECOMP_CASCLIB_SOURCE_DIR` | empty | Build from an existing CascLib 3.0 checkout instead of downloading it (it must already carry the range patch) |

Then create `Data/Retail/RetailCasc.ini` beside the client, or point the `WOW_RETAIL_CONFIG` environment variable at
an ini anywhere. Without a config file the client handles no retail paths at all. A broken one is reported in
`Logs/RetailAssets.log`, which also collects every conversion warning.

The ini is `key=value` lines; `#` starts a comment. Relative paths resolve against the ini's folder.

| Key | Default | Meaning |
|---|---|---|
| `mode` | `online` | `local` reads a retail install, `online` reads Blizzard's CDN |
| `path` | required | Local: the retail install folder (the one holding `.build.info`). Online: the folder CascLib keeps its CDN downloads in |
| `build` | empty | Build config key, 32 hex digits. Required online, so a CDN update never changes what you see. Local mode reads the active build from `.build.info` instead |
| `product` | `wow` | CASC product to open |
| `region` | `us` | CDN region (online mode) |
| `locale` | `2` | CASC locale mask in hex (`2` is enUS) |
| `cache` | `Cache` beside the ini | Where the decoded and converted caches live. Never inside a local install |
| `decoded_cache_mb` | `4096` | Cap of the decoded cache in MB, `0` for no cap |
| `converted_cache_mb` | `2048` | Cap of the converted cache in MB, `0` for no cap |
| `aliases` | empty | Optional file of `FDID;client\path` rows that serve a retail file in place of a stock path |

A local install:

```ini
mode=local
path=C:\Program Files (x86)\World of Warcraft
```

Blizzard's CDN:

```ini
mode=online
path=CdnCache
build=0123456789abcdef0123456789abcdef
```

**Local or online.** Local mode needs a retail install but no network, and a Battle.net patch moves it to the new
build (and a fresh cache) on its own. Online mode needs no install, but opening the storage downloads about 300 MB of
build metadata once, and Blizzard removes old builds from its CDN after a few weeks, so a pinned build stops working
for new caches. Online reads fetch only the bytes of each file (a 46 KB model used to cost a whole 256 MB archive; the
range patch took a test run from 945 MB to 10 MB of downloads).

## What it serves

The client claims two kinds of path before the MPQs are searched:

- `retail/<FileDataID>.<ext>` directly under `retail/`, and `retail-<FileDataID>` file names in any folder (the form
  served display rows use, since the client puts a stock folder in front of item names).
- Stock paths listed in the `aliases` file.

Named paths under `Retail\` (for example downported content in a patch MPQ) stay with the MPQs.

### Creatures and mounts

`CreatureDisplayInfo` and `CreatureModelData` fall back to the retail tables of the pinned build when the stock DBC has
no row. So any retail CreatureDisplayInfo id works anywhere the client takes a display: `.morph <id>`, a creature
whose server row uses it (TrinityCore's `creature_template` model id, `modelid1` on older schemas), mounts, forms and
model frames. Served models take synthetic model ids from `0x01000000` upward, default skins are named per model, and
geoset variants are packed the way the 3.3.5 row expects. Retail fields the client has no tables for (sounds, blood,
footprints, particle colours, extended display) are left 0.

- Model frames (the Retail Browser preview, `SetCreature`) apply the display's geosets as the unit does. They drew
  every geoset before, so a mount showed all its armour variants at once and a banner its display hides, untextured.
- Retail's fourth texture variation fills model texture type 5, which the 3.3.5 row has no field for; it is served
  beside the row and replaced with the other three (`ReplaceMonsterSkin`). 953 retail displays have one.

### Items

`ItemDisplayInfo` falls back the same way, so retail item display ids show without a downported DBC:

- Weapons, shields, held items, shoulders and cloaks, with their object skins and extra layers (glow, collection
  slots).
- Armour body textures for each body section, by gender. The texture compositor now decodes DXT and raw textures and
  fits any size to its section.
- Helms by race and gender. The client's race-named helm request is answered with that race's file, never another
  race's; a race without a file shows the error cube. Retail `HelmetGeosetData` hides hair, beards, sideburns,
  moustaches, ears, facial jewellery and eye effects per race. Retail helms are made for the HD heads, so each helm is
  moved (and for tauren and gnomes scaled) onto the installed head: read off the body when it is an HD port with the
  retail skeleton, else from a measured per-race table.
- Icons from retail `ItemAppearance`. A retail display with no icon borrows one from a display with the same models,
  and a stock row with an empty icon takes the retail icon of its id.

### Humanoid NPCs

Retail displays that name a `CreatureDisplayInfoExtra` row (player-race NPCs) used to draw as the missing-model cube,
because their model is a retail `*_HD.m2` the converter refuses. The client now draws them the way it draws a stock
3.3.5 NPC, with no server change:

- The served display points at the installed body of the NPC's race and gender (stock `CreatureModelData` ids) and at a
  served Extra row. Served Extra ids are `1,000,000 + the retail Extra id`.
- The served Extra row carries race, sex, hair style, hair colour, facial hair, the 11 worn items and a baked skin name
  under `Textures\BakedNpcTextures\`. The character component replaces the skin with the bake, sets the hair and
  facial hair geosets and attaches the items, as for a stock NPC.
- Races 1 to 8, 10 and 11 use the installed HD-port bodies. The HD bake (retail's 2:1 layout) is warped onto the
  installed body's classic square layout, 512x512. Each installed skin vertex is matched to the retail vertex at the
  same position, and each installed triangle resamples the bake under the retail UVs of its matched vertices. The
  warp is used only when at least 60% of the body's skin vertices match (the log says which bodies pass). Tauren and
  Gnome are scaled first, by the helm fit table's scale. Eyes keep the installed base skin's eyes.
- Races 12 to 21 (Fel Orc, Naga, Broken, Skeleton, Vrykul, Tuskarr, Forest Troll, Taunka, Northrend Skeleton, Ice
  Troll) use their stock bodies with the SD bake as is. So do the few SD-only displays of the other races.
- Goblin (9) and races 22 and up (Worgen, Pandaren, the allied races, Dracthyr, Earthen, ...) draw on the display's own
  retail model, through the same character component. Their bakes are in that model's 2:1 layout and are used as is.
  When the display's model is the retail body an installed HD port was made from (Mag'har on `OrcMale_HD`), the NPC is
  drawn as that race instead (the installed orc, warped bake). The details:
  - The model is served under its own name, `Character\retail-<fdid>_npcbody.m2`, which converts with its character
    textures. The same file asked for as a plain model is still refused as a player race model.
  - Races the installed `ChrRaces.dbc` lacks get a served row from the retail one: the retail id and ClientPrefix,
    flag 0x1 (hidden from character creation), and the race's body displays. Installed rows win. The client's
    race-sized tables (CharSections variations, facial hair counts) are sized for the highest served race; a race
    past them is refused with a log line instead of read out of bounds.
  - A baked NPC of a served race needs no CharSections rows. Its geosets come from its customization choices: per
    geoset group of the model, the chosen mesh, else x01 (else the group's lowest mesh). Choice id 0 shows no mesh,
    and the eye glow group (17xx, an additive glow on retail bodies) shows only when chosen.
  - Textures other than the skin (hair 6, facial hair 7, Worgen fur 8, eyes 19, jewelry 20, ...) come from the chosen
    materials, placed by the model's `ChrModelTextureLayer` rows; facial hair without a choice takes the hair's file.
    The bake name is `retail-<fdid>_npc<extra>.blp`, by which the client finds the row's look.
  - Helms: masks cover races 1 to 31; for races 32 and up the served helm's own retail rows decide what it hides.
    Served races ask for helm files by their retail ClientPrefix, and retail helms hang unmoved on retail bodies
    (goblins included). A helm with no file for the race (retail ships none for some Earthen helms) is left off.
- Hair style is matched by geoset id (Tauren horns one above). Facial hair takes the installed combination that matches
  beard, moustache and sideburns, else the closest. Hair colour is the installed colour index and texture. A style the
  installed body lacks draws style 0 and logs a line.
- The 11 item slots come from `NPCModelItemSlotDisplayInfo`. An item display that is neither stock nor served leaves
  its slot empty and logs a line.
- Warped bakes are cached in the converted cache (`converted/bake-<bake FileDataID>-<race>-<gender>`), so the warp
  runs once.
- The NPC tables are encrypted for unreleased content. Their still-locked rows are skipped, with one log line per
  table.

Log lines in `Logs/RetailAssets.log` show what was decided:

```
Character\Human\Male\HumanMale.m2: 97.0% of its skin vertices lie on retail 1011653, so HD bakes are warped onto it (10578 triangles)
race 1 sex 0: hair style geoset 25 has no installed style, style 0 drawn
Character\Orc\Male\OrcMale.m2: 81.0% of its skin vertices lie on retail 917116, so HD bakes are warped onto it (10128 triangles)
extra 156199: helm display 672219 has no file for race 84 male, slot left empty
table 1340661: skipped 8 still-locked rows
```

### Server rows: `Tools/RetailServerDisplays`

The client can show a retail creature display, but a TrinityCore 3.3.5 worldserver drops a `creature_template` model it
cannot find in its own `CreatureDisplayInfo.dbc`, and refuses a spawn without a `creature_model_info` row.
`RetailServerDisplays` (built with the option on) writes what the server needs:

```powershell
RetailServerDisplays <RetailCasc.ini> <server dbc dir> <out dir> [--exclude-client <client CreatureDisplayInfo.dbc>] <id>...
RetailServerDisplays <RetailCasc.ini> <server dbc dir> <out dir> --ids <file>
RetailServerDisplays <RetailCasc.ini> <server dbc dir> <out dir> --all-missing
```

It writes copies of the server's `CreatureDisplayInfo.dbc` and `CreatureModelData.dbc` with a row per retail display
(its retail scale) and one placeholder model row per retail model (collision box, model scale, geobox, mount height),
plus `creature_model_info.sql`. Ids the server already has are refused, `--exclude-client` skips ids the client's own
DBC has, and `--all-missing` covers every retail display of the build. Copy the two DBCs into the worldserver's `dbc`
folder and run the SQL on the world database.

`Tools/RetailAssetProbe` reads paths through the same source, to check a file, prefetch it before playing, print the
animation fallback table (`--animation-fallbacks`) or save what the client would read (`--save <dir>`). For
investigating displays it prints the served rows (`--displays <id>...`: model, variations, packed geosets and the
model's geosets), the displays of a CreatureModelData id (`--model-displays <id>`) and every column of DB2 rows
(`--rows <table FileDataID> <id>... | all`).

### Conversion

Retail models (MD21, versions 272 to 274) become MD20 version 264. Among other things the converter:

- inlines `.anim` files and remaps animation ids past 505 through retail `AnimationData` fallbacks
- merges shared skeletons (`.skel` files and their parent chain) into the model
- converts particle emitters and cameras, and masks flags to what 3.3.5 knows
- cuts each ribbon emitter's textures to the ones it has a material for: retail ribbons list more textures than
  materials (one shader combines them), and the client draws a pass per texture with the material of the same slot,
  so the Voidwing Dragonhawk's ribbons drew their flat grey 8x8 helper texture as large grey quads
- shades batches through a fixed combiner table, maps the 36 retail shader effects to 3.3.5 ones, draws extra effect
  textures as overlay passes, and copies UV1 into UV2 where a batch samples UV1 twice
- splits submeshes wider than their view's bone palette into pieces
- lays out large skins (past 65,535 indices) so the 3.3.5 index buffer can hold them, keeps the views and LOD skins
  that fit (at most four), and as a last resort decimates the smaller submeshes by edge collapse

On a survey of 1,376 retail creature models, 5 skins are still refused, all of them a multi-texture shader case.

## Caches and performance

Both caches live in `<cache>/<product>-<build key>/` (`<product>-local` without a key), so each build has its own:

| Folder | Holds | Cap |
|---|---|---|
| `decoded/` | Original retail bytes, checksummed, so a warm read never opens the storage | `decoded_cache_mb` |
| `converted/` | Each converted model with all of its skins, compressed in 1 MB XPRESS Huffman blocks | `converted_cache_mb` |
| `sequences/` | Per model CSV of remapped animation ids, for reference | none |

- Entries are written to a temporary file and renamed, so a crash never leaves half an entry.
- Past a cap, the least recently used entries are trimmed to 90% of it on a worker thread. A hit counts as a use.
- Converted entries are keyed by the converter version (and the source hash); an entry of another key is deleted on
  read. Caches of the product's other builds are deleted at startup.
- Compression halved the converted cache: 1,376 models went from 6.6 GB to 3.3 GB with identical output.
- The display tables, animation fallbacks and storage open on worker threads at startup, instead of costing about
  1.5 s and 3 s on the main thread at first use.
- Load-progress cache checks read only an entry's header: 0.02 ms instead of 61 ms for a 36 MB model.
- A model and its first skin from a warm converted cache: 61 to 2 ms, 356 to 16 ms and 937 to 5 ms for three sample
  models (before, each read converted again).

A cold read still blocks the game while the file is fetched and converted. Use `RetailAssetProbe` to warm the cache
first.

## Limits and not yet

- NPCs with no bake (about 30 rows) keep their retail model and log one line: the missing-model cube for models on a
  skeleton, an untextured (white) model for the rest.
- Retail bodies are large (Goblin male 216,000 vertices); the converter keeps view 0 and collapses triangles to fit
  the 3.3.5 skin limits, so LOD skins are dropped and some detail is lost.
- Customization layers that need runtime composition (tattoos, scars, face paint drawn into the skin) are not drawn;
  the bake already carries most of them. Texture slots without a chosen material stay white.
- Some retail race helms have no file for every race (no borrowing from another race), so those NPCs go bareheaded.
- Model frames (`PlayerModel:SetCamera`) frame some retail bodies (Kul Tiran, Earthen) too close.
- Hair styles and facial hair the installed bodies lack (about 10% of NPC rows) fall back to style 0 or the closest
  combination. Hair texture comes from the installed CharSections rows, not the retail Hair Color material.
- An item geoset the body lacks (retail boots name 506 to 510, the installed bodies stop at 505) draws the group's
  default mesh instead, the bare shin for boots, with one log line per model and mesh.
- Warped bakes can show thin slivers right at the hairline, where very thin installed triangles do not line up exactly
  with the bake's hairline.
- Game objects, spell visuals and hood models are not served.
- Players need a retail install or the CDN. A server pack is planned for players without either: an offline tool that
  converts the display ids a server uses into patch MPQs plus matching client and server DBC rows, so players need no
  CASC, network or runtime conversion. It does not exist yet.
- Retail files belong to Blizzard. Hosting converted files for players is the server admin's decision.
