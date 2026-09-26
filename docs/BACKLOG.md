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

**Bugs** (something that ships behaves wrongly) are listed first in their own
section and are fixed before new features. Each bug still has a kind that
decides who picks it up.

## Current execution order

Keep this section short. It is only the ordering layer; task details and status
live in the L-items below. If this summary ever disagrees with an L-item, the
L-item wins.

1. **Fix bugs:** L-27 (Design) and the bug research L-37, L-14 and L-17.
2. **Camera requests from users:** L-45 (Ready, awaiting check), L-39 (Design).
3. **High-priority new work:** L-40 Fake Sneak (Research).
4. **Restriction redesign:** L-15 (Design; its resume bug is L-36).
5. **Next features:** L-46 (Ready, awaiting check), L-41 and L-42 (Design).
6. **Run bounded native research in parallel:** L-30 and L-33.
7. **Prepare the first release:** keep user-facing docs current, run a full
   runtime regression on the release build, verify a fresh install/package and
   finish the remaining distribution review. 0.1.1 is the current GitHub
   pre-release (tag v0.1.1; same code as the build verified for L-16) with the
   known issues listed in the README; the version is set in `xmake.lua` and
   `tooth.json`. lip registration is not done yet.

HUD/world presentation and the strong-model HUD/Target polish (L-04a/b/c,
L-05, L-07, L-08, L-09/L-10/L-11 and L-13) are complete and no longer belong
in the active ordering. Large new subsystems in Later / parked do not start
before the first Lamium release unless the maintainer explicitly changes this
plan. Experimental research does not block the release unless the feature is
advertised as finished or uncovers a correctness/safety problem.

Task-picking rule: work in the earliest active group above. Cheap models skip
strong-model, Design and Research work. Within a group, follow dependencies and
the L-item's model/validation requirements. When a task is done, update its
status and relevant feature doc; do not duplicate task details into this
summary.


---

## Open decisions

- L-27: keep the FreeCamera position through menus and focus loss always, or
  as an option?
- L-39: should Freelook starting in third person be an option, and what is the
  default?

HUD (docs/demos/hud.html), the settings key and the shape model are decided;
see DESIGN.md.

---

## Bugs

### L-35 Container previews play the item pickup animation
Kind: Ready. Reported by a user 2026-09-26.
Status: done (verified in game 2026-09-26, DLL 766d6fd6).
Items shown in Shulker/Bundle previews play the vertical stretch that vanilla
uses right after picking an item up. The preview draws stacks decoded from the
container, which keep `ItemStackBase::mShowPickUp`; the Info HUD already clears
it on its icon copies (`InfoHud.cpp`). Draw preview icons from copies with
`mShowPickUp = false` (never touch the real inventory stack). Check in game with
a Shulker Box of blocks right after picking it up, and with items that animate
on their own (clock, compass) to make sure those still animate.

### L-36 Breaking does not resume after a forbidden block
Kind: Research. Split from L-15 on 2026-09-26.
Status: done (verified in game 2026-09-26, DLL aadaa782). The trace
(2026-09-26) showed that when `continueDestroyBlock` returns false for a block
outside the region, vanilla calls `stopDestroyBlock` on the previous block and
never calls `continueDestroyBlock` again while the button stays held. The
hook now skips such a block but returns true (no progress, `destroyed` false)
so the session survives; a menu or settings screen still ends it.
Check 2026-09-26 (DLL d421e275): resuming works, but while an allowed block
was cracking, resting the crosshair on a forbidden block kept cracking the
allowed one. Since 8f2d38a the first forbidden target aborts the allowed
block's progress through vanilla `stopDestroyBlock`.
Check (DLL a6fdad6e): cracking stopped, but afterwards allowed blocks showed the
crack animation without breaking; the trace showed client `destroyBlock` calls
returning true that the server never applied, because the aborted session was
continued without a new start action. Since b5094f7 the first allowed target
after such an abort calls `startDestroyBlock` like a fresh click; verified.
With Breaking Restriction on, once the crosshair passes over a forbidden block,
breaking does not resume on an allowed block until the mouse button is released
and pressed again. The forbidden block must still not break, but the held
button must keep working: fix the input/session handoff, not the region
predicate. Find which vanilla breaking-session state is left stopped after the
rejected block and how a held attack normally restarts it. Listed in the
README's known issues. Reproduced again 2026-09-26 (allowed -> forbidden ->
allowed: the second allowed block does not start); the first trace run lost
its log before this step, so the call sequence is still to be recorded.

