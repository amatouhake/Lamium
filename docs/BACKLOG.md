# Lamium backlog

Work the maintainer has decided to pursue. Ideas are discussed first and
enter this file only once they are to be worked on; each gets the next free
`L-` number, a kind and its open questions. A task with an open user-visible
choice is **Design**, whoever writes it down.

Each task has a **kind**, which decides who should pick it up:

| Kind | Meaning | Who |
|---|---|---|
| **Ready** | Spec is complete; mostly pure logic + tests + small glue | Any agent, including cheap models |
| **Design** | A user-visible choice is open. Output is a spec (often a web demo) that turns into Ready tasks | Maintainer + strong model |
| **Research** | Needs native reverse engineering, trace builds or runtime-driven debugging | Strong model; cheap models may only collect traces |
| **Refactor** | Preserves behavior while changing internal structure | Follow the item's model and validation requirements |

Ready tasks marked **(strong model)** are fully specified but visual or
cross-cutting enough that a strong model should implement them.

A large or open-ended feature (Map, placement and breaking features,
Schematic) starts with a conversation with the maintainer about what it
should be - purpose, scope, what is left out - before any spec, spike or
mockup (decided 2026-09-28).

**Bugs** (something that ships behaves wrongly) are listed first in their own
section and are fixed before new features. Each bug still has a kind that
decides who picks it up.

### How items are kept (decided 2026-11-14)

This file is the index: policy, execution order and one line per open item.
Each item is its own file, `backlog/L-<n>.md`; read the index, then only the
item you work on. Finished, closed and parked items move to `backlog/done/`
([BACKLOG-DONE.md](BACKLOG-DONE.md) lists them). An `L-` number that is not
in `backlog/` is in `backlog/done/`; `git grep -n "L-166" docs/backlog`
finds either.

An item file says what is true now, not how it got there. Rewrite it when
something changes; do not append a dated round. What was built goes in the
commit message, what was seen in game in VALIDATION-LOG.md, and long
contracts in the feature document (MAP.md, SCHEMATIC.md, ...). The old text
stays in `git log -p` of the file. Shape (keep it under about 60 lines):

```markdown
# L-<n> <title>

Kind: Ready | Design | Research | Refactor (Bug: ...) (strong model)
Status: <one line: Open / Waiting for the maintainer / Built, not checked in game / ...>

<Goal: what the player gets, or what is wrong, in a few lines.>

## Decided
<The rules agreed so far, current wording only.>

## Open
<Choices still to agree, questions to research.>

## Next
<The next concrete step, and what it waits on.>

## Known (optional)
<Facts found in the game or SDK that the work relies on.>

## Tried (optional)
<Approaches that failed, one line each with why, so nobody repeats them.>

## Links
<Feature document, mockup, VALIDATION row.>
```

`scripts/Check-Backlog.ps1` (CI) checks that the index and the files agree
and that this file and each item stay short.

## Release policy (decided 2026-09-28)

Lamium is developed at the maintainer's pace, for their own use first, and
published for whoever wants it. There is no "first release" gate.

