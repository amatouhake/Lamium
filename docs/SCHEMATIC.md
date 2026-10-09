# Schematics

Implementation notes for loading, placing, projecting, checking and saving
`.mcstructure` files (L-93). Follow-ups are in
[BACKLOG.md](BACKLOG.md#l-93-schematic-load-place-project-verify-and-list-materials-experimental);
runtime coverage is in [VALIDATION.md](VALIDATION.md), with evidence in
[VALIDATION-LOG.md](VALIDATION-LOG.md).

## Current implementation

Experimental, default off; included in 0.1.7. Files live in
`mods/Lamium/schematics/` (subfolders allowed). The screen has Placed, Files,
Check and Materials tabs. It opens the file folder and warns before loading
a file over 2 MB. Placements persist per world/server and dimension.

One selected placement controls movement, turn/mirror, six-direction layer
selection, checking and materials. A radial menu exposes placement and
save-area operations; a separate held adjust key repeats the last stepper
operation with the wheel. Dedicated keys remain available, all unbound.

The default ghost look is tinted with a light-blue outline. Checking,
materials, the optional HUD, target-card line and nearest-mistake marker are
built. Materials include carried shulker-box contents. File entities use named
dashed frames and are checked by type near their recorded spot, within client
observation range; pose, equipment and contents are not verified.

Area save reads bounded chunk columns and waits for unloaded ones as the
player approaches; it can be stopped. Blocks, states and the water layer are
saved, without block entity contents. Optional entities retain type, position
and facing. Existing files require a second press to overwrite.

Known rendering gaps include half-drawn beds and outlines for heads, doors
and honey blocks. Schematic neighbors, the translucent look, the Check tab's
colored preview and the Files tab's rotatable 3D preview remain follow-ups.
Local checks do not establish server, other-dimension or large-file coverage.

## Screen review against the mockup (decided 2026-10-08)

The agent compared `docs/demos/schematic.html` with the screen code and drew
`docs/demos/schematic-screen.html` (UI Profile 75% proportions); the
maintainer decided:
1. World frames: every placement in the dimension gets a frame of solid
   light-blue lines like the ghost outline; the selected placement's frame
   is full strength, the others faint. (Lines are one pixel wide, so the
   difference is opacity, not thickness.)
2. No "previous/next placement" switcher on the Check and Materials tabs: it
   does not scale to many placements; the Placed list is where to pick.
3. Files: grouped under folder headings, with size and block-count columns;
   the size column goes first when the list is narrow.
4. Placed list: the selected placement has the accent bar on its left edge
   (the list's own cursor keeps its outline); progress is a right-hand
   column with the percentage over a short bar (correct / total in the shown
   layers). When the list is narrow, the name is cut with an ellipsis first,
   then the coordinates shorten or drop (the detail pane always has them).
5. Check: the four filters sit above the list with their counts. A
   wrong-state row shows only the block name; selecting it shows every
   differing state in the right pane as "<state> <now> -> <should be>", with
   readable names (L-112) and the drawn arrow.
6. Materials: block and entity sections, and a "show on the schematic HUD"
   switch. Amounts convert as "1 chest + 4 stacks + 5" (number first; a
   chest is 27 stacks; items with a smaller stack size use it) on hover over
   the counts and in the right pane for the selected row. The right pane
   shows the missing materials drawn like inventory slots, and a button that
   opens ResourceCalculator with the remaining amounts in the URL
   (`#oakplanks=64`; item names from the Bedrock ids with a correction
   table), like the world map's ChunkBase link.
7. Parsing ResourceCalculator's results is out: it computes in the browser
   with no API, and its code and recipe data are GPL-3.0 (Lamium is
   LGPL-3.0). Raw materials (the logs behind these planks) come from the
   game's own recipes instead (`Level::getRecipes()`, sent by the server):
   version-correct, no rights question, server recipes included (L-116).
8. 3D previews in the Files and Check tabs: research how vanilla draws 3D in
   UI (the structure block screen); the ghost mesh building already exists
   (L-114).
9. Entities: no limit on name tags. First, missing entities get the
   light-blue outline and faces like block ghosts instead of dashed frames,
   and translucency is dropped; drawing their real models is research
   (L-115). Built 2026-10-08: missing entities are drawn as their game
   models with light-blue part outlines; entities without a model keep the
   dashed frame. How the model is posed and its limits: BACKLOG L-115.

Built 2026-10-08 (not yet checked in game):
- World frames drawn after the ghosts, one line box per placement in the
  dimension (hidden ones only when selected), alpha 1 for the selected one
  and .35 for the others.
- Progress: while the Placed list is on screen (`ghosts::wantProgress()`),
  other placements are counted in the background, 8,192 cells a frame,
  with the same cell classification the check uses (`classifyCell`); the
  selected placement's numbers come from its check. Rows show "-" until a
  pass has finished.
- Files: `ui/SchematicFiles.h` (tested) builds the heading rows; size and
  block count fill in one file per frame for files of at most 2 MB that are
  not loaded yet; bigger ones show "-".
- Check: the filters replace the list's column headings, each with its count
  from the shown-layer tally; the filter row in the right pane is gone. The
  right pane lists every differing state with the target card's names and
  the drawn arrow (the mismatch now carries the expected block's id for
  names that depend on it).
- Materials: blocks first, then entities, with headings only when both
  exist; the right pane has "only the shown layers" and "show on the
  schematic HUD", the selected material's amount, the missing materials as
  slots (one slot per stack, as many as fit) and "Open in
  ResourceCalculator" with what is missing (`MaterialAmount.h`, tested; the
  id corrections for the site are unconfirmed). Hovering the counts shows
  the amount in chests of 27 stacks. The "How it counts" text is gone.
- Entities: solid light-blue box with faint faces (both windings, the face
  material culls); every missing entity within the draw distance is named.

## Check tab review (decided 2026-10-08)

Against [demos/schematic-check.html](demos/schematic-check.html):
- Layout A: the 3D preview sits at a fixed size at the top of the right
  pane; the counts and the selected mistake's details go below it (they
  scroll when long), so the preview no longer resizes with the text.
- The preview follows the kind chips above the list (only the filtered
  kinds are marked).
- How mistakes are drawn is open: the translucent looks were doubtful in
  the world ghosts. The selected mistake must be findable even underground
  or inside a build: candidates are cutting the view at its layer, drawing
  it through other blocks, or a beam like the nearest-mistake marker.
- Wanted besides the mockup: what was confused with what (expected vs the
  block in the world) readable at the selected mistake; layers in the
  preview ("up to layer N" along any axis), tried from several angles.
- Wheel zoom in both previews (the draw order depends only on the view
  direction, so zoom and pan do not disturb it).
- Preview scope for 0.1.8 (decided 2026-10-08): "view" (turn, zoom, move,
  back to the whole, cut at a layer) and "inspect" (click a block in the
  preview for its name, states and check result; in the Check tab it picks
  the row and leads to "show in world"). Editing blocks in the preview is a
  separate, later feature. None of the three layer-control variants of
  schematic-preview-layers.html felt right; proposed instead: Shift+wheel
  peels layers along the axis the view looks down, a small "layer n/m"
  corner label resets it, the Check tab may cut at the selected mistake's
  layer, right or middle drag moves the view.
