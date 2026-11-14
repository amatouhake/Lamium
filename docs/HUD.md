# HUD (Decided — see [demos/hud-editor.html](demos/hud-editor.html))

Moved from DESIGN.md on 2026-11-14 without changes; still the decided behavior.

Menus are polished; HUD is minimal; debug views may be dense. Everything the
HUD shows is a **HUD element**, placed and styled the same way. Where the
demo and this text differ, this text wins. The first demo
([demos/hud.html](demos/hud.html)) is superseded where they differ; the
rework came from using the first editor build (2026-09-24).

Elements: **Info lines**, **Target**, **Status**, **Toast**, **Zoom
magnification**, **Durability**, **Minimap**, **Schematic HUD**, **Player
list**, **Inventory**, **Used slots** and **Status effects** are Lamium's;
**Chat**, **Paper doll**, **Scoreboard** and **Boss bars** are the game's own
parts, in the editor only while Game HUD places them (L-334). Feature
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
- Look: scale 50–200 % (L-321; 75–150 % in steps of 25 before), background
  none / card, text shadow.

Editing:
- Entry: the pinned "HUD layout" item in the settings sidebar, an unbound
  "Open HUD layout" action under General, and a "Placement and look" link row
  on each HUD feature that opens the editor with that element selected.
  The settings list has no placement or look rows of its own.
- (L-321, decided 2026-11-11 on demos/hud-layout-v2.html.) Resets follow
  "Resetting settings": the selected element's "Reset" sits at the end of
  its toolbar behind a divider and applies at once, like the binding row's
  Reset (the Info HUD's also restores its line order, L-221); "Reset layout"
  (every element and the info-line order) asks for a second press. "Reset
  layout" and "Done (Esc)" sit in a small titled panel (title, one hint
  line) above the crosshair, shown only while nothing is selected. Why: one
  corner button that changed meaning with the selection was the editor's own
  invention, and every corner holds an element by default.
- Popovers (snap-to, lines) never cover their toolbar: they open on the side
  that fits and scroll (wheel or scroll bar) when there is not enough room.
- The editor shows the live HUD (sample content for empty elements).
  Elements that appear only by situation (target, automation status, toast,
  magnification, schematic, player list) are a dashed frame until selected,
  faintly filled, with a name tag outside a corner of the frame, the same
  tag the selected element has but gray instead of the accent color;
  pointing at a frame shows its content. Tags keep clear of each other
  (above the left corner, else below it, else the right corner), so
  overlapping frames stay told apart, and clear of elements drawn in full;
  a tag that would still cover the selected or pointed-at element is left
  out. The toolbar opens below a tag that hangs under its element.
  (2026-11-11, tried in game first: a
  name inside the frame's top left ran into other elements' text; a name on
  a dark plate in the middle looked out of place and two overlapping frames
  put their plates on each other.) Why: overlap is a problem in the editor, where everything shows
  at once, not in play; so there are no overlap warnings, no automatic
  avoidance and, for now, no element list.
- The selected (or dragged) element is drawn last, on top of the others,
  with a darker ground where it lies on another element, so it reads where
  elements overlap; in play the draw order is unchanged.
- Click selects, drag moves, arrows nudge (Shift: 10), Esc deselects then
  leaves. A click on the selected element's place selects the element below
  it; the selected element can be dragged even under another one. A dashed
  guide joins the anchor point and the element while selected. Dragging
  snaps to the screen edge, the 4-unit inset and the center lines only, not
  to other elements (tried in the mockup: it caught too often).
- Selecting an element opens a small toolbar next to it (below, or above
  when there is no room): snap-to 3x3, scale, background, shadow (a "name:
  value" button like background), for info lines a "Lines" popover (switch
  + up/down per line), then the divider and Reset. The toolbar
  stays where it is while only the look changes and re-anchors after a move
  or when the element grows over it. Why: a side panel that follows the
  selection jumps across the screen and is disorienting.
