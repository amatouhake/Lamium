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

Ready tasks marked **(strong model)** are fully specified but visual or
cross-cutting enough that a strong model should implement them.

A large or open-ended feature (Map, placement and breaking features, later
Schematic) starts with a conversation with the maintainer about what it
should be - purpose, scope, what is left out - before any spec, spike or
mockup (decided 2026-09-28).

**Bugs** (something that ships behaves wrongly) are listed first in their own
section and are fixed before new features. Each bug still has a kind that
decides who picks it up.

Finished and closed items live in [BACKLOG-DONE.md](BACKLOG-DONE.md) with
their full history. An `L-` number referenced elsewhere that is not in this
file is there.

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

1. **Small and medium features**, picked by the maintainer:
   - L-90 Simplified Chinese localization: built and checked in game; waits
     for a native review of the wording.
2. **Placement and breaking — L-15 restrictions and L-59 held placement
   style:** specs written after the 2026-09-28 discussion; building waits for
   the maintainer's go.
3. **Map — L-60 minimap, waypoints and world map:**
   resumed 2026-10-01. Runs in parallel with the small and medium features
   in 1; neither ranks above the other. Steps 1-5 (minimap, cave view,
   radar, waypoints with their screen) and the world map are built, checked
   in a local world and, for the minimap, radar and world map, on an
   external BDS server with a large explored area (2026-10-02). L-82
   ended as a link to an external seed map (done 2026-10-01); seed-based
   biomes and structures are a non-goal. L-83's map/settings UI review is
   done. L-89 distant players is done (2026-10-03). Open: waypoint storage
   on a server and L-86 radar-face follow-ups.
4. **Schematic — L-93 load, place, project, verify and list materials:**
   chosen 2026-10-03; the design conversation is in progress (decisions
   and open questions in the L-item). No code before the spec is agreed.
5. **Research when convenient:** L-37 FreeCamera seeing caves (wanted),
   L-79 carved pumpkin and spyglass frame draw path (cheap-model friendly
   trace/test steps), L-71 starting a glide from the mod, L-57
   client counters, L-30 Ender Dragon part hitboxes, L-33 mob growth and
   breeding timers.
6. **L-73 architecture review:** agreed 2026-09-30, in progress step by
   step (order in the L-item); step 13 goes with L-15 breaking.
7. **Before a release:** the pre-release checks below.

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

Behavior confirmed only on trace builds or only locally. Check these on the
trace-disabled release build before tagging (VALIDATION.md has the gaps per
feature):
- 0.1.6 was released on 2026-10-02 (`v0.1.6`, tag CI passed; asset SHA-256
  `3d864ca1...3946554f`): L-90 Simplified Chinese (first AI-assisted
  translation, corrections welcome), L-88 target hearts, L-75 offhand slot,
  L-63 saturation, L-64/L-92 food values and durability inside the vanilla
  tooltip. The maintainer's smoke test passed on the release-ZIP DLL
  `86f5baf0...2587883f` (`b71c9f8`). No settings migration. After tagging:
  check that the registry PR picks up `v0.1.6` and that LeviLauncher/Bedrinth
  offer it.
- 0.1.5 was released on 2026-10-02 (`v0.1.5`, tag CI passed) at the maintainer's request,
  with the map (L-60, L-85, L-87) as its main change. The release build
  `6add9b9` (DLL `0fae1c58...dc657614c`, from the release ZIP) was deployed
  for a smoke test; no separate result was reported before tagging.
  After tagging: check that the registry PR picks up `v0.1.5` and that
  LeviLauncher/Bedrinth offer it.
- 0.1.4 was released on 2026-09-30 (`v0.1.4`, tag CI passed). Its smoke test
  passed on the release build `4d25424`: the 0.1.4 version, Hand Restock
  (L-66) and offhand totems (L-68), Tool Protection (L-62), Tool Switch fetch
  (L-69), Auto Elytra (L-70) and the L-02 dedicated openers. Still open:
  Hand Restock on BDS and with real latency. The coverage gaps below carried
  past 0.1.4; recheck the relevant ones before the next release.
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
- After tagging 0.1.4: the registry PR picks up `v0.1.4`; check that
  LeviLauncher/Bedrinth offer 0.1.4 with the icon (L-72) once it is merged.
- If possible, a server with real latency for Hand Restock.
- Distant players on the map (L-89) and opaque player markers at any
  height: checked on phone/PC-hosted worlds with a trace build; check on the
  release build and a dedicated server.
- Weapon Switch (L-67, after 0.1.6): checked locally on `7b702da`; check on a
  server (the same-hit equipment packet) and on the release build.

---

## Open decisions

HUD (docs/demos/hud.html), the settings key and the shape model are decided;
see DESIGN.md.

---

## Bugs

### L-73 Architecture review
Kind: Refactor (strong model). Review done 2026-09-30 on main 4d1790b
(read-only); classification and order agreed with the maintainer the same
day. No rewrite: pure logic in headers, feature docs and validation records
are sound. One commit per step; build + LamiumTests after each.
Status: steps 1-10 done 2026-09-30. In-game checks 1 and 2 passed except
Auto Attack/Use (fixed in 221edcb, rechecked the same day) and an occasional
Breaking Restriction hold that stops breaking (cause unknown; carried into B
and L-15). Step 9 concluded that no further camera split was useful: `Zoom`
was renamed `CameraSessions` (file and class), with trace/probe code and
detached-camera state already separated; the main file is 752 lines with 5
`#if`. Step 10 (084b424) was checked in game. Steps 11 and 12 were dropped
after review (see D). The only remaining step is 13 with L-15.

Fix (can cause wrong behavior)
- A. Breaking Restriction and Tool Switch read and write their
  `restartPending` flag before checking that the call is the client's own
  player. In a local world the integrated server's player runs the same
  GameMode calls (L-31), so it can consume the client's restart (a held
  attack then does not resume) or restart the server's session; possibly
  from another thread. Tool Protection already filters first. In game:
  singleplayer, hold attack across a rejected block and back; Fetch from
  inventory wait and restart while held.

Tidy (agreed)
- B. Shared mining-session control for Breaking Restriction, Tool Switch
  (L-69) and Tool Protection (L-62). Their order is implicit in hook
  priorities (Highest/High/Normal); each pause uses `stopDestroyBlock` and
  each restart re-enters the whole chain through `startDestroyBlock`, which
  Tool Protection counts as a new press and Tool Switch's stop hook sees as
  its own. Do it with, or just before, the L-15 breaking step: pure header
  and tests first, then move one feature per commit. In game: all
  combinations.
- C. Split `Zoom.cpp` (1,195 lines, 21 `#if`): trace/probe hooks and helpers
  to `CameraTrace.cpp`; camera component save/restore (detach, offset,
  body) to its own file; then decide whether Zoom (magnification, FOV,
  wheel, sensitivity) moves out. Freelook and FreeCamera share one detached
  session by design and stay together. Keep the `Zoom` facade (about 40
  call sites). In game: Zoom, Freelook, FreeCamera, F5, dimension change,
  leaving the world; also build with camera_trace and both probes.
- D. `SettingsScreen.cpp` (1,878 lines). Closed after step 10 (maintainer,
  2026-09-30): the Shapes view and the input listeners use 20+ screen-wide
  variables and the Shapes list also renders inside the table, so a file
  split would only move text behind a header of shared variables. Split it
  when the screen grows again, after grouping its state first. Original
  plan: The pure parts are already out
  (SettingsTable, SettingsNavigation, ShapesLayout, ShapeEditor, NumberInput,
  SearchQuery); what remains is about 80 file-scope variables under one
  mutex. First, Enter/Esc/Tab while editing a number or a shape name saves
  settings and shapes from inside the key event; defer that to the frame.
  Then move the Shapes view and the input listeners to their own files. In
  game: search, number entry, key binding, shape editing, HUD layout.
