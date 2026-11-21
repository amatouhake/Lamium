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
and that this file and each item stay short. Each index line is generated
from its item's title, Kind and Status: after changing an item, run
`Check-Backlog.ps1 -Update`. A Ready item has no Open section; an open
choice makes it Design until the maintainer answers.

## Release policy (decided 2026-09-28)

Lamium is developed at the maintainer's pace, for their own use first, and
published for whoever wants it. There is no "first release" gate.

- Release when a meaningful set of changes has landed and main builds, passes
  the tests and has no known crash. Before tagging, smoke-test the release
  build (the mod loads, settings open, the changed features work) and update
  the README feature tables and Known issues, and [FEATURES.md](FEATURES.md).
  A full regression is not required. From 0.4.0 the smoke test uses the
  commit's CI package and the tag's CI run attaches the assets to a draft
  release ([DISTRIBUTION.md](DISTRIBUTION.md#release-flow-l-298-from-040)).
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

Experimental badge, as it stands (the `experimental` flag in
`SettingsRows.h` is the source):

- Carry it: Inventory transfer (L-299; off for new installs, a saved on
  stays on), Death layout restore (L-240, L-338), Enchantment order (not
  released yet), the Map features (minimap, map marks, Radar for L-330,
  Waypoints, world map) and Schematics (open bugs).
- Settled without it: everything else. Remaining limits stay in the
  README's known issues (Edge Guard: rare drops under heavy lag or a very
  small margin). Block highlight lost it on 2026-11-16 (maintainer, ahead of
  its second release).

## Current execution order

Keep this section short: it orders the work and says nothing else. Kind
and status are in each item's file and on its index line below (generated
from the file); never restate them here.

1. **Bugs first**, in the order of the Bugs list below. Parked bugs (L-194
   save corner marks, L-123 elytra pose, L-91 shield glint, L-132
   inventory-screen movement, L-244 FreeCamera started inside a block, L-287
   minimap arrow look) wait for their item's resume condition; L-181 trident
   pose is low priority. What waits for more game symbols is collected in
   L-315.
2. **Small and medium work before large features/integration** (maintainer
   2026-10-10), in this order: the follow-ups of L-170 enchantment order
   planner and L-319 item details; then
   L-340 settings grouping, L-330 radar icons, L-166 trade helpers, L-167
   resource packs, L-168 trade finder, L-169 transfer extensions, L-173
   quick shapes, L-163 a Lamium inventory screen, L-249 simulation distance
   and L-265 placement assist.
   Agree open choices before building.
   L-359 material plan (with L-116 raw materials) runs beside this list,
   neither ahead of it nor behind it; its next steps are in L-371's order.
3. **Schematic follow-ups (L-93 Schematic):** choose among L-114 3D
   previews, L-115 entity ghosts as models, L-184 inspect ghost data, L-185 instant load, L-186 ghost legibility, and the
   remaining L-117 schematic name tags.
   The initial feature and 0.1.8 work are complete, not new tasks.
4. **Placement/breaking:** L-15 breaking and placement restrictions
   (breaking redesign before placement); L-59 held placement style waits for the maintainer's go-ahead.
5. **Optional integration (L-111 shared controls between features):** shared spatial actions, a common menu,
   selection and cross-links; the required 0.2.0 steps are complete.
6. **Research when convenient:** L-79 pumpkin/spyglass frames, L-71 glide
   initiation, L-30 dragon hitboxes, L-33 mob timers, L-105 profiling.
   L-151 body in first person, L-152 held map preview, L-153 last pressed
   movement key and the previously parked work remain later.
7. **Before the next release:** the release after 0.7.0 takes (maintainer
   2026-11-21) the recipe screens' remainder (L-385: making a plan's
   steps), L-340 settings grouping, L-372 hotkeys by context, and a
   review of the changes since 0.7.0 with its fixes; L-384 is not bound
   to it. Then changed-feature smoke and the relevant
   [pre-release checks](backlog/pre-release-checks.md). Latest release: 0.7.0, 2026-11-16 (`v0.7.0`). A full
   regression is not a release gate.