### L-37 FreeCamera cannot see caves from underground
Kind: Research. Reported by a user 2026-09-26.
Flying FreeCamera into the ground does not show caves the way spectator mode
does: chunk sections are missing or culled, so underground spaces cannot be
looked at cleanly. Likely the render-chunk visibility/occlusion pass is seeded
from the player rather than the detached camera, or spectator gets special
handling. Find which pass decides visible sections and what spectator changes,
then decide whether FreeCamera can use the same path without affecting the
player's own rendering. Separate from L-28 (third-person collision judder).
2026-09-26 observation: from inside solid ground FreeCamera stops drawing
distant caves along straight chunk lines, while spectator at the same spot
shows them; from inside a cave FreeCamera looks normal. This fits an
occlusion flood fill seeded from an opaque section. Survival samples show
culler type 3; the spectator/FreeCamera samples were lost with the log.
Second trace (2026-09-26): survival and FreeCamera use culler type 3 (the
renderer camera position does follow FreeCamera underground), spectator
switches to culler type 5. Next: find where the renderer picks the culler
type (spectator, no-clip or camera-in-block check) and whether FreeCamera
can select type 5 without making the player a spectator for game logic.

### L-14 Hidden offhand still shows a shield
Kind: Research.
With Hide Offhand on, totems disappear but a shield is drawn slightly lower.
The shield likely goes through a path other than
`ItemInHandRenderer::renderOffhandItem` (blocking pose or a shield-specific
renderer). Needs a trace build to find which call draws it.


### L-17 Hand Restock does not replenish
Kind: Research.
Consumption is detected, but the transfer through the HUD fails
(`handlePlaceAmount` returns false). See HAND-RESTOCK.md and VALIDATION.md.
Desired scope also includes **offhand auto-restock when a safe vanilla-backed
path exists**, especially replacing a consumed Totem of Undying from inventory.
Treat offhand consumption/slot mapping as a separate runtime path: do not assume
the main-hand use observer or HUD indices apply, and do not synthesize stacks or
forge inventory packets. Main-hand success is not required to prove feasibility,
but each path needs independent runtime validation.


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
Status: done.
- `src/ui/Localization.cpp` hooks `Localization::_getSimple` only so that
  Minecraft's keyboard settings could show `key.Lamium.*` labels. After L-23
  nothing native asks for them, so the hook runs on every string lookup for
  no reason. Remove the hook and its install/uninstall; keep
  `ui::translated`, which the settings UI uses for those labels.

### L-02 Replace gameplay key hints with an "Open Hotkeys" action
Status: done. Follow-up 2026-09-24: `openshapes` also belongs to the settings
feature, so all three screen openers group under 全般 in Hotkeys.
- Remove the gameplay key-hint overlay and the `interface.gameplayHints`
  setting (keep loading old files without error; just ignore the key).
- Add action `openhotkeys` (Press, unbound): opens the settings screen on the
  Hotkeys view. Mirror how `OpenShapes` opens the Shapes view.
- Files: `Binding.h`, `Actions.cpp`, `SettingsScreen.cpp/.h`, `Options.h`,
  `Settings.h`, `SettingsStore.cpp`, `SettingsRows.h`, `Translations.h`, README.
- Tests: SettingsStore round trip without the key; binding count.

### L-03 Toggle toast
Status: done. Follow-up 2026-09-24: no `[switch]` marker; card background by
default (the panel fades, the text dims); the last 0.3 s dims the text instead
of fading; the toast fires only after the new state is saved. Open: the
toggle switch stays fixed-size when the toast element scales; decide after
seeing it in game.
- When a Toggle action changes a feature from a hotkey, show the feature
  name with its toggle switch for 1.5 s centered above the hotbar, dimming
  over the last 0.3 s. A new toast replaces the current one. Not shown for
  changes made inside the settings screen, and not shown when the save
  fails.
- Setting `interface.toggleToasts` (default on) under the Settings screen
  feature.
