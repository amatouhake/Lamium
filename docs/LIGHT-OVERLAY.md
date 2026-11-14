# Light level overlay

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/LIGHT-OVERLAY.md`.

Shows stored light levels as digits on the floor around the player, with a
faint tint where monsters can spawn. Mockup:
[demos/light-overlay.html](demos/light-overlay.html).

Settings (no Experimental badge): `overlays.light` (switch, toggle key
`lightoverlay`, unbound), `overlays.lightValue` (`block` default, `sky`,
`both`; an old `skyLight: true` loads as `sky`), `overlays.lightRange`
(4-64, default 16) and `overlays.lightFacing` (follow the view, default, or
a fixed north/east/south/west up).

## Behavior

(Decided 2026-09-25, L-16.)

- Numbers plus color. Numbers lie on the floor, filled (not thin lines), and
  turn by quarter turns so their top points where the viewer looks (the
  detached camera's direction during Freelook/FreeCamera).
- Color by spawning, as a faint tint over the whole floor: red = monsters
  can spawn at any time (block light 0, stored sky light below 7), yellow =
  only at night (block light 0, sky light 7 or more), none = block light 1+.
  From minecraft.wiki (Bedrock); compared with real night spawning in game.
- Numbers show block light by default; the setting also offers sky light or
  both (block above, sky below in blue).
- Range: radius 16 by default, 4-64 in settings, the same distance up and
  down (flying high still shows the ground). Whole chunk columns are shown.
  Users lower it if their machine struggles.
- Centered on the camera while it is detached (FreeCamera), like Chunk
  Borders.
- Number direction (2026-09-26): follow the view or a fixed direction.
  Following rebuilds nearest chunks first within a per-frame budget, so
  turning at a large range does not stall; a fixed direction never rebuilds
  on turning.
- Markers sit on air cells whose block below reports solid. Unavailable or
  invalid samples draw no marker, never an invented zero. Values are the
  client's stored light, not effective daylight or server-only data.
- Drawn like Shapes: meshes kept per chunk column, pulled toward the eye
  (`overlay/Depth.h`), and as faint as shape faces on the additive material
  (Simple, Vibrant Visuals). The tint sits a hair (5 mm) above the floor and
  the digits just above it (12 mm). Tints, digits and Vibrant Visuals' line
  digits are separate meshes with growing pulls (.997, .995, .993), so their
  depth gap grows with distance. Shapes and the light overlay let the
  vanilla selection outline show over them ([OVERLAYS.md](OVERLAYS.md)).
- Reading is spread over frames: a bounded number of chunk columns per
  frame, columns next to the viewer re-read every 0.25 s, the rest every
  2 s.

## Code

- `overlay/LightOverlay.h` (pure, LightOverlayTests): `spawnRisk`,
  `facingFromYaw` and `floorPoint` (digits turn toward the view),
  `appendLightNumberQuads` (filled strokes, non-overlapping cells of a 3x5
  bar grid) and `appendLightNumberLines` (Vibrant Visuals), `lightTintQuad`,
  `chunksInRange`, `LightSchedule` (which columns to read this frame).
  `overlay/LightMode.h`: value and facing choices.
- `overlay/WorldOverlay.cpp` keeps markers and a mesh per chunk column. It
  reads at most 32,768 cells per frame, rebuilds a mesh only when its
  markers, the viewing quarter, the number mode or the material change, and
  draws it with Shapes' toward-the-eye matrix in the shape face material.
- Tests also cover negative coordinates, stacked floors, unavailable and
  invalid samples, coordinate overflow and digit geometry for 0-15;
  settings tests cover labels and round trips.

## Known

- The light overlay's face materials are already two-sided in Simple and
  Vibrant Visuals; reverse winding is added only for Fancy.

## Tried

- Raising tint and digits further off the floor to stop flicker: the height
  was never the cause. The causes were (1) both windings on already
  two-sided materials, so two copies at one depth fought; (2) digit strokes
  overlapping at their corners in one plane; (3) tint and digits a few
  millimetres apart running out of depth precision far away (fixed by the
  growing pulls).
- Scanning a fixed 9x9x5 volume every frame without caching: replaced by
  per-column meshes and the read schedule.

## Limits

- Non-air surfaces, partial blocks and unusual collision shapes are not
  comprehensively handled.
- Frame time is not measured on other machines.
- Coverage: [VALIDATION.md](VALIDATION.md) row "Light Level Overlay (L-16)".
