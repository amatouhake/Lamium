# Provenance

Lamium is licensed under `LGPL-3.0-only`. This file records where Lamium's
code comes from and which outside projects are only references. Legal notices
for shipped components are in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md);
this file adds the source-use boundary that agents and contributors follow.

Every project falls in exactly one of three groups. Move a project to another
group only by editing this file in the same change that starts using it that
way.

## 1. Platform and dependencies

Used as separate programs, SDKs or libraries. Their code is not copied into
`src/`.

| Project | Role | License |
| --- | --- | --- |
| Minecraft Bedrock Edition | Game the mod runs in; nothing is bundled | Proprietary (Minecraft EULA) |
| [LeviLamina](https://github.com/LiteLDev/LeviLamina) | Client mod loader, API and SDK headers; dynamically linked | LGPL-3.0 |
| LeviLauncher | Installs and launches instances | GPL-3.0 (separate program) |
| LeviBuildScript | Build-time packaging rule | External build tool |
| SDK libraries | Header-only and support libraries | See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) |

## 2. Incorporated or derived source

Code in this repository that did not start in this repository.

| Source | What came in | License / notice |
| --- | --- | --- |
| [levilamina-mod-template](https://github.com/LiteLDev/levilamina-mod-template) | Build and packaging scaffolding | CC0-1.0 |
| [LaminaView](https://github.com/amatouhake/LaminaView) | Camera integration lessons | Earlier mod by Lamium's author |
| [LaminaPeek](https://github.com/amatouhake/LaminaPeek) | Item-content readers, hover integration, preview rendering | Earlier mod by Lamium's author |
| [LaminaSort](https://github.com/amatouhake/LaminaSort) | Inventory planning, item classification, container transfers | Earlier mod by Lamium's author |
| [nlohmann/json](https://github.com/nlohmann/json) | Settings serialization | See THIRD_PARTY_NOTICES.md |

Before code from any other project enters the tree, record here: the upstream
repository and commit, the files taken, what was changed, and the upstream
copyright and license notices (including whether the license is "only" or "or
later"). Keep the original notices. Do not present incorporated code as
original Lamium code.

A future subsystem may incorporate compatible upstream source only after the
maintainer chooses that path and the exact source/license scope is reviewed.
Until then, candidate upstream implementations remain reference-only research.

## 3. Reference only

Behavior, UX and feasibility research. Do not copy, translate or closely
paraphrase their code, comments, distinctive constants, strings, assets or
implementation structure. Lamium implements the resulting requirements
independently on Bedrock and LeviLamina APIs. Hooking the same SDK function
is expected; the handler bodies must be Lamium's own.

This list names projects only so that contributors know what not to copy.
Comparisons, feature surveys and license analyses stay in the maintainer's
external research notes. Whoever writes Lamium code works from the specs in
`docs/` and does not open reference-only source (or source recovered from
it) while doing so.

- ChiyanMap (GPL-3.0; repository no longer available). Recovered source and
  notes are kept outside this repository.
- LeviSchematic (LGPL-3.0). The maintainer decided on 2026-10-03 not to
  incorporate it; Lamium's schematic subsystem (BACKLOG L-93) is written
  independently.
- Current LeviLamina client mods (licenses as checked 2026-09-28): Stipuleroo (GPL-3.0), LHolo (GPL-3.0),
  CoralFans and BedrockServerClientInterface (AGPL-3.0), Playback (AGPL-3.0),
  FastMiner (no license found), CoralMap and Dear-OreUI (CC0-1.0; permissive,
  but reference-only until the maintainer chooses to incorporate them).
- BedrockTools (an Android native mod; checked 2026-10-05: its root LICENSE
  is MIT but its README says GPL-3.0): read only as the source of the L-96
  feasibility hint. Reference-only until the project clarifies its license.
- Other Bedrock client mods such as Flarial and iInfiniteNightVision.
- GroupMountain FreeCamera (GPL-3.0; a BDS plugin): README read 2026-09-30
  as the source of the L-37 hypothesis; its source is not opened.
- Java mods: MaLiLib, Tweakeroo, MiniHUD, Litematica, Item Scroller, Client
  Sort and similar inventory sorters, Quark, Inventory Profiles Next, Mouse
  Wheelie, Jade / WAILA, AppleSkin, Xaero's Minimap and World Map.

If reference-only source later becomes incorporated source, move it to group 2
in the same change that first imports code. Record the upstream repository and
commit, files used, modifications, copyright notices and exact license scope
before merging that code.

## Review rule

Generated code is not assumed to be free of outside code. If a change contains
an unusually specific implementation that resembles a group 3 project, find its
origin before merging. Describe behavior references and source incorporation
separately in commit messages and pull requests.