- Release when a meaningful set of changes has landed and main builds, passes
  the tests and has no known crash. Before tagging, smoke-test the release
  build (the mod loads, settings open, the changed features work) and update
  the README feature tables and Known issues, and [FEATURES.md](FEATURES.md).
  A full regression is not required. From 0.4.0 the smoke test uses the
  commit's CI package and the tag's CI run attaches the assets to a draft
  release ([DISTRIBUTION.md](DISTRIBUTION.md#release-flow-l-298-from-032)).
- From 0.1.4 on, releases are ordinary GitHub releases, not marked
  pre-release: the 0.x version, the README status and the Experimental badges
  say what is unfinished.
- Pushing a `v<version>` tag is public distribution. The LIP registry picks up
  every semver `v*` tag, including `-rc.N` style prerelease tags, whatever the
  GitHub pre-release flag says, and LeviLauncher offers it. A build meant only
  for testing is shared as a CI artifact or branch build without a tag, not as
  a GitHub pre-release.
- The version is set in `xmake.lua` and `tooth.json`. The asset is
  `Lamium-<version>-client-windows-x64.zip`, built by
  `scripts/New-ReleaseArchive.ps1` (never by hand); the folder inside stays
  `Lamium/`, and release notes name the asset the same way. The LIP registry
  adds each `v*` tag through a registry PR, so Bedrinth and LeviLauncher pick
  up a release without a registration step. Package rules and the release
  checklist: [DISTRIBUTION.md](DISTRIBUTION.md).
- The Experimental badge comes off a feature when all three hold (decided
  2026-11-13): it has been in two or more releases, no bug item is open
  against it, and no change to how it behaves is planned. The maintainer
  decides per feature; meeting the three is what makes it a candidate.
- Large features may start at any time. They land on main in steps, default
  off and with the Experimental badge, so main stays releasable while they
  grow. A step that is not usable yet stays out of the settings screen (or
  behind a build option) instead of being shown half-working.

Experimental badge decisions (moved from DESIGN.md Components on 2026-11-14):

- Freelook, Auto Attack, Auto Use, Light Level Overlay, Permanent Sneak and
  Permanent Sprint have no Experimental badge after maintainer use. For
  0.3.0 (decided 2026-10-22) FreeCamera, Hand Restock, Tool Protection, Auto
  Elytra and Edge Guard lose it too after server use; their remaining limits
  stay in the README's known issues (Edge Guard: rare drops under heavy lag
  or a very small margin). Inventory transfer keeps it and is off for new
  installs (a saved on stays on); none of the features reviewed for 0.3.0
  keeps the badge while on by default. Radar and Waypoints remain on inside
  the Experimental Map. Which features carry the badge now is the
  `experimental` flag in `SettingsRows.h` (2026-11-14: Block highlight,
  Inventory transfer, Death layout restore, Enchantment order, the Map
  features and Schematics); when it comes off is BACKLOG "Release policy"
  (decided 2026-11-13).
- (Decided 2026-11-13) The badge comes off when a feature has been in two
  or more releases, has no open bug item and no planned change of behavior
  (BACKLOG "Release policy"). By that rule Hide effects,
  Fake Offhand, Sneak placement, Slime chunks, Game language and Skip
  server resource packs lose it. Kept: the maps, map marks and Schematics
  (open bugs), Inventory transfer (L-299), Death layout restore (L-240),
  Waypoints (L-328 changes the death point), Radar (its mob icons are to be
  fixed, L-330), Block highlight and Enchantment order (too new). Connected Textures lost
  it, got it back the same day over its glass pane edges (L-331) and lost
  it again once that was fixed and confirmed.

## Current execution order

Keep this section short: ordering lives here; status, contracts and open
choices live in the L-items. An L-item wins if a summary drifts.

1. **Bugs first.** Open: L-343 (a server's map, waypoints and placements lost
   when its address changes; Research), L-335 (rows under a switched-off feature dim only in
   the middle; Ready), L-339 (smaller bugs from the 2026-11-14 code review;
   Ready), L-299 (a slot stuck after a screenless move when client and server
   desync), L-240 (death layout restore acts unexpectedly; Research), L-324
   (custom blocks brown on the map; Research), L-227 (FreeCamera shows only
   the sky outside loaded terrain; Research), L-243 terrain gaps, L-245 ghost
   face beside glass, L-257 FreeCamera culler thread, L-117 schematic name
   tags. What waits for more game symbols is collected in L-315.
   Parked: L-194 save corner marks, L-123 elytra pose, L-91 shield glint,
   L-132 inventory-screen movement, L-244 FreeCamera started inside a block,
   L-287 minimap arrow look; respect each item's stop/resume conditions. L-181
   trident pose is low priority.

2. **Small and medium work before large features/integration** (maintainer
   2026-10-10): the L-266 schematic follow-up batch closed with 0.3.1 except
   L-265 placement assist (Design). Decisions waiting for the maintainer:
   none. Built, with open checks
   or follow-ups in their items: L-338 death layout time limit, L-170
   enchantment order planner, L-319 item
   details, L-320 block highlight (released in 0.6.0). Remaining
   Design/Research: L-336 Durability HUD items, L-337 minimap size, L-340
   settings grouping, L-341 shared widgets (Refactor), L-325 zoom release,
   L-330 radar icons, L-166 trade helpers, L-167 resource packs, L-168 trade
   finder, L-169 transfer extensions, L-173 quick shapes, L-190 list
   selection, L-163 a Lamium inventory screen, L-249 simulation distance and
   L-317 (Nether minimap under forced Surface). Agree open choices before
   building. L-165 Mass Craft remains below Map and needs a scope conversation;
   existing packs already cover part of that need.
3. **Schematic follow-ups (L-93):** choose among L-114/L-115/L-116,
   L-183/L-184/L-185/L-186 and the remaining name-tag issue L-117.
   The initial feature and 0.1.8 work are complete, not new tasks.
4. **Placement/breaking:** L-15 breaking redesign before placement;
   L-59 held-placement style waits for the maintainer's go-ahead.
5. **Optional integration (L-111):** shared spatial actions, a common menu,
   selection and cross-links; the required 0.2.0 steps are complete.
6. **Research when convenient:** L-79 pumpkin/spyglass frames, L-71 glide
   initiation, L-30 dragon hitboxes, L-33 mob timers, L-105 profiling.
   L-151/L-152/L-153 and the previously parked work remain later.
7. **Before the next release:** changed-feature smoke and the relevant
   [pre-release checks](backlog/pre-release-checks.md). Latest release: 0.6.0, 2026-11-12 (`v0.6.0`). A full
   regression is not a release gate.

Task-picking rule: bugs first, then this order; follow each item's model,
dependencies and validation requirements. Cheap models skip Design,
Research and strong-model work. A finished item's file moves to `backlog/done/`;
remaining runtime gaps go to the pre-release checks and VALIDATION.md.

## Pre-release checks

Known gaps to pick release checks from: [backlog/pre-release-checks.md](backlog/pre-release-checks.md).

## Bugs

- [L-343](backlog/L-343.md) A server's map, waypoints and placements are lost when its address changes (Bug)
- [L-335](backlog/L-335.md) Rows under a switched-off feature are dimmed only in the middle (Ready)
- [L-339](backlog/L-339.md) Smaller bugs found by the 2026-11-14 code review (Ready)
- [L-342](backlog/L-342.md) Lower findings of the 2026-11-14 review, kept for later (mixed)
- [L-324](backlog/L-324.md) Map: custom blocks are drawn in a dirt-like brown (Research)
- [L-299](backlog/L-299.md) Screenless inventory moves can leave a slot stuck when client and server desync (Research)
- [L-243](backlog/L-243.md) Large terrain gaps that only reload when the player comes near (Bug / Research)
- [L-244](backlog/L-244.md) FreeCamera keeps the inside-a-block view after starting in a block (Bug)
- [L-245](backlog/L-245.md) Ghost face disappears next to real glass or a beacon from some angles (Bug / Research)
- [L-257](backlog/L-257.md) FreeCamera culler reads blocks on the render thread (Research)
- [L-240](backlog/L-240.md) Death layout restore sometimes restores unexpectedly or wrongly (Bug / Research)
- [L-227](backlog/L-227.md) FreeCamera outside the loaded area shows only the sky (Bug / Research)
- [L-181](backlog/L-181.md) FreeCamera draws the held trident the wrong way round (Bug / Research)
- [L-123](backlog/L-123.md) FreeCamera stretches the worn elytra on the inventory player model (Bug)
- [L-117](backlog/L-117.md) Schematic entity name tags render badly in Japanese (Bug)
- [L-194](backlog/L-194.md) Save corner marks over real glass or water change with the camera (Bug / Research)

## Feature work and research

- [L-338](backlog/L-338.md) When a death layout ends (Feature)
- [L-336](backlog/L-336.md) Durability HUD: choose its items in HUD layout (Design)
- [L-337](backlog/L-337.md) Minimap: one size setting instead of two (Design)
- [L-340](backlog/L-340.md) Settings grouping (Design)
- [L-341](backlog/L-341.md) Shared widgets for repeated controls (Refactor)
- [L-330](backlog/L-330.md) Radar: mob icons that draw wrongly, then icons by default (Research)
- [L-325](backlog/L-325.md) Zoom: return to the set magnification on release (Design)
- [L-320](backlog/L-320.md) Highlight designated block types in the world (Design)
- [L-319](backlog/L-319.md) Item details: Work Penalty first, then the unshown rest (Feature)
- [L-317](backlog/L-317.md) Forcing the Surface view leaves the minimap nearly useless in the Nether (Design)
- [L-315](backlog/L-315.md) Finish what waits for more game symbols (Blocked on a symbol request)
- [L-249](backlog/L-249.md) Show the chunks within simulation distance (Design / Research)
- [L-265](backlog/L-265.md) Schematic placement assist: place the ghost's block in its state (Design / Research)
- [L-233](backlog/L-233.md) Make inventory transfer gestures rebindable (Design)
- [L-236](backlog/L-236.md) Rebuild only the ghost sections whose visible layer changed (Design)
- [L-190](backlog/L-190.md) Select several rows at once in list screens (Design for the whole)
- [L-183](backlog/L-183.md) Temporarily disregard selected schematic check mismatches (Design)
- [L-184](backlog/L-184.md) Inspect ghost block data and schematic container contents (Design / Research for saved data inspection)
- [L-185](backlog/L-185.md) Instantly load a schematic into the world when cheats allow it (Design)
- [L-186](backlog/L-186.md) Improve schematic ghost legibility and reconsider translucency (Design / Research)
- [L-173](backlog/L-173.md) Quick shapes: a beacon's range at a glance, shapes that are not saved (Design)
- [L-170](backlog/L-170.md) Enchantment order planner (Feature)
- [L-169](backlog/L-169.md) Item Transfer: remaining gesture and screen extensions (Design)
- [L-168](backlog/L-168.md) Trade finder: reroll a villager until a wanted trade appears (Design)
- [L-167](backlog/L-167.md) Change the language and resource packs from inside a world (Research)
- [L-166](backlog/L-166.md) Villager trade helpers: favourites and assisted trading (Design)
- [L-165](backlog/L-165.md) Mass Craft: craft a recipe many times at once (Design)
- [L-163](backlog/L-163.md) A Lamium-drawn inventory screen (Research)
- [L-132](backlog/L-132.md) Move while the inventory screen is open (Research)
- [L-59](backlog/L-59.md) Held placement style: vanilla, Java-like or fast (Design done)
- [L-15](backlog/L-15.md) Breaking and placement restrictions (Design)
- [L-91](backlog/L-91.md) Shield glint missing in Lamium's item icons (Research)
- [L-79](backlog/L-79.md) Carved pumpkin and spyglass frame draw path (Research)
- [L-71](backlog/L-71.md) Start an elytra glide from the mod (Research)
- [L-93](backlog/L-93.md) Schematic: load, place, project, verify and list materials (experimental) (Follow-up tracking)
- [L-30](backlog/L-30.md) Ender Dragon multipart hitboxes on Bedrock
- [L-33](backlog/L-33.md) Mob growth and breeding timers in the target card
- [L-114](backlog/L-114.md) 3D previews in the schematic screen (Research)
- [L-115](backlog/L-115.md) Entity ghosts drawn as models (Research)
- [L-116](backlog/L-116.md) Raw materials from the game's recipes (Research)
- [L-111](backlog/L-111.md) Integration between features: remaining shared controls (Design)
- [L-105](backlog/L-105.md) Performance: find the real bottleneck before optimizing (Research)

## Later / parked

- [L-329](backlog/L-329.md) Health a food would give back, beside its food values (idea)
- [L-151](backlog/L-151.md) Show the player's body in first person (Research)
- [L-152](backlog/L-152.md) Preview held map items in inventory (Design)
- [L-153](backlog/L-153.md) Prefer the last pressed opposing movement key (Design)
- [L-282](backlog/L-282.md) Toggle Vibrant Visuals with a key (Research)
- [L-283](backlog/L-283.md) Stronger block outline and a crosshair face highlight (Design)
- [L-284](backlog/L-284.md) Custom crosshair (Design)
- [L-287](backlog/L-287.md) Make the minimap arrows legible at every map size (Design)

### Previously parked work

- L-19 Freelook in multiplayer, riding, dimension change, controller: runtime
  checks only, no code expected.
- L-25 Detached-camera interaction options (parked, after L-18; agreed
  2026-09-24, implement only once FreeCamera proves viable). While detached,
  attack/use are fully blocked and clicks still swing the arm. Desired shape:
  per-feature detail settings, FreeCamera x {attack, use/place/interact,
  break} and Freelook x {attack, use/place/interact, break} (6 toggles,
  all default off). Aim follows the body, never the detached view; state
  this in the help text. Movement freeze (FreeCamera) and movement keep
  (Freelook) stay non-optional. Swing suppression (no arm swing while
  detached) is a separate Research item: find the swing trigger first.
- L-29 Hide the hotbar while detached (parked, after L-18). Requested
  2026-09-24: an option to hide the hotbar while FreeCamera is active
  (looking-only flight needs no hotbar). Find the vanilla hotbar render entry
  first; Freelook is out of scope unless trivially shared.
- L-86 Radar faces follow-up (after L-85; noted 2026-10-02): mobs without
  a "head" part (silverfish, tadpole) could use the whole model seen from
  the front; rotated head bones (camel, hoglin) need the bone rotation in
  the front view; check the remaining mob kinds (only about 25 of 80+ were
  seen); decide whether faces become the default once they hold up.
- L-21 Shape color picker or more colors: only if the four colors prove
  insufficient.
- External PR #10 (Spanish localization and map daylight tint, opened
  2026-10-07): the maintainer asked on 2026-10-08 for one PR per feature,
  translations updated to the latest main, Night Vision and unaffected
  markers for the map tint, and the provenance statement. Nothing is
  discussed further until it is split.
- Mass Craft became L-165 (2026-10-12). (Schematic became L-93; Fast
  Attack/Use became L-34; Scroll Transfer became L-41.)
