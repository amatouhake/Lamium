# Schematic controls redesign

Status: design discussion, 2026-10-20, all recommendations adopted. L-231 is
implemented and merged (PR #36, merge commit `780bfba`); its in-game checklist
passed on 2026-10-21 (build `89c7925`). L-234 is merged (PR #37, merge commit
`75152f1`) after in-game checks. L-236 (per-section ghost reuse) is a
follow-up; L-237 (missing-structure warning) is done. L-238 (menu and key parity,
2026-10-22) is implemented and its in-game check passed (`41d3ea7`). This note records the
agreed direction; the items under "Open questions" remain undecided.

[SCHEMATIC.md](SCHEMATIC.md) still describes the behavior in the game today;
[BACKLOG.md](BACKLOG.md) owns task status (L-231 controls, L-234 layer model,
L-238 menu and keys, L-239 mode memory).

## Problems seen in game

- The six direction keys always move the selected placement, while the radial
  menu moves a separate move target (placement, corner 1, corner 2, save
  area). The same directions mean different things on the two surfaces.
- Corner and area targets cannot be moved from keys, and the move target is
  invisible once the menu closes.
- "Repeat the last adjustment" records only menu steppers and includes the
  layer axis and mode, so a wheel turn can change the layer model by accident.
- Layers are six combined directions plus a mode and a zero-based index from
  the chosen side. The direction decides both the numbering origin and the
  "up to" side, which is hard to read and does not fit a view-based control.
- The radial menu's Area category holds seven items, more than any other
  child category.
- Every schematic key is unbound, so the quick paths are hard to find.

## Adopted direction

### Move target

- One move target shared by keys and the menu: placement, corner 1, corner 2,
  the save area.
- One Press action cycles the target; the looked-at selection can also set it.
- The six direction keys, the menu movement steppers and the move toasts act
  on the target. Rotate/mirror and the layer controls stay settings of the
  selected placement.
- The move-to-feet command uses the view source: the player's feet normally
  and the camera's virtual feet (its height minus the player's eye height) in
  Freelook/FreeCamera. Layer-here follows the same rule, and corners are set at
  the block the view (or camera) hits.
- The settings group is "Move target and placement"; the target key is
  "Select the next move target", placed next to "Select the next placement".
- The move-to-feet key acts on the current target like the menu's item: the
  placement origin, corner 2, or the area (corner 1 to the feet and corner 2
  follows). With no target it toasts. The help says "Move the target to your
  feet"; the action id stays.
- Display: the target name in the schematic HUD when it is on, the target's
  frame or mark emphasized in the world, and the target named in move toasts.
  The HUD is optional, so the HUD-off case relies on the frame/mark and the
  toasts. Where the placement and area frames overlap, the active frame is
  drawn last with a brighter or thicker stroke.
- Cycle only valid targets: the placement when one is selected, a corner when
  it is set, the area when both corners are set. An explicit **no target**
  state exists when nothing is valid.
- After a delete, area clear, visibility change or placement selection,
  revalidate the target and take the first valid one in this order: placement
  (when selected), corner 1, corner 2, area, else no target.
- With no target, the direction keys toast that there is nothing to move and
  the HUD shows `Move target: none`; the target key cycles to the next valid
  target and names it.
- Reset to the placement on world/dimension change and when a placement is
  selected or placed.
- The looked-at selection extends "select the placement you look at" with the
  ray and distance rules below.

### Repeat the last menu adjustment

- Repeats the adjustment last used in the schematic menu: movement,
  rotate/mirror and the layer index. Key actions do not replace it, so a menu
  layer change stays repeatable after a key move.
- The option **Repeat every menu step** (`schematic.repeatAllSteps`, default
  off) adds the layer axis, mode and keep side. The target picker and
  placement selection are never repeated.
- The memory stores the **kind** of adjustment, not its last sign; the wheel
  direction chooses the sign. After a back step, adjust + wheel up moves
  forward. The toast names the value.
