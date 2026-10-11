# Distribution and managed updates

This document defines Lamium's packaging and update contract.

## Current status

Lamium is a client-only Windows x64 package. The repository carries a LIP v3
`tooth.json`, and Bedrinth lists Lamium (seen 2026-09-28 with versions
0.1.1-0.1.3). In LeviLauncher an
update (0.1.2 -> 0.1.3) kept user settings, and uninstall removed the package
files but left `config/`, `logs/` and empty directories such as `licenses/`
(VALIDATION-LOG.md, 2026-09-28). LIP CLI is not checked.

How packages get listed (sources read 2026-09-28; the older
`LiteLDev/bedrinth-api` is archived and no longer describes this):

- The LIP registry `LiteLDev/lipr` runs a daily job that finds repositories by
  GitHub code search for a root `tooth.json` and, for each semver `v*` git tag
  without a manifest yet, opens a registry PR with `tooth.json` from that tag.
  After a registry maintainer merges it, the version appears in the indexes
  (Lamium 0.1.1-0.1.3 came in through lipr PRs #867, #868 and #872). Authors
  have no registration step. Prerelease semver tags count; the GitHub
  pre-release flag is not consulted.
- The Bedrinth site and LeviLauncher both read that registry
  (`lipr.levimc.org`). LeviLauncher recognizes the
  `github.com/LiteLDev/LeviLamina#client` dependency key and uses its range to
  judge compatibility with the instance and may refuse to install a version
  whose range does not match. The range in `tooth.json` therefore gates
  installs, not only display.
- The listing icon is `tooth.json` `info.avatar_url`: the raw GitHub URL of
  `assets/icon/lamium-icon-512.png` on main (L-72). The registry copies
  `tooth.json` from a tag, so a new icon URL shows after the next tag. The
  PNG comes from the SVG master through `scripts/Export-Icon.ps1`; re-run it
  and commit both when the SVG changes. The release ZIP carries no icon.
- LeviLauncher installs, updates and uninstalls Bedrinth packages through the
  LIP daemon (`internal/mcservice/lip_package.go`), not by overwriting the
  mod folder.

## Distribution channels

Recommended order (L-65, 2026-09-28):

1. **LeviLauncher / Bedrinth** — recommended discovery and install/update path.
2. **LIP CLI** — advanced and development-oriented managed install/update path.
3. **GitHub Releases ZIP** — manual install and recovery fallback.

These channels must publish the same Lamium build for a given version.

## Package ownership

Package-owned files are files shipped by the release archive and managed by the
installer. Runtime-owned files are created or changed after installation and
must not be replaced by an ordinary update.

Package-owned examples:

- `Lamium.dll`
- `manifest.json`
- `COPYING`, `COPYING.LESSER`
- `THIRD_PARTY_NOTICES.md` and bundled license texts
- static resources intentionally shipped with Lamium

Runtime-owned examples:

- `config/`, including `config/settings.json`
- `logs/`
- `schematics/`, the user-created/imported structure library
- per-server waypoint, map and schematic-placement data under `config/`
- local-world `lamium/` sidecars (shapes, waypoints, placements and map data),
  outside the installed package directory

Feature storage contracts are in [OVERLAYS.md](OVERLAYS.md), [MAP.md](MAP.md)
and [SCHEMATIC.md](SCHEMATIC.md). These remain user data unless a feature
explicitly defines a migration owned by Lamium.

Release archives must not contain runtime-owned state. The package check already
rejects `config/` and `logs/`.

Prefer leaving runtime-created files outside package ownership. Use explicit
preservation metadata only if managed-update validation proves that the normal
ownership model is insufficient.

Current LIP (`futrime/lip`, source read 2026-09-28) records each file it
places. An update uninstalls the old version and installs the new one, and
uninstall deletes only the recorded files, so runtime-created `config/` and
`logs/` survive by design. `preserve_files` only exempts package-placed files
from that deletion and is matched against the file name, not the path; it is
not a way to protect runtime-created settings. LeviLauncher's own LIP daemon
behaved this way in the 2026-09-28 update and uninstall checks.

## Version and asset contract

A release is valid only when these agree:

- Lamium version in `xmake.lua`
- `tooth.json` version
- Git tag `v<version>`
- release asset `Lamium-<version>-client-windows-x64.zip`
- symbols asset `Lamium-<version>-client-windows-x64.pdb.zip`
- the version shown by the built mod

The archive contains a top-level `Lamium/` directory that installs to
`mods/Lamium/`. From 0.2.0 it holds no `Lamium.pdb` (L-135, decided
2026-10-11): the symbols were about 90% of the archive and only matter for
crash addresses, so LIP, LeviLauncher and a manual install get the DLL alone,
and `Lamium.pdb` is released beside it, compressed, in the symbols asset. To
read a crash address, unzip it next to the DLL of the same version.

`tooth.json` declares Lamium as a Windows x64, client-only package and pins
the supported LeviLamina Client range. Compatibility changes must update the
manifest and user-facing support text together.

`scripts/New-ReleaseArchive.ps1` checks these (the tag only when CI runs on a
tag push), checks the `tooth.json` asset URL and placement, and builds the
release asset into `bin/release/` with `/` entry separators, the top-level
`Lamium/` directory, no runtime state and no PDB. It then checks that the
package's `Lamium.pdb` belongs to the DLL inside the archive (the same debug
GUID and age, `scripts/PdbIdentity.ps1`) and writes the symbols asset beside
it. CI runs it on every push, so drift fails before or at the tag.
`scripts/Check-Package.ps1` checks the same for the package folder.

## Managed update requirements

A normal managed update must:

- replace package-owned files with the new release;
- preserve runtime-owned settings and user data;
- not restore removed defaults over explicit user choices;
- not require moving Lamium settings to a nonstandard location merely to make
  updates safe;
- fail clearly rather than partially replacing the package.

Configuration migration remains Lamium's responsibility when the settings
schema itself changes. Package management preserving a file does not make an
old schema valid.

## Validation matrix

Run this in a dedicated LeviLauncher instance, never the development instance,
with two versions already in the registry. It passed for LeviLauncher on
2026-09-28 (0.1.2 -> 0.1.3, VALIDATION-LOG.md); repeat it when packaging, installer
behavior or compatibility metadata changes.

Minimum validation:

1. Clean managed install.
2. Launch Minecraft and open Lamium Settings.
3. Change at least one saved setting and one key binding.
4. Close Minecraft normally.
5. Update to a newer Lamium package through the same managed path.
6. Confirm `config/settings.json` is still present and the changed values load.
7. Confirm the new Lamium DLL/version is active.
8. Confirm package notices and static files match the new release.
9. Confirm uninstall behavior is understood and does not accidentally promise
   preservation or deletion of user data that the package manager does not
   actually provide.

Run the install/update check through LeviLauncher/Bedrinth and through LIP CLI
when both are intended as supported paths.

Also keep the manual ZIP path usable as a fallback.

## Release checklist integration

Before publishing a managed release:

- build and tests pass;
- `scripts/Check-Package.ps1` passes;
- `scripts/New-ReleaseArchive.ps1` passes, and the ZIP and the symbols ZIP
  it writes to `bin/release/` are the two assets attached to the GitHub
  release (do not zip by hand: hand-made Windows archives, 0.1.1-0.1.3,
  stored `\` separators);
- the pushed tag is `v<version>`, and its CI run passes;
- release notes call out settings-schema migrations when one exists;
- a managed-update smoke test is repeated when packaging, installer behavior or
  compatibility metadata changes.

Runtime Minecraft checks remain separate from package validation. A package can
install correctly and still contain a gameplay regression.
