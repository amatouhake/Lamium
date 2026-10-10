# Lamium backlog: done and closed

Items moved out of [BACKLOG.md](BACKLOG.md) once finished, closed or parked,
kept with their full history. Sections follow the kind each item had.
Runtime status is in [VALIDATION.md](VALIDATION.md), the evidence in
[VALIDATION-LOG.md](VALIDATION-LOG.md).


---

## Bugs

### L-119 Fence gates show no icon where Lamium draws item icons
Done 2026-10-10: fence gates fixed by the shared-mesh icon path (`cbaacfc`, checked). The shield glint stays under L-91.
Kind: Bug. Reported by the maintainer 2026-10-08; widened 2026-10-10.
Status: open after two runtime rounds (2026-10-10, VALIDATION-LOG), both
reverted. Trace: vanilla slots draw fences, gates, stairs and walls as chunk
type 0 into `_renderGuiBlockTypeItem`; Lamium's `renderGuiItemNew` uses chunk
type 12 into the same function, for gates and fences alike, with block
graphics present. Drawing the preview as chunk type 0 with the slot's alpha 0
showed no block icons; with alpha 1 it looked exactly like
`renderGuiItemNew` (fences and blocks drawn, gates missing). So the call
route is not the cause: the gate mesh itself does not show outside a
vanilla slot. Third round (`1e46a3e`, reverted): without a slot background
and at z 40 the gates stayed missing; drawn at scale -1 (flipped winding,
the snow block then showed its inside faces) they stayed missing too. So
neither depth nor face culling hides them: outside the slot batch the gate
mesh draws nothing at all. Stopped after three rounds (2026-10-10). The fix
that remains is the one parked under L-91: draw icons through the vanilla
slot path (drive an `InventoryItemRenderer` or its pass setup), which
could fix gates, the shield glint and leather layers together. A larger
change; the maintainer decides whether to start it.
Started 2026-10-10 together with L-91 (maintainer). Round 4 (trace
`7dac7d5`, VALIDATION-LOG): vanilla slots do not use the geometry atlas
(`GeometryAtlas::ItemRenderContextImpl` and `renderItemToTile` never ran).
Block items (fences, gates, stairs) are a shared-mesh batch (`UIBatchType`
1, UI material 13, `atlas.terrain`): inside a slot `renderGuiItemInChunk`
adds the block to a mesh the batch draws afterwards with its material,
which is why the slot passes alpha 0. Shields and leather are default
batches (type 0); an enchanted shield has three passes (materials 9, 5 with
`enchanted_item_glint`, 7). Calling `InventoryItemRenderer::getItemRenderInfo`
on a preview stack crashed the game; do not call it again.
Next round (proposal): wrap Lamium's icon in the vanilla flow,
`MinecraftUIRenderContext::beginSharedMeshBatch(batch)` ->
`renderGuiItemInChunk` -> `endSharedMeshBatch(batch)` with a
`ComponentRenderBatch` built like the slot's (key: batch type, material 13,
textures), first for block items in the shulker preview only. Risk: the
context's persistent mesh list is indexed per frame
(`mCurrentPersistentMeshItemIdx`); adding batches outside the UI pass may
disturb vanilla's, so the round watches for flicker and crashes.
Done 2026-10-10 for block and flat items (`cbaacfc`, checked in game):
`inspection/render/ItemIcon` batches chunk types 0 (`atlas.terrain`, alpha
0) and 2 (`atlas.items`, alpha 1) with UI material 13 for every Lamium icon;
fence gates and leather layers now match vanilla. Left: the shield glint
(default batch, passes 9/5/7); the offhand icon was 1 px low (fixed
2026-10-10, icon one unit higher). Shield glint round (`75433f6`,
reverted): drawing chunk 4 then 5 directly after the icon, and chunk 4 inside
a batch with UI material 5 and the glint texture in either texture slot,
all showed no glint. The slot's glint pass needs
`InventoryItemRenderer::preRenderSetup` and the default batch's per-pass
material, neither reachable without a vanilla renderer instance. Parked
until the maintainer wants it; a next try would clone a slot's
`InventoryItemRenderer` and drive its `_render` passes.
A fence gate inside a shulker box shows only its count in Shulker Box
Preview, without the item icon. The maintainer saw the same in the other
places that draw icons the same way (Lamium's own `renderGuiItemNew` calls,
for example the Info HUD item lines), so this is the shared icon path, not
the preview. Expected: icon and count everywhere. Find out whether every
fence gate kind does it and whether other special block items (doors,
signs, beds...) do too, then compare with how the inventory slot draws the
same item (see also L-91 for icons that differ from vanilla slots).

### L-121 Night Vision darkens areas around light sources at low Brightness
Done 2026-10-10: checked in game on the batch build (VALIDATION-LOG).
Kind: Ready (small). Found by the maintainer 2026-10-09 as a bug; the same
day they showed it is the game's own behavior (VALIDATION-LOG).
Status: built 2026-10-10 (`2e29216`, not yet checked in game): child option
"Even brightness" (`lighting.nightVisionEven`, default on) raises
`BaseLightData::mGamma` to 1 while Night Vision is on; the log prints the
game's gamma once per change ("Night Vision: light gamma ..."), to confirm
it is the Brightness value. Decided 2026-10-09 (maintainer): a Night Vision child option
that removes the darkening, **default on**; Lamium's Night Vision should
look fully bright by default. Off gives the vanilla look.
Restated by the maintainer 2026-10-10: with a low game Brightness, Night
Vision makes the area around light sources darker than the rest; Smooth
Lighting widens that area. In the first report this showed as the corners
and gaps that smooth lighting darkens turning dark blue, in a wide ring
around a hole in a floor. The vanilla Night Vision effect does the same, also without
LeviLamina. The game's Brightness setting decides how strong it is: strong
at 0%, weaker at 50%, almost gone at 100%. With Smooth Lighting off the
picture is even.

| Smooth Lighting | Night Vision | Seen |
| --- | --- | --- |
| on | off | normal shading |
| on | on | bright tops, gaps dark blue |
| off | off | weaker shading in the gaps |
| off | on | gaps lit too, even brightness |

Lamium's Night Vision (`NightVision.cpp`) sets the game's own night vision
fields of the light texture data and adds nothing else, so it shows the
vanilla look. Candidate fix within the same hook: while Lamium's Night
Vision is on, raise the light texture's `BaseLightData::mGamma` toward
what Brightness 100% gives, so the user's Brightness setting is not
changed. To find out in a first round: whether `mGamma` is the Brightness
slider's value and whether raising it removes the blue ring, and how much
brighter everything else gets. If `mGamma` does not do it, stop after two
runtime rounds and report before trying a deeper path. The option follows
the usual settings steps (Settings, Options, store, row, English, Japanese
and Chinese strings) and only acts while Night Vision is on.
Constraints (maintainer): do not turn Smooth Lighting off for the player;
keep natural shading where possible; avoid deep render hooks. Check all four
Smooth Lighting / Night Vision combinations, at Brightness 0, 50 and 100%,
in bright and dark places, near light sources and on dense builds.

### L-113 Numbers sit higher than Japanese text in the settings screen
Kind: Bug. Reported by the maintainer 2026-10-08 while checking the change
arrow.
Status: closed 2026-10-10 (maintainer): no longer seen where checked; the
Latin raise fixes already cover it. No change made under this item.
With the Japanese locale, Lamium raises Latin runs (letters and digits) by
1.5 units (`latinRaise()` in `Widgets.cpp`, DESIGN.md) so they share the line
with kana and kanji. In the settings screen the numbers now read as higher
than the Japanese text beside them, which suggests the raise is too large
there (the change arrow, placed for Latin text, sat about 0.8 units above
the kanji). Changing the raise moves every Japanese label, so measure it on a
screenshot of the settings screen (and the HUD) before choosing a new value;
check stepper values, sliders, the key cells and the Info HUD lines.

