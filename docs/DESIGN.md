# Lamium design direction

This is the source of truth for how Lamium looks and behaves. Sections marked
**Decided** were agreed with the maintainer; in-game coverage is recorded
separately in [VALIDATION.md](VALIDATION.md). Sections
marked **Proposed** are the current direction but still need the
maintainer's confirmation before an agent builds on them. Do not change a
Decided rule without asking.

## Product principles (Decided)

- Client-side only. Never require a server mod. Never invent information the
  client does not have; show "unavailable" instead.
- One mod, many tools, still easy to handle: every feature is found, understood,
  switched and bound from one settings screen. Tools with their own state
  (Shapes, Waypoints, Schematics) get a dedicated view built from the same parts.
- Settings apply and save immediately. Escape/Close only closes. Only a failed
  save is reported.
- A feature's switch, any key and its options live together. There is no "Advanced"
  bucket; options have names that say what they do.
- Each independent feature has one on/off switch, and its parent row always
  offers a key that toggles it (unbound unless decided otherwise; L-83,
  2026-10-01). Commands of a feature (do now, open its screen, cycle) are
  child rows; a screen opener sits with the feature that owns the screen.
  The world map is the one screen that stays usable with its only state off,
  so its parent opens it and "Record" is a child switch (L-313, 2026-11-06).
  Named commands such as "Open settings" may instead have a key without a
  switch. Related settings may have a keyless group heading.
  Zoom, Freelook and FreeCamera offer "Activation: Hold / Toggle"; Hold lights
  the switch while the key is held. Other Press/Hold/Toggle choices are made
  by the action, not the user. See [SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md).