Task-picking rule: bugs first, then this order; follow each item's model,
dependencies and validation requirements. Cheap models skip Design,
Research and strong-model work. A finished item's file moves to `backlog/done/`;
remaining runtime gaps go to the pre-release checks and VALIDATION.md.

## Pre-release checks

Known gaps to pick release checks from: [backlog/pre-release-checks.md](backlog/pre-release-checks.md).

## Bugs

- [L-358](backlog/L-358.md) Realms joined through a friend share one data set (Research): Built with L-365; a server join reads the cookie without a crash (2026-11-19); no Realm checked, the released log line is the check
- [L-365](backlog/L-365.md) The world behind a server, Realm or friend's world can change (Ready): Built; every question and answer checked on a local BDS (2026-11-19); a Realm not seen
- [L-343](backlog/L-343.md) A server's map, waypoints and placements are lost when its address changes (Bug): Parts 1-3 built and checked in game (not with a friend's world); part 4 (merging) built and checked in game; a Realm and a changing port not seen
- [L-342](backlog/L-342.md) Lower findings of the 2026-11-14 review, kept for later (mixed; nothing here is agreed work): Listed only; pick a finding up through its own item when it is to be worked on
- [L-353](backlog/L-353.md) Input events and the client thread: what is still touched outside the tick (Research): Open, not started; nothing here is known to misbehave
- [L-299](backlog/L-299.md) Screenless inventory moves can leave a slot stuck when client and server desync (Research): Open; question 1 read in code (`8cdfe543`, 2026-11-12), waiting for a reproduction. No release target.
- [L-243](backlog/L-243.md) Large terrain gaps that only reload when the player comes near (Bug / Research): Not reproduced; watch item, reopen with the conditions if it returns
- [L-244](backlog/L-244.md) FreeCamera keeps the inside-a-block view after starting in a block (Bug): Parked (maintainer 2026-10-27) until a path to the renderer that draws the effect is found; left vanilla
- [L-245](backlog/L-245.md) Ghost face disappears next to real glass or a beacon from some angles (Bug / Research): Open, not reproduced on purpose yet.
- [L-257](backlog/L-257.md) FreeCamera culler reads blocks on the render thread (Research): Open, not started
- [L-240](backlog/L-240.md) Death layout restore sometimes restores unexpectedly or wrongly (Bug / Research): Open; waiting to see whether L-338's time limit stops the unexpected restores
- [L-227](backlog/L-227.md) FreeCamera outside the loaded area shows only the sky (Bug / Research): Waiting for the maintainer: whether it is worth two read-only trace rounds
- [L-181](backlog/L-181.md) FreeCamera draws the held trident the wrong way round (Bug / Research): Open, not reproduced; drawing only, harmless (maintainer 2026-10-15)
- [L-123](backlog/L-123.md) FreeCamera stretches the worn elytra on the inventory player model (Bug): Parked 2026-10-10 (maintainer) after two culling rounds; resume from the ideas in Next
- [L-117](backlog/L-117.md) Schematic entity name tags render badly in Japanese (Bug): Partly fixed (`47534d7`, colored fringes gone); dark gray Japanese text accepted for now, item kept open
- [L-194](backlog/L-194.md) Save corner marks over real glass or water change with the camera (Bug / Research): Parked 2026-10-15 after three tries; resume with one of the ideas in Next

## Feature work and research