- Scale: the toolbar's - and + step 5 %, a click on the value types any
  whole percent, and square handles on the selected element scale it by
  dragging in 5 % steps. The anchor point stays put, so handles sit only on
  the corners away from it: one for a corner anchor, two for an edge, four
  for the center. The chat is placed only.

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
  every Lamium UI motion (screens, HUD), and people who turned animations
  off in Minecraft get the same from Lamium without looking for it.
  Drawings in the world follow their own feature's look instead (Block
  highlight: a still color stops the rainbow; decided 2026-11-09).
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
- **Toggle toasts' switch** (decided 2026-11-14, L-338) hides on/off notices
  only. A message that says what an action did (waypoint added, schematic
  saved, restore ended) and a warning show either way.
- **Esc returns to where the page was opened from** (decided 2026-11-14,
  L-338): a tool page opened by its key returns to the game, one opened from
  the anvil to the anvil, one opened from the settings to the settings table.
  This holds for the calculator, the world map, HUD layout, the block
  highlight list and Enchantment order; the schematic menu only opens over
  the game. A page's Back button always goes to the settings table.
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
  old. (Decided 2026-11-13, L-319) The toggles of one key press, which
  happen when keys are bound alike, share the toast instead: a row and a
  switch each, four rows at most, then a count. The key of a child switch
  is named "Feature > Child" (an ASCII sign: others change the font in
  English and sit low in Japanese) with the label the settings screen gives the
  child; there are no toast-only names. No `[switch]` marker text. Card background by default, fading with
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

Game HUD placement (L-321, 2026-11-11): what HUD layout places of the
game's own HUD is one feature with a saved, default-off main switch and a
detail switch per part (chat, paper doll, scoreboard, boss bars), each on by default
and with its own unbound toggle key, built like Hide effects: the main
switch off restores the game's places and keeps the selections. One
"Placement and look" link opens the editor at the chat. The paragraphs below
describe each part; where they say "switch" they mean its detail switch
under this one. The hotbar is not part of it (stopped, BACKLOG-DONE L-321).
The chat's default place stays the lower left (decided 2026-11-11), so
switching the feature on moves the chat at once while the doll and the
scoreboard stay until dragged. Why not the game's place, as for the other
two: the game does not put the chat at a fixed place. It is the last item of
a stack under the paper doll's room, the position line, the day count and
the game tips, so its height changes with what shows; a fixed default could
only imitate one of those states, and it would sit on the default Info HUD.
The lower left keeps clear of Lamium's HUD and matches Java Edition.

Paper doll placement (L-309, 2026-11-11): the game's paper doll is placed
by HUD layout, on the chat's pattern below. Position and scale (50-200 %); its default is the game's
desktop place and size, so switching it on changes nothing until the
element is dragged or scaled. The doll's look and when it shows stay
vanilla. In the editor the game's own elements (chat, paper doll) have a
warning-colored frame and no look controls; all three take a position and
a scale (2026-11-11; the chat was position only before).

Scoreboard placement (L-321, 2026-11-11): the game's scoreboard sidebar is
placed by HUD layout, on the chat's pattern below. Position and scale (50-200 %); the default is the
game's place, the middle of the right edge, at the game's size. It shows only where a world or server displays an
objective, so the editor treats it as a conditional element.

Boss bar placement (L-323, 2026-11-14): the game's boss bars are placed by
HUD layout on the scoreboard's pattern, position and scale (50-200 %). The
default is the game's desktop place, top center 2 units down, at the game's
size. The element is the box the shown bars fill (182 x 20 per bar, as the
game lays them out), so when bars are added or removed the anchor the drop
chose stays put: bars at the top grow downward as in vanilla, bars dropped at
the bottom grow upward. Hidden boss bars stay hidden wherever they are
placed. A conditional element in the editor, with sample bars drawn from the
game's bar textures. The growth direction was chosen by the implementer from
the existing rules when the maintainer asked for it.

Showing and hiding from HUD layout (L-334, decided 2026-11-14 with the
maintainer, mockup demos/game-hud-hide.html; revised the same day after the
first build):
- Every element with a switch can be shown or hidden from the editor. For
  Lamium's elements it is the feature's own switch (Info HUD, Target info,
  Status, toggle toasts, magnification, Durability, minimap, Schematic HUD,
  inventory, used slots, status effects); the player list has none (a held
  key).
