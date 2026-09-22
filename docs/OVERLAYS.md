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

Enumeration has a default work limit of 250,000 candidate cells. Oversized shapes
are rejected before enumeration rather than silently truncated. Coordinates are
checked before integer conversion and retain neighbour-arithmetic headroom.
Shape generation must be cached outside per-frame drawing when connected to
the game; the work limit alone is not a frame-time guarantee.

Pure tests cover negative-coordinate snapping, translations, known small shapes,
shared-face removal, rings, plane orientation, grid gaps, face winding, and
invalid/oversized input. GPU rendering, depth/material behavior, camera-relative
precision, performance, and world/dimension lifecycle remain unverified.
