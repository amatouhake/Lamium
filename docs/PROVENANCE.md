# Provenance

Lamium is licensed under `LGPL-3.0-only`. This file records where Lamium's
code comes from and how outside projects may be used as references. Legal notices
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

A future subsystem may incorporate upstream source only after the maintainer
chooses that path and reviews compatibility, required distribution terms,
notices and whether Lamium's declared license must change.
Until then, candidate upstream implementations remain reference-only research.

## 3. Reference only

Reference-only means that no upstream code is incorporated into Lamium. It
does not always mean that source is unreadable. Each project is assigned one
of the two boundaries below. In both, do not copy, translate or closely
paraphrase code, comments, distinctive constants, strings, assets or
implementation structure. Lamium implements the resulting requirements
independently on Bedrock and LeviLamina APIs. Do not reproduce upstream
function decomposition, branching, naming or other expressive implementation
choices. Protocol- or SDK-required operations are verified independently
against official headers or documentation and implemented from those facts.

Under this policy, reading alone is not incorporation. Any proposal to copy or
adapt upstream expression moves to group 2 before that work begins. The
maintainer must review license compatibility, required distribution terms,
notices and whether Lamium's declared license must change before source is
copied or adapted.

### 3a. Behavior-only reference

Use public descriptions, screenshots, observed behavior and the maintainer's
notes. Do not open the project's source. This is the default for projects with
no identified license, recovered source with an intentionally strict boundary,
or work where source details are unnecessary.

- ChiyanMap (GPL-3.0; repository no longer available). Recovered source and
  notes are kept outside this repository.
- LeviSchematic (LGPL-3.0), until incorporated as described in group 2.
- Current LeviLamina client mods (licenses as checked 2026-09-28): LHolo
  (GPL-3.0), CoralFans and BedrockServerClientInterface (AGPL-3.0), Playback
  (AGPL-3.0), FastMiner (no license found), CoralMap and Dear-OreUI (CC0-1.0;
  permissive, but reference-only until the maintainer chooses to incorporate
  them).
- Other Bedrock client mods such as Flarial and iInfiniteNightVision.
- Java mods: MaLiLib, Tweakeroo, MiniHUD, Litematica, Item Scroller, Client
  Sort and similar inventory sorters, Quark, Inventory Profiles Next, Mouse
  Wheelie, Jade / WAILA, AppleSkin, Xaero's Minimap and World Map.

### 3b. Source-inspected reference

Source may be opened for a named, bounded Research question when behavior
alone is insufficient and the project is explicitly approved below. Before
reading implementation files, add an inspection record with the upstream URL,
exact commit or release, license evidence at that revision (including `only`
or `or later` when applicable), the question, planned scope and research-note
path. Repository metadata, license files and the file tree may be inspected
only to pin that information and narrow the scope. After inspection, record
the exact files read, date and completed status. Any resulting API facts must
be verified independently against official SDK headers or documentation before
implementation.

Record only the minimum observable behavior and API facts needed for an
independent implementation: SDK type and function names, slot or protocol
meanings, response semantics, and ordering required by the SDK or protocol.
Cite upstream file and line locations without quoting. Distinguish facts seen
only in upstream source from facts confirmed in official SDK headers or
documentation. Do not record upstream branching, helper decomposition,
state-machine shape or source-derived pseudocode. Verbatim excerpts, close
paraphrases and source screenshots do not enter the repository, an
implementation prompt, a pull request or an issue.

Finish the research note and close the upstream source before implementing.
The implementer works from Lamium's specification, those API facts and the
official SDK headers. Prefer a different agent or fresh context; when the same
person implements, keep the research and implementation as separate passes.
Review unusually similar structure before merging.

Approved for source-inspected research, but not yet inspected:

- Stipuleroo (GPL-3.0): bounded feasibility research for L-66 Hand Restock
  and L-67 weapon selection. An inspection record is still required before
  its implementation files are opened. Inspection may identify SDK types and
  the transaction flow used without a screen. It does not authorize
  implementing or sending a client-built transaction; the L-66 maintainer
  checkpoint still applies.

Inspection records:

| Status / date | Project and upstream URL | Revision | License evidence | Question and planned scope | Exact files read | Findings location |
| --- | --- | --- | --- | --- | --- | --- |
| _None yet_ | | | | | | |

If reference-only source later becomes incorporated source, move it to group 2
in the same change that first imports code. Record the upstream repository and
commit, files used, modifications, copyright notices and exact license scope
before merging that code.

## Review rule

Generated code is not assumed to be free of outside code. If a change contains
an unusually specific implementation that resembles a group 3 project, find its
origin before merging. Describe behavior references and source incorporation
separately in commit messages and pull requests.
