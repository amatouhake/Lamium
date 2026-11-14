# Lamium design guide

Read this whole file before designing or changing anything a player sees or
does: a screen, a HUD element, a setting, a key, a message. It is short on
purpose. Every rule here was agreed with the maintainer; do not change one
without asking. A rule that only one feature follows belongs in that
feature's document (index at the end), not here. Why a rule exists and when
it changed: `git log -p docs/DESIGN.md`, `docs/backlog/done/`.

When a design needs something these rules do not cover, or would break one,
stop and ask the maintainer (AGENTS.md "When to stop").

## Product

- Client-side only; never require a server mod. Never invent information the
  client does not have; show "unavailable" instead.
- One mod, many tools, one settings screen: every feature is found, switched,
  bound and explained there. Tools with their own state (Shapes, Waypoints,
  Schematics) get a dedicated view built from the same parts.
- Settings apply and save at once. Esc and Close only close. Only a failed
  save is reported.
- A feature's switch, keys and options sit together. No "Advanced" bucket;
  an option's name says what it does.
- Each independent feature has one switch, and its row always offers a key
  that toggles it (unbound unless the maintainer picks one). Commands of a
  feature (do now, open its screen, cycle) are its child rows. A named
  command ("Open settings") may have a key without a switch. Related settings
  may share a keyless group heading. Zoom, Freelook and FreeCamera offer
  "Activation: Hold / Toggle" (Hold lights the switch while the key is
  held); other actions decide Press/Hold/Toggle themselves. Details: [SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md).
- New actions ship unbound unless the maintainer picks a key.
- Large or uncertain features grow on main default-off with the Experimental
  badge and never hold back a release (BACKLOG "Release policy").
- Every feature restores vanilla behavior when switched off, on world exit,
  dimension change and focus loss.
- The map draws only what the client has seen; nothing predicted from the
  seed (it would need a generator per game version and look as real as
  recorded terrain).
- Windows x64 with LeviLamina Client only; Android is not planned.
- Prior art may inform behavior, but specs describe Lamium in its own terms
  and nothing is copied ([PROVENANCE.md](PROVENANCE.md)).

## Engineering

- Bound expensive scans, geometry, uploads and enumeration per frame/tick,
  or move them to background work with a publication budget.
- Background results tied to a world or dimension carry its generation and
  are dropped when it is no longer current.
- Gameplay actions use ordinary game semantics and are confirmed from the
  resulting game state, never from a return value, prediction or sent
  transaction. Multi-step work rechecks state between steps.
- Version-sensitive paths go through the capability catalog
  (`app/Capabilities.h`) and fail open: if the contract cannot be verified,
  vanilla behavior stays. Settings, compatibility and runtime state are
  separate; waiting for context is not failure; a failure latches until
  restart.
- Presentation features change only what is drawn, never the gameplay state
  behind it.

## Look

"Fits Bedrock without cloning it; PC utility density; restrained modern
touch."

- Dark translucent panels over the live world, flat fills, 1-unit frames.
  No gradients, glow, blur or decorative motion.
- Only rectangles and text, through `src/ui/Widgets.h`; the game font as is.
  Symbols outside its glyphs (arrows, pictographs) are drawn from rectangles
  (`arrow`, `chevron`, `changeArrow`), never put in text: they fall back to
  another font or a box, differently per language.
- Dense rows for mouse and keyboard, not large touch buttons.
- List first: name, state, key, then options.

### Colors

Every color is a token in `ui::palette` (`Widgets.h`). A color literal in a
screen's file is a missing token; `scripts/Check-UiTokens.ps1` stops new
ones. Lamium's own colors carry one meaning each, everywhere:

