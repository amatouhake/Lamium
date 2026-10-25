# Map

Implementation notes for the minimap, radar, waypoints and world map (L-60).
Work status and open choices are in
[BACKLOG-DONE.md](BACKLOG-DONE.md#l-60-map-minimap-waypoints-and-world-map-experimental);
runtime coverage is in [VALIDATION.md](VALIDATION.md), with evidence in
[VALIDATION-LOG.md](VALIDATION-LOG.md).

## Current implementation

L-241 A1 (2026-10-22, checked in game on `fca2ef9` except the server map
storage case):
recording follows the camera's X/Y/Z while FreeCamera owns the view; otherwise
it follows the player. The recording center, cave/surface decision and stable
cave band use that same reference. Cave recording is limited to Overworld
-64..319 and Nether 0..127; the End has no cave recording. Out-of-range cave
folders are ignored when listing saved data and are never written.

Connections without a persistent map folder spill to
`maps/.session/<n>/` inside the mod's data folder. They use the same region
files, listing, usage, clearing and resident eviction as persistent worlds,
so moving far away and returning can reload recorded terrain without keeping
every region in memory. The worker removes the session's folder on world exit;
startup removes leftover `.session` data. Waypoints remain session-only in
memory for unsupported connection identities.

Cave layers and the D controls (2026-10-19, L-228; branch
`feat/map-cave-freecamera`, final checklist passed 2026-10-20 on `7e0e2f9`):
the minimap keeps a broad cave view and records the
Overworld surface plus, while the followed point is under a roof, a 16-block cave
band in new `overworld/y<Y>` folders. The Nether keeps its bands. Surface and
cave recording take turns on the existing budget. The world map's top bar
has one terrain state button; its panel opens under the button, keeps the
waypoint list open and shifts left when space is tight. It offers cave
on/off, broad on/off, a `-`/input/`+` height and a `自分の高さに合わせる`
action. The map opens at the player's height (the FreeCamera eye when it
owns the view); in a cave it uses the stable band height the recorder keeps,
so a boundary step does not open an empty new band. Changing it fixes the
height. "Center on me" returns to the body; a green camera arrow marks the
camera (display only; close and reopen to return). Body reference decides
cave/surface from the body's roof; camera reference uses the camera. The
Nether starts broad and an Overworld cave starts in the selected layer.
The layer view dims floors more than 8 blocks from the selected height to a
solid dark color; broad view keeps them. `MapCave.h` reads up to 128 blocks
below and stops at the first floor. `worldMapNetherAuto` is no longer shown
in settings; its stored field remains for compatibility. See the L-228 review below for the
build it replaces and the checked final build.

Scan grid (2026-10-18, L-210): the resumable ring walk of the L-206 review
left one-chunk arcs and wedges unrecorded inside the view distance when the
player moved during a pass (seen in the saved regions: whole chunks missing
along arcs even over ocean). Each scan path (display surface, display caves,
recording) now keeps a dense grid of what it saw of every chunk in range
(never, missing, partial, complete), moved with the player, and every call
picks the due chunks nearest first from scratch: never-seen, incomplete and
near stale chunks first, then stale complete chunks out to a stale radius
(the recording rescans complete chunks only within 16 chunks; farther ones
are recorded once per visit). The recording range is the disc the client
loads, not its square. Never-seen chunks come before retries; far
incomplete chunks retry longest-waiting first and back off (3 s doubling to
24 s per incomplete result). First in-game run (2026-10-18, 96 chunks, new
terrain): about 700 partial chunks at the edge of what the client had
(~33-40 chunks out) were retried nearest first every 3 s, held the whole
budget, and the 14,600 cells beyond were never looked at. Second run: the
81 chunks around the player stayed partial (likely the provisional-tint
path), were retried every 0.5 s and held the budget the same way. Near
rescans and never-seen chunks now take turns, and near incomplete chunks
back off too (0.5 s doubling to 4 s). A pure test replays that range
(client chunks to 40, partial edge and center, three scans a frame) and
requires every cell to be looked at within four minutes of frames.
Third run: still slow until the view was turned. Picking among 30,000
cells by a full pass each frame cost most of the 1 ms budget, so one chunk a
frame was scanned and it was always a near one. Each call now looks at a
bounded number of cells (near cells, never-seen cells from the first unseen
one, a 2,048-cell window that moves on for retries and stale rescans), and
the kinds take turns across calls (never-seen every other turn). The run
also showed what stays dark is mostly what the client never loaded: the
range filled after the view was turned, which suggests the client asks
for terrain toward where the player looks (not confirmed).
Fourth run (2026-10-18): the range filled, but as scattered specks, since
each chunk was picked up at its own retry time. A chunk seen complete now
makes its incomplete neighbors due at once, so a patch the client sent
fills together. The large view-direction gaps were L-212 (distance fog). A pass that finds
nothing skips until the next chunk is due. The 30 s diagnostics line counts
the recording range by state, to tell unvisited chunks from blocks the
client never received. A pure test walks a player through the world and
requires every chunk that was well inside the range to have been scanned.
Not checked in game yet. The other kind of gap in the saved regions, land
inside the edge chunks of a visit drawn as contour-shaped blobs (sections
the client had not received), is not addressed by this and needs the
diagnostics first.

