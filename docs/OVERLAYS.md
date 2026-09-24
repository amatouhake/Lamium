# Overlay foundation

This is an overlay component under development. A prototype world-line backend
and a Chunk Borders setting are connected in source. Shapes are managed in the
Shapes view described below. Creating a sphere, changing
its radius and hiding it were verified in Minecraft; cyan grid lines changed
accordingly behind the editor. This does not validate Chunk Borders, other shape
types, projection/depth accuracy, camera movement or world/dimension lifecycle.

`ShapeCollection` now provides the game-independent manager model: stable
session IDs, names, visibility, dimension ownership, and cached grid-surface
lines for circles, cylinders, spheres and rectangular grid planes. Geometry is
built on add/edit, never during read-only drawing. Visibility changes retain
the cache. Failed generation or budget checks leave the previous entry intact.
The collection defaults to 32 shapes and 200,000 total cached lines, including
hidden shapes; removing entries frees the budget. IDs are not reused by clear,
so a stale editor selection cannot refer to a new shape. The future session
owner must clear on world exit; dimension IDs alone do not identify worlds.
Tests cover cache retention, dimension filtering, transactional edits, aggregate
budget recovery and stale IDs.

The runtime now owns this collection behind a mutex and exposes definition-only
snapshots and mutations through `ShapeSession.h`. The world render hook submits
cached lines only for visible shapes in the player's current dimension. World
exit and overlay shutdown clear the collection; initialization failures clean
up the exit listener and render hook. This wiring builds against the client SDK,
with visible geometry, lifecycle callbacks and render-thread performance still
unverified.

## Shapes view and rendering (2026-09-23; runtime validation pending)

Shapes open from the settings sidebar (pinned below Hotkeys) or the
initially unbound `openshapes` key; they are no longer a row in the Features
list. Features keeps a Shape rendering entry: its switch hides or shows every
shape, with `toggleshapes` and `openshapes` bindings.

The view places a shape list beside an editor. The list shows color, name,
type (other dimensions are dimmed and tagged) and a visibility switch. The
editor shows the name, a top-down block preview of one layer (planes are seen
along their normal) with a layer stepper, and fields grouped as shape,
position and display. Fields come from `ui/ShapeEditor.h`: adding a shape type
means adding its entry and fields there. The header toggles all shape drawing,
links to the Shape rendering row in settings, and docks the view to the right
edge so the world stays visible (the draw-all switch then moves to the
toolbar). Editing keeps both scroll positions.

"New shape" first asks for a type, then opens a draft that is previewed in the
world as light-blue lines but not saved. The center reference is the standing
block (snapped to its center), the exact position (no snapping) or the
targeted block; "Move here" uses the same reference for existing shapes. Create
adds the draft; Cancel or leaving the view discards it. Delete requires a
second press.

Shapes are drawn from the blocks that form them, as in MiniHUD: circles and
cylinders as rings, spheres as the outer surface of the filled volume, planes
as their grid. Each shape has a style (faces: translucent block faces with a
faint outline; lines: block edges) and a color; both are optional fields in
the shape document, so older files load with faces and cyan. Previously the
renderer rebuilt and uploaded every line of every shape each frame, and
circles were drawn as filled disks. Now each shape's faces and lines are
uploaded once per geometry revision, relative to the shape's first block, and
positioned each frame with the world matrix. Meshes are released when shapes
are removed or the world is left. Faces use the block selection overlay
material; whether it blends as intended must be confirmed in Minecraft.

Pure tests cover type definitions, editor rows, stepping and validation,
reference placement, preview layers and runs, and the view's hit testing.

## Shape document format

The definition codec now supports version 1 JSON documents with a `shapes`
array. Each entry contains `name`, `dimension`, `visible` and `geometry`.
Round shapes use `type` (`circle`, `cylinder`, `sphere`), a three-number `center`,
`snap` (`block_center`, `block_corner`, `off`), `radius` and integer `height`.
Planes use `type: plane`, integer `origin`, `width`, `depth`, `spacing` and
`plane` (`xz`, `xy`, `yz`). Coordinates retain double precision. Session IDs
and derived geometry are excluded.

Decoding rejects unsupported versions/options, malformed values, documents over
1 MiB, more than 32 entries, and collections exceeding geometry budgets. Integer
fields never silently truncate fractional values. Collection replacement builds
and validates a complete candidate before swapping it in; failed loading keeps
the old definitions, IDs and caches. Successful replacement assigns fresh IDs.
Codec tests cover round trips across all shape/snap/plane variants, UTF-8 metadata,
large-coordinate precision and malformed input.

`ShapeStore` reads at most 1 MiB plus one detection byte and validates before
returning definitions. Its Windows writer validates first, reserves a sibling
temporary file with exclusive creation, writes and flushes it, then replaces
the destination. On failure it cleans up only its own temporary file and leaves
the previous destination intact. Filesystem tests cover first save, replacement,
UTF-8 names, invalid data, oversized reads and replacement blocked by an open
Windows handle. World association and UI saving/loading are not connected yet;
the running game session still does not persist automatically.

