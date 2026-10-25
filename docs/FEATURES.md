# Lamium features

The user guide to what each feature does. The
[project README](../README.md#features) has the short list with default keys
and experimental marks; known limits are in its
[Known issues](../README.md#known-issues). Sections follow the sidebar of
Lamium Settings.

## Settings screen

Press `L` in a world or on the title screen, or click "Lamium <version>" in
the title screen's bottom right corner, to open Lamium Settings. Views made of
one world's data, such as waypoints, open only in a world. `L` closes it
again.

- A sidebar/table layout fitted to Bedrock, with search.
- English, Japanese, Simplified and Traditional Chinese text, following the game language.
- Changes are saved at once; there is no Save/Cancel step.
- Hotkeys, Shapes and HUD layout are views of the same screen.
- On a small window the whole screen shrinks to fit; long descriptions scroll
  in the footer.
- Every feature has one switch, which its key also toggles. "All", each
  category and Hotkeys can reset their settings to the defaults.
- Features marked Exp. (experimental) are still settling.

### Keys

Lamium owns its key bindings; they do not appear in Minecraft's keyboard
settings. Bindings are edited under each feature or in the Hotkeys view.

- Clear unbinds an action and Reset restores its default.
- The Settings action cannot be cleared, so the UI cannot be locked out.
- New actions start unbound.
- Default keys: `L` settings, `M` world map, `Tab` player list (hold), `C`
  zoom (hold), `R` sort (in a container), `F` offhand swap, `F3` Debug View,
  `F3+B` Hitboxes, `F3+G` Chunk Borders and right click for Fake Offhand's
  use, and `Alt+Wheel` to move the schematic target away/closer.
- `C` replaces Minecraft's "copy coordinates" while Lamium is installed
  unless you rebind it.

## Camera & view

### Zoom

Zoom up to 50x with a smooth wheel. Optional: a magnification readout, and a
wide-angle view where the wheel goes below 1x. Hold or toggle.

### Freelook

Turn only the view while the body keeps its direction and movement. Rear
third person by default; your previous view comes back afterwards. Hold or
toggle.

### FreeCamera

Fly the view freely while the player stays in place. Hold or toggle.

- Toggle keeps its position through menus; death, dimension changes and
  leaving the world turn it off.
- Adjustable speed with a sprint boost.
- A player- or world-fixed position.
- The camera's own position and facing in the Info HUD and Debug View.
- Waypoints are added where the camera is.
- Optionally leaves when your body is hit.
- The hand, the hotbar or the whole HUD can be hidden while it flies.

### Night Vision

Brightens the view locally, without a status effect. Optionally lights
everything evenly, with no dark rings by light sources.

### Hide Offhand Item

Hides the first-person offhand drawing, shields included. Equipment and use
are kept.

### Hide effects (experimental)

Hides the selected visual effects; one main switch turns the selection on.
Sound and gameplay stay unchanged.

- Rain and snow, particles and boss bars.
- The nausea color and the Nether portal's purple screen.
- Underwater, lava and powder snow fog.
- The distance fog on land, in the Nether and in the End.

### Connected Textures (experimental)

Glass, glass panes, bookshelves and sandstone next to the same block draw as
one surface, without seams. Glass, bookshelves and sandstone keep the
texture's own scale. Stained glass joins only the same color.

Two child switches are off by default. "Edges at inside corners" keeps the
border where glass block walls turn a corner, on the inside of the turn;
"Edges at glass pane corners" adds an edge on the outside of an L-shaped
pane. The flat-notch corner texel is kept whenever Connected Textures is on.

## HUD & overlays

### Info HUD

Ordered information lines, among them coordinates (also scaled), biome
names and ids, difficulty, yaw/pitch, sprinting, horizontal and vertical
speed, and real time. Their order and position are set in the HUD layout.

### Target Info

A card for the block or mob you aim at: icon, name, hearts in real health
units (one heart per 2 HP), armor and bars such as growth.

### Debug View

A panel like Java Edition's F3 screen: game, world, look-at and PC details,
entity counts by kind and the most common entity types. The game's own
position line and paper doll are hidden while it is open, and where the Info
HUD covers them. What the client cannot know (slime chunks, server TPS, mob
caps, local difficulty) is not shown.

### Durability HUD

Durability of the held item, and optionally the offhand and armor. Arrows
and fireworks are counted beside bow, crossbow and elytra durability.

### Offhand slot

The offhand item in a slot beside the hotbar, with its count and durability
bar.

### Saturation

Gold outlines on the hunger bar as far as your saturation reaches. Holding
food previews what eating it would add.

### Player list

While `Tab` is held: the players with their faces, platform, dimension,
distance and permission marks.

### Inventory HUD and Used slots

Your inventory as a 9x3 grid during play, and a counter of used slots such
as 13/36. Each works without the other.

### Status effects HUD

Active effects with icons, numeric levels and remaining time. The game's own
effect icons are hidden while it shows.

### Chat position

Off by default. Chat messages at the bottom left as on Java Edition, movable
in the HUD layout editor. Text, backgrounds and fading stay vanilla.

### HUD layout and toggle toasts

A live HUD layout editor places Lamium's HUD elements and shows where the
game's hotbar is. Toggle toasts give a short notice when a key turns a
feature on or off.

### Chunk Borders, Hitboxes and Light Level Overlay

- Chunk Borders: Java-style chunk edges and 16-block layers (`F3+G`).
- Hitboxes: nearby entity bounds with eye and look markers (`F3+B`).
- Light Level Overlay: light level on nearby floors. Red: monsters can spawn
  at any time; yellow: only at night.

### Shapes

Boxes, cones, pyramids, ellipsoids, domes and more drawn in the world, saved
per world. Create and edit them from Shapes in the sidebar.

## Map (experimental)

The minimap and the world map are off by default. Waypoints and the death
point also show in the world while both maps are off.

### Minimap

A HUD element colored from your block textures.

- Size, range, north or heading up, round or square.
- Compass, coordinates and biome lines.
- Hold a key to enlarge it.
- Cave view: under a roof and in the Nether it shows the floors around your
  height.

### Radar

Nearby players, mobs and dropped items as dots on the minimap, or optionally
as mob faces and player skin heads.

### Waypoints

Waypoints with the last death point, shown on the minimap and in the world,
edited in a Waypoints screen.

### World map

A full-screen map (`M`) of the areas you have visited, recorded per world.

- Zooms out until everything explored fits on the screen.
- A side panel lists waypoints, placed schematics and shapes, with a link
  that opens the same place in ChunkBase's seed map.
- Schematic placements and shapes show on the map; a click selects one, and
  its menu shows or hides it or opens it in its own screen.
- Switches in the map's top bar hide waypoints, schematics or shapes on the
  map. Shapes can also show on the minimap (an option, off by default).
- Names show when the pointer is over a mark; an option shows them all.

## Schematics (experimental, off by default)

Place `.mcstructure` files from `mods/Lamium/schematics/` as ghost blocks and
build them.

- **Place:** move, turn and mirror placements; moves follow the way you
  look. A ghost tint strength and outline width of your choice.
- **Layers:** show one layer of a placement, or everything up to it from
  either side, cut along the schematic's own X, Y or Z, so turning or
  mirroring the placement keeps the cut on the same blocks. Moving the cut
  while showing "All" goes back to the view you last adjusted.
- **Check:** see what is missing, wrong or in the wrong state in the world,
  in a Check list and a HUD. The target card names the ghost you aim at (not
  placed yet) with the expected block and what to change; a real block that
  differs says what is wrong first.
- **Materials:** count the materials left against what you carry, Shulker
  Boxes included, in chests and stacks.
- **Save:** save an area of the world as a schematic, also larger than the
  render distance, by walking along it. Saved files use the game's current
  format and load in a structure block.
- **Controls:** a radial schematic menu (Move, Placement, Layers, Show and
  check, Area, Screen) holds every schematic operation; use the menu, keys or
  both. Every operation is reachable from either path (the "Move … →"
  entries use the target-cycle key). The key settings use the menu's groups,
  and the menu shows the keys you bound. Only Alt + wheel (move the target
  away or closer) is bound by default. Another key repeats the last menu
  adjustment with the wheel; "Repeat every menu step" (off by default) also
  covers the layer axis, mode and keep side. The menu opens small in the
  lower right by default.
- **Missing files:** a placement whose file is missing or cannot be read is
  reported once per world with a yellow notice, marked in the Placed list
  and named in the HUD. Its layer fields say the view is unavailable until
  the file loads. A file another program held open loads once it is released.
- **Drawing:** ghosts draw doors, beds, honey, water and lava, block entities
  with their saved data and entities as their models. The Files and Check
  tabs have a turnable 3D preview.
- **Map:** placements show on the minimap and world map, and can be selected,
  shown or hidden there.

## Inventory

### Container previews, Durability numbers and Food values

Inside the game's item tooltip:

- Shulker Box and Bundle contents.
- Remaining durability.
- What food restores: hunger and saturation as hunger-bar icons.

### Trades

- All trader levels: the trade screen lists every level. Locked trades are
  shown dimmed, with their items' names on hover.
- Repeated trades: Shift + right click on a trade row repeats it until it
  sells out, the inputs run out or the output no longer fits.

### Search English names too

The recipe book and creative search find English names and IDs in any game
language.

### Inventory sorting

Merges and sorts inventory or container slots (`R` in a container). Sorting
uses ordinary game operations, waits for matching responses between
operations and revalidates the affected region before continuing. Rejection,
missing responses or a changed screen stops the plan.

### Inventory transfer (experimental, off by default)

Moves items between your inventory and every item screen: chests, furnaces,
brewing stands, hoppers, beacons and more, also the inventory screen.
Furnace, brewing and beacon slots get only what they accept.

| Gesture | Moves |
| --- | --- |
| Wheel, Shift + wheel, drags | One item or a stack |
| Ctrl + wheel, Alt + click | Every item of a kind |
| Ctrl + Shift + wheel, Alt + Shift + click | Everything (the hotbar only when you start over it) |
| Shift + right drag | Leaves one in each slot |

With a stack on the cursor (only these two; every other gesture needs an
empty cursor):

| Gesture | Does |
| --- | --- |
| Shift + click on an empty slot | Puts the stack there and gathers the matching items from the other side |
| Shift + click outside the window | Drops the stack and the matching items in your inventory |

With an empty cursor and the drop key held, drags drop instead of moving:

| Gesture | Drops from each slot you pass |
| --- | --- |
| Ctrl + left drag | One item |
| Shift + right drag | All but one |
| Shift + left drag | The whole stack |

### Tool Switch and Weapon Switch

Picks the hotbar tool when mining, or the weapon that deals the most damage
to what you hit. Optionally fetches one from the inventory, into the
selected slot or a fixed one.

### Offhand swap

Swaps the held item with the offhand (`F`, as in Java Edition).

### Fake Offhand (experimental)

Uses a hotbar block or item as if it were in the offhand, also when the item
in your hand has nothing to do where you aim.

### Hand Restock

Tops up consumed items in the same hand slot from the inventory or hotbar,
with a threshold and a source order. Swaps container remainders (bowls,
bottles, empty buckets) and refills an offhand totem after it saves you.

### Death layout restore (experimental)

Picking up your dropped items puts the hotbar, armor and offhand, optionally
the whole inventory, back the way they were when you died.

## Actions

### Permanent Sneak and Permanent Sprint

Toggle sneaking or sprinting with a key.

### Edge Guard

Stops at block edges without sneaking, with an adjustable margin. Also works
on servers. In rare cases, such as heavy lag or a very small margin, you can
still drop.

### Tool Protection

On by default. At 1 durability a tool or worn elytra is swapped for a spare,
or mining stops.

### Auto Elytra

A key, a firework jump or a Riptide trident launch puts an elytra on; the
chestplate returns after landing.

### Block Restrictions

Breaking modes anchored where you start breaking, including a height band.

### Auto Attack and Auto Use

Clicks for you in Periodic, Hold or Fast click mode. Your own clicks take
priority.

## General

### Calculator

A calculator over the game, with units for stacks, chests and shulker boxes,
history and copy buttons. Nothing is sent to chat.

Text fields in Lamium have a caret, paste and click placement.

### Game language (experimental)

Changes Minecraft's display language without leaving the world.

### Skip server resource packs (experimental)

A server's packs are accepted as it asks but not shown: the world looks
vanilla. Local worlds keep their packs.

### Game version check

On a game version Lamium has not been checked on, the parts that depend on
it (for example Night Vision's light, Edge Guard, Connected Textures, ghost
blocks) stay vanilla. The settings screen, toggle toasts and a toast on
joining a world say which features are paused.

## Diagnostics

Lamium writes diagnostics to `mods/Lamium/logs/lamium.log`, flushing
informational messages immediately so failures can be inspected while the
game is running. Logs rotate by size and date and keep at most seven
archives.
