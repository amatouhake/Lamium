# Map

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/MAP.md`.

A client-side map built only from terrain the client has received (DESIGN
"Product"): a minimap HUD element with a radar, waypoints on the maps and in
the world, and a full-screen world map backed by recorded region files.
Experimental, default off. Agreed mockups: [minimap](demos/minimap.html),
[waypoints](demos/waypoints.html), [world map](demos/worldmap.html),
[world map review](demos/worldmap-review.html),
[cave controls](demos/cave-map-simple.html). Specified here and written
independently; other map mods are reference-only ([PROVENANCE.md](PROVENANCE.md)
group 3).

Settings (category "Map", `Settings::Map`): "Minimap" (switch, key; range,
Map size, heading up, round, Cave view, Debug View hiding, zoom/enlarge keys,
layout link); "Map text" heading (compass letters, coordinates, biome);
"Radar" (switch, one row per kind, invisible ones, faces); "Waypoints"
(switch, add key, Show in the world with its key, Show up to, Show on the
minimap, Record the death point, Temporary death point, cross-dimension
scale, arrival distance, open-screen key); "World map" (switch = Record, open
key `M`, seed link, saved map size with a two-press clear); "Map marks" and
"Slime chunks" headings choose which maps show each layer (L-297).
Remembered on the map without a row: `worldMapPanel`, `worldMapList`, the
layer switches. `worldMapNetherAuto` is no longer shown; its stored field
stays for compatibility. Actions (unbound unless noted): Minimap,
MinimapZoomIn/Out, MinimapView, MinimapEnlarge (hold), AddWaypoint,
HideWaypoints (hold), OpenWaypoints, OpenWorldMap (`M`), ToggleRadar,
ToggleWaypoints, ToggleWorldMap, RadarFaces, ToggleMapMarks, ToggleSlimeChunks.

## Behavior

### Map-wide requirements (maintainer 2026-10-01)

- Clean shutdown: every scan, image or disk thread/task is stopped and joined
  on world exit, dimension change, disabling and mod shutdown, never waiting
  on the game's threads or blocking in DLL unload. Steps adding background
  work are checked by quitting and confirming `Minecraft.Windows` is gone.
- The world map opens as a game screen (the settings screen's scene), not by
  releasing the mouse by hand; closing, focus loss and world exit return
  mouse capture and look control.
- Every map option, including those changed on the world map, lives in the
  "Map" category and is the same setting everywhere.
- The world map opens fast: cached regions show at once, new areas fill in
  without blocking the screen or the game.
- Operation is agreed in a `docs/demos/` mockup before it is built.
- World join/exit or dimension change discards in-memory data; turning the
  minimap off unloads its texture; an exception turns the minimap off for the
  session (fail open).
- The minimap, Lamium HUD and world waypoint markers follow vanilla Hide HUD
  (F1); recording and death tracking continue while hidden (L-106, L-107).

### Minimap

- Square, north up; heading up and round are settings. The player is a white
  arrow with a black edge, drawn by Lamium. About 128 blocks across by
  default; zoom steps 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512 blocks,
  saved as the width.
- Unloaded chunks: a dark half-transparent fill in the card color. Known
  empty ground (no floor in caves, the End's void): an opaque dark color.
  Nothing is requested from a server.
- A 1-unit frame only; compass letters sit centered on the frame, all white.
  Optional lines below (coordinates, biome, compass), default off, shadowed,
  centered on the map whatever its anchor. No clock.
- Terrain is the color the world shows (top texture times biome tint) with
  height shading against north and west neighbors; no day/night darkening.
  The center snaps to the texture's pixel grid along the map's axes so
  terrain and dots step together; the arrow uses the unsnapped center.
  Positions are interpolated with the frame's tick fraction.
- Cave view: under a ceiling it switches on its own; the Cave view setting or
  MinimapView key forces surface or cave, or returns to automatic (L-253).
  Automatic switches wait 1 s after the previous one. The minimap stays broad
  and follows the player; its held height moves only when the player is more
  than 3 blocks away; the roof check uses most of the 3x3 columns around.
- HUD element, default top right; "Map size" 10-50 % of the screen height in
  1 % steps (the layout scale still scales it). Hold-to-enlarge: twice the
  side at the same scale, at most 85 % of the screen height, arrow/dots/names
  at normal size. Hidden while Debug View shows (switch), not while zooming
  or in FreeCamera.
- While FreeCamera owns the view the center and heading follow the camera;
  the player's arrow is drawn where the player is (hidden off the map); no
  setting. Freelook keeps following the player. The camera's heading is a
  green arrow at the center (L-286).
- Placements, shapes, radar and slime chunks show on the maps their "shown
  on" settings choose (L-295, L-297).

### Radar

- Dots by kind, each a setting: players (light blue, named; names flip left
  near the right edge), hostile (red), passive/neutral (white) on by default,
  items (yellow) off. Thick black ring on every dot. Invisible and dead
  actors are left out unless "Radar (invisible ones too)" (off). Help text
  notes servers may treat radar as unfair.
- Dots 8+ blocks above or below are fainter, except players, for whom faint
  means out of range; distant players use vanilla locator state (L-89).
  Optional mob faces and player heads (L-85, L-87).
- Mockup dot size up to 128 blocks across, then shrinking with the square
  root of the range to half at 512. Nearest first, at most 256.

### Waypoints

- The add key opens a small centered prompt (a settings-screen mode owning
  the cursor): "Waypoint N" preselected, 12 colors, next after the last
  used; Enter adds, Esc cancels; position is the block under the feet at the
  press. A switch makes it temporary: removed within the arrival distance
  (L-261; 0 keeps them).
- Minimap: diamonds in the waypoint's color, clamped to the edge pointing
  their way when outside. Temporary ones are hollow in every view.
- World: a 7-unit diamond with the distance ("128 m"), the name above when
  the crosshair is within 20 x 30 units; far ones first; on whole screen
  pixels. "Show in the world": Always (key hides while held) / While the key
  is held / Off (key does nothing). "Show up to": 0 = any, 100-10000 by 100.
  Distance, range and order use the rendered FreeCamera eye, else the
  player's horizontal feet distance (L-199).
- Optional (off): Overworld waypoints in the Nether at 1/8 coordinates and
  Nether ones in the Overworld at 8x.
- Waypoints screen, built like Shapes (list and detail, dock button, "Show
  all", "+ Add here"), pinned in the sidebar: this dimension's waypoints
  nearest first, then others by name with a dimension tag; editor with name,
  X/Y/Z (typed or stepped), Move here, Show, Color; delete takes two presses.
  Opened from the map it shows "< Map"; that and Esc return with the view kept.

### Waypoints and death records

- (Decided 2026-11-13, L-319/L-328) Waypoints off also stops recording a
  death point and removing reached temporary waypoints, a death's waypoint
  included; hiding only the markers is "Show in the world" and the Map marks
  layer.
- (Decided 2026-11-13, L-328) There are two kinds of waypoint, normal and
  temporary, and nothing beside them. A death adds a waypoint named "Death
  point": a cross for its mark, red to start with (white read as a passive
  mob's dot), temporary unless "Temporary death point" is off. Its name,
  color, kind and visibility are edited like any waypoint's.
- (Decided 2026-11-13, L-332) Nothing else is special about a death's
  waypoint: the cross stays whatever is edited, and the next death adds its
  own without replacing one. Death layout restore does not depend on
  waypoints: it belongs to the place of the death (32 blocks around it),
  starts with the first pickup there and ends by leaving it, when everything
  is back, or at the next death. It also ends when the dropped items are gone
  (decided 2026-11-14, L-338): after 5 minutes in total of the place being
  loaded around the player (48 blocks across, in its dimension); time away
  does not count, since unloaded items do not age, and the help text says so.
  The rule behind both: a death's records lean on what waypoints already do
  and never act in a way the player did not intend. The name carries the
  local date and time ("Death point 11-13 21:05") (L-333).

### World map

- Full screen, thin top and bottom bars; opened by `M` (or a mouse button or
  ordered chord), closed by a fresh completing press of the same binding
  (text editing and prompts keep input, L-201) or Esc. Not paused; HUD and
  minimap hidden. Left drag pans, the wheel zooms about the cursor, north up.
- Pinned in the settings sidebar too: closing returns where it was opened
  from. With the feature off the key and sidebar still open it on the saved
  map without recording; the bottom bar's "Record" switch is the feature's
  switch (L-313).
- Top bar: short dimension names (the player's own marked), a drawn close
  cross, the terrain button, the layer switches, "Center on me", "List"; a
  second row only when too narrow.
- Right-click: on empty ground "Add here" (Y is the recorded surface, else
  the player's), teleport items gated by permissions and available commands
  (the ordinary command; server acceptance not assumed, L-104) and the seed
  link (L-82); on a waypoint Edit (selects it in the panel), hide/show,
  delete (two presses).
- Side panel ("List", remembered): rows move the map; clicking a marker
  selects it; editor with name (typed on the map), X/Y/Z steppers, Move here,
  Show, colors, "Open in screen", two-press delete.
- Marks: placements and shapes come from the features that own them (L-139):
  placements select, show/hide, open in the Placed tab; shapes show/hide,
  open in the Shapes view. Overlaps take the cursor in order: waypoints,
  placements, shapes, smallest footprint first. A name shows under the
  pointer, when selected or with its menu open, on a dark plate; "Always
  show names" (`worldMapNames`, off) shows all (maintainer 2026-10-11, names
  crowded). Radar player names are always shown.
- Zoom 1/256 to 16 GUI units per block. Steps past 1/8 open only as far as
  the layer's explored bounds (whole regions) need to fit the map area beside
  the panel, never nearer than 1/8 (maintainer: everything explored fits on
  one screen; it does not extend exploration).
- Images of 256 px, levels 0-8 (up to 65,536 blocks each). On join the worker
  lists region files once; images wait for it and skip tiles without regions
  without disk access. A coarse image is rebuilt from its previous pixels,
  only changed quarters again. About 40 MB LRU, coarse levels kept longer,
  trimmed during builds. 96 runtime textures, at most 6 uploads a frame (16
  in the first second); a coarser image stands in until a tile is ready, and
  after zooming out finer images still on the GPU draw over it and are reused
  last (L-211). Settings show this world's usage; no size cap.

### World map layer switches

- The world map's top bar has three joined switches, Waypoints, Schematics
  and Shapes (`worldMapWaypoints`, `worldMapSchematics`, `worldMapShapes`,
  default on), each with its mark's symbol (diamond, cyan footprint, yellow
  footprint) before the name, left of "Center on me". On: the accent fill and
  frame of a selected button; off: dark with faint text and symbol.
  Remembered in the map settings. Too narrow for the names, they show symbols
  alone and the bottom bar names the one under the pointer ("Show Schematics
  on the map: On"); only when even that does not fit do they go to the second
  row (maintainer 2026-10-15). The cave terrain button is on the first row.
- Off removes its marks from the world map entirely (no ghost, click or
  menu); the death point goes with Waypoints. The side panel works as before.
- The side panel's button is "List" so it does not repeat "Waypoints".

### World map side panel lists

- The panel lists one layer at a time (`worldMapList`): waypoints, placed
  schematics or shapes. Its header names the layer with its count and a down
  arrow; clicking opens a menu of the three with symbols and counts
  (maintainer: tabs side by side do not fit). Selecting a mark on the map
  shows its layer; "+" only for waypoints.
- Placement and shape rows match waypoint rows: symbol in the mark's color,
  name, distance in the player's dimension, nearest first, show/hide switch;
  a click centers and selects (a placement also becomes the schematic's
  selected one), with details and "Open" below.
- Only the dimension shown. A layer switched off keeps its list
  (`State::allAreas`) with a faint "Hidden on the map" line on top.

### Cave and Nether terrain reference

Decided by the maintainer 2026-10-19 (L-228); the controls are mockup D
(A/B/C were rejected as too complex).
- Represent the cavern the player occupies: the current ledge/path and the
  deeper floor or lava in that space, for large Overworld caves as for the
  Nether; narrow caves show floors, walls and forks. Moving up or down in one
  space keeps its outline; separate stacked spaces stay distinguishable.
- Where a bridge overlaps a lower floor, prefer the occupied floor: on the
  bridge its path, with deeper floor/lava in the open parts around; below,
  the lower floor. The exact treatment and selection/transition algorithm
  remain open; nothing claims a shown floor is walkably reachable.
- Three reads only: floor, wall, no-floor (deep drops and unknown terrain
  share the dark no-floor). Only received or recorded terrain, bounded work.
- Explored Overworld caves persist in 16-block bands (`overworld/y<Y>`)
  beside the surface map; the Nether keeps its bands; the End has none.
  Stacked spaces within a band are not distinguished. Cave recording is
  limited to Overworld -64..319 and Nether 0..127; out-of-range folders are
  ignored and never written.
- The cave read goes 128 blocks below and stops at the first floor (the
  Nether roof at Y110-127 still shows the lava sea), gently depth-dimmed.
  Layer view dims floors more than 8 blocks from the selected height to a
  solid dark color (no checker or dots); broad view keeps them.
- One terrain state button (`地表` / `洞窟 広 Y68` / `洞窟 層 Y68`); its panel
  (translucent like the other map panels) opens under it, keeps the waypoint
  list open and shifts left when tight. Rows: cave on/off, broad on/off,
  height `-` / editable number / `+` (Enter or leaving applies, Esc
  discards), and `自分の高さに合わせる`. No follow toggle, slider,
  recorded-height list or three-way mode.
- Opens at the reference height; in a cave at the recorder's stable band, so
  a Y63 to Y64 step does not open an empty band. Changing the height fixes
  it; an edit landing on the reference resumes following; the action returns
  to it. The render loop uses the same stable reference.
- Reference: the FreeCamera eye while it owns the view (`freeCameraPose`),
  else the body; the panel names it (`カメラ` / `本体`); each decides
  cave/surface from its own roof. "Center on me" goes to the body; the green
  camera arrow is display-only; reopening returns to the camera.
- The Nether starts broad, an Overworld cave in the selected layer; the map
  does not switch cave/surface while open.

### Recording and storage

- Recording runs while the World map feature is on, minimap shown or not. It
  follows the camera's X/Y/Z while FreeCamera owns the view, else the player;
  center, cave decision and stable band use that reference (L-241). Surface
  and cave take turns. Range: the client's view distance, at least 384
  blocks wide, at most the game's reported maximum (capped at 96 chunks), as
  the disc the client loads. Budgets: 1.5 ms a frame display, 1 ms recording.
- Scan grid (L-210): each path (display surface, display caves, recording)
  keeps the state of every chunk in range (never, missing, partial,
  complete), moved with the player. A call looks at a bounded set (near
  cells, never-seen from the first unseen, a 2,048-cell window moving on for
  retries and stale rescans); kinds take turns, never-seen every other one.
  Recording rescans complete chunks within 16 chunks, farther ones once per
  visit. Near incomplete chunks back off 0.5 s doubling to 4 s; far ones
  retry longest-waiting first, 1 s doubling to 4 s (`rescanAfter`); a retry
  probes a few columns first and every fourth is a whole scan (L-225). A
  chunk seen complete makes its incomplete neighbors due. An empty pass waits
  for the next due chunk. A 30 s diagnostics line counts the range by state.
- Unloaded chunks are filled from saved regions; incomplete chunks keep
  saved colors; missing sections are requested (L-104).
- Storage per dimension (`overworld`, `nether/y<N>`, `end`, `overworld/y<Y>`):
  a local world's `lamium/map/` and `lamium/waypoints.json`; a server's or
  friend's world's data under its key in Lamium's config folder
  (`map/<key>/`, `waypoints/<key>.json`; which key: L-343 A server's map,
  waypoints and placements are lost when its address changes). Lobby servers
  with several worlds share one set. A waypoint file that fails to load is
  never overwritten.
- Without a map folder, regions spill to `maps/.session/<n>/` in the mod's
  data folder with the same files, listing, usage, clearing and eviction;
  removed on world exit, leftovers at startup. Waypoints then stay in memory.

## Region file format

The LMR2 region files are specified in [MAP-FORMAT.md](MAP-FORMAT.md).

## Code

Under `src/features/map/` unless stated; pure parts tested in
`tests/MapTests.cpp`, `WorldMapTests.cpp`, `MapRegionFormatTests.cpp`,
`WaypointTests.cpp`, `SlimeChunkTests.cpp`.
- `Minimap.cpp`: scanning (`scanChunk`, masks), colors (`blockColor`), cave
  tiles, recording, radar, texture composition. Pure: `MapView.h`,
  `MapImage.h`, `MapColors.h`, `MapCave.h` (`chooseView`, cave window),
  `MapTiles.h` (`ScanGrid`, `rescanAfter`, `dueOf`), `MapRadar.h`.
- `MapStore.cpp`: resident regions, image cache, disk worker, idle
  conversion, `queueSave`. `MapRegion.h` (data, stamps, `fillFromSaved`,
  `underlay`, LMR1, file names), `MapRegionFormat.h` (LMR2 codec),
  `MapRegionFiles.h` (`readRegion`, migration, guarded save), `MapSave.h`
  (failure classes, backoff), `MapSessionFiles.h`.
- `WorldMap.cpp`, `WorldMapView.h`: screen, tiles, uploads, zoom.
  `MapMarks.h` (`layerShown`, `listKind`, hit order), `MapLayers.cpp`
  (footprints), `MapSlime.h`, `SeedLink.h`, `Teleport.h`.
- `WaypointStore.cpp`, `WaypointSession.cpp` (`resolve`), `ServerIdentity.h`,
  `WaypointMarkers.cpp`, `Waypoints.h`, `src/ui/WaypointsView.cpp`.
- zstd is an xmake package (THIRD_PARTY_NOTICES.md, PROVENANCE), linked into
  `LamiumTests`. `scripts/Lmr2Fixture.py` independently writes the golden
  file in `tests/fixtures/lmr2` and reads C++ output: after `xmake run
  LamiumTests`, `python scripts/Lmr2Fixture.py build/lmr2-cpp.lm2` (needs
  `zstandard`); `--write` regenerates it.

## Known

- Textures: a CPU-composed `cg::ImageBuffer` (256 px, 512 enlarged) via
  `IClientInstance::getTextureGroup()` / `uploadTexture`, then
  `updateTextureInPlace`.
- Block color: average top texture (`BlockGraphics`, at most 6 new textures a
  frame, cached by state and path) times the tint (`BiomeColorSampling`
  grass/foliage, water color); the map color only as fallback. Renderer tint
  can come back black (fallback and rescan). Surface: the heightmap block or
  the covering block above (snow layer, carpet), seen through glass/plants.
- Unreceived blocks are request stand-ins; a chunk is received unless all its
  columns are. Hidden sub-chunks stay unrequested, so the cave view counts
  stand-ins as rock without a color.
- Dark areas are mostly terrain never loaded; the client seems to request
  terrain toward where the player looks (not confirmed).
- Automatic cave switch: covered and sky light <= 6, back at >= 10.
- Hostile is the Monster type flag (hoglins count as passive).
- setupCamera is camera-relative: markers use a camera copied in a
  setupCamera hook outside the FreeCamera and Zoom hooks, with the world
  position from the entity pass (`mCameraPosition`); no camera, no markers.
- The death screen hides the HUD, so a death is noticed in the world render
  hook and saved from the next HUD frame.
- Hide HUD: `IOptionRegistry::getHideHud()` each frame.

## Tried

- Vanilla map item palette: unlike the world. Transparent unloaded fill:
  looked wrong. Compass letters inside the map: hard to read.
- Cave view redrawn on every one-block height change: restless.
- Shallow cave window (5 above to 24 below): deep lava in or out by height;
  wall/drop patterns, stand-in color, unknown hatch, checker/dot dimming:
  removed for the three reads.
- Resumable ring-walk scan: left arcs and wedges when the player moved.
- Full pass over ~30,000 cells per frame to pick chunks: ate the budget.
- Retrying incomplete chunks nearest first without turns: held the budget.
- Whole-chunk retries with long backoff: specks; retries now probe a few
  columns, every fourth a whole scan (a try cap at 8 made all later retries
  probes and left holes, L-225).
- Interpolation alone against dot wobble: terrain and dots snapped apart.
- Opening caves at the raw eye height: empty boundary bands, `+` snapping back.
- Body reference judging the roof from the FreeCamera view: wrong roof.
- Every world map name shown: crowded. Panel layers as tabs: do not fit.

## Limits

- Only client-received terrain. Stacked spaces within one 16-block band and
  upper passages a few blocks above the selected floor are not told apart;
  per-band footprints differ with coverage and render distance.
- Unchecked in game: [VALIDATION.md](VALIDATION.md) rows "Minimap,
  experimental", "World map, experimental", "Waypoints, experimental" and the
  map layer rows; [pre-release checks](backlog/pre-release-checks.md).
- Open: [L-287 Make the minimap arrows legible at every map size](backlog/L-287.md);
  [L-317 Forcing the Surface view leaves the minimap nearly useless in the Nether](backlog/L-317.md);
  [L-324 Map: custom blocks are drawn in a dirt-like brown](backlog/L-324.md);
  [L-337 Minimap: one size setting instead of two](backlog/L-337.md);
  [L-343 A server's map, waypoints and placements are lost when its address changes](backlog/L-343.md);
  [L-243 Large terrain gaps that only reload when the player comes near](backlog/L-243.md);
  [L-249 Show the chunks within simulation distance](backlog/L-249.md);
  map findings in [L-339 Smaller bugs found by the 2026-11-14 code review](backlog/L-339.md)
  and [L-342 Lower findings of the 2026-11-14 review, kept for later](backlog/L-342.md).
