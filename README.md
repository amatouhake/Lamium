<p align="center">
  <img src="assets/icon/lamium-icon.svg" alt="Lamium icon" width="120">
</p>

<h1 align="center">Lamium</h1>

<p align="center">
  Client-side quality-of-life tools for Minecraft Bedrock and LeviLamina Client.<br>
  No server plugin or companion protocol is required.
</p>

## Status

Lamium 0.1.9 is an early (0.x) release. The main settings, hotkey, HUD,
target card, camera and overlay flows have been exercised in Minecraft on a
local single-player setup. Map features also have external BDS checks;
server coverage for other features, controllers and broad resource-pack/graphics
coverage remains incomplete. Features marked
experimental in the settings are still settling (see
[Known issues](#known-issues)). Runtime evidence and
remaining gaps are tracked in [validation status](docs/VALIDATION.md).

Supported: Minecraft Bedrock 1.26.51.01, LeviLamina Client 26.51.x, Windows
x64. Android support is not currently planned: it would be a separate ARM64
port, not a repackaged build.

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
   `config/`; do not delete the folder first. From 0.2.0 the ZIP holds no
   `Lamium.pdb`; the symbols are a separate asset
   (`Lamium-<version>-client-windows-x64.pdb.zip`), only needed to read a
   crash address.
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
  position through menus, adjustable speed with a sprint boost, a player- or
  world-fixed position, the camera's own position and facing in the Info HUD
  and Debug View, waypoints added where the camera is, and optionally leaving
  when your body is hit), Night Vision that can light everything evenly (no
  dark rings by light sources), experimental Connected Textures (glass, glass
  panes, bookshelves and sandstone next to the same block draw as one
  surface, without seams; glass, bookshelves and sandstone at the texture's
  own scale), Hide Offhand Item
  (including shields) and
  experimental Hide effects (rain and snow, particles, boss bars, the nausea
  color, underwater, lava and powder snow fog, and the distance fog on land,
  in the Nether and in the End; one main switch turns the selected effects
  on). Zoom, Freelook and FreeCamera can be held or toggled.
- **Information/HUD:** ordered Info HUD lines (now including scaled
  coordinates, biome ids, difficulty, yaw/pitch, sprinting, horizontal and
  vertical speed and real time), a Durability HUD for the held item, offhand
  and armor, an offhand slot beside the hotbar (count and durability bar
  included), saturation shown as gold outlines on the hunger bar (holding
  food previews what eating it would add), a live HUD layout editor, a
  Target card with icons, hearts in real health units (one heart per 2 HP),
  armor and bars, a Java F3-style Debug View (game, world, look-at and PC
  details, entity counts by kind and the most common entity types), a player
  list while `Tab` is held (faces, platform, dimension, distance and
  permission marks), an inventory HUD (your inventory as a 9x3 grid) and a
  used-slots counter such as 13/36, and toggle toasts.
- **World overlays:** Shapes (box, cone, pyramid, ellipsoid, dome and more)
  with per-world persistence, Java-style Chunk Borders, Hitboxes with eye/look
  markers and a light-level overlay.
- **Inventory/inspection:** Shulker and Bundle previews, durability and
  food values (hunger and saturation as hunger-bar icons) inside the game's
  item tooltip, every trader level in the trade screen (locked trades shown
  dimmed, with their items' names on hover), English names and IDs found by
  the recipe book and creative search in any game language, inventory sorting, experimental drag and wheel transfer between
  your inventory and storage (also from the inventory screen), Tool Switch and
  Weapon Switch (optionally fetching a tool or weapon from the inventory, into
  the selected slot or a fixed one), an offhand swap key (`F`, as in Java),
  experimental Fake Offhand (uses a hotbar block or item as if it were in the
  offhand) and experimental Hand Restock (tops up consumed items in the same
  hand slot from the inventory or hotbar, with a threshold and a source order,
  swaps container remainders, and refills an offhand totem after it saves
  you), and experimental Death layout restore (picking up your dropped items
  puts the hotbar, armor and offhand, optionally the whole inventory, back
  the way they were when you died).
- **Interaction:** Permanent Sneak, Permanent Sprint, experimental Edge Guard
  (stops at block edges without sneaking), experimental Tool Protection (on by
  default: at 1 durability a tool or worn elytra is swapped for a spare, or
  mining stops), experimental Auto Elytra (a key or a firework jump puts an
  elytra on; the chestplate returns after landing), breaking restriction
  (modes anchored where you start breaking, including a height band) and
  Auto Attack/Use (Periodic, Hold or Fast click).
- **Map (experimental, off by default):** a minimap HUD element (size,
  range, north or heading up, round or square, compass, coordinates and
  biome lines, hold to enlarge) colored from your block textures, with a
  cave view under a roof and in the Nether; a radar of nearby players, mobs
  and dropped items as dots, or optionally as mob faces and player skin
  heads; waypoints with the last death point, shown on the minimap and in
  the world, edited in a Waypoints screen; and a world map (`M`) of the
  areas you have visited, recorded per world, with a waypoint side panel and
  a link that opens the same place in ChunkBase's seed map. Schematic
  placements and shapes show on the world map, where a click selects one and
  its menu shows or hides it or opens it in its own screen; shapes can also
  show on the minimap (an option, off by default).
- **Schematics (experimental, off by default):** place `.mcstructure` files
  from `mods/Lamium/schematics/` as ghost blocks (move, turn, mirror, show
  layers along any axis), see what is missing, wrong or in the wrong state
  in the world, the target card (with the expected block and what to change),
  a Check list and a HUD, count the materials left against what you carry
  (Shulker Boxes included, in chests and stacks), save an area of the world
  as a schematic (also larger than the render distance, by walking along it),
  and work with all of it during play from a radial schematic menu and a key
  that repeats the last adjustment with the wheel. Ghosts draw doors, beds,
  honey, water and lava, block entities with their saved data and entities as
  their models; the Files and Check tabs have a turnable 3D preview; placements
  show on the minimap and world map, and can be selected, shown or hidden
  there. Saved files use the game's current format and load in a structure
  block.

Lamium owns its key bindings; they do not appear in Minecraft's keyboard
settings. Bindings are edited under each feature or in the Hotkeys view.
Clear unbinds an action and Reset restores its default. The Settings action
cannot be cleared, so the UI cannot be locked out. New actions start unbound.
Default keys: `L` settings, `M` world map, `Tab` player list (hold),
`C` zoom (hold), `R` sort (in a container), `F` offhand swap, `F3` Debug
View, `F3+B` Hitboxes and `F3+G` Chunk Borders. `C` replaces
Minecraft's "copy coordinates" while Lamium is installed unless you rebind it.

## Known issues

- Hand Restock is checked in local worlds; server timing and very fast use or
  ambiguous inventory changes may skip a refill. It does not refill tools
  (Tool Protection swaps a tool before it breaks).
- Enchanted shields show no glint in Lamium's item icons (offhand slot,
  Shulker/Bundle previews, the inventory HUD); enchanted golden apples and
  other flat icons do.
- The Simplified Chinese text is a first, AI-assisted translation;
  corrections are welcome (see [Translating](docs/TRANSLATING.md)).
- Hide effects cannot hide the carved pumpkin overlay or the spyglass frame
  yet. Water, lava and powder snow fog hiding is checked with the vanilla
  resources in Fancy graphics; other packs and graphics modes are unverified.
  Under Vibrant Visuals the distance fog stays vanilla (in 0.1.8 hiding it
  turned the world magenta and flickering there) and Night Vision has no
  effect.
- FreeCamera is experimental. Underground terrain drawing is checked in a
  local world and on BDS; Hold/Toggle operation, menus, focus loss and dimension
  travel are also checked. Other players' view of the body, controllers and
  broader graphics/resource configurations remain untested.
- Edge Guard works in local worlds; on multiplayer servers the server may
  still move the player over the edge (untested).
- Inventory transfer works only in ordinary storage (chests, barrels, Shulker
  Boxes and similar); multiplayer is untested.
- The breaking restriction modes will be redesigned.
- Auto Attack/Use: whether several clicks per tick land on servers is not
  verified.
- The map is experimental: minimap, radar and world map have local and
  external BDS checks with a large explored area. Waypoint storage per server,
  distant players on dedicated servers, and the latest section-request/teleport
  changes on servers remain unverified. Some servers may treat seeing mobs and
  players through walls as unfair. A few mob faces are not right yet (silverfish and tadpoles stay
  dots; camel and hoglin faces may look off), and skins with custom head
  models are untested.
- Schematics are experimental and checked in local worlds only; servers,
  other dimensions and very large files are not verified. Under Vibrant
  Visuals ghosts lose their light-blue tint and the honey ghost is black.
  Entity models stay in their rest pose (a wolf's tail, zombie arms) with
  the default skin, sheep wool is not drawn, and entities without a model
  show as a dashed frame. Entities are checked by type near their spot and
  saved only by type, position and facing. In the 3D previews parts behind
  some cut-out blocks (seagrass, a spawner) can be missing, and grass and
  leaves take the biome you stand in.
- Connected Textures is experimental and checked in local worlds with the
  vanilla resources in Fancy graphics; Vibrant Visuals, resource packs (other
  border widths) and servers are unverified. Only glass, glass panes,
  bookshelves and sandstone connect. Joined glass panes stretch their glass
  by one texel at the joins instead of keeping the texture's own scale (drawn
  that way, panes shaded dark on one half).
- The player list knows where other players are only while they share your
  dimension: elsewhere it shows the dimension they were last seen in, faded,
  without a distance. The player list is checked locally, on BDS and in a
  LAN world; the locked trades only in local worlds.
- Death layout restore is checked in local worlds; with instant respawn the
  inventory may be read as kept and nothing is restored. Servers with
  latency are untested.

Reports, questions and translation fixes are welcome as GitHub issues; see
[Contributing](CONTRIBUTING.md) for what helps in a report and how pull
requests are handled.

## Development

The repository is the source of truth for development:

- [Documentation guide](docs/README.md) indexes the docs and explains their roles.
- [Design](docs/DESIGN.md) defines accepted product and UI behavior.
- [Backlog](docs/BACKLOG.md) defines current work, ordering and model class.
- [Validation](docs/VALIDATION.md) separates compiled/tested behavior from
  behavior actually checked in Minecraft.
- [Distribution](docs/DISTRIBUTION.md) defines packaging, managed updates and
  settings-preservation requirements.
- [Agent guide](AGENTS.md) is the working manual for coding agents.
- [Contributing](CONTRIBUTING.md) covers issues and pull requests from
  outside contributors.
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