Recording-range update (2026-10-17, L-206): recording follows the client's
view distance with the previous 384-block width as its floor and the game's
reported maximum (capped at 96 chunks) as its ceiling. Each scan path keeps
a persistent wide sweep independent of display-cache eviction and alternates
it with a nearest-first search within four chunks. This keeps nearby changes,
new player positions and cave layers from waiting for a full wide pass,
including when the minimap is off. Both searches share the 512-cell lookup
cap and the existing time budget (1.5 ms display, 1 ms recording); even a
one-chunk budget gives both paths turns. Pure regression tests cover nearby
refreshes, movement, layers, resource waits, fairness, lookup bounds and
completion despite eviction. In-game coverage and frame times at 32/64/96
chunks remain unchecked for this update.

Bug fix checked locally (2026-10-15, L-199, `d5978e2`): world-marker distance,
range filtering and near/far ordering follow the rendered FreeCamera eye;
outside FreeCamera they retain the player's horizontal feet distance.
The maintainer's local distance/range/restoration checklist passed; server
coverage remains open.

World-map binding fix (L-201, 2026-10-15): the opener closes the map for
mouse buttons and ordered chords as well as a single key. Only a fresh
completing press can close it; waypoint name/coordinate editing and prompts
retain input. The maintainer's local M/Mouse 5/chord and editing checklist
passed on `d5978e2`; server coverage remains open.

Experimental, default off. The minimap, cave view, radar, waypoints and world
map are built. The map uses received chunks and recorded map data; seed-based
terrain, biomes and structures remain a non-goal. FreeCamera moves the minimap
center; Freelook keeps it on the player. Mob faces and player heads are
optional; distant players use vanilla locator state (L-89).

The world map opens with `M`, supports pan/zoom, dimension selection, the
cave/surface terrain button with its height and broad/layer controls, and a
waypoint side panel. Recording is independent of showing the minimap. L-104 adds renderer biome tint with fallback/rescan, stronger relief,
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

Map layers (L-139, built 2026-10-11; passed the 0.2.0 regression check on `2aea267`): schematic
placements and shapes are marks from the features that own them
(`MapLayers.cpp`), keyed by layer and session id (`MapMarks.h`). The world
map selects and acts on them (placements: select, show/hide, open in the
Placed tab; shapes: show/hide, open in the Shapes view); the minimap draws
placements, and shapes with the minimap's Shapes option (off by default).
Overlapping marks take the cursor in a fixed order: waypoints and the death
point, placements, shapes, the smallest footprint first.
Names on the world map (maintainer 2026-10-11, after names crowded on
`2aea267`): a mark's name shows while the pointer is over it (the mark the
cursor order picks), for the selected one and for the one with a menu
open, on a dark plate; the world map option "Always show names" (off by
default) shows them all. Player names from the radar stay always shown.
Layer switches (L-146, passed in game 2026-10-15): the top bar's
Waypoints / Schematics / Shapes switches (`map.worldMapWaypoints`,
`worldMapSchematics`, `worldMapShapes`, default on) filter the world map's
marks through `layerShown` (`MapMarks.h`); a footprint whose layer turns
off is no longer drawn but stays selectable from the side panel list.
Side panel lists (L-204, passed in game 2026-10-15): `map.worldMapList` picks the
listed layer (`listKind`, `MapMarks.h`); `State::allAreas` keeps the
placements and shapes before the layer switches for the list and details.

## Cave and Nether representation review (L-228, 2026-10-19)

Implemented on branch `feat/map-cave-freecamera`; the final checklist passed
in game on 2026-10-20 (`7e0e2f9`). The observations below describe the build
the maintainer reviewed before this change.
The maintainer found no conspicuous L-216 regression in appearance,
opening/zoom speed, Nether/cave display or retention after rejoining, but
questioned whether the terrain the cave view selects is useful. Supplied
screenshots show band Y64-79 dominated by gray terrain and dark gaps with
expected lower lava lakes hard to see, and band Y32-47 with a smaller
visible footprint dominated by lava. Both have a 500-block scale and
"Loading 4". The difference is explained by per-band recording coverage and
the render distance in effect at that time, not by a code defect (maintainer,
2026-10-20). The same X/Z at different Y is a different 16-block band and
need not have the same recorded footprint; the 500-block scale is zoom, not
coverage. Pending images can add temporary gaps.

