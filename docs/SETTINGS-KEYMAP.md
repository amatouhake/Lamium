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
