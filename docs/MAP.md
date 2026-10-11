# Map

Implementation notes for the minimap, radar, waypoints and world map (L-60).
Work status and open choices are in
[BACKLOG.md](BACKLOG.md#l-60-map-minimap-waypoints-and-world-map-experimental);
runtime coverage is in [VALIDATION.md](VALIDATION.md), with evidence in
[VALIDATION-LOG.md](VALIDATION-LOG.md).

## Current implementation

Experimental, default off. The minimap, cave view, radar, waypoints and world
map are built. The map uses received chunks and recorded map data; seed-based
terrain, biomes and structures remain a non-goal. FreeCamera moves the minimap
center; Freelook keeps it on the player. Mob faces and player heads are
optional; distant players use vanilla locator state (L-89).

The world map opens with `M`, supports pan/zoom, dimension and Nether-layer
selection, and a waypoint side panel. Recording is independent of showing the
minimap. L-104 adds renderer biome tint with fallback/rescan, stronger relief,
saved colors for incomplete chunks, missing-section requests and teleport
menu items gated by permissions and available commands. Teleport uses the
ordinary command; server acceptance is not assumed.

Waypoints and map regions are stored per local world or server address/port
and dimension. Unsupported connection identities remain session-only.
Waypoint server storage and the L-104 server paths still need runtime checks.

In-world waypoint markers follow vanilla Hide HUD (F1), including the death
point, names and distances (decided 2026-10-07, L-106). The marker draw checks
`IOptionRegistry::getHideHud()` each frame; restoring the HUD resumes the
configured Always / While held / Off behavior. This does not change waypoint
storage or recording. The maintainer confirmed F1 hide/restore of all marker
parts and hiding during FreeCamera on `775c8c0` (2026-10-07).
The minimap and other Lamium HUD elements also follow F1 (L-107); map
recording and death tracking continue while hidden (checked on `18e2cc8`).

Map layers (L-139, built 2026-10-11, unchecked in game): schematic
placements and shapes are marks from the features that own them
(`MapLayers.cpp`), keyed by layer and session id (`MapMarks.h`). The world
map selects and acts on them (placements: select, show/hide, open in the
Placed tab; shapes: show/hide, open in the Shapes view); the minimap draws
placements, and shapes with the minimap's Shapes option (off by default).
Overlapping marks take the cursor in a fixed order: waypoints and the death
point, placements, shapes, the smallest footprint first.

## Technical entry points

- `MapMarks.h`, `MapLayers.cpp`: mark keys, hit order and the footprint
  providers with their shared actions (L-139).
- `src/features/map/Minimap.cpp`: bounded scanning, terrain colors, cave
  tiles, radar and runtime texture composition.
- `MapStore.cpp`, `MapRegion.h`: region/image cache, disk worker and format.
- `WorldMap.cpp`, `WorldMapView.h`: screen tiles, bounded uploads and view math.
- `WaypointStore.cpp`: waypoint persistence.
- `tests/MapTests.cpp`, `tests/WorldMapTests.cpp`: pure map logic.

These source paths are under `src/features/map/` unless otherwise stated.
The detailed requirements, storage layout and build budgets are in the
record below. Current L-104 details are retained under L-104 in BACKLOG-DONE.

## Decision and implementation record (L-60)

Moved from BACKLOG without dropping the original decisions or build notes.
Earlier "not yet built", "unchecked" and "no teleport" statements describe
their dated checkpoints. Later decisions and the current L-item/validation
take precedence. Search for the step or decision needed.

Decision record: L-60 was chosen when no LeviLamina map mod with a minimap,
world map and waypoints seemed to exist (ChiyanMap was gone), and put on hold
2026-09-30 until the maintainer had used CoralMap (CC0-1.0, reference-only,
PROVENANCE.md group 3), which per its README lacks waypoints, radar, cave
view and the death point. On 2026-10-01 the maintainer decided to build a
full map in Lamium without trying CoralMap: it has too few features, and
ChiyanMap, though feature-rich, left the mouse cursor free after its world
map closed, scattered its settings and felt rough to operate. Running
several mods together (Lamium, LHolo, ChiyanMap) also often left the
Minecraft process running after exit; the cause is not identified, so
Lamium's map must not become one (see Map-wide requirements).
A client-side map built from the chunks the client has loaded: a minimap HUD
element with a radar and waypoints first, then a full-screen world map backed
by an on-disk cache. It ships default off with the Experimental badge and
grows on main in steps ([Release policy](BACKLOG.md#release-policy-decided-2026-09-28)). Look agreed in
[demos/minimap.html](demos/minimap.html).

Lamium's map is specified here and implemented independently on
LeviLamina/Bedrock APIs. ChiyanMap (GPL-3.0) and the current LeviLamina map
mods are reference-only (PROVENANCE.md group 3). The maintainer keeps
ChiyanMap recovery material outside the repository (local path in
`AGENTS.local.md`, when present); planning may use its notes, but whoever
writes Lamium map code works from this spec and does not open the recovered
source.
The implementation should use a player-centered scan spread over frames with
an explicit budget, retain owned height/color data for shading, partition
persistent data by world and dimension, and build the world map from bounded
cached regions rather than a single unbounded texture.

### Map-wide requirements (decided 2026-10-01)
These apply to every step, the world map and L-82.
- Clean shutdown: every scan, bake or disk-cache thread or task is stopped
  and joined on world exit, dimension change, disabling the feature and mod
  shutdown. Shutdown never waits on the game's threads or blocks in DLL
  unload. In-game checks for steps that add background work include quitting
  the game and confirming the process ends (`tasklist` shows no
  `Minecraft.Windows`).
- Cursor and input: a full-screen map opens as a game screen rather than by
  releasing the mouse by hand. When it closes, when the window loses focus
  and on world exit, mouse capture and look control return to normal play.
  Its in-game check: open, close, Alt+Tab away and back, then turn the view.
- One place for settings: every map option, including anything changeable
  from inside the world map, lives in the "Map" settings category and is the
  same setting wherever it is shown.
- The world map opens fast: regions already cached show at once, and new
  areas fill in progressively without blocking the screen or the game. The
  maintainer found a slow-to-generate world map a constant annoyance.
- Operation is designed before it is built: the world map gets a mockup in
  `docs/demos/` agreed with the maintainer (dragging, zoom, adding and
  editing waypoints, closing) before implementation, as the minimap did.

### Minimap spec (decided with the maintainer, 2026-09-28)
Map
- Square, north up; the player is a white arrow with a black edge, modeled
  on the vanilla map's player marker (check whether the game's own map icon
  can be drawn at runtime). Rotating (heading up) and round are settings.
- About 128 x 128 blocks by default; zoom steps from 16 to 512 blocks
  across (16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512; finer steps
  decided 2026-10-01, saved as the width in blocks). Chunks the client has not loaded stay blank: a dark,
  half-transparent fill in the card color (decided 2026-10-01; transparent
  looked wrong). Known empty ground (a drop in the cave view, the End's
  void) is an opaque dark color instead, so it reads differently from
  ground not loaded yet. Nothing is requested from a server.
- A thin 1-unit frame only; nothing is drawn outside the map except the
  compass letters, which sit on the frame (2026-10-01).
Terrain
- A representative color per block, biome tints for grass, foliage and
  water (the color the world shows, not the vanilla map item's palette:
  the first build used that palette and looked unlike the world), and height shading against the north and west neighbors, at the
  strength shown in the mockup. No day/night darkening.
- Under a ceiling the map switches to a cave view on its own: floors near
  the player's height bright, walls dark. The Nether always uses it. A key
  can force the cave or surface view.
- The cave view must stay calm (2026-10-01, after the first cave build
  redrew on every one-block height change): it is drawn around a held
  height that moves only when the player is more than 3 blocks from it,
  over a window from 5 above to 24 below that height, with depth-based
  brightness; the roof check uses most of the 3x3 columns around the
  player; automatic switches wait 1 s after the previous one. Possible
  later refinement, not decided: hold the height while the player stays in
  the same cave space rather than by distance.
Radar
- Simple dots by kind, each kind a setting: other players (light blue, with
  their name), hostile mobs (red) and passive/neutral mobs (white) on by
  default; dropped items (yellow) off. Every dot has a thick black ring.
  Dots 8 or more blocks above or below the player are drawn fainter, except
  players (decided 2026-10-03, L-89): faint means "out of range" for them.
- On by default; the help text notes that some servers may treat seeing mobs
  and players through walls as unfair.
- Later, as an option: per-mob icons (for example from the spawn egg, as the
  Target card does); dots stay the default.
Waypoints
- Add one at the current position with a key; a small prompt asks for the
  name and color. Only the last death point is recorded automatically, with
  its own cross marker; it can be turned into an ordinary waypoint. No
  teleporting.
- On the minimap: diamonds in the waypoint's color; those outside the map
  sit on its edge pointing their way.
- In the world: a small colored diamond in the waypoint's direction with the
  distance ("128 m"); the name appears when the crosshair is near it.
- A dedicated Waypoints screen like Shapes, pinned at the bottom of the
  settings sidebar: list, name, color, coordinates, shown/hidden, delete.
- Stored per local world, or per server address and port, then per
  dimension. Lobby-style servers with several worlds share one set for now.
Following (decided 2026-10-01)
- While FreeCamera flies, the map's center and heading follow the camera,
  and the player's arrow is drawn where the player is (hidden when off the
  map). No setting. Freelook keeps following the player.
Text, placement and settings
- Optional lines below the map with a shadow, all default off: coordinates,
  biome, compass letters (N E S W). No clock. Decided 2026-10-01 after the
  first build: the lines are centered on the map whatever its anchor; the
  compass letters are centered on the frame (inside the map they were hard
  to read), all white, no special color for north.
- Its own HUD element, default top right at a medium size (about a fifth of
  the screen height; a "Map size" setting, 10-50 % of the screen height in
  steps of 1, changes the map alone while the layout scale still scales
  the whole element, decided 2026-10-01), movable and scalable in the layout editor. Hidden while
  Debug View is shown (a "hide while Debug View is open" switch like the Info
  HUD's and Target's); not hidden while zooming or in FreeCamera.
- A new settings category "Map" holds the minimap, radar and waypoint
  options (and later the world map), with the Waypoints screen pinned at the
  bottom of the sidebar. Decided 2026-10-01 (first build had one feature
  with 19 children): the category lists features of their own: "Minimap"
  (switch, key; range, size, turning, round, Debug View hiding, zoom and
  enlarge keys, layout link), "Map text" (a heading without a switch:
  compass letters, coordinates, biome), "Cave view" (no switch, its key
  forces the other view), "Radar" (switch; one row per kind and the
  invisible option), and later "Waypoints".
- Bindable actions without default keys: minimap zoom in/out, minimap
  show/hide, add a waypoint, force the cave/surface view, and (2026-10-01)
  enlarge while held: twice the side and twice the area at the same scale,
  at most 85 % of the screen height. New actions are
  appended to `enum Action`.

### Steps
Each step lands on main behind the default-off switch and ends with an
in-game check by the maintainer. Pure logic goes in headers with tests.

Built 2026-10-01 (steps 1-2, unchecked in game): `features/map/MapView.h`,
`MapTiles.h`, `MapImage.h` (pure, `tests/MapTests.cpp`) and `Minimap.cpp`.
The whole image (terrain, arrow, frame) is composed on the CPU into a 256 px
RGBA texture uploaded with `uploadTexture` and then `updateTextureInPlace`.
Scanning runs on the client thread at 1.5 ms per frame (nearest chunks
first, near chunks rescanned every second, far ones less often); there is no
background thread yet. Second build (same day, after the first check):
colors are the average of each block's top texture (`BlockGraphics`, image
from the texture group, at most 6 new textures per frame, cached by block
state and path) times the biome tint (`BiomeColorSampling` map grass/foliage
colors, water color), with the old map color only as a fallback; the
surface is the heightmap block, or the covering block just above it (snow
layer, carpet), looking through glass and plants; shading is cached per
chunk so turning recomposes every frame; lines centered, compass letters on
the frame and all white. Third build (same day): cave view (step 3) with
its own tile cache per view, the Nether always in it; floors are found in
a column from 2 above to 24 below the player's block (rock at feet and head
is a wall), cave chunks rescan when the player's height changes by more
than 1; automatic switch when covered and sky light <= 6 (back at >= 10);
a "minimapview" key forces the other view and returns to automatic; the
dark fill for unloaded ground; FreeCamera following. Fourth build: the
calmer cave view above; blocks the client has not received yet (its
request stand-in blocks) leave the column unknown and get the chunk
rescanned soon; bounded log lines name stand-in and colorless blocks.
Fifth build: a chunk counts as received unless all its columns are
stand-ins, empty columns are the dark "known empty" color; the map size
setting and the hold-to-enlarge key (texture 512 px while enlarged).
Sixth build: in the cave view stand-in blocks count as rock without a
color (the client leaves hidden sub-chunks unrequested, which left holes
in the Nether); the finer range steps and 1 % size steps; the center
snaps to whole pixels when a pixel spans several blocks, stopping the
shimmer at wide zoom; chunks kept for the enlarged view.
Step 4, radar (built 2026-10-01, `MapRadar.h`): dots from the client's
actor list each frame (owned positions, kinds and player names only),
sorted nearest first and capped at 256, placed in whole texture pixels and
drawn into the image (dot 3 and ring 2 mockup pixels, palette from the
mockup's option B); invisible and dead actors are left out; hostile means
the Monster type flag (hoglins count as passive for now); player names in
small text beside their dot, flipped left near the right edge; a "Radar"
switch plus one per kind, items off. After the first radar check
(2026-10-01): the kind rows are named "Radar (players)" and so on; dots
keep their mockup size up to 128 blocks across and shrink with the square
root of the range, to half at 512; "Radar (invisible ones too)", default
off, also shows invisible players and mobs (help text notes servers may
treat it as unfair). While enlarged, the arrow, dots and name gaps keep
their normal on-screen size. The map center and the dots use positions
interpolated with the frame's tick fraction (read in a world render hook;
without it, ticked positions). That alone did not stop the wobble while
running (maintainer, 2026-10-01): terrain and dots were quantized to pixels
separately. The center now always snaps to the texture's pixel grid along
the map's own axes, so both step together; the arrow is placed from the
unsnapped center and stays in the middle.
A world join/exit or a
dimension change discards the data; turning the minimap off unloads the
texture. Any exception turns the minimap off for the session (fail open).
Not yet built from step 2: the vanilla map marker check (the arrow is drawn
by Lamium).

