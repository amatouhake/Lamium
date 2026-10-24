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

All colors come from `ui::palette` in `src/ui/Widgets.h`:

| Token | Use |
|---|---|
| `panel` (opacity .8) | Screen background |
| `text` / `dim` / `faint` | Primary text / secondary text and icons / hints |
| `accent`, `accentDeep` | Switch on, selection, links, primary buttons |
| `off` | Switch off track |
| `shadow` | HUD text shadow |
| `keyFill`, `keyEdge` | Key caps |
| `experimental` | "Experimental" badge |
| `warning` | Warnings (e.g. Simple graphics notice) |

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
- **Child settings** are indented with a vertical guide line. Expanding or
  collapsing keeps the clicked row where it was. The wheel scrolls without
  moving the selection.
- **Sidebar** with categories; Hotkeys and Shapes are pinned at the bottom.
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
- **Experimental** features carry a small purple badge.
- Freelook, Auto Attack, Auto Use, Light Level Overlay, Permanent Sneak and
  Permanent Sprint have no Experimental badge after maintainer use. For
  0.3.0 (decided 2026-10-22) FreeCamera, Hand Restock, Tool Protection, Auto
  Elytra and Edge Guard lose it too after server use; their remaining limits
  stay in the README's known issues (Edge Guard: rare drops under heavy lag
  or a very small margin). Inventory transfer keeps it and is off for new
  installs (a saved on stays on); none of the features reviewed for 0.3.0
  keeps the badge while on by default. Radar and Waypoints remain on inside
  the Experimental Map. Fake Offhand, Death layout restore,
  the version-sensitive visuals, Game language, Skip server packs, Map and
  Schematics keep the badge.

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
  (white says what a setting is, faint says how to operate it). All three
  languages use the same behavior. This replaces the in-place expansion
  and the earlier separate-panel contracts of the same date.

- English is the default; Japanese, Simplified Chinese (`zh_CN`, L-90) and
  Traditional Chinese (`zh_TW`) follow the game language, and other languages
  show English. Every user-facing key has all four. Settings labels keep an ASCII ": " between
  name and value in every locale (the settings table splits there).
  Traditional Chinese is translated from English on its own, not converted
  from Simplified Chinese. Contributor notes: [TRANSLATING.md](TRANSLATING.md).
- Japanese locale: Latin runs are raised 1.5 units to share the baseline, and
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

## HUD (Decided — see [demos/hud-editor.html](demos/hud-editor.html))

Menus are polished; HUD is minimal; debug views may be dense. Everything the
HUD shows is a **HUD element**, placed and styled the same way. Where the
demo and this text differ, this text wins. The first demo
([demos/hud.html](demos/hud.html)) is superseded where they differ; the
rework came from using the first editor build (2026-09-24).

Elements: **Info lines**, **Target**, **Status**, **Toast**, **Zoom
magnification**, **Durability**, **Minimap** and **Schematic HUD**. Feature
docs describe the contents of the latter elements. The **Debug**
panel is fixed to the screen edges and is not an element (see its bullet).

Placement (every element):
- Stored as an anchor (9 presets) plus an offset in GUI units, but the user
  never picks an anchor directly. Where an element is dropped decides it:
  on each axis the nearer screen edge gives the anchor, so an element grows
  away from that edge (info lines placed bottom-left grow upward); it is
  centered on an axis only when its gaps to both edges are within a sixth of
  the screen of each other. (Revised 2026-10-06, L-98: the screen third of
  the center made a tall Info HUD near the top anchor to the middle, so it
  moved when lines were added or the line height changed.) There is no "pin". Why: anchor + pin + offset was the
  implementation showing through; people only want to put things somewhere.
- Elements can sit flush against the screen edge. Dragging snaps at the edge
  and at a 4-unit inset from it, and to the center lines; defaults and the
  3x3 "snap to" buttons use the inset. Why: flush must be possible, but a
  small gap reads better and makes aligned layouts easy.
- Look: scale 75–150 %, background none / card, text shadow.

Editing:
- Entry: the pinned "HUD layout" item in the settings sidebar, an unbound
  "Open HUD layout" action under General, and a "Placement and look" link row
  on each HUD feature that opens the editor with that element selected.
  The settings list has no placement or look rows of its own.
- A corner bar holds "Reset" and "Done (Esc)". Reset returns the selected
  element to its defaults at once (the Info HUD's also restores its line
  order, L-221); with nothing selected it resets the whole
  layout (including the info-line order) and asks for a second press.
- Popovers (snap-to, lines) never cover their toolbar or the Reset/Done
  strip: they open on the side that fits and scroll (wheel or scroll bar)
  when there is not enough room.
- The editor shows the live HUD (sample content for empty elements). Click
  selects, drag moves, arrows nudge (Shift: 10), Esc deselects then leaves.
  A dashed guide joins the anchor point and the element while selected.
- Selecting an element opens a small toolbar next to it (below, or above
  when there is no room): snap-to 3x3, scale, background, shadow, and for
  info lines a "Lines" popover (switch + up/down per line). The toolbar
  stays where it is while only the look changes and re-anchors after a move
  or when the element grows over it. Why: a side panel that follows the
  selection jumps across the screen and is disorienting.

Card look (background "card"): Bedrock popup style — a dark translucent
fill (about 72 %) with corners stepped by one GUI pixel, no border, no
accent stripe.

