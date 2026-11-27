# Placement assist

A schematic mode in which using a block on a ghost places the ghost's block
there in the ghost's state: facing, half, axis and the other states a
placement decides. For players building from a schematic by hand (GitHub
issue #85). Item: [L-265 Schematic placement assist: place the ghost's block in its state](backlog/L-265.md).
Nothing is built yet; this is the agreed contract.

Settings: a row under Schematics that enables the mode (off by default).
Keys: a toggle action, unbound by default, that switches the mode on and
off; the schematic HUD shows it while it is on.

## Behavior

Decided by the maintainer on 2026-11-28; lines marked (proposed) are not
agreed yet.

- **Server rules.** Some servers forbid this kind of help; following a
  server's rules is the player's part, as for the rest of Lamium.
- **Packets.** A placement carries only what the player could send by hand,
  with two exceptions decided for this mode: the look sent for that one
  placement (where a state follows the player's facing), and a click on a
  real neighbor's face the crosshair is not on. Any other change needs its
  own decision.
- **When it acts.** With the mode on, a use press whose view ray meets a
  ghost first (the ghost pick's ray, L-263; which placements count is
  proposed: the shown ones, as for the ghost pick) is
  the assist's; a real block in front of the ghost wins and stays vanilla.
  With the mode off, or no ghost first on the ray, the use input is
  vanilla.
- **Target.** The first ghost cell on the ray, not only the cell vanilla
  would fill. The placement is sent as a click on a real neighbor's face
  at a click position, chosen so that the placed block matches the ghost.
  (Proposed) a ghost cell already holding a real block that cannot be
  built over is left alone; one that can (grass) is placed over, as
  vanilla does.
- **State.** Where a state follows the player's facing (stairs, furnaces,
  ...), the auth input sent just before the placement carries the look
  that gives the ghost's state, with the move input turned with it so the
  movement stays the same; the next one carries the real look again.
  Where no neighbor, click and look give the ghost's state, nothing is
  placed.
- **Held block.** When the hand does not hold the ghost's block, it is
  brought into the hand first with the ghost pick's step (hotbar slot, else
  from the inventory; in creative given when the creative inventory lists
  it). When it is not carried, nothing is placed.
- **Holding use.** Holding the use button keeps placing on the ghost under
  the crosshair at vanilla's repeat rate. Placements that need a faked
  look need no slower rate (the fifth trace showed none at 0.1 s).
- **Feedback.** When the assist places nothing, a short Lamium toast says
  why (wording proposed): "Not carried: <block>" or "Cannot be placed in
  this state from here: <block>". Placing leaves the HUD's counts to the Check as usual.
- **Lifecycle.** A held placement waits only for its pick and its one
  faked auth input, a few ticks at most; focus loss, world exit, dimension
  change and switching the mode off drop it, and the next auth input
  carries the real look (a LIFECYCLE.md line when built).
- **Version.** The faked look, the hit result written for the call and the
  placement itself ask `versionSensitiveAllowed()` (a new capability);
  without it the mode stays off (how the row shows that: proposed, as
  other gated features do).

## Code

Planned (the experiment is `placement_fake` in
`src/features/interaction/PlacementTrace.cpp`, option only):

- `src/features/schematic/AssistPlan.h` (pure, tested): for a target cell,
  the neighbors that can be clicked (solid, not replaceable), their faces
  and click positions (side faces: upper and lower half; top and bottom),
  and the looks to try (the real look first, then the four horizontal
  facings and up/down), ordered so that no fake is tried before a fake.
- `src/features/schematic/PlacementAssist.cpp` (game glue): the build hooks
  hold vanilla's placement when the assist takes the press; the candidates
  are tried with `BlockType::getPlacementBlock` with the local player's
  rotation set for the call, compared with the ghost by the Check's cell
  rule; then, after one faked auth input if needed, `GameMode::buildBlock`
  for the chosen neighbor and face with `Level::getHitResult` set to the
  chosen face and click for the call. The placed block is read back on a
  later frame.
- Hook order: `buildBlock` and `useItemOn` are already hooked by
  CameraInteraction, EdgeGuard and Sneak placement; the assist's hold must
  run first and its own `buildBlock` call pass through the others.
- Settings field, option, store, row and strings in four languages; the
  toggle action appended to `Binding.h`; the HUD line in the schematic HUD.

## Known

From the five L-265 traces of 2026-11-28 (VALIDATION-LOG), local world,
1.26.60.05:

- A placement is its own `InventoryTransactionPacket`; the transaction
  carries no rotation. The local server places with the rotation of the
  auth input sent just before it (the fakes changed `mRot`, `mYHeadRot`,
  `mInteractRotation` and `mCameraOrientation` together).
- One auth input with yaw + 180 followed by the placement placed the stair
  turned on both sides. With the move vectors and direction flags turned
  with the look, faked placements one after another (down to one every
  0.1 s, walking and sprinting) moved the player no differently. Without
  turning them, two fakes close together pulled the player back, and once
  a sprint slowed five ticks after a fake.
- `GameMode::buildBlock(neighbor, face)` sends the placement for any cell
  and a face the crosshair is not on is accepted. On a fresh press it takes
  the click from `Level::getHitResult` (set for the call, the sent click is
  the chosen one); while the button is held it took another click in 33 of
  77 placements (relative to another cell), so the held case needs a click
  source that does not depend on the build in progress.
- `BlockType::getPlacementBlock(player, cell, face, click, aux)`, with the
  click relative to the clicked block, matched the placed block in all 189
  client placements tried (dirt, logs, stairs, furnace, door, trapdoor,
  slab).
- Stairs: facing from yaw, upside down from the click height on a side
  face; slabs: half from the click height; logs: axis from the face.
- A use press that meets no block still reaches `GameMode::useItem` (hit
  type NoHit): the trigger for a ghost with air behind.
- Not shown yet: a pick followed by a placement; data-driven blocks.

## Tried

- Replaying the held placement as a use press: the crosshair moves on
  while walking and the replay lands elsewhere or is dropped.

## Limits

- Not checked: a dedicated server, blocks other than stairs, slabs and
  logs, data-driven blocks whose placement may not go through
  `getPlacementBlock`.
- States no placement makes (a stair's corner shape, fence connections)
  follow the neighbors as in vanilla.