- Put the timing/replace logic in a pure header (`ui/Toast.h`) with tests;
  draw with `ui::toggleSwitch` and `ui::label`.
- Files: `input/Actions.cpp` (emit), `features/information/InfoHud.cpp`
  (draw call site), new `ui/Toast.h`, settings files, translations.

### L-04 HUD elements (split into L-04a to L-04c)
Design decided: DESIGN.md "HUD" and [docs/demos/hud.html](demos/hud.html).
Replaces the separate Info HUD, automation status and restriction status
placement. Covers the review points: few info options, hard positioning,
plain look, fixed-position automation status.

#### L-04a HUD element model
Status: done.
- Pure `ui/HudElement.h`: anchor (9 presets), pinned flag, offset, scale
  75-150 %, background (none/card), shadow. Placement math replaces
  `HudLayout::fit` (the anchor point stays put when the element grows; clamp
  to the screen). Drag resolution: nearest anchor when not pinned, offset only
  when pinned; small offsets snap to 0.
- Settings per element with load/save and defaults matching the demo.
- Tests: placement at all anchors, growth direction, clamping, drag rules.

#### L-04b Move existing HUD pieces onto elements
Status: done. Notes 2026-09-24: info lines reorder with Left/Right on the
row (Enter/click still toggles; drags wait for the L-04c editor).
2026-09-24: HUD fills looked opaque because the gameplay screen renders four
views per frame and the HUD was drawn on each; it now draws only on the
`hud_screen` view, fills are translucent, and Target defaults to the card.
- Info lines, target info, status (automation + restriction) and the toast
  (L-03) draw through the element model. Status merges the automation and
  restriction lines into one element with colored markers.
- Info lines become an ordered list with per-line switches (order saved).

#### L-04c Layout editor **(strong model)**
Status: done (verified in game 2026-09-25, merged at 1c6ff4e). Decisions in
DESIGN "HUD" and demos/hud-editor.html:
- Placement: no pin; drop position decides the anchor (screen thirds);
  flush edges allowed; snap at the edge, at a 4-unit inset and to center
  lines; defaults and snap-to buttons use the inset.
- Element toolbar instead of the side panel; it stays put on look changes.
- Settings list: replace the per-element placement/look rows with one
  "Placement and look" link row per HUD feature; add an unbound
  "Open HUD layout" action under General.
- Card look A (Bedrock popup, stepped corners).

### L-05 More Info HUD lines (providers only)
Status: done. Notes 2026-09-24: thunder is not separately exposed (the weather
line shows Clear/Rain via `Weather::isRainingAt`); day count, clock epoch and
the 8-day moon cycle derive from `Level::getTime()` total ticks and need one
in-game check against /time query and the visible moon.
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
Status: done. Notes 2026-09-24: active effects have no list API (only single
`getEffect`), so they are omitted; thunder is not part of this task; armor
rows show only above zero; `facing_direction` 0-5 maps down/up/north/south/
west/east and crop maxima cover common crops only — confirm in game.
- Extend `collectTargetInfo` with client-available details: block: growth
  stage for crops, redstone power level, facing/half/open states in readable
  form; entity: health/max health, armor points, baby/adult, tamed/owner if
  exposed, active effects if exposed.
- Represent them as typed rows (label, value, optional progress 0–1) so L-08
  can draw bars. Keep `TargetRows` pure and tested.
- Anything not present on the client is omitted, not guessed.

### L-08 Target card **(strong model)**
Status: done (verified in game 2026-09-25, merged at 1c6ff4e). Icons use the
pick-block item for blocks and the spawn egg for mobs; hearts use the
game's sprites; the card follows the camera during Freelook/FreeCamera.
- Icon (blocks/items through the item renderer already used by container
  previews; mobs use their spawn egg), name, identifier line, then rows with
  labels in a faint column and values aligned.
- Independent settings rows: icon, ID, health (hearts / bar / number),
  growth (bar / number), other details. No card/simple style switch.
- Ease the card's size and position over about 0.16 s when the target
  changes; fade the content in.
- If icon rendering in the HUD pass is not practical, draw the card without
  the icon and report.

