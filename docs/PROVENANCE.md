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
`src/` or anywhere else in this repository: xmake fetches them at the
versions in `xmake-requires.lock`. "Compiled in" means their headers or
static code end up inside `Lamium.dll`, so their notices ship with it.

| Project | Role | How it enters the build | License |
| --- | --- | --- | --- |
| Minecraft Bedrock Edition | Game the mod runs in; nothing is bundled | Not at all | Proprietary (Minecraft EULA) |
| [LeviLamina](https://github.com/LiteLDev/LeviLamina) | Client mod loader, API and SDK headers | Headers compiled in; `LeviLamina.dll` linked dynamically through an import library generated from the official release DLL (`packages/`) | LGPL-3.0 |
| [nlohmann/json v3.12.0](https://github.com/nlohmann/json/tree/v3.12.0) | Settings and data serialization | Header-only xmake package, compiled in | MIT; notice in THIRD_PARTY_NOTICES.md |
| [Zstandard v1.5.7](https://github.com/facebook/zstd/tree/v1.5.7) | Map region compression | Static xmake package, compiled in | BSD-3-Clause option; notice in THIRD_PARTY_NOTICES.md |
| SDK libraries | Header-only and support libraries the SDK depends on | Headers compiled in where used | See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) |
| LeviLauncher | Installs and launches instances | Not at all (separate program) | GPL-3.0 |
| LeviBuildScript 0.6.1 | Build rules: runs prelink before linking, writes `manifest.json` | Build time only | No license file found (checked 2026-10-20) |
| prelink v0.8.8 and [bedrock-runtime-data](https://github.com/LiteLDev/bedrock-runtime-data) v26.60.5-client.9 | Generate `bedrock_runtime_api.lib`, the import library for the delay-loaded `bedrock_runtime.dll` | Build time; only import records reach the DLL | prelink: closed-source binary release, no license file found; a LiteLDev member stated that anyone can use it ([prelink-release#2](https://github.com/LiteLDev/prelink-release/issues/2), 2026-11-06); bedrock-runtime-data: MIT |

## 2. Incorporated or derived source

Code in this repository that did not start in this repository.

| Source | What came in | License / notice |
| --- | --- | --- |
| [levilamina-mod-template](https://github.com/LiteLDev/levilamina-mod-template) | Build and packaging scaffolding, `src/app/MemoryOperators.cpp` | CC0-1.0 |
| [LaminaView](https://github.com/amatouhake/LaminaView) | Camera integration lessons | Earlier mod by Lamium's author; MIT |
| [LaminaPeek](https://github.com/amatouhake/LaminaPeek) | Item-content readers, hover integration, preview rendering | Earlier mod by Lamium's author; MIT |
| [LaminaSort](https://github.com/amatouhake/LaminaSort) | Inventory planning, item classification, container transfers | Earlier mod by Lamium's author; MIT |
| [Issue #70](https://github.com/amatouhake/Lamium/issues/70) attachment, by MatchaMustard | `tests/Issue70Structure.h`: the bytes of a 5x5x5 structure the game saved, and the same bytes inflated. Test data only, not in `Lamium.dll` | No license stated; game output, kept as a test of the file format |

Earlier mods, checked once on 2026-10-20 (BACKLOG L-196 P3) at LaminaPeek
`9d1f5f8`, LaminaSort `6112b79` and LaminaView `7a4f0f4`: each repository's
LICENSE is MIT, copyright 2026 amatouhake. LaminaSort and LaminaView have
commits by amatouhake only. LaminaPeek was started from a clone of
levilamina-mod-template (template head `5eb4780`, 2026-09-14), so its history
begins with the template's own commits by LiteLDev contributors (the
template was MIT, copyright LiteLDev, from 2023-11-23 and CC0-1.0 from
2023-12-10); every commit after that start is by amatouhake. Of the
template's files only build scaffolding and the two-line memory-operator
file came into Lamium. Some commits in all three name an AI coding agent as
co-author; that label is not a third-party contribution. This is a record of
the repositories' history, not a similarity audit.

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

Specs describe Lamium's behavior in Lamium's terms, not "like mod X". Name
another mod or prior-art project only here, as a one-line source for a
Research hypothesis, in a clean-room boundary ("do not open X"), or in a
dated decision record of what the maintainer referred to. Tools and
platforms Lamium depends on (LeviLamina, LIP, Bedrinth) are named freely.
Do not rewrite historical records (BACKLOG-DONE.md, VALIDATION-LOG.md).
(Moved from AGENTS.md on 2026-11-14.)

- ChiyanMap (GPL-3.0; repository no longer available). Recovered source and
  notes are kept outside this repository.
- LeviSchematic (LGPL-3.0). The maintainer decided on 2026-10-03 not to
  incorporate it; Lamium's schematic subsystem (BACKLOG L-93) is written
  independently.
- [BarelyEnoughItems (BEI)](https://github.com/MiracleForest/BarelyEnoughItems)
  (README and COPYING.LESSER: LGPL-3.0-or-later, © LeviMC). Reference-only
  since 2026-11-17 (maintainer): both projects build item and recipe
  features, so Lamium's item rendering and recipe work
  ([L-359](backlog/L-359.md)) are written from the game and the SDK headers
  without opening BEI's source, whatever the license allows. Opening it
  again takes a new maintainer decision recorded here.
  Before that: reference-only until 2026-11-14 (named on 2026-11-13 as a
  comparison for Lamium's item-icon rendering, L-91; its source read only
  in the maintainer's external research). From 2026-11-14 to 2026-11-17
  the maintainer allowed its source to be opened, from upstream commit
  `2d4facf`, for item rendering and at first also recipe work. The only
  code that came from it was the L-91 glint trial (`8e00a4b1`, 2026-11-14):
  from BEI's `src/ui/gfx/Painter.cpp` (no copyright or SPDX header in the
  file), the three ways its draw context differs around the same two
  `renderGuiItemNew` calls (an off-screen white quad flushed with
  `ui_flush`, zOrder 0, the foil at the icon's alpha). None fixed the
  shield glint, and the next commit (`5e248bde`) removed that code; nothing
  from BEI is in the tree. Lamium's ItemIcon, up to and including Lamium
  commit `34c55be2`, predates BEI's first release (2026-11-01) and was
  written independently from the game's own slot rendering. BEI's
  AGENTS.md says its authors read Lamium for ideas, and its item-icon path
  (`7ed0718`, 2026-10-20) has the same structure as Lamium's earlier
  `ItemIcon` (L-119, L-142); the enchanted shield, enchanted leather armor,
  recovery compass and not-yet-loaded heads fail the same way in BEI
  (maintainer, 2026-11-14).
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
  Wheelie, Jade / WAILA, AppleSkin.
- A public Java minimap/world-map mod, identified only in the maintainer's
  external research notes: its publicly described split between broad
  Nether viewing and height-focused cave viewing is the source of the L-228
  question. Its source is not opened; no code, strings, assets or UI layout
  are used.
- Continuity (Java): its design as described publicly is the source of the
  L-96 layering idea (connection rules apart from texture methods, a
  47-tile method later) and of L-164's question (one quad per face with a
  chosen sprite instead of split quads). Its source is not opened.
- Librarian Trade Finder (Java, GPL-3.0): its repository description,
  named by the maintainer 2026-10-12, is the source of the L-168 idea.
  Its source is not opened.
- enchant-order (a web tool, MIT): the problem it solves, named by the
  maintainer 2026-10-12, is the source of the L-170 idea, and its two
  publicly offered goals (fewest levels, least prior work), named by the
  maintainer 2026-11-12, of L-170's two goals. Its source is not opened;
  Lamium's anvil rules are checked against the game.
- Quick Container (a Bedrock resource pack on CurseForge; its terms forbid
  using its code without permission): named by the maintainer 2026-10-15 as
  evidence that Bedrock can move everything or every matching item at once.
  Its files are not opened. Only the game's behavior while the maintainer
  uses it is observed, through Lamium's `transfer_trace` (vanilla handlers,
  button ids, request timing), for L-169.
- Item Scroller (Java, LGPL-3.0-only): the key list in its public Modrinth
  description, read 2026-10-12 at the maintainer's request, is the source
  of L-169's gesture proposal. Its source is not opened. Its public
  description of favourite trades, named by the maintainer 2026-10-14, is
  also a source of the L-166 part 2 proposal.
- Autotrade Plus and Villager Trading Overhaul (Java; licenses not
  checked): their public descriptions, named by the maintainer 2026-10-14,
  are sources of the L-166 part 2 proposal (running favourites, remaining
  uses, trade caps). Their source is not opened.
- CalcMod (Java, MIT): its Modrinth description, named by the maintainer
  2026-10-12, is the source of the L-171 idea. Its source is not opened.
- Sodium, ImmediatelyFast, Lithium and EntityCulling (Java): their published
  approaches are the source of the L-105 hypotheses (mesh rebuild
  scheduling, batching, caching, occlusion culling). Their source is not
  opened.
- Tweakeroo (Java, listed above): its fake-sneak placement, named by the
  maintainer 2026-11-01 as the closest prior art, is the source of the L-293
  idea. Its source and description text are not opened.
- JEI (Just Enough Items) and EMI (Java recipe viewers; both MIT per their
  GitHub repositories, checked 2026-11-17): named by the maintainer
  2026-11-17 as the source of the L-359 idea, from what they do as the
  player sees it (looking up an item's recipes and uses; tracing an item
  down to its base materials with a recipe chosen per step). Their source
  is not opened; no code, strings, assets or screen layout are used.
  On 2026-11-20 the maintainer also referred to EMI's public pages (README,
  wiki, changelog, the descriptions in the config file it writes) for the
  L-371 direction; its source is still not opened. On 2026-11-24 JEI's
  public wiki pages on item subtypes were read for L-363 (what makes two
  stacks of one item different); its source is not opened.
- The Bedrock slime chunk rule (L-232, `features/information/SlimeChunk.h`):
  MT19937 seeded with `uint32(chunkX) * 0x1f1f1f1f ^ uint32(chunkZ)`, slime
  chunk when the first output `% 10 == 0`. It is a fact about the game, as
  publicly documented (the Minecraft Wiki's Slime article and issue #30 by
  its reporter, who states it is not from another mod's code). The reporter's
  snippet was not copied; Lamium's function computes the first MT19937 output
  from the standard's seeding and tempering, and the tests compare it with
  `std::mt19937`. Checked against the game's own
  `SlimeSpawnRules::checkSpawnRulesCallback` by the `slime_trace` build (see
  VALIDATION-LOG).

If reference-only source later becomes incorporated source, move it to group 2
in the same change that first imports code. Record the upstream repository and
commit, files used, modifications, copyright notices and exact license scope
before merging that code.

## Review rule

Generated code is not assumed to be free of outside code. If a change contains
an unusually specific implementation that resembles a group 3 project, find its
origin before merging. Describe behavior references and source incorporation
separately in commit messages and pull requests.
