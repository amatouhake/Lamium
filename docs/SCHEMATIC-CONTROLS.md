# Schematic controls

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/SCHEMATIC-CONTROLS.md`.

How the player moves, turns and slices schematic placements and the save
area: the move target, view-directed steps, the radial menu, Repeat, the
cut-plane layers and how they are saved, and the looked-at selection.
Everything else about Schematics (files, ghosts, check, materials, area save,
screen) is in [SCHEMATIC.md](SCHEMATIC.md); shared control rules are in
[DESIGN.md](DESIGN.md#controls).

Settings (Schematics category): the menu opener ("Schematic menu") with
`schematic.menuBackground` (none / light / dark, default light),
`schematic.menuSmall` (small in the lower right, default on) and
`schematic.menuReopen` (reopen where it was closed, default off); Repeat
("Repeat the last adjustment", a Hold action) with
`schematic.repeatAllSteps` (default off). Every operation also has a
bindable action; the only default binding is Alt+Wheel up/down for
moving the target away/closer.

## Behavior

### Move target

- One move target shared by keys and the menu: the selected placement,
  corner 1, corner 2 or the save area. The six direction keys, the menu's
  movement steppers and the move toasts act on it. Rotate/mirror and the
  layer controls always act on the selected placement.
- "Select the next move target" (next to "Select the next placement" in the
  "Move target and placement" group) cycles only valid targets: the
  placement when one is selected, a corner when it is set, the area when
  both corners are set. With nothing valid there is an explicit **no
  target** state: direction keys toast that there is nothing to move and
  the target key names the next valid target when one appears.
- After a delete, area clear, visibility change or placement selection, the
  target is revalidated and falls to the first valid one in the order
  placement, corner 1, corner 2, area, else no target. World/dimension
  change and selecting or placing a placement reset it to the placement.
- Move to feet acts on the current target: the placement origin, corner 2,
  or the area (corner 1 to the feet, corner 2 follows). With no target it
  toasts. The help says "Move the target to your feet"; the action id stays.
- The view source: the player's feet normally, the camera's virtual feet
  (camera height minus the player's eye height) when Freelook or FreeCamera
  owns the view. Move to feet, Cut here and the screen's "Layer here" use
  it; corners are set at the block the view (or camera) hits.
- Display (decided): the target named in move toasts, in the schematic HUD
  when the HUD is on, and its frame or mark emphasized in the world; where
  placement and area frames overlap, the active one is drawn last and
  brighter or thicker. Not all of it is built (see Limits).

### View-directed steps

Decided with L-230: all six directions come from one camera frame. Forward
and back step along the view's dominant axis and move on Y past 60 degrees
up or down; left/right stay horizontal; up/down are right x forward, so
they are world Y with a level-ish view and take the horizontal direction
once forward turns vertical. Each step is decided per input, with no gesture
lock or hysteresis. A detached Freelook or FreeCamera view owns the
direction. Press actions can be bound to wheel chords (Alt with a wheel
direction); Hold actions cannot.

### Repeat the last menu adjustment

- Held with the wheel, Repeat repeats the kind of adjustment last used in
  the menu: movement, rotate/mirror or the layer cut. Key actions do not
  replace it, so a menu layer change stays repeatable after a key move.
- `schematic.repeatAllSteps` adds the layer axis, mode and keep side. The
  target picker and the placement selection are never repeated.
- The memory stores the kind, not the sign; the wheel direction picks the
  sign (after a back step, adjust + wheel up moves forward). The toast names
  the value and what it acts on.
- Cleared on target change, another placement selection, world/dimension
  change, feature disable and session reset. A failed or refused operation
  does not update it.

### Corners and default keys

- Corner 1 and corner 2 keep separate actions: a setter is often re-run
  after the view moves.
- Only Alt+Wheel up/down (move away/closer) is bound by default; Lamium binds
  few keys, and a wheel binding is hard to discover in the key settings. No
  corner defaults and no edit mode.
- With Schematics off the default chord is still consumed and shows the
  turn-it-on toast. Accepted; revisit when Schematics leaves Experimental or
  if it bothers non-users (gating the input on the feature is the
  alternative).
- The menu defaults to the small lower-right layout so it does not cover
  the crosshair.

### Layers: a cut plane

- A placement's layer view is **Axis** (Y / X / Z), **Cut**, **Keep**
  (low / high) and **Mode** (All / Only / Up to).
- The plane is attached to the **structure**: Cut is a structure-local offset
  from the low corner along the structure's own axis. Moving, rotating or
  mirroring the placement keeps the same build cells visible; the world plane
  turns with it. The screen shows the layer number and the extent.
- Up to shows cells at or below the cut (Keep low) or at or above it (Keep
  high); Only shows the one layer at the cut; All ignores the cut. Keep stays
  editable under Only/All. The Placed tab has its own Keep row.
- Changing the axis keeps the Cut, clamped to the new extent. Only Cut here
  moves the plane to the view source.
- Cut here keeps axis and keep, sets the cut to the view source's layer,
  then leaves All. Menu Cut steps, Layer up/down keys and the screen's Cut
  +/- first leave All without moving the cut; "Layer here" and a typed cut
  set the cut, then leave All.
- Leaving All returns to the mode in which the cut, Keep or Axis was last
  changed, Only by default (L-239). Modes only viewed or passed through and
  edits under All do not update that memory. It belongs to each placement
  for the world session, is not saved, does not enter the ghost key, and
  rejoining resets it to Only.
- Keys and menu announce the resulting mode when they leave All; the screen
  keeps its own message line.
- Check and Materials read the same cut region; a change restarts the check
  once. A single plane cannot express a compound cut (X here and Y here);
  that is a documented limit.

### Storage and migration

- The placements document stays `version: 1`. A placement carries
  `layerView: { space: "structure", axis: "y"|"x"|"z", cut: <offset>,
  keep: "low"|"high", mode: "all"|"only"|"upTo" }` in structure-local
  coordinates. The old `layerAxis` key is not reused: an older Lamium would
  read a different direction.
- The legacy `layerAxis`/`layerMode`/`layer` fields stay for older readers.
  Older Lamium ignores `layerView`; the current reader prefers it. A
  `layerView` without `space: "structure"` is ignored in favor of its old
  fields; the next layer edit writes the marked form.
- When the structure is known, the exact legacy fields are always written
  too, mapped through the current rotation and mirror (placed-box based: the
  low or high direction with `index` counted from that side), so an older
  Lamium shows the same cells.
- Never convert without the size: a placement whose structure is missing
  keeps its legacy fields and is written back unchanged; `layerView` is added
  at the next layer edit with the size known. Cut edits, and turning or
  resetting a placement already in the new form, are refused without the
  structure ("the layer view cannot be edited yet"). A still-legacy
  placement turns normally.
- A mixed file round-trips both forms; rollback reads the last legacy fields
  and shows the same view.

### Looked-at selection

"Select what you look at" chooses the move target:
- The ray is the drawn view: the detached camera when one owns the view,
  otherwise the player's eye.
- A corner wins when the ray passes through its block cell, even in air; the
  nearest corner entry wins; when both corners share a cell, keep the current
  corner if it is one, else corner 1.
- Otherwise the nearest placement box entry wins, then the save area when the
  ray enters its box. A placement inside the area is chosen over the area.
- A block that fully hides what is behind it occludes targets strictly
  behind it; glass, plants, water and lava do not. A target on, in front of,
  or overlapping the opaque block stays selectable.
- Setting a corner keeps using the game's crosshair hit in normal play, so
  the outlined block and the corner agree at seams.
- Nothing selectable: "No move target where you look".

### Menu and key parity

Decided with L-238 (maintainer: every schematic operation is easy from the
radial menu, from keys, or both; neither path is required):
1. **Either path completes every operation.** Each operation has a bindable
   action and a menu route. Keys reach corners and the area through the
   looked-at selection and the target cycle, so there is no per-target key.
2. **Accelerators need no counterpart.** Opening the menu or the screen and
   Repeat speed up the menu path; cycling the target speeds up the key path.
   The Check rules (L-262, L-303) stay in the settings with optional keys
   and have no menu item.
3. **One vocabulary.** Menu and key settings share names, groups and order.
4. **The menu teaches the keys.** The hovered item's bound keys show above
   the ring, with the Repeat pointer (in the center a long chord would widen
   the whole ring).

The menu is two levels: categories, then their items. Click runs an item,
the wheel over a stepper changes it, right click goes back, the menu key or
Esc closes it. The game keeps running; the menu only takes the mouse. It
opens at the category list, or with `menuReopen` where it was closed.
Items spread out from 55% of the ring over 0.15 s with ease-out, following
the Animations setting. Names, file picking and delete confirmation stay on
the screen; absolute X/Y/Z are not in the menu.

| Category | Items |
|---|---|
| Move | Forward/back, Left/right, Up/down, To feet, Move target, Select what you look at |
| Placement | Selected placement (stepper), Rotate, Mirror, Reset rotation and mirror, Move placement → |
| Layers | Axis, Cut, Mode, Keep, Cut here |
| Show and check | Schematics on/off, Show selected placement, Entities, Extra blocks, Mistake marks, Nearest mistake |
| Area | Corner 1 at the looked block, Corner 2 at the looked block, Move area →, Save, Clear |
| Screen | Place from a file…, Placed list…, Check tab, Materials tab, Schematic HUD, Key settings |

- Five or six items per category (eight categories of three to seven felt
  sparse or crowded). "Move placement/area →" set the target and open Move.
- With Schematics off the menu opens at Show and check with a toast and runs
  only the feature switch, the HUD switch and the screen items
  (`worksWhileOff`); schematic keys toast how to turn the feature on.
- Menu items are not saved ids; reordering only moves the remembered open
  level.
- Key settings: the switch rows at the top (Schematics, HUD, menu, Repeat,
  screen), then groups in the menu's order and names: Move, Placement,
  Layers, Show and check, Area, Screen, plus Check for the rule switches.
  Groups are display only; saved bindings are unaffected.
- No reverse keys for Mirror, Axis, Mode or Keep: they cycle two or three
  states.
- Placement toggle toasts name what changed ("Show placement on").

Long names: every ring plate takes the width of the widest label of all
categories (`renderSchematicMenu`) and the radius grows with it
(`RadialLayout::at`), unclamped to the screen. Menu labels are separate
translation keys from the key-setting labels and stay short in every
language; a pure test sizes the ring for the widest label at a 480x253 GUI
screen.

## Code

- `src/features/schematic/SchematicActions.cpp`: key presses (`press`), menu
  steps (`step`) and commands (`run`), the move target and the Repeat memory
  (`remembered`), lifecycle resets.
- `MoveTarget.h` (valid targets, cycling), `MoveDirection.h` (view frame),
  `TargetRay.h` (corner/area entry, occlusion): pure, tested.
- `Placement.h`: mirror then clockwise quarter turns, the cut plane
  (`cutAlong`, `withAxis`, `leaveAll`, `noteLayerEdit`), legacy conversion
  (`fromLegacy`, `dualWrite`). `PlacementStore.cpp`: `schematics.json`.
- `MenuModel.h`: categories, items, the keys each item names,
  `worksWhileOff`, `keyOnlyAccelerator`. `src/ui/RadialLayout.h`: ring
  geometry, hit test, `spread`. `src/ui/SettingsScreen.cpp`
  (`renderSchematicMenu`) draws it.
- Actions in `src/input/Binding.h` (groups `schematicMove`,
  `schematicPlacement`, `schematicLayers`, `schematicShow`, `schematicSave`,
  `schematicScreen`, `schematicCheck`); the default chord is set there.
- `tests/SchematicTests.cpp`: the above, every-key-has-an-item, the ring fit,
  and the old-to-new layer conversion showing the same cells.
  `LAMIUM_MIGRATE_PLACEMENTS` (a world's `lamium/schematics.json`) with
  `LAMIUM_MIGRATE_SCHEMATICS` (`mods/Lamium/schematics`) makes `LamiumTests`
  report each placement's old/new visible-cell counts and dual-write result.

## Known

- The ghost key covers only visible layer fields (All ignores the cut, Only
  ignores Keep, Up to uses both), so editing an unused field saves without
  rebuilding ghosts. Moving the cut still rebuilds the whole placement.

## Tried

- Six combined directions plus a zero-based index as the layer model: the
  direction decided both the numbering origin and the "up to" side, hard to
  read and unfit for view-based control. Replaced by the cut plane (L-234).
- A "View slice" command: it overlapped Axis and Cut here, changed Only/Keep
  as a side effect, and the axis has only three values. Removed; an "Axis
  from view" shortcut can be considered separately.
- Direction keys that always moved the placement while the menu moved its own
  target: the same directions meant different things. One shared target now.
- Repeat that recorded layer axis and mode by default: a wheel turn changed
  the layer model by accident.
- Reusing `layerAxis` for the new model: older Lamium would misread it.

## Limits

- Not built from the display rule: the target line in the schematic HUD and
  the emphasized target frame in the world ([VALIDATION](VALIDATION.md),
  Schematics row).
- Up to rebuilds every section of a large placement on each cut step:
  [L-236 Rebuild only the ghost sections whose visible layer changed](backlog/L-236.md).
- Shared controls with other features: [L-111 Integration between features: remaining shared controls](backlog/L-111.md).
- Out of scope: item-based operation, a persistent edit mode, a layer peel
  in the 3D previews' own controls (that is [L-114 3D previews in the schematic screen](backlog/L-114.md)).
- In-game coverage: [VALIDATION.md](VALIDATION.md), Schematics row.