### L-09 Colored line batches in the world overlay
Status: done.
- `drawLines` in `overlay/WorldOverlay.cpp` uses two fixed colors. Let callers
  pass a list of (lines, color) groups so chunk borders and hitboxes can use
  several colors in one frame. No behavior change for existing callers.

### L-10 Chunk borders like Java F3+G
Status: done. Colors confirmed against a Java screenshot 2026-09-24: purple
current corners, yellow 2-block grid alternating with dark-cyan verticals,
dark-cyan middle horizontals, blue 16-block sections, red neighbor corners.
Exact shades still need a side-by-side comparison. Reconfirmed target after the
2026-09-24 in-game trial: Java F3+G-like information/visual structure, not
merely a 16x16 chunk outline.
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
Status: done. The red eye box is a fixed-size marker pending a screenshot
comparison; the dragon stays out (L-30). Eye box and look line draw for mobs
only, matching Java (no red frames on items).
- Keep the white bounding box; add a red rectangle at eye height and a blue
  line from the eyes along the view direction (2 blocks long).
- The Ender Dragon is a special case and must not be approximated from model
  geometry. Bedrock multipart/damage-box exposure is tracked separately in
  L-30; ordinary L-11 work must not wait on it.
- Find the eye position and view vector in the SDK (`Actor`,
  `ActorHeadRotationComponent`, `getViewVector`-like functions). If not found,
  stop and hand back.
- Pure geometry for the eye rectangle and look line in `overlay/Hitboxes.h`
  with tests.

### L-13 More shape types
Status: done. First presets: box, cone, frustum, pyramid, ellipsoid, dome
(plus axis X/Z for every round shape). Diamond/octagon sections arrive with
their own presets; no octagon taper.

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

### L-38 Zoom up to 50x with a proportional, smooth wheel
Kind: Ready. Requested by a user 2026-09-26. DESIGN "Camera".
Status: done (verified in game 2026-09-26, DLL 766d6fd6). The Wheel step
setting was removed; old settings files still load. Follow-up: L-45.
- Raise the magnification range from 1x-10x to 1x-50x for both the initial
  setting (Options.h, Settings normalize) and the wheel (`ZoomState`).
- A wheel notch multiplies/divides the target magnification by about 1.15
  (replace the fixed additive step; migrate or drop the `wheelStep` setting
  and keep old settings files loading).
- The shown magnification eases toward the target (frame-rate independent,
  roughly 0.1-0.15 s); FOV and turn sensitivity use the shown value. Releasing
  Zoom resets as today.
- Keep the math in `ZoomState.h` with tests: range clamps, notch symmetry,
  easing convergence, no overshoot, reset.
- In game: 50x is reachable in a reasonable number of notches, zooming feels
  smooth, aiming stays controllable at 50x.

### L-43 Permanent Sprint
Kind: Ready. Notion idea (Masa-style QoL), promoted 2026-09-26.
Status: done (verified in game 2026-09-26, DLL 766d6fd6). The maintainer did not
want menus to cancel it: since 73d8afe it pauses while a menu is open and
only death, dimension change, world exit or disabling Lamium end it
(re-checked 2026-09-26: resumes after closing the inventory). Shares
Permanent Sneak's raw-input hook; unbound by default.
Mirror Permanent Sneak: add `SprintDown` to a transient copy of the raw move
input in the same `extractRawHIDInput` hook, with the same eligibility and
cancellation (screens, settings, death, sleeping, riding, dimension change).
Vanilla still decides whether sprinting is possible (hunger, blindness,
moving forward, sneaking). Add the setting row, action and translations.
Check in game that sprint starts when walking forward, stops when vanilla
would, and that turning it off never leaves sprint stuck.

---

## Design

### L-27 FreeCamera keeps its position through menus
Kind: Design (small). Promoted from Later 2026-09-26 after user feedback.
Opening the inventory or another menu, the pause screen, or switching windows
currently ends FreeCamera and discards the flown position. Proposed direction
(DESIGN "Camera"): keep the detached pose through these and resume on return;
death, dimension change and leaving the world still end it. Open: always, or
an option. The pose must not follow input while a screen owns input, and
inventory interaction while detached stays a separate question (L-25).

