# Settings and controls follow-up

The current build is a development checkpoint, not a finished settings experience.
Settings and input foundation work is now underway, before broader feature expansion.

- The source now substitutes translucent drawing for the owned native dialog
  and requests the world behind it. Runtime world visibility, input isolation,
  and restoration still need verification; this is not a validated UX fix yet.
- Features and Hotkeys now expose a binding capture editor with Clear/Reset.
  Native Minecraft mappings remain the fallback when no override is stored.
  The reported binding usability issue still needs runtime verification of the
  complete edit/save/use workflow. Exact duplicate custom bindings are marked;
  broader conflict detection and clearer feature descriptions remain open.
- The top-left gameplay hints now have a visibility toggle in source. Build and
  persistence tests pass; runtime validation remains pending.
- Features now groups settings and bindings under collapsible feature headers
  with state/binding summaries and short descriptions. Search reveals matching
  children even when the feature is collapsed; Hotkeys remains a flat action
  list. Pure row-generation and text tests pass. In-game input delivery, text
  fit, expansion/click targets, and layout remain unverified.
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
- Shulker enabled, empty Shulker visibility, Bundle enabled, and empty Bundle
  visibility are now editable through the shared settings catalog; runtime
  validation is pending. Defaults retain existing Lamium behavior, including
  empty previews. Vanilla Shulker contents text suppression is also editable,
  defaults off, and only applies while Shulker previews are enabled. Its runtime
  behavior remains unverified.
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
