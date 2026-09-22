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