- Each game part is one row under Game HUD placement with three values:
  Standard (the game's own), Placed (moved and scaled by the layout) and
  Hidden (not drawn), default Placed. The main switch off leaves every part
  the game's. Saved as the part switch and a show value per part. Each part
  has one key that steps Standard, Placed, Hidden like Tool Switch's drop
  priority; with the main switch off it changes the choice only and says so.
  The boss bars' key is the old Hide boss bars key; the separate boss bar
  placement key is retired.
- Hiding skips the part's draw; the game's layout and its state stay. The
  chat hides only the HUD messages; the chat screen stays vanilla.
- The editor's looks (third round, decided 2026-11-14 on
  demos/hud-hide-v2.html after three in-game looks; the earlier rounds hid
  too much or said more for a hidden element than for a situational one):
  - Shown: contents and a frame (yellow for the game's own elements).
  - Shows by situation: contents at half opacity and a white dashed frame.
    This replaces L-321's frame-only look, so every placed element can be
    seen.
  - Hidden: no contents, a dark veil (like a disabled settings row), gray
    diagonal dots and a quieter frame, and its name on a darker gray tag;
    contents at 30 % only while selected. Never yellow,
    which marks the game's own elements. It can still be moved and scaled.
  - Pointed at or selected, an element is drawn in full (a hidden one faint)
    and named. Otherwise only a hidden element carries its name: where the
    contents show, they say what the element is (decided 2026-11-14, after
    situational elements' names repeated their contents' first line).
- Names carry no state ("(hidden)" was dropped: it changed the tag's width).
  Tags hang from the element's anchored side (the right edge for an element
  anchored on the right), keep clear of other tags and of elements drawn in
  full, and with no free place outside go inside the element's top or bottom
  edge, then stack further out above or below, never onto another tag. A tag
  near the screen edge is moved onto the screen rather than sent to the other
  corner. No other name lies on the selected element, and the pointer over
  the selected element counts as pointing at it, not at what lies below.
- The switch is the settings screen's switch, joined to the selected
  element's name tag: the tag is placed with the switch's room, the name keeps
  the end at the element's corner and the switch takes the end toward the
  element's middle, only while selected. The toolbar keeps clear of the tag
  and the switch. A game part offers it only while Placed or
  Hidden. Toolbars do not change (a toolbar switch was ruled out: Lamium
  elements' toolbars are already long).
- The frame's color says who draws the element, its line the state
  (decided 2026-11-14 after a situational scoreboard lost its yellow): white
  for Lamium's, yellow for the game's own, solid when shown, dashed when it
  shows by situation, and over the gray hatching when hidden.
- The idle panel (nothing selected) has a legend: a dashed swatch "Shown by
  situation", a hatched swatch "Hidden", and below them a yellow swatch "The
  game's HUD".
- A game part left Standard (or every part with Game HUD placement off) is
  not in the editor at all: it is the game's own and moving it would change
  nothing (decided 2026-11-14, by taking away rather than marking it). Game
  HUD placement's layout link opens the editor at its first part in the
  editor, or with nothing selected.
- Cost: hidden and situational elements are drawn in the same pass as the
  others with an opacity per element; the dots are bounded per element.
- Hide boss bars moved from Hide effects to the boss bars' row. A settings
  file from before keeps what it showed: bars hidden only where Hide effects
  and its boss choice were both on, with the group on for the boss bars alone
  if it was off.

Chat placement (L-174, decided 2026-10-14): a saved, default-off Chat
position switch moves ordinary HUD messages to the lower left and adds a
position-only element to HUD layout. The default is flush left with its
bottom 44 GUI units above the screen edge, leaving a 4-unit gap above two
default-size Durability HUD rows (chosen 2026-10-14). Saved
placements are kept; this is a fixed preset, not automatic avoidance.
Native text, background, font size, shadow, fading and input/history screen
remain vanilla. See
[CHAT-POSITION.md](CHAT-POSITION.md) for the first step's boundary.
