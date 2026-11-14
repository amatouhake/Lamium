# Placement and breaking restrictions

Breaking follows the press-anchored design of
[BACKLOG.md](BACKLOG.md#l-15-breaking-and-placement-restrictions) (L-15,
step 1, built 2026-10-07). Placement is not implemented; its research notes
are below. Current runtime coverage is in [VALIDATION.md](VALIDATION.md).

The shared region predicate (`RestrictionRegion.h`) defines the modes:

- Plane: fixes the anchor coordinate on the mined face's normal axis.
- Line: fixes the other two coordinates, extending along that axis.
- Column: fixes X/Z and extends vertically, independent of the face.
- Layer: fixes Y and extends horizontally, independent of the face.
- Height band (breaking only): the rows from the feet cell up, `breakingBand`
  rows (default 2, 1-16). The feet row is read when the press anchors.

Modes are saved by name (`heightBand` appended); placement offers the first
four. Preview radius is bounded to 0-16 cells; this bounds rendering work, not
the allowed region. No Minecraft pointers are stored.

## Breaking: press-anchored lifetime

`PressAnchor` (pure, tested) holds the region for one attack press. The
first block mined in a press anchors it (mode from the settings, axis from
the mined face); a release or a new press ends it. The attack button state
comes from vanilla's own button handlers through the Auto Attack/Use adapter
(`periodic::attackButton()`), so Auto Attack's synthetic presses count too.
A call made without a held press does not anchor and is allowed. Toggling,
changing the mode, world exit and dimension change also clear the anchor.
The capture and reset actions are retired: their ids stay in `enum Action`
(saved bindings), but they are not listed or dispatched.

While a press holds a region, the world overlay draws faint faces like a
White shape over the allowed cells within four blocks of the anchor, leaving
out the targeted block so the vanilla outline stays visible. The Status
element shows the mode (and axis) whenever the restriction is on, and the
anchor while a press holds one; switching the element off (Automation status,
or Hidden in HUD layout) hides these lines too (decided 2026-11-14: Hidden
means nothing is drawn; the toggle toast still names the restriction). A rejected block is silent.

## Mining session (L-73 B)

`MiningSession.cpp` owns the GameMode start/continue/stop hooks for the
client's own player and asks Breaking Restriction, Tool Protection and Tool
Switch in that order (`MiningSession.h`, pure and tested). The first answer
other than Proceed is applied and later features are not asked:

- End: a start returns false; a continue returns false, ending vanilla's
  session (a held button never restarts it, L-36).
- Pause: keep the session without progress; progress already made is aborted
  through vanilla's stop, and the next allowed call restarts through vanilla's
  start so the server gets a start action again.
- Restart: Tool Switch fetched a tool; start afresh on this block.

A restart the session makes is not a new press for Tool Protection, and the
pause's own stop is not a stop for Tool Switch. The integrated server's
player in a local world is left to vanilla. `destroyBlock` is still gated by
Breaking Restriction directly (creative instant breaks go through it).

## Placement boundary

Placement must
validate the actual destination cell; blindly adding a face offset is incorrect
for replaceable blocks and special placements. Resolve that through vanilla
placement semantics before connecting the placement gate. Do not cancel unrelated
item use or container interaction merely because the hit block is out of range.
Rejected operations must not mutate player position, inventory, block state or
send a placement/break packet.

## Evidence

Pure tests cover all axes, negative coordinates, preview membership/counts,
unbounded predicate behavior, vertical modes, the height band, opposite faces,
preview work limits, integer-edge handling, the press anchor and the mining
session steps. They do not prove Minecraft hook or placement behavior.
The capture/reset design had local-world evidence for breaking and
resume-after-rejection (L-36, rechecked 2026-09-30), with an occasional held
attack that stopped breaking unexplained. The press-anchored design and the
shared mining session (2026-10-07) passed the in-game checklist in a local
world on 2026-10-08 (VALIDATION-LOG); servers are unchecked. The faces
z-fight with the blocks under them (L-110). The mode set and keys are being
rethought before placement (L-15). Placement enforcement remains unimplemented. Do not claim packet
suppression or complete enforcement from the tests or partial local checks.

## Placement integration research (SDK 26.51.3)

The SDK exposes Item::calculatePlacePos and BlockItem::_calculatePlacePos with
mutable face/position arguments. Chalkboard, hanging sign, sign, skull, frog spawn,
water lily, redstone dust and other items have specialized calculation paths.
BedItem and DoorItem also implement their own use-on paths. A face-offset-only
resolver or a hook on BlockItem alone therefore does not establish general coverage.

The official [v26.51.3 placement event implementation](https://github.com/LiteLDev/LeviLamina/blob/v26.51.3/src/ll/api/event/player/PlayerPlaceBlockEvent.cpp)
was inspected to establish API semantics, without importing its implementation.
PlayerPlacingBlockEvent is emitted from a block-permission check scoped to use-on
processing. Its position is the permission-check argument; the event name does
not prove that it represents every final destination cell. Cancelling it rejects
that permission check. Broad cancellation here could also affect non-placement
uses that consult the same permission API. PlayerPlacedBlockEvent is not
cancellable and is wired to a try-place gameplay event, so its name must not be
used as proof that a placement has already completed.

Other candidates in the installed SDK are Item::_sendTryPlaceBlockEvent (actor,
block, source and position with CoordinatorResult) and BlockType::tryToPlace
(source, position, block and optional sync message). The latter lacks a direct
player argument. Neither header alone proves pre-mutation ordering, all special
item coverage, or atomic rejection for multi-cell placements.

Before connecting enforcement, observe these paths in a local test world for
ordinary solid placement, replaceable vegetation, slabs/snow, doors/beds, signs,
redstone, and non-placement uses such as opening a chest or using a bucket. Record
calculated position, permission-check position, try-place position, cell changes
and item consumption. The desired gate must reject the whole placement before
its first mutation when any required destination violates the selected region.
A gate that only undoes client block writes after server submission is insufficient.
Placement remains unimplemented until a suitable path is established; the existing
mode setting is preparation and does not enable a partial restriction.

## Opt-in placement observation build

`xmake f --placement_trace=y` followed by `xmake` builds local placement diagnostics.
It logs entry/exit positions and return values for Item::calculatePlacePos and
Item::_sendTryPlaceBlockEvent. Only local-player calls are recorded, capped at 200
calls per enable; monotonically increasing IDs pair entries/exits and expose
nesting order. Logs remain in the mod's existing local log file. The hooks call
vanilla exactly once and return its result; they do not decide placement validity.

This is partial observation, not a coverage claim: bypassed overrides, inventory
consumption and final block mutations still require observation in the test world.
Use ordinary items and special placements from the matrix above. Compare the
calculated position with the event position and visible destination. Do not infer
whole-operation atomicity from a single callback or successful build.

Restore a normal build with `xmake f --placement_trace=n` then `xmake`. The option
is off by default; normal builds do not install these hooks. Do not distribute a
trace build as a normal release. Runtime trace collection is still pending.