- E. Runtime feature table: one ordered list of start/stop, stopped in
  reverse. Correction (in-game check 2026-09-30): periodic input and the
  automation trace must start in `load()`; they capture the button handlers
  the client registers between load and enable. Moving them into enable()
  (5e877e5) stopped Auto Attack/Use; 221edcb restores the load() start.
- F. One budgeted trace helper instead of the four `trace(stage, value)`
  copies (ElytraSwap, ToolGuard, HandRestock, InventoryMove) and Zoom's own
  budget loops. The trace-only files (with stubs) already follow the rule;
  keep `#ifdef` for all research traces.
- G. SettingsStore fallbacks: 96 literal defaults repeat `Settings.h` (none
  differ today; camera already uses the struct value). Use the struct value
  everywhere and test that each empty section decodes to `Settings{}`. No
  schema framework.
- H. HideOffhand removes all three hooks on stop even when not installed;
  give each an installed flag. No general HookSet (HideEffects needs
  per-hook fail-open).

Not now
- Moving the totem watch out of `HandRestock.cpp` (about 50 lines sharing
  Restock state, validated in game).
- Test registration: every suite and test function is called today.
- `Runtime::preferences()` copies, `settings::find()` linear scan, JSON write
  per change: profile first.
- Runtime log levels, renaming `Zoom`, test layers (BDS, computer-use): a
  separate Research item if wanted.

Order: 1 A; 2 test that empty sections decode to defaults; 3 G; 4 F; 5 H;
6 E; 7 C trace/probe; 8 C camera state (in-game check); 9 decide on the Zoom
split; 10 D deferred save; 11 D Shapes view; 12 D input listeners (in-game
check); 13 B with L-15 (in-game check). In-game check 1 follows step 1.

---

## Design

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
Kind: Design done (discussion with the maintainer, 2026-09-28); breaking is
then Ready **(strong model)**, placement needs Research first. Replaces the
current Breaking Restriction (capture/reset keys) and the unimplemented
placement mode.
Status: planning. Nothing is built, and no step starts until the maintainer
says so.
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

### L-90 Simplified Chinese localization
Kind: Design decided, then implementation. Chosen by the maintainer
2026-10-02.
Status: built 2026-10-02 (agent-drafted text for all keys, `TranslationsZhCN.h`
with a build-time order check, docs/TRANSLATING.md). Checked in game on
`ca25c2c` (fit, baseline and behavior fine; no Latin raise needed). Open: a
native review of the wording, invited from FeixiangTMC as a PR. Hold the release that first
ships it until the review or a "draft, corrections welcome" note is decided.
Add Simplified Chinese (`zh_CN`) as Lamium's third official UI locale.
English and Japanese remain supported; Traditional Chinese is not claimed
until there is actual demand and a separately reviewed translation.
Scope:
- Translate user-facing Settings text, feature descriptions, editor/prompt
  text and toasts. A full translated README is not required for this item.
- Replace the fixed two-language `Entry { key, english, japanese }` shape
  with a translation representation that can add another locale without
  duplicating lookup logic at call sites.
- Match the game locale to `zh_CN`; unsupported locales still fall back to
  English.
- Keep every locale complete. Tests must fail when a shipped translation key
  is missing in English, Japanese or Simplified Chinese.
- The first Chinese wording may be prepared by an agent, but Minecraft/mod
  terminology corrections from native users are explicitly welcome. Add a
  short contribution note when the locale ships.
Do not generate Traditional Chinese by mechanical conversion and present it as
official support.

### L-94 Swap the held item with the offhand, including items the offhand cannot hold
Kind: Design. Taken up by the maintainer 2026-10-05 after a public request.
Status: open; nothing is built and no step starts until the maintainer says so.
What it is for: one action that puts the selected item in the "second hand"
and brings the second hand's item back, also for items Bedrock does not let
the real offhand hold. Fake Offhand (L-49) only borrows a hotbar slot while a
block is placed; it never touches the real offhand slot. `InventoryMove`
already moves items between the inventory and the real offhand (L-68).

Proposed behavior (to confirm with the maintainer):
- An item the real offhand accepts is swapped with the real offhand. The game
  and server stay authoritative; Lamium never forces an unsupported item in.
- An item it does not accept is swapped with the Fake Offhand target hotbar
  slot.
- A new action, unbound by default, in the Inventory group beside Fake
  Offhand (append to `enum Action`, never reorder).
Bedrock has no built-in default offhand swap key (confirmed in game by the
maintainer, 2026-10-05), so this action is the only swap key and there is
nothing to extend.
Open questions:
1. When the selected slot is the Fake Offhand target slot, or the target slot
   is empty or holds the same stack, what happens and what is shown?
2. Feedback when there is nothing to swap (toast, silence).
3. Interaction with Hand Restock's offhand totem refill and Tool Protection.
Related, not decided: hotbar slot ownership. The Fake Offhand target slot is
an ordinary slot, and other automation (Tool Switch, Weapon Switch, Hand
Restock from the hotbar) may use it. Ideas: keep it empty and out of other
automation's reach; move a stray item into the main inventory after the
server update when there is room, and do nothing when there is not. Settle the
behavior here first and share a helper only when a second feature needs the
same rule (L-97 is the likely one); no reservation manager up front.

### L-97 Tool Switch and Weapon Switch: fetch into a fixed hotbar slot
Kind: Design. Split from L-94 by the maintainer 2026-10-05.
Status: open; nothing is built and no step starts until the maintainer says so.
Today "Fetch from inventory" (L-69, EQUIPMENT.md) swaps the chosen inventory
item with the selected slot, so it overwrites whatever the player held there.
Idea: let the player name a hotbar slot as the destination, so the fetched
tool or weapon always lands in that slot and the other hotbar slots keep their
layout. Reuses the screenless `InventoryMove::movePair()` path.
Open questions:
1. The tool must be in hand to be used, so the fixed slot has to become the
   selected one. Does the previous slot come back afterwards? Weapon Switch
   has no switch back today, and Tool Switch keeps the tool selected.
2. What happens to the item that was in the fixed slot (it goes to the
   inventory slot the tool came from, as in a swap) and when that item is
   itself a good tool or weapon?
