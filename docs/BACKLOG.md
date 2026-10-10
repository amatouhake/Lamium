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

Finished and closed items live in [BACKLOG-DONE.md](BACKLOG-DONE.md) with
their full history. An `L-` number referenced elsewhere that is not in this
file is there.

Long feature contracts and dated build/research records are linked from the
L-item ([MAP.md](MAP.md), [SCHEMATIC.md](SCHEMATIC.md)); current status and
remaining work stay here. Document roles: [README.md](README.md).

## Release policy (decided 2026-09-28)

Lamium is developed at the maintainer's pace, for their own use first, and
published for whoever wants it. There is no "first release" gate.

- Release when a meaningful set of changes has landed and main builds, passes
  the tests and has no known crash. Before tagging, smoke-test the release
  build (the mod loads, settings open, the changed features work) and update
  the README feature list and Known issues. A full regression is not required.
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
- Large features may start at any time. They land on main in steps, default
  off and with the Experimental badge, so main stays releasable while they
  grow. A step that is not usable yet stays out of the settings screen (or
  behind a build option) instead of being shown half-working.

## Current execution order

Keep this section short. It is only the ordering layer; task details and status
live in the L-items below. If this summary ever disagrees with an L-item, the
L-item wins. Every entry names what the task is, not only its number.

1. **Small and medium features and fixes** (maintainer 2026-10-10: these
   come before large features and L-111 integration for now):
   - Bugs: L-123 elytra stretched on the inventory model after FreeCamera
     (parked 2026-10-10 after two rounds); L-91 shield
     glint in Lamium's icons (parked after one round; leather fixed).
   - Paused: L-132 move while the inventory screen is open (research
     paused after eight trial rounds).
2. **Schematic — L-93 follow-ups:** the screen review and the 0.1.8
   rendering work shipped in 0.1.8. Open: L-114 the Check tab preview's
   mistake look and see-through emphasis, L-115 entity models beyond the
   light-blue compromise (real skins, details such as cushion colors),
   L-116 raw materials from the game's recipes, L-117 Japanese name tags.
   Choose with the maintainer; SCHEMATIC.md retains the contract and build
   record. Server/broader coverage remains open.
3. **Placement and breaking — L-15 restrictions and L-59 held placement
   style:** L-15 breaking shipped in 0.1.8; the maintainer will redesign
   it, and placement waits for that Design. L-59 waits for the maintainer's
   go.
4. **L-111 integration between features:** proposal in INTEGRATION.md;
   the maintainer reviews it when it becomes needed. Planned for 0.2.0
   together with L-134 splitting SettingsScreen.cpp, L-136 splitting
   GhostRenderer.cpp and L-135 leaving the PDB out of the release ZIP.
5. **Research when convenient:** L-79 carved pumpkin and spyglass frame draw
   path (cheap-model friendly
   trace/test steps), L-71 starting a glide from the mod, L-30 Ender Dragon
   part hitboxes, L-33 mob growth and
   breeding timers, L-105 performance profiling (measure before any
   optimization).
6. **Before a release:** the pre-release checks below. 0.1.9 was released
   on 2026-10-11; server checks stay listed below as known gaps (Release policy does not require a full
   regression).

Ideas that are not yet chosen (for example more inventory transfer gestures,
an arrow-count HUD line, a fall-rescue elytra, Mass Craft) stay
in the maintainer's notes and enter this file once chosen.
Mass Craft ranks below Map because resource packs already cover part of it.

Task-picking rule: bugs first; otherwise work on what the execution order
names. Cheap models skip strong-model, Design and Research work. Follow the
L-item's dependencies and model/validation requirements. When a task is done,
update its status and relevant feature doc, then move it to BACKLOG-DONE.md;
do not duplicate task details into this summary.

---

## Pre-release checks

Remaining coverage to consider for the next changed-feature smoke test. These
include trace-only and local-only results; VALIDATION.md owns the per-feature
gaps. The Release policy above does not require a full regression or closing
every server gap before a release.

### Released builds and registry follow-up

The following are released-build checkpoints and listing follow-ups, not
unmet gates for versions already published. Registry pickup for 0.1.4 to
0.1.7 is confirmed (maintainer 2026-10-10); 0.1.8's registry PR
(LiteLDev/lipr, "add github.com/amatouhake/Lamium@0.1.8") was merged
2026-10-09 UTC.

- 0.1.9 was released on 2026-10-11 (`v0.1.9` at `8bf28c0`; asset SHA-256
  `c0bbc358...a4b`): L-96 Connected Textures (panes with the one-texel
  stretch; split panes parked as L-133), L-128 player list, L-127 inventory
  HUD and used-slots counter, L-129 locked trades, L-130 English search,
  L-120 entity counts, L-125/L-126 FreeCamera readouts, waypoints and leave
  on hit, L-121 even Night Vision, the Vibrant Visuals distance fog fix and
  vanilla-batched item icons. The maintainer's smoke test passed on the
  release-ZIP DLL `0bc5eac6...079`. No settings migration. Tag CI was
  running when this was written. After tagging: check that the registry PR
  picks up `v0.1.9` and that LeviLauncher/Bedrinth offer it.
- 0.1.8 was released on 2026-10-10 (`v0.1.8` at `e175eed`, tag CI passed;
  asset SHA-256 `b99f9f27...e300da4b`): L-93 Schematics rendering and
  compatibility (version 2 saves, neighbors, render layers, liquids, block
  entity data, performance), L-114 3D previews, L-115 entity models, the
  screen review, map markers and target-card rows; L-118 distance fog
  (selected by default); L-109 death layout restore; L-15 step 1 breaking
  (to be redesigned, maintainer 2026-10-10); L-37 FreeCamera underground
  terrain (landed after the 0.1.7 tag; added to the notes after
  publishing); L-122 Fake Offhand fireworks;
  L-106/L-107 F1; L-108. The maintainer's smoke test passed on the
  release-ZIP DLL `1e2f4e6f...37ae55a2c`. No settings migration. After
  tagging: check that the registry PR picks up `v0.1.8` and that
  LeviLauncher/Bedrinth offer it.
- 0.1.7 was released on 2026-10-07 (`v0.1.7`, tag CI passed; asset SHA-256
  `2e0926e0...93b013b5`): L-93 Schematics (experimental), L-67 Weapon
  Switch, L-97 fixed fetch slot, L-94 offhand swap (`F`), L-95 Fake Offhand
  item use, L-102/L-103 Hand Restock threshold/order and inventory-screen
  transfer, L-104 map follow-ups, L-89 distant players, L-98 HUD density,
  L-99 Zoom below 2x, L-101 version display. The maintainer's smoke test
  passed on the release-ZIP DLL `2c3e7546...fd903806b` (`b3c6555`). No
  settings migration. Schematic gaps (servers,
  other dimensions, large files, block entities from files, half-drawn beds)
  are in VALIDATION.md and the README's Known issues.