1. Texture spike (Research): build an RGBA image at runtime
   (`cg::ImageBuffer`), register it through
   `IClientInstance::getTextureGroup()` / `mce::TextureGroup::uploadTexture`,
   draw it on the HUD with `MinecraftUIRenderContext::drawImage`, and change
   its pixels each second with `updateTextureInPlace`; a checkerboard is
   enough. Measure a full update at minimap size (about 256-512 px) and
   record what happens on world exit, dimension change, resource reload and
   window resize. If this path fails, record why and ask before considering
   anything at the DirectX level. Also check whether the vanilla map's
   player marker texture can be drawn. Not shown in settings.
2. Surface minimap (Ready once step 1 works): scan top blocks around the
   player from the client's `BlockSource` (height map, block, biome) within
   a per-frame time budget, keep owned colors and heights, shade and upload;
   the HUD element, frame, player arrow, zoom steps, rotating/round options,
   the text lines, the "Map" settings category and the Debug View hiding.
   Background scan/bake work carries world + dimension generation identity
   and discards stale completions. Keep full-dirty data changes separate from
   presentation-only refreshes where that avoids unnecessary work. Tests:
   block/biome color and shading math, negative-coordinate chunk/region math,
   world-to-map transforms (north-up and rotating), zoom steps, scan
   scheduling and stale-result rejection. In game: include negative
   coordinates, Nether, quick world re-entry and dimension changes. First
   Experimental release point.