3. One destination slot for both features or one each; the setting shape and
   the default (today's behavior, selected slot).
4. Decide how a fixed destination interacts with a Fake Offhand reserved slot
   (L-94); the conflict policy is not decided.
Keep the existing 150 ms pacing, the server confirmation and the "never one
about to break" rule unchanged.

---

## Research

### L-91 Glint missing on some icons Lamium draws
Kind: Research. Found by the maintainer 2026-10-02 while checking L-75.
Status: open.
Lamium draws item icons with `ItemRenderer::renderGuiItemNew` and a second
foil pass when `Item::isGlint` is true (container previews, the L-75 offhand
slot). The enchanted golden apple shows its glint, but an enchanted shield
shows none in either place although `isGlint` returned true for it (log on
`3360b37`); vanilla inventory slots show the shield's glint. Shields likely
use a different icon path: the maintainer notes the shield icon looks drawn
in 3D (it can carry banner patterns), and other non-sprite icons may behave
the same. `isGlint` is the right predicate (enchanted books, golden apples and
lodestone compasses shine without enchantments); do not replace it with
`isEnchanted`. Find how vanilla slots draw the glint for such icons and use
the same pass in both places.

### L-95 Fake Offhand beyond block placement
Kind: Research **(strong model)**. Taken up 2026-10-05; related to L-94.
Status: open.
Fake Offhand selects its target hotbar slot only around a block placement. The
request is to use other items the same way: use on a block, use in the air and
hold-to-use items. Removing the placement guard is not enough: the temporary
selection has to live through the vanilla use lifecycle (start, hold,
completion or cancel) and the original slot must be restored afterwards.
Hand Restock already follows that lifecycle and is the first place to look.
Find out:
- which game calls start, continue and finish a use, and when it is safe to
  switch the slot back for a held use (eating, drawing a bow);
- what the server sees and whether servers accept it (server authority; fail
  open to vanilla when a step cannot be confirmed);
- which items are worth supporting first, and which must stay vanilla
  (containers, buckets, anything the real offhand already handles).
Output: a short spec with the supported use kinds, then Ready steps.

### L-96 Connected textures, starting with glass
Kind: Research **(strong model)** first, then Design for the settings.
Taken up 2026-10-05 after a public request.
Status: open; default off and Experimental when it ships.
What it is for: blocks next to a block of the same kind draw as one surface,
without the border line between them. Start with glass, then stained glass of
the same color, then glass panes; panes joined to blocks, different colors
and border options come later.
Approach to test (a feasibility hint is recorded in PROVENANCE.md): change how
chunk faces are tessellated, look at the neighbor through the block source and
crop the texture coordinates at the touching edge; changing the setting marks
chunks dirty so they rebuild.
Constraints:
- This changes vanilla chunk tessellation, which is more version-sensitive
  than the schematic ghosts' private tessellator. Capability-gated and fail
  open: on an unverified game version do nothing.
- Bound the neighbor lookups; a toggle rebuilds chunks.
- Glass is the research target. If the approach extends cleanly, a
  table-driven design (which blocks, what they connect to, which edges are
  cropped) can be considered later; do not require one up front.
  Resource-pack-defined tile sets are not promised. Widen beyond glass only
  after surveying which vanilla blocks have an inner border line.
Research output: the function(s) that can be intercepted on this game
version, whether the crop works for glass and panes, the cost on a large
view distance, and how it behaves with Vibrant Visuals / Deferred rendering.

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

### L-37 FreeCamera sees caves from underground (reopened)
Kind: Research. Reopened 2026-09-30: the maintainer wants it. Four traces and
the parked write-up are in BACKLOG-DONE.md (L-37).
Status: open.
Known: underground FreeCamera uses culler type 3 like survival; spectator uses
type 5. Answering spectator from `Actor::isSpectator` or
`getPlayerGameType` did not change the culler.
New hypothesis (source: GroupMountain FreeCamera README, a GPL-3.0 BDS plugin,
PROVENANCE.md group 3; its source is not opened): that plugin shows caves by
making the client really switch to spectator through the server's game-type
packet. So the culler may follow the client's actual game-type change (the
path the packet handler takes), not the queried value. Check whether applying
that change locally during FreeCamera, and restoring it after, selects type 5
without changing server-side game mode, abilities the server checks, or
movement sent to it. Fail open to the current behavior if it does.

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

### L-60 Map: minimap, waypoints and world map (experimental)
Kind: Design done for the minimap (step 0, 2026-09-28); the steps below are
Research then Ready **(strong model)**. The world map still needs its own
design discussion. Chosen by the maintainer 2026-09-28 as the next large
feature.
Status: resumed 2026-10-01. Steps 1-5 and the world map built and checked
in a local world on 2026-10-01. On an external BDS server with other players
and several thousand blocks explored, the minimap, radar and world map
behaved as locally (reported 2026-10-02); players beyond entity tracking
range now show faded from the locator state (L-89, done 2026-10-03).
Decision record: L-60 was chosen when no LeviLamina map mod with a minimap,
world map and waypoints seemed to exist (ChiyanMap was gone), and put on hold
2026-09-30 until the maintainer had used CoralMap (CC0-1.0, reference-only,
PROVENANCE.md group 3), which per its README lacks waypoints, radar, cave
view and the death point. On 2026-10-01 the maintainer decided to build a
full map in Lamium without trying CoralMap: it has too few features, and
ChiyanMap, though feature-rich, left the mouse cursor free after its world
map closed, scattered its settings and felt rough to operate. Running
several mods together (Lamium, LHolo, ChiyanMap) also often left the
Minecraft process running after exit; the cause is not identified, so
Lamium's map must not become one (see Map-wide requirements).
A client-side map built from the chunks the client has loaded: a minimap HUD
element with a radar and waypoints first, then a full-screen world map backed
by an on-disk cache. It ships default off with the Experimental badge and
grows on main in steps (Release policy above). Look agreed in
[demos/minimap.html](demos/minimap.html).

Lamium's map is specified here and implemented independently on
LeviLamina/Bedrock APIs. ChiyanMap (GPL-3.0) and the current LeviLamina map
mods are reference-only (PROVENANCE.md group 3). The maintainer keeps
ChiyanMap recovery material outside the repository (local path in
`AGENTS.local.md`, when present); planning may use its notes, but whoever
writes Lamium map code works from this spec and does not open the recovered
source.
The implementation should use a player-centered scan spread over frames with
an explicit budget, retain owned height/color data for shading, partition
persistent data by world and dimension, and build the world map from bounded
cached regions rather than a single unbounded texture.

#### Map-wide requirements (decided 2026-10-01)
These apply to every step, the world map and L-82.
- Clean shutdown: every scan, bake or disk-cache thread or task is stopped
  and joined on world exit, dimension change, disabling the feature and mod
  shutdown. Shutdown never waits on the game's threads or blocks in DLL
  unload. In-game checks for steps that add background work include quitting
  the game and confirming the process ends (`tasklist` shows no
  `Minecraft.Windows`).
- Cursor and input: a full-screen map opens as a game screen rather than by
  releasing the mouse by hand. When it closes, when the window loses focus
  and on world exit, mouse capture and look control return to normal play.
  Its in-game check: open, close, Alt+Tab away and back, then turn the view.
- One place for settings: every map option, including anything changeable
  from inside the world map, lives in the "Map" settings category and is the
  same setting wherever it is shown.
- The world map opens fast: regions already cached show at once, and new
  areas fill in progressively without blocking the screen or the game. The
  maintainer found a slow-to-generate world map a constant annoyance.
- Operation is designed before it is built: the world map gets a mockup in
  `docs/demos/` agreed with the maintainer (dragging, zoom, adding and
  editing waypoints, closing) before implementation, as the minimap did.

#### Minimap spec (decided with the maintainer, 2026-09-28)
Map
- Square, north up; the player is a white arrow with a black edge, modeled
  on the vanilla map's player marker (check whether the game's own map icon
  can be drawn at runtime). Rotating (heading up) and round are settings.
- About 128 x 128 blocks by default; zoom steps from 16 to 512 blocks
  across (16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512; finer steps
  decided 2026-10-01, saved as the width in blocks). Chunks the client has not loaded stay blank: a dark,
  half-transparent fill in the card color (decided 2026-10-01; transparent
  looked wrong). Known empty ground (a drop in the cave view, the End's
  void) is an opaque dark color instead, so it reads differently from
  ground not loaded yet. Nothing is requested from a server.
- A thin 1-unit frame only; nothing is drawn outside the map except the
  compass letters, which sit on the frame (2026-10-01).
Terrain
- A representative color per block, biome tints for grass, foliage and
  water (the color the world shows, not the vanilla map item's palette:
  the first build used that palette and looked unlike the world), and height shading against the north and west neighbors, at the
  strength shown in the mockup. No day/night darkening.
