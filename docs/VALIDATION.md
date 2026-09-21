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

## Outstanding release gates

- Verify zoom visually while held, wheel capture, sensitivity, and release.
- Verify menu transitions, focus loss, dimension changes, disconnect/rejoin.
- Verify settings survive a full restart and errors preserve existing files.
- Improve keyboard/mouse focus feedback, small-window layout, actual binding
  hints, localization, and gamepad/touch behavior.
- Verify with vanilla UI and additional UI resource packs.
- Implement and validate NightVision, previews, durability, inventory sorting.
- Add CI and validate a clean dependency restore/build/package.
- Complete dependency notices and distribution review.
