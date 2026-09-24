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

## HUD (Decided — see [demos/hud.html](demos/hud.html))

Menus are polished; HUD is minimal; debug views may be dense. Everything the
HUD shows is a **HUD element**, placed and styled the same way. Where the
demo and this text differ, this text wins.

Elements: **Info lines**, **Target**, **Status**, **Toast** (later: F3 view).

Placement and look (every element):
- Anchor (9 presets: corners, edge centers, center) plus an offset in GUI
  units; scale 75–150 %; background none / translucent card; text shadow.
- The anchor is the point that stays put when the element grows or the screen
  resizes (a top-anchored list grows downward). By default a drag picks the
  nearest anchor. Choosing an anchor in the element's panel pins it; drags
  then change only the offset, so an element can hang low from a top anchor.
  Why: automatic anchors alone cannot express "top-anchored but placed low".
- A dashed line from the anchor to the element shows the relation while
  dragging. Small offsets snap to zero.

Editing:
- Settings → General has "Edit HUD layout", which shows the live HUD with
  draggable elements, anchor dots and a per-element panel. Each element also
  has "Placement" and "Look" rows in its settings, so everything is reachable
  without dragging.
- The per-element panel and the settings rows are generated from the same
  option definitions and are never written twice; long lists scroll inside
  the panel. Why: hand-written copies drift when options are added.

Contents:
- **Info lines**: providers with an id, a label and a value; unavailable values
  say so. Each line has a switch and a position in a user-ordered list.
  Defaults on: coordinates, facing, biome, FPS. Everything else starts off.
- **Target** (Jade/WAILA role): "Card" (default) shows an item/block icon,
  name, identifier line, then rows (states, growth, power, health) with
  progress bars where a value has a range. "Simple" keeps today's text-only
  view.
- **Status**: automation (periodic attack/use, permanent sneak) and breaking/
  placement restriction lines in one element, each with a colored marker
  (accent for automation, warning color for restrictions).
- **Toast**: when a hotkey switches a feature, show the feature name with its
  toggle switch for ~1.5 s, dimming over the last 0.3 s; default position
  above the hotbar (bottom center). One at a time; a new one replaces the
  old. No `[switch]` marker text. No background panel: HUD-pass fills render
  opaque, so the toast borrows the text-only style of the other HUD lines
  (verified 2026-09-24). Fires only after the new state is saved. Setting to
  turn it off. Changes made inside the settings screen do not toast.
- The gameplay key-hint overlay is removed; an "Open Hotkeys" action replaces
  it.
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