Previous source behavior (`MapCave.h`, `Minimap.cpp` before L-228): the held
height had 3-block slack and sampled five blocks above through 24 below.
Solid blocks at feet/head height formed a dark wall; a column open through
the window formed a dark drop; other floors used their block color with
depth dimming. Unavailable stand-ins could also count as rock. Thus a deep
lava surface could be outside the window at the higher height and appear in
it lower down. That is a source-based explanation of a possible contributor,
not proof of what each dark screenshot pixel represents.

`MapRegion.h` stores cave terrain separately per 16-block band: the Nether
and now the Overworld. A different band can have a different
recorded/received footprint, which explains the earlier apparent size
change; pending images can add temporary gaps. These storage
bands group scans whose reference height can vary within a band; they are
not fixed cross-sections at a canonical height. Before L-228 the Overworld
recording path saved the surface only; its cave view was transient minimap
data, not a saved cave map for later world-map browsing.

Agreed direction (maintainer 2026-10-19): the map should represent the
cavern the player occupies, showing the current ledge/path and the deeper
floor or lava within that open space. Large Overworld caves share this
goal with the Nether; narrow caves should still communicate floors, walls
and forks. Moving up or down within one space should preserve its
recognizable terrain outline, while separate vertically stacked spaces
remain distinguishable. Height differences should be readable; unknown
terrain, walls and deep drops should read differently. Only received or
recorded terrain may contribute, under bounded scan/upload work.

Overlap priority agreed the same day: prefer the floor the player currently
occupies where a bridge/ledge and a lower floor overlap. On a bridge, its
path takes priority while the surrounding open parts show the deeper floor
or lava. Below it, the lower floor takes priority. The exact visual
treatment and selection/transition algorithm remain open.

Scope extended by the maintainer the same day: persist explored Overworld
cave terrain so it can be revisited on the world map, including after
leaving and rejoining, and retain the surface map alongside it. This is now
recorded in 16-block cave bands; vertically overlapping spaces inside one
band are not distinguished, separate bands are. Implemented on the branch;
the final checklist passed in game on 2026-10-20 (`7e0e2f9`).

Design scenarios to preserve when specifying and implementing L-228:

| Scenario | Required result |
|---|---|
| Move between high and low ledges in one large cavern | Retain its recognizable floor/lava outline while keeping the occupied path readable. |
| Stand on a bridge, then go underneath | Prioritize the occupied floor in the overlap; show deeper terrain in the surrounding open parts. |
| Visit separate cave rooms at the same X/Z and different heights | Keep the rooms distinguishable and retain their recorded terrain for later viewing. |
| Leave and rejoin, then review an explored cave | Its recorded terrain remains available, alongside the surface map. |
| Encounter terrain the client has not received | Distinguish unknown terrain from a wall or a known deep drop; use applicable recorded data where available. |

Interaction decision (D, 2026-10-19): the reduced control model in
[demos/cave-map-simple.html](demos/cave-map-simple.html) was accepted after
the A/B/C mockups were rejected as too complex.

- The top bar has one terrain state button (`地表` / `洞窟 広 Y68` /
  `洞窟 層 Y68`). Its panel opens directly under that button. It does not
  close the waypoint list; when the list leaves too little room, the panel
  shifts left instead.
- The panel has three rows: cave on/off, broad on/off, and height with `-` /
  a directly editable number / `+`. A separate action button,
  `自分の高さに合わせる`, returns the height to the player.
- The map opens at the player's height. Changing the height fixes it; the
  action button returns to the player's height. There is no follow toggle,
  slider, recorded-height list or three-way mode.
- Standard: the Nether starts broad, an Overworld cave starts in the
  selected layer. The minimap stays broad and follows the player.
- The map does not switch between cave and surface while it is open; the
  initial choice is made when it opens.
- Layer switches keep their names while they fit; they become symbols only
  when the bar is too narrow.

