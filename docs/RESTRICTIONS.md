# Placement and breaking restrictions

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/RESTRICTIONS.md`.

Breaking Restriction keeps mining inside a region anchored where the attack
press starts, so ground can be leveled and tunnels dug without breaking past
a chosen level or face. Placement restriction is not implemented. Edge Guard
stops walking off block edges without sneaking. The breaking mining session
is shared with Tool Protection and Tool Switch.

Settings and keys: Breaking Restriction switch (`breakingrestriction`
toggle) with its mode (`cyclebreakingmode` cycles it) and the height band
`breakingBand` (1-16, default 2) under the "Block Restrictions" heading;
Edge Guard switch (`edgeguard` toggle) with "Stop inside the edge by"
(`interaction.edgeMargin`). All keys unbound by default. The whole plan,
placement included, is reopened for redesign:
[L-15 Breaking and placement restrictions](backlog/L-15.md).

## Behavior

### Regions

The shared region predicate (`RestrictionRegion.h`) defines the modes:

- Plane: fixes the anchor coordinate on the mined face's normal axis.
- Line: fixes the other two coordinates, extending along that axis.
- Column: fixes X/Z and extends vertically, independent of the face.
- Layer: fixes Y and extends horizontally, independent of the face.
- Height band (breaking only): the rows from the feet cell up, `breakingBand`
  rows. The feet row is read when the press anchors.

Modes are saved by name, append only (`heightBand` appended); the placement
mode setting offers the first four and enables nothing yet. The preview
radius is bounded to 0-16 cells; it bounds rendering work, not the allowed
region. No Minecraft pointers are stored.

### Breaking: press-anchored lifetime

- `PressAnchor` (pure, tested) holds the region for one attack press. The
  first block mined in a press anchors it (mode from the settings, axis from
  the mined face); a release or a new press ends it. The attack button state
  comes from vanilla's own button handlers through the Auto Attack/Use
  adapter (`periodic::attackButton()`), so Auto Attack's synthetic presses
  count too. A call made without a held press does not anchor and is allowed.
- Toggling, changing the mode, world exit and dimension change clear the
  anchor.
- Rejected blocks never end the physical hold: back on an allowed block,
  breaking continues. A rejected block is silent (no sound, no toast).
- While a press holds a region, the world overlay draws faint faces like a
  White shape over the allowed cells within four blocks of the anchor,
  leaving out the targeted block so the vanilla outline stays visible.
- The Status element shows the mode (and axis) whenever the restriction is
  on, and the anchor while a press holds one. Switching the element off
  (Automation status, or Hidden in HUD layout) hides these lines too
  (decided 2026-11-14: Hidden means nothing is drawn); the toggle toast
  still names the restriction.
- The old capture and reset actions are retired: their ids stay in
  `enum Action` (saved bindings) but they are not listed or dispatched.

### Mining session (L-73 B)

`MiningSession.cpp` owns the GameMode start/continue/stop hooks for the
client's own player and asks Breaking Restriction, Tool Protection and Tool
Switch in that order (`MiningSession.h`, pure and tested). The first answer
other than Proceed is applied and later features are not asked:

- End: a start returns false; a continue returns false, ending vanilla's
  session (a held button never restarts it).
- Pause: keep the session without progress; progress already made is aborted
  through vanilla's stop, and the next allowed call restarts through vanilla's
  start so the server gets a start action again.
- Restart: Tool Switch fetched a tool; start afresh on this block.

A restart the session makes is not a new press for Tool Protection, and the
pause's own stop is not a stop for Tool Switch. The integrated server's
player in a local world is left to vanilla. `destroyBlock` is still gated by
Breaking Restriction directly (creative instant breaks go through it).

### Placement boundary

Rules any placement gate must meet before it is connected:

- Validate the actual destination cell through vanilla placement semantics;
  clicked block plus face offset is wrong for replaceable blocks and special
  placements.
- Reject the whole placement before its first mutation when any required
  destination violates the region. Undoing client block writes after the
  server submission is not enough.
- A rejected operation does not change player position, inventory or block
  state and sends no placement/break packet.
- Do not cancel unrelated item use or container interaction because the hit
  block is out of range.

### Edge Guard

(Decided 2026-10-12, L-160/L-161.)

- Stops walking off edges like sneaking, without sneaking, also on servers:
  the move input the server reads is guarded, not only the client's motion.
- Stops with the box at least a margin over the block, also while sneaking
  (a box that only touches an edge is no support on a server, and a server's
  position can drift a few hundredths further out than the client's). The
  margin is "Stop inside the edge by", 0.02 to 0.30 block in 0.01 steps,
  default 0.10: smaller reaches further for placing blocks, larger is safer
  on servers (maintainer, 2026-10-12). Reconfirmed 2026-10-13 (L-162): keep
  the default and range; one successful BDS check at 0.02 does not establish
  the smaller margin's safety for all server corrections.
- A slab that would merge with the slab the player stands on and push them
  out sideways (their center beside that cell) is not placed; with the
  center over the cell the merge lifts the player and is allowed.

## Code

- `features/interaction/RestrictionRegion.h` (pure: modes, preview,
  `PressAnchor`), `RestrictionMode.h` (saved names and labels),
  `BreakingRestriction.*`.
- `features/interaction/MiningSession.*`, `MiningSessionHooks.h`.
- `features/interaction/EdgeGuard.*`, `EdgeGuardPlan.h` and the other
  `EdgeGuard*` plan headers (pure); capability notes in
  [GAME-UPDATES.md](GAME-UPDATES.md).
- Region faces drawn in `overlay/WorldOverlay.cpp` through the shared cell
  mesh ([OVERLAYS.md](OVERLAYS.md)).
- Tests cover all axes, negative coordinates, preview membership and work
  limits, unbounded predicate behavior, vertical modes, the height band,
  opposite faces, integer edges, the press anchor and the mining session
  steps. They do not prove hook or placement behavior.
- `xmake f --placement_trace=y` (then `xmake`; reset with `=n`):
  `features/interaction/PlacementTrace.cpp` logs entry/exit positions and
  return values of `Item::calculatePlacePos` and
  `Item::_sendTryPlaceBlockEvent` for the local player only, at most 200
  calls per enable, with increasing IDs that pair entries and exits and show
  nesting. The hooks call vanilla once and return its result. Normal builds
  do not install them; never distribute a trace build.

## Known

Placement paths in the SDK (26.51.3, read from headers and the LeviLamina
v26.51.3 event source for API semantics only):

- `Item::calculatePlacePos` and `BlockItem::_calculatePlacePos` take mutable
  face/position arguments. Chalkboard, hanging sign, sign, skull, frog spawn,
  water lily, redstone dust and other items have their own calculation;
  `BedItem` and `DoorItem` have their own use-on paths. A face-offset
  resolver or a `BlockItem`-only hook does not cover placement in general.
- `PlayerPlacingBlockEvent` is emitted from a block-permission check scoped
  to use-on processing; its position is the permission-check argument, not
  necessarily every final destination. Cancelling it rejects that check and
  could affect non-placement uses of the same permission API.
  `PlayerPlacedBlockEvent` is not cancellable and is wired to a try-place
  gameplay event; its name does not prove the placement has completed.
- Other candidates: `Item::_sendTryPlaceBlockEvent` (actor, block, source,
  position, `CoordinatorResult`) and `BlockType::tryToPlace` (source,
  position, block, optional sync message; no player argument). Headers alone
  prove neither pre-mutation ordering, special-item coverage nor atomic
  rejection of multi-cell placements.
- Lamium already has cancellable `GameMode::buildBlock` /
  `SurvivalMode::buildBlock` hooks in the detached-camera interaction guard
  (L-15 Next).

## Tried

- The capture/reset breaking design (explicit capture and reset keys):
  replaced by the press anchor; it occasionally stopped a held attack
  without explanation.

## Limits

- Placement is not restricted; the placement mode setting is preparation
  only. Before connecting a gate, trace in a local test world: ordinary
  solid placement, replaceable vegetation, slabs/snow, doors/beds, signs,
  redstone, and non-placement uses (opening a chest, a bucket), recording
  calculated, permission-check and try-place positions, cell changes and
  item consumption. The trace build has not been run yet.
- Do not claim packet suppression or complete enforcement from tests or
  local checks. Breaking on servers is unchecked.
- Edge Guard: slippery blocks slide further than the guard expects.
- Coverage: [VALIDATION.md](VALIDATION.md) rows "Breaking Restriction,
  press-anchored modes and height band (L-15 step 1)" and "Edge Guard (L-40,
  L-160..L-162)".
