# wowdecomp

A C++ decompilation of the World of Warcraft 3.3.5a client (`Wow.exe`, build 12340, June 2010).

Join the discord for other clients and to show off some cool stuff you build.

https://discord.gg/DsZMGUD8aT

Every function is reconstructed from the original binary and keeps the address it came from in an
`// OFFSET:` tag, so each one can be compared against the original. The source tree follows the layout
of the original client (`Engine/Source/<Module>`, `WoW/Source/<Module>`, original file names).

x64 is the main target. Win32 still builds and is used to check struct layouts against the 32-bit
original.

## What this fork adds

This is the `dev` branch of [Isidorsson/wowdecomp](https://github.com/Isidorsson/wowdecomp), a fork of
[SatyPardus/wowdecomp](https://github.com/SatyPardus/wowdecomp). `main` tracks upstream; `dev` adds the changes below
on top of it. New features are behind CMake options that are off by default, so a default build behaves like
upstream plus the bug fixes and `fastExit`.

### Retail assets

With `-DWOWDECOMP_RETAIL_ASSETS=ON` and a `Data/Retail/RetailCasc.ini`, the client draws retail (including Midnight)
creatures, mounts and gear straight from a retail install or Blizzard's CDN, converted in memory to 3.3.5 models:

- Any retail CreatureDisplayInfo id works for `.morph`, creature spawns and mounts, without a downported DBC.
- Retail ItemDisplayInfo ids show weapons, armour textures, shoulders, cloaks, icons, and helms by race with hair
  hiding and a fit to the 3.3.5 heads.
- Retail humanoid NPCs (player-race displays with extended display info) draw on the installed bodies with their
  retail skin, hair and gear; Goblin and the newer races are not yet.
- `Tools/RetailServerDisplays` writes the worldserver DBC rows and `creature_model_info` SQL those displays need.
- Decoded and converted models are cached on disk with size caps, so a model converts once.

Setup, every config key, what converts and what does not yet are in [docs/RETAIL-ASSETS.md](docs/RETAIL-ASSETS.md).

### Other changes

- **Dev remote** (`-DWOWDECOMP_DEV_REMOTE=ON`): each client listens on a local named pipe,
  `\\.\pipe\wowdecomp-dev-<pid>`, and runs Lua, console and state requests from it on the main thread, so test tools
  can drive the client. Pipe commands count as user activity, so a driven client is not logged out as idle.
- **`fastExit` CVar** (on by default): on exit the client saves the UI, the DB cache and the CVars, flushes its logs
  and quits without tearing the engine down one piece at a time (766 ms instead of 1330 ms in Stormwind). `fastExit 0`
  keeps the original teardown. Based on SatyPardus/wowdecomp#23.
- **Item icons without a client DBC row**: bags, action bars and `GetItemIcon` fall back to the display id the server
  sent, so new custom items no longer need a client `Item.dbc` patch for their icon.
- **Model fixes**: M2s past engine limits (unknown effect ids, bad bone counts) are refused and logged to
  `Logs/Model2.log` instead of crashing; animation tracks that start after the current time no longer underflow
  `FindKey` and are named in `Logs/M2Errors.log`.
- **UI fixes**: `UnitDamage` reads the player's damage done percent as a float (the character panel tooltip showed
  `x106451436800%`), and guild bank withdrawals read as signed, so unlimited shows as Unlimited.

### Tests

`WowdecompTests` is a dependency-free test executable registered with CTest. It always covers the model limits and
BLP code, and with `WOWDECOMP_RETAIL_ASSETS=ON` also the retail config, caches, DB2 reader, converter, displays and
server display rows. It needs no game data or network.

```powershell
cmake --build build/x64 --config Release --target WowdecompTests --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
```

## Progress

The percentages below measure how much of the original binary has a counterpart in this source tree. They do
**not** say how much of the game works, and the missing part is not missing gameplay: the client builds into a
complete executable and the game is 100% playable. Bugs could still be hiding, though; a ported function can behave differently
from the original.

Measured against the 12340 binary on 2026-10-15. A function counts as ported when its address is
tagged in the source.

| | Ported | Total | Share of the original |
|---|---:|---:|---:|
| Functions | 24,515 | 27,157 | 90.3% |
| Code bytes | 5,270,186 | 5,656,573 | 93.2% |

The total leaves out 658 functions that IDA identifies as compiler library code and 132 import thunks.
Most of the untagged remainder is code the build gets elsewhere: compiler-generated code (static
initializers, template instances) that our compiler emits on its own, C runtime helpers, and third-party
libraries that are linked from their upstream sources instead of being decompiled (FreeType, zlib, PCRE,
parts of FMOD Ex).

## Requirements

- Windows 10 or 11
- Visual Studio 2022 with the **Desktop development with C++** workload (MSVC v143 and a Windows SDK)
- CMake 3.10 or newer (the one that ships with Visual Studio works)
- For Win32 builds only: [NASM](https://www.nasm.us/), on `PATH` or passed as `-DNASM_EXECUTABLE=<path>`, and
  `DivxDecoder.dll` from your 3.3.5a client copied to `ThirdParty/DivxDecoder/` (the build copies it next to the
  executables; the x64 build uses its own decoder instead)
- A World of Warcraft **3.3.5a (12340)** client. No game data is included in this repository.

## Building

From a Developer PowerShell or any shell with `cmake` on `PATH`:

```powershell
git clone https://github.com/SatyPardus/wowdecomp.git
cd wowdecomp

# x64 (main target)
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/x64 --config Release --target WowClient --parallel

# Win32 (optional, needs NASM)
cmake -S . -B build/win32 -G "Visual Studio 17 2022" -A Win32
cmake --build build/win32 --config Release --target WowClient --parallel
```

In-source builds are refused, so keep the build directory separate (`build/` above).

The client is written to `bin/Win/`:

| Target | x64 | Win32 |
|---|---|---|
| `WowClient` | `WowClient64.exe` | `WowClient32.exe` |
| `Wow` | `Wow64.exe` | `Wow32.exe` |

`WowClient` is the client with a crash logger: it opens a console window and writes
`WowClient_crash.log` when the client crashes. `Wow` is the same client without it. You can also turn the
logger off in `WowClient` with `-DWOWCLIENT_CRASH_LOG=OFF`.

### Options

| CMake option | Default | Effect |
|---|---|---|
| `WOWDECOMP_DISABLE_SIGNATURE_CHECKS` | `OFF` | Skip the MPQ file and `.toc.sig` signature checks, so modified game data and interface files load |
| `WOWCLIENT_CRASH_LOG` | `ON` | Build `WowClient` with the crash logger |
| `WOWCLIENT_OUTPUT_OVERRIDE` | empty | Name the `WowClient` executable something other than `WowClient64` / `WowClient32` |
| `WOWDECOMP_RETAIL_ASSETS` | `OFF` | (fork) Serve retail models and displays from CASC, see [docs/RETAIL-ASSETS.md](docs/RETAIL-ASSETS.md) |
| `WOWDECOMP_DEV_REMOTE` | `OFF` | (fork) Local named-pipe remote control for test tools |

## Running

1. Copy the `Data` folder of your 3.3.5a client into `bin/Win/`, so you have `bin/Win/Data/`.
2. Point `Data/<locale>/realmlist.wtf` at the server you want to connect to.
3. Start `bin/Win/WowClient64.exe` (or `Wow64.exe`).

## Layout

| Path | Contents |
|---|---|
| `Engine/Source/` | Engine modules: Storm, Gx (Direct3D 9 / OpenGL), Model2, Frame/FrameXML UI, SoundEngine, Lua, ... |
| `WoW/Source/` | Game modules: objects, world rendering, UI, networking, database, glue screens |
| `WoW/Common/` | Code shared by the game modules |
| `ThirdParty/` | Libraries linked from their upstream sources |
| `Tools/WowClient/` | The `WowClient` executable target |
| `Tools/XvidDecoder/` | x64 movie decoder that replaces the original 32-bit `DivxDecoder.dll` |

## Special thanks

- [binana](https://github.com/thunderbrewhq/binana)
- [thunderbrew](https://github.com/thunderbrewhq/thunderbrew)
- [whoa](https://github.com/whoahq/whoa)
- TOMRUS, for the initial IDB
- Everyone who has reported bugs so far
- The WoW modding community