3. Cave view: detect a ceiling, scan floors and walls around the player's
   height, the Nether always in cave view, the force key. Tests: ceiling
   detection and floor selection on synthetic columns.
4. Radar: collect nearby players and mobs each frame (owned positions and
   kinds only), the dot kinds and colors, fainter dots above/below, player
   names, the per-kind switches. Tests: kind classification and the
   above/below rule.
5. Waypoints: storage per world/server/dimension (tolerant JSON like the
   shapes store), the add prompt, the Waypoints screen, markers on the
   minimap (edge clamping) and in the world (direction, distance, name near
   the crosshair), the last death point. Tests: storage round trip and keys,
   edge clamping, marker projection.
   Decided 2026-10-01 with [demos/waypoints.html](demos/waypoints.html)
   (all recommendations accepted): the add key opens a small centered
   prompt with the name "Waypoint N" (地点 N) preselected and 12 colors,
   the next color after the last one used; Enter adds, Esc cancels; the
   position is the block under the feet when the key was pressed; closing
   it restores mouse capture. The Waypoints screen is built like Shapes
   (list and detail, dock button), nearest first, this dimension's entries
   first and others grayed with a dimension tag, the last death point in
   its own group on top with "Make a waypoint"; delete takes two presses.
   Settings feature "Waypoints" (switch: all markers; key: add here) with
   "Show in the world", "Show on the minimap", "Record the death point",
   a key to open the screen and a key that hides world markers while held.
   An option, default off, shows Overworld waypoints in the Nether at 1/8
   of their coordinates (and Nether ones in the Overworld at 8x). World
   markers get an optional distance limit (default none, 100-10000).
   Built in three parts, each checked in game: 5a (built 2026-10-01,
   unchecked) storage (`WaypointStore`, local world `lamium/waypoints.json`
   beside shapes, servers `config/waypoints/<host>_<port>.json`, Realms and
   other connections without an address for the session only; a file that
   fails to load is never overwritten), the add key and prompt (a mode of
   the settings screen, so it owns the cursor like it), minimap diamonds
   with edge clamping and the death cross, the death point (noticed in the
   world render hook because the death screen hides the HUD, saved from the
   next HUD frame), the "Waypoints" settings feature with minimap, death
   and cross-dimension switches. 5b: world markers, the distance limit and
   the hide-while-held key. 5c: the Waypoints screen and the open key.
   5a passed in a local world on 2026-10-01 (server not checked). The
   death cross is red (maintainer, 2026-10-01: white read as a passive
   mob's dot; the cross shape tells it from red hostile dots). 5b built the
   same day: the camera the world was drawn with is copied in a setupCamera
   hook that runs outside the FreeCamera and Zoom hooks, and markers are
   projected onto the HUD from it (axes from the view matrix, scale from
   the projection matrix; no camera means no markers); pixel diamonds 7
   units across and the cross from rectangles; the distance below, the name
   above it when the crosshair is within 20 x 30 units; far markers drawn
   first; "Show in the world", "Show up to" (0 = any distance, steps of
   100 up to 10000) and the hold key. After the 5b check: the camera's
   world position comes from the entity pass (`mCameraPosition`), because
   setupCamera is camera-relative; "Show in the world" is a choice, decided
   2026-10-01: Always (the key hides while held) / While the key is held
   (the key shows them) / Off (the key does nothing), with the key on that
   row. Markers are placed on whole screen pixels, not GUI units (checked
   smooth in game on `189c554`). 5c built 2026-10-01: the Waypoints screen
   reuses the Shapes layout (`ShapesLayout`; header: "Show all" switch, key
   settings link, dock button; toolbar "+ Add here"); a fourth pinned
   sidebar item between Shapes and HUD layout; the list is the death point
   (if any) then this dimension's waypoints nearest first, then the others
   by name, each with its distance or dimension and a shown switch; the
   editor has the name, a large diamond with coordinates, dimension and
   distance, and X / Y / Z (typed or stepped), Move here, Show and Color
   rows; the death point offers "Make a waypoint" (named "Death point",
   the death record cleared) and delete; delete takes two presses; an
   "Open the Waypoints screen" key under the Waypoints feature.

### World map (step 0 done; first build 2026-10-01, not checked in game)
Step 0 started and finished 2026-10-01. Decided by the maintainer (all as proposed):
- Full screen with thin top and bottom bars, opened by a key (default M),
  closed by the same key or Esc; the game is not paused; HUD and minimap
  are hidden while it is open. Left drag pans, the wheel zooms about the
  cursor, north is up.
- Waypoints from the map through a right-click menu: on empty ground "Add
  here" (the usual add prompt; Y is the recorded surface, else the
  player's Y); on a waypoint "Edit" (opens its row in the Waypoints
  screen), hide/show and delete (two presses); on the death point "Make a
  waypoint" and delete. No teleport.