- Release plan (maintainer, 2026-10-08, also in their notes): 0.1.8
  finishes Schematics: the current screen work, then improving the
  schematic rendering with LHolo as a reference (GPL-3.0: behavior and the
  maintainer's notes only; its source is not opened while writing Lamium
  code, see PROVENANCE.md), then small features and polish. Parts that
  depend on how blocks are drawn (the mistake look, see-through emphasis)
  wait for that rendering work.

## Rendering and compatibility plan for 0.1.8 (proposed 2026-10-09)

Decided by the maintainer 2026-10-09: A is Ready; liquids are drawn in the
screen previews too; the check stays strict (D); the internal approach is
left to the agent. Nothing below is built yet. Written from the
reference project's public README, changelog and development notes (its
source was not opened) and Lamium's code at `450d746`. The comparison itself
is in the maintainer's notes. Order follows the maintainer's 2026-10-08
priority: file compatibility, then ghost drawing; placing assistance and
Java files later.

A. File compatibility (Ready, chosen 2026-10-09):
- Saved files say `format_version` 1 (`Structure.h` default) but write
  `block_indices` as `List<IntArray>`, the shape that goes with version 2;
  the maintainer's vanilla export says 2. Write 2. A large area save
  failed to load in another tool at the vanilla structure loader; confirm
  the cause with a copy changed to 2 before calling it fixed.