Chunk Borders defaults off. It draws the current chunk walls with a yellow
2-block grid, blue 16-block section lines and current-chunk corners, and red
neighbor-chunk corners, using floor division at negative coordinates and the
dimension's height range. While a detached camera is active the view chunk
replaces the player chunk as the center. Hitboxes draw the white bounds plus
a red eye-height box and a blue 2-block look line per entity. Callers submit
grouped (lines, color) batches through one tessellator upload. It hooks the
native entity-effects pass, preserves the original pass, and submits
camera-relative line vertices using the standard `debug` material. A private temporary
tessellator avoids overwriting a shared vanilla vertex batch. No world or player
pointers are cached across frames. Depth behavior, render timing, mesh lifetime,
camera transforms, resource-pack compatibility, and performance require runtime
validation; successful compilation is not evidence of correct visible output.

Chunk border line coordinates are cached per render thread by chunk origin and
dimension height range. Movement within a chunk reuses the same CPU geometry;
crossing a chunk boundary or changing the height range rebuilds it. Reuse across
worlds with identical bounds is safe because the cache contains only coordinates.
Inputs are validated even on cache hits, and failed generation preserves the last
valid cache entry. GPU meshes remain temporary and camera-relative vertices are
still submitted each frame. Unit tests cover reuse, positive/negative boundary
crossings, height changes, and invalid input; runtime performance is unmeasured.

World-space lines and wire boxes use continuous coordinates. Building guides use
integer block cells and exposed block faces. They are distinct representations;
turning center snapping off never converts a building guide into a smooth mesh.

Shape centers default to the containing block's center (`floor(position) + 0.5`),
including negative coordinates. Block Corner uses the containing block's lower
corner, and Off retains the supplied center. Radius tests sample block centers.
The filled representation of a circle is a single disk layer; its horizontal
boundary cells form a ring. A cylinder repeats that disk upward from the center's
block Y for the specified number of layers. A sphere uses three-dimensional
distance. Boundary extraction removes shared internal faces; shell cells can be
obtained separately. These are explicit Lamium conventions, not a claim of exact
voxel-for-voxel equivalence with another mod.

Rectangular planes support XZ, XY, and YZ orientations. Unit spacing fills the
plane; larger spacing produces grid lines with a complete outside border. Face
vertices lie on block boundaries and have outward winding for subsequent line
or triangle rendering.

`gridSurfaceLines` converts exposed block faces into unique unit-length edges.
It preserves grid seams on the outside surface so individual block positions
remain visible, while excluding edges contributed only by internal faces.
Canonical integer endpoints avoid reversed-edge duplicates and floating-point
matching. The operation rejects inputs over 250,000 cells or its configurable
line budget (one million by default); it never returns a truncated guide.
Tests cover a single block, adjacent blocks, a solid cube, internal-edge removal,
and budget rejection. These lines use the same `Line` representation accepted
by the world renderer and the session Shape Manager/Editor. CPU geometry
conversion alone does not establish visible rendering correctness.

Enumeration has a default work limit of 250,000 candidate cells. Oversized shapes
are rejected before enumeration rather than silently truncated. Coordinates are
checked before integer conversion and retain neighbour-arithmetic headroom.
Shape generation must be cached outside per-frame drawing when connected to
the game; the work limit alone is not a frame-time guarantee.

Pure tests cover negative-coordinate snapping, translations, known small shapes,
shared-face removal, rings, plane orientation, grid gaps, face winding, and
invalid/oversized input. GPU rendering, depth/material behavior, camera-relative
precision, performance, and world/dimension lifecycle remain unverified.

### Persistence transaction boundary

`ShapeWorkspace` owns a collection and an optional resolved world-specific file.
Entering another workspace clears previous shapes before reading. Successful
loads allocate fresh session IDs so an old editor selection cannot target a
different shape. A failed load leaves an empty collection and blocks editing
until a successful retry or explicit departure; it cannot overwrite the
unreadable source with an empty document.

Changes prepare a candidate collection and publish it only after the complete
file has been written successfully. Failed writes preserve both previous live
values and the destination file. Leaving clears the file binding; subsequent
session-only changes cannot save into the departed world's file. Candidate
copies and geometry validation have a cost and must remain outside rendering.

This boundary is connected to primary-player world entry and world exit. For
local joins, the current game's `FilePathManager::mWorlds` and Level ID resolve
an existing direct-child world directory containing `level.dat`. The shape file
is `lamium/shapes.json` inside that world, separating identical IDs in different
storage roots/profiles and allowing the sidecar to travel with a copied world.
The resolver rejects relative roots, traversal/separator IDs and directories
that do not look like worlds. It does not scan or guess personal directories.

Changes save before publishing to rendering. Save errors have a dedicated
message and keep previous values; load errors clear the previous world's
collection and block edits until reentry. Resolving no supported local target
leaves an explicitly session-only collection. Remote persistence, in-screen
load retry, and broader multi-world/failure runtime validation remain outstanding.
One local-world save/reentry was verified with the stored name, coordinates,
radius, visibility and snap mode restored in the editor and the guide visible.
Runtime testing also verified a replacement-denied save: the previous live
geometry and file survived, a save error was shown, and retyping the edit after
the lock was released successfully saved without leaving the world.

An opt-in `shape_trace` build records bounded primary-player join diagnostics.
One local world was verified to return its storage directory name as Level ID,
unchanged across save/exit/reentry (see `VALIDATION.md`). Persistence integration
uses the storage root to scope local IDs and must separately resolve remote
identities; a world display name or server address alone is insufficient.