### L-45 Zoom level feedback
Kind: Ready (decided 2026-09-26, no mockup). Maintainer feedback on L-38.
Status: check 2026-09-26 (DLL d421e275) passed for the wheel floor, the readout
and its setting. Feedback: the readout was too prominent and too close to the
crosshair, and the HUD layout could not move it. Since cd3850a it is its own
HUD element (75% scale, dimmed, 36 below center) with placement and look in
the layout editor (awaiting re-check).
With the wheel able to go down to 1x, Zoom can be held with no visible effect,
so it is unclear whether it is on. Also wanted: an option to show the current
magnification. Open: raise the wheel's lower bound (for example 1.5x or 2x)
or keep 1x; where and how the magnification is shown (next to the crosshair,
as a toast, or an Info HUD line) and its default.

### L-46 Reset settings to defaults
Kind: Ready (decided 2026-09-26). Status: implemented, awaiting the batched
in-game check. General shows "Reset all" and Hotkeys shows "Reset all keys" on
the column-heading line; the first press arms the button (red, "Press again"),
the second applies, any other click disarms it. "Reset all" restores every
setting including key bindings and the HUD layout; Shapes are untouched. The
HUD layout editor keeps its own per-element and all-element resets.
Raised by the maintainer 2026-09-26. The demo
(docs/demos/settings-reset.html) showed per-row and per-feature resets; the
maintainer judged them excessive (vanilla Minecraft has no per-setting reset)
and narrowed the scope to: reset everything, and reset key bindings only
(HUD elements already have Reset in the layout editor). A per-category reset
is optional and not planned now. Open: whether "reset everything" includes key
bindings and the HUD layout (proposed: yes, Shapes excluded), and where the two
actions live (proposed: General, and the Hotkeys view header, each with an
in-screen confirmation).
The text below is the original question.
Only key bindings (Reset per action) and HUD elements (Reset in the layout
editor) can go back to their defaults; ordinary settings cannot, short of
deleting the settings file. To decide: per-row reset (for example a small
reset control shown only when a row differs from its default), per-feature
reset, a "reset all settings" action with confirmation, and whether the last
two include key bindings, HUD layout and Shapes (Shapes are per-world data and
should probably be excluded).

### L-39 Freelook starts in third person
Kind: Design (small). Requested by a user 2026-09-26.
Proposed: Freelook switches to the rear third-person view while active and
returns to the previous perspective on release, reusing FreeCamera's
perspective save/restore. Open: option or not, the default, and whether the
front view is offered. Starting from third person keeps that view.

### L-41 Inventory drag and wheel transfer
Kind: Design. Notion idea (Item Scroller style), promoted 2026-09-26.
Holding a click (or Shift+click) and sweeping over slots moves each passed
stack to the other side; later, wheel moves one item or one stack. Reuse the
sort infrastructure (screen tracking, hovered slot, vanilla container-controller
transfers with response tracking); never write stacks directly. First target:
player inventory and ordinary storage (chest, barrel, Shulker Box). To decide:
the default gestures (Shift+LMB / LMB / RMB / wheel), how fast a sweep may queue
transfers, and cancel rules (cursor item, text input, screen change, other
players). Research the 26.51.5 quick-move / auto-place API before building.

### L-42 Hide visual effects without changing game state
Kind: Design. Notion ideas, promoted 2026-09-26.
One group of render-only toggles: boss bars, rain/snow, all particles,
carved-pumpkin overlay, spyglass overlay (zoom kept) and the nausea green
vignette (vanilla Screen Distortion already removes the warp). Weather,
effects, boss state and equipment are never changed. To decide: where the rows
live and whether particles offer only All/None at first. Each item needs a small
trace to find its render entry; ship them one by one. Status-effect-only
particle filtering stays an idea until its source can be identified.

### L-15 Breaking/placement restriction redesign
Review points: anchoring UX, height-band clearing, shape-linked limits,
overlay z-fighting and clash with the vanilla selection outline.
Proposed direction to confirm:
- The anchor is the first block you start breaking; the restriction lasts
  while the button stays held and ends on release. The capture/reset keys go
  away. The mode is still picked with one key.
- Rejected blocks must not terminate the user's physical left-click hold.
  If the crosshair passes over a forbidden block and later reaches an allowed
  block while the button is still held, breaking should resume without a
  release/re-press. This bug is tracked separately as L-36 and does not wait
  for the redesign.
