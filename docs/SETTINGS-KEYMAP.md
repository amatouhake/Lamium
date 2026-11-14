# Settings and keymap roles (L-52, decided 2026-09-27; rules revised L-83)

## Rules (decided 2026-10-01, L-83, option X)

These rules govern every feature, current and future. They refine the L-52
layout below; where they differ, these win.

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
- Default keys: none, except where a convention exists or the action is
  used constantly (L settings, C zoom, R sort, F3 / F3+G / F3+B, M world
  map, right button fake offhand, F swap with offhand (2026-10-06, Java's
  key; Bedrock has none), Alt+Wheel to move the schematic target away/closer
  (2026-10-21)). Record the date when one is added.
- A toggle key shows the toggle toast, as all toggles do now.
- New action ids are appended (never reordered); moving an action to
  another row changes only presentation, and existing bindings stay.

## Current implementation reference

The current feature/action rows are in `src/ui/SettingsRows.h`, option
definitions in `src/settings/Options.h`, and append-only action ids/defaults
in `src/input/Binding.h`. Use those catalogs for an exact inventory instead
of a copied feature table that can drift as features are added.

The settings screen remains the place to find switches and options; Hotkeys
lists every action, including unbound commands. Current sections include Map
and Schematics. Shared navigation/input notes are in
[UX-FOLLOWUP.md](UX-FOLLOWUP.md), and runtime coverage in
[VALIDATION.md](VALIDATION.md).

## Earlier layout review (L-52, 2026-09-27)

The maintainer selected layout B in
[the comparison demo](demos/settings-keymap-review.html). Immediate commands
moved to child rows; Breaking Restriction's toggle and mode keys moved from
the group heading to their matching rows. Existing bindings, action ids,
settings ids and saved values were preserved.

At that checkpoint some parent switches had no keys (sorting, previews,
durability and others), and Shapes/automation rows lived elsewhere. L-83's
rules above supersede that arrangement: every feature switch offers its own
toggle key and screen commands sit with their owning feature. The original
review and subsequent changes are retained under L-52/L-83 in BACKLOG-DONE.

## Default-key decision (2026-09-27)

The maintainer chose Java-style debug defaults: F3 for Debug View, F3+B for
Hitboxes and F3+G for Chunk Borders. NightVision loses its arbitrary J default;
N is Minecraft Bedrock's notifications key. Other defaults stay as they were.
F3 alone activates on release because it leads longer chords; completing B or
G suppresses that F3 action. Explicit user bindings, including an empty chord,
take precedence over the changed defaults. No saved binding or action ID is
rewritten. The F3 / F3+B / F3+G combination was confirmed in game on 2026-09-28.

The maintainer confirmed the layout in game on 2026-09-27. Individual
binding-editing and existing-custom-binding cases were not separately
confirmed.

## Resetting settings (Decided 2026-09-26, BACKLOG L-46)

Moved from DESIGN.md on 2026-11-14 without changes; still the decided behavior.
- No per-setting or per-feature reset (vanilla has none either). "All" has
  "Reset all" (every setting, key bindings and HUD layout; not Shapes), each
  category (Camera & view, Inventory, ...) has "Reset category" (its features'
  settings and HUD placement; not key bindings), and Hotkeys has "Reset all
  keys". They sit on the column-heading line and need a second press to apply.
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
- (Decided 2026-10-12, L-159, demos/binding-editor.html A) While a key is
  being assigned, Clear / Reset / Cancel sit on the edited row, ending just
  before its key cell; they take the place of that row's switch or stepper
  until the edit ends. The footer keeps the action's description and the
  "press, then release to assign | Esc: cancel" line.

## Keys

Moved from DESIGN.md on 2026-11-14 without changes; still the decided behavior.
- (Decided) There is no general default-key policy to design now; defaults are
  decided per feature when needed. New actions ship unbound unless the
  maintainer picks a key.
- (Decided) Settings opens with `L` instead of F8 (BACKLOG L-01).
- (Decided 2026-09-27) Debug View defaults to `F3`, Hitboxes to `F3+B`, and
  Chunk Borders to `F3+G`, following Java's debug keys. NightVision defaults
  unbound; its old `J` key had no feature mnemonic and `N` belongs to Bedrock
  notifications. Existing saved bindings, including explicit unbinds, win over
  these defaults.