### L-122 Fake Offhand fires a firework when placing blocks or opening doors
Kind: Bug, high priority. Reported from use 2026-10-09 (maintainer's notes).
Status: fixed and checked in game 2026-10-09 (`77e698e`, checked on
`34c3593`): doors and block placement, the last of a stack too, launch no
firework; gliding and block-target fireworks still do. Cause: with the
right-click binding, the native click handler and the queued press both
decided the same click; after vanilla acted (a door opened, the last block
placed), the queued press saw the changed world and borrowed the firework
too. Now the first of the two decides (`queuedPressDecides`, tested).
With a firework set as the Fake Offhand item, an action that is not meant to
use it (placing a block, opening or closing a door) sometimes launches a
firework. Expected: the placement or door works as usual and a firework
launches only on an intended use (gliding, or a block target with nothing
else to do there; FAKE-OFFHAND.md). First find the reproduction: which
blocks, held items and targets, singleplayer or server. Then check the order
of the slot borrowing, the use-target decision and the right-click handling.


### L-110 Overlay faces z-fight with the blocks they cover
Kind: Bug **(strong model)**. Reported by the maintainer 2026-10-08 while
checking L-15 step 1.
Status: fixed 2026-10-08 and checked in game the same day (restriction faces,
Shapes in Fancy and Simple, ghosts and light overlay unchanged). Cause: Shapes and the
restriction faces were inset 0.005 into their cell; along a grazing view ray
that inset outgrew the pull toward the eye. Faces now stay on their plane and
every overlay takes its pull from `overlay/Depth.h` (rules and tests; see
OVERLAYS.md "Depth").
The breaking restriction's allowed-region faces flicker against the faces of
the blocks they lie on, and Shapes faces show the same problem (seen earlier,
not recorded until now). Schematic ghosts went through several rounds of the
same problem (SCHEMATIC.md, the "looks" before `e09334d`).
Expected: overlay faces never flicker against terrain at any distance or view.
Starting point: `WorldOverlay.cpp` draws Shapes and the restriction faces
through `drawShape` (faces inset 0.005 into their cell, mesh scaled toward the
eye by `withTowardEye`); the schematic ghosts use their own rules
(`faces::beyond`, near-camera handling). Compare the two, then give every
world-space face overlay one shared, tested rule (pure geometry in a header)
instead of per-feature fixes, so a new overlay does not repeat the problem.
Validate Shapes, the restriction faces and the ghosts near and far, in
Fancy/Simple/Vibrant Visuals, and with FreeCamera.

### L-108 Target card text overflows the card during its resize animation
Kind: Bug. Reported by the maintainer 2026-10-07 while checking L-107.
Status: fixed 2026-10-07 (`8366cb9`): while the card grows, its content
waits until the easing background covers it (`cardContentFits`, tested);
checked in game 2026-10-08 (animations on and off, both directions).
With animations on, switching the target from one with few lines to one with
more (for example a block showing its properties) briefly draws the text
outside the card. Expected: no text outside the card at any point while it
resizes.
Starting point: `InfoHud.cpp::drawTargetCard` eases only the background
(`cardMorph`) from the previous box while the header and rows are drawn at the
final size from the first frame, although the comment there says the content
appears once the card settles. Also check targets whose identity
(identifier and name) is unchanged but whose rows change. Validate with
animations on and off, small to large and large to small, and FreeCamera.

### L-107 Lamium HUD elements remain visible with F1
Kind: Bug. Reported by the maintainer 2026-10-07 after checking L-106.
Status: done 2026-10-07; checked in game on `18e2cc8` (DLL `934028ff...16513f9fcbb`).
Vanilla Hide HUD (F1) still leaves at least the minimap, Target Info and
Info HUD visible. The maintainer reports the problem across custom HUD
elements; inspect the other elements when fixing it rather than assuming
only these three are affected. In-world waypoint markers are already fixed
and checked on `775c8c0` (L-106).
Expected: screenshot-facing HUD elements follow vanilla HUD hiding and resume
their configured visibility when the HUD returns. Preserve settings and
background work such as map recording and death-point detection.
Start with `InfoHud.cpp::drawHud` and its call in `SettingsScreen.cpp::render`;
check the remaining HUD draw paths and use the owning client's vanilla HUD
state. No new setting or F1 key interception. Validate hide/restore with each
affected element, with FreeCamera, and that background map/death tracking
continues. The full affected-element inventory remains to be checked.
Implementation (2026-10-07): `drawHud` checks `getHideHud()` and draws
nothing (Debug View, minimap, Status, Target Info, durability, Schematic HUD,
magnification, toasts, Info HUD) while still running map recording, death
tracking and the schematic adjust key; `SettingsScreen.cpp::render` skips the
offhand slot and saturation marks. The HUD layout editor preview is unaffected.
World-space overlays (chunk borders, hitboxes, light levels, Shapes,
schematic ghosts) are not HUD and stay visible; whether F1 should hide them
is an open product question.
The maintainer confirmed every element above hides with F1 and returns,
also during FreeCamera; map recording and death points continue while hidden;
the layout editor preview is unchanged. Evidence: docs/VALIDATION-LOG.md.

### L-106 In-world waypoint markers remain visible with F1
Kind: Bug. Reported and behavior decided by the maintainer 2026-10-07.
Status: done 2026-10-07; checked in game on `775c8c0` (DLL `a719272b...1b67a97e`).
Vanilla Hide HUD (F1) hid the HUD but Lamium's in-world waypoint markers
remained visible, interfering with screenshots. `WaypointMarkers.cpp::draw`
now checks the owning client's `IOptionRegistry::getHideHud()` before drawing.
All of those markers, including death points, names and distances, follow
vanilla HUD hiding. Restoring the HUD resumes the configured visibility
without changing saved waypoint visibility or map recording. No key
interception or new setting.
The maintainer confirmed all marker parts disappear with F1, return to their
configured display when F1 is restored, and also disappear during FreeCamera.
Individual While held key combinations were not reported separately.
Technical notes: docs/MAP.md; evidence: docs/VALIDATION-LOG.md.
The same issue remains in the minimap, Target Info, Info HUD and potentially
other custom HUD elements; tracked separately as L-107 in BACKLOG.md.

### L-84 IME composition leaves its intermediate text in Lamium's text fields
Kind: Bug. Reported by the maintainer 2026-10-01 while checking L-83.
Status: done 2026-10-01; fix checked in game on `384c751`.
Typing "ネザー" with a Japanese IME (n, e, z, a, -, Tab, Enter) into a
shape or waypoint name gave "ｎねｚざーネザー": every composition step was
appended and nothing it took back was removed. The appended pieces match
the composition's growth exactly (ｎ -> ね -> ねｚ -> ねざ -> ねざー -> ネザー),
so the game sends a rewrite as backspace characters followed by the new
text through the native text event, and `SearchQuery::append` rejected any
event with a control character. Fix: `SearchQuery::type` applies 0x08 as a
backspace and drops other control characters; every Lamium text field
(settings search, shape and waypoint names, the add prompt, the world map
panel) uses it. The first events with control characters are logged
("Text input with control characters") to confirm the hypothesis.
Check: type Japanese into each name field and the settings search; the
result is only the converted text; Latin typing and Backspace unchanged.


---

## Ready

### L-129 Villager trades of every level
Done 2026-10-10: all levels and locked tooltips checked (`0a41937`, `c7beed9`, `3cffc44`). Servers unchecked (VALIDATION).
Kind: Research, then Design. From the maintainer's notes (2026-10-09); not
chosen for building yet.
Status: research answered 2026-10-10 (trace `6e29917`, local world): the
client receives every trade. A level 1 weaponsmith's `UpdateTradePacket`
carried 9 recipes with `tier` 0 to 4 (3/2/1/2/1), each with buy/sell items,
counts, uses and `traderExp`, plus `TierExpRequirements` (0, 10, 70, 150,
250) and the trader tier. An exact client-only display of locked trades is
possible; servers send the same packet but were not traced.
Design direction (maintainer 2026-10-10): blend into the vanilla trade screen
as far as possible. Agent's proposal: the higher levels appear in the
vanilla trade list under their usual level headers, below the unlocked
ones, with the look vanilla uses for unavailable trades plus a lock, not
selectable; hovering says which level unlocks it and how much trader XP is
missing. Research first: the list comes from `Trade2ScreenController`
(per-tier collections, `Trade2ContainerManagerModel::getEntityTradeTier`,
`getNumberOfTradesByTier`); find whether the client already holds the
locked recipes there and only stops at the trader's tier, and whether
showing them can stay display-only (no trade request for a locked row).
Fallback if the vanilla list cannot take them: a panel beside the screen in
vanilla's look.
Built and checked 2026-10-10 (`0a41937`): option "All trader levels"
(`inspection.lockedTrades`, default on, toggle action `lockedtrades`)
turns vanilla's `#tier_visible` on for every entry of `trade_tiers`; the
locked look and non-selection stay vanilla's. Open (maintainer): hovering a
locked trade's items should show their description like unlocked ones.
Hover trace `af4f6f4` (round 3, 2026-10-10): unlocked rows bind
`#hover_text` on hover with the full description ("Iron Sword / Looting I /
Bane of Arthropods II / +6 Attack Damage / Durability"); locked rows bind
nothing. The locked toggle (`#trade_toggle_enabled` false, vanilla's
`toggle_locked` state) does not pass the hover to the item buttons inside,
so vanilla never asks for their text. Options for the maintainer: (A) keep
the locked look and have Lamium draw the item's tooltip when the pointer is
over a locked trade's item (find the item controls in the screen's control
tree, as the offhand slot finds the hotbar; text from the item itself); (B)
enable the locked rows' toggles so vanilla hovers work, block their
selection, and lose the locked look on the rows (the level header stays
grey). Maintainer chose (A). Built and checked 2026-10-10 (`c7beed9`):
`LockedTrades.cpp` keeps the offer's items from `UpdateTradePacket`, finds
the locked item under the pointer in `trade_selector_stack_panel`
(tier panels, rows, `trade_item_1`/`trade_item_2`/`sell_item`) and draws
`getFormattedHovertext` in the preview frame; pure parts in
`LockedTradeIndex.h`. Jitter fix `3cffc44` not yet checked.
Show a level 1 villager's trades up to level 5, the locked ones marked and
not usable. First find out whether the client receives the future trades at
all: trace `UpdateTradePacket`, the trade NBT and the UI collection when the
trade screen opens. If they are not sent, an exact client-only display is
not possible and the idea is reconsidered. Open: fitting it into the vanilla
trade screen without a separate feature.

### L-126 Leave FreeCamera when the body is hit (option)
Done 2026-10-10: checked in game on `d2f288c` (hits ended FreeCamera, event 2).
Kind: Ready. From use 2026-10-09 (maintainer's notes).
Status: built 2026-10-10 (`792c931`, not yet checked in game): option
"Leave when the body is hit" (`camera.freeCameraLeaveOnHit`, default off);
`LocalPlayer::handleEntityEvent` with `Hurt` or
`HurtWithoutReceivingDamage` ends FreeCamera and logs "FreeCamera left: the
body was hit". Decided 2026-10-10 (maintainer): an option, **default off**,
that ends FreeCamera and returns to the body's view when the body is hit,
including hits that cost no health (a snowball counts). Being off by
default, a wide trigger is fine: any damage counts, continuous damage
(fire, hunger, poison) included, even though it ends FreeCamera at once;
a player who minds turns the option off. Off keeps FreeCamera through
damage as today. Research first: which client-side signal fires for every
hit (the hurt event the server sends for the local player, hurt time or
animation) and works on servers, not a health drop alone.

### L-125 Waypoints at the current position use the camera during FreeCamera
Done 2026-10-10: checked in game on `d2f288c`.
Kind: Ready (small). From use 2026-10-09 (maintainer's notes).
Status: built 2026-10-10 (`7a19fc0`, not yet checked in game): the Add
waypoint key, the Waypoints screen's add and Move here. Schematic "here"
stays the body's. Decided 2026-10-10 (maintainer): during FreeCamera, a waypoint
added "at the current position" uses the camera's position. The waypoint
creation screen opens as usual (name, color...); only the position it is
filled with changes, and nothing is added to say which one. Otherwise the body's position as today; a position picked
explicitly on the map wins. FreeCamera ends on dimension travel, so the
camera and body share a dimension. Separate from L-124 (what the HUD shows).

### L-124 Camera and player position and facing during FreeCamera
Done 2026-10-10: checked in game after the angle fix `3c17f84`.
Kind: Ready. From use 2026-10-09 (maintainer's notes).
Status: built 2026-10-10 (`af7479f`, not yet checked in game). Decided
2026-10-10 (maintainer chose the agent's proposal): Debug View shows Camera
and Player position and facing lines; block, chunk, light and biome follow
the camera. The Info HUD switches its place and angle lines (coordinates,
scaled coordinates, block, chunk, facing, yaw, pitch, rotation, biome,
light, weather) to the camera with a "Cam" label; speed stays the body's.
During FreeCamera, Info HUD and Debug View show the body's coordinates and
the body's facing (yaw/pitch, direction). The camera has its own position
and its own facing, and neither is shown. Knowing where the body is and
where it faces stays useful, so the camera's values are added, not swapped
in: where there is room (Debug View first) show Camera and Player position
and facing, labeled. Open (Design): how the compact Info HUD shows it (both
lines, a switch, density-dependent), whether position and facing are paired
per owner or per kind, and which owner chunk and biome follow. Body-only
values (health, inventory) never follow the camera.

### L-60 Map: minimap, waypoints and world map (experimental)
Kind: Design completed; implementation built. Remaining work is validation
and the separately listed radar follow-ups.
Status: done 2026-10-10 (maintainer: the remaining checks, including
waypoint storage per server, are fine). L-86 stays under Later / parked.
History: minimap, radar, waypoints and world map built and checked locally
(2026-10-01); minimap/radar/world map also checked on an external BDS with a
large explored area (2026-10-02). L-89 distant players and L-104 map
follow-ups are done.
Requirements, technical notes and the retained decision/build record:
[MAP.md](MAP.md). Runtime coverage: [VALIDATION.md](VALIDATION.md).
Open:
- Waypoint storage per server address/port: in-game validation.
- L-86 radar-face follow-ups (Later / parked).
- Dedicated-server/release coverage for distant players and server checks
  of L-104: see Pre-release checks.
Seed-based terrain, biomes and structures remain a non-goal (L-82).

### L-57 Client info counters
Kind: Research, then built. Split from L-53 on 2026-09-27 (wave 2).
Status: done. Built 2026-10-08 and checked in game the same day locally;
the maintainer reported the server check fine on 2026-10-10. Debug View
shows one line after the fps line, read once a second (`ClientCounters.cpp`;
line model in `DebugLines.h`, tested): entities = `Level::getRuntimeActorList`
filtered to the player's dimension; chunks = the size of the dimension chunk
source's `getStorage()` map (never walked, other threads fill it); particles
= the sum of `ParticleEngine::particleCount` plus
`ParticleSystemEngine::mTotalParticleCount`. Each count fails open (left out).
The help text names what stays out. In game, check that the numbers are
plausible (entities against what is around, chunks against render distance,
particles rising with rain/torches/explosions), in a local world and on a
server, and that the frame time does not change.
- Candidate lines: loaded entity count, loaded chunk count and particle
  count. The SDK exposes `Level::getRuntimeActorList()` and
  `Level::getEntities()`, chunk tracking under `LevelChunkViewTracker`, and
  `ParticleEngine`'s per-type `particleCount`; it is not yet known what each
  returns on the client (whole level vs. focused dimension, cost per frame).
- Establish what one cheap call gives, then decide the lines and their read
  cadence (not per frame if expensive). Keep it read-only.
- Not available on the Bedrock client and must stay out: slime chunk (no
  seed), server TPS/mob caps, Java heap memory, region files, chunk
  section/update stats, the effect list and local difficulty. Record this in
  the help text where users would look for them.

### L-109 Restore the death-time hotbar and inventory layout on pickup
Kind: Design decided 2026-10-08, then Ready **(strong model)**. Idea from the
maintainer 2026-10-07; chosen for building 2026-10-08.
Status: done; shipped in 0.1.8. Built 2026-10-08 (default off, Experimental); checked in game the
same day except two failures, fixed after (VALIDATION-LOG): rejoining read
the player as not alive and dropped the layout (now `LifeWatch`: only a
player seen alive in the world can die), and removing the death point
before the first pickup went unnoticed (now watched every tick). Restore
scope (maintainer, 2026-10-08): the hotbar, armor and offhand by default; a
switch "Also restore the inventory" (off) adds the rest, which takes a
while. A hotbar-only choice was dropped as unneeded, and the two-choice
setting ("Hotbar & equipment" / "Everything", checked on `89950fc`) became
the switch because the English value was cut off and read awkwardly. The
fixes passed the recheck on `1bd1102`. Planner `DeathLayout.h` (one move at a time: swap, or move part of a
stack; equipment never emptied; tested, including 2000 random inventories
for termination and conservation), document `DeathLayoutStore.cpp`
(`death-layout.json` beside the waypoints), glue `DeathRestore.cpp`: the
inventory is snapshotted every tick while alive and the last one becomes the
layout on death; a pickup of a layout item arms a run that starts 1 s after
the last pickup, moves every 250 ms in gameplay only, re-reads the inventory
before each move, and stops after the same move three times or 100 moves
until the next pickup. Known risk: with instant respawn the inventory may
still read full at respawn and be taken for keepInventory.
When the player picks up the items dropped at their death point, rearrange the
inventory to be as close as possible to the layout at death: the same items
back in the same hotbar, inventory, armor and offhand slots. When the feature
is on, the rearrangement runs automatically.
Requirements stated by the maintainer:
- Never drop or destroy an item. Anything that cannot go back to its old slot
  stays in the inventory.
- Items picked up since death and items that were lost (burned, despawned,
  taken by others) are handled by priorities so the result looks close to the
  original to a person, not only by slot count.
Decided 2026-10-08 (agent's proposal, maintainer chose the trigger and armor):
- Trigger: automatic. After an item pickup, once about one second passes
  without another pickup, rearrange; later pickups trigger again.
- Lifetime (corrected by the maintainer 2026-10-08): no real-time expiry.
  Dropped items despawn only while their chunk is loaded and ticking, so a
  long trip back must still restore. The death layout lasts until the next
  death, until everything in it is back, or until the death point is
  removed; it is saved per world with the death point, so it survives
  leaving and rejoining.
- Order: armor and offhand, then hotbar, then the main inventory, each back
  in its death-time slot. Armor is put back on; a slot already wearing
  something else is left alone.
- Matching: the same item with the same enchantments and durability first,
  otherwise the same item. A stack goes back up to its death-time count.
- Items gained since death stay where they are unless they block a target
  slot; then they move to a free slot. Nothing is ever dropped or destroyed;
  a move that needs a free slot and finds none is skipped.
- keepInventory: if the inventory is not empty at respawn, that death is not
  restored.
- Servers: a bounded number of moves per tick, each confirmed from the
  authoritative inventory before the next.
- Default off, Experimental.
Builds on the death-point tracking in L-60 (waypoints) and the inventory
transaction path used by inventory transfer (DESIGN.md "Inventory transfer").
Rearrangements must be confirmed from the authoritative inventory, never
from sent transactions (DESIGN.md Engineering behavior).

### L-112 Readable block state names in the target card
Kind: Ready. Chosen 2026-10-08 with the L-93 target-card redesign.
Status: done. Built 2026-10-08 (`interpretBlockState`, tested) and checked in game
the same day (stairs, trapdoors, slabs, logs, doors). Other blocks with
directions stay raw until named. Text directions (cardinal, facing, block face) now show translated
direction names instead of the raw English word.
Show common block states by name instead of their internal keys and values,
for the card's own state rows and the schematic differences: stairs facing
(`weirdo_direction`), upside down (`upside_down_bit`), slab half
(`minecraft:vertical_half`, `top_slot_bit`), axis (`pillar_axis`),
trapdoor facing (`direction`), alongside the existing facing, open, half and
hinge rows. Unknown states keep their internal name and value. The value
mappings come from the game's documented state values; each new one is
checked in game against the block's look before it counts as verified.

### L-90 Simplified Chinese localization
Kind: Design decided, then implementation. Chosen by the maintainer
2026-10-02.
Status: built 2026-10-02 (agent-drafted text for all keys, `TranslationsZhCN.h`
with a build-time order check, docs/TRANSLATING.md). Checked in game on
`ca25c2c` (fit, baseline and behavior fine; no Latin raise needed). Open: a
native review of the wording, invited from FeixiangTMC as a PR. It shipped
in 0.1.6 as a first AI-assisted translation with corrections welcome; the
native review remains open and is not a release gate.
Closed 2026-10-10 (maintainer): a native review is unlikely to come; wording
corrections are handled one by one if they arrive.
Add Simplified Chinese (`zh_CN`) as Lamium's third official UI locale.
English and Japanese remain supported; Traditional Chinese is not claimed
until there is actual demand and a separately reviewed translation.
Scope:
- Translate user-facing Settings text, feature descriptions, editor/prompt
  text and toasts. A full translated README is not required for this item.
- Replace the fixed two-language `Entry { key, english, japanese }` shape
  with a translation representation that can add another locale without
  duplicating lookup logic at call sites.
- Match the game locale to `zh_CN`; unsupported locales still fall back to
  English.
- Keep every locale complete. Tests must fail when a shipped translation key
  is missing in English, Japanese or Simplified Chinese.
- The first Chinese wording may be prepared by an agent, but Minecraft/mod
  terminology corrections from native users are explicitly welcome. Add a
  short contribution note when the locale ships.
Do not generate Traditional Chinese by mechanical conversion and present it as
official support.

### L-118 Hide distance fog
Kind: Ready. Chosen for 0.1.8 (maintainer, 2026-10-08), from a user
request ("no fog").
Status: done for 0.1.8; checked in game 2026-10-09 on `34c3593` (land
day/night/rain, Nether, End, restoration). Child "Distance fog", selected by
default like the other effects (maintainer, 2026-10-09; `f336be7`). After
vanilla `setupFog` the resolved `mCurrentDistanceFog` moves to 16384/32768
and vanilla's value is put back before the next setup. No effect under
Vibrant Visuals (like Night Vision there): left for later with it.
A new Hide effects child, "Distance fog", for the ordinary fog on land, in
the Nether and the End. The existing underwater, lava and powder snow
children stay separate, so either can be hidden alone. After vanilla
resolves the fog in `LevelRendererPlayer::setupFog` (already hooked in
`HideEffects.cpp`), move the distance fog's start and end far beyond the
render distance; use a safe finite distance, not a huge integer, because of
float precision in the shaders. Render distance itself does not change; the
chunk edge becoming visible is expected. Restores vanilla when off. Check in
game: overworld day/night and rain, Nether, End, with Vibrant Visuals on
and off (fail open if its path differs).


### L-104 Map follow-ups: biome foliage, relief and teleport
Kind: Ready (decided with the maintainer 2026-10-07). Status: done
2026-10-07, checked in game up to `ea70c4d` (local worlds; teleport also in
an environment without /tp).
- Biome tint: block colors use the tint the world's block renderer applies
  (`BiomeColorSampling::getTessellationPolicy(tint).get(block, region, pos,
  nullptr)`) instead of the cartography-map samplers (`getMap*`), which did
  not change swamp foliage. The renderer's tint returns black (0, 0, 0) for
  a while after a chunk loads; black grass was recorded and saved over
  explored land (the rejoin darkening, found 2026-10-07 by logging). A black
  or non-finite tint now falls back to the cartography-map tint and marks
  the chunk for an early rescan. Such stand-in columns, like columns not
  received yet, show the saved map's color when it has one (maintainer's
  choice: as outside the render distance); the stand-in tint shows only
  where nothing is saved. Saved pure black counts as nothing saved, so land
  saved black before the fix recovers when it is scanned again.
- Relief: stronger slope shading only (maintainer's choice over elevation
  brightness or contour lines): 0.1 per block against the north and west
  neighbors, clamped 0.6-1.3 (was 0.06, 0.7-1.2). Shared by the minimap
  and the world map.
- Teleport: the world map's right-click menu adds "Teleport here" (ground)
  and "Teleport" (waypoints, death point) as its last item, only when /tp
  would actually run (maintainer 2026-10-07: usability over the cheat
  setting): the server's command list for the player (`AvailableCommandsPacket`)
  includes `tp`/`teleport`, which also covers LeviLamina's
  `forceEnableCheatCommands`. Until the list arrives, the client's commands
  flag (it follows the world's cheat setting; `LevelData::mCheatsEnabled`
  stays false on the client) and a permission of at least game directors
  decide. Also required: the player's command permission is at least
  game directors, and the target is in the player's dimension; otherwise the
  item is hidden. It sends the ordinary `/tp @s x+0.5 y z+0.5` command
  request (origin type Player); the server decides. Ground height is the
  recorded surface + 1, or the player's height where the map has none.
- Missing sections (2026-10-07; the request part was removed the same day
  because the maintainer saw no improvement): areas the player had not looked toward
  stayed black, often between trees; the client requests a chunk's lower
  sections only when they come into view (`client_request_placeholder_block`
  stand-ins, and sections not received at all read as air: the scan's
  "nothing to stand on" then recorded a known dark column and saved it over
  explored land; outside the End that case is now unknown and requested).
  The scan now notes missing sections and asks for up to 4 per
  frame through `LocalPlayer::requestMissingSubChunk`, the client's own
  request, each again after 3 s at the earliest (maintainer chose this over
  provisional colors). A partly received chunk also keeps the saved map's
  colors for its stand-in columns: after a rejoin, explored land had shown
  black until looked at again (the saved data itself was intact).
  Unverified: request thread safety, server behavior and load.

### L-102 Hand Restock: refill threshold and source stack order
Kind: Ready (decided with the maintainer 2026-10-06, Notion follow-up;
default order chosen 2026-10-07). Status: done 2026-10-07, checked in game on `bd30648` (local survival).
- "Refill at or below" (0-63, default 6): refill after a use leaves the held
  stack at or below N; 0 refills only when it runs out. The value is capped
  below each item's maximum stack. The move itself is unchanged: one source,
  as much as fits.
- "Take from": smallest stack (default) or largest stack, only within a
  source region. Region priority stays main inventory, then the optional
  hotbar; tie-breaks are unchanged. Smallest first empties partial reserves
  (held 6, reserves 12/32/64 takes the 12 and frees its slot).
- Settings keys `inventory.restockThreshold` and `inventory.restockOrder`
  (0 largest, 1 smallest); missing keys take the defaults.

### L-103 Inventory Transfer in the inventory screen
Kind: Ready (decided with the maintainer 2026-10-06, Notion follow-up).
Status: done 2026-10-07, checked in game in every game mode (`bd30648`,
`97c44c6`) and with the drag re-entry fix on trace build `163bb96`.
- The inventory screen (container type Inventory, every game mode; changed
  from survival-only 2026-10-07 at the maintainer's request) moves between the main inventory (upper side) and the hotbar
  (lower side) with the same four gestures and switches: wheel up to the
  main inventory, wheel down to the hotbar. Storage screens keep Storage and
  Player; no three-way routing.
- Destinations are chosen explicitly: the first matching stack with room on
  the other side, else its first empty slot; a stack gesture moves what fits
  and continues with the rest after vanilla accepts each part. Vanilla
  auto-place is not used there because it equips armor.
- A drag moves each slot once per entry (2026-10-07, all screens): staying
  on a slot does nothing more; leaving and entering it again moves what it
  holds then. Before, a slot was skipped for the rest of the drag.
- Armor, crafting and other grids never take part. Shift + left click on a
  hovered item in this screen is now the transfer gesture (with its switch
  on) instead of vanilla's quick move.
- Each distinct layout of the screen is logged once ("Inventory transfer:
  inventory screen with ..."). Survival showed `inventory_items` 27 and
  `hotbar_items` 9 (2026-10-07); a 36-slot layout is handled too.

### L-98 HUD density: line spacing and per-line backgrounds
Kind: Ready (decided with the maintainer 2026-10-06 on
docs/demos/hud-density.html, after community feedback).
Status: done 2026-10-06. Checked in game on `4cf9acd` and, with the
follow-ups, on `f63be4e`. Defaults chosen in game: line height 12, opacity
60 %, Info HUD per line, Status card, Debug View per line.
Decided:
- Scope: the line elements drawn by `drawElement` (Info HUD, Status) and the
  Debug View. The durability HUD, target card and Schematic HUD keep their
  own row heights (icons and bars decide them).
- One shared setting under the general settings: "HUD line height", 9-16 GUI
  units, default 12 (chosen in game; 14 was the old spacing). 9 is Java's
  debug screen spacing.
- Per-line background: a third value "Per line" in the existing per-element
  Background choice of Info and Status (other elements keep None / Card).
  Each line gets a band of its text width plus 1 unit each side, rows touch
  (as on Java's debug screen); text is centered in the band. Fixed padding,
  no extra setting.
- Debug View: a new "Background: None / Per line" choice, default None.
- On a right-anchored element the per-line bands and their text line up on
  the right edge (maintainer, after trying it).
- "Background opacity" (0-100 %, default 60, chosen in game) for every HUD card and band.
- General is split into "Settings screen", "Toggle toasts" and "HUD text"
  headings; line height and opacity sit under "HUD text".
- Found while checking: a tall Info HUD dropped near the top anchored to the
  middle (screen third of its center) and moved when it grew; drops now pick
  the nearer edge (DESIGN "HUD" placement). Saved layouts keep their anchor
  until the element is dropped or snapped again.
In game: line height 9-16 with Japanese and English text (does Japanese fit
at 9-11?), per-line backgrounds on Info and Status (with the markers), the
Debug View background with its right column, the HUD layout editor boxes,
then choose the default.

### L-99 Zoom below 2x
Kind: Ready (decided with the maintainer 2026-10-06, after community
feedback asking for the wheel to reach 1x).
Status: done 2026-10-06; checked in game by the maintainer on `692dfe4`.
Decided (DESIGN.md Camera):
- The wheel goes down to 0.5x while Zoom is held; the setting stays 2x-50x.
- Below 1x the projection widens up to 160 degrees; the wheel floor rises to
  base FOV / 160 so the readout never claims a wider view than is drawn.
- A notch crossing 1x stops on exactly 1x once.
- Below 1x turn sensitivity stays normal (no speed-up).
- The wheel level is kept until world exit or dimension change, below 1x
  too; a level left at exactly 1x reopens at the setting's level.
- The Zoom help text mentions the 0.5x wide view (en, ja, zh_CN).
In game: wheel down past 1x (stops once at ×1.0), down to ×0.5 (wide view,
normal turning), with vanilla FOV at its maximum (stops earlier, near 160
degrees), release and press again (kept), release at ×1.0 and press (back
to the setting), leave the world (setting level).

### L-101 Show the Lamium version in the settings screen
Kind: Ready (spec decided with the maintainer 2026-10-06). Split from L-100.
Status: done 2026-10-06. Checked in game by the maintainer on `ff55b9f`.
The Bug issue form requires the Lamium version. Before this it was only in
`mods/Lamium/manifest.json` and the first line of the Debug View (`F3`,
off by default), which reporters may not find.
Spec:
- The settings panel header shows `Lamium` and then the Lamium version in the
  faint color, in every view inside the panel (All, categories, Hotkeys,
  Shapes, Waypoints, Schematics). It is hidden when it would reach the search
  field (`SettingsTable::placeVersion`).
- Hovering it shows a tooltip under it with
  `Lamium <ver> · Minecraft <ver> · LeviLamina <ver>` (running game and
  loader versions, English in every locale) and "Click to copy for a bug
  report". Clicking copies that line and shows a message toast.
- The `> Shapes` / `> Waypoints` / `> Schematics` breadcrumb is removed: the
  sidebar or tabs show where you are (DESIGN.md). Docked panels keep their
  own title and show no version; the HUD layout editor and the world map
  show none.
- The Debug View's first line uses the same version helpers (`app/Versions.h`).
CONTRIBUTING.md and the Bug form now point at the settings header.

### L-92 Inventory readouts inside the vanilla item tooltip
Kind: Research, then Design. Raised by the maintainer 2026-10-02 after L-64.
Status: done 2026-10-02 (`8e7f8a9` durability, `91d1254`..`49665ca` food
values; checked in game in Japanese and English).
Result: durability is a gray last line of the hover text. Food values end
the hover text with a line of blank U+E0FF glyphs (4 units wide, so 2n + 1
of them hold n icons 8 apart); `HoverTextRenderer::render` draws the whole
tooltip text in one `Font::drawCached` call with its top-left corner (not
through `MinecraftUIRenderContext::drawText`), so Lamium keeps that call's
position, flushes the batched text and paints the hunger-bar icons there.
The values are looked up by the drawn text, because the game builds hover
text for more items than the one shown. Lamium's separate boxes are gone.
Dead ends: color codes do not tint emoji glyphs; the food glyph U+E100 faces
the other way from the hunger icons, so painting over it left parts showing.
The durability numbers and the L-64 food values are drawn as Lamium's own
box above the pointer, separate from the game's item tooltip (name, lore,
enchantments) shown below it. Try to put them inside the vanilla tooltip.
- Text readouts (durability) could be appended to the hover text the way
  the Shulker contents hook (`ShulkerBoxBlockItem::appendFormattedHovertext`
  in `inspection/Inspection.cpp`) already edits it; check that the line
  wraps, colors and localizes like vanilla and that every item type passes
  through the same hook.
- Icon readouts (food values) cannot be text. Find the tooltip's control in
  the screen's visual tree (as the offhand slot and saturation read
  `desktop_hotbar` and `hunger_rend`) and draw inside it, or reserve a line
  for the icons; fail open to the current box when the control is missing.
- Keep one look for both readouts; keep the current boxes until this works.

### L-64 Food values in the inventory
Kind: Ready (L-63 settled the marking on 2026-10-02: the drumstick's own
outline in gold #f2c23a, half marks on the right half; the shared pure code
is `features/information/Saturation.h`). Chosen by the maintainer 2026-09-28
alongside L-63.
Status: done 2026-10-02 (`3ffd304`, checked in game). A box like the
durability readout holds the drumsticks right to left (as on the bar, so
half icons face the same way); saturation beyond the hunger gain adds
outlined empty icons. Raw gains, not capped by the player's state.
Follow-up: L-92 (inside the vanilla tooltip).
Hovering a food item in an inventory shows how much hunger and saturation it
restores, in the same place and style as the durability readout
(`DurabilityTooltip`). Values from the item's food component; foods with
effects (for example rotten flesh) show only the values, not the effects.
Decided 2026-09-28:
- Icons, not text: the hunger gain as drumstick icons (half icons for odd
  values), drawn with the game's own HUD textures the way the Target card
  draws its hearts (`textures/ui/heart*` in `InfoHud.cpp`; the hunger
  textures are the `textures/ui/hunger_*` family - confirm the names). The
  saturation gain is marked on the same icons in the style L-63 settles, so
  the inventory and the hunger bar speak one language.
- Its own row "Food values" under Inventory next to Durability, on by
  default.

### L-63 Saturation on the vanilla hunger bar
Kind: Research, then Design. Chosen by the maintainer 2026-09-28.
Status: done 2026-10-02 (`8cce406`..this commit, checked in game). Research
on trace `8a1214b`. The gold outline is cut in memory from the loaded
`textures/ui/hunger_background` (its edge is the dark outline; `hunger_full`
is only the filling) and uploaded as runtime textures; the game's texture is
never written to disk or the repository (maintainer's condition).
Settled in game: the outline sits at x - 8 - 8i; the held-food preview draws
the gained hunger icons at a fixed 50 % (30-70 % compared) and the gained
saturation as an opaque pale gold (#fff1b8) outline, the left half when it
completes a half-marked icon. A translucent gold outline vanished into the
drumstick. The opacity was a temporary setting and was removed: few players
would change it and extra choices confuse.
Research results (maintainer's run, local world and a server):
- The client has both attributes: `getAttribute(Player::HUNGER())` /
  `SATURATION()` -> `mCurrentValue`. Bread (nutrition 5, modifier 0.6) took
  hunger 14 -> 19 and saturation 0 -> 6; a golden carrot capped saturation at
  the new hunger (20). Gain = nutrition x modifier x 2, saturation never above
  hunger. On the server saturation arrived as well (13.40).
- The bar is drawn by the C++ `hunger_renderer` from the 1x1 HUD control
  `hunger_rend`. Ten 9x9 icons sit right to left from its position, 8 units
  apart (icon i at x - 8 - 8i, y; the trace's x - 9 looked right in 1-unit
  frames but put the gold outline one unit left on `8c23b96`). This matched
  classic at 75 % and 100 % and Pocket UI (top right). The control disappears in creative, and riding was
  fine. Find it by name each frame like the offhand slot; draw nothing when
  it is missing.
Show the normally hidden saturation, drawn over the vanilla hunger bar, so
the player can judge how much food reserve is left before hunger starts to
drop. Behavior reference only; see PROVENANCE.md (group 3).
Decided:
- Drawn on the vanilla hunger bar itself, not as a separate element: an
  outline or inner fill on the drumstick icons marks the saturation level
  (0-20, the same scale as hunger).
- Holding food previews what eating it would give: the hunger and saturation
  gain shows on the bar while the food is
  held. The values come from the item's food component (`getNutrition`,
  `getSaturationModifier`), capped at the maximum.
- Client values only: `Player::HUNGER()` and `Player::SATURATION()`
  attributes of the local player. If the client does not receive saturation
  (for example on some servers), draw nothing rather than a guess.
Research first:
- confirm the client receives saturation (single player and a server);
- find where and how the vanilla HUD draws the hunger bar (render entry and
  icon positions) so the overlay follows GUI scale, hides with the hunger bar
  (creative, riding) and survives non-vanilla resource/UI layouts; if a pack
  moves the bar, the overlay must move with it or stay off, never float in the
  wrong place.
Decided 2026-09-28: saturation is a gold outline on as many drumstick icons
as the saturation level covers (the icons themselves stay readable); the
held-food preview shows the gained icons translucent and still, with no
blinking.
Decided 2026-10-02 with [demos/saturation.html](demos/saturation.html): the
icon's own outline turns gold (mockup option B; the icon is not otherwise
painted), gold #f2c23a, half marks (2 saturation = one icon, 1 = the right
half, fractions dropped), the held-food preview as decided (may be retuned
after seeing it in game), a "Saturation" row under HUD & overlays with a
child "Held food gain", default on. L-64 uses the same marking.

### L-75 Offhand slot beside the hotbar
Kind: Design (small), then Ready. Chosen by the maintainer 2026-09-30.
Status: done 2026-10-02 (`d99bdb1`..`bb9cdb5`, checked in game). Known gap:
enchanted shields show no glint in the slot (L-91, shared with container
previews).
Decided: one slot left of the hotbar (Bedrock has no main-hand setting), the
hotbar's own slot look, count and durability bar drawn like the hotbar,
hidden while the offhand is empty (a child option shows an empty frame),
attached to the hotbar rather than placed in the HUD layout editor, its own
"Offhand slot" row under HUD & overlays with a toggle key, default off.
Bedrock's HUD never shows what the offhand holds (no vanilla setting found by
the maintainer; searches only turn up add-ons and resource packs), so a totem,
map or shield there is invisible during play. Draw one slot for the offhand
item beside the hotbar, as Java does.
Leaning (maintainer, 2026-09-30): a slot frame next to the hotbar on the side
opposite the main hand; settle the look with a mockup in `docs/demos/` first.
To decide with the mockup: which side (fixed or following the main-hand
setting), the frame style (vanilla hotbar sprite or Lamium's own), count and
durability bar inside the slot, hidden while empty or not, and its own
switch under HUD & overlays versus a Hide Offhand sibling.
Research before building: where the hotbar is drawn and how to place beside
it with UI scale and the pocket/classic layouts; whether the game's item
renderer (as used by container previews) draws there.

### L-88 Target health hearts use absolute HP
Kind: Design decided, then a small UI change. Chosen by the maintainer
2026-10-02.
Status: done 2026-10-02 (`c6378e8`, checked in game).
Layout decided 2026-10-02: ten hearts per line, at most five lines (100 HP);
above that the row shows the bar with the number. Scaled hearts (one heart
= N HP) and compressed overlapping lines were rejected.
The Target card's Hearts mode currently fills a fixed number of hearts from
`health / maxHealth`, so a 20-HP and a 40-HP mob can look equally healthy.
Make the hearts encode Minecraft health units instead:
- One full heart is 2 HP. The number of available heart slots comes from the
  target's maximum health rather than a fixed normalized count.
- Current health fills those slots in the same units; odd HP uses a half
  heart. Examples: 20/20 -> 10/10 hearts, 10/20 -> 5/10, 20/40 -> 10/20,
  19/20 -> 9.5/10.
- Reuse the game's health-bar sprites already used by the Target card. The
  Bar and Number modes do not change.
- Do not silently clamp a high-health target back to a normalized 10-heart
  display. If the current card layout cannot present a large derived count
  cleanly, settle that narrow layout question before coding and keep the
  absolute-health semantics.
Tests should cover the examples above, half-heart handling and a maximum-health
value above 20. Confirm in game on ordinary 20-HP and higher-health mobs.

### L-80 Zoom magnification setting and wheel have different lower limits
Kind: Bug, small. Found by the maintainer 2026-09-30 (build 084b424).
The Magnification setting accepts 1x-50x, while the wheel stops at 2x (or
at the setting when it is lower), as DESIGN "Camera" (L-38/L-45) records.
The maintainer finds the mismatch unnatural and suggests 2x as the lower
limit for both. Decided 2026-09-30: both 2x (replaces the L-38/L-45 lower
limit; DESIGN updated). Status: done (144d425, verified in game 2026-09-30, DLL 03de0853).

### L-81 Out-of-range number warning outlives its edit
Kind: Bug, small. Found by the maintainer 2026-09-30 (build 084b424), in
Zoom and Shapes number fields. Finishing an edit with an out-of-range
value ends the edit and keeps the old value, but the footer warning stays,
even on other tabs, until a valid number is entered or the screen is
reopened. The footer `error` in `SettingsScreen.cpp` is one screen-wide
message cleared only by a later successful action. Which rule replaces it
(keep the field open on Enter, or clear the message on navigation) is the
maintainer's choice. Decided 2026-09-30: the edit still ends and keeps the
old value with the warning shown; the warning goes once the user moves to
another tab, row, shape or shape field. Status: done (5a460f3, verified in game
2026-09-30, DLL 03de0853).


### L-74 Shape type icons for the newer presets
Kind: Bug, Ready (small). Found by the maintainer 2026-09-30.
Status: glyphs for all ten types implemented (`ui::shape::typeGlyphs`, tested
count/shape/distinctness) and distinct in game on `87f11cd`. Follow-up asked by
the maintainer: clearer cylinder/sphere/plane glyphs, and the list's color
square replaced by the shape's glyph in its color (`38623de`, unchecked).
The Shapes view draws a 5x5 type glyph per shape (`drawTypeIcon` in
`SettingsScreen.cpp`) but only has four (ring, stacked ring, ball, grid) and
clamps the type index, so box, cone, frustum, pyramid, ellipsoid and dome show
the grid glyph (plane happens to match). Add one glyph per type in
`shape::types` order, keep them readable at 5x5, and add a test that the glyph
count equals the type count. Update `docs/demos/shapes.html` only if it shows
type icons.
Done 2026-09-30, released in 0.1.4 (`v0.1.4`).

### L-78 Opening Lamium views resets FreeCamera position
Kind: Ready. Reported by the maintainer 2026-09-30 on `d20fdf8`.
Status: done; confirmed on `d3f0293`, containing fix `1d28754`, DLL SHA-256
`675AB9C6A76CDAEDD95D15A50DDE2BFED9D2DEB39457DDFB70673FC048960E3F`.
The maintainer confirmed the supplied Toggle Player/World Lamium-view checklist:
position/orientation retained, flight paused in the panel, plus normal inventory,
window movement and explicit FreeCamera release. Hold and lifecycle cleanup
remain broader pre-release checks; individual Escape/Close cases were not given.
The common Settings/Shapes/Hotkeys/HUD-layout opener still called the full
camera reset, despite L-27's decided menu behavior. Suspend input instead:
keep Toggle FreeCamera's wanted state, displacement, reference and rotation;
clear flight input, timing and sprint so no held movement continues behind
the panel. Zoom and Freelook pause and resume when wanted. Hold still ends
when input ownership is lost through the existing action-release dispatch.
World exit, death, dimension/owner changes and explicit deactivation retain
their normal cleanup. Files: SettingsScreen.cpp, Zoom.h/.cpp, CAMERA.md.

### L-77 FreeCamera world reference jitters during rapid body movement
Kind: Research. Reported by the maintainer 2026-09-30 on `43c4211`.
Status: done; the maintainer confirmed improved elytra motion, normal live
reference switching and release on `d20fdf8`, containing fix `1e18b64`, DLL
SHA-256 `79066B8D5ABDC38F6B119A94D2ED1D45783F78F3D3A0FB2195739BE8F2689BBB`.
The runtime log also confirms native interpolation-writer reach.
World position was retained, but after-UI tick-position compensation caused
visible frequent corrections during rapid body movement, including elytra.
Move compensation to CameraAPI's local-actor interpolated-position callback
before native camera offset consumption; preserve vanilla return values,
owner isolation, live reference switching and restoration. Keep the after-UI
fallback until the native callback is observed. Broader frame-rate, lifecycle
and trace-disabled release checks remain in the pre-release list. See CAMERA.md.

### L-58 Target View icons fail for targets without a directly renderable item
Kind: Research. Reported by the maintainer 2026-09-28.
Status: done (verified in game by the maintainer 2026-09-28; commits 06711cb,
971cfa3, 9a4051a, a941826, 786effc; normal build DLL SHA-256
2BC647BE238E521C97D3A876E8CC5C70E4C18D3FC713A0140A65B19EF543F589).
Confirmed: ordinary blocks and the wheat pick item; villager and zombie
villager spawn eggs; snowball, arrow, ender pearl and painting through "the
entity id is itself an item"; the thrown trident; Bedrock's renamed ids
(`ender_crystal -> end_crystal`, `eye_of_ender_signal -> ender_eye`,
`xp_bottle -> experience_bottle`, each checked against the item registry);
dropped stacks; the nether portal and the end portal as one texture frame; and
the end crystal. Falling blocks show the carried block (sand, gravel, anvil,
concrete powder; verified 2026-09-28, commits b989f52 and 51a2ad0): the client
knows it only through the actor's variant, a network block id, while
mFallingBlockId/Data stay 0:0 there and named info_update. Experience orbs, players, lightning and every other
target with neither an egg nor an item stay empty by design.
Target View is expected to show a useful icon for blocks and entities, but
`minecraft:portal` currently shows no icon. A similar class of failure was
previously found for `minecraft:villager_v2`, whose entity identifier does
not directly match the spawn-egg item identifier. Treat this as an icon
resolution coverage problem, not as a `portal -> obsidian` one-off.

Current block resolution calls `Block::asItemInstance(...)` and stores only
an item identifier/aux value. This works for ordinary pick-block results but
can return no usable item for blocks that do not have a normal inventory item.
Current entity resolution mostly derives `<entity id>_spawn_egg`, with small
aliases such as stripping `_v2` and mapping `evocation_illager -> evoker`.
That also fails for identifier mismatches and for entities that have no spawn
egg at all.

Audit and redesign the resolver so the target and its display icon are not
assumed to be the same item namespace. Prefer data/registry-backed resolution
when the Bedrock client exposes it; keep hard-coded aliases only as a bounded
fallback. The icon representation may need to grow beyond
`TargetInfo::iconItem + iconAux` so a target can fall back from a picked item
to a block render/texture, entity-specific representation, or no icon without
constructing a fake ItemStack. Reuse vanilla rendering paths where practical;
do not invent an unrelated substitute icon merely to avoid an empty slot.

Representative runtime/coverage cases:
- ordinary block with a normal item;
- crop/pick-block substitution;
- `minecraft:portal` or another no-item block;
- ordinary mob whose spawn-egg id matches;
- `minecraft:villager_v2` / `minecraft:zombie_villager_v2`;
- other entity/spawn-egg naming mismatches found by registry audit;
- entity with no spawn egg (for example player, item/projectile/vehicle class
  as applicable to Target View);
- falling-block/item-style entities if they are targetable.

Tests should cover pure identifier/fallback decisions, but runtime validation
must prove that every resolved icon actually renders. The fix is complete when
missing-item targets degrade through an intentional fallback path and adding a
new identifier mismatch does not require scattering special cases through
Target collection/render code.

### L-50 Settings search results cannot be expanded
Kind: Ready. Reported by the maintainer 2026-09-27.
Status: done (verified in game 2026-09-27, commit 96d5feb,
DLL SHA-256 125A3B0E670BD3136EECD01706453E3187C6FBAC66225256367C31AD135BA6EF).
Searching for a feature name such as `zoom` showed its collapsed heading, but
clicking the chevron could not reveal Activation, Magnification and the other
children. `setExpanded` returned early for every nonempty query. Search now
permits manual expansion, and a child-setting match that opens automatically
can also be collapsed until the search text changes. Check mouse and keyboard
expansion with `zoom`, and collapse/expand with `magnification`.

### L-51 Hitboxes lag behind moving mobs
Kind: Ready. Reported by the maintainer 2026-09-27.
Status: done. White bounds verified smooth in game 2026-09-27 (commit
3eca83e, DLL 8aa53813); the red eye marker revision f483fff was verified in
game 2026-09-28 on the normal build (commit 786effc, DLL 2BC647BE) with no
remaining visual problem while mobs walk and turn. The marker still works by
interpolating the simulated body position with a sampled eye offset instead of
reading the position the model is rendered from; the maintainer left that open
as a possible more fundamental approach, not as a defect. L-30 (real part
boxes) is the related research item.
Moving mobs originally appeared ahead of their jittering hitbox outlines.
Frame-interpolating the AABB position fixed the white outline. The eye marker
still used `Actor::getEyePos()` plus the AABB translation, and the maintainer
observed it staying at earlier positions. A second build (d31e2ef, DLL
594e6a03) replaced the animated eye position with a fixed eye offset. The
maintainer reported that the red marker no longer follows the eye. The current
revision restores `Actor::getEyePos()`, samples its offset from the simulated
body by actor runtime ID and world tick, and interpolates that offset with the
render frame alpha before adding it to the interpolated body position. Samples
are owned values, discarded when an actor disappears or the world changes.
Checked in game 2026-09-28: walking mobs, turning heads and camera motion
showed no visual problem.

### L-35 Container previews play the item pickup animation
Kind: Ready. Reported by a user 2026-09-26.
Status: done (verified in game 2026-09-26, DLL 766d6fd6).
Items shown in Shulker/Bundle previews play the vertical stretch that vanilla
uses right after picking an item up. The preview draws stacks decoded from the
container, which keep `ItemStackBase::mShowPickUp`; the Info HUD already clears
it on its icon copies (`InfoHud.cpp`). Draw preview icons from copies with
`mShowPickUp = false` (never touch the real inventory stack). Check in game with
a Shulker Box of blocks right after picking it up, and with items that animate
on their own (clock, compass) to make sure those still animate.

### L-36 Breaking does not resume after a forbidden block
Kind: Research. Split from L-15 on 2026-09-26.
Status: done (verified in game 2026-09-26, DLL aadaa782). The trace
(2026-09-26) showed that when `continueDestroyBlock` returns false for a block
outside the region, vanilla calls `stopDestroyBlock` on the previous block and
never calls `continueDestroyBlock` again while the button stays held. The
hook now skips such a block but returns true (no progress, `destroyed` false)
so the session survives; a menu or settings screen still ends it.
Check 2026-09-26 (DLL d421e275): resuming works, but while an allowed block
was cracking, resting the crosshair on a forbidden block kept cracking the
allowed one. Since 8f2d38a the first forbidden target aborts the allowed
block's progress through vanilla `stopDestroyBlock`.
Check (DLL a6fdad6e): cracking stopped, but afterwards allowed blocks showed the
crack animation without breaking; the trace showed client `destroyBlock` calls
returning true that the server never applied, because the aborted session was
continued without a new start action. Since b5094f7 the first allowed target
after such an abort calls `startDestroyBlock` like a fresh click; verified.
With Breaking Restriction on, once the crosshair passes over a forbidden block,
breaking does not resume on an allowed block until the mouse button is released
and pressed again. The forbidden block must still not break, but the held
button must keep working: fix the input/session handoff, not the region
predicate. Find which vanilla breaking-session state is left stopped after the
rejected block and how a held attack normally restarts it. Listed in the
README's known issues. Reproduced again 2026-09-26 (allowed -> forbidden ->
allowed: the second allowed block does not start); the first trace run lost
its log before this step, so the call sequence is still to be recorded.

### L-17 Hand Restock does not replenish
Kind: Research.
Status: done as hotbar reserve selection (decided 2026-09-27). Refilling
from the main inventory and restocking the offhand (e.g. a used totem)
are parked: without an open screen there is no safe vanilla transfer path
(HUD place and take both fail) and forging inventory requests is ruled
out. Revisit only if such a path appears.
Consumption is detected, but the transfer through the HUD fails
(`handlePlaceAmount` returns false). See HAND-RESTOCK.md and VALIDATION.md.
2026-09-27 trace: the HUD and screen controllers report the same transfer
context (`closed=false client=true simulation=false`), so the simulation flag
is not the differentiator; place still returns false with no request. The
Decided 2026-09-27: auto-select a compatible hotbar reserve via `selectSlot`
(the proven Tool Switch API) when the selected stack is consumed; no stacks
are rewritten. Verified in game 2026-09-27 (DLL 1f1f7816): egg consumed with
a hotbar reserve selects it (`selected hotbar reserve`), inventory-only
reserve stops with `no transfer path`, no reserve does nothing. Main-inventory
replenishment stays an open issue (HUD transfers through this class are
unsupported: place and take both fail).
Offhand totem consumption fires no
use/use-on/complete callback (passive damage path), so offhand restock needs
a separate observer. Desired scope also includes **offhand auto-restock when
a safe vanilla-backed path exists**, especially replacing a consumed Totem of
Undying from inventory. Treat offhand consumption/slot mapping as a separate
runtime path: do not assume the main-hand use observer or HUD indices apply,
and do not synthesize stacks or forge inventory packets. Main-hand success is
not required to prove feasibility, but each path needs independent runtime
validation.

### L-37 FreeCamera cannot see caves from underground
Kind: Research. Reported by a user 2026-09-26.
Status: parked as a known limitation 2026-09-27 (README known issues). No
remaining approach without disassembly; see the traces below.
Flying FreeCamera into the ground does not show caves the way spectator mode
does: chunk sections are missing or culled, so underground spaces cannot be
looked at cleanly. Likely the render-chunk visibility/occlusion pass is seeded
from the player rather than the detached camera, or spectator gets special
handling. Find which pass decides visible sections and what spectator changes,
then decide whether FreeCamera can use the same path without affecting the
player's own rendering. Separate from L-28 (third-person collision judder).
2026-09-26 observation: from inside solid ground FreeCamera stops drawing
distant caves along straight chunk lines, while spectator at the same spot
shows them; from inside a cave FreeCamera looks normal. This fits an
occlusion flood fill seeded from an opaque section. Survival samples show
culler type 3; the spectator/FreeCamera samples were lost with the log.
Second trace (2026-09-26): survival and FreeCamera use culler type 3 (the
renderer camera position does follow FreeCamera underground), spectator
switches to culler type 5. Next: find where the renderer picks the culler
type (spectator, no-clip or camera-in-block check) and whether FreeCamera
can select type 5 without making the player a spectator for game logic.
Third trace (2026-09-26): making `Actor::isSpectator` answer true for the local
player during FreeCamera (F10) left the culler at type 3, so the renderer does
not decide from `isSpectator`. Next candidates: the player's game type
(`Player::getPlayerGameType`) or the no-clip ability.
Fourth trace: answering Spectator from `getPlayerGameType` (F10) did nothing
either; the renderer called it once in the whole session. Remaining option
without disassembly: request culler type 5 through the camera's virtual
`updateLevelCullerType` while FreeCamera is underground, and check whether the
renderer keeps it or rebuilds type 3 every frame.
Fifth trace (2026-09-26): the request had to be repeated every frame (~60/s)
and the view went blank while it ran, so the renderer rebuilds its culler each
frame and type 5 cannot be forced this way. No option is left without
disassembly; proposed to park L-37 as a known limitation (awaiting the
maintainer's answer).

### L-14 Hidden offhand still shows a shield
Kind: Research.
Status: done (verified in game 2026-09-27, DLL ce07ac91). The shield is an
attachable drawn by `DataDrivenModel::renderAttachable`, not by the
item-in-hand renderer (`renderOffhandItem` was not called at all with a
shield). Skipping that draw for the local player's `OffhandItem` slot in
first person hides it; third person and blocking still work. The
`shouldRenderAttachableOnActor` predicate is never consulted here. History:
With Hide Offhand on, totems disappear but a shield is drawn slightly lower.
2026-09-27 trace: the shield reaches `ItemInHandRenderer::renderItem` with
WorldPass|InHand and renderingMainHand=false (totem never does), bypassing
the `renderOffhandItem` skip. The fix skips that world-anchored
offhand render too (main hand and other passes untouched). 2026-09-27 check:
the site=2/flags=34/mainhand=0 combo no longer logs (the skip works) but the
shield is still visible, so a SECOND path draws it. The trace now logs every
`renderOffhandItem` call with its flags plus `renderItemNew`, to catch it.

---

## Ready


### L-61 Held-item durability HUD
Kind: Ready **(strong model)** except the elytra flight time, which needs a
short Research step first. Chosen by the maintainer 2026-09-28 from user
feedback (a UI pack's compact held-tool readout such as `1188/1561`; behavior
reference only).
Status: decided 2026-09-28 (look in
[demos/durability-hud.html](demos/durability-hud.html)). Implemented
2026-09-30 without the flight time: the feature row (default off), look,
offhand and armor options, the gliding elytra row and the layout element
(`information/DurabilityHud.h`, tested). Passed in game on `87f11cd` except
leather armor icons: their undyeable layer is missing, dyed or not, in both
this HUD and the shulker box preview (`renderGuiItemNew` draws one pass; the
position rounding in `cb3c078` did not help). Vanilla slots use
`renderGuiItemInChunk` type 2, but calling it outside a slot drew a flat tint
square (reverted); the slot's chunk setup is not reproduced. Parked by the
maintainer 2026-09-30 (a known issue, also in container previews). Decided 2026-09-30 (maintainer): no elytra
special handling for now. The gliding elytra row (first, outlined) is removed;
an elytra is an ordinary chest row under the armor option. The flight time is
parked with it; the gliding and flight-time bullets below are not built.
A HUD element that shows the durability of what the player holds and wears
during normal play, so wear is visible without opening the inventory.
Decided:
- Main hand by default: one row with the item icon, a short bar and
  `remaining/max`, only while the held item is damageable; nothing is drawn
  otherwise.
- Look option, default "Bar and number" (demo B); the others are "Number"
  (icon + `remaining/max`, demo A) and "Bar" (icon + bar; the number appears
  below 25 %, demo C).
- Colors follow vanilla: the bar uses the item durability bar's hue ramp
  (green -> yellow -> red, `DurabilityBar.h`); the number stays the normal
  text color. No extra warning colors.
- No flashing or other animation when durability drops.
- Options, both default on (revised 2026-09-30 before release; the feature
  row itself stays off): offhand (shield and other damageable offhand
  items) and armor (helmet, chestplate or elytra, leggings, boots). Row order:
  main hand, offhand, head, chest, legs, feet.
- While gliding, the elytra row is shown even with the armor option off, as
  the first row with a static accent outline, because the elytra only wears
  while gliding.
- Default position: bottom left of the screen. It is its own HUD element (a
  new `HudElementId`, appended), placed and styled in the layout editor like
  the others. Values come from the item stacks each frame (`getDamageValue`,
  `getMaxDamage`); nothing is kept across frames.
- Elytra flight time ("about 6:12" beside the elytra while gliding) is an
  option (default on) that ships only if the estimate is sound: expected
  seconds = (remaining - 1) x expected seconds per durability point, with
  Unbreaking read from the item (`EnchantUtils::getEnchantLevel`). Research
  first: measure in game how fast the elytra wears with Unbreaking 0 and III
  to confirm Bedrock's rule. If no rule matches the measurements, the option
  is left out rather than showing a wrong time. Mending is not predicted; the
  help text says the time assumes no experience is picked up.
- Settings (confirmed 2026-09-28): its own row "Durability HUD" under HUD &
  overlays   with the look, offhand, armor and flight-time options as children,
  next to the other HUD elements; the existing Durability feature under
  Inventory (hover readout and preview bars) stays as it is.
Tests: row selection (held/offhand/armor/gliding), bar fraction and the
"number below 25 %" rule, the flight-time estimate, settings round trip.
In game: each look, options on/off, elytra while gliding, non-damageable
items draw nothing, layout editor placement.
Done 2026-09-30, released in 0.1.4 (`v0.1.4`). Leather armor's missing undyeable
icon layer stays a known issue (parked); the elytra row and flight time are
parked (not built).

### L-76 FreeCamera world position reference
Kind: Ready. Requested by the maintainer 2026-09-30.
Status: implemented; build and pure tests pass, runtime validation pending.
Add a saved Player/World position reference to FreeCamera, retaining Player
as the default. World compensates player movement through the existing
camera entity offset without touching body position. Preserve the current
camera target when changing the reference during a session; reset on session
exit and begin at the new eye on reactivation. Target readouts share the
position calculation. Runtime checks moved to BACKLOG's Pre-release checks;
see CAMERA.md and VALIDATION.md.

### L-26 FreeCamera flight speed and sprint acceleration
Kind: Ready; scope and acceleration decided with the maintainer 2026-09-30.
Implemented 2026-09-30; runtime checks remain in BACKLOG's pre-release list.
- FreeCamera's child setting saves a base speed of 5-100 blocks/s in steps of
  5, default 20. Speed-up/down actions are appended, unbound by default, and
  only consume input during active FreeCamera gameplay. One fresh press or
  wheel notch changes the base speed by 5, saves it and shows a short message.
  Holding a speed key does not repeat; the limits do not wrap.
- The game's extracted held-sprint input doubles horizontal displacement
  after diagonal normalization. Vertical speed keeps its ordinary value;
  base 100 reaches horizontal 200 while sprinting. Releasing sprint restores
  base speed immediately. Yaw-relative horizontal flight and jump/sneak
  vertical movement stay as before. This is the agreed control behavior,
  not a claim to duplicate vanilla creative acceleration or inertia.
- Focus loss clears the stashed movement/sprint sample and timing while
  keeping the existing toggle-session position policy.
Validation: release DLL, LamiumTests and LamiumNativeTests built and passed;
pure checks cover speed bounds/snapping, maximum boost and release, diagonal
normalization, unchanged vertical displacement, settings round trip and input
context. Native checks cover held SprintDown before movement consumption.
No Minecraft result yet; see CAMERA.md and VALIDATION.md.

### L-72 Product icon on GitHub, Bedrinth and LeviLauncher
Kind: Ready (small). Chosen by the maintainer 2026-09-30.
Status: done 2026-09-30 except what needs a tag. The master is
`assets/icon/lamium-icon.svg` (1024x1024, provided by the maintainer);
`scripts/Export-Icon.ps1` renders `lamium-icon-512.png` with headless Edge or
Chrome (no download); `tooth.json` `info.avatar_url` points at that PNG on
main and the README shows the SVG. Bedrinth/LeviLauncher show it after the
next tag (a pre-release check); the GitHub social preview is the maintainer's
repository setting.
Direction (from the maintainer's 2026-09-28 notes): the square image is the
primary mark; LeviLauncher rounds the corners itself, so no mask is baked in;
it must read at 24-32 px.
To do: export a 512x512 PNG (and smaller sizes only if a surface needs them)
from the SVG with a reproducible script or documented command, commit the
exports next to the master, set `tooth.json` `info.avatar_url` to the
PNG's raw GitHub URL on main, show it in the README, and optionally set the
GitHub social preview (a repository setting, done by the maintainer). The
release ZIP is unchanged; `manifest.json` gets no icon field.
In game/app: LeviLauncher and Bedrinth list and detail pages show the icon
after the next tag (registry refresh), GitHub renders the README image.

### L-53 More Info HUD lines (wave 1)
Kind: Ready. Status: done 2026-09-30 (the maintainer checked the
follow-up in game and found no problem). Agreed with
the maintainer 2026-09-27 during the Info & HUD review; a compact set of
everyday information lines (behavior reference only: MiniHUD, PROVENANCE.md
group 3). Default off for every new line, with only a small everyday set
enabled by default.
- Add providers and rows: real time (IRL clock), scaled coordinates (the
  Nether 1:8 conversion; only where it applies), yaw and pitch as separate
  lines, speed split into horizontal/vertical, a sprinting line shown only
  while sprinting, world difficulty, and the biome registry id beside the
  localized biome name.
- Japanese labels are `視点角度`, `水平角` and `上下角` for the combined
  rotation, yaw and pitch lines; values do not carry a degree symbol.
- `バイオーム表示` follows the Biome switch at the same settings depth and
  selects name, name + registry id, or id only. `現実時刻の表示` similarly
  follows Real time and selects time only or ISO-style date + time. These
  format rows are not separate HUD lines and do not appear in the layout
  editor's Lines popover.
- Files: `Settings.h` fields, `Options.h` rows, `SettingsStore.cpp` load/save,
  `InfoLines.h` + `InfoHud.cpp` providers, `infoLineIds`/`mergeLineOrder`,
  `Translations.h` EN + JA. The settings list and the layout-editor Lines
  popover are built from those definitions, so no separate edits there.
- Pure formatting in `InfoLines.h` with tests; `SettingsStoreTests` round trip;
  `TranslationsTest` covers the new keys.
- Out of scope: the client counters (L-57) and the Debug View layout (L-54).
- In game: enable each line, check the value and the unavailable fallback;
  nether coordinates convert correctly; speed splits match the old total at
  plain walking.
Status (2026-09-28): the maintainer confirmed scaled coordinates in the
Overworld, Nether and End, the movement-dependent lines and difficulty. The
first build exposed that normal gameplay does not load the game's Editor-only
biome-name translations. Lamium now supplies the current vanilla English and
Japanese names, with registry-id fallback for unknown biomes. The revised
names and format rows await an in-game recheck.

### L-45 Zoom level feedback
Kind: Ready (decided 2026-09-26, no mockup). Maintainer feedback on L-38.
Status: done (verified in game 2026-09-26 after commit `cd3850a`; the batched
research builds ran through DLL `1db48ae3`). The first check (DLL `d421e275`)
passed the wheel floor, readout and setting, but the readout was too prominent,
too close to the crosshair and could not be moved. It is now its own HUD
element at 75% scale, dimmed and 36 units below center, with placement and look
controls in the layout editor. The wheel stops at 2x and the element is on by
default, as recorded in DESIGN "Camera".

### L-56 Info HUD default lines follow DESIGN
Kind: Ready. Found 2026-09-27 during the Info & HUD review.
Status: done (commit `8807973`; accepted without a separate runtime check when
L-54/L-55 closed in `e605e63`). DESIGN "HUD" says the default-on lines are
coordinates, facing, biome and FPS. The shipped implementation had coordinates
and dimension on, with facing, biome and FPS off. Fresh settings now enable the
four DESIGN lines and leave dimension off. Existing files keep their stored
values without migration or rewrite. `SettingsStoreTests` covers the fresh-file
defaults.

### L-01 Settings key
Status: done (9e6ee5c). Change the `settings` action's default key from F8 (0x77) to
`L` (0x4C) in `input/Binding.h`; update README, translations/help text that
mention F8, and BindingTests. Users with an override keep it; Minecraft may
keep its own saved mapping for the Lamium key, so tell the user to check
Keyboard settings after updating. Only the settings action's default changes.
F8 is missing on some keyboards and is meaningless for Lamium. Vanilla
Bedrock keyboard defaults (options.txt, 1.26.51): Q drop, 1–9 hotbar, E
inventory, F5 perspective, Space jump, Shift sneak, Ctrl sprint, WASD, Z mob
effects, T/Enter chat, / command, C copy coordinates, X copy facing
coordinates, B emote, F2 screenshot, F4 social, [ ] menu tabs, N toast.
Lamium already uses C (Zoom, clashes with copy coordinates), J, R.
Free single letters include F G H I K L M O P U V Y. Changing a default only
affects users without an override; Minecraft may keep its own saved mapping.

### L-23 Lamium owns all key bindings
Status: done (75e87d9; dispatch fix below). Verified in game 2026-09-23:
L, C, J, R in inventory/chest/ender chest, nothing fires while typing.
Follow-up: running actions inside the key event crashed Sort (Minecraft
asserts when inventories are read outside the client tick). Actions are now
queued and run through `ClientThreadExecutor`. Do this before L-22. Decided: Lamium is the only place key
bindings live; Minecraft's keyboard settings no longer list Lamium actions.

Why: actions were registered in Minecraft's keyboard settings (KeyRegistry)
and could also be overridden in Lamium. Two sources confused Reset: after the
default moved from F8 to L, Lamium's Reset still gave F8 because Minecraft kept
the old key in options.txt, and a Lamium override silently beat any change in
Minecraft's screen.

- `input/Binding.h` (pure, tested): `defaultChord(Action)` returns
  `{Key, defaultKey}` or an empty chord when `defaultKey` is 0;
  `effectiveChord(Bindings const&, Action)` returns the override when present,
  else the default.
- `input/CustomInput.cpp`: dispatch every action through its effective chord
  (today only overridden actions are dispatched here). Keep all existing
  context rules unchanged: text editing, `ui::ownsInput()`, gameplay screens,
  Sort only in containers, focus loss, cancelled events, release handling.
- `input/Actions.cpp`: remove the KeyRegistry registration and `usesNative`
  (and the `registerActions` call in `app/Runtime.cpp`). `actionBindingName`
  shows the effective chord via `bindingChordName`.
- Reset removes the override, which now means Lamium's default. Change the
  `resetBinding` text to "Reset to default" / "既定に戻す". Remove the
  `key.Lamium.*` translation entries if nothing uses them any more.
- README: replace the sentences saying Minecraft's keyboard settings provide the
  base mappings.
- No migration: Lamium has no release yet. Leftover `Lamium.*` lines in
  Minecraft's options.txt are harmless.
- Note for the hand-back: single-key defaults are now consumed by Lamium during
  gameplay (C is also Minecraft's "copy coordinates").
- In-game checks: L opens settings; C zoom, J night vision, R sort in an
  inventory; existing custom chords still work; nothing fires while typing in
  chat or a sign; Lamium no longer appears in Minecraft's keyboard settings;
  Reset in Hotkeys returns an action to its default.

### L-22 Never leave the settings action without a key
Status: done. Found while testing L-01.
- Clearing the settings action's binding saves `"settings": []`, an explicit
  unbind that overrides Minecraft's mapping. The settings screen then cannot
  be opened at all, so the binding cannot be fixed in game.
- Do not offer Clear for the settings action (Reset stays). When loading,
  ignore an explicit empty binding for `settings` (treat it as absent, i.e.
  use the default key after L-23), so existing files recover.
- Tests: BindingTests / SettingsStoreTests for both rules.

### L-24 Remove the unused action-label localization hook
Status: done.
- `src/ui/Localization.cpp` hooks `Localization::_getSimple` only so that
  Minecraft's keyboard settings could show `key.Lamium.*` labels. After L-23
  nothing native asks for them, so the hook runs on every string lookup for
  no reason. Remove the hook and its install/uninstall; keep
  `ui::translated`, which the settings UI uses for those labels.

### L-02 Replace gameplay key hints with an "Open Hotkeys" action
Status: done. Follow-up 2026-09-24: `openshapes` also belongs to the settings
feature, so all three screen openers group under 全般 in Hotkeys.
Follow-up 2026-09-28: the dedicated Hotkeys, Shapes and HUD layout openers
temporarily select their destination without replacing the page remembered by
the ordinary Settings opener. A manual sidebar navigation from a dedicated
view does replace the remembered page.
- Remove the gameplay key-hint overlay and the `interface.gameplayHints`
  setting (keep loading old files without error; just ignore the key).
- Add action `openhotkeys` (Press, unbound): opens the settings screen on the
  Hotkeys view. Mirror how `OpenShapes` opens the Shapes view.
- Files: `Binding.h`, `Actions.cpp`, `SettingsScreen.cpp/.h`, `Options.h`,
  `Settings.h`, `SettingsStore.cpp`, `SettingsRows.h`, `Translations.h`, README.
- Tests: SettingsStore round trip without the key; binding count.

### L-03 Toggle toast
Status: done. Follow-up 2026-09-24: no `[switch]` marker; card background by
default (the panel fades, the text dims); the last 0.3 s dims the text instead
of fading; the toast fires only after the new state is saved. Open: the
toggle switch stays fixed-size when the toast element scales; decide after
seeing it in game.
- When a Toggle action changes a feature from a hotkey, show the feature
  name with its toggle switch for 1.5 s centered above the hotbar, dimming
  over the last 0.3 s. A new toast replaces the current one. Not shown for
  changes made inside the settings screen, and not shown when the save
  fails.
- Setting `interface.toggleToasts` (default on) under the Settings screen
  feature.
- Put the timing/replace logic in a pure header (`ui/Toast.h`) with tests;
  draw with `ui::toggleSwitch` and `ui::label`.
- Files: `input/Actions.cpp` (emit), `features/information/InfoHud.cpp`
  (draw call site), new `ui/Toast.h`, settings files, translations.

### L-04 HUD elements (split into L-04a to L-04c)
Design decided: DESIGN.md "HUD" and [docs/demos/hud.html](demos/hud.html).
Replaces the separate Info HUD, automation status and restriction status
placement. Covers the review points: few info options, hard positioning,
plain look, fixed-position automation status.

#### L-04a HUD element model
Status: done.
- Pure `ui/HudElement.h`: anchor (9 presets), pinned flag, offset, scale
  75-150 %, background (none/card), shadow. Placement math replaces
  `HudLayout::fit` (the anchor point stays put when the element grows; clamp
  to the screen). Drag resolution: nearest anchor when not pinned, offset only
  when pinned; small offsets snap to 0.
- Settings per element with load/save and defaults matching the demo.
- Tests: placement at all anchors, growth direction, clamping, drag rules.

#### L-04b Move existing HUD pieces onto elements
Status: done. Notes 2026-09-24: info lines reorder with Left/Right on the
row (Enter/click still toggles; drags wait for the L-04c editor).
2026-09-24: HUD fills looked opaque because the gameplay screen renders four
views per frame and the HUD was drawn on each; it now draws only on the
`hud_screen` view, fills are translucent, and Target defaults to the card.
- Info lines, target info, status (automation + restriction) and the toast
  (L-03) draw through the element model. Status merges the automation and
  restriction lines into one element with colored markers.
- Info lines become an ordered list with per-line switches (order saved).

#### L-04c Layout editor **(strong model)**
Status: done (verified in game 2026-09-25, merged at 1c6ff4e). Decisions in
DESIGN "HUD" and demos/hud-editor.html:
- Placement: no pin; drop position decides the anchor (screen thirds);
  flush edges allowed; snap at the edge, at a 4-unit inset and to center
  lines; defaults and snap-to buttons use the inset.
- Element toolbar instead of the side panel; it stays put on look changes.
- Settings list: replace the per-element placement/look rows with one
  "Placement and look" link row per HUD feature; add an unbound
  "Open HUD layout" action under General.
- Card look A (Bedrock popup, stepped corners).

### L-05 More Info HUD lines (providers only)
Status: done. Notes 2026-09-24: thunder is not separately exposed (the weather
line shows Clear/Rain via `Weather::isRainingAt`); day count, clock epoch and
the 8-day moon cycle derive from `Level::getTime()` total ticks and need one
in-game check against /time query and the visible moon.
- Add optional lines, each with a setting, English/Japanese label and an
  "unavailable" fallback: rotation (yaw/pitch, one decimal), facing with axis
  ("North (−Z)"), block position, chunk position and position inside the
  chunk, speed (blocks/s from position deltas over ≥0.5 s), time of day and
  day count, weather, moon phase.
- Check that each value is really available on the client (SDK headers). If
  one is not, leave it out and note it here; do not estimate.
- Pure formatting and speed averaging in headers with tests
  (`PlayerInfo.h` / new `InfoLines.h`).

### L-07 More target information (providers only)
Status: done. Notes 2026-09-24: active effects have no list API (only single
`getEffect`), so they are omitted; thunder is not part of this task; armor
rows show only above zero; `facing_direction` 0-5 maps down/up/north/south/
west/east and crop maxima cover common crops only — confirm in game.
- Extend `collectTargetInfo` with client-available details: block: growth
  stage for crops, redstone power level, facing/half/open states in readable
  form; entity: health/max health, armor points, baby/adult, tamed/owner if
  exposed, active effects if exposed.
- Represent them as typed rows (label, value, optional progress 0–1) so L-08
  can draw bars. Keep `TargetRows` pure and tested.
- Anything not present on the client is omitted, not guessed.

### L-08 Target card **(strong model)**
Status: done (verified in game 2026-09-25, merged at 1c6ff4e). Icons use the
pick-block item for blocks and the spawn egg for mobs; hearts use the
game's sprites; the card follows the camera during Freelook/FreeCamera.
- Icon (blocks/items through the item renderer already used by container
  previews; mobs use their spawn egg), name, identifier line, then rows with
  labels in a faint column and values aligned.
- Independent settings rows: icon, ID, health (hearts / bar / number),
  growth (bar / number), other details. No card/simple style switch.
- Ease the card's size and position over about 0.16 s when the target
  changes; fade the content in.
- If icon rendering in the HUD pass is not practical, draw the card without
  the icon and report.

### L-09 Colored line batches in the world overlay
Status: done.
- `drawLines` in `overlay/WorldOverlay.cpp` uses two fixed colors. Let callers
  pass a list of (lines, color) groups so chunk borders and hitboxes can use
  several colors in one frame. No behavior change for existing callers.

### L-10 Chunk borders like Java F3+G
Status: done. Colors confirmed against a Java screenshot 2026-09-24: purple
current corners, yellow 2-block grid alternating with dark-cyan verticals,
dark-cyan middle horizontals, blue 16-block sections, red neighbor corners.
Exact shades still need a side-by-side comparison. Reconfirmed target after the
2026-09-24 in-game trial: Java F3+G-like information/visual structure, not
merely a 16x16 chunk outline.
- Reference behavior (confirm against a Java screenshot from the maintainer):
  current chunk walls have yellow lines every 2 blocks, vertical and
  horizontal; section boundaries every 16 blocks and the current chunk's
  corners are blue; the corners of the 8 neighboring chunks are red vertical
  lines.
- Extend `overlay/ChunkBorders.h` to return colored groups; keep the cache and
  the bounds checks; add tests for line counts and positions.
- When a detached camera (Freelook/FreeCamera) is active, center on the view
  chunk instead of the player chunk (requested 2026-09-24).

### L-11 Hitboxes like Java F3+B
Status: done. The red eye box is a fixed-size marker pending a screenshot
comparison; the dragon stays out (L-30). Eye box and look line draw for mobs
only, matching Java (no red frames on items).
- Keep the white bounding box; add a red rectangle at eye height and a blue
  line from the eyes along the view direction (2 blocks long).
- The Ender Dragon is a special case and must not be approximated from model
  geometry. Bedrock multipart/damage-box exposure is tracked separately in
  L-30; ordinary L-11 work must not wait on it.
- Find the eye position and view vector in the SDK (`Actor`,
  `ActorHeadRotationComponent`, `getViewVector`-like functions). If not found,
  stop and hand back.
- Pure geometry for the eye rectangle and look line in `overlay/Hitboxes.h`
  with tests.

### L-13 More shape types
Status: done. First presets: box, cone, frustum, pyramid, ellipsoid, dome
(plus axis X/Z for every round shape). Diamond/octagon sections arrive with
their own presets; no octagon taper.

Prior art (behavior only, never code):
- MiniHUD (current fork) ships: box, centered box, circle, square, rhombus,
  block line, blocky sphere, spawn/despawn spheres (several variants),
  ellipsoid spawn, cone, pyramid, diamond pyramid, octagon pyramid. Cones and
  pyramids share one "tapered" model: bottom radius, top radius, height,
  direction. Circles have a main axis. Rendering can pick full block / inner
  edge / outer edge for which cells count as the boundary.
- Building generators and WorldEdit-style tools commonly offer sphere and
  ellipsoid (three radii), cylinder (two radii), pyramid, hollow variants.

Model (decided): a shape is a cross-section × a profile × an axis.
- Cross-section: circle, square, diamond (rhombus), octagon.
- Profile: constant (circle/cylinder, square/box), linear taper with
  bottom and top size (cone, frustum, pyramid), round (sphere, ellipsoid,
  dome = half).
- Axis: Y (default), X, Z.
The type list still shows familiar names (Cone, Pyramid, Box, Ellipsoid,
Dome...) as presets over these families, so users find "cone" by name while
the generator stays small. Alternatives: a dedicated type per shape (MiniHUD
style), or not supporting cones at all.

Range presets (numbers from minecraft.wiki, Bedrock):
- Mob spawning: 24–44 blocks spherical at simulation distance 4; 24–128 at 6+,
  limited horizontally by simulation distance.
- Beacon: box, 20/30/40/50 blocks by level toward south/east and one more
  toward north/west (asymmetric in Bedrock), full height.
- Conduit: sphere, 32/48/64/80/96 by frame size (16/21/28/35/42 blocks);
  attacks hostile mobs within 8.
- Despawn distances: not yet confirmed from a reliable Bedrock source.

Implementation notes for later: add types through the registry in
`ui/ShapeEditor.h`; generate by columns like `roundColumns` (never the full
volume); extend `ShapeDocument` load/save; test against a brute-force volume
surface for small sizes.

### L-38 Zoom up to 50x with a proportional, smooth wheel
Kind: Ready. Requested by a user 2026-09-26. DESIGN "Camera".
Status: done (verified in game 2026-09-26, DLL 766d6fd6). The Wheel step
setting was removed; old settings files still load. Follow-up: L-45.
- Raise the magnification range from 1x-10x to 1x-50x for both the initial
  setting (Options.h, Settings normalize) and the wheel (`ZoomState`).
- A wheel notch multiplies/divides the target magnification by about 1.15
  (replace the fixed additive step; migrate or drop the `wheelStep` setting
  and keep old settings files loading).
- The shown magnification eases toward the target (frame-rate independent,
  roughly 0.1-0.15 s); FOV and turn sensitivity use the shown value. Releasing
  Zoom resets as today.
- Keep the math in `ZoomState.h` with tests: range clamps, notch symmetry,
  easing convergence, no overshoot, reset.
- In game: 50x is reachable in a reasonable number of notches, zooming feels
  smooth, aiming stays controllable at 50x.

### L-43 Permanent Sprint
Kind: Ready. Notion idea (Masa-style QoL), promoted 2026-09-26.
Status: done (verified in game 2026-09-26, DLL 766d6fd6). The maintainer did not
want menus to cancel it: since 73d8afe it pauses while a menu is open and
only death, dimension change, world exit or disabling Lamium end it
(re-checked 2026-09-26: resumes after closing the inventory). Shares
Permanent Sneak's raw-input hook; unbound by default.
Mirror Permanent Sneak: add `SprintDown` to a transient copy of the raw move
input in the same `extractRawHIDInput` hook, with the same eligibility and
cancellation (screens, settings, death, sleeping, riding, dimension change).
Vanilla still decides whether sprinting is possible (hunger, blindness,
moving forward, sneaking). Add the setting row, action and translations.
Check in game that sprint starts when walking forward, stops when vanilla
would, and that turning it off never leaves sprint stuck.

### L-55 Target armor icons
Kind: Ready. Requested by the maintainer 2026-09-27.
Status: done (verified in game 2026-09-27, DLL 944E886D). `targetArmor` is
0 icons (new default), 1 bar, 2 number, next to Health. Icons use the vanilla armor-bar
sprites (`textures/ui/armor_full`, `armor_half`, `armor_empty`, 9x9,
overlapping by one like hearts); ten icons cover armor points 0-20. The
armor detail now has its own `DetailKind::Armor`, so it no longer depends on
"Other details" and it follows the meter choice. The row stays hidden at
zero armor. Armor toughness is not exposed by the Bedrock client
(`Mob::getArmorValue` carries the points only). In game: an armored mob
matches its armor value; icons/bar/number modes; no row at zero. Verify the
two new sprite paths load (icons appear at all).

---

## Design

### L-95 Fake Offhand beyond block placement
Kind: Research (strong model). Resumed by the maintainer 2026-10-06.
Closed 2026-10-07 by the maintainer after the property-based eligibility,
firework fixes and the fireworks swap switch passed in game (`d93f04d`).
Deferred, not done: entity-directed use and passive holding effects (still
research); timed secondary use is out of scope (food, bows, potions,
tridents). Unchecked: eggs, non-mouse activation, Hand Restock/Auto Use
overlap, rejoin and servers (Pre-release checks). Block-use items the
classifier misses are fixed as reported.
Status: broad item-use scope chosen 2026-10-06; vanilla use baseline and
existing-placement regression checked on `c673fad`. First instant-use
candidate `621a7b8` failed in every tested empty-hand/sword/pickaxe combination;
placement remained usable. Revised known-item eligibility on `c4d6258`
reached water placement/collection but repeated them during a hold; snowballs
still did nothing (build calls never reached air use). The queued activation
delivered one ordinary use-button pair with a scoped selection. On
`c522b22`, the maintainer confirmed snowballs, water placement/collection,
block placement and chest interaction. Single-use-per-hold is a temporary
adapter limitation, not the desired behavior: held activation must repeat
according to ordinary item-use cadence and cooldowns. Native selected-bucket
traces show repeated replacement transitions roughly 200-250 ms apart.
The maintainer also confirmed native held snowballs in air and on a block;
traces place the repeat air-use calls inside build processing at roughly
200-250 ms intervals. The adapter now retains the ordinary native hold and
borrows/restores selection within each build call, leaving repeat timing to
vanilla. On `e7ce3f1`, the maintainer confirmed empty-primary snowball/bucket
repetition and release, manual-selection cancellation, placement and chest
interaction. Buckets also worked with swords/pickaxes, but snowballs failed
with those primaries. `2f7878c` traces establish that eligibility, selection
and edge replay succeed, but the physical primary use runs first and borrowed
snowball use never follows. The adapter now intercepts the native use-button
handler before that first primary attempt, invokes the captured handler list
once under borrowed selection and suppresses the duplicate queued/physical
press. On `6e41014` (2026-10-07), the maintainer confirmed empty/sword/pickaxe
snowballs in air/on blocks, held repetition/release, one throw per short
click, sword/bucket placement/collection and the placement/chest regression.
The tested instant subset is usable; the sword/snowball and bucket smoke
also passed on trace-disabled `58d121d`. Its primary gate excluded totems
and all other items. The revision adds known passive totems and basic
materials; runtime checks are pending. Applicable primary use retains priority;
the maintainer clarified examples as dirt, rotten flesh and other tools
(2026-10-07): pass when their primary role cannot be performed, not merely
when the item has no role at all. The next revision permits dirt and vanilla
axe/shovel/hoe tags only on a NoHit ray, and vanilla food only at confirmed
full hunger when its food component does not allow always eating. Creative,
missing/invalid state and unknown items keep vanilla priority. These new
cases await runtime checks. Block-target placement/tool failures still need
research. Eggs, other
bindings, overlap, broader cancellation, rejoin and servers remain unchecked.
Manual selection, target changes or lost eligibility cancel held ownership.
2026-10-07: the per-item allowlists were replaced by property-based
classification of both hands (FAKE-OFFHAND.md table), adding every block
item and material in air, plain materials on ordinary blocks, any projectile
or bucket (not milk) as the secondary item, and fireworks while gliding. Food
no longer passes on block targets (plantable foods). Awaiting runtime checks.
Timed secondary use (food, bows, potions, tridents) is out of scope by
maintainer decision (2026-10-07). Entities and passive effects remain open.
Technical findings, scope and the runtime research plan:
[FAKE-OFFHAND.md](FAKE-OFFHAND.md).

Goal: support as much secondary-hand use as the client-only platform permits,
including instant use, timed use and entity interactions, with
ordinary vanilla item-use semantics and restoration of the prior selection.
This remains temporary main-hand selection; it does not move an item into
the real offhand. Keep the existing switch, target slot and activation binding.

Scope (maintainer direction, 2026-10-06):
- Blocks, food/drinks, throwables, buckets, tools, fireworks, bows,
  crossbows, tridents and entity-directed uses such as feeding or shearing.
- Research held-item effects too: blocking, maps, ammunition preference,
  totems and equipment/enchantment effects. A target hotbar slot is not a
  real equipped hand; document unsupported effects individually rather than
  silently treating ordinary item use as complete support.
- Target behavior: normal target interaction and applicable selected-hand
  use take priority; the secondary item is used when the first hand passes.
  A failed or unavailable path must not cause a second mutation. Existing
  L-49 placement behavior stays during the diagnostic step; changes to its
  priority are implemented and checked as part of the extension.
- Restore after instant actions and after timed use ends. An explicit manual
  selection takes ownership and is never overwritten by delayed restoration.
  A visible selected-slot change during timed use is a feasibility question,
  not permission to permanently leave the secondary item selected.

Open research: whether vanilla can retain timed use of the secondary slot
while the primary slot is selected, including ordinary attacks; whether
selection/equipment reporting is accepted by servers; which passive effects
can be supported without a server mod or invented authoritative state.

Steps:
1. Trace ordinary instant use, timed food use, charged use and entity use
   with Fake Offhand off: start, progress, completion/release, selected and
   use slots, and reported selection. `offhand_trace` enables only this
   research, with per-stage budgets. Trace builds require a request before
   deployment.
2. Implement the smallest adapter supported by those observations. Keep
   block placement's per-call restoration; do not extend selection across
   frames for every category. Put ownership/restoration decisions in pure
   logic with tests; cancel on menu/focus/world/dimension changes and disable.
3. Check actual effects and inventory state locally, then on a server;
   a callback result or submitted transaction is not completion evidence.

### L-97 Tool Switch and Weapon Switch: fetch into a fixed hotbar slot
Kind: Ready (decided with the maintainer 2026-10-06). Split from L-94.
Status: done 2026-10-06. Checked in game on `a15930f`, with the follow-ups
on `c7bb827`: a warning on the colliding settings rows (DESIGN "Colliding
settings"), and Weapon Switch fetches a weapon stronger than every hotbar
item even when the hotbar holds a weaker one (a diamond shovel had kept a
diamond sword in the inventory). A server check is in Pre-release checks.
Today "Fetch from inventory" (L-69, EQUIPMENT.md) swaps the chosen inventory
item with the selected slot, so it overwrites whatever the player held there.
Decided:
- Each feature gets "Fetch into: Selected slot / 1-9" under its "Fetch from
  inventory" switch (separate for tools and weapons). Default: Selected slot
  (the old behavior).
- With a fixed slot, the fetched item swaps with that slot's item (which goes
  to the inventory slot the tool or weapon came from), and the fixed slot is
  selected and stays selected, like a hotbar pick (no switch back). A better
  item in the fixed slot would have been picked from the hotbar already.
- A fixed slot that is Fake Offhand's target slot (while Fake Offhand is on)
  falls back to the selected slot, so Fake Offhand's blocks stay; the help
  text says so.
- Weapon Switch reports the new selection to the server at once, as its
  hotbar pick does, so the same hit counts.
- The rows "Fetch into" and Fake Offhand's "Target slot" show a warning
  while they collide (maintainer 2026-10-06).
Keep the existing 150 ms pacing, the server confirmation and the "never one
about to break" rule unchanged.

### L-94 Swap the held item with the offhand, including items the offhand cannot hold
Kind: Ready (decided with the maintainer 2026-10-06). Taken up 2026-10-05
after a public request.
Status: done 2026-10-06. Behavior checked in game on `856d79c`, the switch +
command settings on `ed288b6`; the names were then changed to "Offhand
swap" (heading) and "Swap now" (command) so the two rows differ. A server
check is in Pre-release checks.
What it is for: one action that puts the selected item in the "second hand"
and brings the second hand's item back, also for items Bedrock does not let
the real offhand hold. Fake Offhand (L-49) only borrows a hotbar slot while a
block is placed; it never touches the real offhand slot.
Decided:
- Built like Inventory sorting (SETTINGS-KEYMAP rule 1, maintainer
  2026-10-06): a feature heading "Offhand swap" in Inventory beside Fake
  Offhand, with a saved switch (default on) toggled by an unbound key, and
  the command "Swap now" as its first child, default key F (an
  exception to "new actions start unbound": Java's key, and Bedrock has no
  swap key; confirmed in game 2026-10-05).
- An item the real offhand accepts swaps with the real offhand. Any other
  item swaps with Fake Offhand's target slot while Fake Offhand is on; with
  Fake Offhand off it stays put.
- An empty hand takes the real offhand's item back, else the target slot's.
- Holding the target slot itself, or nothing to swap anywhere: nothing
  happens, silently. No toast in any case.
- Screenless `InventoryMove::movePair`, like Tool Switch's fetch; only in
  gameplay (no screen open), not in creative or spectator (other inventory
  requests there; stays vanilla).
- "Accepts" is the item's own offhand flag (`Item::mAllowOffhand == Yes`);
  the inventory screen's validation is not exported. Items sent to the Fake
  Offhand slot are logged once per kind with their flag, to check that no
  offhand item (arrows, maps, fireworks, ...) is missed.
Hand Restock and Tool Protection need no change: Hand Restock ignores moves
made by `movePair` (`game::moving()`), and the swap is not a consumption.
Related, not decided: hotbar slot ownership. The Fake Offhand target slot is
an ordinary slot, and other automation (Tool Switch, Weapon Switch, Hand
Restock from the hotbar) may use it. Ideas: keep it empty and out of other
automation's reach; move a stray item into the main inventory after the
server update when there is room, and do nothing when there is not. Settle the
behavior here first and share a helper only when a second feature needs the
same rule (L-97 is the likely one); no reservation manager up front.

### L-100 Contribution guide, issue forms and PR template
Kind: Design. Done 2026-10-06. Policy agreed with the maintainer 2026-10-05
(maintainer's notes), then written into the repository one step at a time,
each reviewed by the maintainer before the next.
Decided (summary; the files themselves become authoritative once written):
- Tone: hobby project, no promise of replies or merges; the maintainer tests
  in game. English only. Keep issues and PRs short. AI-assisted code is fine
  without disclosure; the submitter is responsible. Contributions under
  LGPL-3.0-only; no DCO or CLA.
- PRs: translation/doc fixes and small bug fixes welcome without asking;
  large features may come as a PR but an issue first is recommended, and the
  maintainer may rework or decline them; dependency and build-setting changes
  are not accepted unless discussed. CI runs on PRs.
- PR template, three items: what changed (1-2 lines); how it was checked
  (tests / in game with version / not checked); provenance statement (written
  by the submitter, can be offered under LGPL-3.0-only, no code copied from
  the reference-only mods in PROVENANCE.md, any relationship to other mods
  stated).
- Issue forms: Bug, Feature, Translation, Other, plus blank issues allowed.
  Required fields only where they apply to everyone (Bug: what happened vs
  expected, steps, Minecraft/LeviLamina/Lamium versions; Feature: what is
  wanted; Translation: language, where, current wording; Other: body only).
  Optional log field says to check `lamium.log` before pasting. Labels: only
  GitHub's `bug` and `enhancement`.
- Accepted requests stay open, get a comment when they enter BACKLOG and
  close with the release; declined ones close as not planned with a reason.
  Mixed issues are split by the maintainer, not closed.
- Conduct: one line in CONTRIBUTING, no separate file. Security: enable
  GitHub private vulnerability reporting (maintainer's action) and mention it
  in one line; no SECURITY.md.
Steps:
1. Done 2026-10-06: `CONTRIBUTING.md` at the root; README, AGENTS.md and
   TRANSLATING.md point to it.
2. Done 2026-10-06: `.github/pull_request_template.md`.
3. Done 2026-10-06: `.github/ISSUE_TEMPLATE/` with `bug_report.yml`,
   `feature_request.yml`, `translation.yml`, `other.yml` and `config.yml`
   (blank issues turned off 2026-10-06: "Question or other" covers them). Check the chooser on GitHub once they are on main.
4. Done 2026-10-06: the maintainer enabled private vulnerability reporting.
Decided 2026-10-06: non-English writers may use a machine translation with
the original below it; the provenance statement allows code from a named
source (recorded per PROVENANCE.md); contributors write English strings and
may copy them into the other locales for the maintainer to translate.
Showing the Lamium version in the settings screen moved to L-101.

### L-67 Switch to the best weapon when attacking
Kind: Design, then Research. Chosen by the maintainer 2026-09-28 from the
prior-art comparison (behavior reference: Stipuleroo's combat Auto Tool,
PROVENANCE.md group 3).
Status: done 2026-10-02; implemented and checked in a local world on
`7b702da` (servers not checked). A switching hit first did no damage: the
client reports its slot from its tick, after the attack transaction, so a
switch now sends the equipment packet at once. Vanilla's per-enchantment
bonus calls left Smite out; the bonus is now computed from the stack's
enchantment list.
Tool Switch picks a hotbar tool for the block being mined. This does the same
for attacking entities: select the hotbar weapon that deals the most damage
to the target, through the same `selectSlot` path.
Decided (2026-10-02):
- Its own switch "Weapon Switch" in the Inventory section, default off, with
  its own toggle action, and the L-69 child option "fetch from inventory"
  (default off).
- Ranking is the damage against this target: the item's attack damage plus
  Sharpness, plus Smite or Bane of Arthropods only when the target is undead
  or an arthropod. Fire Aspect and Knockback are ignored. On a tie the held
  item stays; otherwise a sword beats an equal axe (an axe loses 2
  durability per hit).
- No switch back after the attack, like Tool Switch; a weapon fetched by L-69
  stays in the selected slot.
- Targets: every living entity (mobs and players). Non-living entities
  (item frames, armor stands, boats, minecarts, End crystals) never switch.
- Mace, trident and spear rank by their melee attack damage only (no mace
  fall bonus or spear charge), so a trident can win over a sword.
- Durability: like Tool Switch, the inventory fetch skips weapons about to
  break; hotbar weapons rank as usual and Tool Protection's held swap handles
  a weapon wearing down.
- Never in Creative or Spectator, like Tool Switch.
Research after that: Lamium already hooks `GameMode::attack` /
`SurvivalMode::attack` (`CameraInteraction.cpp`). Check whether selecting a
slot there changes the weapon used for that hit or only the next one, and
how that looks on a server. When it exists, it gets the L-69 child option
(fetch the weapon from the main inventory; see BACKLOG-DONE.md).

### L-83 Settings screen consistency review
Kind: Design (maintainer + strong model). Noted 2026-10-01; started the same day.
Status: done 2026-10-01; both halves checked in game (`964167c`, `dae926d`).
As features grew the settings screen lost some consistency. Known cases:
- Keys that open the sidebar's tool screens: Hotkeys, Shapes and HUD
  layout keys are under General ("settings" feature), the Waypoints screen
  key under Map ("waypoints" feature).
- Color choosers: the waypoint add prompt shows 12 swatches (as in
  [demos/waypoints.html](demos/waypoints.html)), the Waypoints screen a
  ◀ swatch ▶ stepper, Shapes a ◀ name ▶ stepper over four colors.
- Durability features are spread: the item durability readout under
  Inventory, the Durability HUD under HUD & overlays.
- The world map's waypoint side panel nearly duplicates the Waypoints
  screen (noted 2026-10-01 after checking `e6781e5`): the screen still
  has a translucent view of the world and works with the world map off,
  so it stays for now; reconsider both together.
Started 2026-10-01 at the maintainer's request. Survey of the fully
expanded tree (7 categories, 41 features) found, besides the cases above:
- "Info & overlays" is the largest category (8 features) and mixes HUD
  elements (Info HUD, Target info, Durability HUD, Debug View) with world
  overlays (chunk borders, hitboxes, light overlay, shapes); the
  Automation status HUD element sits under General.
- The Shapes screen opener is under General while the shape drawing switch
  is under Info & overlays; the Waypoints and World map openers sit with
  their features.
- Heading-only rows (Map text, Cave view, Block restrictions) and session
  features (Zoom, Freelook, FreeCamera, permanent sneak/sprint) follow the
  documented rules; action labels' "Lamium: " prefix is stripped in the
  UI, so naming there is consistent.
Proposed principles (to agree before a mockup): a screen opener sits with
the feature whose screen it opens, screens that belong to no feature stay
under General; categories by what a feature does (HUD elements together,
world overlays together); one swatch-row color chooser everywhere; the
Waypoints screen and the world map side panel share one editor layout.
Settings file keys and action ids stay; only presentation and grouping
move.
The maintainer agreed the principles and asked for keymap rules too;
option X was chosen 2026-10-01 (every saved switch gets a toggle action,
unbound by default). The rules are in SETTINGS-KEYMAP.md. Applying them:
nine new toggle actions (container previews, durability, sorting, hide
effects, durability HUD, automation status, radar, waypoints, world map);
"Add a waypoint here" and "Open the world map" (M) move from their
parents' key cells to child rows, the parents' keys toggling the switch;
"Open the Shapes screen" moves under Shape drawing. Mockup:
[demos/settings-review.html](demos/settings-review.html). Agreed 2026-10-01:
automation status moves to the HUD group, "Durability" becomes "Durability
numbers", the nine toggle actions, one swatch-row color chooser and a
shared waypoint editor. Open: the category split; the maintainer found a
4-feature "World display" too thin and apart from "Camera & visuals". Its
section 1b compares the current tree with three options. The maintainer
prefers keeping the current categories (Debug View, chunk borders and
hitboxes recreate Java's F3 tools and are bound alike, so they belong on
one page) and only fixing the order; proposed: Camera & visuals, Info &
overlays, Map, Inventory, Actions, General (option 4 in the mockup), Map
confirmed there 2026-10-01.
First half built 2026-10-01 (not checked in game): section order; the
nine toggle actions (ids appended: previews, durability, sorting,
hideeffects, durabilityhud, automationstatus, radar, waypoints, worldmap);
waypoints and world map parents toggle their switches with Add and Open as
child rows; Open the Shapes screen under Shape drawing; automation status
under Info & overlays; "Durability numbers". Checked in game on `964167c`
(all points passed).
Second half built 2026-10-01 (not checked in game): the Waypoints screen's
and the Shapes editor's color rows show every color as swatches (12 and 4)
in the value column, a click picks one and the arrow keys still step
(`ShapesLayout::swatchAt`); the add prompt and the map's side panel
already used swatches. The waypoint editors already list the same fields
in the same order (X, Y, Z, Move here, Show, Color; the death point's
Make a waypoint and Delete), so principle D needed no further change; the
screen alone takes typed numbers, the panel alone has Open in screen.
Checked in game on `dae926d` (all points passed).


### L-42 Hide visual effects without changing game state
Kind: Design done (2026-09-28); Research next, one render entry at a time.
Status: scope reaffirmed by the maintainer 2026-09-30. Rain/snow and particles
are implemented; the maintainer confirmed independent hiding/restoration and
rain sound on `7e72244`, followed by a positive master/rain-splash playtest on
`41b1ff6` (no individual case results).
Boss bar drawing is now implemented from the `43c4211` runtime trace, with a
saved child switch and unbound key; hiding and switch behavior passed on
`d20fdf8` (optional key coverage was not reported separately).
Nausea color hiding is implemented from the green-overlay route confirmed on
`d3f0293`; hiding/restoration with child/master/key and preserved effect/icon/
vanilla preference passed on normal build `b239eb9`. The other five
effects (carved pumpkin view, spyglass frame, underwater fog, lava fog, powder
snow view) are implemented 2026-09-30 from static evidence. On `87f11cd` the
three immersion switches worked; the pumpkin and spyglass frames did not hide
(no mesh route logged). Three trace rounds (first-seen keys, UI context,
per-frame counts) found no hooked entry that draws either frame. Decided
2026-09-30 (maintainer): both frames are parked as L-79 and their switches
and keys removed (never released); the other seven children stay. See
VISUAL-EFFECTS.md "Frame and immersion step". Technical evidence and the
opt-in read-only trace:
[VISUAL-EFFECTS.md](VISUAL-EFFECTS.md).
Requested 2026-09-30: implement all seven remaining effects. Static inspection
has not established their per-effect draw contracts; full-screen renderer
classes and the internal by-value mesh texture list are opaque in SDK 26.51.5.
The first trace confirmed boss UI paths, immersion fog types and the frozen
mesh. It did not establish pumpkin/spyglass or nausea color drawing. The next
trace adds screen blits, vignette/render-stage context, UI path tails and
settled fog samples. The first test used Fancy graphics and a custom global
HUD pack. The expanded trace was collected on `d20fdf8`; it confirms settled
distance fog but still has no pumpkin/spyglass/nausea color draw candidate.
Screen blit/vignette hooks installed, without candidate records. Inspect a
different documented mesh/render path before requesting another observation.
The next trace now includes the complete GSL multi-texture mesh span and
reference-based tessellator interception, sharing existing budgets, plus
one-time entry-reach records. Nausea was tested as warp, not green color;
the next observation must set vanilla Screen Distortion to zero.
The `d3f0293` test did display the green effect and recorded a distinct stage-1
ui_texture_and_color_blur_additive / textures/misc/nausea draw. The new child
filters that exact pair only during the owning gameplay-screen render; effect,
status icon and vanilla distortion preference are unchanged. Pumpkin/spyglass
remain unidentified after the multi-texture/interception observation.
Keep this item open; remaining trace hooks do not implement hide switches.
Next: look for a documented, typed backend contract for the frame and
immersion view effects instead of repeating the same generic hooks; write the
new native hypothesis down before asking for another runtime probe. If no safe
callable contract exists, record the missing API and ship no speculative
filter. A generic on_screen_effect filter would also hide unrelated effects.
Do not ship immersion fog alone under a switch that promises fog and view
overlays. `bin/Lamium-effects-trace` is the older `d3f0293` trace build without
nausea hiding; never deploy it as the latest build.
One group of render-only toggles: boss bars, rain/snow, all particles,
carved-pumpkin overlay, spyglass overlay (zoom kept) and the nausea green
vignette (vanilla Screen Distortion already removes the warp; reconfirmed
2026-09-30: Lamium adds no warp switch, and the nausea help names Settings >
Accessibility > Screen distortion at 0 as what shows the green color). Weather,
effects, boss state and equipment are never changed. Research each effect in
its own backend category rather than looking for one universal hook: HUD
overlays, weather, particles, camera/media overlays and post-processing may
have separate paths. Version-sensitive renderer paths are capability-gated and
leave vanilla behavior unchanged when the expected contract is unavailable.
Ship effects one by one. Status-effect-only particle filtering stays an idea
until its source can be identified.
Added 2026-09-28 (maintainer, from the prior-art comparison): fog and view
overlays while the camera is in water, lava or powder snow. Night Vision
stays a separate feature (brightness only). These are camera/fog render
paths, not HUD overlays; research them as their own backend. The per-medium
switch choice was settled in the 2026-09-30 discussion below.
Decided 2026-09-28: a keyless group heading "Hide effects" under Camera &
view (beside Hide offhand, which is the same kind of feature) with one switch
per effect, each bindable without a default key. Particles start as a
single hide-all switch; per-kind choices come only once their sources are
identified.
Decided 2026-09-30: separate switches for boss bars, rain/snow, all particles,
the carved-pumpkin overlay, the spyglass frame (keep magnification), the nausea
color effect, and the immersion fog/view effects for water, lava and powder
snow individually. This resolves the per-medium versus combined fog choice.
All switches default off, with no default key bindings. The group heading has
no switch or key. Hide drawing only; sound and gameplay state remain vanilla.
The maintainer may refine the individual effects after trying them. Research
must establish which fog and view overlays can be safely suppressed for each
medium; do not promise a rendering path before runtime validation.

Revised 2026-09-30 after the first playtest: add a saved master Hide effects
switch (on by default, no key), retaining all child selections when off.
Children still default off; editing their switches/keys while the master is
off changes selections only, with a visible paused indication. Rain and snow
also hides rain-derived splash drawing; Particles remains hide-all, so either
child hides rain splashes. Other water splashes follow only Particles. Keep
rain sounds and particle emission/ticking unchanged. Identify the rain source
from SDK types and the game's effect mapping rather than treating every
water splash as rain. This supersedes the original keyless-heading-only UI.
Done 2026-09-30, released in 0.1.4 (`v0.1.4`) with seven effects. The carved
pumpkin and spyglass frames continue as L-79 (Research).

### L-28 Third-person underground camera
Status: closed 2026-09-30 as not worth researching. FreeCamera locks first
person (a detached third-person view makes no sense when the body does not
follow), so the judder it described cannot occur.

### L-69 Tool Switch and weapon switch from the main inventory
Kind: Design (small), then Ready. Chosen by the maintainer 2026-09-30 after
L-66 proved a screenless same-slot move.
Status: Tool Switch part done 2026-09-30; implemented and playtested (local world,
light BDS pass). The first playtest (04b594d)
fetched on a new press; the maintainer wants the right tool every time a held
attack moves to another block, so a fetch there pauses breaking until 150 ms
after the last break (L-66 ordering), moves the tool and restarts. The weapon
switch part waits for L-67.
Decided (2026-09-30):
- A child option of Tool Switch (and of L-67's weapon switch once it exists),
  default off: it rearranges the inventory more than any other feature.
- Only when no suitable tool/weapon is in the hotbar, move the best one from
  the main inventory into the selected slot; the item it replaces goes to the
  source slot. The selection never changes. Same move, ordering and failure
  rules as L-66.
- The moved tool stays in the selected slot afterwards (decided 2026-09-30).

### L-70 Put on an elytra automatically when gliding starts
Kind: Design, then Research. Chosen by the maintainer 2026-09-30.
Status: done 2026-09-30; implemented and playtested after two redesigns (below;
local world, light BDS pass). Armor-slot moves work: the armor setter records
no action and the move adds it.
Idea: put on an elytra from the main inventory for a flight and a chestplate
again after landing.
Decided (2026-09-30): its own switch under Actions, default off, Experimental.
Revised after the playtest (0ca7af5/5728561): any mid-air jump swapped on
ordinary sprint jumps, and vanilla never calls tryStartGliding without a worn
elytra. Settled with the maintainer 2026-09-30:
- Purpose: a general client convenience (survival first, not PvP-specific).
  A fall-rescue elytra is out of scope and goes back to the notes.
- Triggers: a press key "Put on / take off the elytra" (unbound; it also takes
  off an elytra this feature put on) and any jump, from the ground included,
  while holding firework rockets in the main hand.
- No automatic glide: tryStartGliding never succeeded right after a swap. A
  second jump glides as in vanilla. Starting a glide from the mod is L-71.
- Child "Jump with fireworks" (default on) turns the firework trigger off,
  leaving the key only.
- After a glide or a firework jump, a chestplate goes back on a set time after
  the first landing (child option, 0-10 s in 0.5 s steps, default 3 s). Later
  hops do not restart the delay (sprint jumping kept the grounded-time count
  from ever finishing); only a new glide does. A key press on the ground waits
  for the key.
- What goes back on: the chest item the elytra replaced, else the chestplate
  with the highest protection in the inventory (armor, toughness, enchantment
  levels, durability), whose slot takes the elytra. An elytra worn by hand is
  followed the same way once it glides. With no chestplate the elytra stays on.
  (Maintainer, 2026-09-30, after using the build.)
Research: armor-slot moves on a server, and whether gliding can start in the
same jump as the swap or needs a second press.

### L-52 Settings and keymap information architecture review
Kind: Design.
Status: done. Layout B was selected and confirmed in game on 2026-09-27
(`2e3dbab`, DLL
`6B44039B51F71893A904DA0CAE71ADB9C05CC489FEE4BE524F1D497743DC3163`).
The Java-style default keys F3 / F3+B / F3+G for Debug View / Hitboxes / Chunk
Borders were confirmed without conflict by the maintainer on 2026-09-28 on
the installed normal build from commit `51a2ad0`, DLL
`CABB272FD84BA955356016CEEE9CF6370D154AFCFC8115C467628BE1263093FE`;
NightVision is unbound by default. Existing saved bindings and action ids are
preserved.

The audit is in [SETTINGS-KEYMAP.md](SETTINGS-KEYMAP.md). Sort moved from the
Inventory sorting parent's key cell to a child command row. Container previews,
Durability and Automation status remain independent keyless parent switches.
Auto Attack/Use keep their unbound mode-cycle and held-only keys. Durability
stays under Inventory; L-61's Durability HUD gets its own HUD & overlays row.
The restriction actions follow the later L-15 redesign. The comparison demo
keeps all three preliminary layouts, with B marked as selected.

### L-27 FreeCamera keeps its position through menus
Kind: Design (small). Promoted from Later 2026-09-26 after user feedback.
Status: done as part of L-47 (verified in game 2026-09-26). Movement input
counts as zero while a screen owns input.
Opening the inventory or another menu, the pause screen, or switching windows
currently ends FreeCamera and discards the flown position. Proposed direction
(DESIGN "Camera"): keep the detached pose through these and resume on return;
death, dimension change and leaving the world still end it. Open: always, or
an option. The pose must not follow input while a screen owns input, and
inventory interaction while detached stays a separate question (L-25).

### L-46 Reset settings to defaults
Kind: Ready (decided 2026-09-26). Status: done. First build verified in game (General
had "Reset all"); the maintainer then moved "Reset all" to All and asked for a
reset per category. Since the follow-up commit: All shows "Reset all", each
category shows "Reset category" (its features' settings and HUD placement, not
key bindings) and Hotkeys shows "Reset all keys", all on the column-heading
line; the first press arms the button (red, "Press again"), the second
applies, any other click disarms it (verified in game 2026-09-26, DLL c5166d6b).
Shapes are untouched.
The HUD layout editor keeps its own per-element and all-element resets.
Note from the check: after "Reset all", Freelook and FreeCamera are off again
(their defaults), so their keys do nothing until the features are switched on.
Raised by the maintainer 2026-09-26. The demo
(docs/demos/settings-reset.html) showed per-row and per-feature resets; the
maintainer judged them excessive (vanilla Minecraft has no per-setting reset)
and narrowed the scope to: reset everything, and reset key bindings only
(HUD elements already have Reset in the layout editor). A per-category reset
is optional and not planned now. Open: whether "reset everything" includes key
bindings and the HUD layout (proposed: yes, Shapes excluded), and where the two
actions live (proposed: General, and the Hotkeys view header, each with an
in-screen confirmation).
The text below is the original question.
Only key bindings (Reset per action) and HUD elements (Reset in the layout
editor) can go back to their defaults; ordinary settings cannot, short of
deleting the settings file. To decide: per-row reset (for example a small
reset control shown only when a row differs from its default), per-feature
reset, a "reset all settings" action with confirmation, and whether the last
two include key bindings, HUD layout and Shapes (Shapes are per-world data and
should probably be excluded).

### L-47 Enable momentary features by their key alone
Kind: Design. Raised by the maintainer 2026-09-26.
Status: done (verified in game 2026-09-26, DLL 1db48ae3) with L-27; behavior
as in DESIGN "Camera". The old Zoom/Freelook/FreeCamera enable settings are
ignored on load. The first build left a held Freelook restarting after key
release; fixed in eb2aaac.
Night Vision and other persistent features have one switch that their key also
toggles. Zoom, Freelook and FreeCamera instead have an enable switch plus a key
that holds or toggles a session, so a bound key does nothing while the switch
is off (noticed after "Reset all" turned Freelook and FreeCamera off).
Proposed: drop the enable switch for these momentary features; a bound key
means the feature is available and "unbound" disables it. The state column
shows whether the session is running (or stays empty). Experimental features
ship unbound. Migration keeps existing bindings and ignores the old switch.
Maintainer direction (2026-09-26): go further for consistency and drop the
"momentary" category: every feature is a persistent on/off switch that its key
toggles, as Tweakeroo does for most tweaks. This requires FreeCamera to keep
its position through menus (inventory, settings, window switch), so L-27 is
folded into this item. Decided 2026-09-26: Zoom and Freelook keep an
"Activation: Hold / Toggle" option (Hold lights the switch only while the key
is held); FreeCamera's on state is not saved across restarts; Zoom keeps its
default key C (with Zoom unbound, C did not copy coordinates either, so the
vanilla binding may no longer exist). Still open: what the switch does when
clicked in the settings screen, and whether Zoom/Freelook resume after a menu.

### L-39 Freelook starts in third person
Kind: Ready. Requested by a user 2026-09-26; decided 2026-09-27.
Status: done (verified in game 2026-09-27, commit b585411, DLL 7af6b60a).
On activation, save the current first-person, rear-third-person or
front-third-person perspective and switch to rear third person. F5 remains
available while Freelook is active, including the front view. On release or
cancellation, restore the perspective saved at activation. The switch is
automatic in this build. Newly active camera rigs must also be detached so
changing perspective cannot turn the player's body. Configurable starting view
is tracked in L-48.

### L-48 Camera activation options
Kind: Ready. Maintainer feedback after L-39 validation, 2026-09-27.
Status: done (maintainer confirmed in game 2026-09-27, commit a7ce3a6,
DLL d262d42e).
Freelook gets a child choice for its starting perspective: first person, rear
third person (default), or front third person. It still saves and restores the
view from before activation, and F5 remains available during the session.
FreeCamera gets an Activation choice: Hold or Toggle (default). Hold ends when
the bound key/button is released or input ownership is lost; Toggle keeps the
existing press-on/press-off behavior through menus and focus loss. A wheel
binding cannot be held, so it toggles in either mode. Do not
change action ids or discard existing bindings. Both options are saved; the
session on/off state is not.

### L-49 Fake Offhand / Placement Switch
Kind: Research. Notion proposal, selected by the maintainer 2026-09-27.
Status: implemented; the reported left-click overlap was closed 2026-09-28 as
vanilla behavior (see below), so no Fake Offhand change is planned. Broader
runtime validation remains pending.
The feature has one on/off switch and a separate, unbound key to toggle it.
Its child options are an activation binding (right click by default) and a
target hotbar slot (1-9, default 9). While enabled, ordinary right click should
use the configured slot for block placement, then restore the prior selection.
This is a temporary main-hand selection, not an inventory transfer to the real
offhand. The initial goal is placing building blocks; broader item use is a
separate design decision.
Decided 2026-09-27: an interactive block (such as a chest) retains its ordinary
right-click action; sneak + right click places from the target slot against it.
Air, entity targets and empty/non-block target slots keep vanilla behavior.
The selected slot is restored after each build action. A report that held
placement differed from vanilla was withdrawn after checking unmodded Bedrock.
A temporary change that kept the target slot selected while held interfered
with main-hand use and was reverted. Validate inventory sync and server
behavior in game before marking done.
A right-click activation chord (default or with modifiers) is marked active
synchronously by the input dispatcher, before vanilla receives the click;
other chords replay vanilla use edges. The hook switches and restores the slot
within one `_tickBuildAction` call. `LocalPlayer::mSentSelectedSlot` suggests
the selected slot is synced to the server by difference, so the per-tick
switch may send no equipment packet at all; how the server sees the build
transaction's slot is unverified (check in multiplayer).

Diagnostics (commits 460706e, 1f83d6d, 60b1d2f): research_trace builds log the
build tick, the build-action handler, the build/attack press callback and the
BAI reset/clear call sites (with caller offsets) around physical clicks, plus
the current chord state; automation_trace adds the button dispatch names. They
are installed at load time, so they also run while the feature is off. Commit
9e23946 guards them against the null in-progress BAI the game returns when no
build action is running: dereferencing it had crashed every right click (six
crashes on 2026-09-28, all at `getInProgressBAI().mAction`). The trace code
stays as it is.

2026-09-28 overlap report, verified by the maintainer in the local world:
(1) Fake Offhand off, right held, one left click inserted: placement stops;
(2) the same with the feature on: stops; (3) keeping right held does not
resume it; (4) releasing and pressing right again resumes it. **Unmodded
Bedrock behaves identically**, so this is vanilla parity rather than a Lamium
defect and L-49 closes without a fix. Keeping placement alive across an
intervening left click is therefore a new, user-visible behavior instead of a
repair: L-59.

### L-41 Inventory drag and wheel transfer
Kind: Ready. Notion idea (Item Scroller style), promoted 2026-09-26.
Status: done (maintainer confirmed the transfer gestures and gesture switches
in game 2026-09-27, commit f038d76, DLL da76a373). Individual edge cases and
multiplayer remain unverified.
Gestures clarified 2026-09-27: wheel moves one, Shift+wheel moves one matching
stack, Shift+left drag moves each passed stack, Ctrl+left drag moves one from
each passed slot, and Ctrl+right drag stays vanilla. Wheel up targets storage;
wheel down targets player inventory, regardless of hover side. Ordinary wheel
over a destination stack adds into that exact slot unless full. Shift+wheel
selects the highest matching source slot, leaving the hovered source last,
and auto-places its stack on the destination side.
This version applies only to ordinary storage screens (chest, barrel,
Shulker Box and equivalent generic storage). Its saved master switch defaults
on; its toggle action is unbound by default and usable in gameplay or storage.
Four child switches independently enable ordinary wheel, Shift+wheel,
Shift+left drag, and Ctrl+left drag, all on by default. A disabled gesture
passes through to vanilla. The survival inventory screen alone has no
opposite storage side.

Use the screen's vanilla `_handleAutoPlace` or the manager's
`handlePlaceAmount` for the hovered destination, plus the existing response
tracker; never write stacks directly. A sweep visits each slot once and queues at most
128 requests. Submit one request at a time after the previous response, with a
five-second response timeout. Cancel on cursor item, text input, focus/world/
screen change, feature disable, failed request, or a changed source item.
Sweeps also stop on a changed source count (including changes by another
player). Releasing the mouse stops collecting new slots while already visited
slots finish. See DESIGN.md and VALIDATION.md.

### L-54 Debug View as its own Java-F3-style element
Kind: Design. Direction agreed with the maintainer 2026-09-27 during the
Info & HUD review; the concrete layout still needs confirmation (a demo in
`docs/demos/` like the other HUD decisions).
- Debug View becomes its own HUD element (a new `HudElementId` appended to
  the saved list) instead of the current profile that force-enables every
  Info HUD line and the Target card (`DebugView.h`, `InfoHud.cpp:294`).
  Info HUD and Target keep the user's settings while Debug is on; Debug
  draws only its own dense lines. The F3 default key and the saved switch
  stay; it still ships off.
- Content is a fixed, Java-F3-like block layout. Research round 2
  (2026-09-27) found the right column can be filled with local data only:
  - SDK only: client/game version (`ll::getGameVersion().to_string()`),
    render distance current/max (`IOptionRegistry::getViewDistanceChunks`,
    `getMaxViewDistanceChunksRaw`), clouds and skies, ray tracing vs
    Vibrant Visuals, fullscreen and max frame rate.
  - Windows API, small cached helper: process memory
    (`K32GetProcessMemoryInfo`), CPU name and thread count (registry plus
    `GetSystemInfo`), GPU name (`EnumDisplayDevicesW`), desktop resolution
    and the OS build (`RtlGetVersion`). No external communication, no
    game pointers kept.
  - Not available: GPU utilization, the graphics API version
    (`TargetRenderAPI`'s values are not exposed by the SDK), Java heap
    semantics and server-side stats. Omit, do not guess.
- Sketch: left column = version, fps/frame/ping, `XYZ:`, `Block:` and
  `Chunk:`, facing with yaw/pitch, light, biome and difficulty,
  day/time/weather/moon, the L-57 counters once researched; right column =
  client settings and PC info; in B the look-at target sits above them,
  fixed at three lines. Background none, shadow on, F3 toggles, placement
  editable.
- Decided 2026-09-27 (round 3): **A is adopted**; B and C are not built.
  The line set is fixed (the only setting is the label style). The look-at
  depth is id plus at most two lines, with mob health/armor following
  L-55's meter choice. PC info ships; a row disappears when the machine
  cannot provide it. L-57 adds the entity count first; chunks and
  particles only if cheap.
- The label style option is 「項目名の書き方」 (maintainer, 2026-09-27) with
  the values ゲーム標準 and Java F3 風; game standard is the default. FPS
  and Ping read the same in both. Java-F3-style labels are fixed English
  literals (like Java's own debug screen); game standard uses translations.
- Status: done (verified in game 2026-09-27, DLL 944E886D). The Debug panel
  is drawn from its own collected values; the old profile that forced every
  Info HUD line and the Target card is gone. The pure line model is
  `src/features/information/DebugLines.h` (tested); the Windows reads are in
  `SystemInfo.*`. The panel is fixed to the screen edges and is not a HUD
  element. The L-57 counter line is still absent.
- Follow-ups from the first in-game look (2026-09-27): mixed Japanese text
  was not right-aligned (right and center lines now anchor at the right edge
  and lay the runs out backwards, so a run measurement error cannot move the
  edge); registry names made the CPU line long with a wide gap (fixed by
  collapsing spaces); Info HUD and Target overlap while Debug is open, so two
  child options (on by default) hide them. From the second look: Debug is no
  longer a HUD element at all; it is fixed to the top-left and top-right
  insets like Java's screen, and the layout editor neither shows nor edits
  it. From the third look: Lamium draws its own text shadow half a unit away
  (the engine's one-unit shadow read as doubled on Japanese lines), the debug
  panel gained a text-shadow child switch, and the armor bar uses the color
  sampled from the vanilla armor icon. The armor display setting is L-55
  (done, awaiting check).
- Comparison demo: [docs/demos/debug-view.html](demos/debug-view.html)
  compares A, B and C over a mock scene, with a label switch and the
  normal Info HUD and Target drawn alongside to show that Debug no longer
  overrides them. A now carries the researched client/PC column; the demo
  page's table lists a recommendation per open item.
- Implemented as decided; DESIGN "HUD" reflects the fixed panel.

### L-16 Light overlay redesign
Status: done (verified in game 2026-09-26, DLL 9e7fd594). DESIGN "Light
overlay", docs/demos/light-overlay.html, LIGHT-OVERLAY.md. Follow-ups after
the first check: range up to 64 with per-chunk reading/meshes, FreeCamera
center and direction, fixed-direction option, and three flicker causes.
Review points: numbers hard to read and flat on the ground, small range, poor
readability at angles, no spawn marking.
To decide: camera-facing numbers vs colored markers only; range and update
strategy (per chunk cache); marking of spawnable blocks. minecraft.wiki
(Bedrock): most Overworld monsters cannot spawn where sky light is 7 or more
or block light is above 0. Confirm in game before relying on it.

### L-32 Hotkey overlap and chord semantics **(strong model)**
Status: done (verified in game 2026-09-25, last build 8cbf8977). `ChordDispatch` in
`input/Binding.h` (event-sequence tests in BindingTests); choices made while
building are recorded in DESIGN "Keys". Design agreed 2026-09-24.
2026-09-25 runtime check: checklist 1-10 passed (build 188a1c3a). Follow-up
decided the same day and implemented: a key that begins a longer chord fires
on release (Java F3 behavior, DESIGN "Keys"); verified in game 2026-09-25.
Then the conflict display moved from the footer to warning key caps and a
key-cell tooltip (DESIGN "Keys", docs/demos/hotkey-conflicts.html); verified
in game 2026-09-25.
2026-09-24 playtest finding: overlapping bindings do not behave like the
maintainer expects from Java / Tweakeroo / MaLiLib. Concrete required case:
if one action is bound to `B` and another to `F3 + B`, pressing **F3 then B**
must trigger the `F3 + B` action and must **not** also trigger the `B`
action.

Current Lamium behavior explains the mismatch: `BindingState::update` treats a
chord as matched when every token in that chord is present in the held-input
set. It does not reject extra held inputs, so `{B}` remains a match while
`{F3,B}` is held. `canonicalChord` also sorts tokens, making normal chords
order-insensitive, and `CustomInput::process` evaluates every action
independently, so overlapping matches can both fire.

Implement a small Lamium model inspired by MaLiLib/Tweakeroo behavior, without
exposing their full advanced keybind settings:
- Separate **ordinary action chords** from **modifier-like chords** internally;
  this matching mode is part of the action definition, not a user-facing
  advanced setting.
- Ordinary chords are order-sensitive and do not activate a shorter subset when
  a more-specific chord is completed. Example: with `B` and `F3+B`, F3 then B
  fires only `F3+B`. B then F3 may already have fired B; do not delay a simple
  action waiting to see whether another key arrives later.
- Modifier-like actions (Zoom, Freelook and future actions explicitly classified
  that way) allow unrelated held inputs and are not broken by normal movement or
  gameplay keys. Their purpose is to remain usable while moving/acting.
- If a more-specific ordinary chord becomes active, suppress only the overlapping
  shorter match for that activation. Do not invent delayed dispatch or retroactive
  cancellation of an action that already fired earlier in the input sequence.
- **Identical chords are allowed intentionally.** The Hotkeys UI marks them as a
  conflict/shared binding, but all enabled actions with that exact chord fire
  together. This supports deliberate grouped toggles. Do not resolve identical
  bindings with hidden priority and do not disable either action.
- Press/Hold/Toggle semantics remain properties of the action. Matching mode is
  orthogonal to behavior.
- Mouse + keyboard and wheel + modifier bindings follow the same overlap rules.
  Wheel remains an impulse and cannot back a Hold action.
- Preserve current text-entry, UI ownership, focus-loss, cancelled-event and
  client-thread dispatch safety rules.
- F3-style Lamium chords must coexist with vanilla deliberately: when Lamium
  successfully handles the completed chord, consume the relevant completion
  event so the shorter Lamium binding does not fire; preserve the existing
  vanilla/debug-key suppression behavior needed to avoid accidental F3 actions.

Hotkeys UI:
- warn on exact duplicate bindings and on overlapping subset/superset bindings;
- exact duplicates are informational/actionable warnings, not invalid state;
- show which actions share or overlap a binding so the user can intentionally
  keep or change them.

Tests must cover event sequences, not only held snapshots:
- `B` vs `F3+B`: F3 -> B, B -> F3, releases and repeats;
- exact duplicate bindings: both actions fire once from the same completion;
- three-level overlap such as `B`, `Shift+B`, `Ctrl+Shift+B`;
- modifier-like action while WASD/Space/Shift are also held;
- ordinary Hold transition when a more-specific chord appears/disappears;
- mouse + keyboard and modified wheel cases;
- focus loss, text input, settings ownership and cancelled events.

Behavior reference only; do not copy implementation:
https://github.com/maruohon/malilib/blob/ornithe/1.12.2/src/main/java/malilib/input/KeyBindImpl.java
https://github.com/maruohon/malilib/blob/ornithe/1.12.2/src/main/java/malilib/input/KeyBindSettings.java
https://github.com/maruohon/malilib/blob/ornithe/1.12.2/src/main/java/malilib/input/HotkeyManagerImpl.java
https://github.com/maruohon/tweakeroo/blob/ornithe/1.12.2/src/main/java/tweakeroo/config/Hotkeys.java

---

## Research

### L-37 FreeCamera sees caves from underground (completed follow-up)

Kind: Research. Reopened 2026-09-30; completed 2026-10-07 on `d56b81e`.
The maintainer confirmed underground terrain drawing in a local world and
on BDS, then confirmed normal-view and player-control restoration after
FreeCamera was turned off. Other players' view of the body is explicitly
unchecked; broader lifecycle/graphics coverage stays in VALIDATION.md and
BACKLOG's Pre-release checks. The earlier parked L-37 entry remains intact.

Normal/underground FreeCamera used culler type 3, while spectator used type 5.
Answering spectator from `Actor::isSpectator` or `getPlayerGameType` did not
change the culler. Independently forcing 5 every frame fought native 3 and
blanked the view. The successful implementation intercepts the primary
renderer's own `updateLevelCullerType` request and substitutes 5 for 3 only
during the owning FreeCamera session, calling the original once. It resolves
the implementation from the SDK virtual method and an actual renderer,
checks the game/loader version and observes one unmodified request first.
No game-type, ability or packet changes. The first build `aa5efa9` was disabled
by a 26.51.5-only loader gate; `d56b81e` corrected it for installed 26.51.6.
The successful log confirms SDK-derived virtual slot 26 and a retained
native 3 -> 5 request. Technical notes: [CAMERA.md](CAMERA.md#underground-terrain-visibility-l-37-implemented-2026-10-07).
Build/hash/environment and runtime evidence: VALIDATION-LOG.md.

Retained earlier hypothesis (2026-09-30; source: GroupMountain FreeCamera
README, a GPL-3.0 BDS plugin, PROVENANCE.md group 3; its source was not opened):
that plugin shows caves by making the client really switch to spectator through
the server's game-type packet. The culler might follow the client's actual
game-type change rather than queried values. This path was not needed by the
successful render-only implementation.

### L-89 Distant player positions for map and radar
Kind: Research, then implementation if a typed authoritative path is viable.
Chosen by the maintainer 2026-10-02.
Status: research steps 1-2 answered 2026-10-02 (trace `eeb0ab1`, a world
hosted on a phone and joined from the PC); a typed vanilla-owned path exists.
Built 2026-10-02 (`collectDots` reads the receiver each frame, so the
minimap and world map share it). Checked 2026-10-03 on a phone-hosted world:
faded marker beyond range, jumps while moving, kept while still, removed by
sneaking, normal once loaded; the Nether, disconnect, rejoin and PC rejoin
followed on 2026-10-03. Done 2026-10-03 (a dedicated server not checked).
Findings:
- `Level::getPlayerLocationReceiver()` owns `mCurrentPlayerLocationData`, a
  flat map `ActorUniqueID -> optional<Vec3>`; `updatePlayer`/`hidePlayer`
  fill it. No packet hook is needed. `Level::getPlayerList()` entries carry
  the same `ActorUniqueID` with the name and `SerializedSkinRef` (the radar
  head can come from there).
- Positions are exact feet positions (equal to the loaded Actor's when the
  update arrived). The local player has no entry.
- Updates are sparse: none while the player stands still, about one every
  4.5 s while moving (about 45 blocks apart in the trace). While the Actor is
  loaded the entry is not refreshed and goes stale, so the loaded Actor must
  win, as specified.
- HIDE keeps the entry with an empty position (it is not erased); a later
  update shows it again. The first HIDE was the other player sneaking, the
  second going to the Nether (back in the Overworld, an update showed the
  player again near the portal). A carved pumpkin was not tried.
- Not seen yet: disconnect of the other player, rejoin, a dedicated server.
Look decided 2026-10-02 (docs/demos/distant-players.html, option 2): a
player shown from this state is drawn at the last received position at 70 %
opacity with a grey name; a loaded player keeps the normal look. No fading
by age (decided 2026-10-02): vanilla sends nothing while a player stands
still, so an old position of a still player is exact. A HIDE removes the
marker at once; an id missing from the player list is not shown.
The map radar currently obtains player positions from loaded Actor instances,
so a player outside the normal entity-tracking range disappears even when
vanilla's Locator Bar still knows where that player is.
Desired behavior:
- Keep using the loaded Actor's interpolated position while it exists.
- Only for a player without a loaded Actor, supplement that position from the
  same vanilla player-location state used by the Locator Bar. Do not infer
  positions or bypass vanilla visibility/privacy decisions.
- A vanilla HIDE update removes that player's supplemental marker immediately.
  The same resolved player-position source should be usable by both the
  minimap and world map.
Research order:
1. Inspect the current 1.26.51 / LeviLamina 26.51.5 SDK for a typed Locator
   Bar/player-location cache that already owns the authoritative state.
2. If that state is not exposed, inspect the typed receive path for
   `PlayerLocationPacket` (ActorUniqueID + position/HIDE) and determine
   whether a small Lamium cache can mirror only that public packet state.
3. Establish how ActorUniqueID maps to the existing player marker/name/head
   identity without keeping stale pointers. Clear supplemental state on
   world/server/dimension generation changes and disconnect.
4. Validate Actor -> locator fallback -> Actor transitions, HIDE, reconnect,
   and at least one server before claiming multiplayer coverage.
Prefer reading vanilla-owned state over adding a packet hook. If neither path
can preserve HIDE and lifecycle semantics cleanly, leave distant players
unshown rather than broadening visibility.

### L-87 Player heads on the map
Kind: Research **(strong model)**, then Ready. Requested by the maintainer
2026-10-02 (L-85 had kept players as light blue dots "for now").
Status: done 2026-10-02.
Decided 2026-10-02: heads follow the mob faces switch, renamed "Players
and mobs as faces" (default off), and its hold key (at first a setting of
its own, default on; folded in after the first check); the skin's outer
layer (hair, hats) laid over the face; the same
black outline as mob faces; names stay beside the head; the world map
draws heads too. A skin whose head cannot be read stays a light blue dot.
Research (2026-10-02, SDK): `Player::mSkin` -> `SerializedSkinRef::mSkinImpl`
(`ThreadOwner<SerializedSkinImpl>::mObject`) holds `mSkinImage` (an
`mce::Image`, RGBA bytes), `mFullId`, `mIsPersona` and
`mDefaultGeometryName`. The head is cut from the classic skin layout
(front 8x8 at (8, 8), outer layer at (40, 8), per 64 pixels of width).
Unknown: whether character-creator (persona) skins and skins with custom
geometry put the head there; whether remote players' skins are filled in
on a server. The first build logs, once per skin that gives no head, its
size, persona flag and geometry name ("Radar faces: no head for ...").
Built 2026-10-02: `playerHead` in `MapFaces.h` (tested), `faces::headOf`
keyed by skin id in the mob face atlas.
First check (2026-10-02, `723738e`): the world map and the switch worked;
the minimap head showed another part of the skin (most likely a
character-creator skin). Changed: the head comes first from the skin's own
geometry (`SkinGeometry.h`: the geometry the resource patch names, its
"head" bone and the bones under it such as "hat", box or per-face UV,
current or legacy format), the classic layout only when that gives
nothing; one log line per new skin says which was used.
Second check (2026-10-02, research build `8e51d53`): the character-creator
head still wrong. The dumped skin showed why: its default geometry has a
"head" bone without geometry; the head and hat are poly meshes (normalized
UVs, v from the bottom) in the patch's "animated_face" geometry (32x64
texture units), painted on the skin's animated face image
(`mSkinAnimatedImages`, type Face), not on the 256x256 skin image.
Changed: mesh fronts (normal 0, 0, -1) are read, the animated face
geometry is tried after the default one, and its head is cut from the
animated face image; the half-unit larger hat mesh is laid over the face
at the face's size.
Checked in game 2026-10-02 on `e073fdc` (research build) with other players: the
character-creator skin's head is right, other players' heads right, on
the minimap and the world map. The 128x128 classic skin of the first check
was not seen again on this build; its geometry path was right on
`fa1caed`.

### L-85 Radar mob icons
Kind: Research **(strong model)**, then Design (mockup), then Ready.
Chosen by the maintainer 2026-10-01 (option A below).
Status: done 2026-10-02 (faces opt-in; follow-ups in L-86).
Context: the 2026-09-28 minimap decision kept dots as the default and left
"per-mob icons" as a later option (L-60 "Radar"), never scheduled until now.
Approach A (chosen): draw each mob's face cut at runtime from the texture
the player's game already has (vanilla or a resource pack), located through
the mob's own model (the "head" part's front face), so nothing from the
game is stored in the repository or shipped and no per-mob table is needed
where the model gives it. Fallbacks if the model path fails: B, a Lamium
table of face rectangles per vanilla texture (facts, kept current by hand);
C, spawn egg icons as the Target card draws them.
Research (2026-10-01, SDK): `IClientInstance::getEntityRenderDispatcher()`
-> `getDataDrivenRenderer(actor.getActorRendererId())` -> the renderer's
`mDefaultSkin` (`TexturePtr`, whose `mResourceLocationPtr` names the
texture) and `ActorRenderer::mModel` (`Model::mAllParts`, each `ModelPart`
with `mName`, `mCubes` (per-face `mUV`/`mUVSize`) and `mTexSize`). The
image loads with `TextureGroup::getCachedImageOrLoadSync`, as block
textures do for the minimap. `ActorResourceDefinition` and
`ClientPBRTextureData` are opaque in this SDK, so variants chosen by render
controllers (cat colors, villager professions) are out of reach: the
default skin only. Probe build (xmake option `radar_icon_probe`, not
shipped): for each renderer seen within 48 blocks, logs the skin path, part
names, the head cube's six face UVs and the image size, and writes the
north and south face crops to `logs/radar-faces.bmp`.
Probe result (2026-10-01, probe build DLL `ebf63b00...9d40f4f9`, local
world, mobs from spawn eggs; the cropped faces were viewed): the values are
populated; the head's first cube's face 2 (north) is the face for cat,
chicken, pig, bat, cow, sheep, camel, turtle, drowned, creeper, spider,
cave spider, skeleton, zombie, zombie villager, vex, phantom, enderman,
mooshroom and parrot (tiny, 2x3). The default skin is wrong for villagers
(the profession overlay, transparent face), horses (an armor texture) and
donkeys and mules (an empty 16x16 "no armor" texture): a small table of
base textures per renderer id fixes them. No face: silverfish (no "head"
part), breeze (no cubes); dropped items are not data driven. Faces can be
taller than wide (villagers 8x10): keep the aspect. Cost: one load per
kind. Mockup: [demos/radar-icons.html](demos/radar-icons.html) (placeholder
faces drawn for it, no game art). Decided by the maintainer 2026-10-01:
B, the face with a black ring only (as a well-known map mod does; who
wants friend or foe at a glance keeps the dots); the proposed size (an
8-pixel face plus ring, shrinking with range like the dots); players stay
light blue dots with names for now. Open: how faces and dots are chosen
(a setting, a hold key, or both, as another map mod shows heads only while
a key is held); proposed: a three-way "Mob display: faces / faces while
the key is held / dots" with a hold key that flips it, as "Show in the
world" does for waypoints, default faces, no default key. Decided
2026-10-01: a two-way switch instead ("Mobs as faces", default on) with a
hold key on its row that flips faces and dots while held; always faces or
always dots need no key.
Built 2026-10-02 (not checked in game): `MapFaces.h` (cropping to 8x8 with
the aspect kept, the base-texture table, drawing with a black ring),
`RadarFaces.cpp` (per renderer id: the head part's front face of the
renderer's model, the default skin or the table's texture, two new kinds
per frame, forgotten on world change), the minimap draws faces for hostile
and passive mobs (8 mockup pixels plus a 1-pixel ring, shrinking like the
dots, faint by height); players, items and mobs without a face stay dots;
setting `map.radarFaces` with the hold action `radarfaces` on its row.
First check (2026-10-02, `8bfc845`): faces appeared for the mobs tried,
but looked squashed: they were resampled to 8x8 and then drawn into the
minimap's 256-pixel texture, which the screen scales by a fraction, so
texels came out uneven. Asked to respect the texture's shape. Changed: a
face keeps its texture's own size and proportions (a high-resolution face
shrinks by a whole factor to fit 16); faces sit in one runtime texture and
are drawn over the map on the screen's pixel grid, each texel a whole
number of screen pixels, the black ring half a texel (at least a pixel)
wide; dots keep away from the frame by a face's half width when faces are
on.
Second check (2026-10-02, `2543778`): faces even, but the black ring
flickered while moving east or west (a separate fill and the image met
the screen's pixels differently), and sheep, turtles, cats and horses did
not read as faces (their faces are several cubes: snouts, noses, ears,
muzzles). Changed: the outline is part of the face image (one texel,
around the visible pixels), so it moves with the face; the face is the
head seen from the front, every cube of the "head" part and of the parts
hanging from it painted far to near (worn layers such as hats left out).
Third check (2026-10-02, `4b19ba7`): the outline no longer flickers; the
pig's face was upside down, zombie villagers' faces tiny, horses and
spiders partly see-through: the assumed legacy layout was wrong. Cube
origins are model coordinates with y up, children not offset again.
Fourth check (2026-10-02, research build `e0ab6fe`): the pig is upright;
villagers and zombie villagers still small; sheep, spiders, endermen and
cats looked partly see-through. The dumped faces (20 kinds) are all whole
and opaque; the see-through ones are most likely mobs 8 or more blocks
above or below (drawn at 40 %, by design). Villagers were small because
the texel size followed the longer side (11): texels are now one size for
all, an 8-texel face filling the target, faces under 6 texels enlarged.
Fifth check (2026-10-02, `63f79b4`): cats and spiders at the player's
height still showed the ground through their faces. Cause: entity
textures carry partial alpha as a mask (tinting, glow) on pixels the game
draws opaque; faces kept that alpha. The research dump drew any non-zero
alpha as opaque, so it hid this. Now only alpha 0 is clear.
Sixth check (2026-10-02, `67d418d`): the faces common to most mobs are
right. Left: mobs with no "head" part (silverfish, tadpole) stay dots;
camel and hoglin faces are doubtful (their head bones are rotated, which
the front view ignores); the snow golem and shulker show the face inside
(the pumpkin is drawn as a block, the shulker's face hides in its shell).
Decided by the maintainer 2026-10-02: faces default off (dots), the rest
later (L-86). The snow golem and shulker were made dots for one build
(`f225a72`) and restored the same day at the maintainer's request: their
inner face is the mob's real texture and still recognisable.
Checked in game 2026-10-02 on `f225a72` (faces opt-in, the hold key, the
snow golem and shulker as dots) and accepted with that one change.

### L-82 Seed map: structures and terrain of unexplored areas (experimental)
Kind: Research **(strong model)**, then Design. Chosen by the maintainer
2026-10-01 as a later part of the map.
Status: done 2026-10-01 as a link: the world map opens ChunkBase's seed map
at a place (seed, dimension, version, scale) and copies the seed, checked in
a local world and on a server. Showing biomes or structures predicted from
the seed inside Lamium is a non-goal (decided by the maintainer 2026-10-01;
DESIGN "Product principles"): keeping up with each game version would rest
on third-party generator forks, predictions would look as real as recorded
terrain, and the external seed map already serves the need. Reconsider only
if the game gives clients its generation results, or an accurate source
appears that costs next to nothing to keep current.
From the world seed, show what the client has not loaded: structure
locations first (villages, strongholds, ancient cities, ...), biomes if
feasible, terrain only if a cheap path exists. Each layer has its own
difficulty: structure placement needs exact per-version Bedrock rules;
biomes need the world generator's noise; terrain is nearly full generation.
Research questions:
1. Can the game's own generation code, which ships in the client, answer
   "which biome / is there a structure start at this chunk" for a given seed
   without loading chunks or touching the live world? Calling the game keeps
   results exact across versions and avoids reimplementing generation. Find
   the entry points in the SDK headers and measure the cost per chunk.
2. Where the seed comes from: a local world's level data; on a server only
   if the server sends a real seed (often hidden or fake), otherwise typed
   in by the player. Results from a wrong seed must not look authoritative.
3. Budgeting: generation queries run off the client thread within the
   Map-wide requirements (bounded work, stale results discarded by world and
   dimension generation, clean shutdown).
Research findings (2026-10-01, desk research; nothing run in game yet):
- The game's own generator is reachable in a local world. The integrated
  server's `Level` (`ll::service::getLevel()`) has per-dimension
  `Dimension::mWorldGenerator`; `WorldGenerator` offers
  `findNearestStructureFeature(HashedString, origin, out, mustBeInNewChunks,
  biomeTag)` (what /locate uses), `getStructureFeatureOfType`, and
  `getBiomeSource()`, whose `getBiomeArea(BoundingBox, scale)` samples
  biomes for an area without chunks. Each structure feature
  (`VillageFeature`, `AncientCityFeature`, `OceanMonumentFeature`, ...)
  has `isFeatureChunk(BiomeSource, Random, ChunkPos, seed, surface,
  Dimension)` and `getNearestGeneratedFeature`. Calls must run on the
  server thread (`ll::thread::ServerThreadExecutor`). Results are exact
  for the running version by construction; cost per query is unknown.
- On a server the client has no generator for the server's world. The
  client `Level::getSeed()` / `getLevelSeed64()` exist (another map mod
  read the seed this way); whether a BDS sends its real seed in the
  start-game data is unverified. Realms do not send it. Building a
  standalone generator for a given seed inside the client is unexplored
  and likely heavy and version-fragile.
- Reimplementing generation is the other route, needed for servers:
  since 1.18 Bedrock's terrain and biome noise follow Java's (same seed,
  same biome layout, small boundary and height differences), so a Java
  1.18+ biome generator would do; structures differ: Bedrock picks a
  position per region (spacing/separation in chunks) from a region seed
  and MT19937, with per-structure salts, linear or triangular spreads and
  biome checks. Every game version can change these tables, which is how
  external seed maps keep a map per edition and version.
  Candidate sources: cubiomes (MIT; Java biomes and structures; would be
  incorporated under PROVENANCE group 2 only after the maintainer chooses
  that path), a Bedrock seed-cracker project whose README describes the
  Bedrock placement (no license found: reference-only, do not copy), and
  external seed map sites (closed; usable only to compare results).
Direction from the maintainer (2026-10-01): no feature that works only in
local worlds; keep per-version, per-edition seed maps out of Lamium
(maintenance) and link to an external seed map instead; biomes in Lamium
only if reasonably cheap; weigh everything against maintenance cost and
the mod's direction. The maintainer found the seed read correctly by
another map mod, servers included (not yet verified by Lamium).
External link (checked 2026-10-01 in a browser): ChunkBase's seed map
takes `https://www.chunkbase.com/apps/seed-map#seed=<seed>&platform=<id>
&dimension=<overworld|nether|end>&x=<x>&z=<z>&zoom=<z>`. Bedrock ids name
version ranges (`bedrock_26_50` = 26.50-26.52, `bedrock_26_30`,
`bedrock_26_0`, `bedrock_1_21_120`, ... down to `bedrock_1_14`); an
unknown id silently falls back to the newest Java map, so Lamium would
keep a small table from game version to id, using the newest known
Bedrock id for newer versions.
Biomes: the cheapest cross-environment route is a Java 1.18+ biome
generator (cubiomes, MIT, incorporated under PROVENANCE group 2). Open
risks: new biomes arrive with each drop and the library may lag; Bedrock
and Java boundaries differ slightly. Its agreement can be measured in game
by comparing predictions with the biomes of loaded chunks.
cubiomes status (checked 2026-10-01 on GitHub): upstream Cubitect/cubiomes
(MIT) was last pushed 2024-11-10 and stops at Java 1.21.3 / the Winter
Drop (`MC_NEWEST = MC_1_21_WD`); it lacks later biomes (e.g. sulfur caves).
Maintained forks, both MIT: xpple/cubiomes ("active fork", Java up to 26.3
with sulfur caves, tests, last push 2026-09-30; its README says MSVC is not
supported, clang is, so clang-cl needs a build check), and
FragrantResult186/cubiomes-bedrock (Bedrock versions up to `MC_26_50`,
created 2026-04, one maintainer, few stars, last push 2026-08-25; quality
unknown). Any choice depends on a third-party fork keeping up; measuring
agreement in game is the way to judge one.
Decided 2026-10-01 (maintainer, as recommended): the seed map link and
"copy seed" are shown on servers too, with the radar's unfairness note in
the help; they live on the world map (top bar or right-click menu) with one
on/off row in the Map settings. Build this first; the biome layer is
decided after.
Built 2026-10-01 (not checked in game): the world map's right-click menu
on the ground adds "Open this place in ChunkBase" and "Copy the seed"
(`map.seedLink`, default on, a row under World map). The seed is the
client level's `getLevelSeed64()`; zero counts as not sent. The ChunkBase
map id comes from `SeedLink.h`'s table by `ll::getGameVersion()`; links go
to the shell only when they start with `https://` (`app/Desktop.cpp`, also
the clipboard). When ChunkBase adds a Bedrock map, add its row there.
Checked 2026-10-01 on `46d947b` (local world: seed, version, place,
dimension, terrain match; a friend's server: the seed matched what its
owner had given and the scenery, so a server does send it). The link now
also carries the map's scale as ChunkBase's zoom (measured:
log2(pixels per block) = 4 * zoom - 4, at most 1.75); checked in game on
`ab837d8`. The biome layer became a non-goal (see Status).
Design questions once research says what is possible: which layers and
structure kinds, how predicted content looks next to explored terrain, and
the help text that showing unexplored structures may be treated as unfair on
some servers (like the radar's).
Any outside generation code or data is reference-only unless PROVENANCE.md
records otherwise; Lamium's implementation is independent.

### L-66 Restock the hand from the main inventory
Kind: Ready **(strong model)** for implementation; runtime validation remains
Research. Product direction agreed 2026-09-29 after the bounded spikes.
Status: done 2026-09-30 (integrated on main). Blocks (single and held), food
(including held eating and stew), eggs (single and held), water bucket
remainder exchange, largest-first sources and opt-in hotbar sources were
confirmed in a local world and on BDS with trace builds. The trace-disabled
normal build, latency, and context changes during observation are left to
the pre-release check. See HAND-RESTOCK.md and VALIDATION.md.

Decided:
- Automatically top up held food, blocks and other consumables in the same
  selected slot. Also handle depletion following an observed use; never infer
  consumption just because a slot is empty. Do not switch hotbar selection.
- Prefer compatible main-inventory reserves, matching vanilla item components.
  Take the largest stack; equal stacks come from the higher slot (lower rows,
  nearest the hotbar), so stacks packed from the top stay intact (decided
  2026-09-30 after the first-slot rule moved a lone item before a 64 stack).
  One source per move.
- The saved child switch "Restock from hotbar" (default on since 2026-09-30:
  an empty hand despite a hotbar reserve is the larger risk for most players
  than a hotbar stack being drawn down) adds other hotbar slots as sources
  after the main inventory,
  for top-ups and depletion alike: largest first, ties nearest the selection,
  then the higher slot. The item moves into the selected slot; the selection
  never changes. (2026-09-30: selection switching and a default-on
  "only when the hand empties" variant were tried in discussion/play and
  rejected as harder to explain; the one-sentence rule was kept.)
- Prefer the smallest practical threshold that keeps up with consumption.
  Start implementation with an internal, provisional threshold of 6 items,
  capped below the item's maximum stack size. No public numeric control yet.
  A maximum-one stack uses depletion/replacement handling. Continuous-use
  testing decides whether the provisional value is sufficient; no guarantee
  that the hand never reaches zero, especially with latency or rapid use.
- For a recognized consumption remainder (empty bucket, bowl or bottle),
  exchange it with a compatible unused reserve and place the remainder in
  that reserve's former slot. Without a reserve leave the remainder in hand.
  Merging remainders elsewhere is deferred; unsupported or ambiguous changes
  leave vanilla state alone. Replacement exchange needs its own runtime check.
- Keep the existing Experimental badge, default-off switch and unbound toggle
  action. Add only the hotbar-source child setting. No per-refill toast.
- Offhand and passive totem replenishment are L-68, with independent
  consumption observation and transfer validation. Tool-break replacement is
  excluded from this task; it is part of L-62.
- Treat implementation details (planner shape and trigger abstraction) as
  engineering choices. Distinguish duplicate placement notifications from a
  new use; never allow a delayed operation to move stale stacks. Serialize moves,
  resnapshot immediately before transfer and cancel on interference/context
  loss. Remove probe-only behavior from the normal path. Never restore old
  snapshots over server corrections or retry a rejected move automatically.
- SDK-driven client prediction is allowed as part of the tested legacy-scoped
  transaction path. A local prediction, sent transaction or quiet timeout is
  not server confirmation. No packet-only moves, manual packet construction,
  automatically opened inventory screen, or hand-edited legacy slot metadata.

Implementation: pure consumption/transfer planning and lifecycle tests, native
adapter for observed use and one legacy-scoped move, settings and English /
Japanese help, then a normal trace-disabled build. The above direction can be
revisited after hands-on use. Keep historical spike results in VALIDATION.md.
In game: continuous blocks and food; 16-stack throwables; empty-slot refill;
main inventory vs opt-in hotbar sources; containers/remainders; no reserves;
selection, screen, focus and dimension changes; manual drops/moves; server
correction, latency and re-join agreement. Build/test success is not runtime
validation.

Diagnostics: `xmake f ... --research_trace=y` logs lines prefixed
"research L-3x/L-4x" for L-36, L-37, L-40 and L-44 (see
`src/features/research/` and the L-36 block in `BreakingRestriction.cpp`),
`research L-14` render call-site lines for the Hide Offhand shield path
(`HideOffhand.cpp`), the L-49 build-session trace (`FakeOffhandTrace.cpp`) and
the L-58 Target icon lines. Those items are in BACKLOG-DONE.md.

### L-68 Restock the offhand, including totems
Kind: Research, then Ready. Chosen by the maintainer 2026-09-30 after L-66.
Status: done 2026-09-30; implemented and playtested (trace builds, local world,
then a light BDS pass); the offhand setter records no action, so the move adds
it (VALIDATION.md). The trace-disabled build is left to the pre-release check.
Decided (2026-09-30):
- A child option of Hand Restock, "Restock offhand totems", **on by default**
  (a totem that is not replaced can cost the player's life).
- Covers the totem of undying after it saves the player. Firework rockets
  cannot be used from the offhand in vanilla, and arrows were dropped after
  the playtest: they are drawn from anywhere and the offhand count is not
  shown (maintainer, 2026-09-30; an arrow-total HUD line is a Notion idea).
- Same source rule as L-66: largest main-inventory stack, ties from the lower
  rows; the child "Restock from hotbar" applies too. The offhand slot is
  refilled in place.
- A main-hand totem that saves the player is refilled in its slot as well
  (decided 2026-09-30).
Research:
- Totem consumption has no use action. Find the signal that it popped (actor
  event, server slot/content update) and prove it cannot be confused with a
  manual move, drop or death.
- The offhand is a separate container. Confirm the legacy-scoped setter move
  reaches it on a local world and BDS, with the L-66 ordering (server update
  or quiet period) and no duplication, loss or rollback.
- Firework use from the offhand while gliding and arrow use by a bow: which
  callbacks and sends identify them.

### L-62 Stop held mining before the tool breaks
Kind: Research, then Ready. The control point exists; the bounded runtime
ordering check below decides where the stop belongs before implementation.
Chosen by the maintainer 2026-09-28 from user feedback.
Status: done 2026-09-30; implemented and playtested with the swap extension as
"Tool Protection" (`interaction/ToolGuard.cpp`): swaps from the inventory and
the hotbar, stop toast, strict child (local world, light BDS pass). A swap only follows wear while
held or worn (or mining with it), so an item kept at 1 for Mending stays.
While the attack button is held to mine, the game keeps breaking blocks until
the tool breaks. This stops the held mining session when the held tool has
**1** durability left, i.e. the next block would destroy it.
Decided:
- Block breaking only; attacks and use are unchanged.
- When it stops, a toast says why ("Stopped: the tool is about to break").
- The still-held button does not resume mining. Releasing and pressing again
  mines on, knowingly breaking the tool; that new press is not stopped again
  for the same tool.
- Automatic attack in Hold mode (L-34) is stopped the same way; a new
  physical press is the only way on.
- Hooks: the `GameMode::startDestroyBlock` / `continueDestroyBlock` points
  Tool Switch and Breaking Restriction already use. Unlike L-36, the session
  must not resume on its own.
- On by default (maintainer 2026-09-28): it protects tools and does nothing
  until a tool is about to break.
- The switch is a row under Interaction, beside Breaking Restriction and
  Edge Guard (confirmed 2026-09-28): it changes how mining behaves.
- Tool Switch is unchanged in this task; whether it should avoid a tool with
  1 left is a separate question for later.
Validation: bounded runtime check of the order between a block break and the
durability loss (Unbreaking, Mending) before settling where to stop.
Extension (decided 2026-09-30, after L-66 proved screenless same-slot moves):
- Before stopping, swap: if the main inventory holds the **same item** (for
  example another diamond pickaxe) with more durability, move it into the
  selected slot and put the worn tool where it came from; mining continues.
  Stop only when there is no replacement. One behavior, no extra setting.
- The elytra is covered too: it does not break but stops working at 1
  durability, so while gliding, one at 1 left is swapped with another elytra
  from the inventory (armor-slot move, needs its own Research).
- Child "Never let it break", on by default (maintainer, 2026-09-30): a new
  press does not mine on with the tool either; off restores the press-again
  override above.
- Replacements come from the main inventory, then other hotbar slots (moved
  into the selected slot; decided 2026-09-30 after the first playtest).
- Among several replacements, the closest enchantments win, then the most
  durability, then the lower rows (decided 2026-09-30).
- Swapping covers every damageable main-hand item (tools, weapons, shears,
  fishing rods, flint and steel, ...); stopping stays mining-only
  (decided 2026-09-30).

### L-65 Managed distribution and update-safe packaging
Kind: Research, then Ready. Chosen by the maintainer 2026-09-28.
Status: done 2026-09-28. In a dedicated LeviLauncher instance an update
0.1.2 -> 0.1.3 kept a key binding and a feature switch, and uninstall removed
the package files but kept `config/`, `logs/` and an empty `licenses/`
(VALIDATION.md). `scripts/New-ReleaseArchive.ps1` in CI checks version
agreement and builds the release ZIP; the README recommends LeviLauncher.
LIP CLI was not tried. Managed-update checks use a dedicated instance; never
install Lamium from Bedrinth into the development instance, where copy
deploys and LIP's file records would overwrite and delete each other.
Make Bedrinth/LIP a supported discovery and managed install/update path without
sacrificing user configuration. The package contract is
[DISTRIBUTION.md](DISTRIBUTION.md).

Current state:
- A LIP v3 `tooth.json` exists at the repository root and declares a Windows
  x64 client-only variant with the current LeviLamina Client range.
- Bedrinth already lists Lamium (0.1.1-0.1.3, install command
  `lip install github.com/amatouhake/Lamium#client@0.1.3`), so LeviLauncher
  users may already be installing and updating it through LeviLauncher's LIP
  daemon. Whether settings survive those updates has not been checked in the
  field yet; that is the main risk.
- GitHub Releases remain the documented install path until managed
  install/update has been validated.
- Release archives intentionally exclude runtime-created `config/` and
  `logs/`; `scripts/Check-Package.ps1` enforces that boundary.

Steps:
1. In LeviLauncher (which uses the LIP daemon), run a real clean install ->
   settings change -> managed update with versions already in the registry:
   0.1.2 -> 0.1.3. A new tag would already be public, so do not cut one just
   to test. Confirm
   `config/settings.json` and explicit key bindings survive, while the
   DLL/manifest/notices update, and that LeviLauncher accepts the
   `LeviLamina#client` range for the instance. Repeat through LIP CLI when it
   is an intended supported path. Repeat the smoke test on the next release
   only when the package contract changes. Record actual uninstall behavior rather than
   assuming whether user data is kept.
2. If runtime-owned files survive naturally because they are not package
   assets, keep `preserve_files` empty. Add preservation metadata only if the
   real managed-update test proves it is required.
3. Ready (may be done before steps 1-2): add CI/package checks for version
   agreement across `xmake.lua`, `tooth.json`, the expected `v<version>`
   tag/asset convention and package layout. Keep the manual ZIP path usable.
   The release ZIP is built by hand today and the 0.1.1-0.1.3 archives store
   entry names with `\` separators (`unzip` warns; LIP on Windows installed
   0.1.3 correctly). Build the ZIP with a script that writes `/` separators
   and have the check reject `\` in entry names.
4. After the managed path passes, update the README install section so
   LeviLauncher/Bedrinth is recommended, LIP CLI is the advanced path and
   GitHub Releases is the manual fallback.

### L-40 Fake Sneak (edge protection without sneaking) — high priority
Kind: Research. Notion idea, promoted as high priority 2026-09-26.
Status: done as "Edge Guard" (verified in a local world 2026-09-26, DLL
1db48ae3). A switch and an unbound toggle key in the Interaction category; the
exported `MoveCollisionSystem::fetchCollisionShapes` hook shortens the local
player's horizontal move (pure `guardEdge`, tested) until the feet keep
ground within 0.6 blocks below, only when on the ground and not sneaking,
flying, gliding, swimming, in water or riding. The first build stopped only
for a moment: the integrated server moved its own copy of the player and
corrected the client, so since 910d2eb the copy (the other registry's entity
with the player's box) is guarded too. Multiplayer servers keep their own
movement and will likely pull the player over the edge; untested.
Keep the player from walking off block edges like sneaking does, without
actually sneaking: no speed loss, no sneak pose or network sneak state, no
hitbox change. Separate from Permanent Sneak, which feeds real `SneakDown`.
2026-09-26 trace: `PlayerMoveInput::isSneakDown` is never called for any
entity on the client while walking, sneaking or at edges, so it is not the
edge check. Next candidates: the movement/collision systems that read the
sneaking state (`SneakingComponent`, actor sneaking flag or move-input state).
Third trace (2026-09-26): `SneakMovementSystem::getMaxCollisionVolume` runs
~120-150 times a second whether or not the player sneaks, and
`PlayerMovement::calculateMoveVector` always receives a `SneakingComponent`
(movement factor 0.30) even when not sneaking, so the component is a constant
parameter, not the sneak state; giving it factor 1.0 (F9) changed nothing. The
edge check itself sits inside the unexported sneak movement system. Options:
disassemble the caller of `getMaxCollisionVolume` to find which state it reads,
or clamp horizontal movement at edges ourselves (the fallback noted above).
Maintainer decision (2026-09-26): no disassembly; use only what LeviLamina
exposes, otherwise implement the edge clamp ourselves. Candidate found: the
exported `MoveCollisionSystem::fetchCollisionShapes` receives the entity's
`MoveRequestComponent`, which carries an `mSneaking` flag; the fourth trace
sets it for the local player only (F9) to test whether vanilla edge protection
follows it without sneak pose, speed or network state.
Fourth trace (2026-09-26): the flag was set on every local move request
(20/s) with no effect on edges, so it is overwritten later or not the edge
input. Next: implement the edge clamp ourselves in the same exported hook by
shortening `MoveRequestComponent::mSpeed` before collision (Ready once the
collision query is chosen).
Find the vanilla edge-protection check in 26.51.5 (around `Actor::move` /
movement collision) and whether only that check can see "sneaking". Prefer
reusing that vanilla path over clamping movement ourselves (slabs, stairs,
diagonal moves and scaffolding would need re-implementing). Runtime checks:
full block edge, slab/stair, diagonal, sprint, jump, scaffolding, knockback,
and that other players see a normal, non-sneaking player.

### L-44 Static FOV
Kind: Research (small). Notion idea, promoted 2026-09-26.
Status: closed 2026-09-26, not needed. The video settings do have a vanilla
option that keeps FOV fixed; with it on, sprinting kept `fovModifier` at 1.0
and FOV at 90 in the trace (off: 1.28 and 115).
Keep FOV fixed when sprinting, speed/slowness effects or flying would change
it. Find where 26.51.5 applies the dynamic FOV modifier; it should sit next to
Zoom's FOV hook and must compose with Zoom.
2026-09-26: the video settings have no vanilla option for this.
`LevelRendererPlayer::getFov` runs twice a frame: `variable=true` (world,
99 with a 90 setting and modifier 1.1) and `variable=false` (70, likely the
hand). Sprinting samples are still to be recorded.

### L-18 FreeCamera
Status: done as an experiment (Pi, reviewed 2026-09-24). Flight, movement
freeze, first-person lock and exits were verified in game; details and failed
approaches are in docs/CAMERA.md. Follow-ups: L-25 to L-29 (L-27 is in
Design), L-37 and the L-10 note. Open polish: starting from third person begins at the head rather
than at the previous third-person eye; needs a new approach if wanted.

### L-20 Shape name text input adds stray characters
Status: closed 2026-09-25, not reproducible. The maintainer typed shape names
without stray characters and does not recall reporting this; it was most
likely a misreading of the (since fixed) garbled text at the lower left of
the Shapes screen.

### L-34 Auto attack and auto use (periodic, hold, fast click) **(strong model)**
Status: first version (separate Periodic/Hold/Fast toggles) passed its
in-game checklist 2026-09-25 (DLL 85fdf605) but was hard to understand;
reworked the same day to one switch + one mode + a next-mode key, with no
implicit stops (DESIGN "Automatic attack and use", AUTOMATION.md), then Fast
click's held-only switch and keyed option rows. Status: done (verified in game
2026-09-25, DLL 66d534be). The list below is the original brief.
- Rework Periodic Attack/Use into Auto attack / Auto use with three modes:
  Periodic (hotkey toggle, interval in ticks), Hold (hotkey toggle, keeps
  the button down), Fast click (hotkey on/off; while on, holding the button
  clicks N times per tick).
- Drive clicks from the client tick instead of wall-clock timers so whole
  ticks are exact; Fast click may issue several clicks per tick.
- A physical press stops Periodic/Hold; Periodic and Hold of one action are
  exclusive; existing stop rules (menus, focus, world, dimension) apply.
- Settings migrate from the current periodic interval (seconds) to ticks.
- Verify in game: several attacks/uses per tick actually land on Bedrock
  (scaffolding, snowballs, mobs); Hold keeps mining a block; servers.

### L-31 Continuous Tool Switch across block transitions
Status: done (verified in game 2026-09-25, DLL c81c6cb1). SurvivalMode does not
override `continueDestroyBlock`, so `GameMode::$continueDestroyBlock` is
hooked; a pure `ToolTarget` (ToolChoice.h, tested) re-runs the unchanged
choice rule once per new block position, and `stopDestroyBlock` clears it.
Only the client's local player is tracked (the integrated server's player
also breaks blocks). Whether vanilla also calls `startDestroyBlock` on the
new block does not matter to this change; the in-game dirt/wood/stone hold
check confirmed that `continueDestroyBlock` carries the new position.
2026-09-24 in-game observation: Tool Switch chooses for the first block, but a
continuous physical left-click can move from dirt to wood to stone without
re-evaluating the tool for each new target. The current hook only calls
`selectTool` from `GameMode::startDestroyBlock`.

Desired behavior:
- While the player keeps the physical attack button held, re-evaluate Tool
  Switch whenever the actual targeted block transitions to a new block.
- Dirt -> wood -> stone should be able to select shovel -> axe -> pickaxe
  without requiring the player to release left click.
- Preserve the existing per-target choice rule: if the currently selected item
  is already an effective harvesting tool for the new block, do not switch just
  because another tool is faster.
- Respect Breaking Restriction before selecting and do not synthesize attacks,
  continue breaking through UI ownership, or keep a stale target after focus,
  world/dimension, or input cancellation.

First establish which 26.51.5 client path carries the new block while an attack
is held (`continueDestroyBlock` may be sufficient, but that must be verified).
If it is, make the smallest hook/change and add pure choice/transition tests plus
an in-game dirt/wood/stone hold check. If not, trace the attack/retarget path
rather than polling arbitrary world state.

## Refactor

Completed refactors are recorded here once their remaining steps are finished.

### L-73 Architecture review
Kind: Refactor (strong model). Review done 2026-09-30 on main 4d1790b
(read-only); classification and order agreed with the maintainer the same
day. No rewrite: pure logic in headers, feature docs and validation records
are sound. One commit per step; build + LamiumTests after each.
Status: steps 1-10 done 2026-09-30. In-game checks 1 and 2 passed except
Auto Attack/Use (fixed in 221edcb, rechecked the same day) and an occasional
Breaking Restriction hold that stops breaking (cause unknown; carried into B
and L-15). Step 9 concluded that no further camera split was useful: `Zoom`
was renamed `CameraSessions` (file and class), with trace/probe code and
detached-camera state already separated; the main file is 752 lines with 5
`#if`. Step 10 (084b424) was checked in game. Steps 11 and 12 were dropped
after review (see D). Step 13 with L-15: built 2026-10-07 (`MiningSession.h/.cpp`) and checked
in game 2026-10-08 with Tool Switch and Tool Protection. Review complete.

Fix (can cause wrong behavior)
- A. Breaking Restriction and Tool Switch read and write their
  `restartPending` flag before checking that the call is the client's own
  player. In a local world the integrated server's player runs the same
  GameMode calls (L-31), so it can consume the client's restart (a held
  attack then does not resume) or restart the server's session; possibly
  from another thread. Tool Protection already filters first. In game:
  singleplayer, hold attack across a rejected block and back; Fetch from
  inventory wait and restart while held.

Tidy (agreed)
- B. Shared mining-session control for Breaking Restriction, Tool Switch
  (L-69) and Tool Protection (L-62). Their order is implicit in hook
  priorities (Highest/High/Normal); each pause uses `stopDestroyBlock` and
  each restart re-enters the whole chain through `startDestroyBlock`, which
  Tool Protection counts as a new press and Tool Switch's stop hook sees as
  its own. Do it with, or just before, the L-15 breaking step: pure header
  and tests first, then move one feature per commit. In game: all
  combinations.
- C. Split `Zoom.cpp` (1,195 lines, 21 `#if`): trace/probe hooks and helpers
  to `CameraTrace.cpp`; camera component save/restore (detach, offset,
  body) to its own file; then decide whether Zoom (magnification, FOV,
  wheel, sensitivity) moves out. Freelook and FreeCamera share one detached
  session by design and stay together. Keep the `Zoom` facade (about 40
  call sites). In game: Zoom, Freelook, FreeCamera, F5, dimension change,
  leaving the world; also build with camera_trace and both probes.
- D. `SettingsScreen.cpp` (1,878 lines). Closed after step 10 (maintainer,
  2026-09-30): the Shapes view and the input listeners use 20+ screen-wide
  variables and the Shapes list also renders inside the table, so a file
  split would only move text behind a header of shared variables. Split it
  when the screen grows again, after grouping its state first. Original
  plan: The pure parts are already out
  (SettingsTable, SettingsNavigation, ShapesLayout, ShapeEditor, NumberInput,
  SearchQuery); what remains is about 80 file-scope variables under one
  mutex. First, Enter/Esc/Tab while editing a number or a shape name saves
  settings and shapes from inside the key event; defer that to the frame.
  Then move the Shapes view and the input listeners to their own files. In
  game: search, number entry, key binding, shape editing, HUD layout.
- E. Runtime feature table: one ordered list of start/stop, stopped in
  reverse. Correction (in-game check 2026-09-30): periodic input and the
  automation trace must start in `load()`; they capture the button handlers
  the client registers between load and enable. Moving them into enable()
  (5e877e5) stopped Auto Attack/Use; 221edcb restores the load() start.
- F. One budgeted trace helper instead of the four `trace(stage, value)`
  copies (ElytraSwap, ToolGuard, HandRestock, InventoryMove) and Zoom's own
  budget loops. The trace-only files (with stubs) already follow the rule;
  keep `#ifdef` for all research traces.
- G. SettingsStore fallbacks: 96 literal defaults repeat `Settings.h` (none
  differ today; camera already uses the struct value). Use the struct value
  everywhere and test that each empty section decodes to `Settings{}`. No
  schema framework.
- H. HideOffhand removes all three hooks on stop even when not installed;
  give each an installed flag. No general HookSet (HideEffects needs
  per-hook fail-open).

Not now
- Moving the totem watch out of `HandRestock.cpp` (about 50 lines sharing
  Restock state, validated in game).
- Test registration: every suite and test function is called today.
- `settings::find()` linear scan, JSON write per change: profile first.
  (`Runtime::preferences()` no longer locks; hot hooks use `snapshot()`,
  667031e.)
- Runtime log levels, renaming `Zoom`, test layers (BDS, computer-use): a
  separate Research item if wanted.

Order: 1 A; 2 test that empty sections decode to defaults; 3 G; 4 F; 5 H;
6 E; 7 C trace/probe; 8 C camera state (in-game check); 9 decide on the Zoom
split; 10 D deferred save; 11 D Shapes view; 12 D input listeners (in-game
check); 13 B with L-15 (in-game check). In-game check 1 follows step 1.
