# Overlay foundation

This is an overlay component under development. A prototype world-line backend
and a Chunk Borders setting are connected in source; actual rendering has not
been verified in Minecraft. Shape Manager/Editor is not connected yet.

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