Contents:
- **Info lines**: providers with an id, a label and a value; unavailable values
  say so. Each line has a switch and a position in a user-ordered list.
  Defaults on: exact position, facing, biome, FPS. Everything else starts off.
  Names follow the game's own position line (2026-10-19, L-223): "Position"
  / 位置 is the whole-number block position, as Show Coordinates prints it;
  "Exact position" / 正確な位置 has decimals; the target card's row is
  "Position" too; Debug View keeps "XYZ" and "Block" (Java's debug screen).
  Default order, also the settings rows' order (2026-10-18, L-219): position
  (exact position, position, target block, scaled coordinates, chunk,
  dimension, biome), view (facing, rotation, yaw, pitch), movement (speed,
  horizontal, vertical, sprinting), surroundings (light, weather, time,
  moon, difficulty), then FPS, frame interval, ping and real time. A saved
  order is kept; a line added later goes after its default neighbor. The
  settings rows always use this order; the HUD layout's lines list is where
  the HUD order is changed.
- **Target** (Jade/WAILA role): icon, name, identifier line, then rows. What
  is shown is chosen by independent rows in the settings list: icon, ID,
  health (hearts / bar / number), armor (icons / bar / number), growth
  (bar / number), other details. Armor points use the vanilla armor-bar
  sprites; the bar mode uses the color sampled from the icon (#B8B9C4, the
  `armor` palette token). Bedrock exposes no armor toughness, so points only.
  There is no separate "card / simple" style; the element's background
  setting decides whether it has a card. When the target changes, the card
  background eases to its new size and position in 0.1 s; the content is
  always drawn at once (hiding it blanked the card while the view moved).
  Mobs use their spawn egg as the icon; hearts use the game's health-bar
  sprites. In Hearts mode, one full heart represents 2 HP and the number of
  heart slots follows the target's maximum health (L-88, decided 2026-10-02);
  current health fills those absolute slots, including half hearts, rather
  than normalizing every target to the same fixed heart count. Hearts wrap
  ten per line, filled top line first, up to five lines (100 HP, every
  non-boss vanilla mob); a target with more maximum health shows the bar
  instead, with the exact number beside it as always (decided 2026-10-02).
  Bar and Number modes keep their existing semantics.
  While Freelook or FreeCamera is active the card follows the camera: the
  nearest block or entity box along the rendered camera's forward, water and
  lava excluded.
- **Target and Schematics** (decided 2026-10-15, revised the same day): one
  card with the ordinary layout, no second section or column. The maintainer
  found the stacked and side-by-side sections hard to read and kept only the
  setting and the ghost target. Target Info has "Show schematic information"
  (on by default); off, the card neither picks ghosts nor shows differences.
  A ghost hit before a real block is the target: its icon, name (in the
  ghost's light blue), ID, coordinates and states, plus "Schematic: Not placed
  yet". The placement name is not shown. A real block that differs from the
  selected placement at the same cell gets rows in the check's colors: first
  "Schematic: Wrong block / Extra block / Wrong state", then
  "Should be: <icon> <block>" (red; air for an extra block), or one row per
  differing state, current -> expected with the drawn arrow (yellow), which
  stands in for that state's ordinary row. Schematic rows come first, right
  under the header, so a short card keeps them. Saved container contents and
  expanded inspection remain L-184 follow-ups.
- One "Range" (2-64 blocks, default 6) for every viewpoint: the card picks
  along the body's view normally and along the camera during Freelook and
  FreeCamera. Why: one mental model ("this far from where I look"); the
  game's own "reach" was not a usable distance (its hit result runs past the
  outline, its pick range from a camera was about 4), and 6 is close to
  survival reach so distant things do not clutter the card.
- **Animations** (General): one Lamium-wide setting, "Follow Minecraft"
  (Video > Screen Animations, the default), On or Off. Why: one switch for
  every Lamium motion, and people who turned animations off in Minecraft
  get the same from Lamium without looking for it.
- **Status**: automation (periodic attack/use, permanent sneak) and breaking/
  placement restriction lines in one element, each with a colored marker
  (accent for automation, warning color for restrictions).
- **Line height and per-line backgrounds** (BACKLOG L-98, decided
  2026-10-06): the Info HUD, Status and Debug View share one "HUD line
  height" setting, 9-16 GUI units (default 12, chosen in game; it was 14
  before; 9 is Java's debug screen). Info, Status and the player list (L-128) add "Per line" to their
  Background choice, and the Debug View gets "Background: None / Per line":
  each line gets a band of its text width plus 1 unit each side, rows touch,
  text centered in the band. On an element anchored to the right side the
  bands and their text line up on the right edge. One "Background opacity"
  setting (default 60 %, chosen in game) sets every HUD card and line band.
  Defaults (chosen in game): Info HUD per line, Status on a card, Debug View
  per line. Saved layouts keep their own background.
- **General** (2026-10-06, L-98): three headings - "Settings screen" (its
  key, animations, the Hotkeys and HUD layout openers), "Toggle toasts" (the
  switch, then the toast's layout link) and "HUD text" (line height,
  background opacity). The last two have no key of their own, so their
  switch, if any, is the first row.
- **Debug** (BACKLOG L-54, decided 2026-09-27): a Java-F3-like two-column
  panel, off by default, toggled by F3. It is not a HUD element: the left
  column hangs from the top-left screen inset and the right column from the
  top-right (moved down only as far as a long left line beside it needs, so
  it never leaves the screen at a large UI scale), both plain with shadow,
  and the layout editor neither shows nor edits it (it is big and would get in the way; its placement is the
  feature's, not a user choice). Left column: versions (Minecraft,
  LeviLamina and Lamium; LeviLamina added 2026-10-06), fps/frame/ping,
  position, block and chunk, facing with yaw/pitch, light, biome with
  difficulty, day/clock/weather/moon, then the look-at target. Right column:
  client settings (dimension, render distance, ray tracing and Vibrant
  Visuals, clouds and skies, fullscreen, max frame rate) and local PC
  information (memory, CPU, GPU, display, OS); a line disappears when the
  machine cannot provide it. The panel draws independently of Info lines and
  Target, which keep their own settings. A "Row name style" (項目名の書き方)
  option switches the line names between the game language and Java-F3
  abbreviations (default game language); FPS and Ping read the same in both.
  Two child options (both on by default) hide the Info HUD and the Target
  card while Debug is open, so the panel cannot overlap them; the layout
  editor always shows every element. A third option turns the panel's own
  text shadow off (the layout editor is not available to it).
- **Toast**: when a hotkey switches a feature, show the feature name with its
  toggle switch for ~1.5 s, dimming over the last 0.3 s; default position
  above the hotbar (bottom center). One at a time; a new one replaces the
  old. No `[switch]` marker text. Card background by default, fading with
  the toast. Fires only after the new state is saved. Setting to
  turn it off. Changes made inside the settings screen do not toast.
- The HUD is hidden while the settings screen is open (the overlap hurt
  readability); the layout editor shows it on purpose.
- The gameplay key-hint overlay is removed; an "Open Hotkeys" action replaces
  it.
- Dedicated actions that open Hotkeys, Shapes or HUD layout do not replace the
  ordinary Settings action's remembered page merely by being opened. Manual
  sidebar navigation from a dedicated view does update that remembered page.
- Bounded numeric settings where "about this much" is enough (ranges,
  zoom) use a slider styled like the Bedrock switch: green filled track, a
  light square knob with a bottom bevel, the value to the right. Click or
  drag the track; Left/Right step; clicking the value types a number.
  Settings that need precision (periodic intervals, shape coordinates)
  keep the arrows and number entry.
- Element appearance options are scale, background and shadow; HUD cards and
  line bands also use the shared Background opacity setting described above.
  Text color is not a per-element option.

Chat placement (L-174, decided 2026-10-14): a saved, default-off Chat
position switch moves ordinary HUD messages to the lower left and adds a
position-only element to HUD layout. The default is flush left with its
bottom 44 GUI units above the screen edge, leaving a 4-unit gap above two
default-size Durability HUD rows (chosen 2026-10-14). Saved
placements are kept; this is a fixed preset, not automatic avoidance.
Native text, background, font size, shadow, fading and input/history screen
remain vanilla. See
[CHAT-POSITION.md](CHAT-POSITION.md) for the first step's boundary.

## World overlays (Decided unless noted)

- (Decided 2026-10-12) Against the vanilla block selection outline (the black
  frame on the targeted block): position markers win, building guides yield.
  Chunk Borders, Hitboxes and the schematic corners and save area draw over
  it; Shapes and the light overlay let it show over them; the breaking
  restriction leaves the targeted block out (unchanged). Depth.h holds the
  pulls that set this order.

- Block-grid shapes show the blocks that form the shape rather than smooth
  wireframes. Wireframes are for chunk borders and hitboxes.
- Shape style: faces (default, faint outline) or lines. Faces are unlit,
  two-sided, inset 0.005 into their cell, and the whole shape is scaled 0.997
  toward the eye to avoid z-fighting. Do not widen the inset.
- Face material follows the graphics mode (Fancy: hologram pointer, two-sided;
  Simple: lightning, one-sided, with a warning in the Shapes view; Vibrant
  Visuals: full-strength lines). See `docs/OVERLAYS.md`.
- Shape colors: cyan (default), yellow, pink, white. Drafts use a dashed cyan.
- Meshes are rebuilt only when a shape changes, never per frame.
- (Decided) Line overlays get per-batch colors so chunk borders and hitboxes
  can follow Java's F3+G / F3+B color coding (see BACKLOG L-09 to L-11).
- (Proposed) Overlays must not hide vanilla's block selection outline: skip or
  dim geometry on the targeted block.
- (Decided 2026-10-07) World-space overlays (chunk borders, hitboxes, light
  levels, Shapes, schematic ghosts) stay visible with vanilla Hide HUD (F1).
  Screen HUD elements and in-world waypoint markers hide with it (L-106, L-107).

## Camera (Decided 2026-09-26 unless noted)

- (Decided, BACKLOG L-38; lower limit revised 2026-09-30, L-80; wheel range
  widened 2026-10-06, L-99) Zoom goes up to 50x. The initial magnification
  setting stays 2x-50x (a saved value below 2x loads as 2x), so pressing Zoom
  always visibly zooms. While held, the wheel goes from 1x to 50x; with the
  Zoom setting "Wide-angle view" (off by default; decided 2026-10-11, L-140)
  it goes down to 0.5x: below 1x the view widens, but never past 160 degrees
  (the wheel stops there). Turning the setting off brings a kept level below
  1x up to 1x. A
  wheel notch multiplies or divides the target magnification by a fixed ratio
  (about 1.15), so every notch feels the same at 2x and at 40x; a notch that
  crosses 1x stops on exactly 1x once. The shown magnification eases toward
  the target over a short time instead of jumping; turn sensitivity follows
  the shown magnification above 1x and stays normal below it.
- (Decided 2026-10-06, L-99) The wheel level is kept between presses until
  the world is left, including levels below 1x and across dimension changes.
  A level left at exactly 1x is the exception: the next press starts from the
  magnification setting, so Zoom never opens without a visible change.
- (Decided 2026-10-06, L-99 follow-up) The wheel changes the magnification
  only while the Zoom key is held. In Hold mode that is the whole zoom; in
  Toggle mode the wheel keeps its normal job (hotbar) while Zoom is on, and
  holding the key adjusts it. Toggle mode switches on at the press and off at
  the release of a plain tap; a hold that used the wheel leaves Zoom on.
- (Decided 2026-10-06) Pressing and releasing Zoom switch at once, whatever
  the Animations setting. An ease-in/out (0.1 s) was built and tried; it felt
  wrong without a frame like the spyglass's, and following the setting would
  have made it the default. Only wheel changes ease.
- (Noted 2026-10-06) Below 1x the held item drifts toward the screen center
  because the hand follows the wider projection; accepted as is for now.
- (Decided, BACKLOG L-45) While zooming, the magnification is shown as "×12.5" by its own HUD element
  (small, dimmed, below the crosshair by default; movable in the HUD layout);
  a Zoom setting turns it off (default on).
- (Decided 2026-09-26) The toggle toast's default sits above the armor and
  absorption rows (bottom center, 72 up) so it does not cover them.
- (Decided 2026-09-27, BACKLOG L-39/L-48) Freelook saves the current
  perspective (first, rear third or front third), switches to its configured
  starting view (rear third by default), and restores the saved perspective on
  exit. The player may switch perspectives with F5 while Freelook is active.
  FreeCamera offers Activation: Hold / Toggle, default Toggle. A wheel binding
  toggles in either mode because a wheel impulse cannot be held.
- (Decided 2026-10-07, L-37) FreeCamera should show surrounding loaded terrain
  and caves from inside solid ground, as spectator rendering does. This must
  remain a camera/rendering change: retain the player's game mode, abilities,
  body and server movement. Unsupported rendering paths retain vanilla
  visibility. Implementation and runtime coverage are tracked in CAMERA.md
  and VALIDATION.md.
- (Decided 2026-09-26, BACKLOG L-47 and L-27) Zoom, Freelook, FreeCamera,
  Permanent Sneak and Permanent Sprint have no saved switch. Their switch
  shows whether they are wanted; the key or a click on the switch flips it,
  and the effect starts when gameplay allows (a click in settings takes
  effect after closing them). Menus pause Zoom, Freelook, Sneak and Sprint
  and they resume afterwards; FreeCamera in Toggle mode keeps its position
  through menus, settings and focus loss. Hold ends when its input is released
  or input ownership is lost. Focus loss keeps toggle modes wanted; death, dimension
  change and leaving the world turn them all off. Nothing is saved across
  restarts.

- (Decided 2026-09-30, L-26 follow-up) FreeCamera speed actions use concise
  detail labels (Increase / Decrease speed) and explicitly name FreeCamera in
  Hotkeys. A fresh sprint-key press during forward input starts a horizontal
  2x boost that continues after key release. Forward diagonals qualify;
  strafe-only, backward or cancelled forward input ends it. Vertical speed
  stays unchanged. Menus, focus loss and session exit cancel the boost.

- (Decided 2026-09-30, L-76) FreeCamera has a saved Position reference choice:
  Player (default, existing behavior) or World. World keeps the camera's world
  position independent of body movement while still accepting flight input.
  Changing the choice during a session preserves its current position. A new
  session starts at the current player's eye; world/dimension exit clears the
  anchor. This changes only camera offsets, never the player's position.

## Visual effect visibility (Decided 2026-09-30, L-42 follow-up)

Hide effects has a saved master switch with a toggle key (L-83) and
individual effect switches (optionally bound). Revised 2026-09-30 before release: the master
defaults off and every effect switch defaults on, so turning on the one
master switch hides everything; nothing is hidden until then. Master off restores drawing
and retains selections; child switches and keys only edit selections while
paused. The UI and key toasts make that paused state explicit. Rain and snow
includes rain-derived splashes. Particles hides all particles, including rain
splashes, but does not hide falling precipitation. Other water splashes stay
visible with only Rain and snow selected. Sound, weather state and particle
emission/ticking stay vanilla.
- (Decided 2026-10-12, L-148) "Nether portal screen" is another child, on by
  default like the others: it removes the purple screen in a Nether portal.
  The portal transfer, its timing and sounds stay vanilla.

## Automatic attack and use (Decided 2026-09-25, revised the same day)

One feature per mouse action, **Auto Attack** and **Auto Use**, each with
exactly one switch, one mode and one set of keys:

- **Switch**: the feature row's switch; its hotkey toggles it. On means it is
  working in the chosen mode. The switch is session state: never saved, off
  after leaving the world or restarting.
- **Mode** (saved): Periodic (clicks every N ticks), Hold (keeps the button
  pressed: mining, eating, shield, bow) or Fast click (while the physical
  button is held, clicks N times per tick). One mode at a time. A separate
  "next mode" hotkey cycles it (unbound by default). Changing mode while on
  releases the old mode's press first.
- **Settings**: Periodic interval 1-1200 ticks, default 10, shown as
  "10 tick (0.50s)"; Fast click 1-10 clicks per tick, default 1, shown as
  "1/tick (20/s)". Both rows are always listed; their labels name the mode.
- **Fast click only while the button is held** (saved switch, Fast click
  only): off by default, so Fast click acts while switched on like the other
  modes. Only Fast click gets this: Hold "while held" would be vanilla and
  Periodic did not need it (maintainer, 2026-09-25). Toasts/status say
  "Fast click (while held)" when it is on.
- **Keyed option rows** (2026-09-25): a setting and the hotkey that changes
  it share one row. The "Fast click only while held" row has its switch and
  key; the Mode row has its stepper (moved left of the key column) and the
  "next mode" key. Breaking Restriction's switch and mode rows carry their
  respective toggle and "next mode" keys the same way. Those actions get no
  row of their own under the feature;
  Hotkeys still lists them. Clicking the key cell edits the binding.
- **Nothing stops it implicitly.** Only the switch (row or hotkey) turns it
  off, plus leaving the world. A manual click takes priority while held and
  automation resumes after release (Hold presses again; Periodic continues on
  its schedule without saved-up clicks). Menus, focus loss, death, dimension
  changes and detached cameras pause it; it resumes when gameplay input
  returns. The status element shows "Auto Attack: Periodic", with
  "(paused)" while paused.
- Toasts name the mode: "Auto Attack: Hold  ON".
- Help text warns that servers may treat very fast input as cheating.
- Why: the first version had separate Periodic/Hold/Fast toggles plus a
  parent row without a switch, so it was unclear what was on, and stopping on
  a manual click felt arbitrary (maintainer review 2026-09-25). Reference
  behavior: Tweakeroo's periodic/hold attack and use and fast click (behavior
  only, no code).