- Recording runs whenever the world map feature is on, whether or not the
  minimap is shown, with a per-frame budget.
- The on-disk cache has no size cap; the Map settings show this world's
  usage and a two-press clear.
Proposed with them (not questioned): 256×256-block region files of color
and height, stored like waypoints (a local world's lamium folder; for a
server Lamium's config folder per address and port), per dimension;
cached regions read nearest first off the client thread; the minimap
reads the same cache; the Nether recorded in 16-block layers, Overworld
caves not recorded in v1.
Mockup: [demos/worldmap.html](demos/worldmap.html), agreed 2026-10-01 with
every recommendation: the bars as drawn, a dimension switch to view the
others (the player's own marked), the Nether layer stepper with "My height"
(on by default), waypoint names always shown, the menu items as drawn, and
the settings rows (feature switch with the open key M, Nether layer, saved
map size with a two-press delete of the whole world's map).
Build notes (first build): `MapRegion.h` (region data, file format "LMR1"
with runs, shading, 2:1 downsampling), `WorldMapView.h` (view, zoom steps
1/8-16 GUI units per block, image levels), `MapStore.cpp` (regions near the
player kept in memory; a worker thread per joined world reads and saves
region files, builds images by level from regions or the level below with
an LRU of about 40 MB, and is joined on world exit and mod stop; damaged
files count as missing), `Minimap.cpp` `record()` (scans 384 blocks around
the player with its own 1 ms budget whenever the world map is on; chunks
the client has not loaded are filled from the saved regions, Overworld
caves are not recorded), `WorldMap.cpp` (the screen: a pool of 96 runtime
textures, at most 6 uploads per frame, 16 in the first second, a coarser
image stands in until a tile is ready), and a world-map mode in the
settings screen's scene (cursor ownership as for the other screens; the
add prompt opened from the map returns to it; "Edit" opens the Waypoints
screen on that waypoint). Saved under the world's `lamium/map/` or
`config/map/<host>_<port>/`, per dimension (`overworld`, `nether/y<N>`,
`end`). Region edges shade against level ground. L-82 links from the
world map to an external seed map.
After the maintainer's first use (2026-10-01) a review mockup,
[demos/worldmap-review.html](demos/worldmap-review.html), was agreed with
every recommendation and built the same day (not checked in game yet):
- Top bar B: short dimension names (Overworld/Nether/End, 地上/ネザー/
  エンド), no title, close as a drawn cross; "My height" folded into
  "Center on me" (it also returns the Nether layer to the player's height;
  the layer label is accent-colored while it follows). The two-row
  fallback stays for screens too narrow.
- A "World map" item pinned in the settings sidebar (between Waypoints and
  HUD layout). Opened from there, closing the map returns to the settings;
  opened by its key, to the game. With the feature off the sidebar still
  opens it and shows the saved map without recording ("Recording is off").
- A waypoint side panel on the map, toggled by the top bar's "Waypoints"
  button and remembered (`map.worldMapPanel`, no settings row): this
  dimension's death point and waypoints nearest first with shown
  switches; a row moves the map to it; clicking a marker selects it; the
  editor has the name (typed on the map), X/Y/Z steppers, Move here, Show,
  colors, "Open in screen" and a two-press delete. The right-click "Edit"
  selects in the panel.
- The Waypoints screen opened from the map shows "< Map" instead of
  Close; it and Esc return to the map with its view kept.
Checked in a local world 2026-10-01 on `e6781e5` (all points passed).
L-83 completed 2026-10-01 (see BACKLOG-DONE.md): the settings/UI consistency
review covered the map and Waypoints screen. Further map UI changes are new
work, not an open part of L-83.
