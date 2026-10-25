<p align="center">
  <img src="assets/icon/lamium-icon.svg" alt="Lamium icon" width="120">
</p>

<h1 align="center">Lamium</h1>

<p align="center">
  Client-side quality-of-life tools for Minecraft Bedrock and LeviLamina Client.<br>
  No server plugin or companion protocol is required.
</p>

<p align="center">
  English | <a href="README.zh-TW.md">繁體中文</a>
</p>

> [!NOTE]
> Not an official LeviMC product. Not approved by LeviMC and not associated
> with LeviMC. Lamium is an independent community project, published and
> maintained by amatouhake, who is responsible for it. Contact:
> [GitHub issues](https://github.com/amatouhake/Lamium/issues), or a private
> report from the repository's Security tab.

## Status

Lamium 0.3.0 is an early (0.x) release. The main settings, hotkey, HUD,
target card, camera and overlay flows have been exercised in Minecraft on a
local single-player setup. Map features also have external BDS checks;
server coverage for other features, controllers and broad resource-pack/graphics
coverage remains incomplete. Features marked
experimental in the settings are still settling (see
[Known issues](#known-issues)). Runtime evidence and
remaining gaps are tracked in [validation status](docs/VALIDATION.md).

Supported: Minecraft Bedrock 1.26.51.01, LeviLamina Client 26.51.5 or later
(26.51 series), Windows x64. Android support is not currently planned: it
would be a separate ARM64 port, not a repackaged build. On a game version
Lamium has not been checked on, the parts that depend on it (for example
Night Vision's light, Edge Guard, Connected Textures, ghost blocks) stay
vanilla; the settings screen, toggle toasts and a toast on joining a world
say which features are paused.

## Install

**LeviLauncher (recommended).** In an instance with LeviLamina Client 26.51.5
or later for Minecraft 1.26.51.01, find Lamium in LeviLauncher's mod browser
(Bedrinth) and install it. Update and uninstall it the same way. Updates keep
your settings. Start Minecraft, enter a world and press `L` to open Lamium
Settings.

**LIP CLI (advanced).** Lamium is the LIP package
`github.com/amatouhake/Lamium#client`, the same one LeviLauncher installs
(`lip install github.com/amatouhake/Lamium#client` in the instance folder).
Only the LeviLauncher path has been tested by the maintainer.

**Manual ZIP (fallback).**

1. Install LeviLamina Client 26.51.5 or later for Minecraft 1.26.51.01.
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

Press `L` in a world or on the title screen to open Lamium Settings. Every
feature has one switch there, which its key also toggles; changes are saved
at once. *Exp.* marks an experimental feature, still settling. **Key** is
the default key ("—": unbound until you bind one). **Default** is the state
after a fresh install ("—": a key or screen without a switch). What each
feature does in detail: [Features](docs/FEATURES.md).

### Camera & view

| Feature | What it does | Key | Default |
| --- | --- | --- | --- |
| Zoom | Zoom up to 50x with the wheel; optional wide angle below 1x | `C` (hold) | — |
| Freelook | Look around while moving straight on | — | — |
| FreeCamera | Fly the view while the player stays in place | — | — |
| Night Vision | Brighten the view locally, optionally without dark rings | — | Off |
| Hide Offhand Item | Hide the offhand item (shields too) in first person | — | Off |
| Hide effects *Exp.* | Hide rain, particles, boss bars, nausea, fogs, the portal screen | — | Off |
| Connected Textures *Exp.* | Glass, panes, bookshelves and sandstone without seams | — | Off |

### HUD & overlays

| Feature | What it does | Key | Default |
| --- | --- | --- | --- |
| Info HUD | Coordinates, biome, speed, time and other lines | — | Off |
| Target Info | A card for the block or mob you aim at | — | Off |
| Debug View | A Java F3-style panel | `F3` | Off |
| Durability HUD | Held item, offhand and armor durability; arrows and fireworks | — | Off |
| Offhand slot | The offhand item beside the hotbar | — | Off |
| Saturation | Saturation as gold outlines on the hunger bar | — | On |
| Player list | Players with faces, platform, dimension and distance | `Tab` (hold) | — |
| Inventory HUD | Your inventory as a 9x3 grid during play | — | Off |
| Used slots | Used inventory slots, such as 13/36 | — | Off |
| Status effects HUD | Effects with levels and remaining time | — | Off |
| Chat position | Chat messages at the bottom left, as on Java Edition | — | Off |
| Toggle toasts | A notice when a key turns a feature on or off | — | On |
| Chunk Borders | Java-style chunk edges | `F3+G` | Off |
| Hitboxes | Entity bounds with eye and look markers | `F3+B` | Off |
| Light Level Overlay | Where monsters can spawn | — | Off |
| Shapes | Boxes, cones, domes and more in the world, saved per world | — | On |

The HUD layout view moves and styles Lamium's HUD elements.

### Map *Exp.*

| Feature | What it does | Key | Default |
| --- | --- | --- | --- |
| Minimap | A minimap colored from your block textures, with a cave view | — | Off |
| Radar | Players, mobs and items on the minimap | — | On |
| Waypoints | Waypoints and the last death point, on the maps and in the world | — | On |
| World map | A full-screen map of the areas you have visited | `M` | Off |

Radar draws on the minimap. Waypoints and the death point also show in the
world, with both maps off.

### Schematics *Exp.*

| Feature | What it does | Key | Default |
| --- | --- | --- | --- |
| Schematics | Place `.mcstructure` files as ghost blocks, check the build, count materials, save an area | — | Off |
| Schematic menu | A radial menu with every schematic operation; each can also get its own key | — | — |
| Move schematic target | Move the placement, corner or area away/closer | Alt+Wheel | — |
| Schematic HUD | Check counts and materials left for the selected placement | — | Off |

### Inventory

| Feature | What it does | Key | Default |
| --- | --- | --- | --- |
| Container previews | Shulker Box and Bundle contents in the tooltip | — | On |
| Durability numbers | Remaining durability in the tooltip | — | On |
| Food values | Hunger and saturation a food restores, in the tooltip | — | On |
| All trader levels | Every trade level in the trade screen | — | On |
| Repeated trades | Shift + right click repeats a trade | — | On |
| Search English names too | Recipe and creative search by English name or ID | — | On |
| Inventory sorting | Sort the inventory or an open container | `R` | On |
| Inventory transfer *Exp.* | Move items with the wheel, modified clicks and drags | — | Off |
| Tool Switch | Pick the right tool when mining | — | Off |
| Weapon Switch | Pick the strongest weapon when attacking | — | Off |
| Offhand swap | Swap the held item with the offhand | `F` | On |
| Fake Offhand *Exp.* | Use a hotbar block or item as if it were in the offhand | Right click | Off |
| Hand Restock | Refill the held slot when it runs low; refill a used totem | — | Off |
| Death layout restore *Exp.* | Put picked-up items back where they were when you died | — | Off |

### Actions

| Feature | What it does | Key | Default |
| --- | --- | --- | --- |
| Permanent Sneak | Toggle sneaking with a key | — | Off |
| Permanent Sprint | Toggle sprinting with a key | — | Off |
| Edge Guard | Stop at block edges without sneaking | — | Off |
| Tool Protection | Swap or stop before a tool or elytra breaks | — | On |
| Auto Elytra | Put an elytra on for a flight, the chestplate back after | — | Off |
| Block Restrictions | Limit breaking to a region you start in | — | Off |
| Auto Attack / Auto Use | Click for you: periodic, while held, or fast | — | Off |

### General

| Feature | What it does | Key | Default |
| --- | --- | --- | --- |
| Calculator | A calculator with stack, chest and shulker box units | — | — |
| Game language *Exp.* | Change Minecraft's language without leaving the world | — | — |
| Skip server resource packs *Exp.* | Show a server's world without its resource packs | — | Off |

Lamium's keys are its own and do not appear in Minecraft's keyboard
settings. Change them under each feature or in the Hotkeys view; new
actions start unbound. `C` replaces Minecraft's "copy coordinates" while
Lamium is installed unless you rebind it.

## Known issues

Reports, questions and translation fixes are welcome as GitHub issues; see
[Contributing](CONTRIBUTING.md) for what helps in a report and how pull
requests are handled. What has been checked in game, per feature, is in
[validation status](docs/VALIDATION.md).

**Camera & view**

- **FreeCamera:** underground terrain drawing is checked in a local world and
  on BDS; Hold/Toggle operation, menus, focus loss and dimension travel are
  also checked. Other players still see the body, and it works on servers
  (maintainer use). Controllers and broader graphics/resource configurations
  remain untested. Started while your head is inside a block, the camera
  keeps the inside-a-block view while it flies; step out of the block
  first. Moving only the camera does not make the game generate new terrain.
  Flown past the edge of the loaded area, the camera can show only the sky
  (L-227). Large terrain gaps after dimension changes or teleports were
  reported once and did not recur in A/B tests; walking near reloads them
  (L-243).
- **Hide effects:** cannot hide the carved pumpkin overlay or the spyglass
  frame yet. Water, lava and powder snow fog hiding is checked with the
  vanilla resources in Fancy graphics; other packs and graphics modes are
  unverified. Under Vibrant Visuals the distance fog stays vanilla (in 0.1.8
  hiding it turned the world magenta and flickering there) and Night Vision
  has no effect.
- **Connected Textures:** checked in local worlds with the vanilla resources
  in Fancy graphics; Vibrant Visuals, resource packs (other border widths)
  and servers are unverified. Only glass, glass panes, bookshelves and
  sandstone connect. Joined glass panes stretch their glass by one texel at
  the joins instead of keeping the texture's own scale (drawn that way,
  panes shaded dark on one half).

**HUD & overlays**

- **Player list:** knows where other players are only while they share your
  dimension: elsewhere it shows the dimension they were last seen in, faded,
  without a distance. Checked locally, on BDS and in a LAN world.
- **Item icons:** enchanted shields show no glint in Lamium's item icons
  (offhand slot, Shulker/Bundle previews, the inventory HUD); enchanted
  golden apples and other flat icons do. Enchanted leather armor shows its
  undyeable parts see-through there (unenchanted leather draws whole).

**Map**

- Minimap, radar and world map have local and external BDS checks with a
  large explored area. Waypoint storage per server, distant players on
  dedicated servers, and the latest section-request/teleport changes on
  servers remain unverified.
- Some servers may treat seeing mobs and players through walls as unfair.
- A few mob faces are not right yet (silverfish and tadpoles stay dots;
  camel and hoglin faces may look off), and skins with custom head models
  are untested.

**Schematics**

- Checked in local worlds only; servers, other dimensions and very large
  files are not verified.
- Under Vibrant Visuals ghosts lose their light-blue tint and the honey
  ghost is black.
- A ghost face next to real glass or a beacon can disappear from some angles,
  leaving the shared plane empty (L-245).
- Entity models stay in their rest pose (a wolf's tail, zombie arms) with the
  default skin, sheep wool is not drawn, and entities without a model show
  as a dashed frame. Entities are checked by type near their spot and saved
  only by type, position and facing.
- In the 3D previews parts behind some cut-out blocks (seagrass, a spawner)
  can be missing, and grass and leaves take the biome you stand in.

**Inventory**

- **All trader levels:** the locked trades are checked only in local worlds.
- **Inventory transfer:** bulk and leave-one moves are checked in chest,
  furnace, brewing stand and beacon screens, locally and on a test BDS.
  Other screen families, other servers and rejected moves on laggy servers
  are untested. The held-stack Shift + click and drop gestures are checked in
  a chest screen in a local world only.
- **Hand Restock:** checked in local worlds and in everyday server use; server timing and very fast use
  or ambiguous inventory changes may skip a refill. It does not refill tools
  (Tool Protection swaps a tool before it breaks).
- **Death layout restore:** checked in local worlds; with instant respawn the
  inventory may be read as kept and nothing is restored. In use it sometimes
  restores after a death you did not expect, or in a layout that looks
  wrong (under investigation). Servers with latency are untested.

**Actions**

- **Edge Guard:** checked in local worlds, on a test BDS (including server
  position corrections) and in server use. It keeps you on the edge in
  normal play; in rare cases, such as heavy lag or a very small edge margin,
  you can still drop. Other servers' anti-cheat is untested.
- **Block Restrictions:** the breaking modes will be redesigned.
- **Auto Attack/Use:** whether several clicks per tick land on servers is not
  verified.

**General**

- **Game language:** changing the game language inside a world many times in
  a row froze the game once (the title screen does not); Lamium allows one
  change per 10 seconds in a world. The cause is unknown.
- **Skip server resource packs:** checked on a test BDS with a cosmetic pack.
  Servers whose packs add blocks, items, entities or menus will show those
  wrong. On a server that requires its packs, the game itself turns off your
  global resource packs, so the look is vanilla, not your own packs.
- **Simplified Chinese:** the text is a first, AI-assisted translation;
  corrections are welcome (see [Translating](docs/TRANSLATING.md)).

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

Features own their transient state and restore vanilla behavior when
disabled, leaving a world or losing input focus. Inventory actions use
ordinary game operations and verify the resulting slot contents before
continuing.

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

## License

Copyright (C) 2026 amatouhake.

Lamium is licensed under the GNU Lesser General Public License, version 3 only
(`LGPL-3.0-only`); see [COPYING.LESSER](COPYING.LESSER) and [COPYING](COPYING).
Third-party notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