- 0.1.6 was released on 2026-10-02 (`v0.1.6`, tag CI passed; asset SHA-256
  `3d864ca1...3946554f`): L-90 Simplified Chinese (first AI-assisted
  translation, corrections welcome), L-88 target hearts, L-75 offhand slot,
  L-63 saturation, L-64/L-92 food values and durability inside the vanilla
  tooltip. The maintainer's smoke test passed on the release-ZIP DLL
  `86f5baf0...2587883f` (`b71c9f8`). No settings migration.
- 0.1.5 was released on 2026-10-02 (`v0.1.5`, tag CI passed) at the maintainer's request,
  with the map (L-60, L-85, L-87) as its main change. The release build
  `6add9b9` (DLL `0fae1c58...dc657614c`, from the release ZIP) was deployed
  for a smoke test; no separate result was reported before tagging.
- 0.1.4 was released on 2026-09-30 (`v0.1.4`, tag CI passed). Its smoke test
  passed on the release build `4d25424`: the 0.1.4 version, Hand Restock
  (L-66) and offhand totems (L-68), Tool Protection (L-62), Tool Switch fetch
  (L-69), Auto Elytra (L-70) and the L-02 dedicated openers. Still open:
  Hand Restock on BDS and with real latency. The coverage gaps below carried
  past 0.1.4; recheck the relevant ones before the next release.

### Pending feature checks

Server use of Weapon Switch, offhand swap, Fake Offhand, Hand Restock
(threshold/order, inventory-screen transfer) and fixed-slot fetch was
reported working on 2026-10-10 (everyday use, not every listed step); gaps
found later are handled as bugs.

- FreeCamera underground visibility (L-37): the 2026-10-07 native-request
  adapter passed cave drawing in a local world and on BDS on `d56b81e`;
  the log confirms the native 3 -> 5 replacement. Normal-view and player
  control restoration also passed. The follow-up confirms intended Hold/Toggle
  operation, camera retention through menus/focus loss and OFF on dimension
  travel. Other players' view of the body, world-exit/death regression coverage
  for the adapter, controllers and surface/shadow/graphics coverage remain open.
  See CAMERA.md and VALIDATION.md.
- FreeCamera speed controls (L-26): five-step adjustment and speed keys passed
  on `7e72244`; revised labels and forward-only sprint follow-up passed on
  `41b1ff6`. Restart persistence and detailed input/menu/focus combinations
  were not reported separately.
- FreeCamera Position reference (L-76): Player remains the default; World
  compensates body movement, with live switching preserving the camera target.
  Position retention, live switching and release passed on `43c4211`; rapid
  elytra/body movement fix (L-77), switching and release passed on `d20fdf8`.
  Lamium menus, inventory/window movement and release passed on `d3f0293`
  (L-78). Check target readouts, Hold input ownership and session cleanup;
  see CAMERA.md.
- Hide effects first step (L-42): master/rain-splash follow-up passed on
  `41b1ff6`. Remaining coverage: restart persistence, child keys while the
  master is off, both pipelines, ambient layers, resource packs, graphics
  modes and world/dimension transitions; see VISUAL-EFFECTS.md.
- Boss bars (L-42): hiding and switch behavior passed on `d20fdf8`.
  Check the optional key separately, other HUD elements, restart persistence
  and additional resource packs/graphics modes on a trace-disabled build.
- Nausea color (L-42): child/key/master hiding/restoration and unchanged
  effect/icon/vanilla preference passed on normal build `b239eb9`. Remaining:
  restart persistence, additional packs/modes and lifecycle/owner cases.

---

## Bugs

### L-123 FreeCamera stretches the worn elytra on the inventory player model
Kind: Bug, low priority. Reported from use 2026-10-09 (maintainer's notes).
Parked 2026-10-10 (maintainer) after two culling rounds; resume from the next ideas below.
Status: open. Checked 2026-10-10 (VALIDATION-LOG): wrong when the body is
out of view (camera near), right when the body is in view (camera far), so
the inventory model reuses pose state that only the world render of the
body updates. Next: Research which state (elytra wing animation) is left
stale and whether it can be refreshed for the inventory model alone.
Two rounds 2026-10-10, both reverted: keeping the body "visible" to the
camera's `LevelRendererCamera::isAABBVisible` (round 1: never matched on the
render thread) and to both `isAABBVisible` and `cullerIsVisible` on any
thread (round 2: 1751/249 calls, 10/22 near the body, the point test was
overridden) left the elytra stretched. So these camera tests are not what
skips the body's pose update. Stopped after two rounds. Next ideas (not
started): find the entity render queue's own visibility step
(`LevelRendererCamera::queueRenderEntities` and what it calls), or refresh
the pose on the inventory model's side (the paper doll render path,
`GeometryAtlas::DollRenderContextImpl::update`, `PaperDollData`).
Reproduction (maintainer 2026-10-10): wear an elytra, turn FreeCamera on,
move the camera away from the body, open the inventory. The elytra on the
player's 3D model in the inventory screen draws simplified and stretched
downward. The elytra slot icon is not affected. Whether pose, animation
state, level of detail by camera distance or the render context left by
FreeCamera causes it is unknown; a first check is whether the distance
matters (camera near the body vs far).

