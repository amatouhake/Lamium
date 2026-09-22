# Placement and breaking restrictions

The shared region predicate defines the proposed initial modes:

- Plane: fixes the anchor coordinate on the selected face-normal axis.
- Line: fixes the other two coordinates, extending along that axis.
- Column: fixes X/Z and extends vertically, independent of the selected face.
- Layer: fixes Y and extends horizontally, independent of the selected face.

The anchor is a block cell. Preview generation uses the same predicate as future
action enforcement. Preview radius is bounded to 0–16 cells; this bounds rendering
work, not the allowed operation region. Integer coordinate edges retain the
headroom required by block-face geometry. No Minecraft pointers are stored.

## Integration work still required

This commit does not yet enforce restrictions or expose their settings. Placement
and breaking must have separate toggles and anchors, with mode switching, explicit
anchor capture/reset and a visible description of the active region. Anchors are
session state and must be invalidated on world/dimension changes and feature
shutdown. Settings/input ownership must suppress editing anchors while typing.

Breaking must check both initial and continued destruction, including creative
instant break, before any related tool switch or vanilla mutation. Placement must
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
Runtime integration and local-world validation remain outstanding.
