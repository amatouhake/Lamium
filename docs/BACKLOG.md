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

Rules: take the lowest-numbered Ready task whose dependencies are done
(cheap models skip tasks marked strong model). Do not
start a Design/Research task as a cheap model (see AGENTS.md). When a task is
done, change its status line, update the feature doc, and note the commit.

Confirmed working in the 2026-09-23 review (no action): Freelook, Periodic
Attack, Periodic Use, settings screen, Shapes view.

---

## Open decisions

None. HUD (docs/demos/hud.html), the settings key and the shape model are
decided; see DESIGN.md.

---

## Ready

### L-01 Settings key
Status: done (9e6ee5c). Change the `settings` action's default key from F8 (0x77) to
`L` (0x4C) in `input/Binding.h`; update README, translations/help text that
mention F8, and BindingTests. Users with an override keep it; Minecraft may
keep its own saved mapping for the Lamium key, so tell the user to check
Keyboard settings after updating. Only the settings action's default changes.
F8 is missing on some keyboards and is meaningless for Lamium. Vanilla
Bedrock keyboard defaults (options.txt, 1.26.51): Q drop, 1–9 hotbar, E
inventory, F5 perspective, Space jump, Shift sneak, Ctrl sprint, WASD, Z mob
effects, T/Enter chat, / command, C copy coordinates, X copy facing
coordinates, B emote, F2 screenshot, F4 social, [ ] menu tabs, N toast.
Lamium already uses C (Zoom, clashes with copy coordinates), J, R.
Free single letters include F G H I K L M O P U V Y. Changing a default only
affects users without an override; Minecraft may keep its own saved mapping.

### L-23 Lamium owns all key bindings
Status: done (75e87d9; dispatch fix below). Verified in game 2026-09-23:
L, C, J, R in inventory/chest/ender chest, nothing fires while typing.
Follow-up: running actions inside the key event crashed Sort (Minecraft
asserts when inventories are read outside the client tick). Actions are now
queued and run through `ClientThreadExecutor`. Do this before L-22. Decided: Lamium is the only place key
bindings live; Minecraft's keyboard settings no longer list Lamium actions.

Why: actions were registered in Minecraft's keyboard settings (KeyRegistry)
and could also be overridden in Lamium. Two sources confused Reset: after the
default moved from F8 to L, Lamium's Reset still gave F8 because Minecraft kept
the old key in options.txt, and a Lamium override silently beat any change in
Minecraft's screen.

- `input/Binding.h` (pure, tested): `defaultChord(Action)` returns
  `{Key, defaultKey}` or an empty chord when `defaultKey` is 0;
  `effectiveChord(Bindings const&, Action)` returns the override when present,
  else the default.
- `input/CustomInput.cpp`: dispatch every action through its effective chord
  (today only overridden actions are dispatched here). Keep all existing
  context rules unchanged: text editing, `ui::ownsInput()`, gameplay screens,
  Sort only in containers, focus loss, cancelled events, release handling.
- `input/Actions.cpp`: remove the KeyRegistry registration and `usesNative`
  (and the `registerActions` call in `app/Runtime.cpp`). `actionBindingName`
  shows the effective chord via `bindingChordName`.
- Reset removes the override, which now means Lamium's default. Change the
  `resetBinding` text to "Reset to default" / "既定に戻す". Remove the
  `key.Lamium.*` translation entries if nothing uses them any more.
- README: replace the sentences saying Minecraft's keyboard settings provide the
  base mappings.
- No migration: Lamium has no release yet. Leftover `Lamium.*` lines in
  Minecraft's options.txt are harmless.