- Under a ceiling the map switches to a cave view on its own: floors near
  the player's height bright, walls dark. The Nether always uses it. A key
  can force the cave or surface view.
- The cave view must stay calm (2026-10-01, after the first cave build
  redrew on every one-block height change): it is drawn around a held
  height that moves only when the player is more than 3 blocks from it,
  over a window from 5 above to 24 below that height, with depth-based
  brightness; the roof check uses most of the 3x3 columns around the
  player; automatic switches wait 1 s after the previous one. Possible
  later refinement, not decided: hold the height while the player stays in
  the same cave space rather than by distance.
Radar
- Simple dots by kind, each kind a setting: other players (light blue, with
  their name), hostile mobs (red) and passive/neutral mobs (white) on by
  default; dropped items (yellow) off. Every dot has a thick black ring.
  Dots 8 or more blocks above or below the player are drawn fainter, except
  players (decided 2026-10-03, L-89): faint means "out of range" for them.
- On by default; the help text notes that some servers may treat seeing mobs
  and players through walls as unfair.
- Later, as an option: per-mob icons (for example from the spawn egg, as the
  Target card does); dots stay the default.
Waypoints
- Add one at the current position with a key; a small prompt asks for the
  name and color. Only the last death point is recorded automatically, with
  its own cross marker; it can be turned into an ordinary waypoint. No
  teleporting.
- On the minimap: diamonds in the waypoint's color; those outside the map
  sit on its edge pointing their way.
- In the world: a small colored diamond in the waypoint's direction with the
  distance ("128 m"); the name appears when the crosshair is near it.
- A dedicated Waypoints screen like Shapes, pinned at the bottom of the
  settings sidebar: list, name, color, coordinates, shown/hidden, delete.
- Stored per local world, or per server address and port, then per
  dimension. Lobby-style servers with several worlds share one set for now.
Following (decided 2026-10-01)
- While FreeCamera flies, the map's center and heading follow the camera,
  and the player's arrow is drawn where the player is (hidden when off the
  map). No setting. Freelook keeps following the player.
Text, placement and settings
- Optional lines below the map with a shadow, all default off: coordinates,
  biome, compass letters (N E S W). No clock. Decided 2026-10-01 after the
  first build: the lines are centered on the map whatever its anchor; the
  compass letters are centered on the frame (inside the map they were hard
  to read), all white, no special color for north.
- Its own HUD element, default top right at a medium size (about a fifth of
  the screen height; a "Map size" setting, 10-50 % of the screen height in
  steps of 1, changes the map alone while the layout scale still scales
  the whole element, decided 2026-10-01), movable and scalable in the layout editor. Hidden while
  Debug View is shown (a "hide while Debug View is open" switch like the Info
  HUD's and Target's); not hidden while zooming or in FreeCamera.
- A new settings category "Map" holds the minimap, radar and waypoint
  options (and later the world map), with the Waypoints screen pinned at the
  bottom of the sidebar. Decided 2026-10-01 (first build had one feature
  with 19 children): the category lists features of their own: "Minimap"
  (switch, key; range, size, turning, round, Debug View hiding, zoom and
  enlarge keys, layout link), "Map text" (a heading without a switch:
  compass letters, coordinates, biome), "Cave view" (no switch, its key
  forces the other view), "Radar" (switch; one row per kind and the
  invisible option), and later "Waypoints".
- Bindable actions without default keys: minimap zoom in/out, minimap
  show/hide, add a waypoint, force the cave/surface view, and (2026-10-01)
  enlarge while held: twice the side and twice the area at the same scale,
  at most 85 % of the screen height. New actions are
  appended to `enum Action`.

#### Steps
Each step lands on main behind the default-off switch and ends with an
in-game check by the maintainer. Pure logic goes in headers with tests.

Built 2026-10-01 (steps 1-2, unchecked in game): `features/map/MapView.h`,
`MapTiles.h`, `MapImage.h` (pure, `tests/MapTests.cpp`) and `Minimap.cpp`.
The whole image (terrain, arrow, frame) is composed on the CPU into a 256 px
RGBA texture uploaded with `uploadTexture` and then `updateTextureInPlace`.
Scanning runs on the client thread at 1.5 ms per frame (nearest chunks
first, near chunks rescanned every second, far ones less often); there is no
background thread yet. Second build (same day, after the first check):
colors are the average of each block's top texture (`BlockGraphics`, image
from the texture group, at most 6 new textures per frame, cached by block
state and path) times the biome tint (`BiomeColorSampling` map grass/foliage
colors, water color), with the old map color only as a fallback; the
surface is the heightmap block, or the covering block just above it (snow
layer, carpet), looking through glass and plants; shading is cached per
chunk so turning recomposes every frame; lines centered, compass letters on
the frame and all white. Third build (same day): cave view (step 3) with
its own tile cache per view, the Nether always in it; floors are found in
a column from 2 above to 24 below the player's block (rock at feet and head
is a wall), cave chunks rescan when the player's height changes by more
than 1; automatic switch when covered and sky light <= 6 (back at >= 10);
a "minimapview" key forces the other view and returns to automatic; the
dark fill for unloaded ground; FreeCamera following. Fourth build: the
calmer cave view above; blocks the client has not received yet (its
request stand-in blocks) leave the column unknown and get the chunk
rescanned soon; bounded log lines name stand-in and colorless blocks.
Fifth build: a chunk counts as received unless all its columns are
stand-ins, empty columns are the dark "known empty" color; the map size
setting and the hold-to-enlarge key (texture 512 px while enlarged).
Sixth build: in the cave view stand-in blocks count as rock without a
color (the client leaves hidden sub-chunks unrequested, which left holes
in the Nether); the finer range steps and 1 % size steps; the center
snaps to whole pixels when a pixel spans several blocks, stopping the
shimmer at wide zoom; chunks kept for the enlarged view.
Step 4, radar (built 2026-10-01, `MapRadar.h`): dots from the client's
actor list each frame (owned positions, kinds and player names only),
sorted nearest first and capped at 256, placed in whole texture pixels and
drawn into the image (dot 3 and ring 2 mockup pixels, palette from the
mockup's option B); invisible and dead actors are left out; hostile means
the Monster type flag (hoglins count as passive for now); player names in
small text beside their dot, flipped left near the right edge; a "Radar"
switch plus one per kind, items off. After the first radar check
(2026-10-01): the kind rows are named "Radar (players)" and so on; dots
keep their mockup size up to 128 blocks across and shrink with the square
root of the range, to half at 512; "Radar (invisible ones too)", default
off, also shows invisible players and mobs (help text notes servers may
treat it as unfair). While enlarged, the arrow, dots and name gaps keep
their normal on-screen size. The map center and the dots use positions
interpolated with the frame's tick fraction (read in a world render hook;
without it, ticked positions). That alone did not stop the wobble while
running (maintainer, 2026-10-01): terrain and dots were quantized to pixels
separately. The center now always snaps to the texture's pixel grid along
the map's own axes, so both step together; the arrow is placed from the
unsnapped center and stays in the middle.
A world join/exit or a
dimension change discards the data; turning the minimap off unloads the
texture. Any exception turns the minimap off for the session (fail open).
Not yet built from step 2: the vanilla map marker check (the arrow is drawn
by Lamium).

1. Texture spike (Research): build an RGBA image at runtime
   (`cg::ImageBuffer`), register it through
   `IClientInstance::getTextureGroup()` / `mce::TextureGroup::uploadTexture`,
   draw it on the HUD with `MinecraftUIRenderContext::drawImage`, and change
   its pixels each second with `updateTextureInPlace`; a checkerboard is
   enough. Measure a full update at minimap size (about 256-512 px) and
   record what happens on world exit, dimension change, resource reload and
   window resize. If this path fails, record why and ask before considering
   anything at the DirectX level. Also check whether the vanilla map's
   player marker texture can be drawn. Not shown in settings.