## Light overlay (Decided 2026-09-25, docs/demos/light-overlay.html)

- Numbers plus color. Numbers lie on the floor, filled (not thin lines), and
  turn by quarter turns so their top points where the viewer looks (the
  detached camera's direction during Freelook/FreeCamera).
- Color by spawning, as a faint tint over the whole floor: red = monsters
  can spawn at any time (block light 0, stored sky light below 7), yellow =
  only at night (block light 0, sky light 7 or more), none = block light 1+.
  From minecraft.wiki (Bedrock); confirm in game.
- Numbers show block light by default; the setting also offers sky light or
  both (block above, sky below in blue).
- Range (revised after the first in-game check): radius 16 by default, 4-64
  in settings, the same distance up and down (flying high still shows the
  ground). Whole chunk columns are shown. Users lower it if their machine
  struggles.
- Centered on the camera while it is detached (FreeCamera), like Chunk
  Borders; digits turn with the camera's direction.
- Number direction setting (2026-09-26): follow the view (default) or a
  fixed north/east/south/west up. Following rebuilds nearest chunks first
  within a per-frame budget, so turning at a large range does not stall; a
  fixed direction never rebuilds on turning.
- Like shape faces, the tint sits a hair (5 mm) above the floor and the
  digits just above it (12 mm); the mesh is nudged toward the eye. The
  flicker seen in Simple and Vibrant Visuals was never the height: (1) every
  quad had both windings on already two-sided materials, so two copies at one
  depth fought (reverse winding now only for Fancy), and (2) digit strokes
  overlapped at their corners in one plane (digits are now non-overlapping
  cells of a 3x5 bar grid), and (3) tint and digits a few millimetres apart
  run out of depth precision far away. Tints, digits and Vibrant's line digits
  are separate meshes nudged toward the eye by growing factors (.997, .995,
  .993), so their depth gap grows with distance like Shapes' own nudge.
- Drawn like Shapes: meshes kept per chunk, nudged toward the eye so they do
  not flicker against the floor, and as faint as shape faces on the additive
  material (Simple, Vibrant Visuals).
- Reading is spread over frames: a bounded number of chunk columns per frame,
  columns next to the viewer re-read every 0.25 s, the rest every 2 s.

## Resetting settings (Decided 2026-09-26, BACKLOG L-46)

- No per-setting or per-feature reset (vanilla has none either). "All" has
  "Reset all" (every setting, key bindings and HUD layout; not Shapes), each
  category (Camera & view, Inventory, ...) has "Reset category" (its features'
  settings and HUD placement; not key bindings), and Hotkeys has "Reset all
  keys". They sit on the column-heading line and need a second press to apply.
  The HUD layout editor keeps its per-element and all-element resets.
- (Decided 2026-10-12, L-159) Hotkeys has "Find by key" on the same line,
  right next to the key column, with "Reset all keys" to its left (the
  destructive button further from the frequent one): press it, then a key,
  mouse button or combination; the list shows only actions whose binding
  uses every input pressed (Z finds Z and Ctrl+Z). The search field then
  has focus and shows "Key: Z" as one unit: Backspace clears it, typing
  starts a text search instead, Esc clears it, Down/Enter go to the results.
  Typed text search does not match key names (one letter would match too
  many names).
- (Decided 2026-10-12, L-159, demos/binding-editor.html A) While a key is
  being assigned, Clear / Reset / Cancel sit on the edited row, ending just
  before its key cell; they take the place of that row's switch or stepper
  until the edit ends. The footer keeps the action's description and the
  "press, then release to assign | Esc: cancel" line.

## Cave and Nether terrain reference (Decided direction 2026-10-19, L-228)

- Represent the cavern the player occupies: include the current ledge/path
  and the deeper floor or lava within that open space. Large Overworld
  caves share this goal with the Nether. Narrow caves should communicate
  their floors, walls and forks.
- Moving up or down within the same space should preserve the recognizable
  terrain outline. Separate vertically stacked spaces must remain
  distinguishable; height differences should be readable.
- Where a bridge or ledge overlaps a lower floor, prefer the floor the
  player currently occupies. On a bridge, preserve its path and show the
  deeper floor/lava in the surrounding open parts; below it, prefer the
  lower floor. The exact visual treatment and selection/transition
  algorithm remain open.
- Unknown terrain, walls and deep drops must read differently from floors.
  The existing received/recorded-terrain boundary and bounded-work
  requirements apply. V1 uses three reads only: floor, wall and no-floor
  (deep drops and unknown terrain share the dark no-floor state).
- Persist explored Overworld cave terrain in 16-block bands for later
  world-map viewing, including after leaving and rejoining. Retain the
  surface map alongside it. Vertically overlapping spaces within one band
  are not distinguished in v1; separate bands are.
- The world map's cave controls were decided 2026-10-19 (D,
  [demos/cave-map-simple.html](demos/cave-map-simple.html)):
  - The top bar has one terrain state button (`地表` / `洞窟 広 Y68` /
    `洞窟 層 Y68`). Its panel opens directly under that button. It does not
    close the waypoint list; when space is tight it shifts left instead.
  - The panel has three rows: cave on/off, broad on/off, and height with
    `-` / a directly editable number / `+`. A separate action button,
    `自分の高さに合わせる`, returns the height to the player.
  - The map opens at the player's height (the FreeCamera eye when it owns
    the view). Changing the height fixes it; the action button returns to
    the current reference height. There is no follow toggle, slider,
    recorded-height list or three-way mode.
  - Standard: the Nether starts broad, an Overworld cave starts in the
    selected layer. The minimap stays broad and follows the player.
  - The layer view keeps the selected height bright and dims farther floors
    to a solid dark color; there is no checker or dot pattern.
  - A FreeCamera reference is separate from the body: opening and height
    matching use the camera, "Center on me" returns to the body, and a green
    camera arrow marks the camera. The arrow is display-only; closing and
    reopening the map returns to the camera. The panel names the active
    reference (`カメラ` / `本体`). Body and camera references each decide
    cave/surface at their own position; a cave map opens at the stable band
    height the recorder keeps, not at a boundary band that has no data yet.
  - The map does not switch between cave and surface while it is open; the
    initial choice is made when it opens.
