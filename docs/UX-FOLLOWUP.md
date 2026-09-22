# Settings and controls follow-up

The current build is a development checkpoint, not a finished settings experience.
Settings and input foundation work is now underway, before broader feature expansion.

- The F8 panel has an opaque background, preventing inspection of the world
  while changing visual settings. Evaluate a translucent background and how
  settings can be previewed in context.
- Lamium's panel has no key-binding editor. Default keys are defined in code;
  the implementation registers them with Minecraft's Keyboard & Mouse settings,
  and remapping there was previously tested. Users still report being unable to
  change bindings in game. Reproduce that experience and improve discovery and
  editing within the unified settings UI; do not treat the existing registration
  as resolution of the usability problem.
- The top-left gameplay hints now have a visibility toggle in source. Build and
  persistence tests pass; runtime validation remains pending.
- A flat ten-row panel is only a prototype. Revisit navigation, grouping, search
  and descriptions before adding enough features to make it hard to browse.
- Explicit Save/Cancel has been replaced with per-edit persistence and application.
  Escape/Close only dismisses the screen. Failed saves leave the previous setting
  active and display an error. Runtime validation remains pending.

## Foundation requirements

- A translucent, dense, list-first Features view with search from the start.
  Group each feature's toggle, binding, and options together in broad sections.
  Settings own input while the underlying world remains visible.
- A cross-feature Hotkeys view and direct feature binding editor supporting
  arbitrary chords, modifiers, mouse buttons, modified wheel directions,
  Unbound, and Reset. Actions define Press/Hold/Toggle semantics.
- Reusable panels, lists, search, controls, numeric/text input, binding capture,
  item rendering, navigation, descriptions, and localization. Keep these useful
  for dedicated tools without building a separate GUI framework.
- Recover missing preview controls: Shulker enabled, empty Shulker visibility,
  vanilla Shulker contents text suppression, Bundle enabled, and empty Bundle
  visibility. Reference defaults hide empty previews; migration behavior needs
  an explicit decision to avoid silently changing existing Lamium preferences.
- Distinguish world-space lines/boxes from block-grid shapes. Building shapes
  show block positions/faces, with Block Center snapping by default and optional
  Block Corner/Off. A Shape Manager/Editor owns individual shape workflows.

## Subsequent stages

After the foundation, add camera and interaction tools (Freelook, FreeCamera,
hotbar Tool Switch, Hide Offhand Item, Chunk Borders, Hitboxes, placement/breaking
restrictions), then information and everyday utilities (shared providers for
Info HUD/target information/F3, light overlays, Hand Restock, periodic attack/use,
permanent sneak, and wheel transfers).

Schematic workflows are a later subsystem: browser, placements, projection,
verifier, layers, and material list, followed by staged placement guidance and
assistance. Implement independently from functional requirements and public
formats; do not copy reference code, structures, tests, strings, assets, or
pixel-level UI. Mass Craft, fast attack/use, profiler data, and remote server
timing are research tracks, not blockers. Never invent unavailable information.
Map/minimap/waypoints remain external-first; fast/flexible placement is deferred.

Runtime validation gaps remain separately documented in [VALIDATION.md](VALIDATION.md).