- New modes: height band (blocks from the feet level up to N−1 above, for
  clearing 2-high tunnels/fields), inside a shape, on a shape's surface
  (linking to Shapes).
- The overlay uses the shape face renderer (faint faces) and skips the
  targeted block so the vanilla outline stays visible.
Placement restriction additionally needs research into vanilla placement
position rules before it can be built (see RESTRICTIONS.md).

### L-16 Light overlay redesign
Status: done (verified in game 2026-09-26, DLL 9e7fd594). DESIGN "Light
overlay", docs/demos/light-overlay.html, LIGHT-OVERLAY.md. Follow-ups after
the first check: range up to 64 with per-chunk reading/meshes, FreeCamera
center and direction, fixed-direction option, and three flicker causes.
Review points: numbers hard to read and flat on the ground, small range, poor
readability at angles, no spawn marking.
To decide: camera-facing numbers vs colored markers only; range and update
strategy (per chunk cache); marking of spawnable blocks. minecraft.wiki
(Bedrock): most Overworld monsters cannot spawn where sky light is 7 or more
or block light is above 0. Confirm in game before relying on it.

---

### L-32 Hotkey overlap and chord semantics **(strong model)**
Status: done (verified in game 2026-09-25, last build 8cbf8977). `ChordDispatch` in
`input/Binding.h` (event-sequence tests in BindingTests); choices made while
building are recorded in DESIGN "Keys". Design agreed 2026-09-24.
2026-09-25 runtime check: checklist 1-10 passed (build 188a1c3a). Follow-up
decided the same day and implemented: a key that begins a longer chord fires
on release (Java F3 behavior, DESIGN "Keys"); verified in game 2026-09-25.
Then the conflict display moved from the footer to warning key caps and a
key-cell tooltip (DESIGN "Keys", docs/demos/hotkey-conflicts.html); verified
in game 2026-09-25.
2026-09-24 playtest finding: overlapping bindings do not behave like the
maintainer expects from Java / Tweakeroo / MaLiLib. Concrete required case:
if one action is bound to `B` and another to `F3 + B`, pressing **F3 then B**
must trigger the `F3 + B` action and must **not** also trigger the `B`
action.

Current Lamium behavior explains the mismatch: `BindingState::update` treats a
chord as matched when every token in that chord is present in the held-input
set. It does not reject extra held inputs, so `{B}` remains a match while
`{F3,B}` is held. `canonicalChord` also sorts tokens, making normal chords
order-insensitive, and `CustomInput::process` evaluates every action
independently, so overlapping matches can both fire.

Implement a small Lamium model inspired by MaLiLib/Tweakeroo behavior, without
exposing their full advanced keybind settings:
- Separate **ordinary action chords** from **modifier-like chords** internally;
  this matching mode is part of the action definition, not a user-facing
  advanced setting.
- Ordinary chords are order-sensitive and do not activate a shorter subset when
  a more-specific chord is completed. Example: with `B` and `F3+B`, F3 then B
  fires only `F3+B`. B then F3 may already have fired B; do not delay a simple
  action waiting to see whether another key arrives later.
- Modifier-like actions (Zoom, Freelook and future actions explicitly classified
  that way) allow unrelated held inputs and are not broken by normal movement or
  gameplay keys. Their purpose is to remain usable while moving/acting.
- If a more-specific ordinary chord becomes active, suppress only the overlapping
  shorter match for that activation. Do not invent delayed dispatch or retroactive
  cancellation of an action that already fired earlier in the input sequence.
- **Identical chords are allowed intentionally.** The Hotkeys UI marks them as a
  conflict/shared binding, but all enabled actions with that exact chord fire
  together. This supports deliberate grouped toggles. Do not resolve identical
  bindings with hidden priority and do not disable either action.
- Press/Hold/Toggle semantics remain properties of the action. Matching mode is
  orthogonal to behavior.
- Mouse + keyboard and wheel + modifier bindings follow the same overlap rules.
  Wheel remains an impulse and cannot back a Hold action.
- Preserve current text-entry, UI ownership, focus-loss, cancelled-event and
  client-thread dispatch safety rules.
