# Placement and breaking restrictions

The shared region predicate defines the proposed initial modes:

- Plane: fixes the anchor coordinate on the selected face-normal axis.
- Line: fixes the other two coordinates, extending along that axis.
- Column: fixes X/Z and extends vertically, independent of the selected face.
- Layer: fixes Y and extends horizontally, independent of the selected face.

The anchor is a block cell. Preview generation uses the same predicate as
action enforcement. Preview radius is bounded to 0–16 cells; this bounds rendering
work, not the allowed operation region. Integer coordinate edges retain the
headroom required by block-face geometry. No Minecraft pointers are stored.

## Integration work still required

The mode settings are exposed as independent named choices with immediate saving.
Breaking now has a default-off toggle and initially unbound capture/reset actions.
Enable it, then point at a block and invoke capture. With no anchor, breaking is
blocked and the HUD prompts for an anchor. Toggling, changing mode, world exit,
dimension transition and feature shutdown clear the session anchor. The world
preview shows a radius-four sample of the region; its geometry is cached by value.
Settings/input ownership gates capture actions through the shared action layer.
The initially unbound next-mode action cycles the same named option used by the
settings editor and clears the anchor through the same save path. The HUD shows
mode, effective axis and anchor state. Column and Layer report Y regardless of
the captured face. Placement enforcement and its anchor are not yet implemented.

Breaking hooks gate GameMode start/continue/destroy calls for the local client
player. Rejected calls return false and clear the destroyed output parameter when
present. Tool Switch also checks the predicate independently. The runtime call
coverage, creative instant break and cancellation behavior remain unverified.
Placement must
validate the actual destination cell; blindly adding a face offset is incorrect
for replaceable blocks and special placements. Resolve that through vanilla
placement semantics before connecting the placement gate. Do not cancel unrelated
item use or container interaction merely because the hit block is out of range.

World previews should use the existing block-grid geometry and explain the mode,
anchor and axis. Rejected operations must not mutate player position, inventory,
block state or send a placement/break packet. This is independent of fast placement.

## Evidence

Pure tests cover all axes, negative coordinates, preview membership/counts,
unbounded predicate behavior, vertical modes, opposite faces, preview work limits
and integer-edge handling. They do not prove Minecraft hook or placement behavior.
The breaking implementation builds and links, and catalog/settings/action tests
pass. Local-world validation remains outstanding: all modes/faces, creative and
survival, held-button target changes, anchor reset during mining, world exit,
dimension changes, and interaction with Tool Switch. Do not claim packet suppression
or complete enforcement until those native paths have been exercised.

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
