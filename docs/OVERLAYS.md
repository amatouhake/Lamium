# Overlay foundation

This is an overlay component under development. A prototype world-line backend
and a Chunk Borders setting are connected in source. A first Shape Manager/Editor
is reachable through the searchable Features list. Creating a sphere, changing
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

The initial dedicated Shape panel reuses the settings host's modal input ownership,
translucent panel, row layout and keyboard/mouse navigation. It creates spheres,
circles, cylinders and grid planes at the player position. Its editor exposes
visibility, per-axis movement, radius/height or grid dimensions/orientation,
snapping, duplicate and remove. Left/right adjusts values; Enter/click opens
direct numeric input or performs actions; Escape finishes input or returns to
the parent view. Coordinates/radius use double precision; block origins and
grid dimensions require integers. Invalid or incomplete text leaves the last
valid geometry intact. Edits update the session immediately.
The panel explicitly states that shapes are cleared on world exit. This is an
initial workflow: persistence and broader runtime verification remain outstanding.
The Name row supports immediate UTF-8 renaming without regenerating geometry;
empty/invalid names retain the last valid name. Native name input and IME still
need runtime verification. Session IDs distinguish identical names; stored
coordinates retain full precision while display uses three decimal places.
Successful compilation and
search-row tests do not establish rendering or input correctness in Minecraft.

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

Chunk Borders defaults off. The prototype draws the player's current chunk
boundary, using floor division at negative coordinates and the dimension's
height range, with horizontal edges at 16-block intervals. It hooks the native
entity-effects pass, preserves the original pass, and submits camera-relative
line vertices using the standard `debug` material. A private temporary
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
by the world renderer, but no Shape screen or runtime shape consumer is wired
yet. CPU geometry conversion does not establish visible rendering correctness.

Enumeration has a default work limit of 250,000 candidate cells. Oversized shapes
are rejected before enumeration rather than silently truncated. Coordinates are
checked before integer conversion and retain neighbour-arithmetic headroom.
Shape generation must be cached outside per-frame drawing when connected to
the game; the work limit alone is not a frame-time guarantee.

Pure tests cover negative-coordinate snapping, translations, known small shapes,
shared-face removal, rings, plane orientation, grid gaps, face winding, and
invalid/oversized input. GPU rendering, depth/material behavior, camera-relative
precision, performance, and world/dimension lifecycle remain unverified.