### L-117 Schematic entity name tags render badly in Japanese
Kind: Bug, low priority. Reported by the maintainer 2026-10-08 (L-93 checks).
Status: partly fixed (`47534d7`, checked in game 2026-10-08): the colored
fringes are gone. Not solved: Japanese text in these tags draws dark gray
instead of white; a white dark color brought the fringes back. The
maintainer accepts it for now but keeps the item open. World name tags
appear only over entities drawn as dashed frames (no model, L-115); they are
drawn in `GhostRenderer.cpp` `drawNameTags` (Japanese glyphs from sheet 48,
type 3, scale 1.333, with the font's own material).

---

## Feature work and research

### L-134 Split SettingsScreen.cpp (with 0.2.0)
Kind: Refactor. Raised by the maintainer 2026-10-11; planned together with
L-111 integration and other 0.2.0 clean-up, not before.
`src/ui/SettingsScreen.cpp` is 4585 lines (2026-10-11), the largest file by
far, and changed in 131 commits since 2026-09-01. It mixes the settings rows
and navigation, key capture and the Hotkeys view, text and number editing,
the Shapes editor (drafts, swatches, docked rendering), the waypoint list,
the map cache controls and the version tip. L-73 (BACKLOG-DONE, item D)
dropped this split on 2026-09-30 at 1,878 lines: the Shapes view and the
input listeners shared 20+ screen-wide variables, so a file split would only
have moved text behind a header of shared variables ("split it when the
screen grows again, after grouping its state first"). Group the state
first, then split by those parts into files that keep behavior unchanged;
pure layout and state logic moves into headers with tests where it can
(AGENTS.md rule 1). Plan the cut with the
maintainer before moving code: the split should match how L-111 regroups
features.

### L-135 Leave the PDB out of the release ZIP (with 0.2.0)
Kind: Distribution. Decided by the maintainer 2026-10-11.
`Lamium.pdb` is about 90% of the release ZIP and grows each release
(compressed 13.2 MB in 0.1.7, 15.6 MB in 0.1.8, 17.3 MB in 0.1.9; the DLL is
1.8 MB). No crash report has arrived, and a description of the situation is
expected to be enough to reproduce one. From 0.2.0 the ZIP ships without the
PDB (`scripts/New-ReleaseArchive.ps1`, `Check-Package.ps1`, DISTRIBUTION.md,
the LIP package contents). The PDB is attached to the GitHub release as a
separate asset (`Lamium-<version>-client-windows-x64.pdb` or similar,
decided 2026-10-11), so LIP and LeviLauncher install only the DLL while the
symbols stay available for crash addresses.

### L-136 Split GhostRenderer.cpp by responsibility (with 0.2.0)
Kind: Refactor **(strong model)**. Chosen by the maintainer 2026-10-11 after
an outside size review (SettingsScreen.cpp, GhostRenderer.cpp, WorldMap.cpp,
InfoHud.cpp, Translations.h); planned with L-134 and L-111.
`src/features/schematic/GhostRenderer.cpp` is 2,792 lines (701 on
2026-10-05, 1,693 on 2026-10-08) and changed in 107 commits since
2026-10-01. Despite its name it holds more than drawing: the ghost mesh
build (neighbors, culling against ghosts, back faces, duplicate quads,
liquids, layers and quad order), the verification scan and progress
(cell classification, entity checks, mismatches), the area save (stepped
save, vanilla load self-check), block entity loading, the in-world drawing
(selection, placement frames, waiting columns, entities, name tags) and
the performance report. Split along those lines into files that keep
behavior unchanged, with the pure parts (classification, culling rules,
quad ordering) in headers with tests. Every step is checked in game against
the L-93 rendering checks (doors, beds, panes, liquids, honey/slime, block
entities, entity models, large placements' frame rate). WorldMap.cpp
(~1,240 lines) and InfoHud.cpp (~1,400) are worth a look in the same pass;
Translations.h is data and stays as it is.

### L-133 Connected Textures: split glass panes without dark halves
Kind: Research. Opened 2026-10-11 from L-96.
Status: parked 2026-10-11 after four trial rounds; panes ship with the stretch.
Panes draw joined glass by moving the texture inward (a 1-texel stretch,
`4ef3d51`); glass blocks, bookshelves and sandstone draw split at the
texture's scale. Split panes (`17cedc6`..`20b9059`) looked right up close but
translucent panes blended dark on one half of each block depending on the
view: facing south at an east-west pane the west half, facing east at a
north-south pane the south half; facing north or west nothing. Night Vision
hid it. Ruled out with dumps and trial builds: the split values (positions,
UVs, colors, light, normals, facing), redrawing the pane versus appending
copies to the tessellator, and giving every cell its face's quad info
(center included). The likely cause is the translucent face sorting and
culling (`RenderChunkSorter::sortAndCullFaces`, `FaceInfo` with a `reverse`
bit, `RenderChunkGeometry` face sorting metadata); its inputs are not
visible in the SDK headers. Stained glass blocks, also split and blended,
show nothing. A stained pane cross shows seams to its arms even with the
feature off: vanilla, not Lamium's.
Trials 2026-10-11 (`99a3089`..`84f4335`, split panes restored, then reverted):
the dark half is the arm whose glass is cut at a joined edge (west arm of
east-west panes, south arm of north-south ones). Reversing the quads inside
the appended copies, keeping the largest cell in the first draw's place, and
showing every cell its own texels all stayed dark; appended copies folded to
a point with unsplit glass did not. Every quad has `facing` 6 and `twoFace`
0. Up close the dark band is a smooth gradient, darkest at the seam about
two texels from the joined edge, as if a value interpolated from the seam
vertices. So splitting a pane's glass face into quads is the trigger, not
the copies, their order, sort data or texels; the vertex data Lamium writes
matches vanilla. Hypothesis, unchecked: the game derives per-vertex lighting
from vertex positions after tessellation (the `uv1` values are near 0 in
daylight), and the new mid-face vertices get dark values. Parked: the gain
over the one-texel stretch is small.

### L-132 Move while the inventory screen is open
Kind: Research, then Feature. Requested by the maintainer 2026-10-10;
direction agreed the same day.
Status: open; paused 2026-10-10 after eight trial rounds without smooth
movement (findings below).
While the player's own inventory screen is open, keep walking, jumping and
sprinting from the bound movement keys so items and armor can be arranged
on the move. The camera cannot turn (the mouse belongs to the screen).
- Only the player inventory screen at first; containers (chests etc.) and
  other block screens come later, if at all.
- Sneak is not passed through: Shift is shift-click (quick move) in the
  inventory.
- Paused while a text field has focus (creative search, recipe book search)
  and on focus loss.
- Default off; the help text warns that some servers may treat moving with
  an open inventory as cheating (like Auto Attack's warning).
- Research first: where the game stops reading movement input while a screen
  is open (`ClientInputHandler`, `ClientMoveInputHandler`,
  `ClientInputMappingFactory::_createScreen*Mapping`, `MoveInputComponent`)
  and the least invasive place to feed the movement keys back in, without
  touching game state from the window procedure (AGENTS.md rule 2).
- Trial record (2026-10-10, `xmake f --inventorymove_trace=y`,
  `src/features/interaction/InventoryMoveTrace.cpp`, single player on
  1.26.51.01; commits 7e56cc8..40de3e3, last DLL 9edd5c65...):
  - `ClientInputUpdateSystem::extractRawHIDInput` keeps running (~20/s)
    with `inventory_screen` open; vanilla's bindings read from
    `getOptions().getCurrentKeyboardRemapping()` (`key.forward` = 87, ...,
    `key.sprint` = 17). Bedrock's move x is positive to the left.
  - Raw flags written after extraction are gone again by
    `PlayerMovement::calculateMoveVector` (two calls per tick: the
    `RawMoveInputComponent` state and a second state, likely the local
    server's). No movement.
  - Writing the flags into `MoveInputComponent`'s states after
    `inputHandlerUpdatePlayerState` (and forcing the vector) reaches
    `calculateMoveVector`, yet `MoveInputComponent::mMove` and the built
    `PlayerAuthInputPacket` stay zero: the player moved only through server
    corrections (~2/s), in jerks; no jump. The injected state flags also
    persisted after closing the screen (kept walking). Patching the auth
    packet's move vector and input flags did not smooth it.
  - Not the cause: `MoveInputComponent::Flag::MoveInputStateLocked` and the
    client input locks are clear; `UIScene::absorbsInput` forced false for
    the inventory changes nothing; copying the gameplay key bindings
    (`button.up/down/left/right/jump/sprint` from `gamePlayNormal`) into the
    `screen*` input mappings changes nothing.
  - Next guesses: find where `mMove` is zeroed while a non-play screen is
    on top (inside `ClientInputUpdateSystemInternal::tickUpdateClientInputView`,
    perhaps via `VanillaClientGameplayComponent` or a play-screen check);
    or whether the input handler only routes movement button events to
    `ClientMoveInputHandler` while the gameplay mapping is the active one
    (`ClientInputMappingFactory::_activateMapping`).

### L-59 Held placement style: vanilla, Java-like or fast
Kind: Design done (discussion with the maintainer, 2026-09-28); Research
first, then Ready **(strong model)**. Started as "keep placing across a left
click" after L-49 closed as vanilla parity.
Status: planning. Nothing is built, and no step starts until the maintainer
says so.
What it is for: building bridges and long straight lines quickly by holding
the use button with a block. Block placement only; eating, bows and buckets
keep vanilla behavior.

#### Spec (decided 2026-09-28)
- One setting, "Held placement", under Actions: Vanilla (default), Java-like,
  Fast. Default Vanilla because players expecting Bedrock would be
  surprised.
- Vanilla: Bedrock's own held build session, unchanged.
- Java-like: while the use button is held with a block, place on whatever
  face the crosshair targets at Java's fixed interval (every 4 ticks):
  no direction lock and no placing into air; nothing is placed when the
  crosshair is not on a face. An intervening left click does not
  end it (the original L-59 request): placing resumes while right is still
  held.
- Fast: while right is held, place on each new block face the crosshair
  crosses, capped per tick (a setting with a modest default). The help text
  warns that some servers may treat it as unfair. Also survives a left
  click.
- Both non-vanilla styles respect the placement restriction (L-15) when it
  is on, which turns Fast into "paint a flat floor".
- Interactive blocks (chests, doors, buttons), ordinary attacks, Fake
  Offhand and custom activation chords keep their current behavior.

#### Steps
External behavior research suggests the following hypotheses, but they are
not treated as facts until Lamium traces current Bedrock: while the button is
held, Bedrock may lock the build direction set by the first placement; blocks
hovered outside that direction may be ignored; the session may place into air
in front of the last block while bridging; and it may place as soon as a new
position is valid instead of on a fixed interval. The Java-like mode defined
above places on the currently targeted face every 4 ticks and does nothing
when no valid face is targeted. Hypothesis sources (Java mods describing
Bedrock, behavior only): modrinth.com/mod/pro-placer and
github.com/squeeglii/BridgingMod/issues/13.

1. Research: record Bedrock's held build session with the L-49 trace
   (`research_trace`, `FakeOffhandTrace.cpp`) and confirm or correct each
   point above: the direction lock (what sets it, which positions it
   accepts), placing into air along it, the timing, and what ends the
   session. Lamium already hooks `GameMode::buildBlock` and
   `SurvivalMode::buildBlock` for detached-camera interaction; test whether a
   discrete ordinary build action is sufficient for one server-authoritative
   placement, and map any required start/stop lifecycle. Do not synthesize a
   custom packet merely to imitate held input. Report before building.
2. Java-like style (Ready after step 1), with tests for the interval and the
   left-click rule.
3. Fast style with the per-tick cap; then combine both with L-15's placement
   restriction once that exists.

### L-15 Breaking and placement restrictions
Kind: Design (reopened). Breaking shipped in 0.1.8 and will be redesigned;
placement waits for that Design and then needs Research. The 2026-09-28
plan below replaced the old Breaking Restriction (capture/reset keys).
Status: step 1 (breaking) built 2026-10-07 with L-73 step 13 and checked in
game 2026-10-08 (VALIDATION-LOG). Implementation notes: RESTRICTIONS.md.
Shipped in 0.1.8; the maintainer is not yet satisfied with it and expects to
redesign breaking too (2026-10-10).
Reopened for Design (maintainer, 2026-10-08): the plan dates from 2026-09-28
and should be rethought before placement is built. Problems seen: too many
modes to cycle through, and the placement mode has no key (and does nothing
yet). Steps 2-3 below wait for that rethink; the breaking behavior that
passed stays until it is replaced. Faces z-fighting: L-110 (fixed).
Decided 2026-10-07: breaking keeps the existing Breaking Restriction toggle
and Cycle Breaking Mode bindings (no new default keys); the saved breaking
mode carries over unchanged and Height band is added to the list.
What it is for: leveling ground and digging tunnels without breaking past a
chosen level or face, and laying floors, walls and roofs flat without
placing outside them. Placing with a chosen facing is not wanted for now.

#### Spec (decided 2026-09-28)
Anchor and lifetime
- The anchor is the block where the button press starts (breaking: the first
  block mined; placement: the first block placed). The restriction holds
  while that button stays held and ends on release. The capture and reset
  keys go away.
- Rejected blocks never end the physical hold: when the crosshair comes back
  to an allowed block, breaking or placing continues (L-36 already does this
  for breaking).
Breaking modes
- Layer: only blocks at the first block's Y.
- Height band: from the feet level up N blocks (default 2, a setting); this
  mode is anchored at the feet, not the first block. For 2-high tunnels and
  wide leveling.
- Plane: the plane of the first block's mined face.
- Line and column: straight on from the first block, or vertical through it.
Placement modes (chosen separately from breaking, so leveling and laying a
floor can run at once)
- Layer (same Y as the first placed block), plane (a wall: the vertical plane
  through it, oriented by the first clicked face), line, column.
Controls and feedback
- Breaking and placement each have a switch with a key and a "next mode"
  key (no default keys decided yet); while on, the Status element shows the
  mode.
- While the button is held, the allowed region shows as faint faces like
  Shapes, skipping the targeted block so the vanilla outline stays visible.
  A rejected block is silent: no sound, no toast.
- Later, after the basic modes work: shape-linked modes (inside a shape, on
  its surface).
Settings and ids
- The Actions category keeps one "Block Restrictions" group. The old capture
  and reset actions stay in `enum Action` (ids are saved) but are no longer
  shown or dispatched; the old breaking mode setting maps to the new list.
  Say so in the settings migration notes before changing the store.

#### Steps
1. Breaking (Ready, strong model): press-anchored lifetime, the four modes
   plus height band, the allowed-region faces, Status line, removal of the
   capture/reset flow. Pure region predicates already exist
   (RESTRICTIONS.md); extend them and their tests. In game: every mode in
   survival and creative, held-button target changes, Tool Switch together,
   world exit and dimension change.
2. Placement gate (Research): Lamium already has cancellable
   `GameMode::buildBlock` / `SurvivalMode::buildBlock` hooks in the
   detached-camera interaction guard. Determine whether that boundary can
   reject the whole placement before mutation and, separately, how to derive
   the actual destination cell/state using vanilla placement semantics rather
   than assuming clicked-block + face is always correct. Trace ordinary
   blocks, replaceable vegetation, slabs/snow, doors/beds, signs, redstone,
   waterlogged/merge cases and edge placements without touching container use
   or buckets. Prefer a vanilla placement-prediction API when available.
   RESTRICTIONS.md lists the existing trace candidates. Stop and report if no
   safe path exists.
3. Placement modes (Ready once step 2 finds a path): the four modes, anchor
   on the first placed block, faces and Status line.

### L-91 Shield glint missing in Lamium's item icons
Kind: Research. Found by the maintainer 2026-10-02 while checking L-75;
leather added 2026-10-06 (found under L-61).
Status: parked 2026-10-06 after a trace and three in-game experiments; the
Update 2026-10-10: leather layers (and fence gates, L-119) fixed by the
shared-mesh icon path `inspection/render/ItemIcon` (checked). Left: the
enchanted shield's glint; one round failed and is parked; the research
record and the next idea are in BACKLOG-DONE under L-119.
cause is known, the fix needs a different draw path (below). Known issue.
Symptoms: an enchanted shield shows no glint in container previews and the
L-75 offhand slot; dyed or undyed leather armor loses its undyeable layer in
container previews and the durability HUD. Vanilla slots show both.
Findings (VALIDATION-LOG 2026-10-06, `icon_trace`):
- Lamium draws icons with `ItemRenderer::renderGuiItemNew` (plus a foil call
  when `Item::isGlint`). Vanilla slots are `InventoryItemRenderer`, a UI
  custom renderer whose passes the UI batch prepares (material per pass, two
  texture slots) before calling `renderGuiItemInChunk`.
- Leather: one slot pass, UI material `Item` (13), chunk type 2, and one
  `iconBlit` with exactly the arguments Lamium's call makes. With
  `renderGuiItemNew` the undyeable pixels are dropped by every material
  tried: the plain icon material, the multi-color tint material (which tints
  the dyeable pixels with the *secondary* color) and the UI `Item` material;
  the entity change-color material draws the transparent background too.
  Vanilla's own equip animation, which uses `renderGuiItemNew`, shows the
  same gap (maintainer's recording, 2026-10-06). So this call cannot draw
  the layer; it is not a missing argument.
- Glint: flat icons get three slot passes (`ItemGlintStencil` chunk 2,
  `InventoryItemGlint` chunk 4, `ItemUnglintStencil` chunk 5); Lamium's single
  foil call at 1.35 matches them. The shield is a model (`Shield` material,
  chunk 7) and the foil call draws nothing for it.
- Calling `renderGuiItemInChunk` outside a slot drew a flat tint square
  (L-61), because the batch setup is missing.
Next step if resumed (a larger change, maintainer's call): draw these icons
through the slot path, e.g. drive an `InventoryItemRenderer` (or its pass
setup) from Lamium, which could fix the shield glint too. Keep `isGlint` as
the glint predicate; do not replace it with `isEnchanted`.

### L-79 Carved pumpkin and spyglass frame draw path
Kind: Research. Cheap models may run the steps below and report; implementing
a hide switch needs a strong model. Parked from L-42 on 2026-09-30.
Goal: find a typed, verifiable way to skip only the carved pumpkin overlay and
the spyglass frame while keeping the pumpkin worn and the spyglass zoom.
Known (VISUAL-EFFECTS.md "Gated frame trace" and after): neither frame passes
through the three `Mesh::renderMesh` overloads, `ScreenRenderer::blit`,
`Tessellator::triggerIntercept` or the UI render context
(`getTexture`/`drawImage`/`flushImages`), even counted per frame. The frost
frame and nausea color do pass through `Mesh::renderMesh`. The vanilla pack
has `textures/misc/pumpkinblur` and `textures/ui/spyglass_scope`; the vanilla
UI definitions reference neither. `FullScreenEffectRenderer` and
`OnCameraEffectRenderer` (members of `InGamePlayScreen`) are opaque in SDK
26.51.5.
Steps, one at a time, each with a written hypothesis first:
1. Texture test (no code): a local test pack whose `pumpkinblur.png` and
   `spyglass_scope.png` are fully transparent. If the frames vanish, the frames
   sample those textures and a texture-load route (return a transparent
   texture while the switch is on, then reload) becomes the candidate; if not,
   find which texture they use.
2. Search the SDK headers for other typed draw entry points not yet traced
   (dragon frame builder, `mce::MeshHelpers`, render-graph passes) and add
   them to the effects trace's gate/count comparison; never read or guess
   opaque layouts.
3. Record each result in VISUAL-EFFECTS.md and VALIDATION-LOG.md.
Stop and hand back after two runtime rounds without a new candidate.

### L-71 Start an elytra glide from the mod
Kind: Research (cheap models may collect traces). Split from L-70 on
2026-09-30; low priority.
Status: open.
After L-70 puts an elytra on in mid-air, calling `Player::tryStartGliding`
on the swap tick and the next three ticks always returned false (traces at
5728561 and 05648bf), and without a worn elytra vanilla never calls it. Find
what vanilla's own jump-to-glide path checks and sends (input flags, the
start-glide auth input action, equipment sync) and whether the client can
start a glide right after the swap. No faked flags or packets: only a
vanilla path that the server accepts.

### L-93 Schematic: load, place, project, verify and list materials (experimental)
Kind: First scope completed; follow-ups are Ready **(strong model)** or
Research where a new renderer path or behavior needs investigation.
Status: included in 0.1.7 (released 2026-10-07). Loading/placing, ghosts,
checking/materials, HUD/keys/target line, entities, area save, folder opening,
large-file warning, menu/adjust key and the final UI/drawing fixes have local
checks, including the release smoke test. Server and broader coverage remain
open; see [VALIDATION.md](VALIDATION.md).
Requirements, technical notes and the retained decision/build/research record:
[SCHEMATIC.md](SCHEMATIC.md).
Accepted follow-ups (2026-10-07; known gaps, not 0.1.7 blockers):
- Placement markers on the minimap/world map.
- A richer target-card line: expected block icon and differing state values
  (built 2026-10-08, checked in game the same day). Redesigned 2026-10-08
  (maintainer chose the agent's proposal): rows say what to do and take the
  verifier colors (red wrong/extra, yellow state, light blue missing); a
  wrong or extra block reads "Should be [icon] <block>"; each differing
  state is one row "<state>: <now> → <should be>" with readable names
  (L-112), and the block's own row for that state is not repeated.
  Built 2026-10-08 (`SchematicTarget.h`, tested) and checked in game the same
  day, including the drawn change arrow in English and Japanese.
- Beds sometimes drawing only one half or an outline; heads, doors and
  honey blocks still drawing as outlines.
- The Check tab's verifier-colored preview and the Files tab's rotatable
  3D preview.
- Neighbor-dependent ghost shapes using schematic neighbors (B1 done
  2026-10-09; doors draw, connections come from file states).
- The translucent ghost look.
- Entity name-tag distance and quantity.

Screen review (decided 2026-10-08; SCHEMATIC.md "Screen review against the
mockup", mock `docs/demos/schematic-screen.html`). Work order:
1. Screen and world, Ready (strong model), one in-game check for the step:
   placement frames in the world; the Placed list's selection bar and
   progress column; Files folders and columns; Check filter counts and
   differing states in the right pane; Materials sections, HUD switch,
   chest/stack amounts, missing items as slots and the ResourceCalculator
   button; entity ghosts as light-blue outline and faces, name tags without
   limits.
   Built 2026-10-08 and checked in game the same day (VALIDATION-LOG "L-93
   screen review step 1" and "refinements"); shipped in 0.1.8. Commits:
   placement frames (`273fb39`),
   the Placed progress column and selection bar, Files folders and columns
   (`e183d46`), Check filter counts and differing states (`926c9f6`),
   Materials sections, HUD switch, chest amounts, missing slots and the
   ResourceCalculator link (`8dd8457`), entity boxes with faces and every
   name shown (`98d9fe1`).
2. Research, after step 1 or alongside it: L-114 3D previews in the screen,
   L-115 entity models as ghosts, L-116 raw materials from the game's recipes.

0.1.8 rendering and compatibility work (plan 2026-10-09 in SCHEMATIC.md
"Rendering and compatibility plan for 0.1.8"): A saved-file version and a
vanilla-loader check, **done and checked in game 2026-10-09**; B1 schematic
neighbors (doors and their missing halves, mirror axes for block states and
entity facing), **done and checked in game 2026-10-09**; B2 render layers
(honey and slime drawn, blended ghosts and marks sorted), **done 2026-10-09**;
B3 liquids (sloped, flowing, waterlogged; previews too), **done 2026-10-09**;
deferred research (one item, maintainer 2026-10-09): translucency that does not
hide what is behind it, in the world pass (a blended material without depth
writes) and in the previews (water there is opaque), **done 2026-10-09**
(world: blended ghosts and marks drawn after the world's translucent
blocks; previews: shallow real depth, water translucent, cut-out limits
accepted); B4 block entity data (and Lamium saves keep it), **done and
checked in game 2026-10-09**; D waterlogging in the check, **done and checked
in game 2026-10-09**; C measured and tuned (30-50 -> 45-60 fps with several
large placements), **done 2026-10-09**
(B5's block-entity ghost lighting **done and checked 2026-10-09**; looking
straight down and Vibrant Visuals stay on the check list).
Reported 2026-10-09 (maintainer, `c48e59c`): water in waterlogged ghost
stairs showed through the stairs' covered sides (the ghost is translucent,
the real block is not) and its edge cells looked flowing; ghost beetroots
flickered. Built `62bff77`, not yet checked: a waterlogged cell's water is
not drawn on sides its own ghost covers entirely (`sidesCovered`, area of
the side's quads), and quads a ghost repeats (a face in both windings, as
crop planes are suspected to be) are dropped.
Checked 2026-10-10 on `62bff77`: no water through the stairs' sides, no
beetroot flicker. Left then, built `b40c543`: the pool
inside waterlogged stairs still showed the flowing texture (the texture
followed the slope; now it follows the flow, `liquids::flow`, tested: a
still pool leaning toward its walls keeps the still texture, a source at a
drop still flows); farmland beside schematic water flickered on its side
(a water face is no longer drawn against a ghost face in the same plane).
Both checked in game 2026-10-10 on `b40c543`.
Vibrant Visuals (2026-10-09): lines and mistake/selection faces keep their
colors there (done and checked); left, not scheduled: ghost faces untinted,
the honey ghost black, line width (the structure block's outline is wider).
Reported 2026-10-09: in the Files/Check previews biome-tinted blocks
(grass tops, leaves, vines) stayed untinted gray, tessellated above the
build limit where no biome answered. Fixed and checked the same day: cells
are tessellated at the top of the world around the player, so they take the
player's biome (chosen over a fixed tint). Leaves stayed gray (a real
mangrove leaf beside them was green): they draw in the seasons layers, which
the world colors in its own shader. Tinting only the seasons layers changed
nothing (`b02aea7`); built (not yet checked): blocks with a foliage tint
method (leaves, vines) take the renderer's biome tint (`BiomeColorSampling`,
as the minimap) on their still-gray vertices, in previews and world ghosts
alike: checked green (`dac35cb`, VALIDATION-LOG, one oddity noted there).
B-D: Research (strong model), approach left to the agent.

Pick the next follow-up with the maintainer; the list is not an implementation
order. Chosen 2026-10-07: placement markers on the minimap/world map,
built the same day (look in SCHEMATIC.md) and checked in game 2026-10-08.
Deeper map integration (toggling and editing placements from the map) is
part of L-111. Runtime
gaps stay under Pre-release checks and VALIDATION.

### L-30 Ender Dragon multipart hitboxes on Bedrock
Status: research.
The maintainer wants the hitbox overlay to distinguish the dragon's damageable
parts (at minimum head vs body/rest, ideally every real part exposed by the
client) instead of drawing only the dragon's coarse actor AABB.

Java F3+B displays eight damageable sub-hitboxes (head, neck/body, wings and
tail parts). Current public documentation also distinguishes Bedrock head-hit
damage behavior, but that does **not** prove that the Bedrock 26.51.5 client
exposes Java-style part entities or stable per-part AABBs.

- Inspect the current client SDK/symbols and, if needed, a bounded runtime trace
  for dragon-specific part/AABB data used by targeting or damage.
- Prefer the actual client damage/targeting boxes. Do not derive boxes from
  render bones or copy Java dimensions merely to look similar.
- If stable parts are exposed, feed them to the normal hitbox overlay and label
  or color enough to distinguish the head from the other parts. If all eight
  parts are available, render all eight.
- If Bedrock exposes only a coarse box or an opaque internal head test, record
  that limit and leave L-11's ordinary entity hitboxes unchanged.
- Validate against a real Ender Dragon in the End; summoned/custom entities are
  not sufficient evidence for vanilla dragon part behavior.

References for expected Java behavior / Bedrock uncertainty:
https://minecraft.wiki/w/Ender_Dragon
https://minecraft.wiki/w/Tutorial:Hitboxes

### L-33 Mob growth and breeding timers in the target card
Status: research (asked 2026-09-24; not part of L-08).
The maintainer wants the time until a baby mob grows up and the remaining
breeding cooldown. The client SDK has `AgeableComponent::mAge` and
`BreedableComponent::mBreedCooldown`/`mLoveTimer`, but these are behavior
(server-side) components; the client-side actor only receives the synced
`Baby` and `Inlove` flags. Find out:
- whether the client actor carries these components at all (likely not);
- in a local world, whether the in-process server actor with the same
  unique ID can be read safely from the client thread (singleplayer only);
- otherwise show only "baby" / "in love" states, and say so in the help text.
Estimating from observed events (feeding speeds growth up) is not accurate
enough to show as a time.

### L-114 3D previews in the schematic screen
Kind: Research **(strong model)**. Chosen 2026-10-08 (L-93 screen review).
Status: open for the Check tab. The Files preview and the Check tab's
colored preview are built, checked and shipped in 0.1.8. Not done: the Check
tab preview's mistake look and see-through emphasis (decided in the Check
tab review below, then deferred to the 0.1.8 rendering work, which shipped
without them), plus moving the zoomed view and drawing entities, water and
block entities in the previews.
History: Files preview built and checked in game 2026-10-08 (`f2fbf49`);
Check tab colored preview built and checked (`1c0b8bc`). Check tab review
decided 2026-10-08 (SCHEMATIC.md "Check tab review"): layout A, chips drive
the preview, wheel zoom, layers in the preview; the mistake look and
see-through emphasis wait for the 0.1.8 rendering work.
Built and checked 2026-10-09 (`511cb35`): layout A, chips drive the
preview, wheel zoom held at the size that fits the box (the UI scissor
does not clip the mesh), Shift+wheel peels layers from the side chosen when
peeling starts, click to inspect (Files names the block; Check selects its
row or checks the cell on the spot), the Check list keeps the nearest 2000
of each kind. Left for later: moving the zoomed view, entities, water and
block entities in the preview, the mistake look and see-through emphasis. Not drawn yet:
entities, water, block entities (chest, ender chest, shulker box), honey
block and others without an in-world mesh.
How it works (`Preview.cpp`, pure parts in `PreviewView.h`): visible blocks
are tessellated in-world at a spot above the build limit (no real
neighbor or light), moved to their cell, and drawn with the ghosts' block
material into a scissored box. The UI pass keeps the first fragment at a
spot and has no usable depth (giving vertices depth cut blocks apart), so
every quad is sorted near to far for the view's octant; the quads are kept
and only re-sorted when the view turns or recolored when the check
changes. Faces touching a block that covers them (opaque, drawn on this
path) are dropped. Normals all point up and faces are darkened by
direction (lighting through the flattening matrix flashed). Builds 3000
blocks a frame, up to 120000 visible blocks. Drag turns it.
Goal: a rotatable 3D preview of a file in the Files tab and of the selected
placement colored by verifier state in the Check tab (the selected row lit).
Vanilla draws 3D inside UI (the structure block screen), so find that path
first; the ghosts already tessellate schematic blocks into meshes. Stop and
report after two runtime rounds without a working path.

### L-115 Entity ghosts drawn as models
Kind: Research **(strong model)**. Chosen 2026-10-08 (L-93 screen review).
Status: open. A first version is built, checked in game 2026-10-08
(`EntityModels.cpp`, rounds on trace builds up to `ef9bc54`) and shipped in
0.1.8, but it is a compromise (maintainer 2026-10-10): many entities draw
light-blue faces instead of their skin, and details such as cushion
colors are not drawn. Known limits are listed below.
Draw missing schematic entities as their models with the light-blue outline,
not translucent, without a live entity.

How it works:
- Model: `ActorRenderDispatcher::getDataDrivenRenderer(id)` by the entity
  identifier gives the model and default skin, drawn with the renderer's
  `mEntityAlphatestMaterial`. `Model` holds several geometries (adult, baby,
  charged, variants): draw "default", else the first that is not
  baby/charged. Entities without a model keep the dashed frame and the only
  world name tags (logged once per identifier). At most 64 models a frame.
- Placement: `compileCubes` (untransformed, once per part) emits each part's
  quads relative to its bone pivot (`BoneOrientation::mPivot`, absolute) with
  y flipped, cube rotations and inflation applied. Parts are chained in the
  stored y-up space, each turned about its bone pivot; geometry x is mirrored
  in the world, as the game draws it. Rotations in that stored space: x and
  z turn the other way, y as is. Outlines follow the compiled quads.
  `translateTo` and `ModelPart::mRot` are not used (the game writes a live
  entity's pose into the shared model: posing one armor stand moved every
  ghost).
- Pose: bone rest rotations plus the constant parts of the entity's own
  `animation.<name>.*` setup/general animations (an armor stand's
  `default_pose`), rotations and position offsets, only from animations whose
  bones are all in the model (a horse's legacy setup moved the new head).
  Molang is evaluated only when it needs nothing from the entity
  (`RestPose.h`, tested): numbers, `this` as the bone's rest value,
  arithmetic. Legacy `.v1.0` copies are skipped when the current one exists.
  A bone the entity's own animations leave alone takes a rotation that two
  or more other entities' setup/general animations agree on, when all their
  bones are in the model (the witch's crossed arms come from the villager's).
  Borrowing a whole animation family by bone fit was tried and removed: it
  gave traders, strays and polar bears the sheep's head offset.
- Skin: the default skin is one texture of the entity's set; when it is a
  layer (armor, decor, markings, profession, `_none`, baby, saddle, overlay)
  the model draws light-blue faces with the overlay face material
  (`overlay/FaceMaterial.h`, per graphics mode). Horses, donkeys, mules,
  llamas, trader llamas, villagers and rabbits do.
- The saved `Rotation` yaw turns with the placement (`toWorldYaw`, tested).
- `xmake f --schematic_model_trace=y` logs each model's geometries, skin,
  animations, pose and per-part pivots once.

Checked in game: armor stand (default pose, arms on the right sides),
chicken, cow, creeper, witch, wolf, pig, polar bear, turtle, camel, frog,
wandering trader, stray, zombie, drowned, horse family (tinted). Known
limits:
- Poses that need the entity stay at rest: a wolf's tail hangs straight down
  into the body (`query.tail_angle`); zombie-like arms hang down, so a
  drowned's sleeves flicker against its jacket; sitting, walking.
- Layered skins beyond the default: a sheep's wool on head and legs is not
  drawn; variants (cat, rabbit, villager profession) use the default skin.
- An armor stand's own pose (`Pose.PoseIndex` in its saved data) is not
  read. Animations are found by name, so an entity animated under another
  name (donkey: horse) stays unposed.
- Light-blue faces flicker more in Simple graphics (lightning material).
- Many mobs (water, flying, projectiles) were not in the test schematic.

### L-116 Raw materials from the game's recipes
Kind: Research, then Design. Chosen 2026-10-08 (L-93 screen review).
Status: open.
Show the raw materials needed for a schematic's missing blocks (logs for
planks, cobblestone for stone bricks) in the Materials tab, computed from the
recipes the client holds (`Level::getRecipes()`: crafting, furnace,
stonecutter), not from an outside calculator (ResourceCalculator's data is
GPL-3.0; see SCHEMATIC.md). First find what the client actually holds and
how ingredients with several choices (any log, tags) and multi-step chains
(cobblestone, stone, stone bricks) appear; then decide with the maintainer
which recipe to prefer when several make the same item. The calculation is
pure logic with tests.

### L-111 Integration between features (0.2.0)
Kind: Design. Raised by the maintainer 2026-10-08 after checking the
2026-10-07 batch; not chosen for building yet.
Why: the features have matured on their own, and the links between them have
weakened recently. Lamium is one mod, so they can work together more.
Examples from the maintainer:
- Map and Schematics: placements show on the maps (L-93 follow-up), but the
  map cannot toggle their display or edit them the way it edits waypoints.
  The same applies to Shapes.
- The radial menu built for Schematics' many operations could serve the
  whole mod's controls.
- Block Restrictions (L-15) and similar overlays share drawing problems
  (L-110).
Constraints: the module split has real benefits (independent lifecycles,
fail-open per feature, smaller blast radius). Decide with the maintainer
what to integrate, the risks of each step and the order, before any
implementation. Output: a short plan (possibly a demo) that turns into Ready
items.
Status: agent's proposal written 2026-10-08 in [INTEGRATION.md](INTEGRATION.md)
(shared parts features register into: face drawing, map layers, a Lamium
radial menu, looked-at selection, settings cross-links; suggested order and
open questions). Waiting for the maintainer.

### L-105 Performance: find the real bottleneck before optimizing
Kind: Research **(strong model)**. Chosen by the maintainer 2026-10-07 from
their notes.
Status: open. Step 1 only; no optimization is built until it names a
bottleneck.
Why: Hide particles / weather / overlays avoid some drawing, but they are
situational wins, not a way to large frame-time gains. Bedrock is already
native C++ on RenderDragon, so gains that come easily on other platforms may
already be present; measure what is expensive here first. The ideas worth
testing are known in general terms (sources for the hypotheses are in
PROVENANCE.md): rebuild terrain meshes less often or off the critical path,
submit many small draws as fewer batches, skip entities and block entities
that cannot be seen, and cache or make event-driven repeated per-tick work.
Direction: Lamium does not become a renderer replacement. Prefer bounded,
measurable substitutions that skip, cache, batch or defer one expensive
vanilla path while keeping vanilla behavior, capability-gated and fail open
like every version-sensitive path.
Steps:
1. Profiler spike (a build option, like the trace options, never on in a
   release): measure per frame, in heavy real scenes (a village, a large
   farm, a storage room, flying fast over new terrain), frame time and its
   1 % lows, and as far as hooks allow: terrain/chunk drawing, chunk mesh
   rebuild count and time (and on which thread), entity and block-entity
   drawing, particles, HUD/UI, mesh draw-call counts. Record results in
   VALIDATION-LOG.md.
2. Pick at most one or two dominant costs and write a bounded candidate per
   cost (what is skipped, cached, batched or deferred; how vanilla behavior
   is kept; how it fails open). Each becomes its own L-item with the
   maintainer.
Areas to look at first:
- Chunk mesh rebuild scheduling: what triggers rebuilds, how much runs on the
  render or main thread, whether some can be deferred without gameplay
  change. A likely cause of stutter while turning or moving.
- Draw submission: whether meshes, entities, block entities, particles or
  UI still issue many small draws or state changes.
- Visibility: whether entities and block entities behind walls, underground
  or in other rooms still cost a lot. Conservative culling could matter in
  farms, villages and storage.
- Repeated simulation work (lookups, per-tick scans, collision checks,
  short-lived allocations), only after the profile points there.
- Particles: Hide particles skips drawing, not necessarily their creation
  and updates. A "skip particle processing" path may help particle-heavy
  scenes; it is not the main route.
Success: better frame time and 1 % lows in heavy scenes, not a higher
average in an empty world. One proven large bottleneck beats many guessed
micro-optimizations.

---

## Later / parked

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
- Not started, not yet triaged: Mass Craft. It needs a Design pass before
  it becomes a task (Schematic became L-93). (Fast Attack/Use became L-34;
  Scroll Transfer became L-41.)