- Large or uncertain features (Map, Schematic, Mass Craft, profilers,
  Placement Assist) grow on main as default-off features with the
  Experimental badge and never hold back a release (BACKLOG "Release
  policy", decided 2026-09-28).
- Features restore vanilla behavior when disabled or when leaving a world.
- The map draws only what the client has seen. Biomes, structures or
  terrain predicted from the world seed are a non-goal (decided 2026-10-01,
  L-82): they would depend on keeping a generator current for every game
  version and would look as real as recorded terrain. The world map links
  to an external seed map instead.
- Lamium targets Windows x64 with LeviLamina Client. Android support is not
  currently planned (decided 2026-10-05): it needs an ARM64 build,
  replacements for the Windows-specific code and input, and new checks of
  every version-sensitive hook. Reconsider only after the platform split and
  its upkeep are weighed.
- External prior art may inform behavior and feasibility, but repository specs
  describe Lamium's requirements rather than "like mod X". Reference-only
  projects are listed in PROVENANCE.md; their code, strings, assets,
  distinctive constants and pixel-level UI are not copied or translated.

## Engineering behavior (Decided)

These rules apply across feature areas, including large experimental
subsystems:

- Expensive scans, geometry work, uploads and broad enumeration are bounded per
  frame/tick or moved to background work with an explicit publication budget.
- Background results that depend on world or dimension state carry generation
  identity. A completion is published only if its identity is still current;
  stale work is discarded.
- Gameplay-affecting actions use ordinary game semantics where possible and
  do not treat a local return value, prediction or submitted transaction as
  authoritative completion. Multi-step workflows revalidate resulting state.
- Version-sensitive internal renderer/UI paths are capability-gated and fail
  open: when the expected contract cannot be verified, Lamium leaves vanilla
  behavior intact.
- (Decided 2026-10-15, L-202) A shared typed catalog owns compatibility policy,
  runtime availability and failure reasons per capability. Features keep their
  actual native checks and cleanup. Settings, compatibility and runtime state
  are separate; waiting for context is not failure. Execution, settings/help
  and notices read the same status. Loader support is a series plus minimum,
  not a growing patch allowlist; unverified Minecraft builds remain blocked.
  Failures latch until explicit restart, and capability state owns no game
  pointers. Adopt the shared mechanism without merging the feature modules.
- Presentation features keep observed game state separate from display state;
  hiding or augmenting visuals must not silently mutate the underlying
  gameplay state.

## Visual language (Decided)

"Fits Bedrock without cloning it; PC utility density; restrained modern touch."

- Dark translucent panels over the live world. Flat fills and 1-unit frames.
  No gradients, glow, blur or decorative motion.
- Only rectangles and text are drawn (`src/ui/Widgets.h`). The game font is
  used as is; no custom fonts. Symbols outside its glyphs (arrows such as
  U+2192, other pictographs) fall back to another font or a box, differently
  per language, so draw them from rectangles in Widgets (`arrow`,
  `chevron`, `changeArrow`) instead of putting them in text (2026-10-08).
- Dense rows for mouse and keyboard, not Bedrock's large touch buttons.
- The information structure is list-first: name, state, key, then options.

### Tokens

All colors come from `ui::palette` in `src/ui/Widgets.h`. (Decided
2026-11-14, L-341) The palette has two groups, and a color literal in a
screen's file is a missing token:

- **Lamium's own colors** carry one meaning each across every Lamium
  screen and HUD element. A hue is not reused for another meaning: purple is
  only "Experimental", yellow only a warning, red only a destructive action,
  light blue only "reference, not Lamium's own content" (a draft, a ghost,
  the game's own HUD parts in HUD layout).
- **Colors sampled from the game** (`heart`, `heartEmpty`, `armor`, `curse`,
  the container preview's tooltip border) only match what the game draws and
  carry no Lamium meaning. They are to move under `palette::vanilla::`
  (L-341); until then they sit beside the Lamium colors.

| Token | Use |
|---|---|
| `panel` (opacity .8) | Screen background |
| `text` / `dim` / `faint` | Primary text / secondary text and icons / hints |
| `accent`, `accentDeep` | Switch on, selection, links, primary buttons |
| `off` | Switch off track |
| `knobOn`, `knobOff` | Switch knob |
| `keyFill`, `keyEdge` | Key caps and the plain button fill and edge |
| `experimental` | "Experimental" badge |
| `warning` | Warnings (e.g. Simple graphics notice) |
| `danger` (to add, L-341) | Destructive buttons, their armed state (today a literal in 8 files) |
| `hover` (to add, L-341) | A hovered button's fill (today a literal in the world map) |
| `info` (to add, L-341) | Light blue "reference" color (today four near-equal literals) |

Text shadow is drawn by `labelScaled`, not by a token.

Sizes (GUI units, `SettingsTable`): header 20, row 14, nav item 14, sidebar
112, padding 6, gap 6, key cap height 11, switch 18×9. The settings panel is
at most 640×380 and centered. New screens reuse these numbers.
(Decided 2026-10-15) There is one settings layout. A GUI smaller than it
needs (440 wide, and tall enough for every sidebar item beside the
footer) draws the settings screens (table, language list,
Shapes, Waypoints, Schematics) shrunk to fit, rather than a second layout
with tabs or a one-line footer. The world map, calculator, prompts, radial
menu and HUD editor keep GUI units.

### Components (Decided)

- **Switch** for every on/off (Bedrock style, knob position carries state).
  No "ON/OFF" words and no check boxes.
- **Key caps** for bindings. The key column never shows Hold/Toggle/Press.
- **Steppers**: `− value +` for numbers, `◀ value ▶` for choices.
- (Decided 2026-11-14, L-341) **Shared controls.** Every screen draws a
  control through the shared widgets, and a control is specified here when
  its shared widget lands, in the same commit (a spec without the widget
  sends the next screen back to a local copy). The short form is the header
  of `Widgets.h`; `scripts/Check-UiTokens.ps1` stops new color literals and
  hand-drawn buttons. Agreed so far, built in L-341:
  - Buttons come in three kinds: plain (`keyFill` with `keyEdge`), primary
    (`accent` fill) and destructive (press twice: the first press arms it in
    `danger`, the second acts). HUD layout's "Done (Esc)" becomes a primary
    button and its Reset a destructive one.
  - A settings row that opens a page has the shared Open button; Game
    language too. Its row shows the current language as faint text where
    the value button was. Enter or Space on the row still opens the list.
  - A page that replaces the settings rows (Game language, Saved data, Block
    highlight, ...) stays open when the window loses focus: a drag ends and
    an armed button disarms, nothing else changes.
  - In HUD layout the frame and legend of the game's own HUD parts use the
    light blue `info`, not `warning`.
- (Decided 2026-11-13, L-319) While a feature's switch is off, its child
  setting rows are drawn dimmed in every feature. Their names do not change
  and they stay editable; the footer says "Main switch off" above the
  setting's own help, and the toast of a child's key adds it after the name.
  Command rows (open a screen, do now) are not dimmed. There is no
  exception: Waypoints off also stops recording a death point and removing
  reached temporary waypoints (the same day); a death's waypoint is one
  of them (L-328, 2026-11-13); hiding only the markers is
  "Show in the world" and the Map marks layer.
- **Child settings** are indented with a vertical guide line. Expanding or
  collapsing keeps the clicked row where it was. The wheel scrolls without
  moving the selection.
- **Sidebar** with categories; Hotkeys, Shapes, Waypoints, Schematics, World
  map and HUD layout are pinned at the bottom.
  Below 380 units the key column narrows. Search spans all categories.
- **Scrollbars** (decided 2026-10-15): the settings list and the footer
  description show a scrollbar while they overflow; pressing or dragging it
  puts the first line under the pointer, as in the tool lists.
- **Header** (decided 2026-10-06, L-101): `Lamium` and the faint Lamium
  version, then search and Close. The sidebar shows where you are, so
  the header never repeats the page name; only a panel without a sidebar
  (docked Shapes, Waypoints, Schematics) titles itself. Hovering the version
  shows the Lamium, Minecraft and LeviLamina versions (English) and clicking
  copies them for bug reports.
- **Experimental** features carry a small purple badge. Which ones is the
  `experimental` flag in `SettingsRows.h`; when it comes off and the
  earlier badge decisions are in BACKLOG "Release policy".

### Text and languages (Decided)

- (Decided 2026-10-15, L-197; revised the same day) The settings footer
  always shows two description lines. Longer text scrolls there: the wheel
  over the footer or its scrollbar, which appears only while the text
  overflows (the hint line then starts with the wheel hint). There is no
  "Read full description" button, expansion, F1 shortcut or separate help
  surface, so the list keeps its height while the selection moves. A
  newly selected row starts at the top of its description; a change in the
  same row's text keeps the place. Wheel input over the list scrolls the
  list. Numeric descriptions include their own help and range (white);
  their controls replace the faint footer hint while the row is selected
  (white says what a setting is, faint says how to operate it). All
  languages use the same behavior. This replaces the in-place expansion
  and the earlier separate-panel contracts of the same date.

- English is the default; Japanese, Simplified Chinese (`zh_CN`, L-90) and
  Traditional Chinese (`zh_TW`, PR #39) follow the game language, and other
  languages show English. Every user-facing key has all four (maintainer
  decision, 2026-10-26); no language is marked as more provisional than
  another. Settings labels keep an ASCII ": " between name and value in every
  locale (the settings table splits there). Traditional Chinese is translated
  from English on its own, not converted from Simplified Chinese; game terms
  (items, blocks, biomes) follow the game's own text in each language.
  Contributor notes: [TRANSLATING.md](TRANSLATING.md).
- (Decided 2026-11-13, L-319) The raise of Latin runs stays a fixed,
  Japanese-only adjustment and is not a setting. It was tried as one
  ("Latin text raise") and removed the same day: in Chinese a Latin run
  drawn apart from the hanzi is given the pixel font, so raising it changes
  its shape and width, and the small offset there is the game's own.
- Japanese locale: Latin runs are raised 1 unit (1.5 until 2026-11-13, which
  read as too high) to share the baseline, and
  text inside a frame starts `boxTextInset()` lower, and a shape drawn beside
  text (the change arrow) moves `shapeTextDrop()` lower to meet the glyphs. Always draw through
  `ui::label`/`ui::paragraph`, never raw font calls.
- Text shadow: Lamium draws its own copy half a GUI unit away; the engine's
  shadow sits a whole unit away and reads as doubled on dense Japanese lines.
  Labels with `shadow` false draw no copy at all.
- Labels with a value use one translation string with `{}`
  ("Radius: {}"), split by `splitLabel` for table display.
- (Decided 2026-10-12) Feature names and action labels may remain separate:
  commands need wording that describes their operation, and a child label may
  be shorter than its standalone Hotkeys name. Accidental naming drift and
  missing action translations need systemic prevention (BACKLOG L-158), not
  removal of this distinction. Search and display should resolve the same
  intended label for a given context; additional search aliases remain valid.

## Feature decisions

Decisions about one feature live in that feature's document; the
sections below moved there on 2026-11-14 unchanged. A reference to
DESIGN "<section>" means the section of that name in the file listed.

- HUD: [HUD.md](HUD.md)
- World overlays: [OVERLAYS.md](OVERLAYS.md)
- Camera: [CAMERA.md](CAMERA.md)
- Visual effect visibility: [VISUAL-EFFECTS.md](VISUAL-EFFECTS.md)
- Automatic attack and use: [AUTOMATION.md](AUTOMATION.md)
- Light overlay: [LIGHT-OVERLAY.md](LIGHT-OVERLAY.md)
- Resetting settings: [SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md)
- Cave and Nether terrain reference: [MAP.md](MAP.md)
- World map layer switches: [MAP.md](MAP.md)
- World map side panel lists: [MAP.md](MAP.md)
- Inventory transfer: [ITEM-TRANSFER.md](ITEM-TRANSFER.md)
- Repeated villager trading: [TRADE-HELPERS.md](TRADE-HELPERS.md)
- Edge Guard: [RESTRICTIONS.md](RESTRICTIONS.md)
- Keys: [SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md)
- Waypoints and death records: [MAP.md](MAP.md)