| Token | Meaning |
|---|---|
| `panel` (opacity .8) | Screen background |
| `text` / `dim` / `faint` | What it is / secondary text and icons / how to operate it |
| `accent`, `accentDeep` | On, selected, links, primary buttons |
| `off` | Switch off track |
| `knobOn`, `knobOff` | Switch knob |
| `keyFill`, `keyEdge` | Key caps; plain button fill and edge |
| `experimental` | Purple: only the Experimental badge |
| `warning` | Yellow: only warnings (conflicts, refused values, notices, a schematic block in the wrong state) |
| `danger`, `dangerEdge`, `dangerText` | Red: only destructive actions: armed fill, edge (hovered or armed), hovered text |
| `info` | Light blue: reference, not Lamium's own content: drafts, ghosts, waypoint names on the map, the game's own HUD parts |
| `mismatch` | Red text for a schematic block that differs from the world (not destructive, so not `danger`) |
| `cornerOne`, `cornerTwo` | A schematic area's two corners, as their marks in the world |
| `shade` | Black scrims, field backs and outlines; each use sets its opacity |

Colors sampled from the game (`heart`, `heartEmpty`, `armor`, `curse`,
`tooltipBorder` for the container preview's frame) live in
`palette::vanilla::`: they only match what the game draws and carry no
Lamium meaning. Text shadow
comes from `labelScaled`, not a token.

### Sizes

GUI units (`SettingsTable`): header 20, row 14, nav item 14, sidebar 112,
padding 6, gap 6, key cap height 11, switch 18x9. The settings panel is at
most 640x380, centered. New screens reuse these numbers. There is one
settings layout: a GUI too small for it (under 440 wide, or too short for
the sidebar and footer) draws the settings screens shrunk to fit, never a
second layout. The world map, calculator, prompts, radial menu and HUD
layout editor keep GUI units.

## Controls

Every control comes from the shared widgets (`Widgets.h`, `ListViewWidgets.h`).
A control a screen needs and they lack is added there, and specified here in
the same commit. A screen never draws its own variant. Older screens still
have local copies (BACKLOG L-341); they are not the pattern.

- **Switch** for every on/off, knob position carrying the state. No "ON/OFF"
  words, no check boxes.
- **Key caps** for bindings. The key column never shows Hold/Toggle/Press.
  A binding that relates to another draws warning-colored caps: filled for the
  exact same chord, outlined for an overlap; hovering (or selecting with the
  keyboard) lists the related bindings in a tip.
- **Steppers** (`drawStepper`): one box, `- value +` for numbers,
  `< value >` for choices (arrows drawn from rectangles); never separate
  `-`/`+` buttons. The end cell under the pointer lightens; an end that
  cannot step draws faint. Where a value can be typed, a click on it types
  (the edit draws in the box). A value cut with "..." shows whole in a tip
  under the stepper. A value that defeats another setting draws its stepper in
  warning color, and the footer says why.
- **Buttons**, three kinds of one shape: a fill, a 1-unit edge lighter than
  the fill, centered white text; only the colors differ. Hovering lightens
  the fill (a white wash). A disabled button is plain with faint text and
  no hover; faint text means disabled nowhere else. Plain (`keyFill`,
  `keyEdge`), primary (`accentDeep`, `accent`: the main action, and a
  button that is on or selected: an open panel or popover, the active
  tab), both `drawButton` (`drawButtonBox` when the content is an icon or
  colored text), and destructive
  (`drawDangerButton`: looks plain until pointed at, then `dangerEdge` and
  `dangerText`, so an always visible one such as a section's Reset does not
  draw the eye; the first press arms it, `danger` fill with `dangerEdge` and
  white text; the second press acts; losing focus disarms).
- **Text fields** (`drawTextField`): a shaded back with a `keyEdge` edge,
  `accent` while typing; a placeholder faintly when empty. Enter or a click
  outside keeps the edit; Esc drops it and shows the stored value again.
- **Scrollbars** (`drawScrollbar`) on any list or text that overflows: a
  faint track and a 2-unit thumb that widens and turns white under the
  pointer or while dragged; pressing or dragging puts the first line under
  the pointer.
- **Tips** open below their anchor (above when there is no room) and look
  the same everywhere.

## Screens

- **Header**: `Lamium` and the faint version, then search and Close. The
  sidebar shows where you are, so the header never repeats the page name;
  only a panel without a sidebar (docked Shapes, Waypoints, Schematics)
  titles itself. Hovering the version shows the Lamium, Minecraft and
  LeviLamina versions; a click copies them.
- **Sidebar**: categories; Hotkeys, Shapes, Waypoints, Schematics, World map
  and HUD layout pinned at the bottom. Search spans all categories.
- **Rows**: child settings are indented with a guide line. Expanding keeps
  the clicked row in place; the wheel scrolls without moving the selection.
  While a feature's switch is off, its child setting rows are dimmed (names
  unchanged, still editable), the footer says "Main switch off" above the
  help, and a child key's toast says it too. Command rows (a row that runs an action or
  opens a screen: Open buttons, do-now actions, the HUD layout link) are not
  dimmed.
  Switching a feature off stops everything it does, without exception.