- F3-style Lamium chords must coexist with vanilla deliberately: when Lamium
  successfully handles the completed chord, consume the relevant completion
  event so the shorter Lamium binding does not fire; preserve the existing
  vanilla/debug-key suppression behavior needed to avoid accidental F3 actions.

Hotkeys UI:
- warn on exact duplicate bindings and on overlapping subset/superset bindings;
- exact duplicates are informational/actionable warnings, not invalid state;
- show which actions share or overlap a binding so the user can intentionally
  keep or change them.

Tests must cover event sequences, not only held snapshots:
- `B` vs `F3+B`: F3 -> B, B -> F3, releases and repeats;
- exact duplicate bindings: both actions fire once from the same completion;
- three-level overlap such as `B`, `Shift+B`, `Ctrl+Shift+B`;
- modifier-like action while WASD/Space/Shift are also held;
- ordinary Hold transition when a more-specific chord appears/disappears;
- mouse + keyboard and modified wheel cases;
- focus loss, text input, settings ownership and cancelled events.

Behavior reference only; do not copy implementation:
https://github.com/maruohon/malilib/blob/ornithe/1.12.2/src/main/java/malilib/input/KeyBindImpl.java
https://github.com/maruohon/malilib/blob/ornithe/1.12.2/src/main/java/malilib/input/KeyBindSettings.java
https://github.com/maruohon/malilib/blob/ornithe/1.12.2/src/main/java/malilib/input/HotkeyManagerImpl.java
https://github.com/maruohon/tweakeroo/blob/ornithe/1.12.2/src/main/java/tweakeroo/config/Hotkeys.java

---

## Research

Diagnostics: `xmake f ... --research_trace=y` logs lines prefixed
"research L-3x/L-4x" for L-36, L-37, L-40 and L-44 (see `src/features/research/`
and the L-36 block in `BreakingRestriction.cpp`).

### L-40 Fake Sneak (edge protection without sneaking) — high priority
Kind: Research. Notion idea, promoted as high priority 2026-09-26.
Keep the player from walking off block edges like sneaking does, without
actually sneaking: no speed loss, no sneak pose or network sneak state, no
hitbox change. Separate from Permanent Sneak, which feeds real `SneakDown`.
2026-09-26 trace: `PlayerMoveInput::isSneakDown` is never called for any
entity on the client while walking, sneaking or at edges, so it is not the
edge check. Next candidates: the movement/collision systems that read the
sneaking state (`SneakingComponent`, actor sneaking flag or move-input state).
Find the vanilla edge-protection check in 26.51.5 (around `Actor::move` /
movement collision) and whether only that check can see "sneaking". Prefer
reusing that vanilla path over clamping movement ourselves (slabs, stairs,
diagonal moves and scaffolding would need re-implementing). Runtime checks:
full block edge, slab/stair, diagonal, sprint, jump, scaffolding, knockback,
and that other players see a normal, non-sneaking player.

### L-44 Static FOV
Kind: Research (small). Notion idea, promoted 2026-09-26.
Status: closed 2026-09-26, not needed. The video settings do have a vanilla
option that keeps FOV fixed; with it on, sprinting kept `fovModifier` at 1.0
and FOV at 90 in the trace (off: 1.28 and 115).
Keep FOV fixed when sprinting, speed/slowness effects or flying would change
it. Find where 26.51.5 applies the dynamic FOV modifier; it should sit next to
Zoom's FOV hook and must compose with Zoom.
2026-09-26: the video settings have no vanilla option for this.
`LevelRendererPlayer::getFov` runs twice a frame: `variable=true` (world,
99 with a 90 setting and modifier 1.1) and `variable=false` (70, likely the
hand). Sprinting samples are still to be recorded.

### L-18 FreeCamera
Status: done as an experiment (Pi, reviewed 2026-09-24). Flight, movement
freeze, first-person lock and exits were verified in game; details and failed
approaches are in docs/CAMERA.md. Follow-ups: L-25 to L-29 (L-27 is in
Design), L-37 and the L-10 note. Open polish: starting from third person begins at the head rather
than at the previous third-person eye; needs a new approach if wanted.

### L-20 Shape name text input adds stray characters
Status: closed 2026-09-25, not reproducible. The maintainer typed shape names
without stray characters and does not recall reporting this; it was most
likely a misreading of the (since fixed) garbled text at the lower left of
the Shapes screen.

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

