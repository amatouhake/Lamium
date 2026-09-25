# Lamium

Client-side quality-of-life tools for Minecraft Bedrock and LeviLamina Client.
No server plugin or companion protocol is required.

## Development status

Lamium is under active development and has no stable release yet. The current
main branch is substantially beyond the original prototype: the shared settings
screen, Lamium-owned hotkeys, Shapes view, HUD layout editor, target card,
camera tools and world overlays are integrated, and the main HUD/editor/target
flows have been exercised in Minecraft. Runtime evidence and remaining gaps are
tracked in [validation notes](docs/VALIDATION.md).

The repository is the source of truth for development:

- [Design](docs/DESIGN.md) defines accepted product and UI behavior.
- [Backlog](docs/BACKLOG.md) defines current work, ordering and model class.
- [Validation](docs/VALIDATION.md) separates compiled/tested behavior from
  behavior actually checked in Minecraft.
- [Agent guide](AGENTS.md) is the working manual for coding agents.

### Current UI and controls

Press `L` in a world to open Lamium Settings. The screen uses a dense
Bedrock-fitting sidebar/table layout with search, English/Japanese text,
immediate persistence and no Save/Cancel step. Hotkeys, Shapes and HUD layout
are first-class views in the same UI. Bounded numeric options such as view
range and zoom use sliders; precise values keep direct numeric entry.

Lamium owns its action bindings. A missing override uses Lamium's default,
Clear explicitly unbinds an action, and Reset restores the default. The
Settings action itself cannot be cleared so the UI cannot be locked out.
Bindings are edited under each feature or in the Hotkeys view; Open Hotkeys,
Open Shapes and Open HUD layout actions are available for direct access.

The remaining input-foundation task is [L-32](docs/BACKLOG.md): overlapping
chords still use the older order-insensitive/subset matcher. The agreed
ordinary-vs-modifier-like semantics and overlap warnings are documented but
not implemented yet.

### Current feature areas

Implemented areas include:

- Camera/visuals: Zoom, NightVision, Freelook, experimental FreeCamera and
  Hide Offhand Item.
- Inspection/inventory: Shulker and Bundle previews, durability information,
  inventory sorting, experimental Tool Switch and Hand Restock.
- Information/HUD: ordered Info HUD lines, a live HUD layout editor, Target
  card with icons/hearts/bars and camera-following picks, Debug View basics,
  toggle toasts and automation/restriction status.
- World overlays: Shapes with local-world persistence, Java-style Chunk
  Borders, Hitboxes and an experimental light-level overlay.
- Interaction: Permanent Sneak, breaking restriction work and Auto
  Attack/Use (Periodic, Hold and Fast click, each on its own key).

Not every implemented feature is release-ready. Hand Restock has not yet
successfully replenished an item; Hide Offhand still has a shield-specific
render-path issue; continuous Tool Switch, breaking/placement restriction
behavior and several native-data questions remain research items. Auto
Attack/Use's tick-driven modes still need their in-game checks (BACKLOG L-34).
See the backlog instead of inferring readiness from presence in the UI.

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