- Note for the hand-back: single-key defaults are now consumed by Lamium during
  gameplay (C is also Minecraft's "copy coordinates").
- In-game checks: L opens settings; C zoom, J night vision, R sort in an
  inventory; existing custom chords still work; nothing fires while typing in
  chat or a sign; Lamium no longer appears in Minecraft's keyboard settings;
  Reset in Hotkeys returns an action to its default.

### L-22 Never leave the settings action without a key
Status: done. Found while testing L-01.
- Clearing the settings action's binding saves `"settings": []`, an explicit
  unbind that overrides Minecraft's mapping. The settings screen then cannot
  be opened at all, so the binding cannot be fixed in game.
- Do not offer Clear for the settings action (Reset stays). When loading,
  ignore an explicit empty binding for `settings` (treat it as absent, i.e.
  use the default key after L-23), so existing files recover.
- Tests: BindingTests / SettingsStoreTests for both rules.

### L-24 Remove the unused action-label localization hook
Status: ready.
- `src/ui/Localization.cpp` hooks `Localization::_getSimple` only so that
  Minecraft's keyboard settings could show `key.Lamium.*` labels. After L-23
  nothing native asks for them, so the hook runs on every string lookup for
  no reason. Remove the hook and its install/uninstall; keep
  `ui::translated`, which the settings UI uses for those labels.

### L-02 Replace gameplay key hints with an "Open Hotkeys" action
Status: ready.
- Remove the gameplay key-hint overlay and the `interface.gameplayHints`
  setting (keep loading old files without error; just ignore the key).
- Add action `openhotkeys` (Press, unbound): opens the settings screen on the
  Hotkeys view. Mirror how `OpenShapes` opens the Shapes view.
- Files: `Binding.h`, `Actions.cpp`, `SettingsScreen.cpp/.h`, `Options.h`,
  `Settings.h`, `SettingsStore.cpp`, `SettingsRows.h`, `Translations.h`, README.
- Tests: SettingsStore round trip without the key; binding count.

### L-03 Toggle toast
Status: ready.
- When a Toggle action changes a feature from a hotkey, show
  `[switch] <feature name>` for 1.5 s centered above the hotbar, fading over
  the last 0.3 s. A new toast replaces the current one. Not shown for changes
  made inside the settings screen.
- Setting `interface.toggleToasts` (default on) in the General category.
- Put the timing/replace logic in a pure header (`ui/Toast.h`) with tests;
  draw with `ui::panel`, `ui::toggleSwitch`, `ui::label`.
- Files: `input/Actions.cpp` (emit), `features/information/InfoHud.cpp`
  (draw call site), new `ui/Toast.h`, settings files, translations.

### L-04 HUD elements (split into L-04a to L-04c)
Design decided: DESIGN.md "HUD" and [docs/demos/hud.html](demos/hud.html).
Replaces the separate Info HUD, automation status and restriction status
placement. Covers the review points: few info options, hard positioning,
plain look, fixed-position automation status.

#### L-04a HUD element model
Status: ready.
- Pure `ui/HudElement.h`: anchor (9 presets), pinned flag, offset, scale
  75-150 %, background (none/card), shadow. Placement math replaces
  `HudLayout::fit` (the anchor point stays put when the element grows; clamp
  to the screen). Drag resolution: nearest anchor when not pinned, offset only
  when pinned; small offsets snap to 0.
- Settings per element with load/save and defaults matching the demo.
- Tests: placement at all anchors, growth direction, clamping, drag rules.

#### L-04b Move existing HUD pieces onto elements
Status: after L-04a.
- Info lines, target info, status (automation + restriction) and the toast
  (L-03) draw through the element model. Status merges the automation and
  restriction lines into one element with colored markers.
- Info lines become an ordered list with per-line switches (order saved).

#### L-04c Layout editor **(strong model)**
Status: after L-04b.
- "Edit HUD layout" button in General opens an editor over the live HUD:
  drag elements, anchor dots, dashed anchor guide, per-element panel.
- The per-element panel is generated from the same option definitions as
  the settings rows (never duplicated); long lists scroll.

### L-05 More Info HUD lines (providers only)
Status: ready. Layout/appearance changes belong to L-04, not here.
- Add optional lines, each with a setting, English/Japanese label and an
  "unavailable" fallback: rotation (yaw/pitch, one decimal), facing with axis
  ("North (−Z)"), block position, chunk position and position inside the
  chunk, speed (blocks/s from position deltas over ≥0.5 s), time of day and
  day count, weather, moon phase.
- Check that each value is really available on the client (SDK headers). If
  one is not, leave it out and note it here; do not estimate.
- Pure formatting and speed averaging in headers with tests
  (`PlayerInfo.h` / new `InfoLines.h`).

### L-07 More target information (providers only)
Status: ready. The card design is L-08.
- Extend `collectTargetInfo` with client-available details: block: growth
  stage for crops, redstone power level, facing/half/open states in readable
  form; entity: health/max health, armor points, baby/adult, tamed/owner if
  exposed, active effects if exposed.
- Represent them as typed rows (label, value, optional progress 0–1) so L-08
  can draw bars. Keep `TargetRows` pure and tested.
- Anything not present on the client is omitted, not guessed.

### L-08 Target card **(strong model)**
Status: after L-07 and L-04b.
- Card style per the demo: icon, name, identifier line, rows, progress bars
  (growth, health). The current text-only view stays as the "Simple" style.
- Item/block icon rendering must be found in the SDK; if it is not
  practical, draw the card without the icon and report.

### L-09 Colored line batches in the world overlay
Status: ready.
- `drawLines` in `overlay/WorldOverlay.cpp` uses two fixed colors. Let callers
  pass a list of (lines, color) groups so chunk borders and hitboxes can use
  several colors in one frame. No behavior change for existing callers.

### L-10 Chunk borders like Java F3+G
Status: after L-09.
- Reference behavior (confirm against a Java screenshot from the maintainer):
  current chunk walls have yellow lines every 2 blocks, vertical and
  horizontal; section boundaries every 16 blocks and the current chunk's
  corners are blue; the corners of the 8 neighboring chunks are red vertical
  lines.
- Extend `overlay/ChunkBorders.h` to return colored groups; keep the cache and
  the bounds checks; add tests for line counts and positions.
- When a detached camera (Freelook/FreeCamera) is active, center on the view
  chunk instead of the player chunk (requested 2026-09-24).

### L-11 Hitboxes like Java F3+B
Status: after L-09.
- Keep the white bounding box; add a red rectangle at eye height and a blue
  line from the eyes along the view direction (2 blocks long).
- Find the eye position and view vector in the SDK (`Actor`,
  `ActorHeadRotationComponent`, `getViewVector`-like functions). If not found,
  stop and hand back.
- Pure geometry for the eye rectangle and look line in `overlay/Hitboxes.h`
  with tests.

### L-13 More shape types
Status: ready. The model below is decided. The implementing agent picks the
first presets; suggested: box, cone/frustum, pyramid, ellipsoid, dome.
Keep each preset a thin entry over the shared families.

Prior art (behavior only, never code):
- MiniHUD (current fork) ships: box, centered box, circle, square, rhombus,
  block line, blocky sphere, spawn/despawn spheres (several variants),
  ellipsoid spawn, cone, pyramid, diamond pyramid, octagon pyramid. Cones and
  pyramids share one "tapered" model: bottom radius, top radius, height,
  direction. Circles have a main axis. Rendering can pick full block / inner
  edge / outer edge for which cells count as the boundary.
- Building generators and WorldEdit-style tools commonly offer sphere and
  ellipsoid (three radii), cylinder (two radii), pyramid, hollow variants.

Model (decided): a shape is a cross-section × a profile × an axis.
- Cross-section: circle, square, diamond (rhombus), octagon.
- Profile: constant (circle/cylinder, square/box), linear taper with
  bottom and top size (cone, frustum, pyramid), round (sphere, ellipsoid,
  dome = half).
- Axis: Y (default), X, Z.
The type list still shows familiar names (Cone, Pyramid, Box, Ellipsoid,
Dome...) as presets over these families, so users find "cone" by name while
the generator stays small. Alternatives: a dedicated type per shape (MiniHUD
style), or not supporting cones at all.

Range presets (numbers from minecraft.wiki, Bedrock):
- Mob spawning: 24–44 blocks spherical at simulation distance 4; 24–128 at 6+,
  limited horizontally by simulation distance.
- Beacon: box, 20/30/40/50 blocks by level toward south/east and one more
  toward north/west (asymmetric in Bedrock), full height.
- Conduit: sphere, 32/48/64/80/96 by frame size (16/21/28/35/42 blocks);
  attacks hostile mobs within 8.
- Despawn distances: not yet confirmed from a reliable Bedrock source.

Implementation notes for later: add types through the registry in
`ui/ShapeEditor.h`; generate by columns like `roundColumns` (never the full
volume); extend `ShapeDocument` load/save; test against a brute-force volume
surface for small sizes.

---

## Design

### L-15 Breaking/placement restriction redesign
Review points: anchoring UX, height-band clearing, shape-linked limits,
overlay z-fighting and clash with the vanilla selection outline.
Proposed direction to confirm:
- The anchor is the first block you start breaking; the restriction lasts
  while the button stays held and ends on release. The capture/reset keys go
  away. The mode is still picked with one key.
- New modes: height band (blocks from the feet level up to N−1 above, for
  clearing 2-high tunnels/fields), inside a shape, on a shape's surface
  (linking to Shapes).
- The overlay uses the shape face renderer (faint faces) and skips the
  targeted block so the vanilla outline stays visible.
Placement restriction additionally needs research into vanilla placement
position rules before it can be built (see RESTRICTIONS.md).

### L-16 Light overlay redesign
Review points: numbers hard to read and flat on the ground, small range, poor
readability at angles, no spawn marking.
To decide: camera-facing numbers vs colored markers only; range and update
strategy (per chunk cache); marking of spawnable blocks. minecraft.wiki
(Bedrock): most Overworld monsters cannot spawn where sky light is 7 or more
or block light is above 0. Confirm in game before relying on it.

---

## Research

### L-14 Hidden offhand still shows a shield
With Hide Offhand on, totems disappear but a shield is drawn slightly lower.
The shield likely goes through a path other than
`ItemInHandRenderer::renderOffhandItem` (blocking pose or a shield-specific
renderer). Needs a trace build to find which call draws it.

### L-17 Hand Restock does not replenish
Consumption is detected, but the transfer through the HUD fails
(`handlePlaceAmount` returns false). See HAND-RESTOCK.md and VALIDATION.md.

### L-18 FreeCamera
Status: research, assigned as an experiment (2026-09-23). Work on branch
`freecamera`, not `main`, until the maintainer accepts it.

Goal: a Toggle action detaches the camera from the player. The camera turns
like Freelook and flies like creative flight (WASD horizontal relative to the
camera yaw, Space up, Shift down); the player stays where it is and receives
no movement, attack or use. Toggling off returns the view to the player.

Read first: docs/CAMERA.md (all of it), `features/camera/Zoom.cpp` (Freelook),
`DetachedCameraMotion.h`, `DetachedLookState.h`, `CameraMovementInput.*`.

Known facts (verified in game):
- Overriding the view matrix after `setupCamera` changed culling but not the
  rendered rotation; overriding `tryGetActorRotation` had no visible effect.
- Freelook works by removing `VanillaCamera::UpdatePlayerFromCameraComponent`
  from the active camera entity and restoring the camera's own angles later.
  Reuse this for FreeCamera's rotation.
- The `camera_position_probe` build (translation composed after
  `setupCamera`) visibly moved the camera two blocks to the right. Whether
  culling, chunks and overlays agree with a moved camera is unknown.
- `camera::consumeMovement` can clear `RawMoveInputComponent` movement, but no
  hook calls it yet.

Stages. Stop after each stage, deploy, and ask the maintainer to check in
game before the next one:
1. Action + rotation: append a `freecamera` Toggle action (unbound) and an
   experimental feature row; while active, detach rotation exactly like
   Freelook (share its session; Freelook and FreeCamera never run together).
2. Freeze the player: while active, the player does not move, jump, sneak,
   attack or use. Find where vanilla consumes `RawMoveInputComponent` and
   call `consumeMovement` there; keep the extracted input for stage 3.
3. Move the camera: feed the extracted input to `DetachedCameraMotion` and
   apply the position. Try the camera entity's own position (ECS components
   in `MinecraftCamera`/`VanillaCamera`) before a view-matrix translation.
   Report what happens to culling, distant chunks, shapes and chunk borders.