- Layer switches keep their names while they fit; they become symbols only
  when the bar is too narrow.
- This is a decided direction, implemented on branch
  `feat/map-cave-freecamera`; the final checklist passed in game on
  2026-10-20 (`7e0e2f9`). MAP.md keeps the current behavior and the
  implementation record.

## World map layer switches (Decided 2026-10-15, L-146)

- The world map's top bar has three joined switches, Waypoints, Schematics
  and Shapes, each with its mark's symbol (diamond, cyan footprint, yellow
  footprint) before the name, left of "Center on me". On: the accent fill
  and frame of a selected button; off: dark with faint text and symbol.
  They are remembered across openings in the map settings, not a settings
  screen option. On a screen too narrow for the names they show their
  symbols alone, and the bottom bar names the one under the pointer
  ("Show Schematics on the map: On"); only when even that does not fit do
  they go to the second row (maintainer 2026-10-15, after two rows appeared
  at UI Profile 100%). The cave terrain button (L-228) sits on the first
  row; the second-row fallback is unchanged.
- A switch that is off removes its marks from the world map entirely (no
  faint ghost, no click or menu); the death point goes with Waypoints. The
  side panel's list and editor work as before; the minimap and the world
  view are not affected.
- The side panel's button is named "List" so it does not repeat
  "Waypoints".

