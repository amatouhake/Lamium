# Lamium

Client-side quality-of-life tools for Minecraft Bedrock and LeviLamina Client.
No server plugin or companion protocol is required.

## Development status

Lamium is under development. Zoom, local settings, NightVision, Shulker/Bundle
previews, durability information, and inventory sorting are implemented at
prototype quality. Runtime validation is in progress. No stable
release is available yet. See [validation notes](docs/VALIDATION.md) for the
distinction between implemented and verified behavior.
Contributors and coding agents start with [AGENTS.md](AGENTS.md); the design
rules are in [docs/DESIGN.md](docs/DESIGN.md) and open work in
[docs/BACKLOG.md](docs/BACKLOG.md).

Settings and input foundation work is underway. Known settings
and controls limitations are recorded in [UI follow-up](docs/UX-FOLLOWUP.md);
the current settings interaction is not the final design.

The current prototype provides hold-to-zoom (`C`), local settings (`L`), and a
NightVision toggle (`J`) while in a world. Settings also control container
previews, durability information, and inventory sorting. Shulker and Bundle
previews each have their own enable and show-empty switches. Shulker's vanilla
contents text can also be hidden while previews are enabled (off by default).
Use Up/Down or the wheel
to select rows, Left/Right to adjust, and Enter or a click to choose. Short windows
scroll to keep the selected row visible. Changes are saved and applied when edited;
Escape or Close only closes the panel. A failed save leaves the previous value
active and shows an error. Gameplay key hints can be hidden from the panel.
Edit feature bindings directly in Lamium Settings, either under the feature or
in the Hotkeys view. Lamium owns its bindings: an empty slot uses the default,
Clear unbinds an action, and Reset restores the default. On-screen gameplay
hints use the effective bindings.
The current source adds a translucent panel and a Search row; these changes
still require in-game verification. Click Search or select it and press Enter
to type. Enter finishes text editing; Escape leaves editing before closing.
The first row switches between Features and Hotkeys. Features groups each tool's
settings and binding under an expandable header with its state and key. Search
reveals matching settings inside collapsed groups; a description follows the
selected feature. Binding rows open an editor:
press the desired chord and release to set it, or use a wheel direction for a
Press/Toggle action. Clear unbinds; Reset restores the default; Escape
cancels. These controls and custom input dispatch are implemented but await
runtime validation. Identical custom overrides are marked Shared; conflicts with
Minecraft or other mods are not comprehensively detected yet.
An opt-in Chunk Borders prototype is also available in Features. Its world-render
backend compiles but has not yet been validated in Minecraft.
Keyboard/mouse behavior and resource-pack compatibility are still being tested.
Settings, gameplay hints, durability text, and Minecraft's Lamium key-binding labels follow the
game's language: Japanese and English are included, with English as the fallback.
Earlier prototypes used `N` for NightVision, which conflicts with Minecraft's
notification shortcut. Existing saved bindings are preserved; change NightVision
to `J` in Lamium Settings if upgrading from those builds.

Press `R` in an inventory screen to consolidate compatible stacks and arrange
the main inventory. Hover an ordinary storage-container slot to sort that
container instead. Hotbar, equipment, crafting, and machine slots are excluded.
Sorting is skipped while typing or holding an item on the cursor. The feature
can be disabled and its binding edited in Lamium Settings.
Sorting waits for matching vanilla server responses between operations and
checks the region again before continuing. Rejection, missing responses, or a
changed screen stops the plan. Real-game validation of this integration remains
a release gate.

## Build

Target: Windows x64, Minecraft 1.26.51.01, LeviLamina Client v26.51.5.
Install Visual Studio Build Tools with the Windows SDK and C++ toolchain, LLVM
(clang-cl), Git, and xmake. Dependencies are resolved by xmake; no sibling project
or machine-specific configuration is needed.

```powershell
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -y
xmake build LamiumTests
xmake run LamiumTests
./scripts/Check-Package.ps1
```

Use PowerShell 7 for the package check. `xmake-requires.lock` records the package
versions and repository revisions used for Windows x64. Keep it when cloning;
review changes from `xmake require --upgrade` before committing them.
The project includes a client SDK recipe that downloads the matching LeviLamina
source headers and official release DLL with SHA-256 verification, then generates
an import library using Visual Studio's `dumpbin` and `lib`. It does not install
or bundle that runtime. See [SDK build notes](packages/README.md).

The GitHub Actions workflow builds with xmake 3.1.1 on Windows, runs the tests and
package check, and uploads a development artifact. It does not publish releases.
Minecraft runtime checks still need a local installation; CI cannot prove them.

Use a separate launcher instance for development. Do not enable another mod that
changes the same camera or inventory behavior while testing Lamium.

## Design

Lamium writes its own diagnostics to `mods/Lamium/logs/lamium.log`, flushing
informational messages immediately so failures can be inspected while the game
is running. Logs rotate by size/date and retain at most seven archives.

One mod owns settings and input actions. Features own their transient state and
restore vanilla behavior when disabled, leaving a world, or losing input focus.
Settings and UI remain local. Inventory actions must use ordinary game operations
and verify the resulting slot contents before continuing.

## License

Copyright (C) 2026 amatouhake.

Lamium is licensed under the GNU Lesser General Public License, version 3 only
(`LGPL-3.0-only`); see [COPYING.LESSER](COPYING.LESSER) and [COPYING](COPYING).
Third-party notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