- Clear the memory on target change, another placement selection,
  world/dimension change, feature disable and session reset, so a rotate done
  on the placement cannot repeat after the context changed. A failed operation
  does not update it; stopping the feature clears it.
- A successful view slice sets the memory to the layer cut (L-234), so the next
  adjust plus wheel moves the cut.
- Rotate, mirror and layer operations still act on the selected placement; the
  repeat toast names the operation and what it acts on.

### Corners and default keys

- Corner 1 and corner 2 keep separate actions. A setter is often re-run after
  the view moves, so merging them would hurt.
- Default binding only for forward/back: Alt + Wheel up/down. No corner
  defaults and no edit mode in this round.
- While Schematics is off, the default chord is still consumed and shows the
  turn-it-on toast. Accepted for this round; revisit when Schematics leaves
  Experimental or if it annoys non-users (gating the input on the feature
  switch is the alternative).
- The schematic menu defaults to the small lower-right layout
  (`schematic.menuSmall` default on), so it does not cover the crosshair.

### Layers: a cut plane

- Replace the six-direction model with **Axis** (Y / X / Z), **Cut**,
  **Keep** (low/high) and **Mode** (all / only / up to).
- The plane is attached to the **structure**: Cut is a structure-local offset
  from the low corner along the structure's own axis. Moving, rotating or
  mirroring the placement keeps the same build cells visible; the world plane
  turns with the placement. The screen shows the layer number and the extent;
  no world coordinate is stored.
- **Keep** is used by "up to": low shows cells at or below the cut, high at or
  above. "Only" shows the one layer at the cut; "all" ignores the cut. The
  placed screen has its own Keep row, so both sides can be chosen without the
  radial menu.
- The Layers menu is **Axis / Cut / Mode / Keep / Cut here**. Show all is
  dropped because Mode already has All. Keep is a stepper (low / high); in
  only/all it has no visible effect but stays editable for "up to".
- Cut here keeps the axis and keep, sets the cut to the view source's layer
  along that axis (the player's feet in normal play, the camera's virtual feet
  in FreeLook/FreeCamera), then leaves All for the mode in which the cut,
  Keep or Axis was last changed (Only by default; L-239, checked in game on
  `fce4b99`). Modes only viewed or passed through and edits under
  All do not update the memory. The memory belongs to each placement for the
  world session and is not saved.
- Menu Cut steps, Layer up/down keys and screen Cut +/- first leave All
  without moving the cut. The screen's match-layer command and typed cut
  set the cut, then leave All like Cut here. A key/menu cut operation that
  leaves All announces the resulting mode; the screen keeps its own message
  line. Rejoining forgets the remembered mode and defaults to Only.
- Changing the axis **keeps the Cut**, clamped to the new extent. Only Cut here
  moves the plane to the view source.
- **View slice is removed** (2026-10-21): it overlapped Axis and Cut here,
  changed Only/Keep as a side effect, and the axis has only three values. A
  later "Axis from view" shortcut can be considered separately.
- A single plane cannot express a compound cut (for example X here and Y
  here). This is a documented limitation, not something this round adds.
- Check and Materials read the same cut region; a change restarts the check
  once.

### Storage and migration

- Keep the document at `version: 1`. Add a separate per-placement object,
  `layerView: { space: "structure", axis: "y"|"x"|"z", cut: <offset>,
  keep: "low"|"high", mode: "all"|"only"|"upTo" }`, in **structure-local**
  coordinates. The old six-direction `layerAxis` key cannot be reused: an older
  Lamium would read a different direction.
- A `layerView` without `space: "structure"` (written by the first cut-plane
  build) is ignored in favor of its old fields, which carry the same view; the
  next layer edit writes the marked form.
- Leave the legacy `layerAxis`/`layerMode`/`layer` fields for older readers.
  Older Lamium ignores `layerView`; the new reader prefers it.
- When the structure is known, **always** dual-write the exact legacy fields.
  They are placed-box based, so the structure-local plane is mapped through
  the current rotation and mirror: low/only uses the low or high direction
  with `index` counted from that side. An older Lamium then shows the same
  cells under the same placement.