2. Surface minimap (Ready once step 1 works): scan top blocks around the
   player from the client's `BlockSource` (height map, block, biome) within
   a per-frame time budget, keep owned colors and heights, shade and upload;
   the HUD element, frame, player arrow, zoom steps, rotating/round options,
   the text lines, the "Map" settings category and the Debug View hiding.
   Background scan/bake work carries world + dimension generation identity
   and discards stale completions. Keep full-dirty data changes separate from
   presentation-only refreshes where that avoids unnecessary work. Tests:
   block/biome color and shading math, negative-coordinate chunk/region math,
   world-to-map transforms (north-up and rotating), zoom steps, scan
   scheduling and stale-result rejection. In game: include negative
   coordinates, Nether, quick world re-entry and dimension changes. First
   Experimental release point.
3. Cave view: detect a ceiling, scan floors and walls around the player's
   height, the Nether always in cave view, the force key. Tests: ceiling
   detection and floor selection on synthetic columns.
4. Radar: collect nearby players and mobs each frame (owned positions and
   kinds only), the dot kinds and colors, fainter dots above/below, player
   names, the per-kind switches. Tests: kind classification and the
   above/below rule.
5. Waypoints: storage per world/server/dimension (tolerant JSON like the
   shapes store), the add prompt, the Waypoints screen, markers on the
   minimap (edge clamping) and in the world (direction, distance, name near
   the crosshair), the last death point. Tests: storage round trip and keys,
   edge clamping, marker projection.
   Decided 2026-10-01 with [demos/waypoints.html](demos/waypoints.html)
   (all recommendations accepted): the add key opens a small centered
   prompt with the name "Waypoint N" (地点 N) preselected and 12 colors,
   the next color after the last one used; Enter adds, Esc cancels; the
   position is the block under the feet when the key was pressed; closing
   it restores mouse capture. The Waypoints screen is built like Shapes
   (list and detail, dock button), nearest first, this dimension's entries
   first and others grayed with a dimension tag, the last death point in
   its own group on top with "Make a waypoint"; delete takes two presses.
   Settings feature "Waypoints" (switch: all markers; key: add here) with
   "Show in the world", "Show on the minimap", "Record the death point",
   a key to open the screen and a key that hides world markers while held.
   An option, default off, shows Overworld waypoints in the Nether at 1/8
   of their coordinates (and Nether ones in the Overworld at 8x). World
   markers get an optional distance limit (default none, 100-10000).
   Built in three parts, each checked in game: 5a (built 2026-10-01,
   unchecked) storage (`WaypointStore`, local world `lamium/waypoints.json`
   beside shapes, servers `config/waypoints/<host>_<port>.json`, Realms and
   other connections without an address for the session only; a file that
   fails to load is never overwritten), the add key and prompt (a mode of
   the settings screen, so it owns the cursor like it), minimap diamonds
   with edge clamping and the death cross, the death point (noticed in the
   world render hook because the death screen hides the HUD, saved from the
   next HUD frame), the "Waypoints" settings feature with minimap, death
   and cross-dimension switches. 5b: world markers, the distance limit and
   the hide-while-held key. 5c: the Waypoints screen and the open key.
   5a passed in a local world on 2026-10-01 (server not checked). The
   death cross is red (maintainer, 2026-10-01: white read as a passive
   mob's dot; the cross shape tells it from red hostile dots). 5b built the
   same day: the camera the world was drawn with is copied in a setupCamera
   hook that runs outside the FreeCamera and Zoom hooks, and markers are
   projected onto the HUD from it (axes from the view matrix, scale from
   the projection matrix; no camera means no markers); pixel diamonds 7
   units across and the cross from rectangles; the distance below, the name
   above it when the crosshair is within 20 x 30 units; far markers drawn
   first; "Show in the world", "Show up to" (0 = any distance, steps of
   100 up to 10000) and the hold key. After the 5b check: the camera's
   world position comes from the entity pass (`mCameraPosition`), because
   setupCamera is camera-relative; "Show in the world" is a choice, decided
   2026-10-01: Always (the key hides while held) / While the key is held
   (the key shows them) / Off (the key does nothing), with the key on that
   row. Markers are placed on whole screen pixels, not GUI units (checked
   smooth in game on `189c554`). 5c built 2026-10-01: the Waypoints screen
   reuses the Shapes layout (`ShapesLayout`; header: "Show all" switch, key
   settings link, dock button; toolbar "+ Add here"); a fourth pinned
   sidebar item between Shapes and HUD layout; the list is the death point
   (if any) then this dimension's waypoints nearest first, then the others
   by name, each with its distance or dimension and a shown switch; the
   editor has the name, a large diamond with coordinates, dimension and
   distance, and X / Y / Z (typed or stepped), Move here, Show and Color
   rows; the death point offers "Make a waypoint" (named "Death point",
   the death record cleared) and delete; delete takes two presses; an
   "Open the Waypoints screen" key under the Waypoints feature.

#### World map (step 0 done; first build 2026-10-01, not checked in game)
Step 0 started and finished 2026-10-01. Decided by the maintainer (all as proposed):
- Full screen with thin top and bottom bars, opened by a key (default M),
  closed by the same key or Esc; the game is not paused; HUD and minimap
  are hidden while it is open. Left drag pans, the wheel zooms about the
  cursor, north is up.
- Waypoints from the map through a right-click menu: on empty ground "Add
  here" (the usual add prompt; Y is the recorded surface, else the
  player's Y); on a waypoint "Edit" (opens its row in the Waypoints
  screen), hide/show and delete (two presses); on the death point "Make a
  waypoint" and delete. No teleport.
- Recording runs whenever the world map feature is on, whether or not the
  minimap is shown, with a per-frame budget.
- The on-disk cache has no size cap; the Map settings show this world's
  usage and a two-press clear.
