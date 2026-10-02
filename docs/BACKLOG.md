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
   - L-63 Saturation on the vanilla hunger bar (Research, then Design) and
     L-64 food values in the inventory (decided; waits for L-63's saturation
     marking).
   - L-67 Switch to the best weapon when attacking (Design first).
   - L-75 Offhand slot beside the hotbar (Design with a mockup, small).
   - L-88 Target health hearts: show absolute health with one heart per two
     HP instead of normalizing every target to the same heart count.
   - L-90 Simplified Chinese localization: add `zh_CN` as the third official
     UI locale and make the translation table ready for more locales.
2. **Placement and breaking — L-15 restrictions and L-59 held placement
   style:** specs written after the 2026-09-28 discussion; building waits for
   the maintainer's go.
3. **Map — L-60 minimap, waypoints and world map:**
   resumed 2026-10-01. Runs in parallel with the small and medium features
   in 1; neither ranks above the other. Steps 1-5 (minimap, cave view,
   radar, waypoints with their screen) are built and checked in a local
   world (servers not checked). The world map is built and checked in a
   local world (2026-10-01; servers and large worlds not checked). L-82
   ended as a link to an external seed map (done 2026-10-01); seed-based
   biomes and structures are a non-goal. Open: server and large-world
   checks, and the map UI in L-83.
4. **Research when convenient:** L-37 FreeCamera seeing caves (wanted),
   L-79 carved pumpkin and spyglass frame draw path (cheap-model friendly
   trace/test steps), L-89 distant player positions for the map/radar via
   the vanilla locator-player path, L-71 starting a glide from the mod, L-57
   client counters, L-30 Ender Dragon part hitboxes, L-33 mob growth and
   breeding timers.
5. **L-73 architecture review:** agreed 2026-09-30, in progress step by
   step (order in the L-item); step 13 goes with L-15 breaking.
6. **Before a release:** the pre-release checks below.

Ideas that are not yet chosen (for example more inventory transfer gestures,
an arrow-count HUD line, a fall-rescue elytra, Schematic and Mass Craft) stay
in the maintainer's notes and enter this file once chosen.
Schematic and Mass Craft rank below Map because existing standalone tooling
and resource packs already cover part of them.

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
Status: steps 1-8 done 2026-09-30 (c894ae8..8d9ea21; camera trace and both
probe builds compile). In-game checks 1 and 2 passed except Auto Attack/Use
(fixed in 221edcb, rechecked the same day) and an occasional Breaking Restriction
hold that stops breaking (cause unknown; carried into B and L-15). Step 9 is
next. `Zoom.cpp` is now 752 lines with 5 `#if`
(`CameraTrace.cpp`, `DetachedCameraRig.cpp`). Steps 10 (084b424, checked in game) done; 11 and 12 dropped (see D).
Next is 13 with L-15. Step 9 decided 2026-09-30:
no further split; `Zoom` was renamed `CameraSessions` (file and class)
because it holds all three camera sessions.

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

### L-64 Food values in the inventory
Kind: Ready once L-63 settles the saturation marking. Chosen by the
maintainer 2026-09-28 alongside L-63.
Status: open.
Hovering a food item in an inventory shows how much hunger and saturation it
restores, in the same place and style as the durability readout
(`DurabilityTooltip`). Values from the item's food component; foods with
effects (for example rotten flesh) show only the values, not the effects.
Decided 2026-09-28:
- Icons, not text: the hunger gain as drumstick icons (half icons for odd
  values), drawn with the game's own HUD textures the way the Target card
  draws its hearts (`textures/ui/heart*` in `InfoHud.cpp`; the hunger
  textures are the `textures/ui/hunger_*` family - confirm the names). The
  saturation gain is marked on the same icons in the style L-63 settles, so
  the inventory and the hunger bar speak one language.
- Its own row "Food values" under Inventory next to Durability, on by
  default.

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

