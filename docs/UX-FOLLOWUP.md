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
- English is the fallback language; Japanese and Simplified Chinese follow
  the game locale.
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
positions. Info, Target, Status, Toast and Zoom Magnification elements use
anchors plus offsets internally, but users place them directly in the HUD
layout editor. Durability, Minimap and Schematic HUD use the same element
system; their feature-specific behavior is documented separately.

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

The Target element shows a block/entity icon,
name, optional identifier and detail rows, vanilla heart sprites and progress
bars. It follows the rendered camera during Freelook/FreeCamera and uses one
2-64 block Range setting for every viewpoint, skipping water/lava in detached
camera picks. Lamium-wide Animations can follow Minecraft's Screen Animations,
be forced On or forced Off.

## Current input behavior

L-32 Hotkey overlap and chord semantics is implemented and verified in game:

- ordinary chords are order-sensitive;
- completing a more-specific ordinary chord suppresses the competing shorter
  activation for that sequence;
- modifier-like actions such as Zoom/Freelook allow unrelated gameplay keys;
- exact duplicate chords are valid and fire all enabled actions;
- the Hotkeys UI warns about exact duplicates and subset/superset overlaps.

Keys that begin a longer chord fire on release when used alone and stay silent
when the longer chord completes. The Java-style defaults F3, F3+B and F3+G
were confirmed together in game on 2026-09-28.

Do not add a second advanced keybind-settings system; matching mode remains an
action property.

## Settings key feedback (2026-10-14)

[L-188](BACKLOG.md) proposes making the configured settings shortcut
(default L) a close/open toggle rather than open-only. Resolve focus in
text fields, key capture, nested dialogs and remapped bindings before
changing behavior. Separately, [L-182](BACKLOG.md) tracks an intermittent
crash when opening settings while checking a schematic; it needs an actual
reproduction and is not solved merely by changing the toggle.

## Remaining UI/runtime gaps

These are not reasons to redesign the shared settings UI:

- Controller/touch and broad resource-pack/layout coverage are incomplete.
- Hand Restock supports main-inventory/hotbar sources, a threshold and source
  order, and offhand totems. Remaining server/lifecycle checks are in
  [HAND-RESTOCK.md](HAND-RESTOCK.md) and VALIDATION.
- The L-15 breaking/placement restriction redesign and L-59 held-placement
  styles are planned but not built.
- Map and Schematics are built, experimental subsystems; their follow-ups
  are L-60 and L-93. See [MAP.md](MAP.md) and [SCHEMATIC.md](SCHEMATIC.md).
- Mass Craft remains an untriaged idea (BACKLOG's Later / parked section).

Runtime evidence, including exact tested builds, stays in
[VALIDATION.md](VALIDATION.md).