### L-34 Auto attack and auto use (periodic, hold, fast click) **(strong model)**
Status: first version (separate Periodic/Hold/Fast toggles) passed its
in-game checklist 2026-09-25 (DLL 85fdf605) but was hard to understand;
reworked the same day to one switch + one mode + a next-mode key, with no
implicit stops (DESIGN "Automatic attack and use", AUTOMATION.md), then Fast
click's held-only switch and keyed option rows. Status: done (verified in game
2026-09-25, DLL 66d534be). The list below is the original brief.
- Rework Periodic Attack/Use into Auto attack / Auto use with three modes:
  Periodic (hotkey toggle, interval in ticks), Hold (hotkey toggle, keeps
  the button down), Fast click (hotkey on/off; while on, holding the button
  clicks N times per tick).
- Drive clicks from the client tick instead of wall-clock timers so whole
  ticks are exact; Fast click may issue several clicks per tick.
- A physical press stops Periodic/Hold; Periodic and Hold of one action are
  exclusive; existing stop rules (menus, focus, world, dimension) apply.
- Settings migrate from the current periodic interval (seconds) to ticks.
- Verify in game: several attacks/uses per tick actually land on Bedrock
  (scaffolding, snowballs, mobs); Hold keeps mining a block; servers.

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

### L-31 Continuous Tool Switch across block transitions
Status: done (verified in game 2026-09-25, DLL c81c6cb1). SurvivalMode does not
override `continueDestroyBlock`, so `GameMode::$continueDestroyBlock` is
hooked; a pure `ToolTarget` (ToolChoice.h, tested) re-runs the unchanged
choice rule once per new block position, and `stopDestroyBlock` clears it.
Only the client's local player is tracked (the integrated server's player
also breaks blocks). Whether vanilla also calls `startDestroyBlock` on the
new block does not matter to this change; the in-game dirt/wood/stone hold
check confirmed that `continueDestroyBlock` carries the new position.
2026-09-24 in-game observation: Tool Switch chooses for the first block, but a
continuous physical left-click can move from dirt to wood to stone without
re-evaluating the tool for each new target. The current hook only calls
`selectTool` from `GameMode::startDestroyBlock`.

Desired behavior:
- While the player keeps the physical attack button held, re-evaluate Tool
  Switch whenever the actual targeted block transitions to a new block.
- Dirt -> wood -> stone should be able to select shovel -> axe -> pickaxe
  without requiring the player to release left click.
- Preserve the existing per-target choice rule: if the currently selected item
  is already an effective harvesting tool for the new block, do not switch just
  because another tool is faster.
- Respect Breaking Restriction before selecting and do not synthesize attacks,
  continue breaking through UI ownership, or keep a stale target after focus,
  world/dimension, or input cancellation.

First establish which 26.51.5 client path carries the new block while an attack
is held (`continueDestroyBlock` may be sufficient, but that must be verified).
If it is, make the smallest hook/change and add pure choice/transition tests plus
an in-game dirt/wood/stone hold check. If not, trace the attack/retarget path
rather than polling arbitrary world state.

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
- L-26 FreeCamera flight speed (parked, after L-18). Currently fixed at
  20 blocks/s. Add a user-facing speed setting with sane
  bounds; decide on a fast-flight modifier, if any, at design time.
- L-28 Third-person underground camera (parked, after L-18). While detached
  underground, vanilla collision avoidance fights the pivot offset and the
  view judders block by block. Decide whether to soften avoidance while
  detached or document it as a limit; above-ground flight is unaffected.
  Moot while FreeCamera locks first person.
- L-29 Hide the hotbar while detached (parked, after L-18). Requested
  2026-09-24, Tweakeroo-like: an option to hide the hotbar while FreeCamera
  is active (looking-only flight needs no hotbar). Find the vanilla hotbar
  render entry first; Freelook is out of scope unless trivially shared.
- L-21 Shape color picker or more colors: only if the four colors prove
  insufficient.
- Not started, not yet triaged: F3-style debug view, Schematic subsystem
  (browser, placement, projection, verifier, material list), Mass Craft. These
  need a Design pass before they become tasks. (Fast Attack/Use became L-34;
  Scroll Transfer became L-41.)
