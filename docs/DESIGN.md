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
- Every feature restores vanilla behavior when switched off, on world exit
  and dimension change. A feature that holds input or changes game state
  also does on focus loss, so no key or state stays stuck. Draw-only changes
  (HUD placement and hiding) are decided per draw and are kept while the
  window is unfocused.
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
| `panel` | Background, by what is under it: the world `opacity::screen` (.8: the screen, docked panels, prompts, the schematic radial and its cards, the HUD editor's toolbar, idle panel and popovers); another Lamium panel `opacity::floating` (.97: tips, menus, the world map's side panels, the calculator's copy menu and help); the world map's bars .85 |
| `text` / `dim` / `faint` | What it is / secondary text and icons / how to operate it |
| `accent`, `accentDeep` | On, selected, links, primary buttons |
| `off` | Switch off track |
| `knobOn`, `knobOff` | Switch knob |
| `keyFill`, `keyEdge` | Key caps; plain button fill and edge |
| `experimental` | Purple: only the Experimental badge |
| `warning` | Yellow: only warnings (conflicts, refused values, notices, a schematic block in the wrong state) |
| `danger`, `dangerEdge`, `dangerText` | Red: only destructive actions: armed fill, edge (hovered or armed), hovered text |
| `info` | Light blue: reference, not Lamium's own content: drafts, ghosts, waypoint names on the map, the game's own HUD parts |
| `mismatch`, `wrongState`, `notPlaced` | The schematic check's kinds, in the Check list and as mistake marks in the world: red for a block that differs from the file (not destructive, so not `danger`), yellow for one whose state differs (not `warning`), pale blue for one not placed yet |
| `cornerOne`, `cornerTwo` | A schematic area's two corners, as their marks in the world |
| `shade` | Black scrims, field backs and outlines; each use sets its opacity |
| `controlEdge`, `gripOn`, `gripOff` | Switch and slider parts only |
| `syntaxOperator`, `syntaxUnit`, `syntaxVariable`, `syntaxFunction` | The calculator's expression colors |
| `shapeCyan`, `shapePink`, `shapeYellow`, `shapeWhite` | The shapes' colors; the anvil's left (cyan) and right (pink) stacks |
| `mapGround` | The world map where nothing is recorded |

Colors sampled from the game (`heart`, `heartEmpty`, `armor`, `curse`,
`tooltipBorder` for the container preview's frame) live in
`palette::vanilla::`: they only match what the game draws and carry no
Lamium meaning. `window`, `windowInk` and `windowFaint` are the game's
window grey and the text on it: a Lamium window that lies over a game
window and takes its place (the recipe view and the wanted items' window)
is drawn with `windowPanel`, opaque, as the game draws its own window
(`windowEdge`, `windowLight`, `windowShadow`; `ui/VanillaFrame.h`, measured
from the game), its text with `windowLabel` (dark, no shadow); the buttons,
slots and fields on it are the shared dark ones. An item Lamium shows in a
slot's stead has the game's tooltip box (`gameTooltipBox`: `tooltipFill`,
`tooltipLine`) and its lines in the game's colors (`textCode`). These screens copy the game fully or not at all: half a copy
reads as neither (maintainer 2026-11-19, L-366).
Text shadow comes from `labelScaled`, not a token.

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
the same commit. A screen never draws its own variant.

On a window of the game's grey (`windowPanel`, drawn inside `OnWindow`) the
shared controls take the game's look, so they sit in it as its own do
(maintainer 2026-11-19/20, L-367): action buttons are raised (`raisedBox`:
black edge, half a unit of white light and grey shadow) with dark text,
a disabled one flat and darker; a face sunk into the window (`sunkFace`,
`windowSunk`) holds a breakdown or a list of candidates; joined on/off buttons
(`drawOnOffButton`) and a switch keep Lamium's shape, green when on, their
off face the game's lighter grey with a black edge; tabs are the game's
(`windowTab`, below); a stepper has raised ends and its value in the slots'
grey. A tip there is the game's tooltip box. Elsewhere they keep Lamium's
dark look. No control plays a sound.

- **Switch** for every on/off, knob position carrying the state. No "ON/OFF"
  words, no check boxes, except one on a window of the game's grey that
  marks which of a few things is in use, where a switch would not sit
  in the game's look (maintainer 2026-11-21, L-379: a recipe's "use this
  recipe").
- **One of several** (`drawTab`, `tabRule`; decided 2026-11-19 from
  [demos/switching-controls-2.html](demos/switching-controls-2.html)):
  names side by side over a one-unit rule as wide as what they switch. On
  a dark Lamium panel the chosen one stands on a faint accent face over an
  accent bar, the settings sidebar's selected row turned sideways. On a
  window of the game's grey they are the game's tabs (`windowTab`, measured
  from the game's recipe book): blocks standing on the window's top edge,
  outside it, side by side sharing their black edges, with the window's
  stepped top corners and two units of light along the top and left, the others with a dark grey face, the chosen one
  in the window's grey with two units of shadow at its right, joined to
  it; pictures there are 16 units. Such tabs name the window, which then
  has no title line (L-367).
  Where the names do not fit, each is an item's picture, named under the
  pointer. Used in Recipes, on the Schematics screen's tabs and for the
  world map's dimensions. The Check tab's
  filters, also one of several, stay outlined pills: the same look twice
  in the same direction, right under the tabs, would be hard to read.
- **Each on or off**: a name and a switch. Where several are packed into
  a bar, joined buttons, green when on, pictures where the names do not
  fit (the world map's layers; "favorite" and "wanted" in the recipe
  view). A single switch that is not a row of a
  list (the open container's in the band of Recipes) stands in a frame
  of its own.
- **A list's scrollbar** (`listScrollbar`): a view without a layout of
  its own draws the shared bar, takes a press on it and follows the drag
  in one call; the rows leave it a gutter.
- **Slots on a window of the game's grey** (`windowSlot`): as the game
  draws its own, dark upper-left and light lower-right edges on its slot
  grey; what the player lacks stands on the game's red.
- **Key caps** for bindings. The key column never shows Hold/Toggle/Press.
  A binding that relates to another draws warning-colored caps: filled for the
  exact same chord, outlined for an overlap; hovering (or selecting with the
  keyboard) lists the related bindings in a tip. A mouse button alone (left,
  right, middle) is the game's own click: its caps are outlined too, the tip
  and the capture footer say which game action it shares, and the click still
  reaches the game, so both happen (a chord with a modifier takes the click).
  This shows in Hotkeys whatever the toast settings are. While a binding is
  captured the selected row's fill and frame mark it, so its cell adds no fill
  or frame: "Press keys..." faint where "Unbound" sits, then the keys held as
  the caps they will be, each followed by a text field's blinking caret.
- **Steppers** (`drawStepper`): one box, `- value +` for numbers,
  `< value >` for choices (arrows drawn from rectangles); never separate
  `-`/`+` buttons. The end cell under the pointer lightens; an end that
  cannot step draws faint. Shift + an end steps ten of the stepper's own
  steps (a choice ignores Shift); a right click on it steps down like `-`.
  Where a value can be typed, a click on it types (the edit draws in the
  box); a typed value past either end is not applied while typing and
  becomes that end when the edit is kept. A value cut with "..." shows whole in a tip
  under the stepper. A value that defeats another setting draws its stepper in
  warning color, and the footer says why.
- **Buttons** take only a left click (a right click presses nothing, Delete
  included), three kinds of one shape: a fill, a 1-unit edge lighter than
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
  white text; the second press acts; losing focus disarms). Armed, it says
  what the second press does: "Confirm " + the action ("Confirm delete",
  「削除を確定」, 「确认删除」, 「確認刪除」), one whole string per action that
  reads naturally (a reset is 「初期化を確定」, not its label + を確定); a
  button too narrow for that reads just "Confirm" / 「確定」 / 「确认」 /
  「確認」. Built by `armedLabel` in the shared widgets, never worded by a
  screen. A button sized by its text is as wide as the longer of its two
  labels from the start (`armedWidth`), so arming moves nothing. What the
  press removes (a count, a size) goes in the text beside the button, not in
  its label, with one exception: a bulk delete names how many
  ("Confirm delete 5", 「5 件の削除を確定」, `confirmDelete(count)`).
- **Text fields** (`drawTextField`): a shaded back with a `keyEdge` edge,
  `accent` while typing; a placeholder faintly when empty. Enter or a click
  outside keeps the edit; Esc drops it and shows the stored value again.
- **Scrollbars** (`drawScrollbar`) on any list or text that overflows: a
  faint track and a 2-unit thumb that turns lighter (`dim`) under the
  pointer or while dragged, without widening. One bar (`ui/ScrollBar.h`) and
  one drag (`ui/ScrollDrag.h`, also beside the anvil and on the world map):
  a press on the thumb keeps its grip,
  on the track the thumb's middle goes to the pointer, and the bar follows
  the pointer until release. A press takes the bar on its column and 1
  unit to its left (`barReach`). While a bar shows, rows (background,
  selection, hover) and their columns, the settings table's too, end 4 short
  of the list's edge (`rowBarGutter`), clear of that zone with 2 units seen
  between row and bar (maintainer 2026-11-16: kept tight, a little harder to
  grab).
- **List columns** (L-190, `ui/ListColumns.h`, `drawListHeadings`): the
  Waypoints, Shapes and Placed lists, docked or not, share one shape: the
  mark (kind and color; a shape's mark is its type glyph, a placement's a
  footprint), the name, the distance from the player (to a waypoint as the
  Waypoints setting measures it, 3D by default; horizontal to a shape's
  center or a plane's origin, a footprint's center, since areas have no
  height; the dimension's name for another dimension), the list's own column
  (placements: progress) and the shown switch. Positions stay in the detail
  pane. A narrow list drops the distance first, then its own column.
- **Several rows** (L-190, `ui/RowSelection.h`): in a list that offers
  it, Ctrl + click toggles a row and Shift + click takes a range (no drag
  selection); Ctrl + A takes all; every selected row has the same
  highlight. One row: the detail pane shows it. Two or more: the detail pane
  becomes the selection pane (`drawSelectionPane`): "N selected", "Shown N /
  Hidden M", the names in list order as plain lines in the list's columns
  without the switch (mark, name, distance, own column; "N more" when they
  do not fit), and Show, Hide and Delete where the pane's own buttons sit,
  Delete (destructive; armed "Confirm delete N", the bulk-delete exception
  under Buttons) in the single pane's Delete place. No
  bar in or under the list. Esc lets several rows go (nothing selected),
  then closes. The map's side panel has the same pane with its buttons on
  its last row (`compactSelectionPane`). The Schematics Check list (L-183)
  has no shown switch, so no summary line and no Delete: Show in world,
  Ignore places and Stop ignoring share the row, its names sit under the
  preview it keeps, and a button that would change nothing is disabled.
- **Tips** open below their anchor (above when there is no room) and look
  the same everywhere (`drawTipBox`): the floating panel, a `keyEdge` edge
  (`warning` when the tip warns), 4 units of padding, white text. The
  locked-trade tip copies the game's own tooltip instead, and an item
  in the recipe screens has the game's tooltip box (`gameTooltipBox`).

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
  the row opens it too, a click opens it only on the button, and its key cell
  takes the opener's hotkey (Game language included). A row with a current value (Game language) shows it
  as faint text beside the button (Game language).
- **Pages** that replace the settings rows (Game language, Saved data, Block
  highlight, Enchantment order) share one shape (`RowPages.h`,
  `drawPageHeading`): a heading "Section  >  Feature" with "< Section"; one
  page at a time; a click outside it closes it and goes on to the rows; they
  stay open when the window loses focus: a drag ends and an armed button
  disarms, nothing else. The tool views (Shapes, Waypoints, Schematics)
  share their header (`drawToolHeader`) and, docked, their frame
  (`drawDockedFrame`).
- **Passing notices**: a toast is for what the player did outside a Lamium
  screen (keys, and the schematic radial menu, whose items are the same
  actions as their keys: every item toasts as its key would, seen once the
  menu closes) or what came from outside (a death
  layout restored, a waypoint reached, a background save finished). Inside
  a Lamium screen a result of the player's own action goes to that screen's
  status line (the settings footer, the tool views' footers, the anvil
  panel's status lines) with `screen::setInfo` (white) or `setMessage`
  (`warning`), never a toast; a screen without one (world map, calculator)
  shows it in the shared notice box above its bottom edge (`drawNotice`,
  2.5 s). A message or warning toast from outside raised while the settings
  screen is open waits and starts when it closes; one older than 60 s is
  dropped. The screen's own results (its menus and prompts) and on/off
  toasts never wait (L-339).
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
- Deleting what the player made or saved (waypoints, shapes, placements,
  saved data, the map cache) is "Delete" / 削除 / 删除 / 刪除, never 消す;
  labels use plain forms, not polite ones (no 〜します on a button).
- A feature's name and its actions' labels may differ (a command says what
  it does; a child label may be shorter than its Hotkeys name).

## Before a design is done

- Works in all four languages, with the longest (often Japanese or English)
  fitting, and at a small GUI scale.
- Usable by keyboard as well as mouse; Esc does what the header says.
- Uses only shared controls and palette tokens.
- Says what happens when the feature is off, on world exit and dimension
  change, and on focus loss if it holds input or changes game state.
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
