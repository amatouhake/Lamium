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

Behavior, UX and feasibility research. Named research sources, URLs, license
notes and comparisons are kept in the maintainer's external research notes,
not in this repository.

Do not copy, translate or closely paraphrase reference-only code, comments,
distinctive constants, strings, assets or implementation structure. Lamium
implements the resulting requirements independently on Bedrock and LeviLamina
APIs.

If reference-only source later becomes incorporated source, move it to group 2
in the same change that first imports code. Record the upstream repository and
commit, files used, modifications, copyright notices and exact license scope
before merging that code.

## Review rule

Generated code is not assumed to be free of outside code. If a change contains
an unusually specific implementation that resembles a group 3 project, find its
origin before merging. Describe behavior references and source incorporation
separately in commit messages and pull requests.
