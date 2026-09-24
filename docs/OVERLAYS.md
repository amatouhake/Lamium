# Overlay foundation

Lamium's world-overlay foundation is integrated rather than compile-only.
Shapes, Chunk Borders, Hitboxes and the experimental light overlay share the
world-rendering infrastructure. Shapes have a dedicated editor and local-world
sidecar persistence. Basic Shapes rendering/persistence, Java-style Chunk
Borders and ordinary Hitboxes have runtime evidence; broad graphics-mode,
resource-pack, crowded-world and lifecycle coverage remains incomplete.

`ShapeCollection` is the game-independent shape manager: stable session IDs,
names, visibility, dimension ownership and cached block-surface geometry.
Geometry is built when a shape changes, not during read-only drawing. Failed
generation or budget checks leave the previous entry intact. The collection
defaults to 32 shapes and 200,000 total cached lines, including hidden shapes;
removing entries frees the budget. IDs are not reused by clear, so a stale
editor selection cannot refer to a new shape.

The runtime owns the collection behind a mutex and exposes definition-only
snapshots/mutations through `ShapeSession.h`. Visible shapes in the current
dimension draw from cached CPU geometry and uploaded meshes. World exit and
overlay shutdown release session state/resources. Local-world joins bind the
workspace to `lamium/shapes.json` under the resolved world directory; unsupported
or remote identities remain session-only.

## Shapes view and rendering

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

Shapes are drawn from the blocks that form them, not as smooth wireframes.
The current presets include circle, cylinder, sphere, box, cone, frustum,
pyramid, ellipsoid and dome, plus rectangular planes; ShapeSpec presets can be
oriented on Y/X/Z where supported. Each shape has a style (faces: translucent
block faces with a faint outline; lines: block edges) and one of the shared
shape colors. Older version-1 files load with compatible defaults for fields
that did not exist yet.

Each shape's faces and lines are uploaded once per geometry revision, relative
to the shape's first block, and positioned each frame with the world matrix.
Meshes are released when shapes are removed or the world is left. Face
materials follow graphics mode as documented in DESIGN.md (Fancy uses the
hologram-pointer path where available; Simple/Vibrant Visuals use the
lightning/strong-line fallbacks). Runtime work established the current
translucent/line strategy, but alternate packs and graphics modes are not
exhaustively covered.

Pure tests cover type definitions, editor rows, stepping and validation,
reference placement, preview layers and runs, and the view's hit testing.

## Shape document format

The definition codec uses version 1 JSON with a `shapes` array. Every entry
stores `name`, `dimension`, `visible`, `style`, `color` and `geometry`.
Runtime IDs and derived faces/lines are never persisted.

Shape geometry types are `circle`, `cylinder`, `sphere`, `box`, `cone`,
`frustum`, `pyramid`, `ellipsoid` and `dome`. They store a three-number
`center`, `snap` (`block_center`, `block_corner`, `off`), `radius`,
integer `height`, `axis` (`y`, `x`, `z`), `topRadius`, `heightRadius`
and `dome`. Fields added after the first version-1 files are optional on read
and receive legacy-compatible defaults. Plane geometry uses `type: plane`,
integer `origin`, `width`, `depth`, `spacing` and `plane` (`xz`, `xy`,
`yz`).

Decoding rejects unsupported versions/options, malformed values, documents over
1 MiB, more than 32 entries and collections exceeding geometry budgets. Integer
fields never silently truncate fractional values. Collection replacement builds
and validates a complete candidate before swapping it in; failed loading keeps
the previous definitions, IDs and caches. Successful replacement assigns fresh
IDs. Codec tests cover legacy fields, new presets/axes, UTF-8 metadata,
large-coordinate precision and malformed input.

`ShapeStore` reads at most 1 MiB plus one detection byte and validates before
returning definitions. Its Windows writer validates first, reserves a sibling
temporary file with exclusive creation, writes and flushes it, then replaces
the destination. On failure it cleans up only its own temporary file and leaves
the previous destination intact. Filesystem tests cover first save, replacement,
UTF-8 names, invalid data, oversized reads and replacement blocked by an open
Windows handle. The store is connected to the local-world workspace: supported local joins
load and save `lamium/shapes.json`; unsupported/remote identities stay
session-only. Save-before-publish behavior preserves the previous live/file
state on failure.

Chunk Borders defaults off. It draws the current chunk walls with a yellow
2-block grid, blue 16-block section lines/current-chunk corners and red
neighbor-chunk corners, using floor division at negative coordinates and the
dimension height range. During Freelook/FreeCamera the view chunk replaces the
player chunk as the center. The Java-style color structure has been compared
against a Java reference in game.

Hitboxes draw white actor bounds plus a red eye marker and blue 2-block look
line for ordinary mobs. Ender Dragon multipart damage boxes are deliberately
not guessed; L-30 tracks whether Bedrock exposes real part boxes. Both overlays
submit grouped per-color line batches through the shared tessellator path and
retain no actor/player pointer across frames. Basic visible output is past the
compile-only stage; crowded-world performance, graphics/resource-pack variants
and broader lifecycle cases still need coverage.

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
shared-face removal, rings, plane orientation, grid gaps, face winding, new
shape-profile/axis cases and invalid/oversized input. Runtime checks cover the
basic renderer/editor path, while exhaustive depth/material, camera-relative
precision, performance and lifecycle coverage remains incomplete.

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
