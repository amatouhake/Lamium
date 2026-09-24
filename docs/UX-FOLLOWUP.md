# Settings, controls and HUD follow-up

This file records the current state of the shared Lamium UI foundation and the
remaining input/UI gaps. Product behavior is defined in
[DESIGN.md](DESIGN.md); task status and ordering live in
[BACKLOG.md](BACKLOG.md). Do not use old prototype behavior as a second source
of truth.

## Current foundation

The settings foundation is integrated and has been exercised in Minecraft:

- `L` opens Lamium Settings. The owned native dialog provides focus while
  Lamium draws a translucent, dense sidebar/table UI over the live world.
- Categories, search, collapsible feature rows, per-feature options and action
  bindings share one screen. Hotkeys, Shapes and HUD layout are pinned tools in
  the same navigation.
- Changes apply and persist immediately. Escape/Close dismisses the screen;
  failed saves keep the previous value active and report an error.
- English is the fallback language and Japanese follows the game locale.
  Shared widgets handle text, descriptions, switches, key caps, steppers,
  sliders and text/numeric entry.
- Bounded numeric options that do not need precision use Bedrock-style sliders.
  Clicking the value enters a number; Left/Right or -/+ steps it. Precise
  settings keep direct numeric editing.
- Lamium owns all action bindings. Clear means explicitly Unbound and Reset
  returns to Lamium's default. The Settings action cannot be cleared.
- Gameplay key hints were removed. Unbound Open Hotkeys, Open Shapes and Open
  HUD layout actions provide direct entry points instead.

## HUD and target UI

The HUD is now one shared element system rather than separate hard-coded
positions. Info, Target, Status and Toast elements use anchors plus offsets
internally, but users place them directly in the HUD layout editor.

The editor was reworked after in-game use and verified with:

- click/drag placement, edge/center snapping and keyboard nudging;
- per-element scale, background and shadow controls;
- an Info lines popover with switches and ordering;
- reset for one element or the whole layout;
- toolbar/popover placement that avoids covering the selected element controls.

HUD drawing is restricted to the gameplay HUD view so translucent cards remain
translucent instead of being composited repeatedly. The normal settings screen
hides the HUD for readability; the HUD editor intentionally shows live/sample
content.

The Target element is the current Jade/WAILA-style surface: block/entity icon,
name, optional identifier and detail rows, vanilla heart sprites and progress
bars. It follows the rendered camera during Freelook/FreeCamera and uses one
2-64 block Range setting for every viewpoint, skipping water/lava in detached
camera picks. Lamium-wide Animations can follow Minecraft's Screen Animations,
be forced On or forced Off.

## Remaining input work

The major unresolved settings/input item is **L-32 Hotkey overlap and chord
semantics**. The current matcher still canonicalizes chords without press
order and treats a shorter chord as matched when its tokens are a subset of a
longer held chord.

The accepted replacement is already specified in BACKLOG/DESIGN:

- ordinary chords are order-sensitive;
- completing a more-specific ordinary chord suppresses the competing shorter
  activation for that sequence;
- modifier-like actions such as Zoom/Freelook allow unrelated gameplay keys;
- exact duplicate chords are valid and fire all enabled actions;
- the Hotkeys UI warns about exact duplicates and subset/superset overlaps.

Do not add a second advanced keybind-settings system; matching mode remains an
action property.

## Remaining UI/runtime gaps

These are not reasons to redesign the shared settings UI:

- L-20: Shape name native text input can still insert stray characters.
- Controller/touch and broad resource-pack/layout coverage are incomplete.
- Some feature-specific native paths remain research items: shield rendering
  under Hide Offhand, Hand Restock transfers, continuous Tool Switch and
  breaking/placement handoff.
- F3-style full Debug View, Scroll Transfer, Schematics and Mass Craft are
  separate future work and need their own design passes.

Runtime evidence, including exact tested builds, stays in
[VALIDATION.md](VALIDATION.md).