4. Exits: toggling off, settings opening, world exit, dimension change,
   death, focus loss and disable all return to vanilla cleanly.

Hand back instead of pushing on when a stage fails in game twice with the
same approach, or when a needed game function cannot be found in the SDK.
Use trace options (`xmake f --camera_trace=y`) for evidence, never in a build
handed over as final.

Stage 3 status 2026-09-24: the post-setup view override failed twice in game
(924dd12 view only; 8c81b36 view plus render eye and dependencies), in first
and third person. Trace proves the transform is applied with growing
displacement, yet no visible motion; terrain vanishes instead. Third attempt
(af27536): drive the detached camera entity's offset component, stash WASD
from direction flags. Verified 2026-09-24: first-person flight correct
(WASD/Space/Shift, slow), terrain follows, no residue on exit, menus exit.
Remaining: third-person flight (rotation only), speed, menu behavior.

### L-20 Shape name text input adds stray characters
Native text entry for shape names inserts extra characters.

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
- L-26 FreeCamera flight speed (parked, after L-18). Verified slow but correct
  at the internal 10 blocks/s. Add a user-facing speed setting with sane
  bounds; decide on a fast-flight modifier, if any, at design time.
- L-28 Third-person underground camera (parked, after L-18). While detached
  underground, vanilla collision avoidance fights the pivot offset and the
  view judders block by block. Decide whether to soften avoidance while
  detached or document it as a limit; above-ground flight is unaffected.
- L-27 Detached menu behavior (parked, after L-18; small Design open).
  Inventory/settings opening currently exits FreeCamera and discards the flown
  position, which is safe but annoying. Desired: an option around inventory
  rendering while detached (Java mods have similar), so looking-only flight
  need not pass through inventory. Exits for death/dimension/world change
  stay mandatory regardless of the option.
- L-21 Shape color picker or more colors: only if the four colors prove
  insufficient.
- Not started, not yet triaged: F3-style debug view, Scroll Transfer
  (wheel transfers between inventories), Schematic subsystem (browser,
  placement, projection, verifier, material list), Mass Craft, Fast
  Attack/Use. These need a Design pass before they become tasks.