- Never convert without the size: a placement whose structure is missing keeps
  its legacy fields and is written back unchanged; other placements can still
  be saved. `layerView` is added when the layer view is next edited while the
  size is known; unrelated saves keep the legacy fields.
- Cut edits are allowed only while the size is known, so the legacy fields are
  never stale. Without the size the screen says the layer view cannot be edited
  yet. The same applies to turning or resetting a placement whose cut plane is
  in the new form: the old `layer` index depends on the extent and the
  rotation, so the command refuses without the structure. A still-legacy
  placement can turn normally because old and new readers both use its old
  fields.
- A mixed file round-trips both forms. Rollback to an older Lamium reads the
  last legacy fields and shows the same view.

### Looked-at selection

- Use the same drawn view ray as movement: the detached camera when one owns
  the view, otherwise the player's eye and view.
- A corner wins when the ray passes through its **block cell**, even when the
  cell is air. The nearest corner entry wins; when both corners share a cell,
  keep the current corner if it is one, else corner 1.
- Otherwise the nearest placement box entry wins; then the save area when the
  ray enters its box (in the air too). A placement hit inside the area is
  therefore chosen over the area.
- A block that fully hides what is behind it occludes these targets; glass,
  plants and other see-through blocks do not. A target on, in front of, or
  overlapping the opaque block stays selectable; only a block strictly between
  the view and the target hides it. Water and lava are looked through.
- Rebuilding a ray from the eye is exact for the cell tests; setting a corner
  keeps using the game's crosshair in normal play so the outlined block and the
  corner agree at seams.
- Looking at nothing selectable reports "No move target where you look".

## Third review (2026-10-20, adopted)

The four points were adopted the same day and are folded into the sections
above; nothing remains proposed.

## Menu and key parity (2026-10-22, adopted, L-238)

Goal (maintainer): the schematic operations are easy to understand and use
from the radial menu, from keys, or both, as the player chooses. Neither path
is required for any operation.

### Principles

1. **Either path completes every operation.** Each operation has a bindable
   action and a menu route. An operation counts as reachable when its path
   achieves it, directly or not: keys reach corners and the area as move
   targets through the looked-at selection and the target cycle, so no
   per-target key is added.
2. **Accelerators need no counterpart.** Opening the menu and Repeat speed up
   the menu path; cycling the move target speeds up the key path. Repeat
   stays a key only: it exists to avoid reopening the menu.
3. **One vocabulary.** The menu and the key settings use the same names, the
   same groups and the same order.
4. **The menu teaches the keys.** A menu item shows its bound key, if any.

### Menu: six categories of five or six items

Eight categories of three to seven items felt either sparse or crowded; the
target is five or six per category.

| Category | Items |
|---|---|
| Move | Forward/back, Left/right, Up/down, To feet, Move target, Select what you look at |
| Placement | Selected placement (stepper), Rotate, Mirror, Reset rotation and mirror, Move placement → |
| Layers | Axis, Cut, Mode, Keep, Cut here |
| Show and check | Schematics on/off, Mistake marks, Show selected placement, Entities, Extra blocks, Nearest mistake |
| Area | Corner 1 at the looked block, Corner 2 at the looked block, Move area →, Save, Clear |
| Screen | Place from a file…, Placed list…, Check tab, Materials tab, Schematic HUD, Key settings |

L-266 (2026-10-26): the HUD switch moved from Show and check to Screen to
make room for Mistake marks; the three Check rules (signal, on/off as a
wrong state, plant growth) stay out of the menu, in the settings with an
optional key.

- Turn merges into Placement: rotation and mirror are settings of the
  selected placement.
- Area drops "Move corner 1/2 →"; the Move category's target stepper and the
  looked-at selection choose a corner.
