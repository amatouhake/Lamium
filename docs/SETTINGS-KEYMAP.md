# Settings and keymap roles

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/SETTINGS-KEYMAP.md`.

Where a feature's switch, options and keys sit in the settings screen, how
key bindings match and conflict, and the resets. The general rules (one
switch per feature with its toggle key, new actions unbound, key caps,
steppers, tips, resets need a second press) are in [DESIGN.md](DESIGN.md);
this document adds the detail.

The exact inventory is in the catalogs, not copied here: feature and action
rows in `src/ui/SettingsRows.h`, options in `src/settings/Options.h`,
append-only action ids and default keys in `src/input/Binding.h`.

## Behavior

### Rows (L-83, decided 2026-10-01)

| Row | Examples | Key on the parent row | Keys on child rows |
|---|---|---|---|
| 1. Feature with a saved switch | Night Vision, Minimap, Sorting, Waypoints | Toggles that switch. **Always provided**, unbound by default | The feature's commands (row 4) |
| 2. Session feature (no saved state) | Zoom, Freelook, FreeCamera, permanent sneak | Starts/stops it (hold or toggle, per its Activation) | Commands such as speed |
| 3. Named command without a switch | Settings screen, Schematic menu | Runs that command | none |
| 4. Command inside a feature | Sort now, Add here, open its screen, cycle a mode, hold-to-do | never on the parent | one row per command |
| 5. Child switch or choice | Hide rain and snow, Breaking restriction, Show in the world, Cave view | none | on the same row, only when changing during play is useful |
| 6. Heading only | Map text, Block restrictions | none | none |

- A key that opens a screen belongs to the feature that owns the screen
  (row 4). Screens owned by no feature (Hotkeys, HUD layout) stay under
  General > Settings screen. The sidebar's pinned items open every screen
  with the mouse.
- A toggle key shows the toggle toast.
- Hotkeys lists every action, unbound commands included.
- Moving an action to another row changes only presentation; ids and
  existing bindings stay.

### Resetting settings

(Decided 2026-09-26, L-46.)

- No per-setting or per-feature reset (vanilla has none either). "All" has
  "Reset all" (every setting, key bindings and HUD layout; not Shapes), each
  category (Camera & view, Inventory, ...) has "Reset category" (its features'
  settings and HUD placement; not key bindings), and Hotkeys has "Reset all
  keys". They sit on the column-heading line and need a second press.
  The HUD layout editor keeps its per-element and all-element resets.
- (Decided 2026-10-12, L-159) Hotkeys has "Find by key" on the same line,
  right next to the key column, with "Reset all keys" to its left (the
  destructive button further from the frequent one): press it, then a key,
  mouse button or combination; the list shows only actions whose binding
  uses every input pressed (Z finds Z and Ctrl+Z). The search field then
  has focus and shows "Key: Z" as one unit: Backspace clears it, typing
  starts a text search instead, Esc clears it, Down/Enter go to the results.
  Typed text search does not match key names (one letter would match too
  many names).
- (Decided 2026-10-12, L-159, [demos/binding-editor.html](demos/binding-editor.html)
  A) While a key is being assigned, Clear / Reset / Cancel sit on the edited
  row, ending just before its key cell; they take the place of that row's
  switch or stepper until the edit ends. The footer keeps the action's
  description and the "press, then release to assign | Esc: cancel" line.

### Keys

- Lamium owns its key bindings; Minecraft's keyboard settings do not list
  Lamium actions (L-23). Clear means explicitly Unbound; Reset returns to
  Lamium's default. The Settings action cannot be cleared.
- Default keys exist only where a convention exists or the action is used
  constantly; defaults are decided per feature, there is no general policy.
  Current defaults: `L` settings (L-01, replacing F8), `C` zoom, `R` sort,
  `M` world map, `Tab` player list (hold), `F3` Debug View, `F3+B` Hitboxes,
  `F3+G` Chunk Borders (Java's debug keys, 2026-09-27), right button Fake
  Offhand use, `F` swap with offhand (2026-10-06, Java's key; Bedrock has
  none), `Alt+Wheel` move the schematic target away/closer (2026-10-21).
  Record the date when one is added.
- Night Vision is unbound: `J` had no mnemonic and `N` is Bedrock's
  notifications key. Saved bindings, explicit unbinds included, win over
  changed defaults; no saved binding or action id is rewritten.
- New action ids are appended, never reordered.
- The Settings key also closes (L-188): when the Settings binding is a
  single key, pressing it on a settings page (the table, Shapes, Waypoints,
  Schematics) closes the screen like Esc. It does nothing special while text
  or a number is being edited, the search field has focus or a key is being
  captured; the world map, radial menu, HUD editor, prompts, calculator and
  language list keep their own keys. A binding on Backspace, Tab, Enter,
  Esc, Space or an arrow keeps the screen's meaning. Key repeats are
  ignored, including a key still held from opening the screen.

### Chord matching (L-32)

- A chord is stored in press order; its last input completes it. Actions
  activate only on that completing press, never while keys merely stay held.
- Ordinary chords are order-sensitive and a more specific completed chord
  suppresses an overlapping shorter one for that activation: with `B` and
  `F3+B`, F3 then B fires only `F3+B`; B first may fire B at once and is not
  delayed in case another key follows.
- Modifier-like actions (Zoom, Freelook) tolerate unrelated held gameplay
  inputs so they work while moving. Both kinds tolerate unrelated held
  inputs (walking with W still allows another toggle); ordinary chords
  differ by order and by yielding. Matching mode is an action property, not
  a user option; do not add a second advanced keybind system.
- The most specific completed chord wins for everyone, modifier-like actions
  included (Ctrl+C beats C when Ctrl is held). A chord that yielded stays
  silent until one of its keys is released: no late firing on key repeat.
- When a longer chord starts on top of an active ordinary one (B held, then
  B + click), the shorter one is released and does not resume. Active
  modifier-like actions (Zoom) keep running.
- Exact duplicate bindings are valid: all enabled actions on that chord fire
  together (grouped toggles), never silently prioritized or disabled. The UI
  warns about duplicates and subset/superset overlaps. Mouse, keyboard and
  wheel combinations follow the same rules; Press/Hold/Toggle is separate
  from matching.
- (Decided 2026-09-25) Keys that begin a longer chord act on release, like
  Java's F3. A Press/Toggle action whose chord leads another bound chord (F3
  with F3+B bound) does nothing on press and fires on release, unless a
  longer chord containing it fired while it was held. Unrelated keys do not
  cancel it; focus loss, menus and text input drop it silently. Derived from
  the bindings, not a user option. Actions without such a longer chord fire
  on press; Hold actions (Zoom, Freelook) always act on press.
- Settings files written before L-32 stored chords sorted by code. They load
  in the order modifiers, function keys, other keys, mouse, wheel (a saved
  F3+B stays F3 then B); new files carry `orderedBindings: true`.

### Conflict display

- (Decided 2026-09-25, [demos/hotkey-conflicts.html](demos/hotkey-conflicts.html))
  Every key cell (Hotkeys and feature rows) draws related bindings with
  warning caps (DESIGN). No text badge beside the keys (it looked like
  another key cap). The tip lists every related binding grouped as same key /
  starts with this key / longer chords containing it / shorter chords inside
  it / same keys in another order, plus "fires on release" when that
  applies; overflow ends in "N more". Keyboard selection shows the same tip
  (keyboard users are not told to use the mouse); moving the mouse returns
  to hover. The footer keeps only the description.
- (Decided 2026-10-06, L-97) Colliding settings: when one option's value
  defeats another (a fetch slot equal to Fake Offhand's target slot), every
  row involved draws its stepper frame and value in the warning color, and
  selecting one shows the reason in the warning color in the footer.
- (Decided 2026-11-01, L-293 follow-up) A stepper value cut with "..." shows
  whole in a tip right under the stepper (above when there is no room), on
  hover and for the keyboard-selected row; a value that fits shows no tip.
  Chosen over a footer line because the tip is where the eye already is.

## Code

- `src/input/Binding.h` (actions, defaults, matching kind),
  `src/input/ToggleAction.h`, `src/input/Actions.cpp`,
  `src/input/CustomInput.cpp` (queues actions to the client thread).
- `src/ui/SettingsRows.h`, `src/settings/Options.h`,
  `src/settings/SettingsStore.cpp` (`orderedBindings`).

## Limits

- Coverage of binding edits and remapped keys: [VALIDATION.md](VALIDATION.md)
  row "Settings screen, search, layout B (L-50, L-52, L-81)".
