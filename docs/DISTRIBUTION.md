# Distribution and managed updates

This document defines Lamium's packaging and update contract.

## Current status

Lamium is installable from GitHub Releases as a client-only Windows x64
package. The repository carries a LIP v3 `tooth.json`, and Bedrinth already
lists Lamium (seen 2026-09-28 with versions 0.1.1-0.1.3). Managed install and
update behavior has not been validated yet.

How Bedrinth indexes packages (its bot source, `LiteLDev/bedrinth-api`,
checked 2026-09-28):

- It finds repositories by GitHub code search for a root `tooth.json`; there
  is no registration step. Package metadata comes from `tooth.json` at `HEAD`.
- Versions come from the Go module proxy's list of `v<semver>` tags, each read
  with the `tooth.json` at that tag. The GitHub pre-release flag is ignored, so
  every pushed version tag is offered to users.
- The listed LeviLamina requirement is read from the exact dependency key
  `github.com/LiteLDev/LeviLamina`; the `#client` key Lamium (like other
  client mods) uses is not shown there. Display only.

Until L-65 is complete, GitHub Releases remain the documented fallback and
managed installation must not be described as verified.

## Distribution channels

Target order once validated:

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
- `Lamium.pdb` when included
- `manifest.json`
- `COPYING`, `COPYING.LESSER`
- `THIRD_PARTY_NOTICES.md` and bundled license texts
- static resources intentionally shipped with Lamium

Runtime-owned examples:

- `config/`, including `config/settings.json`
- `logs/`
- future per-world or user-created data unless a feature explicitly defines a
  migration owned by Lamium

Release archives must not contain runtime-owned state. The package check already
rejects `config/` and `logs/`.

Prefer leaving runtime-created files outside package ownership. Use explicit
preservation metadata only if managed-update validation proves that the normal
ownership model is insufficient.

LIP (`futrime/lip`, source read 2026-09-28) records each file it places and
uninstall deletes only those files, so runtime-created files are left alone.
`preserve_files` only exempts package-placed files from that deletion and is
matched against the file name, not the path. It is not a way to protect
runtime-created settings. Whether an update is uninstall + install, and which
LIP build LeviLauncher uses, is still to be checked in L-65.

## Version and asset contract

A release is valid only when these agree:

- Lamium version in `xmake.lua`
- `tooth.json` version
- Git tag `v<version>`
- release asset `Lamium-<version>-client-windows-x64.zip`
- the version shown by the built mod

The archive contains a top-level `Lamium/` directory that installs to
`mods/Lamium/`.

`tooth.json` declares Lamium as a Windows x64, client-only package and pins
the supported LeviLamina Client range. Compatibility changes must update the
manifest and user-facing support text together.

CI should reject version or asset-contract drift before a release is tagged.

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

L-65 is not complete until the current Bedrinth/LIP path has been tested with a
real release pair.

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
- version sources agree;
- the expected release asset exists with the expected directory layout;
- `tooth.json` resolves that exact asset and compatible client runtime;
- release notes call out settings-schema migrations when one exists;
- a managed-update smoke test is repeated when packaging, installer behavior or
  compatibility metadata changes.

Runtime Minecraft checks remain separate from package validation. A package can
install correctly and still contain a gameplay regression.