- (Decided) Lamium owns its key bindings; Minecraft's keyboard settings no
  longer list Lamium actions (BACKLOG L-23).
- (Decided, BACKLOG L-32) Ordinary action chords and modifier-like chords have
  different matching semantics. Ordinary chords are order-sensitive and a
  more-specific completed chord suppresses an overlapping shorter chord for
  that activation. Example: with `B` and `F3+B`, pressing F3 then B fires
  only `F3+B`; pressing B first may fire B immediately and is not delayed in
  case another key arrives later.
- (Decided, BACKLOG L-32) Modifier-like actions such as Zoom and Freelook allow
  unrelated held gameplay inputs so they remain usable while moving. Matching
  mode is an action property, not another advanced user option.
- (Decided, BACKLOG L-32) Exact duplicate bindings are valid. The Hotkeys UI
  warns that the actions share a binding, but all enabled actions assigned to
  that exact chord fire together. This intentionally permits grouped toggles;
  duplicate bindings are never silently prioritized or disabled.
- (Decided, BACKLOG L-32) Hotkeys UI also warns about subset/superset overlaps.
  Press/Hold/Toggle remains separate from chord matching semantics, and mouse /
  keyboard / wheel combinations follow the same overlap rules.
- (Implemented L-32, 2026-09-25) Details chosen while building it:
  - A chord is stored in press order; its last input completes it. Actions
    activate only on that completing press, never while keys merely stay held.
  - Both kinds tolerate unrelated held inputs (walking with W still allows
    another bound toggle). Ordinary chords differ by order and by yielding.
  - The most specific completed chord wins for everyone, modifier-like actions
    included (Ctrl+C beats C when Ctrl is held). A chord that yielded stays
    silent until one of its keys is released: no late firing on key repeat.
  - When a longer chord starts on top of an active ordinary one (B held, then
    B + click), the shorter one is released and does not resume. Active
    modifier-like actions (Zoom) keep running.
  - Settings files written before L-32 stored chords sorted by code. They load
    in the order modifiers, function keys, other keys, mouse, wheel (so a saved
    F3+B stays F3 then B); new files carry `orderedBindings: true`.
- (Decided 2026-09-25, after the L-32 playtest) Keys that begin a longer chord
  act on release, like Java's F3. A Press/Toggle action whose chord is the
  leading part of another bound chord (F3 with F3+B bound) does nothing on
  press and fires when released, unless a longer chord containing it fired
  while it was held. Unrelated keys (walking, other Lamium actions) do not
  cancel it; focus loss, menus and text input drop it silently. This is
  derived from the bindings, not a user option. Actions without such a longer
  chord still fire on press, and Hold actions (Zoom, Freelook) always act on
  press.
- (Decided 2026-10-06, L-97) Colliding settings: when one option's value
  defeats another (a fetch slot equal to Fake Offhand's target slot), every
  row involved draws its stepper frame and value in the warning color, and
  selecting one shows the reason in the warning color in the footer. Same
  idea as hotkey conflicts, for values.
- (Decided 2026-09-25, docs/demos/hotkey-conflicts.html) Conflict display.
  Every key cell (Hotkeys and feature rows alike) draws related bindings with
  warning-colored key caps: filled for the exact same chord, outlined for an
  overlap. No text badge beside the keys (it looked like another key cap). Hovering
  the key cell opens a tooltip below it (above when there is more room) that
  lists every related binding grouped as same key / starts with this key /
  longer chords containing it / shorter chords inside it / same keys in
  another order, plus "fires on release" when that applies; overflow ends in
  "N more". Keyboard navigation shows the same tooltip for the selected row
  (keyboard users are not told to use the mouse); moving the mouse returns to
  hover. The footer keeps only the feature/action description.
- (Decided 2026-11-01, L-293 follow-up) A stepper value cut with "..." shows
  whole in a tip right under the stepper (above when there is no room), with
  the same look as the key-cell tip: on hover over the stepper, and for the
  keyboard-selected row. A value that fits shows no tip. Chosen over a first
  footer line because the tip is where the eye already is.
