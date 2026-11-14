# HUD

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/HUD.md`.

Everything Lamium shows during play is a **HUD element**, placed and styled
the same way in the HUD layout editor. Menus are polished; the HUD is
minimal; debug views may be dense. Mockups:
[demos/hud-editor.html](demos/hud-editor.html),
[demos/hud-layout-v2.html](demos/hud-layout-v2.html),
[demos/hud-hide-v2.html](demos/hud-hide-v2.html); where a mockup and this
text differ, this text wins.

Elements: **Info lines**, **Target**, **Status**, **Toast**, **Zoom
magnification**, **Durability**, **Minimap**, **Schematic HUD**, **Player
list**, **Inventory**, **Used slots** and **Status effects** are Lamium's;
**Chat**, **Paper doll**, **Scoreboard** and **Boss bars** are the game's own
parts, in the editor only while Game HUD placement places them. Feature
documents describe the contents of the latter elements. The **Debug** panel
is fixed to the screen edges and is not an element.

## Placement (every element)

- Stored as an anchor (9 presets) plus an offset in GUI units, but the user
  never picks an anchor. Where an element is dropped decides it: on each
  axis the nearer screen edge gives the anchor, so the element grows away
  from that edge (info lines placed bottom-left grow upward); it is centered
  on an axis only when its gaps to both edges are within a sixth of the
  screen of each other. There is no "pin". Why: anchor + pin + offset was the
  implementation showing through; people only want to put things somewhere.
- Elements can sit flush against the screen edge. Dragging snaps at the
  edge, at a 4-unit inset from it and to the center lines, never to other
  elements. Defaults and the 3x3 "snap to" buttons use the inset. Why: flush
  must be possible, but a small gap reads better and aligns layouts easily.
- Look: scale 50-200 %, background none / card (per line where offered),
  text shadow. Text color is not a per-element option. The game's own
  elements take position and scale only.
- Card look: Bedrock popup style, a dark translucent fill with corners
  stepped by one GUI pixel, no border, no accent stripe. One "Background
  opacity" setting (default 60 %) sets every HUD card and line band.

## HUD layout editor

- Entry: the pinned "HUD layout" item in the settings sidebar, an unbound
  "Open HUD layout" action under General, and a "Placement and look" link row
  on each HUD feature that opens the editor with that element selected. The
  settings list has no placement or look rows of its own.
- The editor shows the live HUD (sample content for empty elements). The
  HUD is hidden while the settings screen is open; the editor shows it on
  purpose.
- Click selects, drag moves, arrows nudge (Shift: 10), Esc deselects then
  leaves. A click on the selected element's place selects the element below
  it; the selected element can be dragged even under another. A dashed guide
  joins the anchor point and the selected element.
- The selected (or dragged) element is drawn last, on top, with a darker
  ground where it lies on another element; in play the draw order is
  unchanged. Overlap matters only in the editor, so there are no overlap
  warnings, no automatic avoidance and, for now, no element list.
- Selecting an element opens a small toolbar next to it (below, or above
  when there is no room): snap-to 3x3, scale, background, shadow (a "name:
  value" button like background), for info lines a "Lines" popover (switch +
  up/down per line), then a divider and Reset. The toolbar stays put while
  only the look changes and re-anchors after a move or when the element grows
  over it. Why: a side panel that follows the selection jumps across the
  screen.
- Scale: - and + step 5 %, a click on the value types any whole percent, and
  square handles drag in 5 % steps. The anchor point stays put, so handles
  sit only on the corners away from it: one for a corner anchor, two for an
  edge, four for the center.
- Popovers (snap-to, lines) never cover their toolbar: they open on the side
  that fits and scroll when there is not enough room.
- Resets (decided 2026-11-11): the selected element's Reset applies at once,
  like a binding row's Reset (the Info HUD's also restores its line order);
  "Reset layout" (every element and the info-line order) asks for a second
  press. "Reset layout" and "Done (Esc)" sit in a small titled panel (title,
  one hint line, a legend) above the crosshair, shown only while nothing is
  selected.

### Showing, hiding and names (L-334, decided 2026-11-14)

- Every element with a switch can be shown or hidden from the editor. For
  Lamium's elements it is the feature's own switch (Info HUD, Target info,
  Status, toggle toasts, magnification, Durability, minimap, Schematic HUD,
  inventory, used slots, status effects); the player list has none (a held
  key). A game part offers it only while Placed or Hidden.
- The switch is the settings screen's switch, joined to the selected
  element's name tag: the name keeps the end at the element's corner and the
  switch takes the end toward the element's middle, only while selected. The
  toolbar keeps clear of the tag and switch. No toolbar switch (toolbars are
  already long).
- Looks:
  - Shown: contents and a frame.
  - Shows by situation (target, automation status, toast, magnification,
    schematic, player list, scoreboard, boss bars): contents at half opacity
    and a dashed frame.
  - Hidden: no contents, a dark veil (like a disabled settings row), gray
    diagonal dots, a quieter frame and its name on a darker gray tag;
    contents at 30 % only while selected. It can still be moved and scaled.
  - Pointed at or selected, an element is drawn in full (a hidden one faint)
    and named. Otherwise only a hidden element carries its name: where the
    contents show, they say what the element is.
- The frame's color says who draws the element, its line the state: white
  for Lamium's, yellow for the game's own; solid when shown, dashed when it
  shows by situation, over the gray hatching when hidden. A hidden element
  is otherwise never drawn in yellow.
- The idle panel's legend: a dashed swatch "Shown by situation", a hatched
  swatch "Hidden", and below them a yellow swatch "The game's HUD".
- Names carry no state (no "(hidden)": it changed the tag's width). Tags hang
  from the element's anchored side, keep clear of other tags and of elements
  drawn in full, and with no free place outside go inside the element's top
  or bottom edge, then stack further out above or below, never onto another
  tag. A tag near the screen edge is moved onto the screen rather than sent
  to the other corner. No other name lies on the selected element, and the
  pointer over the selected element counts as pointing at it. The toolbar
  opens below a tag that hangs under its element.
- Cost: hidden and situational elements are drawn in the same pass as the
  others with an opacity per element; the dots are bounded per element.

## Contents

- **Info lines**: providers with an id, a label and a value; unavailable
  values say so. Each line has a switch and a place in a user-ordered list.
  Defaults on: exact position, facing, biome, FPS. Names follow the game's
  own position line (L-223): "Position" / 位置 is the whole-number block
  position, as Show Coordinates prints it; "Exact position" / 正確な位置 has
  decimals; the target card's row is "Position" too; Debug View keeps "XYZ"
  and "Block". Default order, also the settings rows' order (L-219):
  position (exact position, position, target block, scaled coordinates,
  chunk, dimension, biome), view (facing, rotation, yaw, pitch), movement
  (speed, horizontal, vertical, sprinting), surroundings (light, weather,
  time, moon, difficulty), then FPS, frame interval, ping and real time. A
  saved order is kept; a line added later goes after its default neighbor.
  The HUD order changes only in the editor's Lines list.
- **Target** (look-at card): icon, name, identifier line, then rows chosen
  by independent settings rows: icon, ID, health (hearts / bar / number),
  armor (icons / bar / number), growth (bar / number), other details. The
  element's background decides whether it has a card; there is no separate
  style. When the target changes, the card background eases to its new size
  and place in 0.1 s; the content is always drawn at once.
  - Mobs use their spawn egg as icon. Armor uses the vanilla armor-bar
    sprites; the bar mode uses the `armor` palette color. Bedrock exposes no
    armor toughness, so points only.
  - Hearts (L-88, decided 2026-10-02): one full heart is 2 HP and the number
    of heart slots follows the target's maximum health; current health fills
    those slots, half hearts included. Ten per line, top line first, up to
    five lines (100 HP); a target with more shows the bar instead, with the
    exact number beside it as always.
  - One "Range" (2-64 blocks, default 6) for every viewpoint: along the
    body's view normally, along the rendered camera's forward during
    Freelook and FreeCamera (nearest block or entity box, water and lava
    excluded). "Full range" (`targetFullRange`, off; L-308) reaches to the
    render distance instead. Why: one mental model; the game's own reach was not a usable
    distance, and 6 is close to survival reach.
  - Schematics (decided 2026-10-15): one card with the ordinary layout, no
    second section or column. "Show schematic information" (on by default);
    off, the card neither picks ghosts nor shows differences. A ghost hit
    before a real block is the target: its icon, name (in the ghost's light
    blue), ID, coordinates and states, plus "Schematic: Not placed yet"; the
    placement name is not shown. A real block that differs from the selected
    placement at that cell gets rows in the check's colors: "Schematic: Wrong
    block / Extra block / Wrong state", then "Should be: <icon> <block>"
    (red; air for an extra block), or one row per differing state, current ->
    expected with the drawn arrow (yellow), in place of that state's ordinary
    row. Schematic rows come first, right under the header.
- **Status**: automation (periodic attack/use, permanent sneak) and
  breaking/placement restriction lines in one element, each with a colored
  marker (accent for automation, warning color for restrictions).
- **Toast**: when a hotkey switches a feature, the feature name with its
  toggle switch for 1.5 s, dimming over the last 0.3 s; default above the
  hotbar (bottom center). One at a time; a new one replaces the old. The
  toggles of one key press (keys bound alike) share one toast: a row and a
  switch each, four rows at most, then a count (L-319). A child switch is
  named "Feature > Child" (ASCII sign: others change the font in English and
  sit low in Japanese) with the settings screen's child label; no toast-only
  names, no `[switch]` marker text. Card background by default, fading with
  the toast. Fires only after the new state is saved. Changes made inside
  the settings screen do not toast. The toggle toasts' switch hides on/off
  notices only; a message saying what an action did (waypoint added,
  schematic saved, restore ended) and a warning show either way.
- **Line height and per-line backgrounds** (L-98): the Info HUD, Status and
  Debug View share one "HUD line height", 9-16 GUI units (default 12). Info,
  Status and the player list add "Per line" to their Background choice; the
  Debug View has "Background: None / Per line". Each line gets a band of its
  text width plus 1 unit each side, rows touch, text centered in the band; on
  an element anchored to the right the bands and text line up on the right
  edge. Defaults: Info HUD per line, Status on a card, Debug View per line.
- **Debug** (L-54): a two-column panel modeled on Java's F3 screen, off by
  default, toggled by F3. Not a HUD element: the left column hangs from the
  top-left inset and the right column from the top-right (moved down only as
  far as a long left line beside it needs), both plain with shadow; the
  editor neither shows nor edits it. Left: versions (Minecraft, LeviLamina,
  Lamium), fps/frame/ping, position, block and chunk, facing with yaw/pitch,
  light, biome with difficulty, day/clock/weather/moon, then the look-at
  target. Right: client settings (dimension, render distance, ray tracing
  and Vibrant Visuals, clouds and skies, fullscreen, max frame rate) and
  local PC information (memory, CPU, GPU, display, OS); a line the machine
  cannot provide disappears. It draws independently of Info lines and
  Target. "Row name style" switches line names between the game language
  (default) and Java F3 abbreviations; FPS and Ping read the same in both.
  Two child options (on by default) hide the Info HUD and the Target card
  while Debug is open; a third turns the panel's text shadow off.

## Game HUD placement (L-321, L-309, L-323, L-334)

What HUD layout places of the game's own HUD is one feature with a saved,
default-off main switch and a row per part (chat, paper doll, scoreboard,
boss bars). Each row is Standard (the game's own), Placed (moved and scaled
by the layout) or Hidden (not drawn), default Placed, saved as the part
switch and a show value. Each part has one unbound key that steps Standard,
Placed, Hidden like Tool Switch's drop priority; with the main switch off it
changes the choice only and says so. The boss bars' key is the old Hide boss
bars key. The main switch off leaves every part the game's and keeps the
choices. The hotbar is not part of it.

- Hiding skips the part's draw; the game's layout and state stay. The chat
  hides only the HUD messages; the chat screen stays vanilla.
- A part left Standard (or every part with the feature off) is not in the
  editor: moving it would change nothing. The feature's "Placement and look"
  link opens the editor at its first part in the editor, or with nothing
  selected.
- The game's look and when a part shows stay vanilla; its elements have no
  look controls.
- **Chat**: default lower left, flush left, bottom 44 units up (decided
  2026-11-11), so switching the feature on moves the chat at once while the
  others stay until dragged. Why not the game's place: the game has no fixed
  chat place (it is the last item of a stack whose height changes with what
  shows), a fixed default would imitate one state and sit on the default
  Info HUD, and the lower left keeps clear of Lamium's HUD and matches Java
  Edition. Details: [CHAT-POSITION.md](CHAT-POSITION.md).
- **Paper doll**: default the game's desktop place and size, so switching it
  on changes nothing until dragged or scaled.
- **Scoreboard**: default the game's place, the middle of the right edge, at
  the game's size. It shows only where a world or server displays an
  objective, so the editor treats it as situational.
- **Boss bars**: default the game's desktop place, top center 2 units down,
  at the game's size. The element is the box the shown bars fill (182 x 20
  per bar, as the game lays them out), so the anchor the drop chose stays
  put as bars come and go: at the top they grow downward, dropped at the
  bottom upward (chosen by the implementer from these rules at the
  maintainer's request). Hidden boss bars stay hidden wherever placed.
  Situational, with sample bars from the game's bar textures.
- Settings from before L-334 keep what they showed: boss bars hidden only
  where Hide effects and its boss choice were both on, with the group on for
  the boss bars alone if it was off.

## Related rules

- **Animations** (General): one Lamium-wide setting, "Follow Minecraft"
  (Video > Screen Animations, default), On or Off, for every Lamium UI motion
  (screens, HUD). World drawings follow their own feature's look (Block
  highlight: a still color stops the rainbow; decided 2026-11-09).
- **General** has three headings: "Settings screen" (its key, animations,
  the Hotkeys and HUD layout openers), "Toggle toasts" (the switch, then the
  toast's layout link) and "HUD text" (line height, background opacity). The
  last two have no key of their own, so their switch, if any, is the first
  row.
- **Esc returns to where a page was opened from** (decided 2026-11-14): a
  tool page opened by its key returns to the game, one opened from the anvil
  to the anvil, one opened from the settings to the settings table. This
  holds for the calculator, the world map, HUD layout, the block highlight
  list and Enchantment order; the schematic menu only opens over the game. A
  page's Back button always goes to the settings table.
- Dedicated actions that open Hotkeys, Shapes or HUD layout do not replace
  the Settings action's remembered page by being opened; manual sidebar
  navigation from a dedicated view does.
- There is no gameplay key-hint overlay; "Open Hotkeys" replaces it.
- Bounded numeric settings where "about this much" is enough (ranges, zoom)
  use a slider styled like the Bedrock switch: green filled track, a light
  square knob with a bottom bevel, the value to the right. Click or drag the
  track; Left/Right step; clicking the value types a number. Settings that
  need precision (periodic intervals, shape coordinates) keep the arrows and
  number entry.

## Code

- HUD elements draw only in the gameplay HUD view, so a translucent card is
  not composited again by other screens; the settings screen hides the HUD,
  the HUD layout editor shows it.

- `src/ui/HudElement.h` (pure): anchor/offset model, drop-to-anchor, snap,
  scale steps (`clampHudScale`), default placements. `HudEditorLayout.h`
  (pure): editor geometry. `HudEditor.cpp/.h`: the editor screen.
  `HudLines.h` (pure): line height and per-line bands.
  `Toast.h`: toast timing and grouping.
- `src/features/information/`: `InfoLines.h` / `InfoHud.cpp` (Info lines),
  `TargetCard.h`, `TargetRows.h`, `TargetPick.h`, `TargetInfo.cpp`,
  `SchematicTarget.h` (Target), `DebugLines.h` / `DebugVanillaHud.cpp`
  (Debug), `ChatPosition.cpp`, `ScoreboardPosition.cpp`,
  `BossBarPosition.cpp` (Game HUD placement).
- Settings: `Settings.h` `information` and `ui` groups (`targetDistance`,
  `hudBackgroundOpacity`, the Game HUD part switches).

## Tried

- Anchoring to the center when an element lay in the middle third of the
  screen: a tall Info HUD near the top anchored to the middle and moved when
  lines were added or the line height changed.
- Snapping to other elements (mockup): caught too often.
- A name inside a frame's top left: ran into other elements' text. A name on
  a dark plate in the middle: looked out of place, and overlapping frames put
  their plates on each other.
- One corner button whose meaning changed with the selection, for resets:
  every corner holds an element by default.
- Hiding the target card's content while the card eased: blanked the card
  while the view moved.
- Stacked or side-by-side schematic sections in the target card: hard to read.
- Earlier hidden-element looks hid too much or said more for a hidden
  element than for a situational one.

## Limits

- Coverage: [VALIDATION.md "Information and overlays"](VALIDATION.md#information-and-overlays).
- Open work:
  [L-336 Durability HUD: choose its items in HUD layout](backlog/L-336.md),
  [L-184 Inspect ghost block data and schematic container contents](backlog/L-184.md),
  [L-341 Shared widgets for repeated controls](backlog/L-341.md).