Implementation on branch `feat/map-cave-freecamera` (final checklist passed
in game 2026-10-20 on `7e0e2f9`): explored
Overworld cave terrain is recorded in 16-block bands alongside the retained
surface map, using new `overworld/y<Y>` folders under the existing LMR2
layout. Vertically overlapping spaces inside one band are not distinguished;
separate bands are. `MapCave.h` reads a deeper 128-block window and stops at
the first floor, so the occupied floor and lower floor/lava stay in the
cavern without always paying for the full window. The world map's layer view
dims floors farther than 8 blocks from the selected height to a solid dark
color and keeps floors, walls and no-floor distinct; broad view keeps them. Surface and cave recording share
the budget by taking turns. `worldMapNetherAuto` is no longer shown in
settings (the map opens at the player's height); its stored field remains
for settings compatibility.

The world map's top bar state button and its panel follow the D decision:
the panel opens under the button, does not close the waypoint list, and
shifts left when the list leaves too little room. The panel's height field
uses the shared number input; Enter or leaving the field applies it, and
Esc discards it. Layer switches keep their names while they fit and use
symbols only when the bar is too narrow. The panel uses the same translucent
panel fill as the other map panels.

Feedback follow-up (2026-10-19; final checklist passed 2026-10-20 on
`7e0e2f9`): the cave read is now
128 blocks deep with gentler depth dimming, so the Nether roof at Y110 and
Y127 still shows the lava sea. The cave panel and heights follow the
FreeCamera eye while it owns the view (`freeCameraPose`); "Center on me"
returns to the body and a green camera arrow marks the camera without a
click action. Closing and reopening the map returns to the camera. A manual
height edit that lands on the reference height resumes following instead of
offering a redundant match action. The display uses three states only: floor,
wall and no-floor. Layer view dims far floors to a solid dark color instead of a
checker or dots; wall/drop patterns, the stand-in color and the unknown hatch
were removed. A cave scan that used stand-ins waits for its saved region to
merge before recording so saved terrain is not masked after rejoining.
Review follow-up (2026-10-20, checked in game as `c67054d`): the
world map opens at the stable cave band used for recording, so a Y63 to Y64
step does not open the empty Y64 band, and body reference checks the body's
own roof instead of the FreeCamera automatic view.
Follow-up `07f0dc1` (2026-10-20, checked in game): the render loop
uses the same stable reference as the height controls. A `+` that reached the
stable band used to enable following and then snap back to the raw eye height
(64 -> 64 -> 64, 78 -> 79 -> 80 -> 78).
Column revision 4 makes chunks recorded by the previous shallow window due
again. Upper passages within a few blocks above the selected floor and
vertically stacked spaces inside one 16-block band remain limitations of the
band model.

This does not select a connectivity algorithm or claim that a displayed
floor is walkably reachable. The final checklist passed; broader
server/pack coverage remains. The earlier footprint difference is explained
by per-band recording coverage and render distance, not a defect.
DESIGN.md records the product direction and BACKLOG L-228 owns the work
status. The current LMR2 migration contract is unchanged.

## World map zoom-out (L-187, 2026-10-14)

Maintainer 2026-10-14: zoom out far enough that **everything explored fits on
one screen**. It shows more of the already recorded world at once; it does
not extend exploration, cache radius or data collection.

Built (not checked in game): ten steps past 1/8 continue the same 1.5x/1.33x
curve down to 1/256 GUI units per block (about 100,000 blocks across a
400-unit screen). They open only as far as needed: the farthest step is the
closest one at which the layer's explored bounds (whole regions) fit the map
area between the bars and beside the side panel, and never nearer than 1/8,
so small maps zoom as before. Image levels go to 8 (65,536 blocks per
image). On joining a world the worker lists the saved region files once;
images wait for that list, then skip tiles with no region under them without
touching the disk. A coarse image is rebuilt from its previous pixels: only
quarters whose regions changed are built again, so recording near the player
rebuilds one path down rather than every region under a far view. The image
LRU keeps coarse levels longer and also trims during a build. Not measured:
time to build the first far view of a very large map, and memory/IO on it.

## Region file format LMR2 (L-216, 2026-10-19)

Agreed with the maintainer 2026-10-19 and revised after the spec reviews of
that day; the last one closed it (maintainer: no further spec reviews, the
next step is building it). Implemented for PR review on 2026-10-19. Live
filesystem/log checks on `6ff0610` confirmed migration of LamiumTest's
1,563 old regions, preserved recorded columns and reduced folder size;
the maintainer's local appearance, opening/zoom, Nether/cave display and
rejoin-retention smoke check found no abnormality attributed to L-216.
Cave/Nether representation remains under Design review (L-228). Evidence
and remaining scope: VALIDATION-LOG.md, L-216. It replaces `LMR1` for
writing; `LMR1` stays readable.

What is stored: the colors seen and the heights, as in `LMR1`. Blocks and
biomes are not stored: a resource-pack change does not recolor the saved
map, and no block-state conversion is needed when the game updates. If they
are ever wanted they become optional sections.

### Layout

File: `r.<x>.<z>.lm2` in the same layer folders as now (`overworld`,
`nether/y<N>` per 16-block layer, `end`). All numbers little-endian. The
magic `LMR2` names the container and never changes; later formats raise the
version.

```
header (18 bytes): "LMR2" | u16 version (1) | u16 flags (0) | i32 region x | i32 region z | u16 section count
table (18 bytes per section):
  4-byte tag | u8 codec (0 none, 1 zstd) | u8 flags (bit 0 required, bit 1 keep)
  u32 raw size | u32 stored size | u32 CRC-32 of the stored bytes
bodies: in table order, right after the table, no padding, nothing after the last
```

CRC-32 is the IEEE/zlib variant (`"123456789"` gives `0xCBF43926`). zstd
bodies are exactly one frame without a dictionary, with nothing after it;
a content size in the frame, if present, equals the raw size; stored size
is above 0. Codec 0 has raw size equal to stored size. One zstd context is
kept per worker and reused.

`required` is a forward-compatibility flag: a reader that does not know the
section's tag or codec must not rewrite the file. It says nothing about
which known sections must be present; that is listed per section below.

Tags are compared as 4 raw bytes. Lamium writes only A-Z and 0-9, but that
is a rule for writers: a reader never rejects a tag for its bytes, so a
future section with another tag is judged by its flags like any unknown one.

Sections in version 1 (zstd level 3; there is no compression setting):
- `CHNK` (required flag set; may be absent): raw 2,560 bytes, 256 entries
  of 10 bytes in chunk order `z * 16 + x`: u64 recorded time (Unix
  milliseconds, 0 = never) and u16 column revision (0 = unknown).
- `COLR` (required flag set; must be present): u32 palette size `p` (1 to
  65,536), `p` u32 colors in `packColor` byte order (r lowest byte, alpha
  highest), then one index per column in row-major order (z rows of x): u8
  when `p <= 256`, otherwise u16 as a plane of low bytes then a plane of
  high bytes. Raw size is exactly `4 + 4p + 65,536 * (p <= 256 ? 1 : 2)`.
  Every index is below `p`. A color's alpha is 255, or the color is exactly
  0 ("not recorded", an ordinary palette value, so 65,536 distinct columns
  still fit u16 indices).
- `HGHT` (required flag set; must be present): raw 131,072 bytes. Each
  column's height as a difference from `prev = x > 0 ? h[z][x-1] : (z > 0 ?
  h[z-1][0] : 0)` (the start of a row refers to the north column, not to the
  end of the previous row), in 16-bit wrapping arithmetic on unsigned
  values: `u = uint16(h) - uint16(prev)`, stored `z = uint16((u << 1) ^ (u
  & 0x8000 ? 0xFFFF : 0))`; decoding `u = (z >> 1) ^ (z & 1 ? 0xFFFF : 0)`,
  `h = int16(uint16(prev) + u)`. Lossless over the whole i16 range; heights
  are not limited to a world range. Written as a low-byte plane then a
  high-byte plane. Columns not recorded are written with height 0.
- `OLDF` (optional, not kept): raw 20 bytes: the `.lmr` this file absorbed,
  as u64 size, u64 modification time (`last_write_time` converted to the
  system clock, truncated to Unix milliseconds) and u32 CRC-32 of the whole
  `.lmr`. Written with codec 0 (20 bytes gain nothing from zstd); readers
  accept both codecs.

Size limits, all checked from the table before any body is allocated or
decompressed (step 5 below): a file above 8 MiB; more than 64 sections;
known sections must have the raw size given above (`COLR` at most 393,220
before its palette size is read); an unknown optional section with the keep
flag has a stored size of at most 1 MiB, and those sections at most 2 MiB
together. Unknown sections are never decompressed.

### Reading

The result is Loaded, Missing (no file), Damaged or Refused, checked in this
order so that a future file is never taken for a damaged one:
1. File above 8 MiB: Refused.
2. Shorter than 4 bytes or a magic other than `LMR2`: Refused if it starts
   with `LMR`, otherwise Damaged.
3. The version is read as soon as 6 bytes are there, before the header
   length is checked: shorter than 6 bytes or version 0: Damaged; a version
   above 1: Refused (a 6-byte `LMR2` file with version 2 is Refused).
4. Then: a header shorter than 18 bytes: Damaged; header flags other than
   0: Refused.
5. The table, before any body is read: more than 64 sections, a kept
   unknown section above 1 MiB or kept unknown sections above 2 MiB
   together, flag bits 2-7 set, or an unknown tag or codec on a section
   with the required flag: Refused (a v1 writer never produces these, so
   they belong to a newer one). A table that does not fit, bodies that do
   not add up to the file size exactly, a known tag twice, or a known
   section of the wrong raw size: Damaged. Unknown tags may repeat.
6. Region x/z different from the file name: Damaged.
7. Per known section: a CRC mismatch, a body that does not decode to exactly the raw size, an index out of
   range or a color with another alpha: that section counts as missing.
   Without `COLR` or `HGHT` the file is Damaged. Without `CHNK` every
   recorded chunk counts as time 0, revision 0.
8. Unknown optional sections: kept with the keep flag (stored bytes as
   they are) if their CRC matches and `CHNK` was read; otherwise dropped.

What each result does:
- Refused: logged once, not shown, never written; that region's `.lmr` is
  neither read nor deleted. What is recorded there stays in memory for the
  session until normal far-resident eviction. Clearing the world's map removes
  its files with everything else.
- Damaged: logged; the region is read from its `.lmr` if there is one, and
  the next save replaces the `.lm2` and deletes the `.lmr` as below.
- Loaded with no `.lmr`: used as is.

### Unknown sections on a resave

An unknown optional section with the keep flag is written back unchanged
(codec, flags, stored bytes, CRC); one without it is dropped. A future
section whose data belongs to columns or chunks sets keep only if it
carries its own per-chunk times: a newer reader then treats a chunk whose
`CHNK` time differs from the section's own as missing for that section
(the chunk was rewritten by a reader that kept the section blindly). If
`CHNK` is missing, all kept sections are dropped (step 8), because nothing
says which chunks are still theirs.

If retaining these sections plus the current known sections would exceed
64 entries (for example after adding `OLDF`), saving fails with a specific
section-limit message before compression or replacement. Kept data is not
dropped to make room; the existing `.lm2` and `.lmr` stay untouched.

### Column revision

One constant beside the column color calculation, raised whenever a change
makes colors or heights recorded before it wrong (such as the
renderer-tint black of 2026-10-07, or a different surface block choice
like L-205's). Kept per chunk because one region mixes chunks recorded by
different builds. A chunk below the running build's revision is shown as
saved and is due for a rescan whenever the client has it loaded; it is
never discarded unseen.

`record` is given, with the columns, two masks: "produced", the columns
whose final recorded value is this scan's own result (false for columns the
scan did not know, for those `fillFromSaved` filled and for those colored
with the stand-in tint), and "scanned", the columns whose final value came
from this scan at all (produced plus provisional). `underlay` never sets
either. With "full" meaning every column the chunk has recorded after the
merge is produced:

| Scan result | Revision | Time |
|---|---|---|
| full, colors or heights changed | the build's | now |
| full, nothing changed | max(stored, build's) | unchanged |
| partial, filled from saved, or provisional | min(stored, build's); a chunk new to the map 0 | now if a scanned column changed |
| no column scanned | unchanged | unchanged |

Time follows "scanned", so a change of provisional colors alone updates it;
revision follows "produced".

A change of revision alone marks the region for saving but does not make
its images stale; colors and heights changing does both. Because revision
only moves once per chunk and build, this does not resave regions on every
rescan. The unchanged-tile shortcut in `scanChunk` still calls `record` for
a chunk whose stored revision is below the build's, so that the revision
can rise. When the region's file arrives after the scan and `underlay`
fills columns under scanned ones, each chunk that took disk columns gets
min(its revision, the disk chunk's revision) and max of the two times.

The scan grid learns stored revisions from the store (the store reports
the chunks below the build's revision of each region it loads, and the grid
treats them as partially seen), so such chunks become due without a new
grid cell kind.

### Migration and older releases

- Listing on join, the settings' size and clearing include both `.lm2` and
  `.lmr`.
- Reading prefers `.lm2`; when it is missing or Damaged, `.lmr` is read with
  the `LMR1` decoder (every recorded chunk at time 0, revision 0).
- An unreadable or undecodable `.lmr` not matched by `OLDF` is retained and
  logged separately from `.lm2` damage. It does not mark a valid `.lm2`
  damaged or schedule a save by itself, and cannot be deleted as absorbed
  data. A later ordinary save can still persist the current `.lm2`.
- A region read from `.lmr` is marked for saving once, at load.
- Idle conversion (maintainer 2026-10-19: convert the whole map in spare
  time): when the worker has no save, load, listing or image build due, it
  takes one region that had an `.lmr` in the join listing and is not
  resident. The listing is only a list of candidates: right before
  converting, the worker looks at the files as they are now and loads the
  region exactly as a resident load would (`.lm2` first, the `OLDF` check,
  the fill-only merge below, or `LMR1` alone), then saves the result, which
  deletes the `.lmr` under the rule below. A region that became resident,
  or whose `.lm2` is Refused, is skipped; a region with no `.lmr` any more
  is done. At most one every 100 ms; dropped when the world or generation
  changes. Regions with both files are converted the same way, so over time
  the whole saved map is in `.lm2` with no `.lmr` left. Until then, images
  of regions that are not resident are built from the same load, merged in
  memory without saving.
- Saving writes `.lm2` first (atomic replace as now), recording in `OLDF`
  the `.lmr` it absorbed. Right before deleting that `.lmr` the worker
  checks its size, modification time and CRC again and deletes it only if
  they still match what was absorbed; otherwise it is left, the `OLDF` no
  longer matches, and the next load merges it (fill only).
- A region stays marked for saving until its write succeeds or fails
  permanently, and is not
  queued again while a write is in flight. A change while it is in flight
  keeps it marked. Every completion starts a fresh save delay (10 s) for
  any pending changes, including a successful write with newer changes;
  continuous edits therefore remain batched. I/O failures retry after 10 s,
  20 s, 40 s and so on, capped at 5 minutes; success resets the backoff.
  Section-limit errors, Refused files and invalid region data are permanent:
  logged once per region per session, with no save retry or conversion requeue,
  including after eviction and reload. They leave existing files untouched and
  no longer keep a dirty resident from being evicted. World exit still flushes
  transient pending changes once without waiting for their delay.
- An older Lamium does not list `.lm2`, so it cannot damage them (except by
  clearing the world's map, which removes the whole folder as asked); it
  records anew into `.lmr`. When a Loaded `.lm2` and an `.lmr` both exist:
  - the `.lmr` matches `OLDF` (size, modification time and CRC): it was left
    by a save interrupted before the delete. It is deleted without merging.
  - otherwise it was written by an older Lamium after the switch. The merge
    only fills: columns the `.lm2` has not recorded are taken from the
    `.lmr`; recorded columns are never overwritten, whatever the times say.
    A chunk that took columns, or that has at least one column recorded in
    both files with a different color or height, gets revision 0 (columns
    only the `.lm2` has, or equal in both, do not count) (so it is rescanned when visited) and the
    later of its time and the `.lmr` time; other chunks keep theirs. The
    region is then saved with
    this `.lmr` in `OLDF`, which deletes it.
  File times and the system clock never decide which colors win, so a
  clock set back or forth cannot bring old colors over new ones; it can
  only make a chunk look older or newer than it is.

### Implementation

`MapRegion.h` owns chunk stamps, scan-mask merging and the retained LMR1
codec. `MapRegionFormat.h` implements the LMR2 table, CRC, palette, height
planes and reusable zstd contexts; `MapRegionFiles.h` shares the bounded
file reads, migration and guarded atomic save between resident loads,
non-resident image reads and idle conversion. The file table is checked
before allocating bodies. Unreadable files also fail closed for saving.
`MapSave.h` owns failure classification, save state and backoff.
`MapStore.cpp` retains transiently dirty snapshots until success and retries
resident saves and idle conversions with the growing delay above. Permanent
failures are remembered for the joined world, including across resident eviction.
Idle conversion takes at
most one non-resident candidate per 100 ms, after interactive work, and
recounts folder usage at most once per 5 s. It reloads both files at the
time of conversion; listing-time file facts never decide a merge.

`Minimap.cpp` sends the produced/scanned masks and certifies unchanged
chunks whose stored revision is old. Loaded-region revision reports mark
the scan grids partial, including chunks outside the ordinary stale radius.
Revision-only changes save metadata without invalidating images.

Review follow-up (2026-10-19): surface and cave scans now share provisional
tint marking at `blockColor`, so a cave floor using the stand-in biome tint
is partial and cannot certify a chunk as current. Column revision 2 makes
older chunks due for revalidation, including cave chunks the first build
could falsely certify at revision 1. The regression tests cover the new
save cadence, separate legacy status/retention, the 64-section preservation
failure and cave provisional revision/time rules. These corrected runtime
paths have not yet been checked in game; the recorded live audit and smoke
results above belong to `6ff0610`.

L-228 (2026-10-19): column revision 4 makes chunks recorded with the earlier
shallow cave window due again; the saved cave colors then pick up the deeper
read and the simpler three-state view. When a cave scan used client stand-ins
and its saved region has not merged yet, the scan waits for the load instead
of recording stand-ins over saved terrain.

`MapRegionFormatTests.cpp` covers palette boundaries, malformed/future
containers, unknown section retention, revision/time rules, migration,
write refusal, failed atomic replacement and save retry state. Existing
LMR1 tests remain. `scripts/Lmr2Fixture.py` independently writes the
checked-in golden file and reads the C++ output. To repeat the latter after
`xmake run LamiumTests`: `python scripts/Lmr2Fixture.py build/lmr2-cpp.lm2`
(Python `zstandard` required). The fixture is regenerated with `--write`.
Build/tests are not evidence of in-game behavior or migration speed.

Implementation requirements retained from the agreed spec:

- `readRegion` returns Loaded/Missing/Damaged/Refused with the region, the
  `CHNK` entries, `OLDF` and the kept sections; `Resident` and the save
  entry carry the same, plus the `.lmr` facts.
- `queueSave` clears the mark only after a successful write, keeps an
  in-flight flag, and on every completion sets `dirtySince` to now.
- `regionFileName`, `regionPath`, `regionOfFileName`, listing, usage and
  clearing handle both extensions. Listing carries candidate keys only;
  legacy size/time/CRC facts are collected at load and checked at deletion.
- `MapStore.h` `record` takes the produced and scanned masks; the store
  reports chunks below the build's revision per loaded region.
- `Minimap.cpp` `scanChunk` builds both masks (around `fillFromSaved` and
  the provisional flags) and still records unchanged tiles whose stored
  revision is low; `MapTiles.h` `ScanGrid` marks reported chunks as
  partially seen so they become due.
- Idle conversion and the in-memory merge for images of non-resident
  regions in `MapStore.cpp`.
- zstd comes in as an xmake package (with its notice in
  THIRD_PARTY_NOTICES.md and PROVENANCE's dependency table) and is linked
  into `LamiumTests`, so the format is tested end to end.

### Measurements

Measured 2026-10-19 on 150 of LamiumTest's 1,563 regions (Python, so only
the ratios carry over to C++): palette + height deltas + zstd 3 averaged
22.7 KB per region against 136 KB in `LMR1` (6.0x), with colors about
12.5 KB and heights about 10 KB of it; the median region has 288 distinct
colors. zlib 6 on the same data was slightly smaller (20.9 KB) at about
eight times the CPU.

### Tests

- Round trip with palettes of 1, 256, 257 and 65,536 colors; heights
  -32768 next to 32767 and back; a row start referring to the north
  column; unrecorded columns at height 0; a file without `CHNK` (all chunks
  0/0); `OLDF` in codec 0 and in zstd.
- Malformed input: version 0, a 6-byte `LMR2` with version 2 (Refused),
  `LMR3`, `JUNK`, header flags, flag bits 2-7, a known tag twice, unknown
  optional and required sections with lowercase or non-ASCII tags (kept or
  dropped / Refused, never Damaged), a 1.5 MiB kept section and kept
  sections of 2.5 MiB together (Refused), trailing bytes, a table past the end, huge raw or stored
  sizes, wrong known sizes, bad CRC, zstd with two frames or trailing
  bytes, a raw size that does not match, palette size 0 and 65,537, an
  index out of range, a color with alpha between 0 and 255, x/z mismatch.
  Each gives the result listed above; Refused files are left untouched by a
  save.
- CRC-32 of `"123456789"`.
- Unknown sections: optional skipped, written back with keep, dropped
  without it, dropped with a bad CRC, dropped when `CHNK` is missing.
- Revision and time per row of the table, including an unchanged full scan
  (max), a partial rescan, saved fill, provisional, `underlay` after a scan
  (min), and revision 2, a partial rewrite by a revision 1 build, then back
  to revision 2 (the chunk is at 1 and due); an unknown column that has a
  saved color is not produced, so the chunk is not full; the full scan after
  an `underlay`; a change of provisional colors alone updates the time but
  not the revision.
- Migration: `.lmr` only (marked for saving, then `.lm2` with `OLDF`), the
  interrupted-save leftover (deleted unmerged), an older build's `.lmr`
  (fill only; a chunk where only the `.lm2` has extra columns, or where the
  values in both are equal, keeps its revision; a chunk with a column that
  differs goes to 0), a Refused `.lm2` with an `.lmr` (both untouched), a
  Damaged `.lm2` with an `.lmr` (read from the `.lmr`, then replaced), a
  `.lmr` replaced after load (the save does not delete it), a failed write
  retried after the delay and not every frame, idle conversion skipping
  resident and Refused regions, an `.lm2` created after the listing (idle
  conversion merges through the normal load and never writes the old
  `.lmr` over it), and a region with both files converted by idle
  conversion with no `.lmr` left.
- A golden `.lm2` written by an independent Python encoder checked in with
  its script, read by the C++ decoder, and a C++-written file read by the
  script, to catch byte order, zigzag and plane mix-ups. The `LMR1` tests in
  `WorldMapTests.cpp` stay.

In game after the build: an existing world's map looks as before; regions
turn into `.lm2` while playing and the map folder shrinks; opening and
zooming out the world map is not slower; a map recorded by the previous
release merges without losing newer colors; Nether layers and the cave view
still work; a refused file placed by hand is logged and never overwritten,
a damaged one is logged and replaced on the next save; a failed save
(read-only folder) is retried every 10 s, not every frame.

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
files count as missing), `Minimap.cpp` `record()` (scans out to the view
distance around the player, never below 384 blocks and never above the
game's own view-distance maximum, with its own 1 ms budget whenever the
world map is on; each scan path keeps a grid of what it saw around the
player and picks due chunks nearest first every call (L-210), so moving
leaves nothing behind and display-cache eviction cannot make the recording
rescan what it already took; chunks the client has not loaded are
filled from the saved regions, Overworld
caves are not recorded), `WorldMap.cpp` (the screen: a pool of 96 runtime
textures, at most 6 uploads per frame, 16 in the first second, a coarser
image stands in until a tile is ready, and after zooming out the finer
images still on the GPU draw over it and are the last slots taken for new
tiles, L-211), and a world-map mode in the
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
