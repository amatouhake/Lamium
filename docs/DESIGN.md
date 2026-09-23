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

## HUD (Proposed — needs confirmation)

Menus are polished; HUD is minimal; debug views may be dense. The HUD is now a
set of separate pieces (Info HUD, target info, automation status, restriction
status) with their own fixed or percentage positions. The proposal is to turn
them into one **HUD element** system before adding more content:

- Every element has: anchor (9 presets: corners, edge centers, center) plus
  an offset; scale (75–150 %); background (none / translucent card);
  text shadow on/off. Settings rows are generated from this shared model.
- Element types: Info lines (user-chosen, user-ordered lines), Target card,
  Status (automation, restriction, toggle toasts), later F3 view.
- Info lines are providers with an id, a label and a value; unavailable values
  say so. Line order is user-editable in a small list editor.
- **Target card** (Jade/WAILA role): item/block icon, name, mod-style source
  line ("Minecraft"), then provider rows (state, growth, power, health bars).
  Built from panel/row tokens above; needs a web demo before implementation,
  like the settings screen and Shapes view had.
- **Toggle toast**: when a hotkey switches a feature, show
  `[switch] Feature name` for ~1.5 s centered above the hotbar, fading out.
  One toast at a time; a new one replaces the old. Can be turned off.
- The gameplay key-hint overlay is removed; an "Open Hotkeys" action replaces
  it.

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
- (Proposed) Line overlays get per-batch colors so chunk borders and hitboxes
  can follow Java's F3+G / F3+B color coding (see BACKLOG L-09 to L-11).
- (Proposed) Overlays must not hide vanilla's block selection outline: skip or
  dim geometry on the targeted block.

## Keys (Open)

F8 is missing on some keyboards and says nothing about Lamium. The default
settings key and the policy for other defaults are an open decision
(BACKLOG L-01). Until decided, do not add new default keys; new actions ship
unbound.
