# Development validation

Baseline: Minecraft 1.26.51.01, LeviLamina Client 26.51.3, Windows x64.

## Confirmed for the initial camera/settings prototype

- Release DLL compilation and mod packaging completed.
- Camera state tests passed: inactive pass-through, held projection scaling,
  sensitivity, wheel limits, transient reset, invalid configuration, small FOV.
- Minecraft loaded Lamium with the older feature mods disabled.
- F8 opened the local settings panel from a creative world.
- Arrow keys changed magnification from 3.0 to 3.5.
- Clicking Save closed the panel and wrote 3.5 to the settings file.
- Reopening the panel showed the saved value.
- Toggling Zoom off and pressing Escape discarded the change; reopening still
  showed Zoom on.
- The panel rendered with the Deesse UI 1.3.9 resource pack enabled.

These observations validate the UI prototype, not the whole feature suite.

## Lighting and settings persistence

- NightVision toggled on with N in an Overworld night scene, visibly brightened
  the same terrain, then returned to normal lighting when toggled off.
- The View settings panel displayed the NightVision state.
- An additive-settings regression was found during restart verification: the
  initial SDK deserializer rejected a missing lighting section and reset camera
  preferences. Settings now use a backward-compatible decoder.
- Regression tests preserve a 3.5x preference from the original camera-only
  schema, supply defaults for missing sections, reject invalid/future schemas,
  preserve unknown fields, and round-trip through a real settings file.
- A locked-destination test confirms failed replacement preserves the previous
  file and removes the temporary file.
- A full Minecraft restart with the revised decoder preserved the saved 3.5x
  magnification from a file without the new inspection section. F8 showed 3.5x,
  with container previews and durability correctly defaulting to enabled.

## Item inspection prototype

- Shulker/Bundle preview providers and numeric durability display compile in
  the release DLL; layout, bundle fingerprint, and durability-bar tests pass.
- The updated DLL loaded in Minecraft. In the creative inventory, hovering a
  diamond sword displayed `Durability: 1561 / 1561` above the vanilla tooltip.
  Moving to a renamed diamond pickaxe updated the tooltip without a crash.
- Filled containers, damaged tools, preview toggles, and live content changes
  still require runtime verification.

## Inventory sorting prototype

- Integrated a pure consolidation/ordering planner, vanilla item classification,
  ordinary container transfers, text-focus tracking, and an R binding.
- Settings independently enable sorting and storage-container targeting.
- Release DLL builds. Planner/key tests cover consolidation, fixed slots,
  region bounds, full inventories, deterministic/idempotent ordering, custom
  names, enchantments, damage, and Shulker content signatures.
- Runtime execution checks local predicted contents before and after each
  vanilla transfer. This does not prove server acceptance: response correlation,
  latency/rejection handling, and in-game conservation checks are outstanding.
- This build has not yet been installed for in-game sorting validation.

## Outstanding release gates

- Verify zoom visually while held, wheel capture, sensitivity, and release.
- Verify menu transitions, focus loss, dimension changes, disconnect/rejoin.
- Verify all settings survive restart and in-game errors preserve existing files.
- Improve keyboard/mouse focus feedback, small-window layout, actual binding
  hints, localization, and gamepad/touch behavior.
- Verify with vanilla UI and additional UI resource packs.
- Verify NightVision underwater, in Nether/End, and across restart/dimension changes.
- Complete runtime validation of previews and durability; complete inventory
  response handling and validate sorting in game.
- Add CI and validate a clean dependency restore/build/package.
- Complete dependency notices and distribution review.
