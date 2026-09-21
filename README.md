# Lamium

Client-side quality-of-life tools for Minecraft Bedrock and LeviLamina Client.
No server plugin or companion protocol is required.

## Development status

Lamium is under development. The first milestone is a locally configurable Zoom
feature verified in game. NightVision, item previews, durability information, and
inventory sorting are planned. No stable release is available yet.

The current prototype provides hold-to-zoom (`C`) and a local camera settings
panel (`F8`) while in a world. Use arrow keys and Enter or click the rows; Save
applies changes, Cancel/Escape discards them. These keys are registered with
Minecraft's keyboard settings. The prototype's on-screen hints show defaults.
Keyboard/mouse behavior and resource-pack compatibility are still being tested.

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
```

Use a separate launcher instance for development. Do not enable another mod that
changes the same camera or inventory behavior while testing Lamium.

## Design

One mod owns settings and input actions. Features own their transient state and
restore vanilla behavior when disabled, leaving a world, or losing input focus.
Settings and UI remain local. Inventory actions must use ordinary game operations
and verify the resulting slot contents before continuing.

## License

Copyright (C) 2026 amatouhake.

Lamium is licensed under the GNU Lesser General Public License, version 3 only
(`LGPL-3.0-only`); see [COPYING.LESSER](COPYING.LESSER) and [COPYING](COPYING).
Third-party notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
