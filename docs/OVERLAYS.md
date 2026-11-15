# Overlay foundation

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/OVERLAYS.md`.

World-space overlays drawn by Lamium: Shapes (block-grid building guides with
their own view and per-world file), Chunk Borders, Hitboxes, and the shared
face/line drawing that the light overlay ([LIGHT-OVERLAY.md](LIGHT-OVERLAY.md)),
the breaking restriction ([RESTRICTIONS.md](RESTRICTIONS.md)) and Schematics
([SCHEMATIC.md](SCHEMATIC.md)) also use.

Settings and keys:
- Shape rendering: switch with the `toggleshapes` key on the parent row, the
  `openshapes` command on a child row (both unbound). Shapes also open from
  the pinned sidebar entry.
- Chunk Borders: default off, `chunkborders` key, default `F3+G`.
- Hitboxes: `hitboxes` key, default `F3+B`.

## Behavior

### World overlays

- Block-grid shapes show the blocks that form the shape, not smooth
  wireframes. Wireframes are for Chunk Borders and Hitboxes. Turning center
  snapping off never turns a building guide into a smooth mesh.
- (Decided 2026-10-12) Against the vanilla block selection outline (the black
  frame on the targeted block): position markers win, building guides yield.
  Chunk Borders, Hitboxes and the schematic corners and save area draw over
  it; Shapes and the light overlay let it show over them; the breaking
  restriction leaves the targeted block out. `Depth.h` holds the pulls that
  set this order.
- (Decided 2026-10-07) World-space overlays (Chunk Borders, Hitboxes, light
  levels, Shapes, schematic ghosts) stay visible with vanilla Hide HUD (F1).
  Screen HUD elements and in-world waypoint markers hide with it (L-106,
  L-107).
- (Decided) Line overlays get per-batch colors so Chunk Borders and Hitboxes
  follow Java's F3+G / F3+B color coding.
- Meshes are rebuilt only when a shape changes, never per frame.

### Depth

Every face overlay follows `overlay/Depth.h` (rules in its header comment):
faces lie on their cell's own plane, never inset into a cell that may be
solid; the whole mesh is pulled toward the eye by a fraction so it stays in
front of the block face at any distance and angle; an offset along the
normal is allowed only toward the side the camera must be on (light tints
lift into their air cell); layers drawn over each other use increasing
pulls. Pulls: schematic ghosts .998; faces and light tints .997; light
digits and marks over the selection outline .995; Chunk Borders and
Hitboxes .994; light digit lines .993 (save-area corners in between, see
the file).

### Shapes view

The view places a shape list beside an editor. The list shows color, name,
type (other dimensions are dimmed and tagged) and a visibility switch. The
editor shows the name, a top-down block preview of one layer (planes are seen
along their normal) with a layer stepper, and fields grouped as shape,
position and display. Fields come from `ui/ShapeEditor.h`: adding a shape type
means adding its entry and fields there. The header toggles all shape drawing,
links to the Shape rendering row in settings, and docks the view to the right
edge so the world stays visible (the draw-all switch then moves to the
toolbar). Editing keeps both scroll positions.

- "New shape" first asks for a type, then opens a draft previewed in the
  world as light-blue lines and not saved. Create adds it; Cancel or leaving
  the view discards it. Delete needs a second press.
- The center reference is the standing block (snapped to its center), the
  exact position (no snapping) or the targeted block; "Move here" uses the
  same reference. Standing and exact follow the drawn view (L-248): while
  Freelook or FreeCamera detaches it they use the camera's virtual feet (its
  position minus the player's eye height), as the schematic placement actions
  do. A targeted block comes from the hit alone.
- Follow the player (L-311, maintainer 2026-11-06): one switch per shape.
  The feet block is the center, height included; FreeCamera keeps it on the
  body; it is drawn only in the shape's own dimension and the saved
  coordinates do not change while following. Turning it off leaves the shape
  where it is drawn, which becomes its saved place; from another dimension,
  where the player left the shape's dimension. The maps draw the followed
  place.
- Shapes also appear on the world map and minimap ([MAP.md](MAP.md)).

### Shape look

- Presets: circle, cylinder, sphere, box, cone, frustum, pyramid, ellipsoid,
  dome, oriented on Y/X/Z where supported, plus rectangular planes (XZ, XY,
  YZ).
- Style: faces (default; translucent, unlit, two-sided, faint outline) or
  lines (block edges).
- Colors: cyan (default), yellow, pink, white. Drafts are light blue.
- Face material follows the graphics mode (`overlay/FaceMaterial.h`): Fancy
  uses the hologram pointer material with both windings; Simple uses the
  two-sided additive lightning material, fainter, and the Shapes view footer
  warns about it; Vibrant Visuals tries lightning and draws full-strength
  outlines so the shape stays readable even if faces do not show.

### Shape geometry conventions

These are Lamium's own conventions:

- Centers default to the containing block's center (`floor(position) + 0.5`),
  negative coordinates included. Block Corner uses the block's lower corner;
  Off keeps the supplied center. Radius tests sample block centers.
- A filled circle is one disk layer; its horizontal boundary cells form a
  ring. A cylinder repeats the disk upward from the center's block Y for the
  given number of layers. A sphere uses 3D distance. Boundary extraction
  removes shared internal faces.
- Planes: unit spacing fills the plane; larger spacing draws grid lines with
  a complete outside border. Face vertices lie on block boundaries with
  outward winding.
- `gridSurfaceLines` turns exposed faces into unique unit edges, keeping the
  grid seams on the outside surface and dropping edges from internal faces
  only; canonical integer endpoints avoid reversed duplicates.
- Enumeration is limited to 250,000 candidate cells and line conversion to
  one million lines by default; oversized shapes are rejected before
  enumeration, never truncated. Coordinates are checked before integer
  conversion. The limit is not a frame-time guarantee: generation stays out
  of drawing.

### Shape collection and persistence

- `ShapeCollection` holds stable session IDs, names, visibility, dimension
  and cached block-surface geometry. Defaults: 32 shapes and 4,000,000 total
  cached lines, hidden shapes included. Geometry is built when a shape
  changes; a failed generation or budget check leaves the previous entry.
  IDs are not reused by clear, and a successful load assigns fresh IDs, so a
  stale editor selection cannot reach another shape.
- The runtime owns the collection behind a mutex and exposes definition-only
  snapshots and mutations through `ShapeSession.h`.
- `ShapeWorkspace` binds the collection to a world file. Local-world joins
  resolve `lamium/shapes.json` inside the world directory: the current game's
  `FilePathManager::mWorlds` root plus the Level ID must name an existing
  direct-child directory with `level.dat`. Relative roots, traversal or
  separator IDs and non-world directories are rejected; personal
  directories are never scanned. The file travels with a copied world.
  Remote or unresolved worlds stay session-only.
- Entering another workspace clears the previous shapes before reading. A
  failed load leaves an empty collection and blocks editing until reentry,
  so an unreadable file is never overwritten with an empty one.
- Changes build a complete candidate, save it, and publish it to rendering
  only after the file is written. A failed save keeps both the live shapes
  and the file and shows a save error. Leaving clears the binding, so
  later session-only changes cannot write into the departed world's file.
  Candidate copies and validation stay outside rendering.
- World exit and overlay shutdown release session state and meshes.

### Shape document format (`lamium/shapes.json`)

Version 1 JSON with a `shapes` array. Every entry stores `name`,
`dimension`, `visible`, `style`, `color` and `geometry`; `follow: true` is
written only when on (older builds ignore it). Runtime IDs and derived
faces/lines are never stored.

- Shape geometry types `circle`, `cylinder`, `sphere`, `box`, `cone`,
  `frustum`, `pyramid`, `ellipsoid`, `dome`: a three-number `center`, `snap`
  (`block_center`, `block_corner`, `off`), `radius`, integer `height`, `axis`
  (`y`, `x`, `z`), `topRadius`, `heightRadius`, `dome`. Fields added after the
  first version-1 files are optional on read with compatible defaults.
- Plane geometry: `type: plane`, integer `origin`, `width`, `depth`,
  `spacing`, `plane` (`xz`, `xy`, `yz`).
- Decoding rejects unsupported versions or options, malformed values,
  documents over 1 MiB, more than 32 entries and collections over the
  geometry budget. Integer fields never silently truncate fractions. A
  replacement is validated whole before it is swapped in; a failed load
  keeps the previous definitions, IDs and caches.
- `ShapeStore` reads at most 1 MiB plus one detection byte. Its writer
  validates first, creates a sibling temporary file exclusively, writes and
  flushes it, then replaces the destination; on failure it removes only its
  own temporary file and leaves the destination intact.

### Chunk Borders

Draws the current chunk's walls with a yellow 2-block grid, blue 16-block
section lines and current-chunk corners, and red neighbor-chunk corners,
using floor division at negative coordinates and the dimension height range.
During Freelook/FreeCamera the view's chunk replaces the player's as the
center. Line coordinates are cached per render thread by chunk origin and
height range: moving inside a chunk reuses them; crossing a boundary or a
height change rebuilds. Inputs are validated on cache hits too, and a failed
generation keeps the last valid entry. Meshes are temporary;
camera-relative vertices are submitted each frame.

### Hitboxes

White actor bounds plus a red eye marker and a blue 2-block look line for
ordinary mobs. Lines use each actor's frame-interpolated render position;
the eye marker adds an interpolated, actor-specific eye offset sampled from
`getEyePos()`, and the look vector uses the same frame alpha. Ender Dragon
multipart damage boxes are not guessed
([L-30 Ender Dragon multipart hitboxes on Bedrock](backlog/L-30.md)).

### Line drawing

Chunk Borders and Hitboxes submit grouped per-color line batches through the
shared tessellator path, pulled toward the eye by `worldLinePull`, and keep
no actor or player pointer across frames. `drawLines()` submits each
nonempty color group separately (up to five for Chunk Borders, three for
Hitboxes) with the `LineColor.h` selection-outline material and that group's
shader color, restored after every draw; vertex colors stay for the `debug`
fallback.

## Code

- `overlay/CellSurface.h` (pure): faces on the cells' own planes and the
  outline for a set of cells. `overlay/CellMesh.*`: upload, face material,
  outline coloring, pull by `Depth.h`. Shapes, the breaking region and the
  schematic save area all draw through these, each with its own colors and
  visibility rules.
- `overlay/Depth.h`, `overlay/FaceMaterial.h`, `overlay/LineColor.h`.
- `overlay/Geometry.h` (pure): rasterizing presets and planes,
  `gridSurfaceLines`, `faceLines`.
- `overlay/ShapeCollection.h`, `ShapeSession.h`, `ShapeWorkspace.h`,
  `ShapeDocument.*`, `ShapeStore.*`, `LocalShapePath.h`.
- `overlay/ChunkBorders.h`, `overlay/Hitboxes.h`; rendering in
  `overlay/WorldOverlay.cpp`. Each shape's faces and lines are uploaded once
  per geometry revision relative to its first block and positioned each
  frame with the world matrix; a following shape is drawn shifted by whole
  blocks without a rebuild.
- `ui/ShapesView.*`, `ui/ShapesLayout.h`, `ui/ShapeEditor.h`.
- Tests: OverlayGeometryTests (depth, snapping, presets, shared-face removal,
  rings, planes, grid gaps, winding, budgets), codec and store tests (legacy
  fields, presets/axes, UTF-8, large coordinates, malformed input, replace
  blocked by an open Windows handle), chunk-border cache tests, editor rows,
  reference placement, preview layers and view hit testing.
- `xmake f --shape_trace=y`: bounded primary-player join diagnostics (Level
  ID and storage directory).

## Known

- For a local world the Level ID is the storage directory name and stays the
  same across save, exit and reentry. A world display name or server address
  alone does not identify a world.
- The vanilla block selection outline beats a pull of .996 but not .995.
- The hologram pointer material is not loaded in Simple graphics (its
  pointer resolves but draws nothing) and does not show in the Vibrant
  Visuals deferred pipeline. The block selection material multiplies the
  scene color, so it is unsuitable for faces.
- One `debug` mesh with vertex colors loses its colors in Vibrant Visuals.

## Tried

- Insetting faces 0.005 into their cell: at a grazing angle a few blocks
  away the inset outgrows the pull and the faces fall behind the block face,
  fighting the terrain (replaced by the `Depth.h` rules, L-110).
- Lines on block faces without a pull break into dashes (L-157); a pull of
  .996 still lets the selection outline cover them.
- Chunk Borders and Hitboxes as one vertex-colored `debug` mesh: colors lost
  in Vibrant Visuals (L-176).

## Limits

- Remote worlds keep shapes for the session only; there is no in-screen
  load retry after a failed load.
- Ender Dragon part boxes are not shown.
- In-game coverage, graphics modes, resource packs and performance:
  [VALIDATION.md](VALIDATION.md) rows "Shapes (L-13)" and "Chunk Borders
  (L-10), Hitboxes (L-11, L-51)".
