# Light level overlay

Current behavior is the L-16 redesign below; it no longer carries the
Experimental badge. Current coverage is in [VALIDATION.md](VALIDATION.md).
The initial implementation and smoke check are retained as dated history.

## Redesign (L-16, 2026-09-25)

Decided in DESIGN "Light overlay" (mockup: docs/demos/light-overlay.html).

- Settings: `overlays.light` (switch), `overlays.lightValue` (`block`
  default, `sky`, `both`; the old `skyLight: true` loads as `sky`) and
  `overlays.lightRange` (4-64, default 16; same distance up and down).
- Pure logic in `overlay/LightOverlay.h`, tested in LightOverlayTests:
  `spawnRisk` (red/yellow/none), `facingFromYaw` and `floorPoint` (digits turn
  toward the view), `appendLightNumberQuads` (filled strokes) and
  `appendLightNumberLines` (fallback), `lightTintQuad`, `chunksInRange` and
  `LightSchedule` (which chunk columns to read this frame).
- `WorldOverlay.cpp` keeps markers and a mesh per chunk column. It reads at
  most ~32k cells per frame (near columns every 0.25 s, far ones every 2 s),
  rebuilds a mesh only when its markers, the viewing quarter, the number mode
  or the material change, and draws it with Shapes' toward-the-eye matrix in
  the shape face material. With Vibrant Visuals the digits are also lines.
  The center is the detached camera during FreeCamera.
- Verified in game 2026-09-26 (DLL 9e7fd594): spawn colors against real
  night spawning, digits readable and steady in Fancy, Simple and Vibrant
  Visuals, torch placement refresh, radius 64 while walking and flying,
  FreeCamera center and direction, fixed directions, dimension changes.
  Not measured: frame time numbers on other machines.

## Initial implementation record (before the L-16 redesign)

Experimental, disabled and unbound by default. Features and Hotkeys expose a
toggle; the feature also selects stored sky light instead of stored block light.
Settings apply immediately and use the existing persistence and binding system.

The initial implementation scans a 9 by 9 by 5 cell volume around the player's
floored feet position (405 candidates). It draws decimal 0 through 15 as flat line
digits above air cells whose block below reports solid through the SDK. Multiple
floors in the volume can each have a marker. Digits have north at their top.
Only loaded chunks and cells within the dimension height range are queried.
Unavailable/invalid samples produce no marker, never an invented zero.

These are stored client light values, not effective daylight, a hostile-mob
spawn verdict, or server-only information. Non-air surfaces, partial blocks and
unusual solid/collision shapes are not comprehensively supported. This first
pass scans each rendered frame and keeps no world references or cached samples;
runtime performance must be measured before increasing its fixed small range.
Digit geometry appends directly into one reserved render batch, avoiding a
temporary vector and copy for every marker. This does not reduce native world
queries or cache light values; frame-time impact has not been measured.

Pure tests cover negative coordinates, stacked floors, unavailable and invalid
samples, coordinate overflow, and decimal geometry for every valid value.
The shared settings tests exercise labels and disk round trips for both options.
Native value correctness, solid-surface filtering, depth/readability, light
updates and dimension changes still require Minecraft validation. Do not classify
this feature as runtime-validated based on build/tests alone.

### Initial runtime smoke (2026-09-23)

The normal (non-trace, non-probe) build of `a4213a0` displayed floor digits
in a local creative world in rear third-person view. Switching from stored block
light to stored sky light changed visible digits from 0 to 15; the overlay also
remained visible after closing settings. Switching back and disabling the feature
removed the digits while the existing Shape overlay remained visible. Both light
options were restored to Off, with the binding still Unbound, before normal exit.

This verifies rendering and immediate settings integration in that scene only.
There was no independent comparison of the sampled values, light-source edit,
dimension transition, or performance measurement. Those validation gates remain
open. See [VALIDATION-LOG.md](VALIDATION-LOG.md) for the installed build identity.

## Light overlay (Decided 2026-09-25, docs/demos/light-overlay.html)

Moved from DESIGN.md on 2026-11-14 without changes; still the decided behavior.
- Numbers plus color. Numbers lie on the floor, filled (not thin lines), and
  turn by quarter turns so their top points where the viewer looks (the
  detached camera's direction during Freelook/FreeCamera).
- Color by spawning, as a faint tint over the whole floor: red = monsters
  can spawn at any time (block light 0, stored sky light below 7), yellow =
  only at night (block light 0, sky light 7 or more), none = block light 1+.
  From minecraft.wiki (Bedrock); confirm in game.
- Numbers show block light by default; the setting also offers sky light or
  both (block above, sky below in blue).
- Range (revised after the first in-game check): radius 16 by default, 4-64
  in settings, the same distance up and down (flying high still shows the
  ground). Whole chunk columns are shown. Users lower it if their machine
  struggles.
- Centered on the camera while it is detached (FreeCamera), like Chunk
  Borders; digits turn with the camera's direction.
- Number direction setting (2026-09-26): follow the view (default) or a
  fixed north/east/south/west up. Following rebuilds nearest chunks first
  within a per-frame budget, so turning at a large range does not stall; a
  fixed direction never rebuilds on turning.
- Like shape faces, the tint sits a hair (5 mm) above the floor and the
  digits just above it (12 mm); the mesh is nudged toward the eye. The
  flicker seen in Simple and Vibrant Visuals was never the height: (1) every
  quad had both windings on already two-sided materials, so two copies at one
  depth fought (reverse winding now only for Fancy), and (2) digit strokes
  overlapped at their corners in one plane (digits are now non-overlapping
  cells of a 3x5 bar grid), and (3) tint and digits a few millimetres apart
  run out of depth precision far away. Tints, digits and Vibrant's line digits
  are separate meshes nudged toward the eye by growing factors (.997, .995,
  .993), so their depth gap grows with distance like Shapes' own nudge.
- Drawn like Shapes: meshes kept per chunk, nudged toward the eye so they do
  not flicker against the floor, and as faint as shape faces on the additive
  material (Simple, Vibrant Visuals).
- Reading is spread over frames: a bounded number of chunk columns per frame,
  columns next to the viewer re-read every 0.25 s, the rest every 2 s.
