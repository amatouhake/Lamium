# Lamium

Client-side quality-of-life tools for Minecraft Bedrock and LeviLamina Client.
No server plugin or companion protocol is required.

## Development status

Lamium is under development. Zoom, local settings, NightVision, Shulker/Bundle
previews, durability information, and inventory sorting are implemented at
prototype quality. Runtime validation is in progress. No stable
release is available yet. See [validation notes](docs/VALIDATION.md) for the
distinction between implemented and verified behavior.

The current prototype provides hold-to-zoom (`C`), local settings (`F8`), and a
NightVision toggle (`N`) while in a world. Settings also control container
previews, durability information, and inventory sorting. Use Up/Down or the wheel
to select rows, Left/Right to adjust, and Enter or a click to choose. Short windows
scroll to keep the selected row visible. Save applies changes; Cancel/Escape
discards them. These feature keys are registered with
Minecraft's keyboard settings. The prototype's on-screen hints show defaults.
Keyboard/mouse behavior and resource-pack compatibility are still being tested.

Press `R` in an inventory screen to consolidate compatible stacks and arrange
the main inventory. Hover an ordinary storage-container slot to sort that
container instead. Hotbar, equipment, crafting, and machine slots are excluded.
Sorting is skipped while typing or holding an item on the cursor. The feature
can be disabled in Lamium settings; its default key can be remapped in Minecraft.
Sorting waits for matching vanilla server responses between operations and
checks the region again before continuing. Rejection, missing responses, or a
changed screen stops the plan. Real-game validation of this integration remains
a release gate.

## Build

Target: Windows x64, Minecraft 1.26.51.01, LeviLamina Client v26.51.3.
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
review changes from `xmake require --upgrade` before committing them. The current
LeviLamina tag uses the pinned repository's `main` dependency mapping because
that repository has no version-specific entry for the tag.

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