### L-75 Offhand slot beside the hotbar
Kind: Design (small), then Ready. Chosen by the maintainer 2026-09-30.
Status: open.
Bedrock's HUD never shows what the offhand holds (no vanilla setting found by
the maintainer; searches only turn up add-ons and resource packs), so a totem,
map or shield there is invisible during play. Draw one slot for the offhand
item beside the hotbar, as Java does.
Leaning (maintainer, 2026-09-30): a slot frame next to the hotbar on the side
opposite the main hand; settle the look with a mockup in `docs/demos/` first.
To decide with the mockup: which side (fixed or following the main-hand
setting), the frame style (vanilla hotbar sprite or Lamium's own), count and
durability bar inside the slot, hidden while empty or not, and its own
switch under HUD & overlays versus a Hide Offhand sibling.
Research before building: where the hotbar is drawn and how to place beside
it with UI scale and the pocket/classic layouts; whether the game's item
renderer (as used by container previews) draws there.

### L-67 Switch to the best weapon when attacking
Kind: Design, then Research. Chosen by the maintainer 2026-09-28 from the
prior-art comparison (behavior reference: Stipuleroo's combat Auto Tool,
PROVENANCE.md group 3).
Status: open.
Tool Switch picks a hotbar tool for the block being mined. This does the same
for attacking entities: select the hotbar weapon that deals the most damage
to the target, through the same `selectSlot` path.
To decide with the maintainer: a Tool Switch option or its own switch
(either way default off); how damage is ranked (base damage, Sharpness,
Smite/Bane against their mob types) and whether a sword beats an equal axe;
whether to switch back afterwards; which targets count (hostile only, all
mobs, players).
Research after that: Lamium already hooks `GameMode::attack` /
`SurvivalMode::attack` (`CameraInteraction.cpp`). Check whether selecting a
slot there changes the weapon used for that hit or only the next one, and
how that looks on a server. When it exists, it gets the L-69 child option
(fetch the weapon from the main inventory; see BACKLOG-DONE.md).

### L-88 Target health hearts use absolute HP
Kind: Design decided, then a small UI change. Chosen by the maintainer
2026-10-02.
Status: open.
The Target card's Hearts mode currently fills a fixed number of hearts from
`health / maxHealth`, so a 20-HP and a 40-HP mob can look equally healthy.
Make the hearts encode Minecraft health units instead:
- One full heart is 2 HP. The number of available heart slots comes from the
  target's maximum health rather than a fixed normalized count.
- Current health fills those slots in the same units; odd HP uses a half
  heart. Examples: 20/20 -> 10/10 hearts, 10/20 -> 5/10, 20/40 -> 10/20,
  19/20 -> 9.5/10.
- Reuse the game's health-bar sprites already used by the Target card. The
  Bar and Number modes do not change.
- Do not silently clamp a high-health target back to a normalized 10-heart
  display. If the current card layout cannot present a large derived count
  cleanly, settle that narrow layout question before coding and keep the
  absolute-health semantics.
Tests should cover the examples above, half-heart handling and a maximum-health
value above 20. Confirm in game on ordinary 20-HP and higher-health mobs.

### L-90 Simplified Chinese localization
Kind: Design decided, then implementation. Chosen by the maintainer
2026-10-02.
Status: open.
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

---

## Research

### L-89 Distant player positions for map and radar
Kind: Research, then implementation if a typed authoritative path is viable.
Chosen by the maintainer 2026-10-02.
Status: open.
The map radar currently obtains player positions from loaded Actor instances,
so a player outside the normal entity-tracking range disappears even when
vanilla's Locator Bar still knows where that player is.
Desired behavior:
- Keep using the loaded Actor's interpolated position while it exists.
- Only for a player without a loaded Actor, supplement that position from the
  same vanilla player-location state used by the Locator Bar. Do not infer
  positions or bypass vanilla visibility/privacy decisions.
- A vanilla HIDE update removes that player's supplemental marker immediately.
  The same resolved player-position source should be usable by both the
  minimap and world map.
Research order:
1. Inspect the current 1.26.51 / LeviLamina 26.51.5 SDK for a typed Locator
   Bar/player-location cache that already owns the authoritative state.
2. If that state is not exposed, inspect the typed receive path for
   `PlayerLocationPacket` (ActorUniqueID + position/HIDE) and determine
   whether a small Lamium cache can mirror only that public packet state.
3. Establish how ActorUniqueID maps to the existing player marker/name/head
   identity without keeping stale pointers. Clear supplemental state on
   world/server/dimension generation changes and disconnect.
4. Validate Actor -> locator fallback -> Actor transitions, HIDE, reconnect,
   and at least one server before claiming multiplayer coverage.
Prefer reading vanilla-owned state over adding a packet hook. If neither path
can preserve HIDE and lifecycle semantics cleanly, leave distant players
unshown rather than broadening visibility.

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
Status: resumed 2026-10-01. Steps 1-5 built and checked in a local world on
2026-10-01 (a server is not checked yet); the world map is next and starts
with its own design discussion.
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
  Dots 8 or more blocks above or below the player are drawn fainter.
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
Open (L-83): the settings and UI around the map may be discussed again,
including the side panel overlapping the Waypoints screen.

### L-63 Saturation on the vanilla hunger bar
Kind: Research, then Design. Chosen by the maintainer 2026-09-28.
Status: open.
Show the normally hidden saturation, drawn over the vanilla hunger bar, so
the player can judge how much food reserve is left before hunger starts to
drop. Behavior reference only; see PROVENANCE.md (group 3).
Decided:
- Drawn on the vanilla hunger bar itself, not as a separate element: an
  outline or inner fill on the drumstick icons marks the saturation level
  (0-20, the same scale as hunger).
- Holding food previews what eating it would give: the hunger and saturation
  gain shows on the bar while the food is
  held. The values come from the item's food component (`getNutrition`,
  `getSaturationModifier`), capped at the maximum.
- Client values only: `Player::HUNGER()` and `Player::SATURATION()`
  attributes of the local player. If the client does not receive saturation
  (for example on some servers), draw nothing rather than a guess.
Research first:
- confirm the client receives saturation (single player and a server);
- find where and how the vanilla HUD draws the hunger bar (render entry and
  icon positions) so the overlay follows GUI scale, hides with the hunger bar
  (creative, riding) and survives non-vanilla resource/UI layouts; if a pack
  moves the bar, the overlay must move with it or stay off, never float in the
  wrong place.
Decided 2026-09-28: saturation is a gold outline on as many drumstick icons
as the saturation level covers (the icons themselves stay readable); the
held-food preview shows the gained icons translucent and still, with no
blinking.
Open (after research, with a mockup): the exact gold, the outline width, and
the default.

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
- Not started, not yet triaged: Schematic subsystem (browser, placement,
  projection, verifier, material list), Mass Craft. These
  need a Design pass before they become tasks. (Fast Attack/Use became L-34;
  Scroll Transfer became L-41.)
