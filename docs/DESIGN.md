# Lamium design direction

This is the source of truth for how Lamium looks and behaves. Sections marked
**Decided** were agreed with the maintainer and verified in game. Sections
marked **Proposed** are the current direction but still need the
maintainer's confirmation before an agent builds on them. Do not change a
Decided rule without asking.

## Product principles (Decided)

- Client-side only. Never require a server mod. Never invent information the
  client does not have; show "unavailable" instead.
- One mod, many tools, still easy to handle: every feature is found, understood,
  switched and bound from one settings screen. Tools with their own state
  (Shapes, later Schematics) get a dedicated view built from the same parts.
- Settings apply and save immediately. Escape/Close only closes. Only a failed
  save is reported.
- A feature's toggle, key and options live together. There is no "Advanced"
  bucket; options have names that say what they do.
- Press/Hold/Toggle is decided by the action, not chosen by the user. Only
  Freelook exposes "Activation: Hold / Toggle" because both are useful.
- Uncertain, high-effort features (Mass Craft, profilers, Placement Assist) are
  experimental tracks and never block the roadmap.
- Features restore vanilla behavior when disabled or when leaving a world.
- Do not copy other mods' code, strings, assets or pixel-level UI.

## Visual language (Decided)

"Fits Bedrock without cloning it; PC utility density; restrained modern touch."

- Dark translucent panels over the live world. Flat fills and 1-unit frames.
  No gradients, glow, blur or decorative motion.
- Only rectangles and text are drawn (`src/ui/Widgets.h`). The game font is
  used as is; no custom fonts.
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

### Components (Decided)

- **Switch** for every on/off (Bedrock style, knob position carries state).
  No "ON/OFF" words and no check boxes.
- **Key caps** for bindings. The key column never shows Hold/Toggle/Press.
- **Steppers**: `− value +` for numbers, `◀ value ▶` for choices.
- **Child settings** are indented with a vertical guide line. Expanding or
  collapsing keeps the clicked row where it was. The wheel scrolls without
  moving the selection.
- **Sidebar** with categories; Hotkeys and Shapes are pinned at the bottom.
  Below 440 units wide the sidebar becomes tabs; below 380 the key column
  narrows. Search spans all categories.
- **Experimental** features carry a small purple badge.

### Text and languages (Decided)

- English is the default; Japanese follows the game language. Every string
  has both. Chinese only on request.
- Japanese locale: Latin runs are raised 1.5 units to share the baseline, and
  text inside a frame starts `boxTextInset()` lower. Always draw through
  `ui::label`/`ui::paragraph`, never raw font calls.
- Labels with a value use one translation string with `{}`
  ("Radius: {}"), split by `splitLabel` for table display.

## HUD (Decided — see [demos/hud-editor.html](demos/hud-editor.html))

Menus are polished; HUD is minimal; debug views may be dense. Everything the
HUD shows is a **HUD element**, placed and styled the same way. Where the
demo and this text differ, this text wins. The first demo
([demos/hud.html](demos/hud.html)) is superseded where they differ; the
rework came from using the first editor build (2026-09-24).

Elements: **Info lines**, **Target**, **Status**, **Toast** (later: F3 view).

Placement (every element):
- Stored as an anchor (9 presets) plus an offset in GUI units, but the user
  never picks an anchor directly. Where an element is dropped decides it:
  the screen third holding the element's center gives the anchor, so an
  element grows away from the nearest edge (info lines placed bottom-left
  grow upward). There is no "pin". Why: anchor + pin + offset was the
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
  element to its defaults at once; with nothing selected it resets the whole
  layout (including the info-line order) and asks for a second press.
- Popovers (snap-to, lines) never cover their toolbar: they open on the side
  that fits and scroll when there is not enough room.
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
  Defaults on: coordinates, facing, biome, FPS. Everything else starts off.
- **Target** (Jade/WAILA role): icon, name, identifier line, then rows. What
  is shown is chosen by independent rows in the settings list: icon, ID,
  health (hearts / bar / number), growth (bar / number), other details.
  There is no separate "card / simple" style; the element's background
  setting decides whether it has a card. When the target changes, the card
  background eases to its new size and position in 0.1 s; the content is
  always drawn at once (hiding it blanked the card while the view moved).
  Mobs use their spawn egg as the icon; hearts use the game's health-bar
  sprites.
  While Freelook or FreeCamera is active the card follows the camera: the
  nearest block or entity box along the rendered camera's forward, water and
  lava excluded.
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
- Bounded numeric settings where "about this much" is enough (ranges,
  zoom) use a slider styled like the Bedrock switch: green filled track, a
  light square knob with a bottom bevel, the value to the right. Click or
  drag the track; Left/Right step; clicking the value types a number.
  Settings that need precision (periodic intervals, shape coordinates)
  keep the arrows and number entry.
- Appearance options stay at scale, background and shadow for now; add text
  color or background opacity only if asked.

## World overlays (Decided unless noted)

- Block-grid shapes show the blocks that form the shape (MiniHUD approach),
  not smooth wireframes. Wireframes are for chunk borders and hitboxes.
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

## Keys

- (Decided) There is no general default-key policy to design now; defaults are
  decided per feature when needed. New actions ship unbound unless the
  maintainer picks a key.
- (Decided) Settings opens with `L` instead of F8 (BACKLOG L-01).
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
  - Both kinds tolerate unrelated held inputs (walking with W still lets J
    toggle). Ordinary chords differ by order and by yielding.
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
