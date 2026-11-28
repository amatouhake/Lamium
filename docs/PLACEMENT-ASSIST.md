# Placement assist

A schematic mode in which using a block on a ghost places the ghost's block
there in the ghost's state: facing, half, axis and the other states a
placement decides. For players building from a schematic by hand (GitHub
issue #85). Item: [L-265 Schematic placement assist: place the ghost's block in its state](backlog/L-265.md).
Built; not yet checked in game.

Settings: "Placement assist" under Schematics (off by default, needs
Schematics on). Keys: a toggle action, unbound by default, not in the
schematic menu (a settings row with an optional key, like the Check rules);
the schematic HUD shows "Placement assist on" while it is on.

## Behavior

Decided by the maintainer on 2026-11-28.

- **Server rules.** Some servers forbid this kind of help; following a
  server's rules is the player's part, as for the rest of Lamium.
- **Packets.** A placement carries only what the player could send by hand,
  with two exceptions decided for this mode: the look sent for that one
  placement (where a state follows the player's facing), and a click on a
  real neighbor's face the crosshair is not on. Any other change needs its
  own decision.
- **When it acts.** With the mode on, a use press whose view ray meets a
  ghost first (the ghost pick's ray, L-263: any shown placement's ghost)
  is the assist's; a real block in front of the ghost wins and stays vanilla.
  With the mode off, or no ghost first on the ray, the use input is
  vanilla. Whatever the hand holds, a tool or food too: it is used by
  looking off the ghost (maintainer 2026-11-28, replacing the block-only
  rule once ghosts could be shrunk).
- **Real blocks on the ray.** A real block stops the search only where
  the ray meets its outline: right against a fence post the eye shares the
  post's cell column (2026-11-28).
- **Target.** The first ghost cell on the ray that has a real neighbor to
  be clicked from, not only the cell vanilla would fill; ghosts in front of
  it without one do not hide it (maintainer 2026-11-28, after the first
  run: an inner ghost was out of reach). The placement is sent as a click on a real neighbor's face
  at a click position, chosen so that the placed block matches the ghost.
  A neighbor that opens or toggles when used (a chest, a door, a lever) is
  clicked only while the player sneaks.
  A ghost cell holding a real block that cannot be built over is left
  alone; one that can (grass, flowers, snow layer) is placed over, as
  vanilla does.
- **State.** Where a state follows the player's facing (stairs, furnaces,
  ...), the auth input sent just before the placement carries the look
  that gives the ghost's state, with the move input turned with it so the
  movement stays the same; the next one carries the real look again.
  Where no neighbor, click and look give the ghost's state, nothing is
  placed.
- **Held block.** When the hand does not hold the ghost's block and the
  ghost can be placed (checked first, so the hand never switches where
  nothing places), it is brought into the hand with the ghost pick's step (hotbar slot, else
  from the inventory; in creative given when the creative inventory lists
  it). When it is not carried, nothing is placed. A block that came by a
  swap or a give is placed two auth inputs later: sent at once, the
  placement read back as air and the slot went back (2026-11-28, cause to
  confirm by the readback log, which names the clicked neighbor, its
  distance and the swap).
- **Holding use.** Holding the use button keeps placing on the ghost under
  the crosshair. The game repeats its build call only while its hit is in
  reach (2026-11-28 trace), so after a press it took the assist aims again
  every 250 ms itself until `stopBuildBlock` (the release; the game also
  calls it every tick while nothing is held). A press the assist took stays its
  own until the button is let go: where no ghost is aimed meanwhile,
  nothing is placed (vanilla's held placement had put blocks the schematic
  does not have). A refusal is said again only after 1 s while the button stays held. Placements that need a faked
  look need no slower rate (the fifth trace showed none at 0.1 s).
- **Feedback.** When the assist places nothing, a short Lamium toast says
  why: "Not carried: <block>" or "Cannot be placed in this state from
  here: <block>". Placing leaves the HUD's counts to the Check as usual.