- Headers read 2026-10-09 (files in the instance's schematics folder):
  vanilla exports `mixture` (4x2x4) and `broken_village_house` (10x5x8)
  have version 2 and one IntArray layer; every Lamium save (`schematic`,
  `terrain big` 122x39x203, `terrain big 2` 276x14x232, `desert village`
  122x69x112) has version 1 and two layers, the second all void when
  nothing is waterlogged. So also write the second layer only when a cell
  holds a liquid. How vanilla 26.51 writes a waterlogged block (a second
  layer, or something else) is not known yet: the maintainer exports a
  small structure with waterlogged stairs from a structure block for the
  agent to read. Read 2026-10-09: the maintainer's vanilla export
  `water_and_lava` (7x2x10, copied into the schematics folder) has version
  2 and one layer; water and lava are ordinary palette entries in that
  layer (`minecraft:water` with `liquid_depth` 0-7 for the flow, lava 0, 2,
  4, 6 here), next to `minecraft:air`. So plain liquids need no second
  layer; only waterlogged blocks are still unknown (that file has none).
  Read 2026-10-09: `submerged` (7x2x10, vanilla structure block on the
  maintainer's LeviLamina + Lamium instance, copied into the schematics
  folder) settles it: version 2 with **two** IntArray layers when
  something is waterlogged. The second layer is -1 everywhere except
  waterlogged cells, which hold the palette index of
  `minecraft:water` `liquid_depth` 0 (the same entry plain water uses in
  the first layer). Waterlogged there: fence, fence gate, stairs,
  trapdoor, iron bars, sign, copper golem statue, ladder, spawner, chest,
  scaffolding, sea pickle, seagrass, rail, slab, glass pane; infested
  stone is not. One first-layer air cell also has water in the second
  layer, and the palette lists a jungle door that no cell uses (the door's
  upper half was outside the area). So: write version 2; write the second
  layer only when some cell has a liquid in it, with -1 elsewhere (what
  `SaveArea` already fills); a palette entry no cell uses is valid. The
  structure block's own preview did not draw the water around these
  blocks. All three large saves are wider than the structure
  block's 64-block limit in X and Z, so also test a small Lamium save
  (`schematic`, 3x2x3) with version 2 to separate a version problem from a
  size limit.
- After a save, hand the written NBT to the game's own loader
  (`StructureTemplate::load(CompoundTag const&)`) and report failure in the
  save prompt; keep the file either way. Check whether the loader has a
  size limit for very large areas, apart from the version.
- Reading already accepts one or two layers and an empty second layer.
  Add: reject a layer whose length is not the volume (already) and a
  second layer that names palette entries out of range, with a message.
- Built 2026-10-09 (`d37bd6d`) and checked in game the same day (VALIDATION-LOG):
  `writeStructure` always writes
  version 2 (`writtenFormatVersion`; a version 1 file read in is written
  back as 2) and the second layer only when some cell in it is not -1.
  Layer errors name the layer ("the second block layer has N cells",
  "... names palette entry N, the palette has M"); water over an air cell
  and unused palette entries read as valid. After writing, the save hands
  the same bytes to `StructureTemplate::load` on a throwaway template
  (the level's unknown-block registry); a rejection keeps the file and the
  prompt says "Saved ..., but the game's structure loader rejected it"
  (`schematic.save.gameRejected`); the log line names the size, layer count
  and result. Tests: `structureWrittenShape`, the layer rejections, and
  `LAMIUM_SAMPLE_STRUCTURES` (a folder or `;`-separated files, e.g. the
  vanilla `submerged` and `water_and_lava`) reads each file, writes it back
  and checks both layers, the version and the layer count. Seen in game:
  saves of 3x2x3, 7x2x10 (waterlogged) and 66x53x65 were accepted by the
  game's loader, placed correctly by the structure block (the first two)
  and read by the reference tool. A version 2 copy of the 122x39x203
  save that failed there before (only the version and the empty second
  layer changed) loaded in it and in the game's structure block, while
  both version 1 originals still failed there: the version 1 header was
  the cause, and the game's loader takes 122x39x203. A is done; files
  saved before `d37bd6d` stay version 1 until saved again.

B. Ghost drawing (Research, strong model; one runtime round per step):
1. Schematic neighbors: `BlockTessellator` reads neighbors through
   `mRegion` (`setRegion(BlockSource&)`) and `BlockSource::getBlock` is
   virtual. Spike: tessellate one door and one fence against a view that
   answers from the placement (cells outside fall back to the real world
   or air). If a subclass cannot be built safely, the fallback is a
   thread-scoped hook on the region read, active only while Lamium
   tessellates. Hypothesis to test: doors and beds draw half or nothing
   because the partner half is not where the tessellator looks; fences
   and panes take their connections from neighbors; walls also store
   connection states, so the verifier must compare them after the same
   recomputation (or ignore them).
   Spike built 2026-10-09 (not yet checked in game): `SchematicRegion`
   derives from `BlockSource` (built with the exported constructor on the
   world's level, dimension and chunk source, not public) and overrides
   both `getBlock` overloads and `getMaterial`; links and loads with the
   SDK's prelink. Ghosts: the private tessellator reads through it; a cell
   answers the placement's (rotated/mirrored) block where a ghost is drawn
   (shown layer, real cell air), else the world. Opaque full ghosts that
   must not hide a neighbor's face (no mesh, or the cell or the drawn cell
   next to the camera) read as the world, so the camera rule in
   `cullAgainstGhosts` still decides those faces. Ghost-to-ghost face
   culling now also happens in the tessellator (glass next to glass may
   lose the shared face). Files/Check previews: the view answers the
   file's block at the spot above the build limit where each cell is
   tessellated. Liquids (second layer) are untouched (B3). If the shapes
   stay wrong in game, the tessellator or the block code reads neighbors
   without the virtual call; then try the thread-scoped hook.
   Checked 2026-10-09 (VALIDATION-LOG): doors and beds draw both halves;
   no regressions. 26.51 files store fence/pane connections
   (`minecraft:connection_*`) and stair corners (`minecraft:corner`) as
   block states, so those shapes come from the file's states turned by
   `transformBlock`, not from neighbors. That check also showed the mirror
   axes swapped: the game's `Mirror::Z` flips east-west states, so
   Lamium's X maps to it and Z to `Mirror::X` (fixed after `0486186`).
   Checked on `54c6a8a`: X, Z and mirror plus 90 degrees match a structure
   block for blocks. Open: a door or bed whose partner lies outside the
   file draws nothing; under the X mirror an armor stand facing south stays
   south in Lamium (`toWorldYaw`, a true reflection) but the structure block
   turns it north (maintainer to choose which to follow).
   Decided 2026-10-09 (maintainer): follow the structure block for entity
   facing, and supply a missing other half. Built (not yet checked):
   `toWorldYaw` mirrors on the other axis (a true mirror turned half
   round; X: 180 - yaw, Z: -yaw), on the assumption that the game makes
   the same axis swap for entities as for block states (one sample, yaw 0
   under X). `otherHalf` (`Structure.h`) gives a two-block-tall block's
   other half (`upper_block_bit` flipped, one step up or down); while a
   ghost or a preview cell with one is tessellated, that cell answers the
   other half unless the real half is placed there, so a door at the
   area's edge or under water draws its own half. Only the drawn half is
   shown; nothing is drawn outside the placement. Beds are drawn per half
   by the block-entity renderer against the real world and are unchanged.
   Checked on `a32a49f` (VALIDATION-LOG): both changes pass; the armor
   stand matched under X, Z and mirror plus 90 degrees, confirming the
   axis-swap assumption for entity facing. B1 is done; left unchecked: a
   bed with one half outside the file, redstone (no sample).
2. Render layers: build each section's ghosts into separate meshes by
   the block's render layer (opaque, alpha-test, blended) and draw the
   blended ones last, sorted back to front by section. This is the base
   for the translucent look, honey and slime blocks, and real translucent
   blocks behind ghosts.
   Built 2026-10-09 (not yet checked in game): each ghost is tessellated
   once per render layer it draws in (`BlockType::getRenderLayer`, then
   each bit of `getExtraRenderLayers` as a further layer), with the
   tessellator's `mRenderingLayer` set for that call and restored. Blend
   and blend-to-opaque layers go into a second mesh per section (`blend`),
   its quads sorted far to near from the camera at build time; the other
   layers stay in the alpha-tested mesh as before. Blended sections are
   drawn after everything else, far to near, with the moving-block
   renderer's blend material, fully bright and tinted like the rest.
   `coversNeighbors` counts only non-blended layers, so a honey block no
   longer hides its neighbors' faces. Liquids keep the single default pass
   (B3). The Files/Check previews tessellate every layer into their one
   mesh. The log names the tessellator's default layer and each palette
   entry with extra layers, to read if honey or slime still draw wrong.
   Expected limits: quads inside one blended section are ordered for the
   camera at the build (far sections are rebuilt rarely), and real
   translucent blocks behind ghosts may still vanish where ghosts write
   depth.
   Checked 2026-10-09 (VALIDATION-LOG, final `59fc02c`): honey draws its
   real look; no regressions. Follow-ups made in that round: the
   tessellator's current shape is cleared around each call; ghost faces
   against real opaque blocks are dropped unless the block is blended;
   `coversNeighbors` needs the unblended mesh to reach all six sides
   (`sidesReached`); mistake marks are white concrete boxes, recolored and
   pushed 0.01 out, inside the sorted blended mesh (a separate translucent
   draw swapped order with blended ghosts each frame), except over a real
   blended block, where they keep the holo material without depth writes
   so that block stays visible; a mark leaves out its face against a ghost
   that fills that side (`Resolved::boxed`). Known limit: that last kind of
   mark still swaps order with a blended ghost beside it. Removing it needs
   a blended material without depth writes, the same research as real
   translucent blocks behind ghosts. B2 done.
3. Liquids: water and lava from the second layer and from water/lava
   cells drawn by Lamium as simple shells textured from the terrain atlas
   (shared faces between same liquids dropped, faces against opaque
   blocks dropped), blended; only where the liquid is missing. The
   Files and Check previews draw the file's liquids as well (decided
   2026-10-09).
   Built 2026-10-09 (not yet checked in game): `liquidShell` tessellates a
   white concrete cube at the cell (so every vertex stream is filled and
   faces against opaque blocks are already culled), then leaves out faces
   toward the same liquid (`liquidAt`: the file's either layer in shown
   layers, or the world's block or extra block), lowers the top to 0.875
   unless the same liquid is above, maps the faces' UVs onto the liquid's
   own `BlockGraphics` textures (slot 0 bottom, 1 top, 2 sides) and colors
   water blue at alpha 0.55, lava white at 0.75. Shells go into the sorted
   blended mesh. World: a first-layer liquid cell where the world is air,
   and a second-layer liquid where neither the world's block nor its extra
   block is that liquid (also under a ghost or a real block). Liquid cells
   get no outline. Previews: liquid cells and waterlogged cells get the
   same shell against the file's neighbors. The log names the water
   texture's UV rectangle once, to read if the texture comes out wrong.
   First check 2026-10-09 (`0f164ee`): water and lava draw textured and
   whole, waterlogged ghosts show their water, no shell in real water; the
   preview drew liquids but hid waterlogged blocks and the floor under
   water, and no flow was shaped. Then: each top corner is set by
   `liquids::corner` (`LiquidShape.h`, tested): the four cells around it,
   the same liquid above any of them making it full, sources weighing ten
   times flowing cells (surface `1 - (depth + 1) / 9`, falling as a
   source), open cells pulling it down, solid ones ignored. In the
   preview a cell's liquid quads sort after its block's, so a waterlogged
   block shows. Known preview limit: its one pass keeps the first
   fragment at a spot, so water is opaque there and hides what lies under
   it (a floor); translucency there needs another depth approach.
   Checked 2026-10-09 (VALIDATION-LOG): shapes and slopes match the real
   liquids; sloped tops (sources beside a flow too) take the side slot's
   flowing texture turned downhill, still pools the still texture. The
   flowing texture's atlas cell is one block's texture (16x16): sampling
   its middle half looked 8x8 and fast, so `96fe71f` maps it whole
   (scaled down only for diagonal flow). B3 done apart from the preview
   limit, deferred (maintainer, 2026-10-09) into one research item with
   B2's leftover: a translucent path that does not hide what is behind it
   (a blended material without depth writes in the world pass; real depth
   or another order in the preview pass).
4. Block entities with their data: load the schematic's block entity
   NBT into the created block actor before drawing (bed color and part,
   skull type and rotation, sign text, banner pattern).
5. Lighting regressions to keep in the check list: ghosts the same at
   night, underground and looking straight down; Vibrant Visuals uses its
   own material path or is named as unsupported.

C. Updates (after B1-B2, measure first, L-105):
- Rebuild on block change events (a section and its six neighbors) instead
  of the 0.25 s / 2 s hash timers; fixes the known border-cell limit.
  Moving a placement should move its meshes, not rebuild them.
- A background mesh worker only if measurement shows tessellation is the
  cost; results carry a world/dimension/placement generation and are
  dropped when stale.

D. Check accuracy (pure rules with tests, small):
- Strict (maintainer, 2026-10-09): every differing state stays a
  mistake, including states the game changes on its own (growth, leaf
  decay bits); no ignore table.
- Waterlogging: compare the second layer with the world's extra block.
- One material identity for the material list, the check and later
  placing assistance (already one pick-item rule; keep it that way).

Later (not 0.1.8 unless chosen): placing assistance (looked-at missing
block, area fill), Java `.litematic` files, layers by material.

## Technical entry points

- `src/features/schematic/Nbt.*`, `Structure.*`: NBT and structure read/write.
- `Placement.h`: transforms/layers; `Verify.h`, `Verification.h`: pure rules
  and progressive checking/material publication.
- `GhostRenderer.cpp`: section caches, culling, private tessellation and
  shared-face removal. Neighbor-dependent meshes read schematic neighbors
  through `SchematicRegion` (B1 spike, unchecked).
- `MenuModel.h`, `src/ui/RadialLayout.h`: menu operations and geometry.
- `tests/SchematicTests.cpp`: pure logic; `LAMIUM_SAMPLE_STRUCTURES` optionally
  supplies real exports.

These source paths are under `src/features/schematic/` unless otherwise stated.

## Decision and implementation record (L-93)

Moved from BACKLOG without dropping decisions, experiments or build notes.
Earlier "awaiting check", "before 0.1.7" and "not yet built" statements are
historical. Later decisions and the current L-item/validation take precedence.
Search for the decision, commit or experiment needed.

Scope for 0.1.7 (maintainer, 2026-10-07), in this order:
1. Entities from files: named dashed frames, verified by type near the
   spot, their own section in the material list, the per-placement switch
   (decided below; the saved `entities` flag already exists).
2. Area selection and save (decided below): corner keys on the looked-at
   block, the frame, number adjustment and name on the screen, "Include
   entities" (default off).
3. "Open folder" on the Files tab, and a warning before loading a very large
   file.
4. Blocks without a mesh on the ghost path that the block-entity renderer
   does not draw either (torch, bed, skull, door ...): at least the outline
   alone, so no block of a schematic is invisible.
Built 2026-10-07; items 1-3 checked in broad terms on `14c2265`, with the
follow-ups below changed in `cd21ee4` (not yet checked):
- Corner 1 green, corner 2 yellow (world and prompt). Corner 2 no longer
  opens the prompt; only the save key does. The area stays after saving
  (until "Clear area" in the prompt or leaving the world). Entity names
  are sized like text a quarter block tall over the frame, shrinking with
  distance and hidden when too small to read.
Changed after the second check, in `1c5a676` (not yet checked):
- Corner 1 outlined red and corner 2 blue on the block's own edges, with
  faintly tinted faces just outside, so a full block shows its corner.
- Large areas: the save reads the area chunk column by chunk column; a
  column whose chunk is not loaded waits until the player comes near (a
  toast every 8 s names the nearest waiting spot). The prompt shows the
  progress and its Save button becomes "Stop saving" while a save runs.
- The save prompt is wider (340), with a status row; "Clear area" sits at
  the left of the button row.
- Text fields (save and waypoint prompts, shape and waypoint names) show a
  blinking caret after the text, or the selection as a highlight, instead
  of "_" or "[...]". Number fields keep their old look.
- Entity names are drawn in the world pass as name tags: a dark plate with
  the text, facing the camera, 0.025 blocks per font pixel, with the
  game's name tag materials (the both-sides variants). Adding them to the
  game's own name tag list is not possible from a mod: that list's
  allocator is not exported.
- Blocks with neither a world mesh nor a block entity (honey block, door)
  fall back to the block's shape mesh set on the cell floor; it ignores
  block states (a door shows its default shape). Skulls stay an outline:
  their model needs their block entity data (research).
As first built:
- 1 (`1a82afe`): missing entities get a dashed frame (one size, 0.8 x 1.8;
  the client cannot know a type's size without the entity) and their name
  above it; an entity counts when one of the same type stands within one
  block of its spot. Entities are judged only within 48 blocks of the
  player and in loaded chunks (beyond that the client does not know them),
  so they are neither placed nor missing there. Check lists missing ones as
  "not placed (entity)"; Materials lists them after the blocks, carried only
  when an item of the same name exists (armor stand), "-" otherwise. The
  HUD's materials can include them.
- 2 (`14c2265`): keys "corner 1" / "corner 2" on the looked-at block
  (unbound, new "Save an area" key group); the second corner, or the save
  key, opens a save prompt over the world with both corners as − / +
  steppers (Shift: 10), the size, the name, the file it becomes and
  "Include entities". The world shows the area as a white frame with yellow
  corners, following the steppers. The world render reads 32768 cells per
  frame and writes the file; a chunk not loaded stops the save with its
  position. Blocks with their states and the water layer are saved; block
  entity data (container contents, sign text) is not; entities keep type,
  position and facing. An existing file needs a second press
  ("Overwrite").
- 3 (`14c2265`): "Open folder" replaces "Reload files"; the Files list
  rescans every two seconds while shown. Files over 2 MB (about a quarter
  million blocks; not measured) show a warning and a "Load" button first.
- 4: already in place: a block that gives no mesh and no block-entity
  model still gets the light-blue outline of its cell (to be checked with
  torch, bed, skull and door).
Paused 2026-10-07 for a design conversation (maintainer). Open points from
the third check (VALIDATION-LOG): how schematic keys work overall (too many
single-purpose keys; the selection and corners should share the placement
keys; a key to clear the selection), drawing cost of large schematics
(culling, not rebuilding every placement on any change), name tag
visibility (walls, distance, many entities), showing unloaded parts of a
running save, and the small fixes (prompt line spacing, caret height,
honey block and door fallback, the large-file warning not seen).
Controls decided 2026-10-07 (mockup [demos/schematic-controls.html](demos/schematic-controls.html),
second round):
- A schematic menu on one key: a two-level radial menu (categories, then
  their items) drawn by Lamium. Clicking an item runs it; the wheel over a
  stepper item changes its value; right click goes back one level; the
  menu key or Esc closes it. The game keeps running while it is open; the
  menu only takes the mouse, like Lamium's screens.
- It reaches most of what the screen does for placements and the save
  area. Categories (to be finalized with the mockup): Move (forward/back,
  left/right, up/down relative to the view, to feet; acts on the "move
  target"), Turn (rotate, mirror, reset), Layers (axis, mode, layer, the
  layer you stand in, show all), Show (this placement, extra blocks,
  entities, Schematic HUD, the feature), Area (corner 1/2 at the looked-at
  block, "move corner 1 / corner 2 / the whole area ->", save, clear),
  Placement (selected placement, "move the placement ->", look-at select,
  place from a file, delete via the screen), Check (nearest mistake only,
  as decided 2026-10-03; Check and Materials tabs), Screen (tabs, key
  settings). Names, file picking and delete confirmation stay on the
  screen. Absolute X/Y/Z are not in the menu (Y duplicated up/down).
- The move target is one of: selected placement, corner 1, corner 2, the
  whole area. It is chosen by the "... ->" items, which open Move; Move
  shows the target in its color (green, red, blue, white) in its center.
- An adjust key (recommended): held with the wheel, it repeats the stepper
  item used last in the menu (for example up/down or the layer), with a
  hint under the crosshair saying what it repeats. No tap/hold
  distinction anywhere.
- The single-purpose keys stay as advanced shortcuts, unbound by default.
- A held stick (or any item) as a tool is not built; only if users ask.
Built 2026-10-07 while the maintainer cannot test (none of it checked in
game yet):
- `7fe5324`: name tags hidden behind blocks (a ray from the camera); honey
  block and door back to the outline alone; prompt line spacing, caret
  margins, the large-file warning above its button.
- `8a09567` drawing: a change keeps the built sections of placements whose
  `drawKey` (PlacementStore) is unchanged, and the check keeps running
  unless its own placement changed; a section is rebuilt only when a hash
  of its world blocks changed (compared on the old 0.25 s / 2 s timers, at
  most 8 sections per frame); sections outside the view are not drawn and
  are built after those in view; a ghost with an opaque full block (real,
  or an opaque ghost that will be drawn) on all six sides is not
  tessellated. Known limit: a change in a neighboring section does not
  rebuild an enclosed cell at the border until its own section changes.
- `b80f4dc` menu and adjust key: as decided above. Pure parts:
  `MenuModel.h` (categories, items, targets, where it opens) and
  `RadialLayout.h` (ring geometry and pointer hit), with tests. The menu is
  a mode of Lamium's screen like the prompts; the adjust key is a Hold
  action whose wheel turns are counted on the input thread and applied by
  the HUD frame. Settings rows: "Schematic menu (start here)" with the
  three menu options right under Schematics, "Repeat the last adjustment
  (recommended)", and the single-purpose groups renamed "Shortcuts: ...".
Changed after the first look (`4e413ed`, 2026-10-07, not yet checked):
the menu drawn with Lamium's panel, selection frame and labels, its center
fixed on every level and the ring fitted to narrow screens; "Move target"
in Move; the adjust key reports through toasts (no crosshair hint); a
layer direction turned to the opposite side keeps the same layer
(`withAxis`); skipping enclosed ghosts became the option "Lighter drawing
for large schematics" (off), suggested by a toast when a file over 256k
cells is placed; a waiting save names a direction and distance and frames
the columns it waits for in yellow; key labels without "start here" /
"recommended" and the footer tip in faint text.
Second look (`f6f5386`, 2026-10-07, not yet checked): each tessellated
ghost drops its quads on a side touching an opaque ghost (`GhostFaces.h`;
collapsed to a point so no vertex data moves), which also ends the
z-fighting between adjacent ghosts; the camera's eye and feet cells are
not drawn and count as open, so from inside a schematic the ghosts around
form walls, and the sections around them rebuild when the camera changes
cell; enclosed ghosts are skipped for everyone again (the option is
gone). The ring is sized from the item and center sizes so no item meets
its neighbor or the center at any count (`RadialLayout`, tested); small
mode scales the whole menu; hints are one per line.
Third look (`efcbda7`, 2026-10-07, not yet checked): the open camera cells
are gone; ghosts within one cell of the camera's eye and feet cells keep
every face and are never skipped as enclosed (so a border position or the
legs show blocks); the menu's items spread out from 55% of the ring over
0.15 s with ease-out when a level opens, their text appearing once the
plates are mostly in (`RadialLayout::spread`, tested); the key groups are
named "Placement keys", "Shown layers", "Check", "Save an area" again.
Fourth look: near the camera each pair of touching ghost faces kept only
the one facing the camera; the menu opening follows the Animations setting
(confirmed). Fifth look (`60b5fab`, not yet checked): that left hollows
where the near clip plane cut the kept face, so ghosts within one cell of
the camera now keep every face, each shrunk by 0.4% toward its cell center
so touching faces never share a plane. Also the settings sidebar fits short
windows (`SettingsTable::navStep`, tested), found at UI Profile 100%
(confirmed). Sixth look: the inset still z-fought east-west; now every
pair of touching faces where either cell is within one of the camera's
cells keeps only the camera-facing face (`faces::beyond`, tested), ghosts
within two cells are never skipped, no inset (not yet checked).
After 0.1.7 (maintainer, 2026-10-07; not blockers, known issues): placements
on the minimap and world map; a more detailed target-card line (the
expected block's icon, and for a wrong state which states differ); beds
sometimes drawing only one half, and a bed cell showing the outline alone;
the Verify tab's colored preview; the rotatable 3D preview in Files; shapes
that depend on neighbors following the schematic; the translucent look;
drawing heads, doors and honey blocks; how far and how many entity name
tags show. (The menu animation was moved before 0.1.7 the same day.)
To check before 0.1.7: see [Pre-release checks](BACKLOG.md#pre-release-checks).
- Placements on the maps (built 2026-10-07; look chosen by the agent at the
  maintainer's request, checked in game 2026-10-08): while Schematics are on, each
  placement in the viewed dimension shows its footprint from above as a cyan
  outline (the map palette's cyan) with a black edge and a faint fill, under
  the waypoints; the selected one is outlined white. The minimap shows
  visible placements only, turning with a heading-up map and clipped to a
  round one; the world map also shows hidden ones faintly with "(hidden)"
  and the name under the footprint. A footprint is never drawn smaller than
  a few pixels. No click action on the map yet.
- Richer target-card line (built 2026-10-08, not yet checked): the
  "Schematic: <block> (<kind>)" row shows whether or not the card's other
  details are on, with the expected block's item icon before the text. For a
  wrong state, up to three rows follow naming each differing state as
  "<state>: <expected> (now <actual>)", in key order, with raw values
  (`stateDifferences`, tested). Checked in game 2026-10-08, then redesigned
  the same day (`SchematicTarget.h`, not yet checked): rows say what to do
  in the verifier's colors — "Should be [icon] <block>" (red) for a wrong
  block, "Should be Air" (red) for an extra one, "Place [icon] <block>"
  (light blue) for a missing one, and per differing state
  "<state>: <now> → <should be>" (yellow) with readable names (L-112); the
  card's own row for that state is left out so it is not listed twice.
- Menu settings (decided 2026-10-07): background dimming light by default
  (none and dark selectable); shown centered at full size by default, or
  as an option small in the lower right so the view stays free; it opens
  at the category list by default, or as an option exactly where it was
  closed (the level shown at closing; the list if the player had gone back
  up with right click).
- All keys stay unbound by default (schematics are experimental). The
  settings lead to the right ones first: the menu key and the adjust key
  sit right under the Schematics switch, marked as where to start and
  recommended; the single-purpose keys move into a collapsed "Shortcuts
  (advanced)" group. While the menu key is unbound, the Schematics
  screen's footer suggests binding it with a link to the key settings;
  while the adjust key is unbound, the menu's center says what it would
  do. (Wording to be settled when built.)
If there is room before 0.1.7 (maintainer's call, not required): the
rotatable 3D preview in Files, neighbor-dependent shapes (fences, panes,
stair corners, redstone) following the schematic's neighbors instead of the
real world's, and the translucent look as an option. Otherwise they become
follow-ups after 0.1.7.
A client-side schematic subsystem for building from a saved structure: pick
a file, place it in the world, see it as ghost blocks, compare it with what
is built, and see which materials are still needed. It ships default off
with the Experimental badge and lands on main in steps
([Release policy](BACKLOG.md#release-policy-decided-2026-09-28)); steps that
are not usable yet stay out of the settings screen.

Decided 2026-10-03:
- Independent implementation. LeviSchematic (the maintainer's fork) stays
  reference-only (PROVENANCE.md group 3): it shows what was feasible
  (`.mcstructure` load/save, ghost projection, transform, world comparison,
  selection) but its code is not incorporated. The maintainer wants to own
  the code for design freedom and licensing. Whoever writes the Lamium code
  works from this spec and does not open LeviSchematic source.
- One design for the whole first scope: schematic browser, placement
  (move, rotate, mirror), ghost projection, layer controls, verifier and
  material list are specified together, so later parts are not bolted on.
  Implementation may still land in steps.
- File format: `.mcstructure` only. Java `.litematic` import is a possible
  later addition, not part of this item.
- Ghost look: translucent real block models are the target, on one
  condition: their brightness must not depend on the world's light level
  (a projection in a dark cave reads as well as one in daylight). If that
  cannot be done, another look is chosen with the maintainer. Research
  first.
- Operation: a dedicated screen plus keys, consistent with the rest of
  Lamium (DESIGN.md "Tools with their own state get a dedicated view").
  No held-item tool (no stick or wand selection); that may be revisited
  only if users ask.
- Later, not in this item: placement guidance, schematic-aware placement
  restriction, hotbar item selection and placement assist; the entity and
  block-entity follow-ups listed below.

Also decided 2026-10-03 (the recommendations, accepted):
- Saving is in scope: select an area in the world without an item and save
  it as `.mcstructure`.
- Files live in `mods/Lamium/schematics/` (subfolders allowed); the browser
  has "open folder".
- Several placements at once, remembered per world (servers by address and
  port) and dimension across sessions.
- Verifier: not placed shows the translucent ghost, correct hides it, wrong
  block is red, wrong state (facing etc.) is yellow.
- Material list: total, placed, remaining and in inventory on the
  dedicated screen. "In inventory" includes the contents of shulker boxes
  carried in the inventory.
- Container contents are ignored; structure void means "place nothing
  here". (Entities were first listed as ignored too; reopened the same day,
  see the entity proposal below.)

Decided 2026-10-03 after the first mockup
([demos/schematic.html](demos/schematic.html)):
- Extra blocks (a block where the schematic has air) show red like wrong
  blocks by default, because they can break redstone machines; each
  placement can switch to ignoring them (no red, not counted), for builds
  where they do not matter.
- Placement keys (move one block toward / away from the look direction,
  to the feet, rotate 90°, mirror, layer up / down, next placement), the
  area-selection flow (corner keys on the looked-at block, frame, adjust
  numbers on the screen, name and save), the Schematic entry pinned at the
  bottom of the sidebar and a "Schematic" settings category are accepted
  as in the mockup. All keys unbound by default.
- The material list lives on the dedicated screen. Its HUD list is
  default off and changes only when the player switches it (setting, key
  or the screen's switch); placing a schematic never turns it on. Large
  builds would not fit the HUD, and the screen is one key away.
- Layers work along any axis: height (from below / from above), east-west
  (from west / east) and north-south (from north / south), each with
  all / this layer only / up to this layer. Default: height from below.
- The file browser shows a rotatable 3D preview of the schematic with
  "up to layer N". It depends on the same research as the ghost look; if
  real block models cannot be drawn there, fall back to a top-down
  layer-by-layer plan like the Shapes editor.

Decided 2026-10-03 (verifier views):
- Seeing what is wrong, in four places (accepted): the target card adds "schematic:
  <expected block> (<kind>)" when the crosshair is on a mismatched block;
  an optional HUD element (default off, switched like the material list)
  shows, for the selected placement and the visible layers, correct/total,
  not placed, wrong, wrong state, extra, and the distance to the nearest
  mistake; a key marks the nearest mistake in the world with its distance
  (pressing again moves to the next); and a "Verify" tab on the screen
  lists mismatches (kind, position, expected → actual, distance; filtered
  by kind, mistakes before not-placed) next to a preview colored by
  verifier state, with "show in world". Counts follow the visible layers.
  The HUD shows the counts and the distance to the nearest mistake.
- No separate placement-screen preview showing rotation and mirror; the
  verifier-colored preview in the Verify tab is the only one besides the
  file browser's.

Decided 2026-10-03 (selection, tabs and HUD; accepted as proposed):
- One selected placement for the whole subsystem: placement keys, the
  Verify tab, the material list, the HUD and the nearest-mistake key all
  act on it. It changes only when a schematic is placed (the new placement
  becomes selected), when one is picked in the placement list, with ◀ ▶ on
  the Verify and material tabs, or with the "select the looked-at
  placement" and "next placement" keys; never by walking near another.
  Its frame is green in the world, others grey. Remembered per world and
  dimension.
- The Verify and material tabs pick the placement with one "◀ name ▶"
  stepper instead of a row of buttons, which would not scale; the
  placement list is the way to choose among many.
- No counts on the screen's tabs ("Placements 2", "Verify 8"); the list
  and details already show them. The sidebar entry keeps its count like
  Shapes and Waypoints.
- One "Schematic" HUD element (default off, switched only by the player)
  with two child switches for its sections: verifier counts and remaining
  materials (remaining / in inventory). It moves as one element in the HUD
  layout editor.
- Wrong blocks and extra blocks share the red color, so they are one group
  everywhere: the Verify tab filters are "mistakes / wrong or extra / wrong
  state / not placed" (the row's kind column still says which), and the
  HUD and the tab summary count "wrong or extra" together.
- Wherever a block or item is named (Verify rows, material list, the HUD
  material lines, the target-card line), its item icon is drawn before the
  name, using Lamium's existing item icon drawing.

Decided 2026-10-03 (entities, first version; accepted as proposed):
- Entities in a `.mcstructure` (armor stands, mobs, ...): shown as a named
  dashed frame rather than a translucent model; verified by type and
  position only (one of that type near the spot), not pose, equipment or
  name; listed in their own section of the material list ("in inventory"
  only where an item places them, e.g. armor stands; "—" for mobs); each
  placement can turn entity display and verification off (default on).
- Saving: the save prompt has "Include entities" (default off). Only what
  the client knows can be saved (type, position, rotation; equipment and
  other data depend on research).
- Later, not in the first version (research items): an option to show
  entities with their real look (not necessarily translucent), or at least
  a frame that shows their facing; verifying armor stand equipment and
  item frame contents. In Bedrock an item frame is a block with a block
  entity, not an entity, so its contents belong with the container-content
  question rather than with entities.

- Size and load (decided 2026-10-03): no hard limit. Building ghost meshes,
  verifying and counting materials run in bounded steps per frame/tick
  (DESIGN.md "Engineering behavior") and fill in progressively; counts show
  "counting" until complete. Loading a very large schematic (on the order
  of hundreds of thousands of blocks) shows a warning first. Exact numbers
  come from measurement.

Implementation (2026-10-03, maintainer's go): game-independent core first,
then rendering, screen and keys.
- Done: `.mcstructure` read/write (`src/features/schematic/Nbt.*`,
  `Structure.*`): little-endian NBT, layers as int arrays (current exports)
  or int lists (older ones), palette with state keys, block entity data by
  cell, entities relative to the corner. Placement and layer math
  (`Placement.h`: mirror then clockwise quarter turns, inverse lookup, six
  layer axes with all/only/up-to) and verifier/material rules
  (`Verify.h`). Covered in `tests/SchematicTests.cpp`; set
  `LAMIUM_SAMPLE_STRUCTURES` to a folder of real exports to parse them too.
- Block states are turned by the game:
  `VanillaBlockStateTransformUtils::transformBlock(block, Rotation, Mirror)`.
  Whether its rotation direction and mirror axes match `Placement.h`
  (clockwise from above; X flips east-west) must be checked in game with
  stairs once ghosts are drawn from a placement.
- First playable step (2026-10-03, awaiting the maintainer's check): the
  "Schematic" switch (default off, Experimental, new "Schematics" settings
  category; keys to toggle and to open the screen, unbound), a Schematics
  screen pinned in the sidebar (placements above the files of
  mods/Lamium/schematics; place a file at your feet; edit position,
  rotation, mirror, visibility, layers, extra blocks; delete), placements
  saved per world beside the waypoints (`schematics.json`), and ghosts
  drawn per 16-block section (`GhostRenderer.cpp`): tinted ghosts where a
  block is missing, red outlines for wrong or extra blocks, yellow for a
  wrong state, orange for unknown block names, block-entity models with an
  outline. Sections rebuild two per frame and refresh every two seconds.
  Not yet: Verify and material tabs, HUD, placement keys, the translucent
  option, entities, saving areas.
- Verify and materials (2026-10-03, awaiting the maintainer's check): the
  screen has four tabs (Placed, Files, Check, Materials). The world render
  scans the selected placement 16384 cells per frame and publishes a
  finished pass (`Verification.h`): counts in the shown layers, up to 2000
  mismatches (mistakes before missing blocks, nearest first) with item
  icons, and material lines by item (the block's pick item; double slabs
  count two, upper door/bed halves none). Check filters mistakes / wrong or
  extra / wrong state / not placed; "Show in world" closes the screen and
  marks the cell with a white box and a beam for 30 s. Materials show
  need / placed / left / carried (inventory plus shulker box contents;
  green when enough, yellow when short), all layers or shown layers only.
- HUD, keys and target card (2026-10-03, awaiting the maintainer's check):
  a "Schematic HUD" element (default off; switch, key and the HUD layout
  editor; child switches for check counts and materials left) shows the
  selected placement's counts in the shown layers, the distance to the
  nearest mistake and up to five materials left with icons (yellow when
  short). Keys, all unbound: nearest mistake (again: the next), select
  the looked-at placement, next placement, move one block along the view
  (steep views move up or down) or back, move to feet, turn 90° right,
  mirror, shown layer up/down; each confirms with a toast. The target card
  adds "Schematic: <block> (<kind>)" when the crosshair block is a
  mistake. The Placed tab has "Layer here: match where I stand".
- Settings regrouped after the first look (2026-10-03): the Schematics
  category has five rows: Schematics (switch, key, open screen), Schematic
  HUD (switch, key, "show check counts", "show materials left", layout
  link), and keyless groups Placement keys (select looked-at, next,
  away/closer/left/right/up/down, to feet, turn, mirror), Shown layers
  (up, down, layer here) and Check (nearest mistake, always the nearest).
  Action rows explain themselves through "help.key.<id>". The HUD is a
  fixed-width card: name and layer, a two-by-two grid of marked counts,
  nearest mistake, then up to five materials with right-aligned left/have.
  "Select the placement you look at" casts the view ray against placement
  boxes.

Research (2026-10-03, in progress):
- Ghost look. Candidates, compared in game with `xmake f --ghost_probe=y`
  (`src/features/schematic/GhostProbe.cpp`; F7 anchors four rows of test
  blocks three blocks ahead, F6 toggles ignoreLighting for rows B-D):
  A `BlockTessellator::renderGuiBlock` with alpha 0.5 and light 1 (the GUI
  block path); B a private `BlockTessellator` appending the block, drawn
  with the `moving_block_blend` material, the moving-block renderer's
  terrain atlas and `ActorShaderManager::setupShaderParameters` with
  ignoreLighting; C the same with in-world tessellation at the real
  position (shapes from real neighbors); D like B with the moving-block
  renderer's own blend material. A private tessellator keeps the alpha
  color override out of vanilla's block mesh caches. To check: which rows
  draw, whether they are translucent, whether brightness stays the same in
  daylight, at night and in a dark cave, stairs facing, grass tint, and
  that held/dropped blocks still look normal afterwards.
  Round 1-2 (maintainer, 2026-10-03, 1.26.51.01 with LeviSchematic, LHolo
  and ChiyanMap also installed): the in-world path drew translucent blocks
  whose brightness did not change at night, with parts missing, then
  crashed (null read inside `tessellateBlockInWorld` with a private
  tessellator); the appended mesh with `moving_block_blend` drew nothing;
  with the renderer's blend material it drew opaque and off the block grid;
  the GUI path ignored the alpha and was off the grid too. Round 3 shifts
  the appended mesh to the block corner, fills its missing light UVs and
  adds the fallback look.
  Round 3: on the block grid now, brightness unchanged at night, nothing
  missing, but opaque with both materials (the named one drew nothing);
  torch and chest give no vertices on this path; the outline was a full
  block regardless of shape; in daylight the fallback is hard to tell from
  real blocks. The log showed the appended mesh has no vertex colors and
  no light UVs at all, and the color override is ignored on this path.
  Round 4 writes vertex colors (alpha 0.5, or a light-blue tint for the
  fallback), outlines the mesh bounds, and tests stair states 0-4.
  Round 4: the named `moving_block_blend` still drew nothing. With the
  moving-block renderer's blend material the blocks were translucent and
  kept their brightness day and night, but whole blocks turned darker or
  lighter while jumping or turning (likely draw order against other
  translucent geometry such as water, since depth is written). The tinted
  fallback was stable, readable as schematic blocks in daylight, and its
  outline followed the shape. The item-shape mesh ignores block states
  (all five stairs faced the same way), so real schematics need the
  in-world mesh. Round 5 primes a private tessellator and retries the
  in-world mesh, translucent and outlined.
  Round 5: no crash after priming. `tessellateBlockInWorld` drew every
  block as a plain cube (it is the cube path; the shape dispatcher is
  `tessellateInWorld`). The in-world mesh carries light UVs and AO colors.
  Translucent blocks flickered even with the view still; with the opaque
  tint, glass flickered where it overlapped other ghost blocks. Round 6
  uses `tessellateInWorld`, scales ghosts slightly toward the eye like
  shapes, and logs how often the render pass runs.
  Round 6: stairs face the right ways and the fence takes its
  unconnected shape (neighbors come from the real world). The pass runs
  once a frame (about 300 calls in 5 s at 60 fps), yet translucent cubes
  and glass in front of planks still flickered with the view still: the
  engine evidently reorders separate draw calls between frames. Round 7
  tessellates all ghost blocks into one mesh and sorts its quads far to
  near before one draw.
  Round 7 (`3c29f5d`): no flicker in either look, moving or still; the
  2x2x2 cube and planks behind ghost glass are stable; stairs face the
  right ways. Remaining defect: real translucent blocks (glass) behind a
  ghost block disappear, in both looks. The ghost mesh writes depth in the
  entity-effects pass, which runs before the world's translucent layer.
- Ghost path found (2026-10-03): a private `BlockTessellator` (primed with
  one appended block per frame), `tessellateInWorld` per block into one
  shared `Tessellator`, vertex colors rewritten (alpha 0.5, or a light-blue
  tint), quads sorted far to near, and one draw with the moving-block
  renderer's blend material and terrain atlas after
  `ActorShaderManager::setupShaderParameters(..., ignoreLighting = true,
  ...)`. Brightness stays the same in daylight and at night. Both looks
  work: translucent, and tinted with a light-blue outline that follows the
  shape.
- Round 8 (`52cd792`, block coverage; both looks behave the same): a mesh
  for stone, glass, stairs, torch, lantern, redstone wire, poppy, lever,
  ladder (invisible from behind), rail, glass pane, slab, trapdoor, leaves
  (no biome tint: white), flower pot, campfire, bell, piston (no head) and
  end portal frame. Water drew a missing-texture block. No mesh: door,
  chest, ender chest, bed, sign, skull, shulker box (block entity
  renderers, and the door reads its other half from the world).
  `minecraft:white_banner` is not a block name. In cut-out blocks (poppy,
  redstone, campfire, glass) the empty texels hid what is drawn later
  (water), like real glass behind ghosts. Drawing in the cracks or name tag
  passes instead (each runs once a frame) changed nothing. Round 9 tries
  the alpha-test block material for the outlined look and an unlit
  blended material without depth writes for the translucent look.
- Round 9 (`7694edc`): the outlined look drawn with the moving-block
  renderer's alpha-test material is stable, and gaps in cut-out blocks
  (poppy, redstone, campfire, ghost glass) show water, ground and real
  glass behind them correctly. This is the outlined look's material. The
  unlit blended material without depth writes was very faint and varied
  with what lay behind (dense over terrain, faint over water), so the
  translucent option stays on the blend material with its limit (real
  translucent blocks behind a ghost disappear). Ladders show from both
  sides. Round 10 draws block-entity blocks through
  `BlockActorRenderDispatcher::render` with block entities created by
  `BlockActor::create` from NBT, as a `.mcstructure` stores them.
- Round 10 (`6a6212a`): `BlockActor::create` from NBT returned null for
  every block entity id (Chest, EnderChest, Bed, Sign, Skull, ShulkerBox,
  Banner), so nothing was drawn. The game's own structure block preview
  does not show chests, ender chests or shulker boxes either. Round 11
  uses `VanillaBlockActorFactory::createBlockActor(pos, blockType)`.
- Round 11 (`d7efc6b`): with `VanillaBlockActorFactory::createBlockActor`
  and `BlockActorRenderDispatcher::render` (render position relative to
  the camera), chest, ender chest, sign, banner and shulker box draw their
  real models on the block grid. Bed and skull drew nothing (they likely
  need their block entity data: bed parts and color, skull type and
  rotation). These models look like real blocks, not tinted, and their
  brightness follows the world's light; the outline still marks them.
  Loading the schematic's block entity data into them is untested.
- `.mcstructure` layout confirmed on the maintainer's export (2026-10-03,
  `mixture.mcstructure`): root `format_version` (2 here), `size`,
  `structure_world_origin`, `structure.block_indices` (layers),
  `structure.palette.default.block_palette` (name, states, version),
  `structure.palette.default.block_position_data` (index -> block_entity_data
  with `id` such as Chest, EnderChest, MobSpawner, ShulkerBox, Campfire),
  and `structure.entities` (e.g. an armor stand with `Pos`). Sample files
  the maintainer allows for testing: `mixture.mcstructure` and
  `broken_village_house.mcstructure` (in their Downloads folder; not
  committed).
- A possible path for the file browser preview: the game's
  `StructureVolumeRenderer` (the structure block's 3D view) renders a
  block volume into UI. Not tried yet.
- Default look (decided 2026-10-03): tinted with a light-blue outline.
  Translucent stays as an option: it looks right block by block, but with
  many adjacent blocks (builds) its result is hard to predict.
- Torches, chests and other blocks without a mesh on this path: explore
  more render paths before falling back to an outline only (maintainer,
  2026-10-03).
- Still open before or during implementation: real translucent blocks
  behind ghosts (draw later than the world's translucent layer, or without
  depth writes now that quads are sorted); blocks without a mesh on this
  path (torch, chest and other block entities, which need another path or
  an outline only); shapes that depend on neighbors (fences, panes, stairs
  corners, redstone) should follow the schematic's neighbors, not the real
  world's; per-section cached meshes with sorting kept within the frame
  budget instead of rebuilding everything each frame.
- Fallback accepted by the maintainer if translucency fails: opaque blocks
  drawn slightly differently (tinted) inside a light-blue outline, clearly
  readable as schematic blocks.