## World map side panel lists (Decided 2026-10-15, L-204)

- The side panel lists one layer at a time: waypoints (with the death
  point), placed schematics or shapes. Its header names the layer with its
  count and a small down arrow; clicking it opens a menu of the three
  layers, each with its symbol and count (maintainer: a menu, since tabs
  side by side do not fit the panel). The choice is remembered. Selecting a
  mark on the map shows its layer's list; "+" shows only for waypoints.
- Placement and shape rows match waypoint rows: symbol in the mark's
  color, name, distance when the player is in that dimension, nearest
  first, and the show/hide switch; a click centers the map and selects it
  (a placement also becomes the schematic's selected one), with the
  existing details and "Open" below.
- Only the dimension the map shows. A layer switched off in the top bar
  keeps its list, with a faint "Hidden on the map" line on top.

## Inventory transfer (Decided 2026-09-27, BACKLOG L-41)

- The feature has one saved switch, off for new installs since 0.3.0 (a saved
  on stays on), and an unbound toggle action usable in gameplay or an item
  screen. Transfers act in screens
  with a player inventory and another side; the inventory screen moves between
  main inventory and hotbar. Four original child
  switches enable ordinary wheel, Shift+wheel, Shift+left drag and Ctrl+left
  drag independently; all default on. An off gesture passes through to vanilla.
- Wheel up sends one item into storage; wheel down sends one item into the
  player inventory, regardless of which side is hovered. When hovering the
  source side, send from that slot through vanilla auto-place. When hovering
  the destination side, bring one matching item from the highest source slot
  into the hovered stack. A full hovered destination stack accepts nothing.
- Shift+wheel sends one stack of the hovered item kind in the same direction.
  Pick the highest eligible source slot first and leave the hovered source
  stack for last. Use vanilla auto-place for the destination, including when
  the hovered item is already on that side.
- Shift+left drag transfers the entire stack from every passed slot to the
  other side. Ctrl+left drag transfers one from each passed slot. Shift+right
  drag leaves one when its new child switch is on; Ctrl+right stays vanilla.
  A slot is visited once per entry during a drag. Ctrl takes precedence when both modifiers
  are down. Unmodified clicks and drags stay vanilla.
- Requests go out together (revised 2026-10-15, L-169; before: one at a
  time, each confirmed before the next): every queued move, up to 64 per UI
  tick, calls the screen's vanilla auto-place or explicit slot-transfer path
  in the same tick, so vanilla sends them as one batch. Lamium then waits for
  all of their responses and checks every touched slot against the
  authoritative state once. Moves gathered meanwhile (drag, wheel) form the
  next batch. Why: a resource pack's bulk move, traced on 2026-10-15, sent up
  to 44 auto-place requests in one frame and the game answered them in one
  response; waiting for each answer made Lamium's moves visibly one by one.
- Each move's local prediction must still be a conserved move of its own
  source; a move that predicts nothing (no room) is skipped. Close/focus
  changes, a cursor item, text editing, feature disable, a rejected or
  timed-out request, an unexpected prediction or a changed source stop the
  operation and drop what is still queued; vanilla rolls back its rejected
  predictions. Mouse release stops gathering slots but lets visited ones
  finish.

### Bulk moves and more screens (Decided 2026-10-15 for 0.2.2, L-169; smoke-tested, unreleased)

- Ctrl + wheel moves every stack of the hovered kind, Ctrl + Shift + wheel
  everything, in the plain wheel's direction. Alt + left click and Alt +
  Shift + left click do the same from the clicked slot's side to the other.
  "Everything" from the player inventory moves one section: the hotbar
  when a hotbar slot is hovered, otherwise the main inventory without the
  hotbar (maintainer 2026-10-15, after the batched build's check).
- Shift + right drag moves all but one item from each passed slot.
- Transfers work in every screen that holds items beside the player
  inventory. Slots with roles get only what they accept; outputs are only
  taken from.
- Each new gesture has its own switch, on by default. Not chosen: Shift +
  click on an empty slot with a cursor item, a wheel "all but one", drop
  gestures, W/S moves.

### Held stack and drops (Decided 2026-10-15, L-169; passed in a chest on `9da5d9d`)

- Items on the cursor + Shift + left click on an empty slot: the held stack
  goes into that slot, then every matching item from the other side moves
  to that side; the hand ends empty. A slot that does not take the whole
  stack stops after vanilla's own placement.
- With the game's drop key held: Ctrl + left drag drops one item from each
  passed slot, Shift + right drag all but one, Shift + left drag the whole
  stack. The press of the drop key itself stays vanilla (one item, or the
  stack with Ctrl, from the slot under the pointer).
- Items on the cursor + Shift + left click outside the window: vanilla
  drops the held stack, then every matching item in the player inventory,
  hotbar included, is dropped too.
- Each has its own switch, all on by default (maintainer's choice; the
  agent had proposed matching drops off). Drops use vanilla's drop request
  and are confirmed with the batch like moves. Not chosen for now: Shift +
  double click, Ctrl + Shift + click, "fill existing stacks only".

Vanilla baseline reported 2026-10-15: chest inputs behave as before;
Shift + left click routes items by role in furnace, brewing stand and
beacon screens. A furnace fuel slot also accepted an item that cannot
burn. Native slot acceptance and routing, rather than a new Lamium fuel
filter, remain the baseline. Brewing/beacon restrictions were not checked
exhaustively. The e3f9276 diagnostic checkpoint
confirmed unchanged vanilla behavior in a large chest, furnace, brewing
stand and beacon and captured Alt and native handlers/orders; it is not
validation of the new Lamium gestures. Native discovery follows those orders
with same-player-side fallbacks removed. On c8c1f1b the maintainer confirmed
the normal implementation checklist, including the five new gestures and
switches and chest/furnace/brewing/beacon checks, all behaved as expected.
Checked screens in that smoke checklist: chest, furnace, brewing stand and
beacon. Individual combinations and the remaining screen families, servers
and controlled failure cases were not separately reported. Details:
[ITEM-TRANSFER.md](ITEM-TRANSFER.md).

## Repeated villager trading (Decided 2026-10-12, trigger 2026-10-13, L-166)

- Shift + right click on an unlocked trade row trades it as often as
  allowed, default on. Left click and plain right click keep vanilla
  behavior; any key or click stops a run.
- Use vanilla's trade request and confirm each round from authoritative
  inventory and offer state. Stop when sold out, inputs run out or output
  cannot fit; never drop output to continue.
- The maintainer chose this part on 2026-10-13 through its first required
  runtime checkpoint. Favourites and remaining-use presentation remain
  undecided. Research notes: [TRADE-HELPERS.md](TRADE-HELPERS.md).

## Edge Guard (Decided 2026-10-12, L-160/L-161)

- Stops walking off edges like sneaking, without sneaking, also on servers:
  the move input the server reads is guarded, not only the client's motion.
- Stops with the box at least a margin over the block, also while sneaking
  (a box that only touches an edge is no support on a server, and a server's
  position can drift a few hundredths further out than the client's). The
  margin is a setting under Edge Guard ("Stop inside the edge by"), 0.02 to
  0.30 block in 0.01 steps, default 0.10: smaller reaches further for placing
  blocks, larger is safer on servers (maintainer, 2026-10-12).
  Reconfirmed 2026-10-13 after L-162: keep default 0.10 and the existing range
  despite a successful BDS sneak-release check at 0.02; that one result does
  not establish the smaller margin's safety for all server corrections.
- A slab that would merge with the slab the player stands on and push them
  out sideways (their center beside that cell) is not placed; with the
  center over the cell the merge lifts the player and is allowed.
- Slippery blocks slide further than the guard expects (known limit).

## Keys

- (Decided) There is no general default-key policy to design now; defaults are
  decided per feature when needed. New actions ship unbound unless the
  maintainer picks a key.
- (Decided) Settings opens with `L` instead of F8 (BACKLOG L-01).
- (Decided 2026-09-27) Debug View defaults to `F3`, Hitboxes to `F3+B`, and
  Chunk Borders to `F3+G`, following Java's debug keys. NightVision defaults
  unbound; its old `J` key had no feature mnemonic and `N` belongs to Bedrock
  notifications. Existing saved bindings, including explicit unbinds, win over
  these defaults.
- (Decided) Lamium owns its key bindings; Minecraft's keyboard settings no
  longer list Lamium actions (BACKLOG L-23).
- (Decided, BACKLOG L-32) Ordinary action chords and modifier-like chords have
  different matching semantics. Ordinary chords are order-sensitive and a
  more-specific completed chord suppresses an overlapping shorter chord for
  that activation. Example: with `B` and `F3+B`, pressing F3 then B fires
  only `F3+B`; pressing B first may fire B immediately and is not delayed in
  case another key arrives later.
- (Decided, BACKLOG L-32) Modifier-like actions such as Zoom and Freelook allow
  unrelated held gameplay inputs so they remain usable while moving. Matching
  mode is an action property, not another advanced user option.
- (Decided, BACKLOG L-32) Exact duplicate bindings are valid. The Hotkeys UI
  warns that the actions share a binding, but all enabled actions assigned to
  that exact chord fire together. This intentionally permits grouped toggles;
  duplicate bindings are never silently prioritized or disabled.
- (Decided, BACKLOG L-32) Hotkeys UI also warns about subset/superset overlaps.
  Press/Hold/Toggle remains separate from chord matching semantics, and mouse /
  keyboard / wheel combinations follow the same overlap rules.
- (Implemented L-32, 2026-09-25) Details chosen while building it:
  - A chord is stored in press order; its last input completes it. Actions
    activate only on that completing press, never while keys merely stay held.
  - Both kinds tolerate unrelated held inputs (walking with W still allows
    another bound toggle). Ordinary chords differ by order and by yielding.
  - The most specific completed chord wins for everyone, modifier-like actions
    included (Ctrl+C beats C when Ctrl is held). A chord that yielded stays
    silent until one of its keys is released: no late firing on key repeat.
  - When a longer chord starts on top of an active ordinary one (B held, then
    B + click), the shorter one is released and does not resume. Active
    modifier-like actions (Zoom) keep running.
  - Settings files written before L-32 stored chords sorted by code. They load
    in the order modifiers, function keys, other keys, mouse, wheel (so a saved
    F3+B stays F3 then B); new files carry `orderedBindings: true`.
- (Decided 2026-09-25, after the L-32 playtest) Keys that begin a longer chord
  act on release, like Java's F3. A Press/Toggle action whose chord is the
  leading part of another bound chord (F3 with F3+B bound) does nothing on
  press and fires when released, unless a longer chord containing it fired
  while it was held. Unrelated keys (walking, other Lamium actions) do not
  cancel it; focus loss, menus and text input drop it silently. This is
  derived from the bindings, not a user option. Actions without such a longer
  chord still fire on press, and Hold actions (Zoom, Freelook) always act on
  press.
- (Decided 2026-10-06, L-97) Colliding settings: when one option's value
  defeats another (a fetch slot equal to Fake Offhand's target slot), every
  row involved draws its stepper frame and value in the warning color, and
  selecting one shows the reason in the warning color in the footer. Same
  idea as hotkey conflicts, for values.
- (Decided 2026-09-25, docs/demos/hotkey-conflicts.html) Conflict display.
  Every key cell (Hotkeys and feature rows alike) draws related bindings with
  warning-colored key caps: filled for the exact same chord, outlined for an
  overlap. No text badge beside the keys (it looked like another key cap). Hovering
  the key cell opens a tooltip below it (above when there is more room) that
  lists every related binding grouped as same key / starts with this key /
  longer chords containing it / shorter chords inside it / same keys in
  another order, plus "fires on release" when that applies; overflow ends in
  "N more". Keyboard navigation shows the same tooltip for the selected row
  (keyboard users are not told to use the mouse); moving the mouse returns to
  hover. The footer keeps only the feature/action description.