- [L-359](backlog/L-359.md) Material plan across features, from the game's recipes (Design): Open; the viewer, search, favorites, the wanted items and placing are built behind the Recipes setting and were seen in game; so were the schematic hand-over and the counted places. The look was reviewed, and what was agreed is built and was seen in game (RECIPES.md "The look")
- [L-385](backlog/L-385.md) Make a plan's steps from its "to make" tab (Design): Direction agreed (maintainer 2026-11-21); the controls wait on a mockup
- [L-384](backlog/L-384.md) HUD: scale above 200 %, and a paper doll cut to its upper body (Design): Requested by a player (Discord, 2026-11-21); not started, no release target
- [L-360](backlog/L-360.md) Take items out of the item list in creative (Design): Open, not started
- [L-362](backlog/L-362.md) What a recipe gives back (Design): Open, not started
- [L-363](backlog/L-363.md) The game's tooltip and item details for a listed item (Research): Built 2026-11-19 with the stack's own hover text and seen in game; a listed stack still carries no data
- [L-368](backlog/L-368.md) The game's recipe book: its button and its state beside Lamium (Research): Open, not started; the button's choice is agreed (maintainer 2026-11-20)
- [L-370](backlog/L-370.md) What the plan is for and how it shows (Design): The 2026-11-21 review's choices built and seen in game
- [L-372](backlog/L-372.md) Hotkeys: where and on what each action works (Design): Open; the direction is agreed (maintainer 2026-11-20), the details are not
- [L-371](backlog/L-371.md) Recipe screens: groundwork before the next features (Design): Agreed (maintainer 2026-11-20); split into L-373 to L-379 and existing items
- [L-369](backlog/L-369.md) Recipe screens: open fixes of the look (Ready): All seen in game 2026-11-21; to close with the next release notes
- [L-364](backlog/L-364.md) Draw "one of several" the agreed way on the other screens (Polish): Built and seen in game 2026-11-19; to close with the next release notes
- [L-340](backlog/L-340.md) Settings grouping (Design): Open; an agent's draft below (2026-11-17) waits for the maintainer.
- [L-330](backlog/L-330.md) Radar: mob icons that draw wrongly, then icons by default (Research): Open, not started
- [L-319](backlog/L-319.md) Item details: Work Penalty first, then the unshown rest (Feature): Built 2026-11-13 after 0.6.0 (not released) and checked in game; unchecked cases remain
- [L-315](backlog/L-315.md) Finish what waits for more game symbols (Blocked on a symbol request): Parked until the maintainer files one request to `LiteLDev/mcapi-requests` (not now, 2026-11-06: keep requests to LiteLDev infrequent). Nothing here is urgent.
- [L-249](backlog/L-249.md) Show the chunks within simulation distance (Design / Research): Research partly done: the client reads the range (4 in a local world); two checks left before the presentation is agreed
- [L-265](backlog/L-265.md) Schematic placement assist: place the ghost's block in its state (Design / Research): Open, not started; no release target
- [L-233](backlog/L-233.md) Make inventory transfer gestures rebindable (Design): Open, not urgent; nothing is blocked today
- [L-184](backlog/L-184.md) Inspect ghost block data and schematic container contents (Design / Research): Partly done: target card and saved block entity rows shipped; file-only data (containers, comparator signal) open.
- [L-185](backlog/L-185.md) Instantly load a schematic into the world when cheats allow it (Design): Open, not approved for implementation or release
- [L-186](backlog/L-186.md) Improve schematic ghost legibility and reconsider translucency (Design / Research): Partly done: tint slider and outline widths shipped and checked; translucent ghosts open (maintainer: after the release).
- [L-173](backlog/L-173.md) Quick shapes: a beacon's range at a glance, shapes that are not saved (Design): Direction decided (2026-11-06); waiting for the in-game range check.
- [L-170](backlog/L-170.md) Enchantment order planner (Feature): Built after 0.6.0 (not released) and checked in game step by step; runtime checks remain
- [L-169](backlog/L-169.md) Item Transfer: remaining gesture and screen extensions (Design): Waiting for the maintainer to choose any extension; none approved
- [L-168](backlog/L-168.md) Trade finder: reroll a villager until a wanted trade appears (Design): Waiting for the maintainer's choices below; not chosen for building, no release.
- [L-167](backlog/L-167.md) Change the language and resource packs from inside a world (Research): Partly done: Game language and Skip server resource packs shipped in 0.2.1; global pack stack work not chosen.
- [L-166](backlog/L-166.md) Villager trade helpers: favorites and assisted trading (Design): Part 1 (Shift + right-click repeated trading) shipped in 0.2.1; part 2 is candidates waiting for the maintainer, not approved to build
- [L-165](backlog/L-165.md) Mass Craft: craft a recipe many times at once (Design): Open; built behind its own setting and seen in a local world; a server is unchecked. No release
- [L-163](backlog/L-163.md) A Lamium-drawn inventory screen (Research): Open, not started; recorded for later.
- [L-132](backlog/L-132.md) Move while the inventory screen is open (Research): Parked 2026-10-10 after eight trial rounds without smooth movement; resume from Next
- [L-59](backlog/L-59.md) Held placement style: vanilla, Java-like or fast (Design done; Research first): Planned, nothing built; no step starts until the maintainer says so
- [L-15](backlog/L-15.md) Breaking and placement restrictions (Design): Breaking shipped in 0.1.8 and stays until replaced; the whole plan is reopened for redesign before placement is built
- [L-91](backlog/L-91.md) Shield glint missing in Lamium's item icons (Research): Parked 2026-11-17 (maintainer): only the shield glint is left. Known issue
- [L-79](backlog/L-79.md) Carved pumpkin and spyglass frame draw path (Research): Open; next step is the transparent-texture test pack (no code)
- [L-71](backlog/L-71.md) Start an elytra glide from the mod (Research): Open; headers read 2026-11-12, next trace not run
- [L-93](backlog/L-93.md) Schematic: load, place, project, verify and list materials (experimental) (Follow-up tracking; each L-item below owns its kind and open choices): Initial scope shipped in 0.1.7, 0.1.8 rendering/compatibility work done; follow-ups open
- [L-30](backlog/L-30.md) Ender Dragon multipart hitboxes on Bedrock (Research): Probed: parts are readable on the client; waiting for a real End dragon check, a server check and the color choice
- [L-114](backlog/L-114.md) 3D previews in the schematic screen (Research): Partly done: Files and Check tab previews shipped (0.1.8); the Check tab's mistake look, see-through emphasis and moving the zoomed view are open
- [L-115](backlog/L-115.md) Entity ghosts drawn as models (Research): A compromise version shipped in 0.1.8; the limits below are open
- [L-116](backlog/L-116.md) Raw materials from the game's recipes (Research): The client's recipe table read in game 2026-11-17 (local and an Endstone server, identical); pure calculation and recipe index built and tested (L-359); not wired to any screen
- [L-111](backlog/L-111.md) Integration between features: remaining shared controls (Design): Optional scope, nothing chosen for implementation or release
- [L-105](backlog/L-105.md) Performance: find the real bottleneck before optimizing (Research): Open; pure groundwork in place, profiler spike not built (needs the maintainer at the controls)

## Later / parked

- [L-329](backlog/L-329.md) Health a food would give back, beside its food values (idea): Parked; not pursued unless the maintainer takes it up
- [L-151](backlog/L-151.md) Show the player's body in first person (Research): Open, not started; no runtime evidence
- [L-152](backlog/L-152.md) Preview held map items in inventory (Design): Open, no implementation commitment
- [L-153](backlog/L-153.md) Prefer the last pressed opposing movement key (Design): Pure model built and tested; no input hook, setting or action; release behavior and default not chosen
- [L-282](backlog/L-282.md) Toggle Vibrant Visuals with a key (Research): Open, not started; low priority, no release target
- [L-283](backlog/L-283.md) Stronger block outline and a crosshair face highlight (Design): Open, not started; no release target selected
- [L-284](backlog/L-284.md) Custom crosshair (Design): Open, nothing decided; low priority, no release target
- [L-287](backlog/L-287.md) Make the minimap arrows legible at every map size (Design): Parked (2026-10-31), no release target; reopen with a new marker idea or if the Map Size 18 % distortion matters

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
