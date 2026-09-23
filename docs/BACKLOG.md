# Lamium backlog

Triage of the maintainer's 2026-09-23 review, plus earlier deferred items.
Each task has a **kind**, which decides who should pick it up:

| Kind | Meaning | Who |
|---|---|---|
| **Ready** | Spec is complete; mostly pure logic + tests + small glue | Any agent, including cheap models |
| **Design** | A user-visible choice is open. Output is a spec (often a web demo) that turns into Ready tasks | Maintainer + strong model |
| **Research** | Needs native reverse engineering, trace builds or runtime-driven debugging | Strong model; cheap models may only collect traces |

Rules: take the lowest-numbered Ready task whose dependencies are done. Do not
start a Design/Research task as a cheap model (see AGENTS.md). When a task is
done, change its status line, update the feature doc, and note the commit.

Confirmed working in the 2026-09-23 review (no action): Freelook, Periodic
Attack, Periodic Use, settings screen, Shapes view.

---

## Quick decisions for the maintainer

These block Ready work. Answer them in one short session.

- **D-1 Settings key (L-01).** Decided: `L` replaces F8. Only this key changes.
- **D-2 HUD proposal.** Direction accepted; details are being agreed through
  the web demo [docs/demos/hud.html](demos/hud.html). Do not implement L-02,
  L-03, L-04 or L-08 until DESIGN.md marks the HUD section Decided.
- **D-3 Shape types (L-13).** Decided: cross-section × profile × axis model
  with familiar names as presets. Which presets come first is still open.

---

## Ready

### L-01 Settings key
Status: ready. Change the `settings` action's default key from F8 (0x77) to
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

### L-02 Replace gameplay key hints with an "Open Hotkeys" action
Status: waiting for D-2.
- Remove the gameplay key-hint overlay and the `interface.gameplayHints`
  setting (keep loading old files without error; just ignore the key).
- Add action `openhotkeys` (Press, unbound): opens the settings screen on the
  Hotkeys view. Mirror how `OpenShapes` opens the Shapes view.
- Files: `Binding.h`, `Actions.cpp`, `SettingsScreen.cpp/.h`, `Options.h`,
  `Settings.h`, `SettingsStore.cpp`, `SettingsRows.h`, `Translations.h`, README.
- Tests: SettingsStore round trip without the key; binding count.

### L-03 Toggle toast
Status: waiting for D-2.
- When a Toggle action changes a feature from a hotkey, show
  `[switch] <feature name>` for 1.5 s centered above the hotbar, fading over
  the last 0.3 s. A new toast replaces the current one. Not shown for changes
  made inside the settings screen.
- Setting `interface.toggleToasts` (default on) in the General category.
- Put the timing/replace logic in a pure header (`ui/Toast.h`) with tests;
  draw with `ui::panel`, `ui::toggleSwitch`, `ui::label`.
- Files: `input/Actions.cpp` (emit), `features/information/InfoHud.cpp`
  (draw call site), new `ui/Toast.h`, settings files, translations.

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
Status: model decided (below, "Proposed model" is accepted); pick the first
presets with the maintainer, then this becomes Ready.

Prior art (behavior only, never code):
- MiniHUD (current fork) ships: box, centered box, circle, square, rhombus,
  block line, blocky sphere, spawn/despawn spheres (several variants),
  ellipsoid spawn, cone, pyramid, diamond pyramid, octagon pyramid. Cones and
  pyramids share one "tapered" model: bottom radius, top radius, height,
  direction. Circles have a main axis. Rendering can pick full block / inner
  edge / outer edge for which cells count as the boundary.
- Building generators and WorldEdit-style tools commonly offer sphere and
  ellipsoid (three radii), cylinder (two radii), pyramid, hollow variants.

Proposed model: a shape is a cross-section × a profile × an axis.
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

### L-04 HUD element system
Merge Info HUD, automation status and restriction status into HUD elements
with anchor presets, offset, scale, background and shadow (DESIGN.md "HUD").
Demo under review: [docs/demos/hud.html](demos/hud.html).
Covers the review points: few info options, hard positioning, plain look,
fixed-position automation status. Start with a web demo, as the settings
screen and Shapes view did. Then split into Ready tasks.

### L-08 Target card (Jade/WAILA-like) redesign
Card with icon, name, source line, provider rows and progress bars in
Lamium's tokens. Depends on L-07 data rows. Web demo first. The drawing
itself should stay with a strong model because visual consistency matters.

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

### L-18 FreeCamera (paused)
Movement extraction and motion math exist but are not connected. Paused by
the maintainer.

### L-20 Shape name text input adds stray characters
Native text entry for shape names inserts extra characters.

---

## Later / parked

- L-19 Freelook in multiplayer, riding, dimension change, controller: runtime
  checks only, no code expected.
- L-21 Shape color picker or more colors: only if the four colors prove
  insufficient.
- Schematic subsystem, Mass Craft, Scroll Transfer: see the Notion roadmap
  (Waves 2–3). Not started.
