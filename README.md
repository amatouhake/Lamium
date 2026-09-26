# Lamium

Client-side quality-of-life tools for Minecraft Bedrock and LeviLamina Client.
No server plugin or companion protocol is required.

## Status

Lamium 0.1.2 is a **pre-release**. The main settings, hotkey, HUD, target
card, camera and overlay flows have been exercised in Minecraft on a local
single-player setup; multiplayer servers, controllers and broad
resource-pack/graphics coverage are not verified yet. Some features are still
experimental (see [Known issues](#known-issues)). Runtime evidence and
remaining gaps are tracked in [validation notes](docs/VALIDATION.md).

Supported: Minecraft Bedrock 1.26.51.01, LeviLamina Client 26.51.x, Windows
x64.

## Install

1. Install LeviLamina Client 26.51.x for Minecraft 1.26.51.01 (for example
   with LeviLauncher).
2. Download `Lamium-client-windows-x64.zip` from the
   [Releases](https://github.com/amatouhake/Lamium/releases) page.
3. Extract it so that `Lamium.dll` and `manifest.json` end up in
   `<instance>/mods/Lamium/`.
4. Start Minecraft, enter a world and press `L` to open Lamium Settings.

Settings are stored in `mods/Lamium/config/` and the log is written to
`mods/Lamium/logs/lamium.log`. To uninstall, delete the `mods/Lamium/` folder.

## Features

Press `L` in a world to open Lamium Settings. The screen uses a dense
Bedrock-fitting sidebar/table layout with search, English/Japanese text,
immediate persistence and no Save/Cancel step. Hotkeys, Shapes and HUD layout
are first-class views in the same UI. Every feature has one switch that its
key also toggles; "All", each category and Hotkeys can reset their settings
to the defaults.

- **Camera/visuals:** Zoom up to 50x with a smooth wheel and an optional
  magnification readout, Night Vision, Freelook, experimental FreeCamera
  (keeps its position through menus) and Hide Offhand Item (including
  shields). Zoom and Freelook can be held or toggled.
- **Information/HUD:** ordered Info HUD lines, a live HUD layout editor, a
  Target card with icons, hearts and bars, Debug View basics and toggle
  toasts.
- **World overlays:** Shapes (box, cone, pyramid, ellipsoid, dome and more)
  with per-world persistence, Java-style Chunk Borders, Hitboxes with eye/look
  markers and a light-level overlay.
- **Inventory/inspection:** Shulker and Bundle previews, durability
  information, inventory sorting, Tool Switch and experimental Hand Restock
  (switches to a matching stack elsewhere on the hotbar).
- **Interaction:** Permanent Sneak, Permanent Sprint, experimental Edge Guard
  (stops at block edges without sneaking), breaking restriction and Auto
  Attack/Use (Periodic, Hold or Fast click).

Lamium owns its key bindings; they do not appear in Minecraft's keyboard
settings. Bindings are edited under each feature or in the Hotkeys view.
Clear unbinds an action and Reset restores its default. The Settings action
cannot be cleared, so the UI cannot be locked out. Default keys: `L` settings,
`C` zoom (hold), `J` night vision, `R` sort (in a container). `C` replaces
Minecraft's "copy coordinates" while Lamium is installed unless you rebind it.

## Known issues

- Hand Restock only switches to another hotbar slot; it does not refill from
  the main inventory or refill the offhand (for example a used totem).
- FreeCamera is experimental; multiplayer, controllers and some dimension/menu
  edges are untested. Looking from inside solid blocks, distant caves can be
  cut off along chunk lines (spectator mode does not have this).
- Edge Guard works in local worlds; on multiplayer servers the server may
  still move the player over the edge (untested).
- The breaking restriction modes will be redesigned.
- Auto Attack/Use: whether several clicks per tick land on servers is not
  verified.

Reports are welcome as GitHub issues; please attach
`mods/Lamium/logs/lamium.log`.

## Development

The repository is the source of truth for development:

- [Design](docs/DESIGN.md) defines accepted product and UI behavior.
- [Backlog](docs/BACKLOG.md) defines current work, ordering and model class.
- [Validation](docs/VALIDATION.md) separates compiled/tested behavior from
  behavior actually checked in Minecraft.
- [Agent guide](AGENTS.md) is the working manual for coding agents.
- [Provenance](docs/PROVENANCE.md) separates dependencies, incorporated source
  and reference-only projects.

Inventory sorting uses ordinary game operations, waits for matching responses
between operations and revalidates the affected region before continuing.
Rejection, missing responses or a changed screen stops the plan.

## Build

Target: Windows x64, Minecraft 1.26.51.01, LeviLamina Client v26.51.5.
Install Visual Studio Build Tools with the Windows SDK and C++ toolchain, LLVM
(clang-cl), Git and xmake. Dependencies are resolved by xmake; no sibling
project or machine-specific configuration is required.

```powershell
xmake f -a x64 -m release -p windows --target_type=client -y
xmake build Lamium
xmake build LamiumTests
xmake run LamiumTests
xmake build LamiumNativeTests
xmake run LamiumNativeTests
./scripts/Check-Package.ps1
```

Use PowerShell 7 for the package check. `xmake-requires.lock` records the
package versions and repository revisions used for Windows x64. Keep it when
cloning; review changes from `xmake require --upgrade` before committing them.

The project includes a client SDK recipe that downloads the matching
LeviLamina source headers and official release DLL with SHA-256 verification,
then generates an import library using Visual Studio's `dumpbin` and `lib`.
It does not install or bundle that runtime. See
[SDK build notes](packages/README.md).

GitHub Actions builds on Windows, runs the pure and native tests plus package
checks, and uploads a development artifact. Current main CI is exercised, but
Minecraft runtime checks still require a local installation and cannot be
proved by CI.

Use a separate launcher instance for development. Do not enable another mod
that changes the same camera or inventory behavior while testing Lamium.

## Design and diagnostics

Lamium writes diagnostics to `mods/Lamium/logs/lamium.log`, flushing
informational messages immediately so failures can be inspected while the game
is running. Logs rotate by size/date and retain at most seven archives.

One mod owns settings and input actions. Features own their transient state and
restore vanilla behavior when disabled, leaving a world or losing input focus.
Settings and UI remain local. Inventory actions must use ordinary game
operations and verify resulting slot contents before continuing.

## License

Copyright (C) 2026 amatouhake.

Lamium is licensed under the GNU Lesser General Public License, version 3 only
(`LGPL-3.0-only`); see [COPYING.LESSER](COPYING.LESSER) and [COPYING](COPYING).
Third-party notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