Proposed with them (not questioned): 256×256-block region files of color
and height, stored like waypoints (a local world's lamium folder; for a
server Lamium's config folder per address and port), per dimension;
cached regions read nearest first off the client thread; the minimap
reads the same cache; the Nether recorded in 16-block layers, Overworld
caves not recorded in v1.
Mockup: [demos/worldmap.html](demos/worldmap.html), agreed 2026-10-01 with
every recommendation: the bars as drawn, a dimension switch to view the
others (the player's own marked), the Nether layer stepper with "My height"
(on by default), waypoint names always shown, the menu items as drawn, and
the settings rows (feature switch with the open key M, Nether layer, saved
map size with a two-press delete of the whole world's map).
Build notes (first build): `MapRegion.h` (region data, file format "LMR1"
with runs, shading, 2:1 downsampling), `WorldMapView.h` (view, zoom steps
1/8-16 GUI units per block, image levels), `MapStore.cpp` (regions near the
player kept in memory; a worker thread per joined world reads and saves
region files, builds images by level from regions or the level below with
an LRU of about 40 MB, and is joined on world exit and mod stop; damaged
files count as missing), `Minimap.cpp` `record()` (scans 384 blocks around
the player with its own 1 ms budget whenever the world map is on; chunks
the client has not loaded are filled from the saved regions, Overworld
caves are not recorded), `WorldMap.cpp` (the screen: a pool of 96 runtime
textures, at most 6 uploads per frame, 16 in the first second, a coarser
image stands in until a tile is ready), and a world-map mode in the
settings screen's scene (cursor ownership as for the other screens; the
add prompt opened from the map returns to it; "Edit" opens the Waypoints
screen on that waypoint). Saved under the world's `lamium/map/` or
`config/map/<host>_<port>/`, per dimension (`overworld`, `nether/y<N>`,
`end`). Region edges shade against level ground. L-82 links from the
world map to an external seed map.
After the maintainer's first use (2026-10-01) a review mockup,
[demos/worldmap-review.html](demos/worldmap-review.html), was agreed with
every recommendation and built the same day (not checked in game yet):
- Top bar B: short dimension names (Overworld/Nether/End, 地上/ネザー/
  エンド), no title, close as a drawn cross; "My height" folded into
  "Center on me" (it also returns the Nether layer to the player's height;
  the layer label is accent-colored while it follows). The two-row
  fallback stays for screens too narrow.
- A "World map" item pinned in the settings sidebar (between Waypoints and
  HUD layout). Opened from there, closing the map returns to the settings;
  opened by its key, to the game. With the feature off the sidebar still
  opens it and shows the saved map without recording ("Recording is off").
- A waypoint side panel on the map, toggled by the top bar's "Waypoints"
  button and remembered (`map.worldMapPanel`, no settings row): this
  dimension's death point and waypoints nearest first with shown
  switches; a row moves the map to it; clicking a marker selects it; the
  editor has the name (typed on the map), X/Y/Z steppers, Move here, Show,
  colors, "Open in screen" and a two-press delete. The right-click "Edit"
  selects in the panel.
- The Waypoints screen opened from the map shows "< Map" instead of
  Close; it and Esc return to the map with its view kept.
Checked in a local world 2026-10-01 on `e6781e5` (all points passed).
L-83 completed 2026-10-01 (see BACKLOG-DONE.md): the settings/UI consistency
review covered the map and Waypoints screen. Further map UI changes are new
work, not an open part of L-83.

### L-93 Schematic: load, place, project, verify and list materials (experimental)
Kind: Design (in progress). Chosen by the maintainer 2026-10-03.
A client-side schematic subsystem for building from a saved structure: pick
a file, place it in the world, see it as ghost blocks, compare it with what
is built, and see which materials are still needed. It ships default off
with the Experimental badge and lands on main in steps (Release policy
above); steps that are not usable yet stay out of the settings screen.

Decided 2026-10-03:
- Independent implementation. LeviSchematic (the maintainer's fork) stays
  reference-only (PROVENANCE.md group 3): it shows what was feasible
  (`.mcstructure` load/save, ghost projection, transform, world comparison,
  selection) but its code is not incorporated. The maintainer wants to own
  the code for design freedom and licensing. Whoever writes the Lamium code
  works from this spec and does not open LeviSchematic source.
- One design for the whole first scope: schematic browser, placement
  (move, rotate, mirror), ghost projection, layer controls, verifier and
  material list are specified together, so later parts are not bolted on.
  Implementation may still land in steps.
- File format: `.mcstructure` only. Java `.litematic` import is a possible
  later addition, not part of this item.
- Ghost look: translucent real block models are the target, on one
  condition: their brightness must not depend on the world's light level
  (a projection in a dark cave reads as well as one in daylight). If that
  cannot be done, another look is chosen with the maintainer. Research
  first.
- Operation: a dedicated screen plus keys, consistent with the rest of
  Lamium (DESIGN.md "Tools with their own state get a dedicated view").
  No held-item tool (no stick or wand selection); that may be revisited
  only if users ask.
- Later, not in this item: placement guidance, schematic-aware placement
  restriction, hotbar item selection and placement assist; the entity and
  block-entity follow-ups listed below.

Also decided 2026-10-03 (the recommendations, accepted):
- Saving is in scope: select an area in the world without an item and save
  it as `.mcstructure`.
- Files live in `mods/Lamium/schematics/` (subfolders allowed); the browser
  has "open folder".
- Several placements at once, remembered per world (servers by address and
  port) and dimension across sessions.
- Verifier: not placed shows the translucent ghost, correct hides it, wrong
  block is red, wrong state (facing etc.) is yellow.
- Material list: total, placed, remaining and in inventory on the
  dedicated screen. "In inventory" includes the contents of shulker boxes
  carried in the inventory.
- Container contents are ignored; structure void means "place nothing
  here". (Entities were first listed as ignored too; reopened the same day,
  see the entity proposal below.)

Decided 2026-10-03 after the first mockup
([demos/schematic.html](demos/schematic.html)):
- Extra blocks (a block where the schematic has air) show red like wrong
  blocks by default, because they can break redstone machines; each
  placement can switch to ignoring them (no red, not counted), for builds
  where they do not matter.
- Placement keys (move one block toward / away from the look direction,
  to the feet, rotate 90°, mirror, layer up / down, next placement), the
  area-selection flow (corner keys on the looked-at block, frame, adjust
  numbers on the screen, name and save), the Schematic entry pinned at the
  bottom of the sidebar and a "Schematic" settings category are accepted
  as in the mockup. All keys unbound by default.
- The material list lives on the dedicated screen. Its HUD list is
  default off and changes only when the player switches it (setting, key
  or the screen's switch); placing a schematic never turns it on. Large
  builds would not fit the HUD, and the screen is one key away.
- Layers work along any axis: height (from below / from above), east-west
  (from west / east) and north-south (from north / south), each with
  all / this layer only / up to this layer. Default: height from below.
- The file browser shows a rotatable 3D preview of the schematic with
  "up to layer N". It depends on the same research as the ghost look; if
  real block models cannot be drawn there, fall back to a top-down
  layer-by-layer plan like the Shapes editor.

Decided 2026-10-03 (verifier views):
- Seeing what is wrong, in four places (accepted): the target card adds "schematic:
  <expected block> (<kind>)" when the crosshair is on a mismatched block;
  an optional HUD element (default off, switched like the material list)
  shows, for the selected placement and the visible layers, correct/total,
  not placed, wrong, wrong state, extra, and the distance to the nearest
  mistake; a key marks the nearest mistake in the world with its distance
  (pressing again moves to the next); and a "Verify" tab on the screen
  lists mismatches (kind, position, expected → actual, distance; filtered
  by kind, mistakes before not-placed) next to a preview colored by
  verifier state, with "show in world". Counts follow the visible layers.
  The HUD shows the counts and the distance to the nearest mistake.
- No separate placement-screen preview showing rotation and mirror; the
  verifier-colored preview in the Verify tab is the only one besides the
  file browser's.

Decided 2026-10-03 (selection, tabs and HUD; accepted as proposed):
- One selected placement for the whole subsystem: placement keys, the
  Verify tab, the material list, the HUD and the nearest-mistake key all
  act on it. It changes only when a schematic is placed (the new placement
  becomes selected), when one is picked in the placement list, with ◀ ▶ on
  the Verify and material tabs, or with the "select the looked-at
  placement" and "next placement" keys; never by walking near another.
  Its frame is green in the world, others grey. Remembered per world and
  dimension.
- The Verify and material tabs pick the placement with one "◀ name ▶"
  stepper instead of a row of buttons, which would not scale; the
  placement list is the way to choose among many.
- No counts on the screen's tabs ("Placements 2", "Verify 8"); the list
  and details already show them. The sidebar entry keeps its count like
  Shapes and Waypoints.
- One "Schematic" HUD element (default off, switched only by the player)
  with two child switches for its sections: verifier counts and remaining
  materials (remaining / in inventory). It moves as one element in the HUD
  layout editor.
- Wrong blocks and extra blocks share the red color, so they are one group
  everywhere: the Verify tab filters are "mistakes / wrong or extra / wrong
  state / not placed" (the row's kind column still says which), and the
  HUD and the tab summary count "wrong or extra" together.
- Wherever a block or item is named (Verify rows, material list, the HUD
  material lines, the target-card line), its item icon is drawn before the
  name, using Lamium's existing item icon drawing.

Decided 2026-10-03 (entities, first version; accepted as proposed):
- Entities in a `.mcstructure` (armor stands, mobs, ...): shown as a named
  dashed frame rather than a translucent model; verified by type and
  position only (one of that type near the spot), not pose, equipment or
  name; listed in their own section of the material list ("in inventory"
  only where an item places them, e.g. armor stands; "—" for mobs); each
  placement can turn entity display and verification off (default on).
- Saving: the save prompt has "Include entities" (default off). Only what
  the client knows can be saved (type, position, rotation; equipment and
  other data depend on research).
- Later, not in the first version (research items): an option to show
  entities with their real look (not necessarily translucent), or at least
  a frame that shows their facing; verifying armor stand equipment and
  item frame contents. In Bedrock an item frame is a block with a block
  entity, not an entity, so its contents belong with the container-content
  question rather than with entities.

- Size and load (decided 2026-10-03): no hard limit. Building ghost meshes,
  verifying and counting materials run in bounded steps per frame/tick
  (DESIGN.md "Engineering behavior") and fill in progressively; counts show
  "counting" until complete. Loading a very large schematic (on the order
  of hundreds of thousands of blocks) shows a warning first. Exact numbers
  come from measurement.

Implementation (2026-10-03, maintainer's go): game-independent core first,
then rendering, screen and keys.
- Done: `.mcstructure` read/write (`src/features/schematic/Nbt.*`,
  `Structure.*`): little-endian NBT, layers as int arrays (current exports)
  or int lists (older ones), palette with state keys, block entity data by
  cell, entities relative to the corner. Placement and layer math
  (`Placement.h`: mirror then clockwise quarter turns, inverse lookup, six
  layer axes with all/only/up-to) and verifier/material rules
  (`Verify.h`). Covered in `tests/SchematicTests.cpp`; set
  `LAMIUM_SAMPLE_STRUCTURES` to a folder of real exports to parse them too.
- Block states are turned by the game:
  `VanillaBlockStateTransformUtils::transformBlock(block, Rotation, Mirror)`.
  Whether its rotation direction and mirror axes match `Placement.h`
  (clockwise from above; X flips east-west) must be checked in game with
  stairs once ghosts are drawn from a placement.
- First playable step (2026-10-03, awaiting the maintainer's check): the
  "Schematic" switch (default off, Experimental, new "Schematics" settings
  category; keys to toggle and to open the screen, unbound), a Schematics
  screen pinned in the sidebar (placements above the files of
  mods/Lamium/schematics; place a file at your feet; edit position,
  rotation, mirror, visibility, layers, extra blocks; delete), placements
  saved per world beside the waypoints (`schematics.json`), and ghosts
  drawn per 16-block section (`GhostRenderer.cpp`): tinted ghosts where a
  block is missing, red outlines for wrong or extra blocks, yellow for a
  wrong state, orange for unknown block names, block-entity models with an
  outline. Sections rebuild two per frame and refresh every two seconds.
  Not yet: Verify and material tabs, HUD, placement keys, the translucent
  option, entities, saving areas.
- Verify and materials (2026-10-03, awaiting the maintainer's check): the
  screen has four tabs (Placed, Files, Check, Materials). The world render
  scans the selected placement 16384 cells per frame and publishes a
  finished pass (`Verification.h`): counts in the shown layers, up to 2000
  mismatches (mistakes before missing blocks, nearest first) with item
  icons, and material lines by item (the block's pick item; double slabs
  count two, upper door/bed halves none). Check filters mistakes / wrong or
  extra / wrong state / not placed; "Show in world" closes the screen and
  marks the cell with a white box and a beam for 30 s. Materials show
  need / placed / left / carried (inventory plus shulker box contents;
  green when enough, yellow when short), all layers or shown layers only.
- HUD, keys and target card (2026-10-03, awaiting the maintainer's check):
  a "Schematic HUD" element (default off; switch, key and the HUD layout
  editor; child switches for check counts and materials left) shows the
  selected placement's counts in the shown layers, the distance to the
  nearest mistake and up to five materials left with icons (yellow when
  short). Keys, all unbound: nearest mistake (again: the next), select
  the looked-at placement, next placement, move one block along the view
  (steep views move up or down) or back, move to feet, turn 90° right,
  mirror, shown layer up/down; each confirms with a toast. The target card
  adds "Schematic: <block> (<kind>)" when the crosshair block is a
  mistake. The Placed tab has "Layer here: match where I stand".
- Settings regrouped after the first look (2026-10-03): the Schematics
  category has five rows: Schematics (switch, key, open screen), Schematic
  HUD (switch, key, "show check counts", "show materials left", layout
  link), and keyless groups Placement keys (select looked-at, next,
  away/closer/left/right/up/down, to feet, turn, mirror), Shown layers
  (up, down, layer here) and Check (nearest mistake, always the nearest).
  Action rows explain themselves through "help.key.<id>". The HUD is a
  fixed-width card: name and layer, a two-by-two grid of marked counts,
  nearest mistake, then up to five materials with right-aligned left/have.
  "Select the placement you look at" casts the view ray against placement
  boxes.
- Next: material names to items (wall torch -> torch, double slab -> two
  slabs, two-cell beds and doors -> one item) and the inventory count are
  game glue; then the placement session (files, saved placements, the
  selected placement), ghost meshes per section, the verifier scan within a
  frame budget, the screen, HUD and keys.

Research (2026-10-03, in progress):
- Ghost look. Candidates, compared in game with `xmake f --ghost_probe=y`
  (`src/features/schematic/GhostProbe.cpp`; F7 anchors four rows of test
  blocks three blocks ahead, F6 toggles ignoreLighting for rows B-D):
  A `BlockTessellator::renderGuiBlock` with alpha 0.5 and light 1 (the GUI
  block path); B a private `BlockTessellator` appending the block, drawn
  with the `moving_block_blend` material, the moving-block renderer's
  terrain atlas and `ActorShaderManager::setupShaderParameters` with
  ignoreLighting; C the same with in-world tessellation at the real
  position (shapes from real neighbors); D like B with the moving-block
  renderer's own blend material. A private tessellator keeps the alpha
  color override out of vanilla's block mesh caches. To check: which rows
  draw, whether they are translucent, whether brightness stays the same in
  daylight, at night and in a dark cave, stairs facing, grass tint, and
  that held/dropped blocks still look normal afterwards.
  Round 1-2 (maintainer, 2026-10-03, 1.26.51.01 with LeviSchematic, LHolo
  and ChiyanMap also installed): the in-world path drew translucent blocks
  whose brightness did not change at night, with parts missing, then
  crashed (null read inside `tessellateBlockInWorld` with a private
  tessellator); the appended mesh with `moving_block_blend` drew nothing;
  with the renderer's blend material it drew opaque and off the block grid;
  the GUI path ignored the alpha and was off the grid too. Round 3 shifts
  the appended mesh to the block corner, fills its missing light UVs and
  adds the fallback look.
  Round 3: on the block grid now, brightness unchanged at night, nothing
  missing, but opaque with both materials (the named one drew nothing);
  torch and chest give no vertices on this path; the outline was a full
  block regardless of shape; in daylight the fallback is hard to tell from
  real blocks. The log showed the appended mesh has no vertex colors and
  no light UVs at all, and the color override is ignored on this path.
  Round 4 writes vertex colors (alpha 0.5, or a light-blue tint for the
  fallback), outlines the mesh bounds, and tests stair states 0-4.
  Round 4: the named `moving_block_blend` still drew nothing. With the
  moving-block renderer's blend material the blocks were translucent and
  kept their brightness day and night, but whole blocks turned darker or
  lighter while jumping or turning (likely draw order against other
  translucent geometry such as water, since depth is written). The tinted
  fallback was stable, readable as schematic blocks in daylight, and its
  outline followed the shape. The item-shape mesh ignores block states
  (all five stairs faced the same way), so real schematics need the
  in-world mesh. Round 5 primes a private tessellator and retries the
  in-world mesh, translucent and outlined.
  Round 5: no crash after priming. `tessellateBlockInWorld` drew every
  block as a plain cube (it is the cube path; the shape dispatcher is
  `tessellateInWorld`). The in-world mesh carries light UVs and AO colors.
  Translucent blocks flickered even with the view still; with the opaque
  tint, glass flickered where it overlapped other ghost blocks. Round 6
  uses `tessellateInWorld`, scales ghosts slightly toward the eye like
  shapes, and logs how often the render pass runs.
  Round 6: stairs face the right ways and the fence takes its
  unconnected shape (neighbors come from the real world). The pass runs
  once a frame (about 300 calls in 5 s at 60 fps), yet translucent cubes
  and glass in front of planks still flickered with the view still: the
  engine evidently reorders separate draw calls between frames. Round 7
  tessellates all ghost blocks into one mesh and sorts its quads far to
  near before one draw.
  Round 7 (`3c29f5d`): no flicker in either look, moving or still; the
  2x2x2 cube and planks behind ghost glass are stable; stairs face the
  right ways. Remaining defect: real translucent blocks (glass) behind a
  ghost block disappear, in both looks. The ghost mesh writes depth in the
  entity-effects pass, which runs before the world's translucent layer.
- Ghost path found (2026-10-03): a private `BlockTessellator` (primed with
  one appended block per frame), `tessellateInWorld` per block into one
  shared `Tessellator`, vertex colors rewritten (alpha 0.5, or a light-blue
  tint), quads sorted far to near, and one draw with the moving-block
  renderer's blend material and terrain atlas after
  `ActorShaderManager::setupShaderParameters(..., ignoreLighting = true,
  ...)`. Brightness stays the same in daylight and at night. Both looks
  work: translucent, and tinted with a light-blue outline that follows the
  shape.
- Round 8 (`52cd792`, block coverage; both looks behave the same): a mesh
  for stone, glass, stairs, torch, lantern, redstone wire, poppy, lever,
  ladder (invisible from behind), rail, glass pane, slab, trapdoor, leaves
  (no biome tint: white), flower pot, campfire, bell, piston (no head) and
  end portal frame. Water drew a missing-texture block. No mesh: door,
  chest, ender chest, bed, sign, skull, shulker box (block entity
  renderers, and the door reads its other half from the world).
  `minecraft:white_banner` is not a block name. In cut-out blocks (poppy,
  redstone, campfire, glass) the empty texels hid what is drawn later
  (water), like real glass behind ghosts. Drawing in the cracks or name tag
  passes instead (each runs once a frame) changed nothing. Round 9 tries
  the alpha-test block material for the outlined look and an unlit
  blended material without depth writes for the translucent look.
- Round 9 (`7694edc`): the outlined look drawn with the moving-block
  renderer's alpha-test material is stable, and gaps in cut-out blocks
  (poppy, redstone, campfire, ghost glass) show water, ground and real
  glass behind them correctly. This is the outlined look's material. The
  unlit blended material without depth writes was very faint and varied
  with what lay behind (dense over terrain, faint over water), so the
  translucent option stays on the blend material with its limit (real
  translucent blocks behind a ghost disappear). Ladders show from both
  sides. Round 10 draws block-entity blocks through
  `BlockActorRenderDispatcher::render` with block entities created by
  `BlockActor::create` from NBT, as a `.mcstructure` stores them.
- Round 10 (`6a6212a`): `BlockActor::create` from NBT returned null for
  every block entity id (Chest, EnderChest, Bed, Sign, Skull, ShulkerBox,
  Banner), so nothing was drawn. The game's own structure block preview
  does not show chests, ender chests or shulker boxes either. Round 11
  uses `VanillaBlockActorFactory::createBlockActor(pos, blockType)`.
- Round 11 (`d7efc6b`): with `VanillaBlockActorFactory::createBlockActor`
  and `BlockActorRenderDispatcher::render` (render position relative to
  the camera), chest, ender chest, sign, banner and shulker box draw their
  real models on the block grid. Bed and skull drew nothing (they likely
  need their block entity data: bed parts and color, skull type and
  rotation). These models look like real blocks, not tinted, and their
  brightness follows the world's light; the outline still marks them.
  Loading the schematic's block entity data into them is untested.
- `.mcstructure` layout confirmed on the maintainer's export (2026-10-03,
  `mixture.mcstructure`): root `format_version` (2 here), `size`,
  `structure_world_origin`, `structure.block_indices` (layers),
  `structure.palette.default.block_palette` (name, states, version),
  `structure.palette.default.block_position_data` (index -> block_entity_data
  with `id` such as Chest, EnderChest, MobSpawner, ShulkerBox, Campfire),
  and `structure.entities` (e.g. an armor stand with `Pos`). Sample files
  the maintainer allows for testing: `mixture.mcstructure` and
  `broken_village_house.mcstructure` (in their Downloads folder; not
  committed).
- A possible path for the file browser preview: the game's
  `StructureVolumeRenderer` (the structure block's 3D view) renders a
  block volume into UI. Not tried yet.
- Default look (decided 2026-10-03): tinted with a light-blue outline.
  Translucent stays as an option: it looks right block by block, but with
  many adjacent blocks (builds) its result is hard to predict.
- Torches, chests and other blocks without a mesh on this path: explore
  more render paths before falling back to an outline only (maintainer,
  2026-10-03).
- Still open before or during implementation: real translucent blocks
  behind ghosts (draw later than the world's translucent layer, or without
  depth writes now that quads are sorted); blocks without a mesh on this
  path (torch, chest and other block entities, which need another path or
  an outline only); shapes that depend on neighbors (fences, panes, stairs
  corners, redstone) should follow the schematic's neighbors, not the real
  world's; per-section cached meshes with sorting kept within the frame
  budget instead of rebuilding everything each frame.
- Fallback accepted by the maintainer if translucency fails: opaque blocks
  drawn slightly differently (tinted) inside a light-blue outline, clearly
  readable as schematic blocks.

### L-57 Client info counters
Kind: Research. Split from L-53 on 2026-09-27 (wave 2).
- Candidate lines: loaded entity count, loaded chunk count and particle
  count. The SDK exposes `Level::getRuntimeActorList()` and
  `Level::getEntities()`, chunk tracking under `LevelChunkViewTracker`, and
  `ParticleEngine`'s per-type `particleCount`; it is not yet known what each
  returns on the client (whole level vs. focused dimension, cost per frame).
- Establish what one cheap call gives, then decide the lines and their read
  cadence (not per frame if expensive). Keep it read-only.
- Not available on the Bedrock client and must stay out: slime chunk (no
  seed), server TPS/mob caps, Java heap memory, region files, chunk
  section/update stats, the effect list and local difficulty. Record this in
  the help text where users would look for them.

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
- Not started, not yet triaged: Mass Craft. It needs a Design pass before
  it becomes a task (Schematic became L-93). (Fast Attack/Use became L-34;
  Scroll Transfer became L-41.)
