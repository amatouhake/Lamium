<p align="center">
  <img src="assets/icon/lamium-icon.svg" alt="Lamium icon" width="120">
</p>

<h1 align="center">Lamium</h1>

<p align="center">
  Client-side quality-of-life tools for Minecraft Bedrock and LeviLamina Client.<br>
  No server plugin or companion protocol is required.
</p>

## Status

Lamium 0.1.6 is an early (0.x) release. The main settings, hotkey, HUD,
target card, camera and overlay flows have been exercised in Minecraft on a
local single-player setup; multiplayer servers, controllers and broad
resource-pack/graphics coverage are not verified yet. Features marked
experimental in the settings are still settling (see
[Known issues](#known-issues)). Runtime evidence and
remaining gaps are tracked in [validation status](docs/VALIDATION.md).

Supported: Minecraft Bedrock 1.26.51.01, LeviLamina Client 26.51.x, Windows
x64. Android is not planned: it would be a separate ARM64 port, not a
repackaged build.

## Install

**LeviLauncher (recommended).** In an instance with LeviLamina Client 26.51.x
for Minecraft 1.26.51.01, find Lamium in LeviLauncher's mod browser (Bedrinth)
and install it. Update and uninstall it the same way. Updates keep your
settings. Start Minecraft, enter a world and press `L` to open Lamium
Settings.

**LIP CLI (advanced).** Lamium is the LIP package
`github.com/amatouhake/Lamium#client`, the same one LeviLauncher installs
(`lip install github.com/amatouhake/Lamium#client` in the instance folder).
Only the LeviLauncher path has been tested by the maintainer.

**Manual ZIP (fallback).**

1. Install LeviLamina Client 26.51.x for Minecraft 1.26.51.01.
2. Download `Lamium-<version>-client-windows-x64.zip` from the
   [Releases](https://github.com/amatouhake/Lamium/releases) page.
3. Extract it so that `Lamium.dll` and `manifest.json` end up in
   `<instance>/mods/Lamium/`. Extracting over an older install keeps
   `config/`; do not delete the folder first.
4. Start Minecraft, enter a world and press `L` to open Lamium Settings.

Do not mix the two: a manual copy into a LeviLauncher-managed Lamium folder
leaves LIP's file records out of date.

Settings are stored in `mods/Lamium/config/` and the log is written to
`mods/Lamium/logs/lamium.log`. For a manual install, deleting the whole
`mods/Lamium/` folder also deletes those runtime-created settings and logs.
Uninstalling from LeviLauncher removes Lamium's files but keeps `config/` and
`logs/`, so a later reinstall picks the settings up again; delete
`mods/Lamium/` afterwards to remove them too.
The managed-update ownership and preservation contract is documented in
[Distribution](docs/DISTRIBUTION.md).

## Features

Press `L` in a world to open Lamium Settings. The screen uses a dense
Bedrock-fitting sidebar/table layout with search, English, Japanese and
Simplified Chinese text (following the game language), immediate persistence and no Save/Cancel step. Hotkeys, Shapes and HUD layout
are first-class views in the same UI. Every feature has one switch that its
key also toggles; "All", each category and Hotkeys can reset their settings
to the defaults.

- **Camera/visuals:** Zoom up to 50x with a smooth wheel and an optional
  magnification readout, Night Vision, Freelook (rear third person by
  default, restoring your previous view), experimental FreeCamera (keeps its
  position through menus, adjustable speed with a sprint boost, and a
  player- or world-fixed position), Hide Offhand Item (including shields) and
  experimental Hide effects (rain and snow, particles, boss bars, the nausea
  color, and underwater, lava and powder snow fog; one main switch turns the
  selected effects on). Zoom, Freelook and FreeCamera can be held or toggled.
- **Information/HUD:** ordered Info HUD lines (now including scaled
  coordinates, biome ids, difficulty, yaw/pitch, sprinting, horizontal and
  vertical speed and real time), a Durability HUD for the held item, offhand
  and armor, an offhand slot beside the hotbar (count and durability bar
  included), saturation shown as gold outlines on the hunger bar (holding
  food previews what eating it would add), a live HUD layout editor, a
  Target card with icons, hearts in real health units (one heart per 2 HP),
  armor and bars, a Java F3-style Debug View (game, world, look-at and PC
  details) and toggle toasts.
- **World overlays:** Shapes (box, cone, pyramid, ellipsoid, dome and more)
  with per-world persistence, Java-style Chunk Borders, Hitboxes with eye/look
  markers and a light-level overlay.
- **Inventory/inspection:** Shulker and Bundle previews, durability and
  food values (hunger and saturation as hunger-bar icons) inside the game's
  item tooltip, inventory sorting, experimental drag and wheel transfer between
  your inventory and storage, Tool Switch and Weapon Switch (optionally
  fetching a tool or weapon from the inventory), experimental Fake Offhand (temporarily selects a hotbar block for
  placement) and experimental Hand Restock (tops up consumed items in the same
  hand slot from the inventory or hotbar, swaps container remainders, and
  refills an offhand totem after it saves you).
- **Interaction:** Permanent Sneak, Permanent Sprint, experimental Edge Guard
  (stops at block edges without sneaking), experimental Tool Protection (on by
  default: at 1 durability a tool or worn elytra is swapped for a spare, or
  mining stops), experimental Auto Elytra (a key or a firework jump puts an
  elytra on; the chestplate returns after landing), breaking restriction and
  Auto Attack/Use (Periodic, Hold or Fast click).
- **Map (experimental, off by default):** a minimap HUD element (size,
  range, north or heading up, round or square, compass, coordinates and
  biome lines, hold to enlarge) colored from your block textures, with a
  cave view under a roof and in the Nether; a radar of nearby players, mobs
  and dropped items as dots, or optionally as mob faces and player skin
  heads; waypoints with the last death point, shown on the minimap and in
  the world, edited in a Waypoints screen; and a world map (`M`) of the
  areas you have visited, recorded per world, with a waypoint side panel and
  a link that opens the same place in ChunkBase's seed map.

Lamium owns its key bindings; they do not appear in Minecraft's keyboard
settings. Bindings are edited under each feature or in the Hotkeys view.
Clear unbinds an action and Reset restores its default. The Settings action
cannot be cleared, so the UI cannot be locked out. New actions start unbound.
Default keys: `L` settings,
`C` zoom (hold), `R` sort (in a container), `F3` Debug View, `F3+B` Hitboxes
and `F3+G` Chunk Borders. `C` replaces
Minecraft's "copy coordinates" while Lamium is installed unless you rebind it.

## Known issues

- Hand Restock is checked in local worlds; server timing and very fast use or
  ambiguous inventory changes may skip a refill. It does not refill tools
  (Tool Protection swaps a tool before it breaks).
- Leather armor icons miss their undyeable part in the Durability HUD and in
  Shulker/Bundle previews.
- Enchanted shields show no glint in the offhand slot and in Shulker/Bundle
  previews (enchanted golden apples and other flat icons do).
- The Simplified Chinese text is a first, AI-assisted translation;
  corrections are welcome (see [Translating](docs/TRANSLATING.md)).
- Hide effects cannot hide the carved pumpkin overlay or the spyglass frame
  yet. Water, lava and powder snow fog hiding is checked with the vanilla
  resources in Fancy graphics; other packs and graphics modes are unverified.
- FreeCamera is experimental; multiplayer, controllers and some dimension/menu
  edges are untested. Looking from inside solid blocks, distant caves can be
  cut off along chunk lines (spectator mode does not have this).
- Edge Guard works in local worlds; on multiplayer servers the server may
  still move the player over the edge (untested).
- Inventory transfer works only in ordinary storage (chests, barrels, Shulker
  Boxes and similar); multiplayer is untested.
- The breaking restriction modes will be redesigned.
- Auto Attack/Use: whether several clicks per tick land on servers is not
  verified.
- The map is experimental: it is checked in local worlds and, for the
  radar's players, with other players; servers and very large worlds are
  not verified. Some servers may treat seeing mobs and players through walls
  as unfair. A few mob faces are not right yet (silverfish and tadpoles stay
  dots; camel and hoglin faces may look off), and skins with custom head
  models are untested.

Reports are welcome as GitHub issues; please attach
`mods/Lamium/logs/lamium.log`.

## Development

The repository is the source of truth for development:

- [Design](docs/DESIGN.md) defines accepted product and UI behavior.
- [Backlog](docs/BACKLOG.md) defines current work, ordering and model class.
- [Validation](docs/VALIDATION.md) separates compiled/tested behavior from
  behavior actually checked in Minecraft.
- [Distribution](docs/DISTRIBUTION.md) defines packaging, managed updates and
  settings-preservation requirements.
- [Agent guide](AGENTS.md) is the working manual for coding agents.
- [Provenance](docs/PROVENANCE.md) separates dependencies, incorporated source
  and reference-only research.

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
