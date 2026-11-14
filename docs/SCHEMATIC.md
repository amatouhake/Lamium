# Schematics

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/SCHEMATIC.md`.

Build from a saved `.mcstructure`: pick a file, place it in the world, see it
as ghost blocks, compare it with what is built, list the materials still
needed, and save an area of the world as a file. Experimental, default off.
Controls (move target, menu, Repeat, cut-plane layers and their storage,
looked-at selection) are in [SCHEMATIC-CONTROLS.md](SCHEMATIC-CONTROLS.md).
Open follow-ups are indexed by [L-93 Schematic: load, place, project, verify and list materials (experimental)](backlog/L-93.md).

Settings (Schematics category, `schematic.*`): `enabled` (with key and
screen opener); menu and Repeat rows (SCHEMATIC-CONTROLS); `ghostTint`
0-100% in 5% steps (70); `ghostLines` Thin / Normal / Thick / None (Normal);
`outlineDistance` 0-192 in 16s (48, 0 draws all); Schematic HUD `hud` (off)
with `hudVerify`, `hudMaterials` (on). Check heading: `mistakeMarks` (on),
`ignoreSignal`, `litAsState`, `ignoreGrowth`, `ignoreBlockEntities`,
`ignoreSignText` (off), each with an optional key. Target Info: "Show
schematic information" (`information.targetSchematic`, on).

## Behavior

Decided 2026-10-03 (maintainer): an independent implementation (see
[PROVENANCE.md](PROVENANCE.md)); `.mcstructure` only (`.litematic` maybe
later); a dedicated screen, keys and a radial menu, no held-item tool unless
users ask; no hard size limit, work runs in bounded steps per frame and fills
in progressively ([DESIGN.md](DESIGN.md#engineering)).

### Files

- Files live in `mods/Lamium/schematics/` (subfolders allowed). The Files tab
  groups them by folder with size and block-count columns (size first when
  narrow; filled one file per frame for unloaded files up to 2 MB, "-" above),
  rescans every two seconds while shown, and has "Open folder". A file over
  2 MB shows a warning and "Load" first; over 256 MB it is refused.
- Missing or unreadable files (L-237, L-241, L-242): failures are cached with
  the same two-second recheck as loads; read, size and I/O failures retry on
  the next check, parse failures wait for a changed file time; only a new or
  changed error is logged. While Schematics is on (else once it is turned on),
  unreadable placements are announced once per world generation: one toast
  per frame with the count and at most two names, "Schematics:", six seconds,
  warning-colored, shown even with toggle toasts off. Recovery and failure
  again in the same world do not repeat it.
- An unreadable placement: "!" and its name in the warning color in the Placed
  list, the reason in its detail pane, "File unreadable" in the HUD layer
  cell, layer fields that say the view is unavailable, and no ghosts (the size
  is unknown). Restoring the file clears it after the next check.

### Placements

- Several placements at once, saved per world (servers by address) and
  dimension beside the waypoints (`schematics.json`, format in
  [SCHEMATIC-CONTROLS.md](SCHEMATIC-CONTROLS.md#storage-and-migration)).
- One selected placement drives movement, turn/mirror, layers, Check,
  Materials, the HUD and the nearest-mistake key. It changes only when a
  schematic is placed (the new one is selected), when one is picked in the
  Placed list, with next/previous placement, or with the looked-at selection;
  never by walking near another. No placement switcher on the Check and
  Materials tabs: it does not scale; the Placed list is where to pick.
- Per placement: position, rotation, mirror, visibility, layer view, extra
  blocks (shown red and counted by default, or ignored), entities (default
  on). Structure void means "place nothing here".
- Where two placements' ghosts share cells, the one earlier in the list draws
  them (two ghosts on the same planes flickered; L-141).
- World frames: every placement in the dimension gets a frame of light-blue
  lines (hidden ones only when selected); the selected one at full strength,
  the others at .35 alpha.
- Maps: placements in the viewed dimension show their footprint (cyan outline,
  black edge, faint fill, under the waypoints; selected outlined white). The
  minimap shows visible ones; the world map also hidden ones, faintly, with
  "(hidden)" and the name.

### Ghosts

- Missing blocks are drawn as the real block model tinted light blue at
  `ghostTint` with a light-blue outline (maintainer 2026-10-03: brightness
  never follows the world's light, so a ghost in a cave reads as in
  daylight). Outlines: Thin is game lines (one pixel), Normal and Thick are
  solid bars on the edges, None draws no ghost outline (mistake marks keep
  theirs, thin). Do not change the defaults without the maintainer (L-186).
- Where neighbors' bars meet, only the strongest result's copy is drawn within
  a section, slightly wider to cover the other across sections: wrong block >
  unknown block > wrong state > ghost/missing.
- Shapes come from the file: neighbor-dependent blocks are tessellated against
  the placement's own cells (the real world or air outside it). A two-block
  block whose other half is missing from the file draws its own half against a
  made partner; when the file has its own other half there, that is used.
- Blocks drawn by a block-entity renderer (chest, bed, sign, banner, head,
  shulker box) get an actor loaded with the cell's saved block entity data,
  turned with the placement, lit fully, and outlined by the block's own
  selection shape. Blocks whose mesh is only a base (piston, lectern, flower
  pot, enchanting table, item frames, campfire, spawner, bell, shelf;
  `actorDrawsOverMesh`) get the actor too. Ghost actors are not animated.
- A block with neither mesh nor actor still gets the outline of its cell, so
  no block is invisible.
- Liquids: water and lava are drawn only where missing, as textured, sloped,
  flowing shells, blended, with no outline; faces toward the same liquid or an
  opaque block are left out. Second-layer (waterlogged) water is drawn only
  under a ghost; a shell around a placed block looked like real water
  (maintainer 2026-10-09). A covered side of a waterlogged block hides its
  water. Real liquids beside a ghost do not bend its shell.
- Render layers: each ghost is drawn once per render layer it uses; blended
  ghosts, liquid shells and mistake marks go into one camera-sorted mesh drawn
  after the world's translucent blocks, so real water, honey and glass stay
  visible inside and behind them.
- Near the camera: in the camera's cell a ghost keeps only faces turned toward
  the camera (as real blocks are not drawn from inside); a pair of touching
  ghost faces near the camera keeps only the camera-facing one, for every pair
  (`pairAllGhostFaces` on, maintainer 2026-10-09: ghosts flickered more than
  real blocks; the switch stays so it can be turned off quickly). A ghost face
  covered by a real opaque or see-through block's face is left out.
- Ghosts with an opaque full block (real or ghost) on all six sides are not
  tessellated, except within two cells of the camera.
- Missing file entities: their game model with light-blue part outlines
  (L-115), else a dashed frame with a name tag, hidden behind blocks.
- Vibrant Visuals: supported with limits. Every schematic line and the
  mistake faces keep their colors; ghost faces lose their tint.

### Check

- States: not placed (ghost), correct (hidden), wrong block or extra block
  (red; one group everywhere), wrong state (yellow), unknown block name
  (orange). Counts follow the shown layers.
- Strict by default (maintainer 2026-10-09): every differing state is a
  mistake, including states the game changes itself. The Check switches
  relax it by category (L-262, global, off by default, maintainer
  2026-10-26): ignore redstone signal; ignore plant growth; on and off forms
  (redstone lamp, repeater, comparator, redstone torch) as a wrong state with
  a "Lit"/"Powered" row instead of a wrong block, never correct. An ignored
  difference is correct everywhere (list, HUD, nearest mistake, marks). Rules
  are a table: a new category or state is a row, not code. Selected-cell
  suppression is a separate design ([L-183 Disregard selected schematic check mismatches](backlog/L-183.md)).
- Waterlogging: once a cell's block is right, the second layer must match the
  world's extra block (none on both sides included); a difference is a state
  row `waterlogged: true -> false` (or the reverse) and a yellow mark. A
  first-layer air cell with second-layer water expects standing water.
- Block entity data (L-303, selected placement only): the fields the client
  keeps current are compared: note block note and instrument (where the
  file's blocks decide it), lectern book and page, flower pot plant, banner
  colors and patterns, a floor skull's type and rotation, bed color, decorated
  pot sherds, item frame item and rotation, sign text, and a shulker box's
  opening. A difference is a wrong state with a row per field. A world value
  that cannot be read passes. Not compared: container contents and a
  jukebox's disc. "Ignore block entities" turns the whole comparison off
  (block states are still compared); "Ignore sign text" only the sign's text,
  color and glow.
- Entities are checked by type only: one of the same type within one block of
  its spot, judged only within 48 blocks of the player in loaded chunks
  (beyond that the client does not know them). Pose, equipment, name and
  contents are not checked.
- Mistake marks (`mistakeMarks`): red/yellow boxes in the world. Off, they
  are not built or drawn; the check still runs.
- Nearest mistake key: marks the nearest mistake with its distance; again,
  the next. "Show in world" from the Check list closes the screen and marks
  the cell with a white box and a beam for 30 s.
- The Check tab lists up to 2000 mismatches per pass, mistakes before not
  placed, nearest first, filtered by Mistakes / Wrong or extra / Wrong state /
  Not placed with counts above the list; selecting a wrong-state row shows
  every differing state in the right pane as "<state> <now> -> <should be>"
  with readable names.

### Materials

- Lines by item: the block's pick item; double slabs count two; upper door and
  bed halves count none; liquids count source cells only (a flowing cell needs
  no item; the Check still compares flow, L-289).
- Columns need / placed / left / carried, all layers or shown layers only.
  Carried includes shulker boxes in the inventory. Green when enough, yellow
  when short. Blocks first, then entities ("-" carried unless an item places
  them, such as an armor stand).
- Amounts convert to "1 chest + 4 stacks + 5" (a chest is 27 stacks; smaller
  stack sizes respected) on hover and in the right pane. The right pane shows
  the missing materials as slots, "Show on the schematic HUD", and "Open in
  ResourceCalculator" with what is missing in the URL fragment.
- ResourceCalculator's results are never parsed or its data used: it has no
  API and its code and recipe data are GPL-3.0 (Lamium is LGPL-3.0). Raw
  materials come from the game's own recipes ([L-116 Raw materials from the game's recipes](backlog/L-116.md)).
- One material identity for the list, the check and later placing assistance.

### Target card and ghost pick

- With "Show schematic information" on, a ghost hit first is the target:
  named in light blue with "Schematic: Not placed yet", no placement name, not
  paired with the real block behind it. Ghost liquids are always targetable;
  real liquids only with Target Info's own option (L-264).
- A real block's differences are read live for that cell (after checking the
  selected placement's revision, visibility, dimension and shown layers), not
  from the capped list. Difference rows come first: "Schematic: Wrong block /
  Extra block / Wrong state" in the check's color, then "Should be [icon]
  <block>" (red), "Should be Air" (red), or per state "<state>: <now> ->
  <should be>" (yellow); the card's own row for that state is left out.
- The card keeps its ordinary single layout. A ghost's card follows the real
  block's rules from the file alone; a value the file does not decide reads
  "Not set by the schematic" (L-184).
- Ghost pick (L-263, L-285): the vanilla pick-block input on a ghost brings
  the ghost's block into the hand like vanilla (hotbar slot, else from the
  inventory). The nearest thing on the view ray wins: a game hit counts only
  when it lies on the ray (0.05 blocks), so the feet fallback never beats the
  ghost and a real block in front still wins. In creative an uncarried block
  is given if the creative inventory lists it (capability-gated). The move is
  confirmed from the slot on a later frame.

### Schematic HUD

One element, default off and switched only by the player (placing never turns
it on): a fixed-width card with the name and layer, a two-by-two grid of
counts (correct/total, not placed, wrong or extra, wrong state), the nearest
mistake's distance, then up to five materials left with icons and
right-aligned left/have (yellow when short).

### Area save

- Corner keys set corner 1 (red) and corner 2 (blue) on the looked-at block,
  outlined on the block's edges with faintly tinted faces just outside. Only
  the save key opens the prompt: both corners as -/+ steppers (Shift: 10), the
  size, the name, the file it becomes, "Include entities" (default off),
  "Clear area". The area stays after saving until cleared or the world is
  left.
- The save reads bounded chunk columns; an unloaded column waits until the
  player comes near (yellow column frames, a toast every 8 s with direction
  and distance); "Stop saving" stops it. An existing file needs a second
  press ("Overwrite").
- Saved: blocks with states, the water layer, block entity data as the client
  knows it (a container's items only if synced), and optionally entities'
  type, position and facing.
- Written as `format_version` 2, with the second layer only when some cell in
  it is not -1. After writing, the same bytes go to the game's structure
  loader on a throwaway template; a rejection keeps the file and the prompt
  says so (`schematic.save.gameRejected`).

### Screen review against the mockup

Decided 2026-10-08 against [demos/schematic-screen.html](demos/schematic-screen.html):
tabs Placed, Files, Check, Materials, without counts on the tabs (the
sidebar entry keeps its count); the Placed list marks the selected placement
with the accent bar and shows progress as a percentage over a short bar
(correct / total in the shown layers, other placements counted in the
background while the list is shown, "-" until done); when narrow, the name
is cut first, then coordinates shorten or drop. Wherever a block or item is
named, its icon comes first. With Schematics off, Check and Materials explain
the off state (an unreadable file keeps its error first) and name the "Show
and check" menu category.

### Check tab review

Decided 2026-10-08 against [demos/schematic-check.html](demos/schematic-check.html):
layout A (the 3D preview at a fixed size at the top of the right pane, counts
and the selected mistake's details below, scrolling); the preview follows the
kind filters; wheel zoom in both previews. Preview scope: view (turn, zoom,
move, back to the whole, cut at a layer) and inspect (click a block for its
name, states and result; in Check it picks the row). Editing blocks in the
preview is a separate, later feature. Still open: how mistakes look in the
preview and how the selected one is found inside a build, which wait for
rendering work ([L-114 3D previews in the schematic screen](backlog/L-114.md),
which also says how the previews are built).

## Code

Under `src/features/schematic/` unless stated.
- Files: `Nbt.*`, `Structure.*` (`parseStructure`, `writeStructure`,
  `writtenFormatVersion`, `otherHalf`), `StructureInflate.*`;
  `SchematicSession.*` (files, placements, load cache, size limits);
  `PlacementStore.*` (`schematics.json`).
- Pure rules: `Placement.h` (`toWorldYaw`, `toWorldFace`, layers), `Verify.h`
  (`withLiquid`, `stateDifferences`), `CheckRules.h`, `BlockEntityCheck.h`,
  `Verification.h` (`maxMismatches`), `GhostFaces.h`, `LiquidShape.h`,
  `PlacementOverlap.h`, `RayCells.h`, `PickRay.h`, `SectionLayers.h`,
  `PreviewView.h`, `MaterialAmount.h`, `RawMaterials.h`,
  `src/ui/SchematicFiles.h`, `src/features/information/SchematicTarget.h`.
- Ghost pass: `GhostRenderer.cpp` (sections to build and draw, the blended
  mesh, lifecycle); `GhostMesh.*` (8-block sections, `cullAgainstGhosts`,
  `dropBackFaces`, `dropDuplicateQuads`, liquid shells); `GhostBlocks.cpp`,
  `ResolvedPlacement.*`; `GhostActors.*` (`makeBlockActor`,
  `renderBlockActor`, light hooks); `SchematicRegion.*`; `GhostVerify.*`
  (`classifyCell`, progress, entity spots); `GhostMarks.*` (frames, save area,
  name tags); `EntityModels.*`, `RestPose.h`; `GhostPick.*`.
- Area save: `AreaSave.*`, `SaveArea.h`, `Selection.*`. Previews:
  `Preview.cpp`. Screen: `src/ui/SchematicsView.cpp`. Lines:
  `src/overlay/LineColor.h`.
- Tests: `tests/SchematicTests.cpp` (`structureWrittenShape` and the rest),
  `BlockEntityCheckTests.cpp`, `RawMaterialsTests.cpp`,
  `SectionLayersTests.cpp`. `LAMIUM_SAMPLE_STRUCTURES` (a folder or
  `;`-separated files) reads real exports, writes them back and checks layers,
  version and layer count. Traces: `schematic_perf_trace`,
  `schematic_quad_trace`, `ghost_probe` (`GhostProbe.cpp`).

## Known

File format:
- Root `format_version`, `size`, `structure_world_origin`, `structure`;
  `structure.block_indices` (layers, int arrays; older files int lists),
  `structure.palette.default.block_palette` (name, states, version),
  `...block_position_data` (index -> `block_entity_data` with `id`), and
  `structure.entities` (with `Pos`). 1.26.60 stores `structure` as a byte
  array, zlib when root `compression` is 1; Lamium writes the compound form.
- Version 2 has one layer, or two when something is waterlogged: the second
  is -1 except waterlogged cells, which hold the palette index of
  `minecraft:water` `liquid_depth` 0. Plain water and lava are ordinary
  first-layer entries (`liquid_depth` gives the flow). An air cell may hold
  second-layer water; unused palette entries are valid.
- A version 1 header with `List<IntArray>` layers was what other tools
  rejected; the game's loader accepts at least 122x39x203. Files saved by
  Lamium before version 2 stay version 1 until saved again.
- A shulker box keeps its opening in block entity data (`facing`), not a
  state, so the game's state transform does not turn it; Lamium turns it.
- Fence/pane connections and stair corners are stored as block states and
  turned with the file's states.

Game and SDK:
- Block states are turned by
  `VanillaBlockStateTransformUtils::transformBlock`. The game's `Mirror::Z`
  flips east-west, so Lamium's X maps to it and Z to `Mirror::X`. Entity
  facing follows the structure block (maintainer 2026-10-09): X is
  180 - yaw, Z is -yaw.
- `SchematicRegion` derives from `BlockSource` (exported constructor) and
  overrides `getBlock`, `getMaterial` and `getBrightness`; the private
  tessellator reads neighbors through it.
- The shape dispatcher is `BlockTessellator::tessellateInWorld`
  (`tessellateBlockInWorld` is the cube path). A private tessellator needs
  priming and keeps the color override out of vanilla's mesh caches. Render
  layers: `BlockType::getRenderLayer` plus each bit of
  `getExtraRenderLayers`, with `mRenderingLayer` set per call.
- Faces: the moving-block renderer's alpha-test material and terrain atlas
  after `ActorShaderManager::setupShaderParameters(..., ignoreLighting)`.
  Blended mesh: drawn from `LevelRendererCamera::$renderBlockEntities` when
  `renderAlphaLayer` is set (once a frame, after translucent terrain) with
  `beacon_beam_transparent`; marks over real blended blocks use
  `holo_hand_pointer`. Lines: `selection_box` keeps its color under Vibrant
  Visuals.
- Block actors: `VanillaBlockActorFactory::createBlockActor`, loaded with
  `BlockActor::load`, drawn by `BlockActorRenderDispatcher` against a
  `SchematicRegion`; both `setupShaderParameters` overloads (one takes a
  `BrightnessPair const&`) are hooked to the fully bright setup for them.
- Liquid textures: the liquid's `BlockGraphics` slot 0 bottom, 1 top, 2 sides;
  the flowing texture's atlas cell is one 16x16 texture, mapped whole.
- The game's name tag list allocator is not exported, so name tags are drawn
  in Lamium's own pass.
- The client knows entities only within about 48 blocks, and container items
  only when synced; other block entity fields stay at the copy the world
  loaded with except the ones the client draws.
- Budgets (L-256): the verification scan is capped at 0.5 ms a frame,
  rebuilds at 3 ms and 32 sections a frame.

## Tried

- `moving_block_blend` by name: drew nothing. `renderGuiBlock`: ignored
  alpha, off the grid. The item-shape mesh: ignores block states.
- Separate draw calls per ghost: reordered between frames (flicker); one
  sorted mesh fixed it.
- An unlit blended face material without depth writes: very faint, varying
  with what lay behind.
- A translucent ghost look as default: right block by block, unpredictable in
  dense builds; the tinted look is default, translucency is [L-186 Improve schematic ghost legibility and reconsider translucency](backlog/L-186.md).
- `BlockActor::create` from NBT: null for every id.
- Hooking `BlockSource::getLightColor`: crashed at start (its `Brightness` is
  by value in the game, by reference in the SDK); `getBrightnessPair`: no
  effect.
- A `static` block pointer in `liquidShell`: crashed after a registry reload.
  Look it up each time.
- `beacon_beam_transparent` in the previews: no depth, so the far-to-near
  order alone erred inside cells (parts above water looked submerged).
- Insetting near-camera ghost faces by 0.4%: still z-fought; opening the
  camera's cells: hollows at the near plane.
- Drawing see-through marks in the cracks, name tag or block entity alpha
  passes, or lifting them out: no effect (L-194).
- A near-camera combined mesh (L-256 item 7a): measured and dropped.
- Stacked and side-by-side target card sections: hard to read (maintainer).

## Limits

- Unchecked in game (servers, other dimensions, large files, redstone, a bed
  with one half outside the file, preview tints follow the player's biome):
  [VALIDATION.md](VALIDATION.md), Schematics row.
- Previews (accepted, maintainer): empty texels write depth, so through
  seagrass or a spawner's cage parts behind can be missing.
- A door or bed half drawn against a made partner shows only that half;
  nothing is drawn outside the placement.
- A cell whose real liquid has another flow level gets a yellow mark and no
  shell, so a thin real flow shows little of the file's surface.
- Under Vibrant Visuals ghost faces are untinted and the honey ghost is black;
  faces and lines glow after closing the inventory until the player moves.
- Next to real see-through blocks: [L-194 Save corner marks over real glass or water change with the camera](backlog/L-194.md),
  [L-245 Ghost face disappears next to real glass or a beacon from some angles](backlog/L-245.md).
- A server's placements are lost when its address changes: [L-343 A server's map, waypoints and placements are lost when its address changes](backlog/L-343.md).
- Open work: [L-114 3D previews in the schematic screen](backlog/L-114.md);
  [L-115 Entity ghosts drawn as models](backlog/L-115.md);
  [L-117 Schematic entity name tags render badly in Japanese](backlog/L-117.md);
  [L-184 Inspect ghost block data and schematic container contents](backlog/L-184.md);
  [L-185 Instantly load a schematic into the world when cheats allow it](backlog/L-185.md);
  [L-190 Select several rows at once in list screens](backlog/L-190.md);
  [L-265 Schematic placement assist: place the ghost's block in its state](backlog/L-265.md)
  (L-116, L-183 and L-186 are linked above). Later, not chosen: area fill,
  layers by material.