- **Lifecycle.** A held placement waits only for its pick and its one
  faked auth input, a few ticks at most; focus loss, world exit, dimension
  change and switching the mode off drop it, and the next auth input
  carries the real look (a LIFECYCLE.md line when built).
- **Version.** The faked look, the hit result written for the call and the
  placement itself ask `versionSensitiveAllowed()` (a new capability);
  without it the mode stays off and its row and toggle toast say so, as
  for the other gated features.

## Code

- `src/features/schematic/AssistPlan.h` (pure, `tests/AssistPlanTests.cpp`):
  the candidates for a cell (the clickable neighbors below, beside, above;
  side faces clicked at a quarter and three quarters high, top and bottom
  in the middle) under thirteen looks (the player's own, then the four
  facings level, looking down and looking up), unfaked ones first;
  `placesAs`, the ghost match that leaves derived states (a stair's
  corner, wall connections, open, powered, growth) to the neighbors and
  later use; `turnedMove`/`keysOf`/`viewOf` for the faked auth input.
- `src/features/schematic/PlacementAssist.cpp` (game glue, the `Placement
  assist` feature row): `takePress` (from the build hooks and from
  `useItem` with no block hit) finds the ghost with the ghost pick's ray
  (`ghosts::aimedGhost`), leaves a cell holding a block that cannot be
  built over to vanilla, refuses a second half (a door's top, a bed's head:
  `describe(...).perBlock == 0`),
  asks the ghost item's block `getPlacementBlock` for each candidate (local
  rotation set for faked looks; the player's own look is the one last
  sent) and keeps the first that `placesAs` the ghost, then brings the
  item into the hand (`inventory::bringToHand`, in creative
  `giveToHand`). A refusal (and a placement read back as not the ghost's)
  is kept for that cell for a second (not cleared by
  `PlacementAssistStopBuild`, which runs every tick while nothing is built), so a held button does not
  ask again every tick; the log names the first state that differed, or
  the six neighbors when none can be clicked. `ghostAlong` takes an
  `accept` test, so the search walks past ghosts with no clickable
  neighbor. `placeNow` runs
  queued: `buildBlock` on the chosen neighbor and face with the hit result
  and build context set (`AsClicked`: no build direction, the initial click
  at the chosen point, no liquid hit) and the rotation set (`AsLook`).
  `PlacementAssistSend` fakes every auth input until the Place
  transaction leaves (at most three) and corrects a differing click; each
  placed block is compared with the ghost ten auth inputs later and logged
  when it does not count as placed.
- Hooks run at the highest priority, so the assist decides before
  CameraInteraction, EdgeGuard and Sneak placement; its own `buildBlock`
  call passes through them (`placing`).
- Settings field `schematic.placementAssist`, option, store, row
  (`schematicAssist`), action `TogglePlacementAssist`, capability
  `PlacementAssist`, HUD line in `InfoHud.cpp`, strings in four languages.

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
  type NoHit): the trigger for a ghost with air behind. The game's hit may
  also be a block or an entity past reach with no outline (2026-11-28),
  taken the same way (`meetsNoBlock`; blocks seen placing).
- Not shown yet: a pick followed by a placement; data-driven blocks.

## Tried

- Replaying the held placement as a use press: the crosshair moves on
  while walking and the replay lands elsewhere or is dropped.

## Limits

- Not checked: everything in game (VALIDATION row to come); a dedicated
  server; data-driven blocks whose placement may not go through
  `getPlacementBlock`; the turned move input at quarter turns (the traces
  turned only by 180 degrees).
- A double slab and the second half of a two-cell block (a door's top, a
  bed's head) are not placed ("Cannot be placed in this state from here");
  aim at a door's lower half.
- States no placement makes (a stair's corner shape, fence connections,
  `minecraft:connection_*` since 2026-11-28) follow the neighbors as in
  vanilla.