- **A row that opens a page** has the shared Open button; Enter or Space on
  the row opens it too. A row with a current value (Game language) shows it
  as faint text beside the button (Game language).
- **Pages** that replace the settings rows (Game language, Saved data, Block
  highlight, Enchantment order) share one shape (`RowPages.h`,
  `drawPageHeading`): a heading "Section  >  Feature" with "< Section"; one
  page at a time; a click outside it closes it and goes on to the rows; they
  stay open when the window loses focus: a drag ends and an armed button
  disarms, nothing else. The tool views (Shapes, Waypoints, Schematics)
  share their header (`drawToolHeader`) and, docked, their frame
  (`drawDockedFrame`).
- **Footer**: always two description lines; longer text scrolls there (wheel
  or its scrollbar). No "read more", expansion or help panel. White says
  what a setting is (a number's help includes its range); faint says how to
  operate it. A newly selected row starts at the top of its description; a
  change in the same row's text keeps the place.
- **Experimental** features carry a small purple badge: the `experimental`
  flag in `SettingsRows.h`. When it comes off: BACKLOG "Release policy".
- **Resets** need a second press and exist per category and for all, not per
  setting ([SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md)).

## Text and languages

- English, Japanese, Simplified Chinese (`zh_CN`) and Traditional Chinese
  (`zh_TW`) for every user-facing key; other game languages show English.
  `zh_TW` is translated from English, not converted. Game terms follow the
  game's own text. Notes: [TRANSLATING.md](TRANSLATING.md).
- A label with a value is one string with `{}` and an ASCII ": "
  ("Radius: {}"); the table splits there.
- Draw text only through `ui::label` / `ui::paragraph` / `labelScaled`,
  never raw font calls. They handle the Japanese adjustments (Latin runs
  raised 1 unit, `boxTextInset()` inside frames, `shapeTextDrop()` for shapes
  beside text). These are fixed, not settings.
- Lamium draws its own text shadow half a unit away (the engine's reads as
  doubled on Japanese lines).
- A feature's name and its actions' labels may differ (a command says what
  it does; a child label may be shorter than its Hotkeys name).

## Before a design is done

- Works in all four languages, with the longest (often Japanese or English)
  fitting, and at a small GUI scale.
- Usable by keyboard as well as mouse; Esc does what the header says.
- Uses only shared controls and palette tokens.
- Says what happens when the feature is off, on world exit, dimension
  change and focus loss.
- New strings, settings and keys follow AGENTS.md rules 4 to 6.

## Feature decisions

One feature's decisions live in its document. A reference to DESIGN
"<section>" in older text means the section of that name below:

- HUD, HUD layout editor, target card, debug view: [HUD.md](HUD.md)
- World overlays: [OVERLAYS.md](OVERLAYS.md)
- Camera: [CAMERA.md](CAMERA.md)
- Visual effect visibility: [VISUAL-EFFECTS.md](VISUAL-EFFECTS.md)
- Automatic attack and use: [AUTOMATION.md](AUTOMATION.md)
- Light overlay: [LIGHT-OVERLAY.md](LIGHT-OVERLAY.md)
- Resetting settings, Keys: [SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md)
- Cave and Nether terrain reference, World map layer switches, World map
  side panel lists, Waypoints and death records: [MAP.md](MAP.md)
- Inventory transfer: [ITEM-TRANSFER.md](ITEM-TRANSFER.md)
- Repeated villager trading: [TRADE-HELPERS.md](TRADE-HELPERS.md)
- Edge Guard: [RESTRICTIONS.md](RESTRICTIONS.md)