- Check splits: Extra blocks and Nearest mistake go to Show and check; the
  tab openers go to Screen. The duplicated Check and Materials tab items in
  Screen go away; "Place from a file…" and "Placed list…" (was "Delete a
  placement…") stand for the Files and Placed tabs.
- Renames: "This placement" becomes "Show selected placement"; "Corner 1/2
  here" names the looked block (the command always used it).
- Menu items are not saved ids, so reordering only moves the remembered
  open level.

### Keys: the same groups plus General

General (open the menu, open the Schematics screen, Repeat), then Move,
Placement, Layers, Show and check, Area, Screen, in the menu's order. Groups
are display only; saved bindings are unaffected.

New actions, appended and unbound: Reset rotation and mirror, Rotate
counterclockwise, Previous placement, Show selected placement, Extra blocks,
Entities, Clear area, Open key settings. No reverse keys for Mirror, Axis,
Mode or Keep: they cycle two or three states, so the forward key reaches
every state in one or two presses.

Default keys stay Alt+Wheel only (move the target away/closer). Lamium binds
few keys by default; that one exists because a wheel binding is hard to
discover in the key settings.

### Long names

Every ring plate takes the width of the widest label of all categories
(`renderSchematicMenu`), and the ring radius grows with that width
(`RadialLayout::at`), so one long label enlarges the whole ring in that
language. The ring is not clamped to the screen. Keep menu labels short in
English, Japanese and Chinese (they are separate keys from the longer key
setting labels), and add a pure check that the ring for the widest label
still fits a small GUI screen (for example 480x253) before relying on it.

### As built (2026-10-22)

- Each menu item names its keys in `MenuModel.h`; a test checks that every
  schematic key has an item and every item a key, except the accelerators
  (menu, screen, Repeat; "Move placement/area →").
- The hovered item's bound keys show **above the ring**, with the Repeat
  pointer, not in the center: a long chord would otherwise widen the center
  and with it the whole ring.
- The Schematics on/off and HUD keys stay on their switch rows at the top of
  the key settings (with the menu, Repeat and screen keys); the key groups
  below follow the menu: Move, Placement, Layers, Show and check, Area,
  Screen. Group names reuse the menu's category labels.
- The long-name check estimates text (ASCII about 6 units, CJK about 9) and
  sizes the ring as the menu does; at 480x253 it failed for English, so the
  longest labels became "Current placement", "Move placement >", "Looked-at
  target" and "Place a file...".
- Placement-toggle toasts now name what changed ("Show placement on").

## Open questions

- L-236 (per-section ghost reuse) is a separate follow-up; L-237
  (missing-structure warning) is done. L-238 is done; finer adjustments are expected only
  from use.

## Suggested order

From the 2026-10-20 reviews, for the maintainer to confirm:

1. **L-231 first (done)**: move target, target display and repeat cleanup with
   the current layer model, so the operation feel can be checked before the layer
   model changes. Merged in PR #36.
2. **L-234 as a separate step**: prove with pure logic that the conversion from
   every old direction and mode shows the same cells, then implement the cut
   plane and the storage form. Merged in PR #37 (2026-10-22).
3. **L-238 menu and key parity** (2026-10-22): one set of operations for
   the radial menu and the keys; see "Menu and key parity".
4. **Then inspection work**: L-183's single-row ignore first; L-190's
   multi-selection after it, sharing only the parts that fit. L-185 stays a
   separate, larger task.

## Not in this round

- Item-based operation and a persistent edit mode.
- The Files/Check 3D preview's own layer peel.
- Other check or material semantics.

## Files likely touched

`Placement.h`, `PlacementStore.cpp`, `GhostBlocks.cpp`, the verification and
materials paths, `MenuModel.h`, `SchematicActions.cpp`, `SchematicsView.cpp`,
the three-language help, and `tests/SchematicTests.cpp`.

## Review

Three independent reads were returned on 2026-10-20. The second found the
`layerAxis` key collision in the first migration proposal, the missing repeat
setup after a slice, target invalidation after delete/clear, and the
distance/view-source gap in the looked-at selection. The third found the
corner-inside-the-box selection gap, the missing move-to-feet unification, the
undefined Cut-here/axis handover, and the incomplete repeat clearing. All are
adopted above. A prototype may live on a separate branch; main keeps today's
behavior until the design settles.
