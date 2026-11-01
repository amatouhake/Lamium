# Lamium backlog: done and closed

Items moved out of [BACKLOG.md](BACKLOG.md) once finished, closed or parked,
kept with their full history. Sections follow the kind each item had.
Runtime status is in [VALIDATION.md](VALIDATION.md), the evidence in
[VALIDATION-LOG.md](VALIDATION-LOG.md).


---

## Bugs

### L-290 A filled bucket in the main hand stops the Fake Offhand entirely

Kind: Bug. Maintainer report 2026-10-31 (build not recorded): with a
water bucket in the main hand the Fake Offhand never fires, in any
situation - also while aiming at the sky, where a filled bucket cannot
pour anything.

Cause: `primaryPass` (`FakeOffhandPlan.h`) refuses every bucket as a
passing primary before the aim is looked at, and `idleUse`
(`FakeOffhand.cpp`) only treats an EMPTY bucket or a glass bottle with
no liquid aimed as idle. A filled bucket is therefore never idle, and
`placementSlot` asks for the same `primaryIdle`, so the instant use and
the placement borrow are both closed.

Decided 2026-11-01 (maintainer): a filled POUR bucket (water, lava,
powder snow) is idle when no block face is in reach, whatever lies
along the ray - with a face it really does pour, including onto a
water surface (that places water in the cell above), so any face keeps
the bucket primary. A fish bucket is idle only with no face AND no
liquid aimed, because the game can put the fish into aimed liquid;
milk is never idle (drinking works anywhere). A face the bucket cannot
pour onto is out of scope: it would need a pour model, and guessing
wrong steals a real pour.

Status 2026-11-01: built (`34aafa86`, branch `l290-bucket-idle`), not
yet checked in game. `bucketIdle` (`FakeOffhandPlan.h`) holds the rule;
`idleUse` (`FakeOffhand.cpp`) calls it and the empty bucket, the glass
bottle and the entity guard are unchanged. Table tests in
`tests/FakeOffhandTests.cpp` cover every bucket kind across air, aimed
liquid, block face and entity targets, and an unknown namespace.

In game (right-click binding; the dedicated key from L-292 ignores the
priority): a water bucket in the main hand and a firework in the Fake
Offhand - aiming at the sky uses the offhand, a ground face pours as
vanilla, a water surface pours as vanilla. Also a lava and a powder
snow bucket at the sky, a fish bucket aimed at water (the fish is
placed, not the firework) and at the sky with no liquid (the firework),
milk at the sky (drunk, not the firework), and the empty bucket
unchanged.

In game 2026-11-01 on main `9a06ada2` (PR #59): local passes (sky:
firework, ground: water poured). BDS: gliding with a water bucket in the
main hand and fireworks in the Fake Offhand, the firework fires only
sometimes. Suspect: the game's hit while gliding is sometimes a block
hit (the fallback below the player?), which keeps the bucket primary.
Next: `offhand_trace` build (`444224ca`) logs the hit with each choice.
Trace 2026-11-01: while gliding the hit comes from a 1000-block ray, so
blocks 22-115 ahead were block targets. Fix `938a630c`: a tile hit past
the pick range plus one block (`blockInReach`, `FakeOffhandPlan.h`) is no
target for the Fake Offhand. Not yet checked in game.

Closed 2026-11-01: checked in game on `938a630c` (local and BDS) -
VALIDATION-LOG 2026-11-01.

### L-285 Ghost pick loses to the block at the player's feet

Kind: Research (pick path), then Ready. Maintainer report
2026-10-30: with a block at the player's feet, middle-clicking the
ghost next to that block to pick the ghost's item brings the feet
block into the hand instead; the ghost under the crosshair is not
picked. With schematics off the same middle-click also brings the
feet block, so the immediate cause is vanilla's own pick result in
this geometry (middle-click toward an empty cell still picks a
block), not a Lamium fall-through.
Under L-263's decided rule the ghost pick applies when the crosshair
is on a ghost and not on a real block, and a real block stays
vanilla. If vanilla's hit here is the feet block, that rule reads
the situation as "on a real block" and lets vanilla win — but the
player was aiming at the ghost, so the outcome misreads the aim.
Goal (maintainer 2026-10-30): when the crosshair is on a ghost and
vanilla's pick is only the feet block, pick the ghost's block
instead; that is the natural reading of the aim. A real block
genuinely between the camera and the ghost still wins, as L-263
decided, and with schematics off nothing changes.
Research: what the pick path can see in this geometry — whether
vanilla reports the feet block as its hit or falls back to it on a
miss, and where GhostPick.cpp takes its "on a real block" decision
from. Prefer ray order (the first ghost cell the view ray reaches
versus the hit block's position) over adjacency or distance to the
feet, which would misfire when the feet block is honestly aimed at.
Decided 2026-11-01 (maintainer): (A) the pick ray decides. A game hit
only counts as something the player aimed at when its position lies on
the pick ray (0.05 blocks); then the ghost search stops just before it,
so a real block in front of a ghost still wins. The feet fallback is
off the ray, so it no longer shortens the search and the ghost under the
crosshair wins. The rule is "nearest thing on the view ray", which is
as close as a real block's middle-click behaves. The ray is the hit's
own (`mStartPos`/`mRayDir`), so FreeCamera needs no separate reference,
and the creative give path inherits the same precedence.

Status 2026-11-01: built (`16443e31` plus `9e759986`, branch
`l285-pick-feet-fallback`), not yet checked in game. `PickRay.h` holds
`onRay` and `ghostReach` (pure, table-tested in
`tests/SchematicTests.cpp`); `pickedGhost` (`GhostPick.cpp`) asks them
and passes `throughBlocks` for an aimed hit or the feet fallback, because
within the reach the game's own pick hit nothing solid (L-224); with no
game hit, or an entity off the ray, a real block still stops the search.
The target card (`TargetInfo.cpp`) applies the same `onRay` test, so the
feet fallback no longer shortens its ghost search either and the card
names the ghost the pick brings. Open: whether `HitResult::mIndirectHit`
marks the fallback (a trace would tell; it could replace the tolerance).
In game 2026-11-01 on main `9a06ada2` (merged as PR #62): NOT fixed -
aiming at a ghost beside the feet block, middle-click still brings the
feet block. So the game's hit for this case is not what the rule assumes
(its own ray may point at the feet block, or the hit may be on the view
ray after all). Next: the `pick_trace` build (`444224ca`) logs the hit's
type, block, position, start, direction and `mIndirectHit` with the eye,
view, reach and the ghost found; decide the fix from that log.
Trace 2026-11-01 (VALIDATION-LOG): the fallback is an indirect tile hit
with its own ray straight down. Fix `d54654c4`: an indirect hit is not
aimed and the pick searches along the view; the card treats it as no
hit. Not yet checked in game.

Closed 2026-11-01: checked in game on `938a630c` (local) -
VALIDATION-LOG 2026-11-01.

### L-291 Hand Restock leaves a consumption remainder as its own stack

Kind: Bug. Maintainer report 2026-10-31 (everyday use, build not
recorded): refilling a non-stackable item leaves the consumption
remainder as a separate stack even when an existing stack of the same
item could take it. With water buckets the empty bucket left after
placing water stays as a one-item stack in the slot the bucket came
from, while other empty buckets sit elsewhere in the inventory.

Cause: the replacement branch of `planRestock` (`RestockPlan.h`) is a
single swap between the source slot and the hand - the refill goes into
the hand, the remainder goes into the source slot - and `applyMove`
(`HandRestock.cpp`) sends that one `game::movePair`. Nothing merges the
remainder into a stack of the same item, so one leftover stack
accumulates per restock.

Decided 2026-11-01 (maintainer): (A) a restock may cost two moves. The
swap stays exactly as it is - the hand is refilled at once - and the
remainder is merged into an existing stack of the same item on a later
tick, once the swap's transaction is gone. No target, a full target, a
changed inventory or hand, or about 40 ticks end the wait silently;
the family is `restockRemainder` (buckets, stews to bowls, potions and
honey bottles to glass bottles).

Status 2026-11-01: built (`f30ea0c5`, branch
`l291-restock-remainder-merge`), not yet checked in game.
`RemainderPending` and `planRemainderMerge` (`RestockRemainder.h`) hold
the wait and the destination rule (main inventory before the hotbar,
fullest stack, ties to the lower slot, the offhand never used, the
whole remainder must fit). `HandRestock.cpp` records the remainder when
the replacement swap is predicted and merges it from the same per-tick
path while no restock is being observed; a refused send is retried on
later ticks up to a cap and the 40-tick deadline bounds the wait.
Back-to-back uses keep each remainder's wait (up to four, oldest first;
the deadline restarts once the next restock is no longer observed); a
slot that changed under the merge, or the family's switch turned off,
ends it. The ordinary refill is unchanged. Table tests in
`tests/RestockRemainderTests.cpp`.

In game: with a stack of empty buckets in the inventory, place water
from a water bucket and let the restock refill; the emptied bucket
joins the existing empty-bucket stack instead of sitting on its own.
The same with a stew and with a potion. No stack and a full stack
leave the remainder where it is, opening the inventory right after
gives up silently, and a server (a laggy one especially) exercises the
retry window.
Closed 2026-11-01: checked in game on `9a06ada2` (local and BDS) - VALIDATION-LOG
2026-11-01.

### L-289 Schematic materials count flowing water as water to place

Kind: Bug. Maintainer report 2026-10-31: the materials list counts a
schematic's water as one number, source and flowing together, so the
water actually needed - one bucket per source cell - cannot be read off
it.

Cause: a material line is keyed by the item the block asks for
(`addMaterial` in `GhostVerify.cpp`: `info.item`, else the block name),
and `describe` (`GhostBlocks.cpp`) goes through
`BlockType::asItemInstance`, which ignores block state. A schematic
stores source and flowing water as the same block with a different
`liquid_depth` state - `sulfur_spring_medium_1.mcstructure` carries
`minecraft:water [liquid_depth=0]` (source) and `[liquid_depth=8]`
(falling) - so every water cell collapses into one line and the cells
that need no bucket inflate the count.

Decided 2026-11-01 (maintainer): one line with the source count only -
a flowing cell needs no item, so it adds nothing. The Check tab does
not split: every cell, flowing ones included, is still compared state
for state (`CheckRules.h` `sameBlock` compares `liquid_depth`), because
the flow pattern differs even when the sources match. Only the
materials line stops counting flowing cells.

Status 2026-11-01: built (`62eac8fb`, branch
`l289-water-source-material`), not yet checked in game. `flowingLiquid`
and `materialsCount` (`Verification.h`) give a flowing liquid cell zero
items and every other cell what `itemsPerBlock` says;
`ResolvedPlacement.cpp` uses it for `perBlock`. `GhostVerify.cpp` is
untouched, so the check, the tallies and the mismatch list behave
exactly as before. Table tests in `tests/SchematicTests.cpp`. A liquid
whose cells all flow keeps its line with nothing left to place.

In game: a schematic with a pool whose edges flow (a vanilla structure
file or a saved pond) shows a source count equal to the buckets it
needs; placing those sources leaves nothing to place while the flowing
cells are still compared and still reported when the world's flow
differs. Lava the same, and a waterlogged block still counts as its
own item.
Closed 2026-11-01: checked in game on `9a06ada2` (local world) - VALIDATION-LOG
2026-11-01.

### L-288 A Fake Offhand key that is not right click still acts as one

Kind: Bug. Maintainer report 2026-10-31: with the Fake Offhand bound to
a button other than right click, pressing it acts like a right click
whenever the offhand is not borrowed - a firework in the Fake Offhand
and a block in the main hand place the block. The binding form (a
single button, or a chord that contains right click without ending on
it) was not recorded; both reach the same path.

Cause: `press()` (`FakeOffhand.cpp`) returns early only for a binding
whose LAST token is the right mouse button (that chord lets vanilla
receive the click) and for an instant borrow; every other press,
including one where `chooseSlot` found nothing because the primary
keeps priority, replays a vanilla use edge
(`interaction::periodic::sendUseEdge`). Vanilla then runs its own use
on the main hand, which is the right click the player did not press.

Decided 2026-11-01 (maintainer: work it): a press that borrows nothing
does nothing; only a placement borrow replays the use edge, and a
binding that ends on right click still leaves that borrow to vanilla.
The three outcomes are `queuedPressAction` in `FakeOffhandPlan.h`, so
every reason `chooseSlot` refuses (UI, screen, FreeCamera owning the
look, primary priority) reaches the replay decision without a slot and
replays nothing. The same rule holds for every build tick of the hold
(`queuedHoldEdge`): a hold on another binding sends the use edge, with
the borrowed slot selected, on the first tick that has a placement
slot, and releases it on a tick that loses the slot, so vanilla's
repeat never acts on the primary item.

Status 2026-11-01: closed. Merged via PR #58 (press decision `318fb2c7`,
per-tick edge `1ced1bcb`). `press()` (`FakeOffhand.cpp`) asks
`queuedPressAction` once; the build-action hook sends or releases the edge
per tick (`hold-down` / `hold-up` in the offhand trace); a press that borrows
nothing traces as `press-idle`. Table tests in `tests/FakeOffhandTests.cpp`
cover both helpers. Checked in game on `1ced1bcb` (VALIDATION-LOG
2026-11-01): a hold that starts in the air places once the aim reaches a
block face, so `_tickBuildAction` runs while no use is held.

A chord that contains right click before its last token is not a
right-click chord: the click reaches vanilla before the chord completes, so
the main hand acts on it. Decided 2026-11-01 (maintainer): that is expected,
and such bindings are not refused or warned about.
Follow-up: L-292 (a non-right-click key uses the Fake Offhand without the
primary priority).


### L-248 Shapes creation uses the body instead of the active camera
Kind: Bug / Ready. Maintainer note 2026-10-22 after 0.3.0; not fixed in
that release. `referencePoint()` in `src/ui/ShapesView.cpp` takes
`LocalPlayer::getFeetPos()` for Standing block and Exact position. With
FreeCamera detached from the player, creating a shape (and Move here)
therefore uses the body's position rather than the location being viewed.
Decided (maintainer 2026-10-26): use the same view reference as the schematic
placement actions: the camera's virtual feet (camera position minus the
player's eye height) whenever Freelook or FreeCamera detaches the view, the
player's feet otherwise. Reuse `viewFeet()` from
`src/features/schematic/SchematicActions.cpp` (move it to a shared header
if needed) rather than a second rule. Applies to creating a shape and Move
here with Standing block and Exact position; check Target block picking and
its missing-target fallback, snapping and dimension changes. Validate in
game with FreeCamera detached, in Freelook and with the ordinary view. No
release target selected.
Built 2026-10-27, not yet checked in game: the rule now lives once in
`features/camera/ViewFeet.h` (`camera::viewFeet()`); the schematic actions
floor its point to a cell, the shapes view keeps it precise. Creating a shape,
a draft's reference change and Move here use it, including the missing-target
fallback, which now stands under the camera rather than the body. A targeted
block is taken from the hit before the view point is asked for, so a frame
with an unusable pose still uses the block under the crosshair and only a
missing target falls back. `CameraSessions::freeCameraPose()` (debug readout,
map centre, "add waypoint here") calls the same helper, so those agree with a
shape placed at the same view and a mid-teleport pose falls back to the
standing eye height there too. Pure tests cover the body, Freelook,
FreeCamera, sneaking and unusable poses, and check the standing-block center
as well as its snap mode. In game: create a shape and Move here with Standing
block and Exact position while FreeCamera is detached, repeat in Freelook and
with the ordinary view, confirm a Target block shape with nothing under the
crosshair falls back to the block the camera stands in, and compare the
coordinate readout with a shape's center while detached.

Done 2026-10-31: checked in game by the maintainer before the 0.3.1 release; the fix shipped in `v0.3.1` (`91dcc958`, #44). The instance, the build and the DLL were not recorded. Shapes are created and moved from the camera's view while FreeCamera or Freelook detaches it, and from the body in the ordinary view.

### L-247 Status effects HUD reorders rapidly under beacon effects
Kind: Bug / Ready. Maintainer note 2026-10-22 after 0.3.0; not fixed in
that release. `statusEffects::order()` in
`src/features/information/StatusEffects.h` sorts by remaining ticks each
update. Beacon effects regularly refresh their durations, so their rows
repeatedly trade places and the HUD is hard to read.
Goal: keep rows visually stable across duration refreshes, without hiding
new effects, expiry or changing countdown values.
Decided (maintainer 2026-10-26): a new effect enters at its place by remaining
time; after that each row keeps its place until the effect ends, so a
duration refresh (beacons) never moves it. Rows that end are removed and the
rest close up. Put the rule in `StatusEffects.h` as a pure function of the
previous order and the current effects, with tests (beacon renewals mixed
with normal effects, new/removed effects, a longer potion replacing a
shorter one). Check in game at a beacon with a potion running. No release
target selected.
Status 2026-10-27: built, not yet checked in game: `arrange()` replaces
`order()` in `StatusEffects.h`; it takes the last frame's row ids and keeps
each row in place until its effect ends, so a beacon refresh only changes the
time shown, a new effect enters above the first row with a longer time and an
ended row closes up. `InfoHud.cpp` holds the ids in `shownEffects`: only a
read that succeeded and found no effects ends the order, so one unreadable
frame cannot cost the rows their places; it is kept while the HUD is not
drawn (turning it off and on does not reshuffle) and forgotten on world exit
and when the feature stops, so a new world starts fresh. The layout editor's
samples do not set it. Tests: beacon renewals, new and ended effects, a longer
potion, the infinite-last rule, the read-failed case. In game: the rows must
stay put with a beacon running, a new effect must still land by its remaining
time and an ending one must be removed.

Done 2026-10-31: checked in game by the maintainer before the 0.3.1 release; the fix shipped in `v0.3.1` (`9e039c18`, #43). The instance, the build and the DLL were not recorded. With a beacon running the rows stay put, a new effect enters by its remaining time and an ended effect is removed.

### L-279 "None" outlines still drew the entity ghosts' outlines

Kind: Bug. Review finding 2026-10-27: the "None" outline width removed only the
block ghosts' light-blue outlines (`GhostMesh.cpp`), while entity ghosts kept
theirs (`EntityModels.cpp`), contradicting the help text "None draws no light-blue
outlines". Maintainer decision 2026-10-27: hide the entity ghosts' outlines too.

`EntityModels.cpp` now decides once per model whether to outline at all, fills no
line mesh and renders none when the width is None, instead of building a mesh it
does not draw.

Status 2026-10-27: built, not yet checked in game.
Done 2026-10-27: checked by the maintainer on `9006c175` (DLL SHA-256 `6FA2A242B5B3DA96E66EE3021F3196C8814D5465A3BFF9A66607C959B84C0B15`). The None outline width removes the entity ghosts' light-blue outlines as well as the blocks', and the mistake outlines stay.

### L-278 Real liquids come before a ghost when the target option is on

Kind: Bug / decided. Review finding 2026-10-27: `TargetInfo::target()` checked a
schematic ghost before real water and lava, so with "Target water and lava" on a
liquid in front and a ghost behind showed the ghost, the opposite of what the
option promises. Maintainer decision 2026-10-27: with the option on, real water
and lava come first; with it off, the ghost keeps the target as before.

The liquid scan now runs before the ghost scan (`TargetInfo.cpp`), and the
option's help text in four languages says a real liquid in front of a ghost is
what the target shows. The scan still stops at the first non-liquid, non-air
block, so tall grass or a snow layer hides water behind it; that is recorded here
as the intended rule rather than a defect.

Status 2026-10-27: built, not yet checked in game.
Done 2026-10-27: checked by the maintainer on `9006c175` (DLL SHA-256 `6FA2A242B5B3DA96E66EE3021F3196C8814D5465A3BFF9A66607C959B84C0B15`). With "Target water and lava" on, water in front of a ghost is the target; with it off the ghost is, as before.

### L-277 Flowing lava reads "4 / 3" from an out-of-range depth
Kind: Bug. Review finding 2026-10-27: `interpretBlockState()` in
`src/features/information/TargetInfo.h` halved a slow lava depth
(`(depth + 1) / 2`) against a maximum of three, so a lava `liquid_depth` of 7
outside the Nether printed "4 / 3" and its progress sat at 0. The step count
is clamped to the liquid's own maximum: that row reads "3 / 3", and every
in-range row is unchanged (L-264: three steps outside the Nether, seven in it).
Status 2026-10-27: built, not yet checked in game: the table test in
`tests/TargetDetailTests.cpp` covers `liquid_depth` 0-9 for water, Overworld
lava and Nether lava. In game: aim at flowing lava in the Overworld and in the
Nether (the flow row is unchanged), then set a lava block's depth to 7 outside
the Nether (a `/setblock` with that state) and read "3 / 3".
Done 2026-10-27: checked by the maintainer on `9006c175` (DLL SHA-256 `6FA2A242B5B3DA96E66EE3021F3196C8814D5465A3BFF9A66607C959B84C0B15`). Target Info reads flowing water as n / 7, overworld lava as n / 3, Nether lava as n / 7, and an overworld lava block placed with depth 7 as 3 / 3 with the bar inside its range.

### L-275 A ghost without an item lets vanilla pick the real block behind it
Kind: Bug. Review of L-263, 2026-10-27. In `GhostPick.cpp` the ghost branch
returned only when `describe(...).item` was not empty. For fire, water and
other item-less ghosts the call fell through to `origin(hit, withData)` and
picked the real block behind the ghost, contradicting the comment "The
ghost's block or nothing: never the real block behind it". The same
fall-through happened when the ghost path threw, so a failed pick also
handed the real block to vanilla.
Decided: once a ghost is detected, `origin` is never called. A ghost without
an item does nothing (nothing to select or give), and an exception after that
point is swallowed inside the ghost branch. `origin` stays for the no-ghost
path, which includes the feature being off (`ghostAlong` returns nothing
then).
Status 2026-10-27: built, not yet checked in game. In game: middle-click a
fire or water ghost standing in front of a real block and the hand must stay
as it was.
Done 2026-10-27: checked by the maintainer on `9006c175` (DLL SHA-256 `6FA2A242B5B3DA96E66EE3021F3196C8814D5465A3BFF9A66607C959B84C0B15`). Middle-clicking a fire and a water ghost in front of a real block changes nothing; the block behind is not picked, and ordinary picks are unchanged.

### L-273 Ghost pick ignores the data value and picks another variant
Kind: Bug. Review of L-263, 2026-10-27. `pickFor` in
`src/features/inventory/HandPick.h` compared the 36 slots by item name only,
and `bringToHand` in `src/features/inventory/BringToHand.cpp` filled those
names from `stack.getTypeName()`. The creative give path already compared the
data value (`pickedStack` and `listedInCreative`). For blocks whose variants
share a name (skull, monster egg) the owned path therefore selected or
swapped a different variant and reported success.
Decided: the pure slot description carries both. `HandSlot` (name + aux) and
`HandSlots` describe the 36 slots; `pickFor(slots, selected, name, aux)`
compares name and data value, `creativeSlotFor` takes the same description,
and `bringToHand` takes name + aux, fills them from the stack and returns the
slot that now holds the item. `GhostPick.cpp` passes the ghost block's item
name and aux, the values `pickedStack` builds with `reinit(name, 1, aux)`, so
both paths ask for the same item.
Status 2026-10-27: built, not yet checked in game. Tests in
`tests/FetchSlotTests.cpp`: skull and monster egg variants (another variant
is swapped instead of the one in the hotbar, the carried variant is selected,
a variant that is not carried is nothing).
In game: a skull and a monster egg in another variant must be swapped in, and
a variant that is not carried must do nothing (not select its sibling).
Done 2026-10-27: checked by the maintainer on `9006c175` (DLL SHA-256 `6FA2A242B5B3DA96E66EE3021F3196C8814D5465A3BFF9A66607C959B84C0B15`). A skull and a monster egg ghost of a variant that is not carried is given in creative and does nothing in survival; a carried variant is still selected, and the carried-block and plain survival picks are unchanged.

### L-250 Edge Guard makes the game feel very heavy
Kind: Bug / Research. User report 2026-10-23 after 0.3.0: turning Edge Guard
on makes the game very heavy. Not reproduced or measured yet; the report
does not say whether frame rate drops, input lags or the server corrects
the position (rubber-banding). Edge Guard (`src/features/interaction/
EdgeGuard*.cpp`, plans in `EdgeGuard*Plan.h`) runs per tick and keeps a
correction history for replays; VALIDATION records only the rare drop under
heavy lag, not a cost.
Goal: find whether Edge Guard costs frame time or tick time, and bound it
(DESIGN.md engineering behavior) without weakening the edge stop.
Open: ask the reporter for single-player vs server, what feels heavy and the
settings; compare frame times with Edge Guard on/off locally and on a test
BDS; check the history and replay paths for unbounded work. No release
target selected.
Done 2026-10-27: measured with `--edge_guard_perf=y` (probe build `2e5f6441`) and fixed. Before, `history::observe()` re-derived the replay vtable targets on every own move-vector call - about forty times a second at 4.1-6.9 ms each, 688-1043 ms per five seconds, 14-21% of wall time, and paid even while standing still; the guard itself was cheap (4-19 collision queries per call, 1.4-3.9 ms per five seconds). After limiting the target check to once every 200 calls (`checkReplayTarget`, `EdgeGuardPlan.h`, tested): observe 31-43 us per call, 6-9 ms per five seconds, about 0.15% of wall time, with the guard unchanged. Release build `15d23d7a` deployed and walked (standing, straight walking, at edges) with no behaviour change.

### L-271 Glass pane inside edge stops at a T or cross junction
Kind: Bug / Ready. Maintainer decision 2026-10-27, option (c): a pane's inside
corner edge must also appear at T and cross junctions, not only at an L.
`paneCornerPoint` returns a corner only when a cell has one arm per axis, so a
T or a cross drew no inside edge at all.
Goal: an inside edge at every junction where a pane turns inward - one corner
at an L, two at a T, four at a cross - with the outside edge still an L's alone
(L-208) and the option gate as L-270 left it: outside on
`visuals.connectedPaneCorners`, inside on that and `visuals.connectedCorners`,
nothing computed with the pane option off.
Built 2026-10-27, not yet checked in game: `paneConcaveCorners(parts)` derives
the cell's concave corners from its arms - an east (west) arm at x = 9/16
(7/16), a north (south) arm at z = 7/16 (9/16), the coordinates
`paneCornerPoint` already gives for an L's inside, and a test says the two
agree for all four L rotations. `paneSlidePlan(part, extents, parts)` finds
which of a quad's ends sit at one of those corners: none, one (the whole u
window slides there, as L-255 draws it) or both. `panePieces` cuts a both-ends
face into a corner slice, the untouched middle and a corner slice - a slice is
two texels, the tile border's width, or half a narrower face - so each end
reaches its border with no texel stretched; `ConnectedTexturesHooks.cpp` asks
`Tessellator::vertex` for the extra quads, so the tessellator's own vertex
count, quad info and index phase stay its own, and copies every per-vertex
stream from the face, changing only the axis coordinate and u. Two corners of
one cell are always two texels apart, so no single-cell shape gives a both-ends
face wider than that: the split shows up as two one-texel slices, and the three
pieces are what a wider face would take. What splits is the post face between
two junctions; the post's single-corner faces are still the joints the arm
boxes cover (L-255), so an L looks exactly as it did. `paneCornerFor` now says
which corner a quad takes at a T and at a cross too, and its outside branch is
unchanged. Tests: the corners of L, T, cross, straight and lone; a table of
none / one end / both ends for post and arm faces at every junction; the pieces
of a split; a sliced window keeping its two texels.
`help.visuals.connectedPaneCorners` now names L, T and cross in all four
languages.
In-game check still owed: on a pane row with a stem (a T) and on a cross, both
options on, every face that meets a concave corner must show its inside line at
that corner and the faces that meet none must stay plain; an L must look as
L-255 drew it and the L-208 outside edge must not change; straight and lone
panes must not change. With "Edges at inside corners" off, T and cross must
look as before (merged inside); with the pane option off nothing is drawn.
Watch the split quads with resource packs and Vibrant Visuals: a face of the
bar must not disappear, flicker at the joint or lose its shading, and stained
panes must behave like clear ones.
No release target selected.
Done 2026-10-27: checked by the maintainer on the merged build `c5dcb63c` (DLL SHA-256 `8B3CF63DA8173317A99EA3DBD1011069CF4172DB667E0A6EAEA1A84DAD70FFE2`). A T (east+west+north and east+west+south) and a cross showed the inside edge on every face meeting a concave corner, faces meeting none stayed plain, an L looked as L-255 left it, the outside edge and straight/lone panes were unchanged, and stained panes, resource packs and Vibrant Visuals showed no missing, flickering or unshaded faces.

### L-270 Glass pane inside edge ignores the inside-corner option
Kind: Bug / Ready. Maintainer report 2026-10-27, right after L-255 landed
(`3ac38a3a`): with only "Edges at glass pane corners"
(`visuals.connectedPaneCorners`) on and "Edges at inside corners"
(`visuals.connectedCorners`) off, an L-shaped pane still draws an inside edge,
and he judged that wrong - the inside edge belongs to the inside-corner
policy, which has its own option.
Decided (maintainer 2026-10-27): a pane's inside edge needs both options. Its
outside edge stays on `visuals.connectedPaneCorners` alone, exactly as L-208
left it. Pane option on and inside-corner option off must look like L-208
(outside edge only, inside merged); both on, both edges as L-255 draws them;
pane option off, nothing is computed.
Built 2026-10-27, not yet checked in game: `paneEdgeKinds(paneCorners,
insideCorners)` in `ConnectedTextures.h` gates the two edges (pure, tested for
all four combinations and for an L's outside corner coming out the same either
way); `ConnectedTexturesHooks.cpp` computes and slides the inside corner only
when it allows it, so with the inside-corner option off the arm faces keep the
merged corner post L-208 left. `help.visuals.connectedPaneCorners` now says the
outside edge is this option's and the inside edge needs "Edges at inside
corners" too, in all four languages.
In-game check still owed: pane option on and inside-corner option off, an
east+north and an east+south L seen from inside the turn must show no inside
line and the same outside edge as with L-208; then turn the inside-corner
option on and both lines must appear as L-255 drew them; pane option off must
leave the corner vanilla. Check glass and stained panes, and that the
glass-block inside corners this option also draws are unchanged.
No release target selected.
Done 2026-10-27: checked by the maintainer on the merged build `c5dcb63c` (DLL SHA-256 `8B3CF63DA8173317A99EA3DBD1011069CF4172DB667E0A6EAEA1A84DAD70FFE2`). With the pane option on and the inside-corner option off an L keeps its inside merged (outside edge only, as L-208); with both on both edges draw; with the pane option off the corner is vanilla.

### L-269 3D preview: a waterlogged block's water fights with its faces
Kind: Bug. Seen 2026-10-26 by the maintainer in the Files tab's 3D
preview: a waterlogged stair shows its water where the stair covers the
side, and the water's face fights with the stair's back. The world's
ghosts already hide a waterlogged block's water on the sides the block
covers (`coverMasks`, `sidesCovered`); the preview did not.
Built 2026-10-26 (not yet checked): the preview keeps each palette
entry's covered sides and leaves the water's faces there out. Check
waterlogged stairs, slabs, fences and trapdoors in the preview.
No release target selected.
Done 2026-10-26: checked by the maintainer on `e2ec2d6` (same entry).

### L-268 Schematic cross-plant ghosts flicker
Kind: Bug. Seen 2026-10-26 by the maintainer on `main` (`86723d2e`) and
on the L-256 branch alike: ghosts of cross-shaped plants (dead bush and
similar) flicker. Suspect (not checked): the cross planes are drawn twice
in one place, for example in both windings or in two render layers, and
`dropDuplicateQuads` does not catch the pair. Find which quads the
tessellator emits for one such ghost first. Note from the code:
`faces::sameQuad` ignores winding, so a plane drawn in both windings at the
same corners is already dropped; a pair a little apart would not be.
Trace built 2026-10-26: `xmake f --schematic_quad_trace=y` logs, once per
block whose quads are not all on its cell's sides, every quad one ghost
drew (corners relative to the cell, normal, first UV, render layer) and
whether it was dropped. Also to check: other cross plants (grass, ferns,
flowers, saplings) and whether they flicker too.
Found 2026-10-26 with the trace (VALIDATION-LOG "L-267 door halves and the
L-268 quad trace"): every cross plant draws each plane twice, the copy
reversed and 0.0002 off, beyond `sameQuad`'s default 0.0001, so neither
was dropped. Built (not yet checked): duplicate quads now match within
0.001 (real parts of a block lie 1/16 apart). Check every cross plant and
the stonecutter (it has the same pair).
No release target selected.
Done 2026-10-26: checked by the maintainer on `e2ec2d6` (VALIDATION-LOG
"L-268 cross plants and L-269 preview water checked"): every traced cross
plant and the stonecutter, other ghosts unchanged.

### L-267 Schematic door ghosts: upper and lower halves face different ways
Kind: Bug. Reported 2026-10-26 by the maintainer while checking L-256 on
`291fcd4`: some door ghosts face correctly, others draw their upper and
lower halves facing different ways; one of the two may be ignoring the
open state. Seen again 2026-10-26 by the maintainer on the PR #41 build
(code as `cbf1742`, no near mesh, faces built as on `main`), so it is not
from the L-256 experiment; not a blocker for that PR.
Hypothesis from the code (not checked): while a door half is drawn, the
tessellator's answer for the other half's cell is always the half Lamium
makes from the drawn entry with `upper_block_bit` flipped
(`ghostNeighbor`, `Resolved::halves`), unless the real block there is a
door, even when the file has its own other half there. The game draws a
door's upper half from the lower half's states, so an upper entry whose
saved direction or open state differs from its lower half's would draw
the upper ghost with the upper entry's own states. Possible fix: answer
with the file's ghost at that cell when it is the same block type, and
make a half only where the file has none. Check in game: closed and open
doors in all four facings, both hinges, rotated and mirrored placements,
a door with one half placed for real.
Built 2026-10-26 (not yet checked): while a half is drawn, the other
half's cell answers the file's own block there when it is the same block
type, else the made half as before (the area's edge, a different block);
the same in the 3D preview (`Preview.cpp`).
No release target selected.
Done 2026-10-26: checked by the maintainer on `0a2e0a9` (VALIDATION-LOG
"L-267 door halves and the L-268 quad trace"): four facings, open on and
off, rotations, mirrors, a half placed for real, the 3D preview.

### L-260 Connected Textures: missing-texture checkerboard with a ray tracing pack
Closed 2026-10-26: reproduced locally with the reporter's pack by disabling
or re-enabling it on the title screen and joining; the next atlas build
reused sprite images cached under the same names, drawn at twice or half
the size or as the missing texture. Fix `bb49043d`: sprite names carry the
atlas build number (`spriteName`/`spritePath` in `ConnectedTextures.h`).
Passed in game the same day (DLL SHA-256
`fe7a6dcb0a1b8e93e980b460c62c559379b0643917dcf540abe3c46b37de6190`); the
reporter's own setup is unchecked. Graphics mode changes were ruled out.

Kind: Research (bug). User report 2026-10-26 after 0.3.0, two screenshots:
after installing the ray tracing resource pack "COMPLETELY NORMAL RTX pack",
joined glass faces and sandstone show the magenta/black missing-texture
checkerboard, with ray tracing on and off. Both are Connected Textures
targets, so a joined face is most likely shifted onto a sprite tile the
atlas could not build.
Suspects, in `ConnectedSprites.cpp`: the derived `.texture_set.json` (color
renamed to the sprite, maps baked only when 16-64 px square, otherwise the
base's map is kept, so the color and its maps can differ in size); maps the
pack ships as `.tga` or as uniform values; a sprite counted as placed
because its color PNG was served although the atlas rejected its texture
set.
Reporter's answers (2026-10-26): it is intermittent (it could not be
triggered for a while, then came back); turning Connected Textures off
clears it; it happens with ray tracing on and off; every covered block is
affected (glass, stained glass, panes, sandstone, bookshelves), single
blocks included. The pack is "Mr.Atlaz's COMPLETELY NORMAL RTX" (MCPEDL
`completely-normal-rtx`, CurseForge `completely-normal-rtx`): vanilla 16 px
colors with 32 px normal maps.
Their log (Lamium 0.3.0, LeviLamina 26.51.8, Minecraft 1.26.51, Amethyst
launcher): every terrain atlas build reports "833 sprites for 21
textures, 833 texture sets", "0 skipped" and "833 of 833 sprites in place;
base places match". With the pack the bookshelf set is
`1.16.100` color + `_mer` + `_normal` (1666 baked maps); two builds in
between had the vanilla `1.21.30` set with `_mers` (835 maps), so the pack
was apparently switched off and on. Graphics mode changes 3 -> 1 -> 2 -> 3
right before the second set of screenshots logged no atlas build.
So the sprite places are found and every sprite image was served at the
build; the checkerboard must come later. Leading hypothesis: a graphics
mode change (or another texture reload without `_createAtlas`) reloads the
terrain textures through a path Lamium does not serve, such as
`TextureGroup::reloadAllTextures`/`reloadImages`, `getCachedImageOrLoadSync`
or another `ResourcePackManager::load`/`loadTexture` overload, so the
sprite tiles become the missing texture while the faces still point at
them. Not yet seen locally.
Next: reproduce with the pack in a local world (ray tracing not needed):
Connected Textures on, switch the graphics mode between Fancy and Vibrant
Visuals a few times, turn the pack off and on, reload resources. If it
reproduces, a trace build that logs sprite-path requests on the other
load paths and texture reloads tells which path misses. Whatever the
cause, a sprite that cannot be served must fall back to the vanilla face,
never to the missing texture.
No release target selected.

### L-229 Holding the schematic menu opener closes the menu
Closed 2026-10-20: the second fix passed in game on the same day (deployed
DLL SHA-256
`e09ef07ed07208180dec1c2e725769ce3ff4fd84cb857cd69810a3c7749bd8bc`): holding
the opener leaves the radial menu open, and release then a fresh press
closes it. Esc, right-click/back and the menu actions are unchanged.

Kind: Bug / Ready. High priority. Maintainer runtime report, 2026-10-20,
from the [Notion idea inbox](https://app.notion.com/p/3f7c12ab5b2d8121b2a0db32608f6b7a).
History: confirmed in game on `07f0dc1`; a first fix on `7f47f9a` dropped
repeats only after the settings screen had seen the key, but the opening
press happens before the screen listens, so the first repeat still looked
fresh and closed the menu; the maintainer re-reported it on `dcab6ce`.
Fix: `seedHeld` marks the opener's still-held inputs when a screen opens
(shared by the Settings, World Map and schematic menu openers), so its
repeats are recognised and the existing guard drops them. Files:
`src/ui/SettingsScreen.cpp`.

### L-207 FreeCamera halves the frame rate
Closed during documentation reconciliation, 2026-10-20: the measured open-ground regression was fixed and locally checked on 2026-10-18, as recorded below and in CAMERA.md. The later investigation prompts below predate that result. Culler-switch stability and underground hold were checked separately as L-213/L-214; per-hold frame times and broader coverage remain unchecked. The sky-only view outside loaded terrain is the separate open bug L-227.

Kind: Bug / Research. User report 2026-10-17; maintainer asked the reporter
for other observed factors the same day.
Status: reproduced and measured in a local world 2026-10-18: 65 -> 32 at a
standstill, 39 while flying, 64 after FreeCamera off. The forced 3 -> 5
spectator override alone accounts for the drop; the detached view and
terrain streaming do not. Fix 2026-10-18: the override is only requested
while the camera is inside terrain; measured in game 2026-10-18 (local
world): requests arrive continuously (~85-110 per second), the type
returns to the normal culler within a second of the camera leaving
terrain, and the above-ground frame rate is back at 78 (49 inside).
Check when reproducing (measure first, L-105): how much of the drop is the
detached view drawing more of the world, how much the forced spectator
culler (3 -> 5, L-37; its cost is unmeasured and it may cull less than the
normal type), and how much fast flight (up to 200 blocks/s with sprint)
keeps terrain streaming and mesh building busy. Questions for the reporter:
constant or only while moving; hover in place; recovery after stopping;
speed setting. [CAMERA.md](CAMERA.md)

### L-225 Incomplete map chunks stop getting whole rescans after eight retries
Kind: Bug. Code review of v0.2.3..main, 2026-10-19; not seen in game.
`ScanGrid::saw` caps a chunk's incomplete-result count at 8, and
`probeFirst` asks for a whole scan only when `tries % 4 == 3`, so from the
eighth retry on every retry is the five-column probe. If the probed columns
never arrive while others do, the chunk stays a hole. Fix: keep the backoff
cap but let the count cycle so every fourth retry stays a whole scan.
Checked in game 2026-10-19 (`9a565f0`): maintainer OK. [MAP.md](MAP.md)

### L-226 Recording drops the minimap's tiles while FreeCamera flies far away
Kind: Bug. Code review of v0.2.3..main, 2026-10-19; not seen in game.
The recording scan runs before the minimap each frame and evicts the shared
tile caches around the player (`keepChunks`, 51 chunks) once a second. With
the world map recording and FreeCamera more than 51 chunks from the player,
the minimap's tiles around the camera are dropped while its own scan grid
still says Complete, so they wait 4-15 s for a rescan and the minimap keeps
losing pieces. Fix: eviction keeps tiles near any scan grid's center (display
and recording), so every grid cell keeps its tile.
Checked in game 2026-10-19 (`9a565f0`): maintainer OK. [MAP.md](MAP.md)

### L-224 Target card shows what is behind a ghost at a ghost's edge
Kind: Bug. Maintainer 2026-10-19 (`cdc02c5`): dragging the crosshair slowly
across the border between a real block and a schematic ghost, the card
sometimes names the block or entity behind the ghost. Two causes in the
ghost search on the game's line (L-222): an entity hit skipped the search
altogether, and the walk could graze a real block's cell at the edge (the
game's line does not hit it) and stop there. Fix: entity hits also search
on the game's line, and on that line real block cells are passed over.
Checked 2026-10-19 (`507c605`): stacked cases fixed. Still: between a ghost
and a real block set diagonally (touching at an edge), something else
showed for a moment; between two ghosts set diagonally, slowly turning flipped black/white several times; any
ghost edge made the card flicker. The rebuilt line used the frame's view
direction, which runs a little off the game's own pick ray while the view
moves. Second fix: the search uses the game's ray itself (the hit's start
and direction) when it starts near the eye, falling back to the rebuilt
line. Checked in game 2026-10-19 (`30b66ce`): OK.

### L-222 Target card can show a ghost above the outlined block
Kind: Bug. Found 2026-10-19 while following up L-217 (maintainer asked to
fix the schematic side too). `GhostMarks.cpp`'s clip only decides whether a
missing entity's name tag is hidden, from the render camera; it names no
block and needed no change. The same mechanism as L-217 remained in the
card's ghost search: it walks the eye ray, so at a block's edge it could
enter the air cell above the outlined block and show a ghost there. From
the body the search now stops just short of the game's own hit. Checked
2026-10-19 (`b66feb0`): not enough. A black wool ghost (527, 63, -460) under
a real (extra) block and in front of real stone: aimed just under the seam
the card named the stone behind; the eye ray met the real block above the
ghost and the search stopped there. Second fix: the search runs on the
game's own line, back from its hit along the view, so the eye's offset no
longer matters. Checked in game 2026-10-19 (`41ee214`): OK. Check: with a schematic ghost directly
above a real block, aim just under the seam; the card names the real block.

### L-221 Chat sample shows through the HUD layout editor's lines list
Kind: Bug. Maintainer screenshot 2026-10-18 (`a5edf9a`): with the Info HUD
selected and its lines list open over the chat preview, the sample chat
text reads through the list and through the "Reset this element" button.
Cause not confirmed: the toolbar, its popovers and the action bar were
92-94 % opaque and the buttons 90 %. They are opaque now. If the text still
shows, the next suspect is text drawn after the editor's fills in the same
frame. Checked 2026-10-18 (`7856e1e`): the chat sample no longer shows
through. The maintainer also found: the lines list ran under the Reset/Done
strip (its row text mixed into "Reset this element"), it scrolled neither
by wheel (wheel events carry no position; the pointer as last drawn is
used now) nor by its bar (only drawn), and nothing restored the default
line order. Fixed: the list stops above the strip and its text is flushed
before the strip is drawn; the bar has its own strip and drags; the Info
HUD's "Reset this element" also restores the line order. Checked
2026-10-18 (`5a99b86`): all OK except dragging the bar (a click moved it,
a drag did not: the drag read the bar's track after it had been cleared for
the frame). Fixed (`b8b065a`); checked in game 2026-10-18: OK.

### L-217 Target View names the block above the aimed one
Kind: Bug. Maintainer report 2026-10-18: with another block directly above
the target, looking at the target's upper part sometimes shows the upper
block's coordinates.
Cause (not reproduced here): the card takes the cell from one
`BlockSource::clip` call along the view ray and trusts its `mBlock`. Fix:
the card also clips each solid cell the ray passes before that hit against
the full ray and keeps the nearest hit (`information/TargetPick.h`, pure and
tested), so a cell reported behind the face the ray reaches first loses.
Check in game: stack two blocks, aim at the lower one's upper edge from
several angles and distances; the card shows the lower block's name and
coordinates, also in Freelook and FreeCamera.
Checked 2026-10-18 (`1b578e4`): still wrong. The maintainer's screenshots
(stone on stone, aiming just under the seam) put the
card's ray origin a few hundredths above the game's own pick, so near a
block's edge the card's ray, not the game's clip, reaches the upper block.
Second fix: from the body the card uses the game's own hit (the outlined
block) whenever it is within the card's distance; its own ray only reaches
past it and serves Freelook and FreeCamera.
Maintainer checked the second fix in game 2026-10-18 (`c4884e5`): OK.

### L-215 World map stays black at other scales while the player moves
Kind: Bug. Maintainer 2026-10-18 (in game, flying with the map open): at a
coarser scale, areas already recorded stayed black and were not drawn until
the player stopped. Cause (from the code): the one image worker builds the
first wanted image that is due; moving keeps the images around the player
stale (each recorded chunk makes its region's images at every scale due a
rebuild every 1.5 s), so images never built waited behind those rebuilds.
Fix (`l215-map-coarse-refresh`): a never-built wanted image comes before
any rebuild (`nextBuild`, tested); stale images keep showing their last
version meanwhile. Second report (same day): better, but at 1000 blocks a
quarter-shaped area that had been drawn turned black and showed again after
zooming slightly. Cause: rebuilding a coarser image cleared a quarter whose
child image could not be made right then (a region loading or not saved
yet), and the cleared image counted as fresh. Now a quarter with recorded
data keeps its pixels and the image is rebuilt again later; only a quarter
with nothing recorded is cleared (`quarterAction`, tested). The map screen
also lets a coarser image stand in for a tile whose image came out empty.
Third report: the area right by the player lagged at 2000 blocks (stale
images waited behind never-built ones); never-built and stale images now
take turns. Fourth report: the area by the player was still black at
2000 blocks. Cause (from the code): a coarse image kept a quarter when its
child was not marked dirty, but a child rebuilt on its own first (viewed at
a finer scale) had lost that mark, so the old quarter stayed. Coarse images
now remember the child versions they were made from and keep a quarter only
while that child is unchanged (`quarterUnchanged`, tested). The log also
names images that come out empty with recorded land under them, and why
("World map image ..."), bounded to 150 lines. Not checked in game.
Done 2026-10-18: merged from PR #25 after the maintainer's in-game check of
`f281fa1` (VALIDATION-LOG): no unnatural gaps at any scale while flying and
zooming; the image trace logged nothing.

### L-213 FreeCamera underground view turns itself off mid-session
Kind: Bug. Maintainer 2026-10-18 (in game, `38d7d5a`): during FreeCamera the
underground view worked, then stopped; settings showed "FreeCamera:
Underground visibility" paused, log: "native culler update did not retain
its requested type" 19 s after the hook bound. Since L-207 the culler type
switches between normal (3) and spectator (5) as the camera crosses
terrain, and one native update that still showed the previous type failed
the contract check for the session. Fix (`l213-culler-check`): only 30
such updates in a row fail it (`CullerCheck`, tested). Not checked in game.
Done 2026-10-18: merged from PR #23 after the maintainer's in-game check
(VALIDATION-LOG). Follow-up on how the switching feels: L-214.

### L-211 World map goes dark when zooming out until coarse images are built
Kind: Bug / Ready. Maintainer report 2026-10-18 (during L-210 work): zooming
the world map out turns already drawn areas dark ("Loading N") until the
coarser image levels are built; zooming in already had a coarser stand-in.
Fix 2026-10-18 (branch `l211-map-zoom-out-stand-in`): a wanted tile not
shown yet is covered by the finer images of its area still on the GPU,
drawn over any coarser stand-in (`finerStandIns()`, tested). While they
stand in they are the last slots taken for new tiles, so the coarse uploads
do not evict them first. No extra uploads or CPU images: the 96 textures,
6 uploads per frame and the store's image cache are unchanged. Not checked
in game.
Done 2026-10-18: merged from PR #21 after the maintainer's in-game check
(VALIDATION-LOG): zooming out shows the drawn map, sometimes with a short
delay before the coarse images replace it.
### L-210 World map: black gaps inside the recorded area
Kind: Bug. Maintainer 2026-10-18: places inside the view distance stay black
on the world map; to a player that reads as a bug, not as a limit. Goal:
where the player went, the map fills; the logic stays bounded.
Found from the saved regions (2026-10-18, LamiumTest): two kinds of gaps.
(a) Whole chunks missing along one-chunk arcs and wedges, also over ocean:
the L-206 ring walk resumed relative to a moving player and skipped cells.
(b) Contour-shaped blobs inside partial chunks at the edge of a visit, on
land only: sections the client had not received when the chunk was
scanned, never completed because the player left.
Status: (a) fixed 2026-10-18 on `l210-map-scan-grid` (scan grid, see
MAP.md). First game run: no arcs, but at 96 chunks the map stopped about
33-40 chunks out while standing - edge retries starved the scan (fixed the
same day: unseen first, retries oldest first with backoff); the log now
also gives the reach and the reported view distance. Second run: the near
chunks stayed partial and starved the range the same way (fixed: near and
never-seen chunks take turns, near retries back off). Third run: filled
only after the view turned and then slowly; picking cost a full pass of
30,000 cells per frame (fixed: bounded picking, kinds take turns across
calls). Open: wedges the player never faced stay dark (client loading,
likely by view direction); zooming the world map out shows coarse images
being rebuilt as dark areas for a while (display, separate from recording). (b) open: the 30 s diagnostics line now counts
the recording range as unseen / missing / partial / complete. Check whether
partial edge chunks complete while standing still before deciding anything;
forcing sections to load costs frame rate and is not a fix (maintainer).
Done 2026-10-18: merged from PR #20 after five in-game runs by the maintainer
at 96 chunks (VALIDATION-LOG). The recording now looks at the whole range
within about 30 s and fills what the client has received; incomplete chunks
are probed cheaply and retried within seconds, and an arrived chunk's
neighbors right away. What stays dark is what the client has not received:
directions not yet faced fill when looked at (more since L-212), and
one-chunk lines of chunks that never arrive also stay as holes in the world
itself with Lamium disabled on 1.26.51.01 + LeviLamina, not in the latest
store release. Open for later: the recording budget as a setting (Design).

### L-212 Hiding distance fog keeps far chunks from being built
Kind: Bug. Found 2026-10-18 while chasing L-210 (maintainer, in game, 96
chunks, same place re-entered): with "Distance fog" hidden, large unbuilt
areas filled only exactly where the player looked; with it shown, the area
around the view filled too. Small holes that stay even when looked at were
seen in every case, with Lamium disabled as well (game behavior at 96).
Cause (likely): the fog range was moved to 16384-32768 blocks, and the game
uses it when choosing which chunks to build. Fix (`l212-distance-fog-range`):
the fog starts at the render distance (or vanilla's end if farther) and ends
5% past it, so nothing drawn is fogged, in rain too.
Done 2026-10-18: checked in game by the maintainer (VALIDATION-LOG); merged
from its PR. The small holes are outside Lamium: they stay with Lamium
disabled on 1.26.51.01 + LeviLamina (no other mod loaded), sit at the same
places each time, and do not appear in the latest store release without
LeviLamina. Recheck when moving to a new game or LeviLamina version.
[VISUAL-EFFECTS.md](VISUAL-EFFECTS.md)

### L-205 Carpets do not draw with their colors on the world map
Kind: Bug / Research. User report 2026-10-17.
Status: fixed 2026-10-18 (verified in game from the saved regions). A build
made of wool carpet appeared on the world map without the carpets' colors.
Cause: a carpet's block-graphics visual shape reads as its center only
((0.4,0.4)-(0.6,0.6)), so the covering-block test never fired and the
surface scan kept the block below it; the heightmap already returns the
carpet's position. The scan now also takes the block type's own
getVisualShape (the full (0,0)-(1,1) footprint) as a second source. The
region data after the fix shows carpet columns at the carpet level with
their colors. [MAP.md](MAP.md)
Done 2026-10-18: merged from PR #18. The first attempt (PR #14) started
the scan one above the heightmap on the belief that the heightmap misses
carpets; that start is kept (harmless, and it covers blocks the heightmap
may not count) with its comments corrected. Side effect: other thin blocks
whose type shape spans the column (rails, redstone dust, closed
trapdoors) may now show their own color on the map; not checked in game.

### L-197 Make long settings descriptions fully readable
Done 2026-10-15: in-place, button-only footer expansion passed the maintainer's focused local check on `c4b8f01` (stable hint/button position, collapse on row change, overflow wheel scrolling). Other sizes, languages, numeric help and editing states while expanded stay in VALIDATION.
Kind: Bug / Ready (strong model).
Imported at the maintainer's request on 2026-10-15 from the
[Notion idea inbox](https://app.notion.com/p/3f7c12ab5b2d8121b2a0db32608f6b7a).
Status: the compact translucent panel and chat-return follow-up passed on
`f8c714b` (2026-10-15). The maintainer then chose to replace the help surface
with in-place footer expansion and remove F1 (`b2885fb`); hint anchoring and
collapse on row change followed in `c4b8f01`. No release chosen.
The maintainer reported the truncation again with L-198 through L-201.

Problem confirmed in the current code: `SettingsTableView.cpp` draws the
footer description with a two-line `paragraph`, or a one-line `label` when
`SettingsTable.h` selects `shortFooter` below 230 GUI units of panel height.
`TextFit.h` truncates overflow with `...`; there is no route to the rest of
the text. The missing content depends on font measurement, language and
available space, rather than a limit on the stored description. This is a
settings-comprehension bug; the affected runtime layouts have not been
reproduced in this task.

Latest decision 2026-10-15 (supersedes separate-panel/F1 interaction): keep
the existing settings screen visible and use only the "Read full description"
button to expand its own footer. Change that button to "Collapse" while
expanded. Height follows measured wrapped text, limited to keep several
settings rows and readable categories (up to about 60% of the settings area).
Overflow scrolls under the pointer in the footer; the list keeps independent
wheel scrolling. Header, categories, settings, editing and Esc retain their
ordinary behavior. Preserve selection; reveal it minimally when reflow needs
room. No dedicated help input mode, separate surface or F1 shortcut.

Requirements for the chosen design:

- Detect overflow using the current font, available width, line limit and
  explicit newlines; account for language, GUI scale and resizing. Do not
  use a fixed character/byte limit or break UTF-8 boundaries. Short text
  must not gain an unnecessary expansion control.
- Make all description content reachable. Numeric options currently return
  range/operation text before looking up `help.<option>` in `description()`;
  include the option's own help as well as its range and controls.
- Preserve key capture, numeric/text editing, conflict tooltips, footer
  errors and operation hints. Expanding/collapsing the footer cannot commit
  an edit or change a setting; all ordinary settings interactions stay active.

Files to inspect: `src/ui/SettingsTableView.cpp`, `SettingsTable.h`,
`TextFit.h`, shared widgets and screen input, translations in all three
languages, and their pure layout/text tests. Foundation: [UX-FOLLOWUP.md](UX-FOLLOWUP.md).
Acceptance: pure tests for measured overflow, wrapping and UTF-8; in-game
checks at wide, narrow and short sizes in English, Japanese and Simplified
Chinese, including long Schematics and Item Transfer descriptions and
numeric help. Exercise button expansion/collapse, independent footer/list
scrolling, selection retention and editing/capture/error states. F1 must not
open help; the full text must remain readable without leaving the settings.

### L-203 Keep chat position stable when returning from chat input
Done 2026-10-15: the maintainer passed both follow-up checks on `f8c714b`; the local chat-return and OFF-restoration checklist reported no vanilla-position jump. Server and broader layout/pack coverage remains in VALIDATION.
Kind: Bug / Ready (strong model). Maintainer report 2026-10-15 on `d5978e2`.
Status: render-scope follow-up built and pure tests pass; runtime verification pending.
HUD chat can briefly draw at vanilla's upper left after sending/closing chat
before returning to the configured position. Keep text/background placement
consistent on the first returning HUD draw, without moving input/history or
other menus. The actual rendered HUD identity must not be vetoed by a delayed
global screen-stack name. Retain measured geometry, bounded hierarchy checks,
foreground/player/capability guards and immediate cache restoration.
Files: `features/information/ChatPosition.cpp`, its pure tests if applicable;
contract and timing hypothesis: [CHAT-POSITION.md](CHAT-POSITION.md).
Check send/close and Esc-close repeatedly, several and wrapped messages,
expiry, OFF restoration, alt-tab and world re-entry. No native timing result
is established by compilation or pure geometry tests.

### L-202 Share compatibility, availability and failure reasons
Done 2026-10-15: implementation/build/code checks and the maintainer's local normal-build smoke on `d5978e2` are complete. L-197 help presentation and L-203 chat-return timing remain separate fixes. Server, forced-unverified and injected-failure coverage stays in Pre-release checks.
Kind: Ready (strong model). Chosen by the maintainer 2026-10-15 after the
L-200 audit. Status: implementation and code review complete; normal and
forced-unverified DLL builds, pure/SDK tests and catalog/inventory checks
pass. Minecraft verification remains in Pre-release checks; no release or
deployment chosen. The typed API and its callers migrate together so no
commit can silently retain an unregistered string gate.
Use a typed catalog of risky capabilities (subpaths, not entire features),
one loader-series/minimum policy and explicit Minecraft verification, with
separate runtime waiting/ready/stopped/failed states and typed reasons.
Settings stay separate. Native ownership, field/target checks and restoration
remain with each feature. A patch update within the supported loader series
must not need a feature-local allowlist edit. Unknown game builds stay vanilla.
UI suffixes, detailed help, header and notices consume the same capability
state as execution. Pending game context is not a permanent failure; failures
stay latched until an explicit lifecycle restart. Never retain game pointers
in the registry or add per-frame logging/locking in hot guarded paths.
Migrate every existing production/diagnostic gate to typed ids, and publish
the existing hook/contract failures instead of private availability flags.
Include shared policy/state/aggregation/notice tests, translated names and
reasons in English/Japanese/Simplified Chinese, catalog coverage checks and
normal/forced-unverified build validation. Retain settings/action formats.
Shared consumers are included: schematic 3D previews use the block/model
capabilities too, so a blocked world-render path cannot be bypassed in UI.
Optional Night Vision/trading/Connected Textures/server-pack startup failures
roll back their adapters and keep settings/unrelated features available;
critical input/render foundation failures still abort startup.
Review follow-up: effect hiding, FreeCamera hiding, chat placement and schematic
rendering now report failed or throwing hook removals as `RemovalFailed` for
every dependent capability. Shared cleanup continues through the other hooks,
and restart cannot reattach hooks belonging to a latched cleanup failure.
Schematic blend and actor-light hooks track partial installation individually.
Files: `app/Capabilities.h`, `Versions.*`, `PausedFeatures.h`, guarded feature
adapters listed in GAME-UPDATES.md, paused UI/Actions/translations, tests,
release/catalog checks and documentation. Low-level memory/input/HUD helper
deduplication is separate work, not bundled into this migration.
Game checks: normal changed-feature smoke (especially FreeCamera terrain and
restoration), pending before first native use, partial failures and reasons,
unverified-game vanilla fallback, enable/disable and world re-entry. Builds
and pure/native tests alone do not establish these Minecraft outcomes.

### L-198 Fake Offhand blocks primary use and misses secondary instant use
Done 2026-10-15: the three reported priority/firework/eating cases passed the maintainer's local normal-build checklist on `d5978e2`. Broader and server coverage remains in VALIDATION.
Kind: Bug / Research. Maintainer report 2026-10-15.
Status: fix built with pure regression tests; game verification pending.
Reported: a block in the main hand prevents fireworks and similar secondary
uses; when both hands have blocks the secondary can take priority
over the main hand; a secondary block prevents eating primary food.
Clarified by the maintainer: fireworks while gliding and aiming into air;
the priority case has blocks in both slots. The placement branch now asks
whether primary use passes and refuses an active primary timed use. Normal
blocks pass in air before considering a reported native duration; the
reported block's native traits have not been traced.
Preserve ordinary target interaction and meaningful primary use before
secondary placement or instant use. Never select away from an active timed
use. Investigate air versus block aim and gliding for the fireworks case;
do not turn a valid primary placement into a secondary use.
Files: `FakeOffhand.cpp`, `FakeOffhandPlan.h`, `tests/FakeOffhandTests.cpp`;
contract: [FAKE-OFFHAND.md](FAKE-OFFHAND.md). Check food while hungry,
both hands holding blocks/instant items, fireworks while gliding with air
and block aim, containers, repeated input and selection restoration.

### L-199 Waypoint distance stays at the body during FreeCamera
Done 2026-10-15: the maintainer's local checklist on `d5978e2` covered FreeCamera marker distance/range and restoration; no issue reported. Server coverage remains open.
Kind: Bug / Ready. Maintainer report 2026-10-15.
Status: fix built; game verification pending. The previous code used player
feet even during FreeCamera.
Use the viewing position for world-marker distance, its range filter and
near/far ordering while FreeCamera is active. Keep the existing horizontal
distance convention and ordinary-view behavior. Check flight toward/away
from a waypoint and the death point, range filtering and restoration.
Files: `WaypointMarkers.cpp`; contract: [MAP.md](MAP.md).

### L-200 FreeCamera underground visibility regressed after loader update
Done 2026-10-15: underground visibility and restoration passed the local normal-build checklist on `d5978e2`, loader 26.51.7; ordinary log records native `3 -> 5` retention. Server coverage remains open.
Kind: Bug / Research. Maintainer report 2026-10-15.
Status: fix built, game verification pending. The local log identifies
loader 26.51.7 rejected by the explicit 26.51.5/26.51.6 allowlist
(`unsupported loader version`). This installed patch is now admitted while preserving executable,
renderer ownership, virtual-entry and native request/retained-field checks.
The initial .7 allowlist edit is superseded by L-202's common 26.51 series,
minimum patch 5 policy; the adapter no longer owns a loader version list.
Do not remove the fail-open gates or change player abilities/game type.
Files: `FreeCameraCulling.h`, its tests; contract: [CAMERA.md](CAMERA.md).
Check caves from solid ground, normal view after leaving FreeCamera, local
and server worlds, and the ordinary log's retained 3 -> 5 request.

### L-201 World map opens but cannot close with Mouse 5 binding
Done 2026-10-15: the maintainer's local M/Mouse 5/chord open-close and editing checklist on `d5978e2` reported no issue.
Kind: Bug / Ready. Maintainer report 2026-10-15.
Status: fix built with chord-completion tests; game verification pending.
The report was M closes, Mouse 5 opens only.
The assigned opener must also close the map for keyboard and mouse chords,
once per new press. Preserve editing, waypoint prompts, key capture and
native input releases; never close immediately from the opening press.
Files: `SettingsScreen.cpp`, input binding helpers and tests;
contract: [MAP.md](MAP.md). Check M, Mouse 5, a modified mouse chord,
held/released input, waypoint name/coordinate editing and prompts.

### L-195 Separate the Done and Shown headers in the Placed list
Done 2026-10-15: measured headers (`bd28452`) passed in English, Japanese and docked.
Kind: Bug / Ready. Maintainer screenshot/report 2026-10-15.
Status: recorded for later; explicitly outside the current Target View work.
The English Schematics > Placed column headers read as "DoneShown" because
their text touches/overlaps. The screenshot shows two placements and 0%/1%
progress; the row switches remain recognizable. Reserve separate measured
widths and a normal shared gap for both headers, keeping the headers aligned
with their progress values and switches. Check English, Japanese and Simplified
Chinese at normal and docked widths. Built 2026-10-15 for 0.2.2 (maintainer
raised it before tagging): both headers measured, "Shown" centered over the
switch within the list, "Done" right-aligned 6 units before it. Not checked.

### L-182 Intermittent settings crash during schematic checking
Done 2026-10-14: the preview reset on world exit (`95a09e0`) passed a rejoin and repeated preview check locally; the reporter has not confirmed yet.
Kind: Bug / Research. User report 2026-10-14; not yet reproduced.
Where the logs are (2026-10-14): the reporter's `lamium.log` has no crash
lines (Lamium logs only its own messages). LeviLamina's CrashLogger writes
`<instance>/logs/crash/trace_<date>.log` and `minidump_<date>.dmp`; the
trace names Lamium frames as `Lamium.dll+offset` without the PDB, which can
be resolved here with the release's symbol asset. Ask for both files and
the Lamium version.
Status: cause found 2026-10-14 from the reporter's three 0.2.1 traces
(VALIDATION-LOG): the schematic 3D preview kept blocks and block actors from
the previous world and made actors from them; fixed in `95a09e0` (reset on
world exit). Check in game: leave and rejoin a world with a checked
placement, open the Check tab preview repeatedly.
Original report: pressing the settings key (default L) while a schematic is
being checked/used can occasionally crash. Capture crash log/stack and
placement, Check-tab, background work and world-exit state; look for
lifetime/reentrancy, UI ownership and stale pointers without assuming a
specific cause. Test repeated open/close with active checking and after
world/dimension changes. No release target; crashing is a priority when
reproducible. [SCHEMATIC.md](SCHEMATIC.md).

### L-178 Schematic piston face texture does not match geometry
Done 2026-10-14: piston heads and arms in ghosts (`b9c9d69`, state restore `46ff471`) and in the 3D preview (`27150e5`) passed. The glass border flicker between two placements continues as L-193.
Kind: Bug / Research. User report 2026-10-14; not yet reproduced.
Status: world ghosts right since `5711cc3` (extended, rotated, mirrored;
seen 2026-10-14). The 3D preview lacked the heads; fix `27150e5` unchecked. Glass
next to piston ghosts no longer fights; glass ghosts of two different
placements still do (open). Earlier status: cause found and fix built 2026-10-14 (`b9c9d69`). The maintainer's test file `pistons.mcstructure` showed the real
symptom: the base draws, but the pushing face and arm never draw, in any
state. Ghosts drew a block actor only for blocks without a mesh; a piston's
mesh is only its base. The z-fighting seen beside it (glass next to piston
ghosts and next to another placement's glass) is fixed in `17d7201`.
Original wording: a piston ghost can show the texture intended for its extending
face on a face that does not extend. The rendered shape and face texture
disagree; this is not yet evidence that the stored facing/state is wrong.
Compare retracted/extended, facing and rotation/mirror combinations with the
file's states and an actual block. Keep mesh/state and texture/UV diagnosis
separate. [SCHEMATIC.md](SCHEMATIC.md).

### L-180 Auto Elytra does not trigger on Riptide travel
Done 2026-10-14: the Riptide launch child (`e8c5e3f`) passed locally. A server stays in VALIDATION.
Kind: Bug / Research. User report 2026-10-14; scope of expected trigger
needs confirmation. Rule decided 2026-10-14 (EQUIPMENT.md "Auto Elytra
feedback"): put the elytra on at the Riptide launch, default on, no
automatic glide. Status: built, not checked in game. Check in game: a
Riptide launch in water and in rain puts the elytra on; jumping in the air
glides; it comes off after landing; the child off keeps vanilla; charging
without a launch changes nothing; a server.
Auto Elytra is reported not to respond when a Riptide trident launches the
player. The current documented trigger is jumping with fireworks in the main
hand (L-70), so first establish the intended Riptide trigger and the observable
transition into flight, rather than treating every trident use as an existing
guarantee. Test whether an elytra is available, whether launch occurs in
water/rain, and whether any attempted equip/glide is authorized by the
server. Do not change vanilla movement without an agreed rule.
[EQUIPMENT.md](EQUIPMENT.md).

### L-176 Preserve Chunk Borders and Hitboxes line colors in Vibrant Visuals
Done 2026-10-14: colors seen in Vibrant Visuals on `9b3b8a9` (fix `2d176aa`). Ordinary modes, graphics switching and depth stay in VALIDATION.
Kind: Bug / Research (strong model). Imported from the maintainer's Notion
idea inbox and chosen for the backlog 2026-10-14; no implementation or
release selected in this step.
Status: fixed; colors seen in Vibrant Visuals on `9b3b8a9` (2026-10-14).
Ordinary modes, graphics switching and depth remain in VALIDATION. The
maintainer reports that Chunk Borders loses its colors
with Vibrant Visuals enabled, and has now confirmed the same loss on
Hitboxes in game. The build/hash and detailed environment for this report
were not separately identified (VALIDATION-LOG).
Goal: retain the existing five Chunk Borders color groups and the white
bounds, red eye marker and blue look line of Hitboxes in both ordinary
graphics modes and Vibrant Visuals.

Current evidence and candidate fix:
- `src/overlay/WorldOverlay.cpp` `drawLines()` writes per-group vertex
  colors into one mesh drawn with the `debug` material. Both features use
  this path; the renderer's exact cause remains a hypothesis.
- `src/overlay/LineColor.h` already provides `lines::material()`
  (`selection_box`, with a `debug` fallback) and `lines::colored()` to set
  and restore `ShaderColor`. The schematic line path has runtime evidence
  of colors retained under Vibrant Visuals (SCHEMATIC.md, 2026-10-09).
- Investigate using that existing path for each nonempty color group:
  up to five submissions for Chunk Borders and three for Hitboxes. Merely
  changing the material on a single multicolor draw cannot supply distinct
  per-draw shader colors. Preserve color/alpha semantics and restore shader
  state after every draw, including early returns.

Scope: keep geometry, chunk caching, camera following, `worldLinePull`,
Hitboxes distance filtering and frame interpolation unchanged. Audit all
shared `drawLines()` callers for side effects; do not expand this task into
unconfirmed Light Overlay or other rendering changes. Bound submissions
and retain the existing version/capability and invalid-material fallbacks.
Likely files: `src/overlay/WorldOverlay.cpp`, `src/overlay/LineColor.h` if
needed; pure group helpers/tests only if introduced; OVERLAYS and VALIDATION.

Acceptance: compare both features in ordinary modes and Vibrant Visuals,
including switching graphics settings during play. Verify all existing
color groups, line thickness, terrain depth/occlusion, lines on walls and
over the vanilla selection outline, drawing order with other overlays,
camera tracking and feature OFF restoration. Build/tests alone cannot
close the item. Technical notes: [OVERLAYS.md](OVERLAYS.md).

### L-179 Schematic ghost orientation for shulker boxes and similar blocks
Done 2026-10-14: ghost openings under rotation and mirror match a structure block (`dfc262d`, seen on `9b3b8a9`); the Check compares a placed box's opening (`27fff48`) and marks it yellow in the world (`c92d93a`), both passed on `7548f0f`.
Kind: Bug / Research. User report 2026-10-14; not yet reproduced.
Status: ghost fix checked 2026-10-14 on `9b3b8a9`: openings under rotation
and mirror match a structure block. The Check now also compares a placed
box's opening (`27fff48`, seen working 2026-10-14) and marks it yellow in
the world (`c92d93a`, unchecked). Check in game: all six directions, each rotation and both
mirrors against a structure block load of the same file; if an unturned
placement is also wrong, the cause is elsewhere (actor load or renderer).
Shulker boxes in a schematic can be rendered without their
recorded orientation. Determine whether the file contains the expected
BlockState/NBT and where the placement rotation/mirror, block actor or mesh
draw path loses it. Test all six shulker directions and transformed placements;
audit other orientation-sensitive blocks without assuming they share the
cause. [SCHEMATIC.md](SCHEMATIC.md).

### L-162 Edge Guard: fall after a server correction re-runs recent ticks
Done 2026-10-13: `4fe7b29` captures guarded replay input through an owned copy;
`e8b0a52` also aligns the outgoing raw move vector without modifying physical
input. BDS checks confirm normal controls and slab bridging, twelve correction
windows including nine guarded replays with no input loss or fall dump.
The normal artifact from the same source is restored; its next-launch smoke,
longer play, other servers and unsupported-position correction limits remain
in BACKLOG's Pre-release checks. The exact original fall chain and the server's
internal raw-field handling are not proven; completion covers the verified
input-consistency fix, not a guarantee against every correction.

Follow-up 2026-10-13: the restored normal `e8b0a52` artifact passes BDS edge
stopping and short slab bridging. At margin 0.02, approaching while holding
Shift and releasing it also does not fall. The normal-build smoke gap is
closed; keep default 0.10 and the existing range (maintainer), with the other
coverage limits unchanged. See the newest VALIDATION-LOG entry.

Implementation and validation history below retains superseded candidates
and their dated next steps. Current coverage is in VALIDATION.md and the
latest VALIDATION-LOG entry.

Kind: Bug, Research (strong model). Split from L-161, 2026-10-12; rare.
Current status 2026-10-13: round 2 establishes guarded horizontal input
being lost during history replay. The scoped capture candidate passes the
four smoke checks in BDS and solo play, but the additional BDS run records
full direction keys in eight of nine correction replays. It is insufficient.
The second candidate (`981f630`) broke normal WASD operation and has been
removed along with the ineffective scoped capture candidate. The instance
was restored to `8b8823d`, with WASD, key-release and edge stopping confirmed.
Read-only diagnostic build `a32e9bb` then passes those checks and several
minutes of BDS bridging. Its actual capture/application hooks attach and
connect two guarded captures to full direction-key replay in three correction
windows. Candidate `4fe7b29` substitutes an owned copy at actual input
capture, without changing live physical input. The initial BDS smoke checks
pass. The additional slab-bridging run reproduces a fall after a correction,
even though two correction replays retain guarded raw/processed input. The
copy fixes the observed input loss, but is insufficient: the larger correction
already moves the feet outside the last logged support. Next investigate
slab-placement/correction ordering and guarding the corrected replay position.
No further repetition on the unchanged DLL is requested. The next candidate
aligns the outgoing packet's raw move vector with already guarded analog
input, addressing a remaining full-speed field in the fall trace. Whether
the server uses it to cause this correction is still a hypothesis. The exact
original fall chain is not established by this new case; this item stays open.
Seen once on the local test BDS (trace of `2dc7e45`, VALIDATION-LOG "L-161
margin 0.1"): after a +0.047 block server correction, the client's next
position was 0.17 block further out, off the block, and it fell. The client
re-runs its recent ticks after a correction (`ReplayableActorInput` captures
`MoveInputComponent`; `preApplyInput` restores it); Edge Guard seems not to
apply during that re-run. Next: trace `calculateMoveVector` calls during the
re-run (which state, which position) and whether the captured input is the
guarded one; fail open if the replay path cannot be told apart.
Investigation 2026-10-13: the live input guard only accepts the addresses of
the local player's input component states and only on the client thread;
the collision guard also consults the live player's walking state. Whether
replay uses those states, a copied entity, or its captured `mMove` directly
is not established by the headers. No movement fix yet.
Initial diagnostic approach (round 1, replaced by round 2 below):
the maintainer authorized an `--edge_trace=y` test build on the existing
test BDS and client instance. It observes `ReplayableActorInput::capture`,
`preApplyInput`/`postApplyInput`, the correction-replay tick, move-vector and
collision calls (including counters for replay on another thread), and
packet ticks. It keeps 400 lines, writes the first guarded approach even
without a correction/fall, up to 16 correction windows after three input
sends, and the existing maximum of 12 falls. No history pointers are kept
or replay data changed. Test ordinary edges and slab bridging at margin
0.10 first; a fall is not required to collect useful input-capture evidence.
The history methods' SDK thunks are unavailable at link time; the diagnostic
attaches through the SDK interface's virtual slots on the actual captured
input, verifying executable game code and stable targets. An unavailable
path is logged and left alone; the normal build has none of these hooks.
Normal and diagnostic builds, LamiumTests, LamiumNativeTests and the hook
inventory check pass. Diagnostic DLL SHA-256 (source and deployed copy):
`b612379a7034312c2999cc3d08c0382dd06216dba81cdd0f4c8907d0fee92f5f`.
Round 1 (`d476ae5`) checked 2026-10-13 on the test BDS, actual loader
26.51.7: ordinary edges stop at margin 0.10, slab bridging has no pullback
or fall, jumping may leave. No rare fall reproduced. One correction was
recorded; replay calculated vectors from `other` input states, skipped the
live input guard and ran collision in registry 2. The history virtual-slot
diagnostic refused to attach (unsupported SDK thunk), so input capture and
replay ownership were not established. This does not prove the earlier
fall's cause or a fix.
Round 2 replaces that unavailable diagnostic with exported methods:
`ActorHistory::addFrame`, `prepareEntityForRewindFromCapture` and
`ClientPlayerRewindSystem::applyHistoryChangesToLiveEntity`. Record the
local player's captured/live/rewind inputs, box and motion, frame ticks and
the live result, retaining no game pointers and changing no movement.
Next: establish which replay entity belongs to the local player and what
input/position it uses before connecting a replay guard. Build configuration
returns to `--edge_trace=n`; restore the instance's normal DLL after testing.
Round 2 diagnostic builds and LamiumTests pass; it is deployed for another
short run at margin 0.10. Source and deployed DLL SHA-256:
`7f10147c19e878b0ced7d4da94c70c2e8d6ca0e15c35e2ca6b760b67c4e238d0`.
Round 2 checked 2026-10-13: the same three smoke checks passed, with five
corrections and no fall. Preparation ties each copied replay entity to the
local player. The copied processed input is guarded, but its raw direction
keys rebuild full input during replay (for example guarded (0.030,0.036)
becomes (-0.707,-0.707)). History capture retains those raw keys. The live
collision guard sometimes clamps the replay, but does not prevent this
loss of the guarded input. This identifies a concrete bug without proving
the earlier rare fall's cause.
First candidate (superseded after the additional BDS run): `EdgeGuardHistoryHook` guards an owned copy of
the local raw input at `ActorHistory::addFrame`, which runs before that
tick's movement calculation. Vanilla calculates the copy's move vector and
the existing input guard uses the current live position and velocity. Only
if the guard changes it, horizontal raw fields are substituted during
capture, then restored. Exact entity/registry and client-thread ownership,
feature enable and the capability gate are required; missing data stays
vanilla. Jump, sneak and look fields stay as captured, and the existing
jump/walking exclusions apply. No input or game pointer is retained across
frames, worlds or dimensions. Pure tests cover scoped restoration and a
correction-replay example; SDK tests cover independent snapshot copies and
restoration after unwinding.
Next: trace build to check raw history now agrees with the guarded input,
replay no longer rebuilds full direction-key movement, and the three smoke
checks plus disable, sneak release and rejoin remain normal. Do not mark
the rare fall fixed from build/tests or no-fall smoke alone.
The final normal and diagnostic builds, LamiumTests, LamiumNativeTests and
hook inventory check pass. The fix's trace DLL is deployed for validation;
source and deployed SHA-256:
`476dc27c76b6943d19fac13f13df06da4e0dcefe77cf8f5314f6f30acc780077`.
Configuration is back to `--edge_trace=n`; the normal DLL is retained locally
for restoring the instance after the diagnostic run.
Fix build `d59f16b` checked 2026-10-13: margin 0.10 stops at ordinary edges,
slab bridging has no pullback or fall, jumping leaves, sneak release does
not fall, disabling restores vanilla movement, and enabling then rejoining
restores the stop. The trace verifies guarded raw input during capture and
physical raw movement outside it. No correction window was recorded;
replay and the original rare fall remain unverified. The maintainer agreed
to a few more minutes of diagonal edge approaches and slab bridging with
the same diagnostic build. A subsequent solo-play run passes all four
checks too; its separate log verifies guarded raw history input and the
local server-copy guard, with no correction window or fall dump. The
additional BDS run subsequently passes all four checks, but records nine
corrections: prepared raw snapshot input is guarded, while eight replays
still calculate full Down movement (0,-1). This scoped substitution is
insufficient; the live history raw fields still contain physical direction
keys when later replay input is captured. No fall dump was recorded.
Second candidate (rejected and removed after its runtime check): remove `EdgeGuardHistoryHook`. At the existing
`calculateMoveVector` hook, after guarding the exact local processed input
state, copy only its eight horizontal flags and encoded analog vector to
`MoveInputComponent::mRawInputState`. Do this even if the earlier device
input calculation already applied the guard, so later capture retains the
same movement. Vanilla refreshes device input next tick; no game pointer
or input is retained across calls. Preserve jump, sneak, sprint and look
fields; keep the client-thread, exact-state, walking/jump and capability
checks. Pure/SDK tests cover field preservation and an independent delayed
snapshot surviving the next device refresh. Runtime validation pending;
keep L-162 open until correction replay is checked.
Second-candidate normal and diagnostic builds, LamiumTests,
LamiumNativeTests and the hook inventory check pass. The diagnostic DLL is
deployed (source and deployed SHA-256
`49dab77189696704dd485d5206eebab2a86b2c243af75ce3c8467a30d78cda89`).
Configuration is reset to `--edge_trace=n`; the normal DLL is saved for
restoration (SHA-256
`d5b75af3c0190cc73ce046c1eee0790d9a9e420253aeea0f744aa0f4a1abac81`).
Next BDS check: diagonal approaches and slab bridging at margin 0.10 for
several minutes, plus jump, sneak release, disable and enable/rejoin. Trace
must show whether synchronized raw fields reach actual replay input,
not just the prepared snapshot. No runtime success claimed yet.
Second-candidate result: `981f630` on BDS broke WASD; the maintainer could
only slide slowly relative to the view direction and could not run the
four checks. The trace retains the synchronized analog value across normal
raw move calculations, consistent with feedback into device processing.
The assumption that vanilla would refresh this field independently next
tick was wrong. Field-copy tests did not cover that runtime dependency.
Both candidate mutations are removed from source, along with their
candidate-specific helper/tests. The instance is restored to `8b8823d`
diagnostics (DLL SHA-256
`7f10147c19e878b0ced7d4da94c70c2e8d6ca0e15c35e2ca6b760b67c4e238d0`).
Do not deploy either saved failed candidate as a normal-build restoration.
Next: verify normal WASD and edge stopping are restored, then trace the
actual `IReplayableActorInput` capture/application boundary, which is
distinct from the prepared entity snapshot. Keep live physical/history raw
input untouched across frames. No further movement fix is runtime-verified.
The reverted normal build, LamiumTests, LamiumNativeTests and hook inventory
check pass; trace configuration is off. A clean normal artifact is saved
(SHA-256 `cdc270e3ab5e8cd3a195b754dbe10b73b7f58d02cf45d2c723cb8d72dfa03c51`),
while the instance uses the previously tested `8b8823d` diagnostic artifact.
SDK investigation found exported `ReplayStateComponent::getCurrentCapturedInput`
as a potential observation point for the actual selected replay input;
no hook or mutation has been added there yet.
Recovery confirmed on BDS: WASD, key-release stopping and margin 0.10 edge
stopping are restored. Two correction windows retain the unfixed replay
path, without a fall dump.
Next diagnostic: observe `getCurrentCapturedInput` and bind read-only
capture/preApply/postApply hooks from the actual selected implementation.
The first-round SDK slot helper rejected clang-cl's load-then-jump thunk
form; a standalone program measured the actual SDK thunks, and a bounded
decoder plus pure/native tests handle those forms without assuming slots.
Check the exact local component, client thread, capability gate, readable
game vtable and executable game targets; unsupported or changed targets
fail open. Retain function addresses only, no input/entity pointers across
frames. Gameplay remains the recovered baseline. Runtime attachment and
capture/application observations are pending.
Normal and diagnostic builds, LamiumTests, LamiumNativeTests (including
the actual SDK thunk check) and the hook inventory check pass. The new
read-only diagnostic DLL is deployed; source and deployed SHA-256
`f7b23cf81bcb228209497a24ab220d17c8f53147aab7c136fca11ff16f34bef1`.
Configuration is reset to `--edge_trace=n`; the safe normal artifact is
retained (SHA-256
`0e773c452cd845d4892bfcae6691b5ec65676a3710795b7141773385115daa95`).
Next BDS run: verify WASD/key release and margin 0.10 edge stopping first,
then diagonal approaches and slab bridging for a few minutes. Check that
input diagnostics actually attach and connect local capture to the selected
input's pre/post application. A fall is not required; no new movement fix.
Diagnostic build `a32e9bb` checked: normal controls and margin 0.10 edge
stopping work; several minutes of diagonal approaches and slab bridging
found no problem. Capture/application hooks attach; three correction
windows include two direct guarded-input-loss chains, with the same input
object observed at local capture, selection and replay pre/post application.
For example guarded (0.170,-0.145) is captured alongside raw Down/Right,
then replay rebuilds (-0.707,-0.707). No fall dump was recorded.
Next candidate: at that actual local `capture(MoveInputComponent const&)`
boundary, pass an owned component copy whose horizontal raw fields agree
with its guarded processed input. Preserve non-horizontal raw fields and
the original live component, and retain all ownership/capability checks.
This avoids both the ineffective earlier frame-snapshot substitution and
the physical-input feedback that broke WASD. The candidate is now implemented
in `EdgeGuardHistory.cpp` with an owned-copy helper. Pure and SDK-type tests
verify independent stored input, preserved non-horizontal fields, later
device refresh/key release, analog input and a modeled correction replay.
Normal and diagnostic builds, LamiumTests, LamiumNativeTests and the hook
inventory check pass; actual capture ownership and correction replay
remain runtime-unverified. Completion requires normal controls and evidence that correction replay
uses the guarded captured input, with remaining rare/large-correction and
other-server coverage stated explicitly.
The actual-capture copy diagnostic DLL is deployed for the next BDS check;
source and deployed SHA-256:
`2fd64e3bbbd9706a8ef4e5b5d66a5aadd275d6ace9950c9e1abfad66d4ec05ed`.
Configuration is reset to `--edge_trace=n`. The normal candidate is saved
locally (SHA-256
`c811d03c3a48298c2485309f1c9fe444b8f566db349a22bc83c107dfbf6678cc`);
it is not a runtime-verified recovery build. The previously tested read-only
baseline remains saved separately.
Next check: WASD and key-release stopping first; Edge Guard on, margin 0.10,
diagonal edge approaches and slab bridging for several minutes, then jump,
sneak release, disable and enable/rejoin. Inspect the one-time capture-copy
message and actual replay raw/processed input in correction windows. A fall
is not needed; no success or completed fix is claimed from the builds alone.
Candidate `4fe7b29` checked on BDS: WASD/key release, margin 0.10 diagonal
approaches and slab bridging, jump, sneak release, disable and enable/rejoin
all pass. The capture hook attaches and executes its owned-copy path;
read-only diagnostics attach too. One correction (server tick 802) replays
a normally captured full Down/Right vector, not a guarded capture. This is
expected and does not settle the fix's replay behavior. No fall dump or
hook failure. Keep L-162 open; the next check is a bounded additional BDS
run on this same DLL, alternating diagonal approaches and slab bridging
for roughly five minutes. Fall reproduction and the full smoke checklist
are not needed; inspect whether a correction actually includes guarded input.
Additional BDS result on `4fe7b29`: the maintainer falls while placing slabs
to bridge. Three corrections and one fall dump. Correction 1 verifies the
owned-copy path: guarded (0.042,0.024) survives preApply, both replay
calculations and postApply, without raw direction keys. At correction 2,
server tick 979, x=0.161 becomes x=0.339 (about +0.178); the corrected box
starts at x=0.039, beyond the last logged slab ending at x=0.000. Guarded
(-0.084,-0.099) survives replay too, but replay adds about +0.075 x and
starts falling. Client placement/refusal calls precede it; which server
placement/collision causes the larger correction remains unverified.
The current collision guard intentionally leaves unsupported boxes vanilla,
and copied replay inputs bypass the live-state input guard. Investigate
correction-position handling and slab placement rather than repeating
input-copy tests. Do not mark L-162 done or remove the verified copy fix
solely because it does not cover this additional failure. See the newest
VALIDATION-LOG entry for the preserved fall trace.
Next candidate (packet consistency): at the fall's tick 979, outgoing move
and analog are (-0.084,-0.099), but `mRawMoveVector` is still (-0.707,-0.707).
The server's treatment of this raw field is unverified; the larger correction
does not by itself prove a raw-input or placement cause. Align only that
outgoing field with the guarded analog vector at `LoopbackPacketSender::send`.
Require exact sender subclient, client thread, walking, feature/capability,
no horizontal direction flags, no jump/teleport/vehicle exit, and finite
packet move/analog matching the current local component. Preserve physical
input and all other packet fields, including placement data and sneak state.
Use unscaled analog rather than the separately scaled move vector. Pure/SDK
tests cover stop, brake, sneak scaling, ordinary directions, intentional
exits, stale/mismatched values and independent physical input. The diagnostic
logs the raw change before the send dump. No correction position is changed
and no unsupported replay is frozen. Runtime success is not claimed.
Normal and diagnostic builds, LamiumTests, LamiumNativeTests and the hook
inventory check pass. The packet-consistency diagnostic DLL is deployed;
source and deployed SHA-256:
`406861193453a1f8e8256ce540c38dd6ea803785da2609b5157de5f93a938553`.
Configuration is reset to `--edge_trace=n`; the normal candidate is saved
locally (SHA-256
`c698ec122e28399d5bf82a539cb9b2ea449ecf7e00af98641f11979ae1794699`).
Next BDS check: WASD/key-release and margin 0.10 edge stop first, then
repeat the slab-bridging operation that fell for two to three minutes.
Check jump, sneak release and disable still restore their normal behavior.
Stop if movement breaks or a fall occurs. Inspect the packet raw-change
message, matching outgoing raw/analog fields and any correction/fall window.
Do not infer a solved server correction or a placement cause from smoke
success alone; keep L-162 open until the new trace is reviewed.


### L-156 Low bookshelves above glass become dark under Vibrant Visuals
Done 2026-10-13 by L-164: the darkening followed the split; sprite faces show none under Vibrant Visuals (checked in game).
Kind: Bug, Research (strong model).
Status: open. Maintainer's actual-play report, 2026-10-12, Lamium 0.2.0.
Reported conditions: Vibrant Visuals enabled, glass below, lower bookshelves
become dark. Screenshot: `スクリーンショット 2026-10-12 014000.png`
(report attachment), showing dark vertical areas in the bookshelf faces.
The maintainer says the appearance closely resembles the existing darkening
issue (L-133). This is a comparison of symptoms, not evidence of a shared
cause. L-133 concerns split translucent panes; bookshelves also use split
Connected Textures geometry but are a separate reproduction.
Maintainer follow-up, 2026-10-12: Connected Textures on/off compared; the
dark vertical bands appear only with Vibrant Visuals and Connected Textures
both on, so Lamium's split bookshelf geometry is implicated under Vibrant
Visuals (shadows/normals of the split faces are the first suspects).
Next: glass present/absent, bookshelf height/view direction, time of day
(sun shadow direction), and sandstone (also split) under the same setup. Record
lighting, Night Vision and resource packs. Separate vanilla lighting from
Lamium tessellation or renderer-state effects before changing the path.
Linked to L-133 (maintainer 2026-10-12: the look is too similar to treat
separately). Both appear only when Lamium splits a face into several quads,
and both are dark gradients rather than wrong texels; research them as one
question (what per-quad value the renderer interpolates or sorts differently
for split faces), but confirm the shared cause before reusing L-133 results.
Do not treat old pane experiments as a proven fix or cause for this report.
L-164's spike answered it (round 4, 2026-10-12): bookshelf sides drawn as
one quad with a generated sprite show no dark bands under Vibrant Visuals;
the one-sided split next to them does. The fix is L-164's method.
Keep investigation bounded; native render changes must fail open.
Exact reproduction, affected extent and cause remain unverified by an agent;
no fix or post-fix runtime check yet.

### L-157 Chunk Borders are intermittently hard to see
Done 2026-10-12: Chunk Borders and Hitboxes pulled toward the eye by 0.996 (`6cd2a68`); lines on walls stay whole, checked in game.
Kind: Bug, Research; Design only if a visibility policy change is needed.
Status: open. Maintainer's actual-play report, 2026-10-12, Lamium 0.2.0.
Reported symptom: Chunk Borders sometimes become difficult to see.
Screenshot: `スクリーンショット 2026-10-12 015458.png` (report attachment).
Lines remain visible in the image; some are much less distinct against
terrain than against the sky. The report does not establish missing geometry,
a change from earlier releases, or the trigger. Maintainer follow-up,
2026-10-12: taken under Fancy graphics at night; other video settings not
checked.
Next: compare the same scene/view with different lighting and graphics
modes, camera distance and direction, near/behind terrain, and other overlay
combinations. Inspect the shared line material/depth path in WorldOverlay
and record whether geometry, depth, color/alpha or contrast is responsible.
Preserve existing colors and grid structure until evidence justifies a
change. Check Chunk Borders and Hitboxes together if shared rendering changes.
See OVERLAYS.md for the current contract.
Maintainer 2026-10-12: the lines break up where they meet terrain (other
choices offered: thin lines, colors blending in, lines vanishing). In the
screenshot a dirt wall lies on the chunk wall's plane and the lines on it
show as dashes: lines coplanar with block faces fight for depth. Chunk
Borders and Hitboxes were the only world lines not pulled toward the eye
(Depth.h rule 2). Built 2026-10-12: `drawLines` pulls them by 0.996
(`worldLinePull`); colors and grid unchanged. In-game check pending.

### L-161 Edge Guard: refuse slab merges that push the player off an edge
Done 2026-10-12: pushing slab merges refused at build start and `useItemOn`, the edge margin 0.1 by default and a setting (0.02-0.30; `bd65aae`); checked on the local test BDS and locally. The rare fall after a server correction moved to L-162.
Kind: Bug follow-up of L-160 (strong model). Decided with the maintainer
2026-10-12 (DESIGN.md, Edge Guard).
Found on BDS while bridging with slabs: walking with Edge Guard at an edge,
a slab placed onto the slab the player barely stands on merges into a full
block in the player's own cell, and the game pushes the player out sideways
(the shallowest way out), off the edge. Sneaking refuses placements into the
player's own cell, so vanilla sneaking never hits this.
Built 2026-10-12, in-game check pending: `GameMode`/`SurvivalMode`
`useItemOn` hooks refuse a slab when the target cell (the clicked slab through
its top or bottom face, or the neighbor cell when it holds a slab) overlaps
the player's box while the box's center lies outside that cell (pure
`mergePushesOff`, tested). Under the center the merge lifts the player and
stays allowed. Only while Edge Guard is on and walking (not sneaking).
Checked 2026-10-12 (`b36cf7b`, normal build): on BDS the slab still went into
the player's own cell and they fell; not seen locally. The maintainer
suspects the BDS-only fall predates this change. Next: a trace build logging
`useItemOn` decisions and `buildBlock` calls, to see whether BDS placement
reaches the hook and what it decides.
Trace (VALIDATION-LOG): the check runs and refuses; one BDS fall came from a
server correction 0.04 further out than the client at 0.026 overlap, the
other from a merge whose check was not logged. Next build: the overlap margin
grows to 0.1 block and the merge check also runs in `startBuildBlock` and
`buildBlock`, before the client may ask the server to place.
Checked 2026-10-12 (`2dc7e45` trace build): no falls on BDS or locally
except one. That one: after a small server correction the client re-ran its
recent ticks and moved 0.17 block outward in one tick, off the block; the
guard seems not to apply while the client replays inputs (open, rare).
Next build: the margin becomes a setting (DESIGN.md), default 0.10.

### L-160 Edge Guard lets the player fall on BDS
Done 2026-10-12: the move vector and the sent input state are guarded, with look-ahead, partial input, gentle braking, a 0.02 block overlap margin (also while sneaking) and stepping back off a bare edge (`1153107`..`29bf8ad`); checked on the local test BDS and in a local world with trace builds (the trace only logs). Slab merges that push the player off went to L-161.
Kind: Bug, Research (strong model).
Status: open. Maintainer's report, 2026-10-12 (0.2.0-era builds): in a
local world Edge Guard holds the player at edges; on BDS the player stops
for a moment at the edge, then falls within a second. Expected since L-40:
the guard shortens the move request in `MoveCollisionSystem` on the client
(and on the integrated server's copy in a local world, `910d2eb`); a
dedicated server simulates movement itself from the client's input and
corrects the client, so the client-only guard is undone.
Next: confirm with the log/trace on BDS (correction packets after the stop);
then look for a client-side change the server also honors, such as changing
the movement input the client sends (the input vector before it is packed
into the auth-input packet) near an edge, rather than the client's resulting
speed. Sending real sneak input is not acceptable (L-40: no sneak pose,
speed loss or network sneak state). Fail open on an unverified version.
Record the server's movement settings if they are visible.
Built 2026-10-12, in-game check pending: a second hook on
`PlayerMovement::calculateMoveVector` (the vector the client simulates and
sends; move x positive to the left, per L-132) guards the local player's own
input states on the client thread. Pure `guardInput` (EdgeGuardPlan.h,
tested against a simulated server) keeps input on an axis only if the feet
stay supported along the slide that follows when it stops (ordinary friction
0.546), drops it otherwise, and reverses it when the slide alone would leave.
Jump input passes. Ice and other slippery blocks slide further than assumed.
The client-side movement-request guard stays. Test server: official BDS
1.26.51.1 at C:/Users/kenke/Documents/Lamina/bds-1.26.51.1 (outside the repo;
local, players are operators, world `lamium-edge`).
That build stopped locally but still fell on BDS: the trace showed the server
rebuilding the vector from the sent input state (direction keys, raw and
analog move), not from the packet's move (VALIDATION-LOG). Next build: when
the guard changes the vector it also rewrites the local player's input state
(direction keys cleared, the guarded vector as the analog and raw move). Not
yet checked; sprinting may end when the forward key is cleared at an edge.
Checked 2026-10-12 (`68d2211`): stops on BDS and locally; bridging was
sometimes pulled slightly back. Next build: keep the largest safe share of
the input instead of dropping it, and brake only as hard as needed.
Checked (`1870ca1`): no pull back; still fell on BDS after sneaking to an
edge and letting go (the box only touches the block, which the server treats
as no support). Next build: the input guard also runs while sneaking and keeps
0.02 block of overlap; feet only touching an edge step back onto the block;
the sent analog input undoes sneaking's 0.3 scale. Slab bridging unverified.
Checked (`c73fc16`): sneak release and slab bridging hold on BDS and locally;
sliding along a wall into a block corner still fell (box only touching the
corner, step back blocked by the wall). Next build: the movement-request
clamp keeps the same margin and runs while sneaking; step back only goes
where the body is free.

### L-155 Connected Textures action shows its translation key in search
Done 2026-10-12: labels for connectedtextures, inventoryhud and usedslots (`3767542`); checked in game in English, Japanese and Simplified Chinese.
Kind: Bug, Ready (small).
Status: open. Maintainer's actual-play report, 2026-10-12, Lamium 0.2.0.
Reproduction: search settings for `connected`; an action row displays
`key.Lamium.connectedtextures` instead of a translated label. Screenshot:
`スクリーンショット 2026-10-12 013700.png` (report attachment).
Static check: Binding.h declares action ID `connectedtextures`;
SettingsRows.h forms its `key.Lamium.` translation key, but that exact key
is absent from Translations.h and TranslationsZhCN.h. The translated feature
name is present, which does not cover the action label.
Next: add the English/Japanese/Simplified Chinese action label in matching
table order; check all registered actions for the same omission and extend
translation coverage tests if needed. Keep saved action IDs unchanged.
When built, verify search and Hotkeys in all three languages, with and
without a binding; labels must show words rather than raw translation keys.
Fixed 2026-10-12 (source): labels added for connectedtextures and, found by
the same check, inventoryhud and usedslots; SettingsRowsTests now requires an
English, Japanese and Simplified Chinese label for every non-retired action.
Post-fix runtime check pending.
The immediate missing-label fix stays here; L-158 owns systemic prevention.

### L-142 Enchanted items lost their glint in Lamium's icons (0.1.9)
Done 2026-10-11: glinting items drawn on the direct path before any batch, glint right after the icon (`70590f6`, checked on `df79b8d`). Enchanted leather's undyeable parts see-through: recorded under L-91.
Kind: Bug. Reported on Discord after 0.1.9 (maintainer's notes,
2026-10-11): enchanted swords and pickaxes show no glint in the shulker
preview. Cause (from the code; not traced): since `cbaacfc` flat items are
drawn in a shared-mesh batch, and the caller's foil call draws no glint
over a batched icon. Fix built 2026-10-11: glinting items (`Item::isGlint`)
keep the direct icon path as in 0.1.8, so the foil call draws over them
again; enchanted leather pieces then lose their undyeable layer as before
0.1.9. Applies to every Lamium icon (previews, HUD, offhand slot, settings).
In-game check pending; the shield (L-91) is unchanged.
Round 1 (`beece33`, checked 2026-10-11): still no glint in the shulker
preview. 0.1.8 drew each icon and then its glint; since `cbaacfc` all
icons are drawn first and the glints after, and the foil call apparently
covers only the icon drawn just before it (the offhand slot, one icon then
its glint, kept its glint). Round 2: `drawItemIcons` draws a glinting
item's foil call right after its own icon (`IconAt::glint`), for the
previews, the inventory HUD and the offhand slot.
Round 2 (`77ceea4`, checked 2026-10-11): still none in the preview, for a
pickaxe, an enchanted golden apple and a shield. Left as the difference
from 0.1.8: the preview now runs shared-mesh batches before the foil
calls (0.1.8 had none). Round 3: glinting items (icon, then glint) are
drawn before any batch in `drawItemIcons`. Passed on `df79b8d`
(2026-10-11): pickaxe, golden apple and book glint in the shulker preview.
Known cost: enchanted leather armor's undyeable parts are see-through
there (L-91). Done; moves to BACKLOG-DONE with the 0.2.0 release notes.

### L-141 Ghosts of two schematic placements on the same cells flicker
Done 2026-10-11: the earlier placement draws shared cells; no flicker, hiding it shows the later one's ghosts (`9c173b7`, checked).
Kind: Bug. Found by the maintainer 2026-10-11 on `9e675cc`: two placements
whose ghost blocks occupy the same cells flicker where they overlap (both
faces drawn on the same planes). Not yet known whether 0.1.9 does the same
or the L-136/L-138 split introduced it. Expected: a steady look where
placements overlap (one placement's ghost wins, decided by a stable order).
Status: built 2026-10-11, in-game check pending. The placement earlier in
the list draws a shared cell (a ghost block of it, within its shown
layers, while it is shown in this dimension); the later one leaves the cell
out, mistake marks included. The earlier overlapping placements are part of
a placement's build key, so moving, hiding or changing one rebuilds the
overlap (`schematic/PlacementOverlap.h`, tested). Known limit: the later
placement still culls its faces against its own blocks in the left-out
cells. Verification and progress are unchanged (each placement counts its
own cells).

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

### L-232 Slime chunk indicator (Bedrock slime chunks do not depend on the seed)
Kind: Ready, **done 2026-11-01**: built, checked in game (VALIDATION-LOG)
and issue #30 closed. Reported
2026-10-19 as issue #30 (the reporter offers a PR: HUD line first, overlay
separately). Chosen 2026-10-19.
Built 2026-11-01 (maintainer chose the scope that day): `SlimeChunk.h`
(rule, per-block cache, row runs; tests), an opt-in Info HUD line "Slime
chunk: Yes/No" after Chunk (Overworld only, hidden elsewhere, follows
FreeCamera), " | Slime chunk" on the Debug View block/chunk line in both
label styles, and a world map layer switch "Slime chunks" (Overworld only,
default off): a green wash with the areas' outline, the place under the
pointer names a slime chunk at any zoom, and the switch's hover text says to
zoom in when it is too far out. Drawing (2026-11-01, after the maintainer
asked to see it farther out and a rectangle per run and edge was far too
heavy at 4 GUI units per chunk): rectangles from 16 units per chunk in;
below that prepared 256-pixel images of whole squares of chunks
(`SlimeImageLevel`: 32 chunks at 8 px, 64 at 4 px, 256 at 1 px without
outline), built once and kept in 48 texture slots, 4 uploads a frame; not
drawn below 0.75 units per chunk.
Minimap and a world overlay stay out (maintainer 2026-11-01). The
`help.debugView` text is corrected in all four languages.
Check: `xmake f --slime_trace=y` hooks the game's own
`SlimeSpawnRules::checkSpawnRulesCallback` and logs (a) every spawner call
with the rule's answer for that chunk (a `MISMATCH` line when the game says
yes outside a rule chunk) and a 30 s summary with the Y range of yes
answers, (b) a probe of the loaded chunks within 4 of the player at fixed
heights, which also gives the height limit. Close #30 when the trace shows
no mismatch, a slime is seen spawning in a marked chunk, and the Y limit in
the help text matches the probe.
Premise: a Bedrock chunk's slime status is a function of its chunk
coordinates only, so the client can know it without a seed or server. This
corrects L-57's "no seed, so unavailable" and the `help.debugView` text, which
must change in all three languages once a line ships (L-57 is history; do not
rewrite it).
Claimed rule (public documentation, not Mojang source): MT19937 seeded with
`uint32(chunkX) * 0x1f1f1f1f ^ uint32(chunkZ)`, slime chunk when the first
output `% 10 == 0`. Checked 2026-10-19 only against the issue's list; the
chunks in x,z in [-5,5] are (-4,-1) (-4,2) (-3,-2) (-3,-1) (-1,0) (0,-5)
(0,-2) (0,4) (1,-2) (3,0) (3,1). Sources disagree on seed dependence (the
Minecraft Wiki says independent; a finder site implies a seed), so **the rule
is unverified in game**. Slimes spawn below roughly Y 40 (wiki; one source
says 39) at any light level; verify the boundary before drawing a column.
Decision (maintainer, 2026-10-19): first scope is the Info HUD and the Debug
View only; the issue reporter may open a PR for that. Superseded 2026-11-01:
the maintainer added the world map layer (the map now has layer switches,
L-146); minimap and world overlay remain out.
Step 1 (Ready): pure `isSlimeChunk(x, z)` in a header with tests using the
list above, plus one line in each of the two HUDs (the Info HUD line is
opt-in, default off, and cheap; follow Settings/Translations rules in
AGENTS.md, three languages). Also correct the `help.debugView` text. Do not
ship the lines until the maintainer has seen the rule match real slime
spawns in game.
Decided 2026-11-01: yes/no only; the Y limit goes in the help text once the
probe confirms it (written "below about Y 40" until then). The formula's
source is in [PROVENANCE.md](PROVENANCE.md); the reporter's code was not
copied.
Confirmed 2026-11-01: the game says yes at Y 38 and no at Y 39 for the
block under the feet, and the maintainer saw slimes on a floor at Y 38 but
not at Y 39, so the help says "below Y 40 (standing on a block at Y 38 or
lower)". The rule matched the game everywhere it was asked. One defect
seen: each row run had its own frame, so chunks above and below each other
showed a line between them; the map now fills runs and draws only the
area's outline (`slimeEdges`); seen joined in game the same day. The far-out
images were checked the same day too: no slowdown at the widest zoom, no
shift or blur where rectangles hand over to images. Minimap: L-295.

### L-293 Place on interactive blocks as if sneaking, with a toggle

Kind: Research (how to make a use act as a sneaking use), then Ready.
Maintainer request 2026-11-01: while building a machine or a build, a
chest, furnace, door or lever that is only part of it opens or toggles
by mistake when a block is placed against it. Sneaking avoids that in
vanilla but changes movement.

Decided 2026-11-01 (maintainer): an on/off setting plus a toggle key.
While on, every use on a block behaves as it does while sneaking: the
held block is placed against an interactive block instead of opening or
toggling it. An option, off by default, inverts it while on: sneaking
then opens or toggles as an ordinary use would. Off restores vanilla
completely. The maintainer named a Java mod's fake-sneak placement as
the closest prior art (PROVENANCE.md); its source is not opened.

Open (Research): where the use path reads "sneaking" for the
interactive-block decision (`GameMode::useItemOn`, the build action,
the block's own `use`), and whether that can be answered without
changing the player's real sneak state, movement or what the server
sees; on a server the placement must be confirmed from the world, not
the sent transaction. Also: whether an empty hand or a non-block item
follows the same rule (vanilla sneak use applies to all items), the
Fake Offhand's `interactSlot` and `placementSlot` must read the same
effective sneak, and the HUD/indicator for the toggle state.

Findings 2026-11-01 (SDK 26.51.8 headers and Lamium code, nothing run):
- The server decides from its own sneak state. A block use reaches it as
  an `ItemUseInventoryTransaction` (position, face, slot, item, click
  position, predicted result) with no sneak bit; `PlayerAuthInputPacket`
  carries the sneak state (`InputData::Sneaking`, `StartSneaking`,
  `StopSneaking`) and can also carry an item use
  (`PerformItemInteraction`, `mItemUseTransaction`). That also holds in
  a local world, where the integrated server keeps its own player. So a
  client-only answer (setting the local player's sneak flag around
  `useItemOn`) would predict a placement that the server turns into
  opening the container: rejected.
- `Actor::isSneaking()` is an inline read of `ActorFlags::Sneaking`, so
  there is no function to hook for the decision.
- In Bedrock an empty hand interacts even while sneaking (VALIDATION-LOG,
  L-95 sneak + chest), so with the toggle on an empty hand still opens a
  chest; the invert option matters for a held item.
- Lamium already has the two halves: Permanent Sneak
  (`PermanentSneak.cpp`) feeds `SneakDown` into a transient copy of the
  move input, which vanilla turns into a real sneak the server sees;
  the L-288 / L-292 path can hold back the native first use and replay
  it with `interaction::periodic::sendUseEdge`. Edge Guard already edits
  outgoing `PlayerAuthInputPacket`s on `LoopbackPacketSender::send`.

Two shapes, with different user-visible costs:
- Sneak pulse: a use aimed at an interactive block while the toggle is
  on is held back, sneak is fed until the local player's flag is set
  (and the auth input that carries it has gone out), the use is
  replayed, then sneak is released. Server-legitimate input; costs a
  delay of a few ticks before the placement and a brief crouch (eye
  height dips, other players see it, movement slows and an edge stops
  the player for those ticks).
- Sent state only: add the sneak bits to the outgoing auth input of
  that tick and set the client flag only inside `useItemOn`, without
  changing movement. No crouch, but the server's movement then runs
  with a sneak the client did not simulate (corrections on servers,
  anti-cheat risk); version-sensitive and only viable if a trace shows
  the server reads the sneak bit before the use in the same packet.

Next: a trace-only experiment (no behavior change): on sneak + right
click on a chest with a block, log which packet carries the use, the
client tick of the auth input that first has `Sneaking`/`StartSneaking`,
and when the local `ActorFlags::Sneaking` turns on - in a local world
and on BDS (`xmake f --sneak_trace=y`, `SneakUseTrace.cpp`).

Trace result 2026-11-01 (local world; VALIDATION-LOG): the use goes out
as its own `InventoryTransactionPacket` between two auth inputs, and the
server reads the sneak state of the auth inputs that arrived before it;
sneak pressed in the same tick as the click comes too late and the
chest opens. So either shape has to hold the use back until an auth
input with the sneak bits has been sent, which costs at least one
client tick (about 50 ms). Real sneak input sets the client flag and
`StartSneaking` in the same tick, so the sneak pulse needs about one
tick of crouch before the use plus the release after it. BDS was not
traced; packet order should be the same, and a built feature gets its
server check then.

Decided 2026-11-01 (maintainer): the sneak pulse ("Crouch briefly") is the
default method; "Send only (experimental)" is a second method to try.
Superseded the same day after the BDS checks: "Send only" is the default
and labelled recommended; "Crouch briefly" stays as the fallback for
servers that object to an edited sneak state, labelled as such. The
feature keeps its experimental badge.

Status 2026-11-01: built on main, checked in a local world (both methods
pass; the maintainer prefers "Send only"; servers unchecked). Settings
`interaction.sneakPlacement`, `sneakPlacementInvert`,
`sneakPlacementPacket` (method), action `sneakplacement` (unbound),
experimental badge, capability `SneakPlacement`. `SneakPlacementPlan.h`
(`gate`, `afterAuth`, tests in `tests/SneakPlacementTests.cpp`) holds a
use on an interactive block back in `buildBlock` / `startBuildBlock` /
`useItemOn` until an auth input with the wanted sneak state has gone
out, keeps the override three ticks after the last such use, replays a
click vanilla did not retry, and gives up after ten ticks. "Send only"
drops the sneak bits on the auth input right after the use went out
(`releaseBeforeAuth`), because each extra tick was a tick of sneak speed
on BDS (VALIDATION-LOG 2026-11-01). Unverified:
how the send-only method looks to the server's movement; invert while
Permanent Sneak or toggle sneak holds the sneak.

In game (after it is built): with the toggle on, a block placed against
a chest, furnace, door and lever is placed and nothing opens or
toggles; with the invert option on, sneaking opens the chest; toggle
off and the key restore vanilla; on a server the placed block stays.
Closed 2026-11-01: checked in game on `9a06ada2` (local world and the test BDS; the invert option with Permanent Sneak or toggle sneak) - VALIDATION-LOG
2026-11-01.

### L-292 A Fake Offhand key that is not right click uses the Fake Offhand

Kind: Ready (decided 2026-11-01 by the maintainer, after L-288). A
Fake Offhand binding that does not end on right click is the player
asking for the Fake Offhand, so it no longer yields to the primary
item: `chooseSlot` skips the primary-priority checks (`primaryIdle` for
`placementSlot` and `instantUseSlot`) for such a binding. A binding
that ends on right click keeps the priority, because the same click
also reaches vanilla.

Kept for every binding: an item in use on the primary is not
interrupted; timed items (bows, tridents, fishing rods, milk) are not
started by a per-call borrow; a firework is only an instant use while
gliding or aimed at a block (on the ground at air it has no vanilla
effect, so nothing happens).

An interactive block (chest, furnace) aimed without sneaking opens, as
vanilla does; sneaking places the Fake Offhand block on it. The
borrowed slot is selected while that edge is replayed, so the main-hand
item never acts on the block (a bucket on a cauldron).

Status 2026-11-01: closed, built in `136e04d8` and checked in game
(VALIDATION-LOG 2026-11-01).
`chooseSlot` (`FakeOffhand.cpp`) takes `dedicatedKey` (the queued press
on a binding that does not end on right click, and build ticks without a
right-click chord); `primaryYields` and `interactSlot` in
`FakeOffhandPlan.h` carry the rule, with table tests in
`tests/FakeOffhandTests.cpp`. The interaction runs through the L-288
placement hold, so the edge is released once the screen opens.

L-290 (a filled bucket in the main hand stops the Fake Offhand) then
only applies to the right-click binding.

In game, on a single non-right-click key: a block in the main hand and
a snowball or a bucket in the Fake Offhand - the offhand item is used;
a block in both - the Fake Offhand block is placed; a chest aimed
without sneaking opens, with sneaking the Fake Offhand block is placed;
the right-click binding is unchanged.
No release target selected.


### L-286 Show the FreeCamera heading on the minimap with the world map's arrow
Kind: Ready. Maintainer request 2026-10-31 after the L-253 check: only the
world map marks where FreeCamera looks (the green arrow), so the minimap
follows the camera without showing which way it faces; adopt the world map's
camera arrow there.
Decided: the minimap draws the same green arrow (`cameraArrowColor` in
MapImage.h) at its center while FreeCamera owns the view, turned with the map
the way the player arrow is, so a north-up map shows the camera's bearing and
a heading-up map keeps it at the top. No setting; the arrow exists only while
the map follows the camera. The player arrow keeps its own place and color,
and the world map is unchanged.
Check in game: with FreeCamera flying, the green arrow at the center points
where the camera looks, in the ordinary north-up view and with the rotating
map; turning the camera turns it, stopping FreeCamera removes it and leaves
the player arrow.
Built 2026-10-31 (`b2f563e4`): the snapshot marks the followed point as the
camera, the map overlay carries the camera flag and angle, and the arrow is
drawn into the map texture like the player's.
Done 2026-10-31: checked in game by the maintainer on `0580f7ae` (DLL SHA-256
`0580f7ae4c3e4e72976f35eb2463b970ac5000dab4c67b2a72c666b8997ed52a`, local
world): every point passed, also with other map sizes and ranges. The same day
the maintainer found the minimap arrows hard to read at small sizes; the look
was reworked on branch `l286-minimap-camera-arrow` in four passes and given up
without merging them. The camera arrow itself was cherry-picked to main as
`a5cae157` and keeps the minimap's existing in-texture arrow drawing. The
remaining legibility work is L-287.

### L-253 Make the minimap cave view a persisted three-way option the key cycles

Kind: Ready. Rewritten 2026-10-30. The original text (maintainer note
2026-10-24, decided 2026-10-26) said the minimap switches by itself and
only the world map has a manual cave on/off, and decided a three-way
setting with "No key". That premise was wrong: the maintainer had
forgotten the shipped MinimapView key. Current main already forces the
view from a key (`pressViewKey` / `ViewForce` in MapCave.h and
Minimap.cpp, the `caveView` settings row with no switch, toasts
`mapView.cave` / `mapView.surface` / `mapView.auto`; MAP.md documents
it). What is actually missing is persistence and a settings surface:
the force is an in-memory atomic, not a Settings value, the row shows
no state and offers no choice, and FEATURES does not mention the key.
The three-way decision stands; the "No key" part is reversed.

Decided (maintainer 2026-10-30):

- One state, two entry points. A new persisted field in `Settings::Map`
  holds the view mode: Automatic / Surface / Cave, default Automatic
  (today's behavior). The in-memory `viewForce` goes away; the minimap
  reads the setting. `applyForce` semantics are unchanged (a chosen
  mode wins over the automatic choice), and Cave keeps the cave view at
  the height the automatic mode would use. The world map's opening
  choice (`automaticView`), cave recording, and the FreeCamera
  reference for the followed point are unchanged.
- Settings placement: the `caveView` feature row is removed. The mode
  becomes a choice option under the Minimap parent, after Round and
  before the debug-hide option, shown as a stepper like the map zoom
  and waypoints-world choices ("Cave view: < Automatic >"). Cave view
  is not an independent feature — with the minimap off the key already
  does nothing — so it does not keep a feature row of its own.
- Key: the MinimapView binding moves to that option row (the
  option-key precedent: `map.radarFaces`, `map.waypointsWorld`). The
  key cycles the same setting Automatic → Surface → Cave → Automatic,
  saves immediately like every settings change, and toasts the new
  mode with the existing `mapView.*` strings. `pressForce` ("force
  the view not shown, press again for automatic") is replaced by the
  plain cycle; finding a mode can take two presses, accepted because
  the settings row and the toast always name the current mode.
- Texts: `help.caveView` is rewritten (it currently describes the old
  key behavior) and the new option label and mode names land in all
  four languages; FEATURES gains the option and the key in the Minimap
  section; the MAP.md line about the key is updated to match.
- Tests: the MapTests `pressForce` cases become cycle cases (order,
  wrap-around, forced mode beats automatic).

Check in game: cycle from the key and from the settings row above and
below ground in the Overworld and in the Nether (a forced Surface in
the Nether included), the mode surviving a restart and a world
rejoin, and the key doing nothing while the minimap is off.
Status 2026-10-31: built (`9a4e0ff7`). The
in-memory force is gone: `Settings::Map::caveView` holds Automatic /
Surface / Cave, the minimap reads it every frame, and both the key and
the stepper walk the same order. `viewForceOf` reads a corrupt value as
Automatic. The key sits on the new `map.caveView` row under Minimap
(after Round, before Debug View hiding), and its label now says "Cycle
the minimap view"; `help.map.caveView` replaces `help.caveView`, and the
`feature.caveView` strings are removed.

Done 2026-10-31: checked in game by the maintainer on `9a4e0ff7` (DLL SHA-256
`667A6236848FBADA3D86A75ACBC93F76467CDB1F018597862DAC907D38619A97`, local
world): every point of the check list was as expected, and the session log
held no settings-save or cave view error. See VALIDATION-LOG.

### L-280 Auto Elytra: put the elytra on after a Riptide launch only when the player can fly
Kind: Ready. Maintainer report 2026-10-30 after 0.3.1: Riptide is used
underwater to fight mobs, and Auto Elytra swapped the chestplate for an elytra
at the launch. Under water the elytra can never glide, so the swap only took
the chestplate away.
Decided (maintainer 2026-10-30): a launch arms a wait instead of swapping; the
elytra goes on the first tick the player is airborne and out of the water. The
wait ends once the spin plus a two-second coast is over, or half a second after
the launch stopped both spinning and lifting, and it clears on a pause, a
dimension change or once the elytra is seen worn, so a refused swap is retried.
A launch in rain or from land is airborne a tick or two later. The earlier
"swap at the launch, undo it in water" rule was replaced before it shipped.
Help text in all four languages.

Status 2026-10-30: built on `l280-riptide-water-revert`, PR #57, merged in
`99145be3`.
Done 2026-10-30: checked by the maintainer on `f63cf36d` (DLL SHA-256
`F0FDCAAA94A5EA33C12367C7FEAD3608669EE2822538A38104AAA1A812264BAA`),
`1.26.51.01` LIP, release build: a launch in water does not swap; a launch
that breaks the surface equips once airborne; a launch in rain equips as
before; a spin with the elytra on restarts the return delay; firework jump and
key unchanged.

### L-281 Auto Elytra: landing in water starts the return delay
Kind: Ready. Maintainer report 2026-10-30 after 0.3.1: an elytra glided into
water never came off until the player reached ground, because the return delay
ran only from a ground landing.
Decided (maintainer 2026-10-30): landing in water counts as a landing, so the
configured "chestplate back after landing" delay applies there too, and a glide
that restarts within the delay still keeps the elytra. The delay row's help
text says water is included, in all four languages.

Built with L-280 in one change: `elytraStep()` takes water as a landing.
Status 2026-10-30: merged in `99145be3`.
Done 2026-10-30: checked by the maintainer on `f63cf36d` (DLL SHA-256
`F0FDCAAA94A5EA33C12367C7FEAD3608669EE2822538A38104AAA1A812264BAA`),
`1.26.51.01` LIP: gliding into water returns the chestplate after the
configured delay instead of waiting for ground.

### L-246 Option to trigger Auto Elytra with Fake Offhand fireworks
Kind: Ready. Maintainer idea 2026-10-22 after 0.3.0. The firework-jump
trigger in `src/features/interaction/ElytraSwap.cpp` checked only the
selected main-hand hotbar slot (`holdingFireworks()`); a rocket assigned to
Fake Offhand did not qualify.
Goal: equip an elytra on jump when Fake Offhand holds fireworks, even if the
main hand holds another item, keeping the main-hand behavior and the
chestplate return after landing.
Decided (maintainer 2026-10-26): no new setting. The existing Firework jump
trigger also fires when Fake Offhand is on and its slot holds firework
rockets, whatever the main hand holds; with Fake Offhand off, or its slot
empty or not rockets, nothing changes. An equipment trigger only: no firework
is used and no glide is started (L-71), and the Firework jump help text
changes in all four languages.

`ElytraSwap.cpp` reads the main hand and, while the feature is on, Fake
Offhand's target slot; `jumpHasFireworks()` (`EquipmentPlan.h`) is the pure
decision and `tests/EquipmentPlanTests.cpp` covers it. The trigger follows
the setting, not the `FakeOffhandTargets` capability: that capability pauses
only the target-specific main-hand checks in `targetUse()`, while the slot's
own instant uses (fireworks while gliding among them) still work, so counting
the slot stays consistent (reviewed 2026-10-30; see
[FAKE-OFFHAND.md](FAKE-OFFHAND.md)). The tick reads the settings through
`Runtime::snapshot()` instead of copying them.

Status 2026-10-30: built on `l246-elytra-fake-offhand`, PR #56, merged in
`f88a8a8b`.
Done 2026-10-30: checked by the maintainer on `447449a` (DLL SHA-256
`0EAF85C678C21435D072AE067507475CF68B84AA2619E8A5B9F14E24199E3634`),
`1.26.51.01` LIP, release build: a main-hand rocket still equips the elytra;
with Fake Offhand on and a rocket in its slot the elytra goes on with any
main-hand item, the rocket is not used and no glide starts; a second jump
glides and the slot's rocket works; an empty or non-rocket slot, or Fake
Offhand off, equips nothing; the chestplate returns a few seconds after
landing; the new help text shows in Japanese and both Chinese languages.

### L-255 Connected Textures: edges on the inside of glass pane corners
Kind: Ready. User report 2026-10-23 after 0.3.0, with a screenshot: at an
L-shaped glass pane corner the outside has its edge but the inside has no
seam. L-208 (2026-10-18) kept the inside merged on purpose. Maintainer
2026-10-26: the inside edge looks good enough to offer; put it under the
existing "Edges at glass pane corners" option
(`visuals.connectedPaneCorners`, default off), no new setting.
Goal: with that option on, an L-shaped pane corner draws a border on the
inside of the turn as well as the outside; with it off, nothing changes.
Notes: `ConnectedTexturesHooks.cpp` (`wantPane`) and `ConnectedSprites.cpp`;
the inside faces must not stretch (the L-208 outside fix moved the whole
two-texel window). Rewrite `help.visuals.connectedPaneCorners` in all four
languages (it says the inside stays joined). Check in game: L corners in
both directions, T and cross junctions, stained panes, option off.
Built 2026-10-27, not yet checked in game: with the option on, the post's
outside faces keep their edge (L-208) and the faces on the inside of the turn
gain one, so the turn shows an edge on both sides. The inside faces are the
arms', not the post's: in an east+north L the post's east face (x = 9/16,
z in 7/16..9/16) and north face (z = 7/16, x in 7/16..9/16) are exactly the
joints the arm boxes cover, so sliding them changed nothing on screen
(review, 2026-10-27). What a player sees from inside the turn is the east
arm's north face (z = 7/16, x > 9/16) and the north arm's east face
(x = 9/16, z < 7/16), which belong to the arm parts. `paneCornerFor(part,
x0, x1, z0, z1, parts)` now decides which corner a quad takes - outside for
the post's own faces, inside for the arm face at the corner on the axis the
arm does not run along, nothing otherwise - and is tested for all four L
orientations and every part, so a test says which face takes which corner.
The slide still moves one quad's whole u window by one delta: a post face
keeps its two texels and an arm face its own slice of the tile (seven texels
here), so nothing stretches. The post's four faces now take only the outside
corner, which leaves the L-208 look as it was. With the option off no parts
or corner geometry is computed. `help.visuals.connectedPaneCorners` says
both sides are drawn, in all four languages.
In-game check still owed: an east+north L and an east+south L seen from
inside the turn must each show a vertical border line at the corner on both
faces; the outside of each turn must look as it did with L-208; a straight
run, a lone pane, a T and a cross must not change; with the option off the
inside shows no line. The inside line is the tile's own one-texel frame at
the corner end, while the outside edge fills the post's two texels; if the
inside line reads too thin, or a face shows the border at its far end
instead of at the corner, that is the thing to look at.
Note 2026-10-27 (L-270): the inside half now also follows the inside-corner
option (`visuals.connectedCorners`), so the line described here needs both
options on; the outside half still needs only the pane option. The inside
checks above assume both are on, and with the inside-corner option off the
corner must look as L-208 left it.
No release target selected.
Done 2026-10-27: checked by the maintainer on the merged build `c5dcb63c` (DLL SHA-256 `8B3CF63DA8173317A99EA3DBD1011069CF4172DB667E0A6EAEA1A84DAD70FFE2`): the outside edge reads as L-208 and the inside edge draws without stretching, in both L directions. L-270 later put the inside edge behind the inside-corner option as well, and L-271 extended it to T and cross junctions.

### L-251 Choose whether Hand Restock refills shulker boxes and buckets
Kind: Ready. User request 2026-10-23 after 0.3.0: a setting to choose
whether Hand Restock also refills shulker boxes and buckets. Today a placed
shulker box is refilled like any block, and a last water/lava/milk/powder
snow bucket that empties is exchanged with a reserve
([HAND-RESTOCK.md](HAND-RESTOCK.md)); neither can be excluded.
Goal: let the player keep these items out of Hand Restock when refilling
them gets in the way.
Decided (maintainer 2026-10-26): two child switches under Hand Restock,
"Shulker boxes" and "Buckets", both on by default (today's behavior). Off
for shulker boxes: a placed or used shulker box is not refilled. Off for
buckets: a bucket that empties (water, lava, milk, powder snow) is not
exchanged with a reserve. Other remainders (potions, stews, honey bottles)
are unchanged. Settings, option rows and help in all four languages; pure
planner tests in `RestockPlan.h`; check in game. No release target
selected.
Built 2026-10-27, not yet checked in game: two child switches under Hand
Restock, "Shulker boxes" (`inventory.restockShulkerBoxes`) and "Buckets"
(`inventory.restockBuckets`), both on by default. Off for shulker boxes keeps a
placed or used shulker box out of Hand Restock; off for buckets keeps an emptied
water/lava/milk/powder-snow bucket instead of exchanging it with a reserve.
Filling a bucket from a source, the other remainders (potions, stews, honey
bottles) and the master switch are unchanged. The decision is
`restockFamily`/`restockFamilyEnabled` beside `planRestock` in `RestockPlan.h`,
table-tested per family and per switch combination. To check: place a shulker
box and pour a water bucket with each switch on (both refill as before), then
with "Shulker boxes" off (no refill) and "Buckets" off (the empty bucket
stays); a potion or stew still swaps its container with either switch off.
Done 2026-10-27: checked by the maintainer on the merged build `c5dcb63c` (DLL SHA-256 `8B3CF63DA8173317A99EA3DBD1011069CF4172DB667E0A6EAEA1A84DAD70FFE2`). Both switches on behave as before; "Shulker boxes" off stops the refill; "Buckets" off leaves the empty bucket for water, lava, milk and powder snow while filling from a source is unchanged; potions and stews still swap their container; both settings survive a world reload.

### L-266 Schematic follow-up batch (0.3.1)
Kind: Ready (umbrella). Maintainer decision 2026-10-26: the schematic
requests that piled up after 0.3.0 are done together instead of one by one,
so the Schematics settings and the four languages are reviewed once. The
L-items stay the specifications; this item holds the order and the scope.
Release: 0.3.1, together with other work chosen later (for example L-255
pane corner inside edges and the other requests from the same Discord
thread); 0.3.1 ships only after more of that is done.
Stages, in order:
1. Settings and ghost drawing: L-252 hide the mistake marks, L-254 "None"
   ghost outline width.
2. Check rules: L-262 rule table with the signal, lit/unlit and growth
   switches.
3. The aimed ghost: L-264 schematic water in Target View, L-263 pick the
   aimed ghost's block.
4. Performance, research only: L-256 measure and find the cost; whether to
   change anything is decided from the results (L-236 is related). Done
   2026-10-26 (BACKLOG-DONE).
Out of the batch: L-183 (reconsider after L-262), L-184, L-185, L-186,
L-265, L-114 to L-116, and the parked or waiting bugs (L-177, L-192, L-193,
L-245).
In-game checks are collected into one checklist at the end of the batch
where possible.
Status 2026-10-26: stages 1-3 built and pushed, not yet checked in game:
L-252/L-254 `d70676e4` (a "Check" heading under Schematics holds the new
switches), L-262 `53035913` (`CheckRules.h`; the rules apply to the check,
the Check list's state rows and the mistake marks), L-264 `0f06b820` (ghost
search passes through real water and lava; flow rows; "Target water and
lava" under Target Info, off), L-263 `a1e629e7` (`GhostPick.cpp` hooks
`LocalPlayer::pickBlock`; `inventory::bringToHand` is the shared part).
Left open by these: L-264 does not yet show a ghost's waterlogged second
layer. Stage 4 waits for a `--schematic_perf_trace=y` measurement after the
normal checks pass.
In-game check 2026-10-26 (maintainer, `887ae23a`): the Check heading, mistake
marks, None outlines, the signal switch, ghost water, real liquids and the
ghost pick passed; plant growth not checked (no way to set a cactus's age
was at hand: `/setblock ~ ~ ~ cactus ["age"=5]` sets one). Follow-ups
decided the same day:
- The on/off switch no longer accepts the other form as correct: off it is
  a wrong block (red, as before), on it is a wrong state (yellow) with an
  "On" row; there is no ignore for it (L-262).
- Lava rows count its own steps: three blocks outside the Nether
  (liquid_depth 0, 2, 4, 6), seven in it (L-264).
- In creative, picking a ghost whose block is not carried gives the block
  like vanilla, always (no setting); how to do that so servers accept it
  is Research (L-263).
Checked in game 2026-10-26 (VALIDATION-LOG "L-266 schematic batch"):
L-252, L-254 and L-262 done and moved to BACKLOG-DONE; L-263 (creative
pick of a block not carried) and L-264 (waterlogged ghosts) keep their open
parts; stage 4 is next.
Decided later the same day (maintainer): Mistake marks goes into the
menu's Show category, and the HUD switch moves to Screen (both stay at six
items); every Check switch gets a bindable action, unbound by default; the
three rules stay out of the menu (an L-238 exception). The on/off rows are
named after the Java Edition states: "Lit" for lamps and torches,
"Powered" for repeaters and comparators.

Done 2026-10-31: the batch closed with the 0.3.1 release (2026-10-27), as BACKLOG.md's execution order already recorded; the item is moved here to complete the bookkeeping. L-263, which keeps an open part, stays in BACKLOG.md; L-264 was closed 2026-10-26 and L-265 was out of the batch from the start.

### L-252 Hide the schematic mistake marks in the world
Done 2026-10-26 (L-266 batch): `d70676e4`, in the menu's Show category since `c37f6937`; passed in game.

Kind: Ready. User request 2026-10-24 after 0.3.0: an on/off for the
schematic check (照合). Today the check runs whenever Schematics is on and
its mistake marks (red/yellow boxes and outlines) are drawn in the world
(`GhostMesh.cpp`, `GhostVerify.cpp`); the only related setting is the
Schematic HUD's check counts (`schematic.hudVerify`).
Decided (maintainer 2026-10-26): first a switch "Show mistake marks" under
Schematics, on by default. Off: the red/yellow boxes and their outlines are
not built or drawn; the check itself still runs, so the Check tab, the HUD
counts and the materials are unchanged. The reporter is being asked about a
fuller check on/off, which was tracked as L-258 (closed 2026-10-26, answered by L-262 to L-264). Settings, option
row and help in all four languages; check in game with mistakes in view.
Release target: 0.3.1, batch L-266.

### L-254 Turn the light-blue schematic ghost outlines off
Done 2026-10-26 (L-266 batch): `d70676e4`; passed in game.

Kind: Ready. User request 2026-10-24 after 0.3.0: an on/off for the
light-blue ghost outlines. The outline distance setting
(`schematic.outlineDistance`, 0-192 in steps of 16, default 48) is the only
control, and 0 means no limit, so the outlines cannot be turned off; the
smallest limit is 16 blocks (maintainer reply 2026-10-24: worth a look).
Goal: allow ghosts without outlines while keeping the distance limit.
Decided (maintainer 2026-10-26): add "None" to the existing outline width
choice (`schematic.ghostLines`: thin, normal, thick), so no new setting.
Saved values keep their meaning (append the new value; settings format
stays compatible). With None, the light-blue ghost outlines are not built
or drawn; the red/yellow mistake marks and their outlines stay. The outline
distance row stays as it is. Labels and help in all four languages; check
in game. Release target: 0.3.1, batch L-266.

### L-262 Schematic check: ignore redstone signal, lit/unlit variants and plant growth
Done 2026-10-26 (L-266 batch): `53035913`, on/off forms as a wrong state `0ce689e6`, Lit/Powered rows and keys `c37f6937`; passed in game, cactus growth included.

Kind: Research (state lists), then Ready. User feedback 2026-10-26 (Discord
thread by the L-252/L-258 reporter):
the check should be able to ignore signal state and plant age, and redstone
blocks whose on and off forms are different blocks count as wrong blocks.
Decided (maintainer 2026-10-26): three separate switches in Lamium
Settings under Schematics, global (not per placement), all off by default:
1. Ignore redstone signal: differences only in signal/power states are not
   mistakes.
2. On and off forms as a wrong state (changed 2026-10-26 after the in-game
   check): block pairs that differ only by being on (redstone lamp,
   repeater, comparator, redstone torch) are a wrong state (yellow, with an
   "On" row) instead of a wrong block (red); never correct.
3. Ignore plant growth: growth/age states (crops, saplings, cocoa, sugar
   cane, cactus, bamboo, kelp, vines, nether wart and the like) are not
   compared.
An ignored difference counts as correct everywhere: Check list, HUD counts,
nearest mistake and the red/yellow marks. The strict comparison stays the
default. This is not L-183 (suppressing chosen mismatches); L-258 was
closed as answered by this report.
Decided (maintainer 2026-10-26, granularity): one general mechanism, a
pure rule table of "state keys not compared" and "block names that compare
as the same block", each rule belonging to a category; the three switches
turn categories on. A later category is a table entry, not new code. No
user-editable rule list until there is a need for one.
Research first: the exact state keys and block-name pairs from the SDK /
block data (pure tables in `features/schematic/`, with tests), and whether
open/extended states (doors, trapdoors, pistons) count as "signal" (ask the
maintainer before including them). Settings rows and help in all four
languages. Release target: 0.3.1, batch L-266.

### L-261 Temporary waypoints
Done 2026-10-26: `daba6e9b` (prompt switch, arrival removal, one-click
delete, hollow marks, `map.waypointArrival` setting) and `03695bda`
(Temporary switch in the Waypoints screen and world map panel, one hollow
size rule for every view) passed in game the same day. Added beyond the
decisions: a temporary waypoint the player is already near counts only
after the player has moved away, so it does not vanish where it is
added. Servers and old waypoint files are unchecked.

Kind: Ready. Maintainer request 2026-10-26:
waypoints today are long-lived marks; add throwaway ones that disappear on
arrival or can be removed without the usual confirmation.
Decided (maintainer 2026-10-26):
- A waypoint is either normal or temporary. The existing add prompt gets a
  normal/temporary switch; it opens with the kind chosen last time (kept
  per world beside `lastColor` in the waypoint set), so placing several
  temporary ones in a row needs no switching. No separate add key.
- A temporary waypoint is removed when the player comes within a set 3D
  distance of it (height included). The distance is a setting; 0 turns
  automatic removal off.
- Deleting a temporary waypoint in the list takes one click; normal ones
  keep the two-click confirmation.
- Temporary waypoints are saved with the world like normal ones until they
  are removed. The waypoint file gains one field; files without it load as
  normal waypoints.
Also decided (maintainer 2026-10-26): arrival distance default 6 blocks
(about the reach distance), range 0-32;
temporary marks drawn so they are told apart from normal ones (in the
world, on the minimap and map, and in the list) with the existing palette;
only waypoints in the current dimension are removed by arrival; FreeCamera
does not count as arriving (the body's position is used).
Notes: `features/map/Waypoints.h` (data, pure arrival test with tests),
`WaypointStore.cpp`, `WaypointSession.cpp`, `ui/WaypointsView.cpp`,
`WaypointPromptLayout.h`; setting row and help in all four languages.
Check in game: add both kinds, the prompt remembering the kind, arrival
removal, one-click delete, rejoin keeps temporary ones, an old waypoint
file. No release target selected.

### L-241 Pre-0.3.0 review: settings, map and schematic fixes
Closed 2026-10-26 (backlog cleanup): A1 and A2 implemented and checked in
game; check (c) moved to BACKLOG's Pre-release checks (Gaps carried).
Kind: Ready (strong model; decided with the maintainer 2026-10-22). A1 and A2
implemented and checked in game on `fca2ef9` (2026-10-22). Two
independent reviews of `v0.2.3...main` before 0.3.0; every finding below
was checked against the code. Implement in two Codex batches (A1 settings
and map, then A2 schematics and docs), each with tests and docs, then the
maintainer's in-game check. L-242 follows after this item.

Status: A1 implemented (items 1-5); in-game checks (a) and (b) passed on
`fca2ef9`, (c) not checked. A2 implemented (items 6-14); in-game checks
(d)-(f) passed. Build, `LamiumTests` and `LamiumNativeTests` pass.

**A1. Settings and map**

1. **Settings type errors wipe every setting** (since before 0.2.3).
   `decodeSettings` (`src/settings/SettingsStore.cpp`) reads sections with
   `data.at(...)`/`value(...)` and no type checks, so `{"camera":true}` or a
   string where a number belongs throws; `Runtime.cpp` then loads defaults
   and the next save's `merge_patch` overwrites bindings and the HUD layout.
   Fix: a section that is not an object is skipped; a field of the wrong
   type keeps its default (a small helper, no exceptions); other sections
   and fields still load. If the file cannot be parsed at all, rename it to
   `settings.json.broken` (replacing an older one) before writing defaults,
   and log it. Tests in `SettingsStoreTests`: wrong-type section, wrong-type
   field, the rest kept, bindings kept.
2. **World map empty optional read** (`WorldMap.cpp`, the
   `index >= 0 && ... == image->version` line): require `image` before
   reading it; drop a slot whose image is gone.
3. **FreeCamera cave recording** (`Minimap.cpp` `record()`; decided: follow
   the **camera**): the band and the cave/surface choice already use the
   followed point (`view->y`, the camera while FreeCamera owns the view);
   make the recording centre the same point (`view->x/z`, not
   `playerX/Z`) so band and area agree. Never record a cave band outside
   the dimension's build range (Overworld -64..319, Nether 0..127; End
   none): `caveLayer`/`layerFolder` callers skip it, and `layerOfFolder`
   rejects out-of-range folders. Tests for the range rule. Docs: MAP.md says
   the recording follows the camera while FreeCamera owns the view.
4. **Session-only worlds spill to a temporary folder** (decided): a world
   without a map folder (`root` empty in `MapStore.cpp`) saves its regions
   like any other world, but under a per-session temporary folder inside
   the mod's data folder (for example `maps/.session/<n>/`), so far
   residents are evicted as elsewhere and reloaded on return. The folder is
   deleted on world exit, and leftover `.session` folders are deleted at
   start. Listing, the settings' size and clearing treat it as that world's
   map.
5. **Save failures by kind** (decided): `encodeRegion2`'s section-limit
   error, Refused files and invalid data are permanent: log once per region
   per session, do not requeue the conversion, do not retry the save, and
   let the region be evicted (its file untouched). I/O failures are
   transient: retry with a growing delay (10 s, 20 s, 40 s, ... up to
   5 min). A resident that is dirty only because of a permanent failure no
   longer blocks eviction. Tests for the classification and the backoff.

**A2. Schematics and docs**

6. **Move-target choice ignores the dimension**: `actions::setTarget`
   (`SchematicActions.cpp`) checks only existence; use the same
   `validity(player)` as the target cycle (same dimension) and toast
   `schematic.toast.targetUnset` when the target is not valid.
7. **A transient schematic read failure never recovers** (from L-237):
   `load()` caches a failure with the file time, so a file another program
   held open stays failed until its time changes. Cache read, size and I/O
   failures without the time (retried on the next two-second check); keep
   the time only for parse failures. A `file_size` error says "could not
   read the file", not "file too large".
8. **Schematic HUD shows stale counts** after a layer or move change:
   `drawSchematicHud` (`InfoHud.cpp`) treats a `stale` result as ready;
   show it as counting, like the Check tab.
9. **Menu "previous placement" from no selection** skips one (the
   Placement stepper in `SchematicActions.cpp`): reuse the key path's start
   rule (from none, Previous starts at the last placement).
10. **Look-select rewrites schematics.json** even when the placement is
    already selected: the `session::change` callback returns whether the
    selection changed.
11. **Menu key line**: `schematic.menu.key` reads "Key: A / B"; use
    "Keys: {}" / "キー: {}" / "按键: {}" (one placeholder each).
12. **Chinese Edge Guard help** names the margin row differently from the
    row itself (`TranslationsZhCN.h`): use the row's own name.
13. **Docs and help after the badge review**: FreeCamera's help still
    starts with "Experimental:" in en/ja/zh: remove the prefix. DESIGN.md:
    the transfer contract line says on by default (now off for new
    installs; a saved on stays on); the badge note's "no Experimental
    feature is on by default" is wrong (Radar and Waypoints are on inside
    the Experimental Map): narrow it to the features changed in 0.3.0.
    Remove leftover "experimental" wording for the five un-badged features
    in CAMERA.md, HAND-RESTOCK.md and EQUIPMENT.md. Default keys: add
    Alt+Wheel (move target away/closer) to SETTINGS-KEYMAP.md, FEATURES.md
    and README where they say no schematic key is bound; fix SCHEMATIC.md's
    "centered at full size by default" (menuSmall is on) and "all keys
    unbound". docs/README.md: SCHEMATIC-CONTROLS.md is implemented, not a
    design-only note.
14. **Version**: 0.3.0 in `xmake.lua`, `tooth.json` and the README (the
    release itself stays the maintainer's step).

Done when: build, `LamiumTests` and `LamiumNativeTests` pass per batch.
In-game check by the maintainer: (a) a settings file with `"camera": true`
loads every other setting and keeps the bindings; (b) FreeCamera flown high
above the Nether creates no `nether/y*` folders outside 0..127, and caves
under the camera are recorded; (c) a server without map storage keeps its
map while moving far and back, and the temporary folder is gone after
leaving; (d) "Move area →" with corners in another dimension says there is
no target; (e) a schematic file held open by another program loads once it
is released; (f) the Schematic HUD shows counting right after a cut change.

### L-242 Map and minor follow-ups from the pre-0.3.0 review
Closed 2026-10-26 (backlog cleanup): implemented and checked in game on
`2900d1dc`; the render-thread `BlockSource` read became L-257.
Kind: Ready, after L-241 (maintainer 2026-10-22: fix these too, after
L-241's map changes). Lower-risk findings: cave image focus keyed by exact
height (`MapStore.cpp`/`WorldMap.cpp`; quantize to the focus slack so
FreeCamera does not rebuild every LOD per block); the world map worker
keeps rebuilding the last view after the map closes (`close()` clears
`wanted`); `recordCaveGrid` missing from the minimap eviction centres; a
data race on `logged` in `MapStore.cpp` (make it atomic or worker-owned);
cave folders for dimension 3+ written but not listed (`layerOfFolder`);
`surfaceColumn` reading above `maxY`; `Lmr2Fixture.py` always writes the
COLR high plane (wrong for palettes of 256 or fewer) and CI does not run
it (add a CI step); `spectatorTerrainWanted` is tested but is not the gate
in use (test the real gate); untested `TargetPick.h` branches; the Target
block HUD line missing the camera tag in Freelook/FreeCamera, and the
target card and that line running the same ray twice; the default
Alt+Wheel binding drawn as one clipped key cap in the key settings; opener
chords not seeded for the screen-opening schematic keys. Docs: MAP.md's
L-60 link, `L-232` comments that mean L-235 (`KeyLabel.h`), VALIDATION.md's
stale "Check tab layout to be redesigned". Research, not a blind fix: the
FreeCamera culler hook now reads `BlockSource::getBlock` on the render
thread; confirm its thread safety before changing it.

Status: implemented 2026-10-22 (all listed code and docs fixes except the
culler research). The LMR2 cross-check now runs as a C++ raw-section
comparison against the Python golden file, so CI needs no Python dependency;
the Python verifier stays manual. Build, `LamiumTests` and
`LamiumNativeTests` pass. In-game checklist 2026-10-22 (`2900d1dc`, DLL
`E4847CC5…`): keycap display, map open/pan/zoom/close and the Target block
line/camera tag passed. The legacy unavailable display, unknown mode and
map-clear/session edges were not in the shortened check. The renderer-thread
`BlockSource` read remains the open research item; it was not changed
without runtime proof.

### L-206 World map recording range is a fixed radius instead of the view distance
Closed 2026-10-26 (backlog cleanup): implemented; the remaining runtime
coverage is tracked under BACKLOG's Pre-release checks (Gaps carried).
Kind: Ready, implementation built; runtime coverage pending. User report
2026-10-17; maintainer 2026-10-17: fix it, then
approved PR #13's range rule and merging after review fixes and main sync.
Implemented: `record()` follows the client's view distance, retaining the
384-block width as a floor and the reported maximum (at most 96 chunks) as
a ceiling. No new settings rows. Display retention and time budgets stay
bounded. L-210 replaced the earlier resumable ring sweep with a dense scan
grid, bounded picking and alternating near/never-seen/retry work; L-225 and
L-226 fix whole-rescan retries and cache retention around a far FreeCamera.
The scheduler is pure and tested, including small budgets and resource
waits. Current implementation and the 96-chunk local results: MAP.md and
VALIDATION.md; the older ring-sweep description is superseded.
Remaining: in-game recording coverage and frame times at 32/64/96 chunks,
with the minimap off as well as on; near changes and movement during a wide
pass. Build/tests alone do not establish these runtime results.

### L-239 Leaving "All" returns to the last layer mode
Kind: Ready. Closed 2026-10-22: the revision was implemented by Codex,
reviewed and checked in game on `fce4b99` (DLL `743C0B2C…`). The first
version (`5a6f8ef`, not checked in game) is superseded by the revision below.

**Revision (maintainer 2026-10-22, supersedes "Decided" and step 1's
recording rule below).** Recording the mode left for All is unpredictable:
the mode stepper cycles All -> Only -> Up to, so reaching All from Only by
the forward step passes Up to and remembered it, while the back step did
not. New rule: leaving All returns to the mode in which the cut, Keep or
Axis was **last changed** (Only by default). Modes merely passed through or
viewed are not remembered.

- `Placement.h`: remove `setLayerMode`; mode changes assign the mode again.
  Add `noteLayerEdit(Layers&)`, which sets `lastShown = mode` when the mode
  is Only or Up to and does nothing under All. `leaveAll` is unchanged.
- Call `noteLayerEdit` after every cut, Keep or Axis change, wherever it is
  made: `applyLayerStep` (Axis, Keep, Cut steps; keys share it), Layer
  up/down, Cut here, and the screen's Axis, Keep, Cut +/-, typed cut and
  "Match the layer you are in". A cut operation that leaves All therefore
  notes the restored mode (no change). Edits made while the mode is All do
  not change the memory.
- Tests replace the stepper-recording ones: Only with a cut edit, then the
  mode stepper forward through Up to to All, then leaveAll -> Only; Up to
  with a Keep or cut edit, then All -> Up to; an Axis edit counts; edits
  under All do not count; a mode only passed through does not count; the
  default is Only; drawKey and the placement document still ignore the
  memory.
- Docs: SCHEMATIC.md and SCHEMATIC-CONTROLS.md say "the mode in which the
  cut, Keep or Axis was last changed".
- In-game check, replacing (a)-(d) below: (a) in Only move the cut, step
  the mode forward through Up to to All, then Cut up: back to Only;
  (b) in Up to change Keep or the cut, switch to All, Cut here: Up to at
  the view's layer; (c) the same from the Schematics screen with Cut + and
  a typed cut; (d) after rejoining the world, leaving All gives Only.

The original record follows.
Decided with the maintainer 2026-10-22. Previously a cut change made
while the layer mode was All switched the mode to Only (L-234): the first Cut
step from the menu or the Layer up/down keys only switched, and Cut here set
the cut and switched. Users who worked with Up to then had to change the mode
back each time. The Schematics screen was also inconsistent: its Cut +/- and
typed cut kept All, and only "Match the layer you are in" switched.

Decided: leaving All returns to the mode used before All (Only or Up to);
Only when there is none. The memory lasts for the world session; the
settings file format does not change.

Do:

1. **Pure rule** in `src/features/schematic/Placement.h`, tested in
   `tests/SchematicTests.cpp`: `Layers` gains `LayerMode lastShown =
   LayerMode::Only`, not saved (`PlacementStore.cpp` does not write or read
   it; `layerVisibilityKey`/`drawKey` ignore it; `fromLegacy` keeps the
   default). Add `setLayerMode(Layers&, LayerMode next)`, which records
   `lastShown` when the mode leaves Only or Up to for All, and
   `leaveAll(Layers&)`, which sets `mode = lastShown` when the mode is All
   and returns whether it changed. Tests: All -> leaveAll gives Only by
   default; Up to -> All -> leaveAll gives Up to; Only -> All -> leaveAll
   gives Only; leaveAll on a non-All mode changes nothing; cycling the mode
   through All records the mode it left.
2. **Every mode change goes through `setLayerMode`:** the Mode stepper
   (`applyLayerStep` in `SchematicActions.cpp`, which also serves the Cycle
   layer mode key) and the screen's Mode field (`SchematicsView.cpp`).
3. **Every cut change from All calls `leaveAll`:**
   - menu Cut stepper and Layer up/down keys: the first step from All only
     leaves All (the cut stays), as now;
   - Cut here (key, menu) and the screen's "Match the layer you are in": set
     the cut, then leave All;
   - the screen's Cut +/-: like the keys, the first press from All only
     leaves All;
   - the screen's typed cut (`applyNumber`): set the cut, then leave All.
4. **Toasts:** when a cut change left All, say the resulting mode with the
   existing `schematic.toast.mode` ("{name}: mode {mode} (cut {n}/{count})")
   instead of `schematic.toast.cutOnly`, and remove `schematic.toast.cutOnly`
   from `Translations.h`, `TranslationsZhCN.h` and `tests/TranslationsTest.cpp`.
   The screen keeps its own message line (no toast).
5. Update SCHEMATIC.md's layer section and SCHEMATIC-CONTROLS.md where they
   say All switches to Only.

Done when: build and `LamiumTests` pass; docs updated. In-game check for the
maintainer: (a) Up to, then All, then Cut up: the mode returns to Up to and
the cut stays; (b) Only, then All, then Cut here: Only at the view's layer;
(c) in the Schematics screen, from All, Cut + and a typed cut both leave All
to the last mode; (d) after rejoining the world, leaving All gives Only.

### L-230 Move schematics with view-directed steps
Kind: Ready (rule decided 2026-10-20). Closed 2026-10-22: movement passed in
game on `dcab6ce`; the maintainer approved the directional key help text
(English and Japanese) as written. Maintainer feedback, 2026-10-20, from
the [Notion idea inbox](https://app.notion.com/p/3f7c12ab5b2d8121b2a0db32608f6b7a).
Status: rule decided and built 2026-10-20; the first rule was checked in game
the same day and reworked after it. The all-six rule then passed every
checklist item on `dcab6ce` (pitch boundary, up/down taking the horizontal
direction, 45-degree yaw, four move targets, rotated/mirrored placement,
Freelook/FreeCamera, layer keys, wheel chords), with no problems found. The
old behavior picked a horizontal X/Z axis, so looking up or down never moved
the target on Y.
Rule (maintainer 2026-10-20, revised after the first check):
- All six directions come from one camera frame. Forward is the view's
  dominant world axis; past 60 degrees up or down (`|y| > tan 60 *
  horizontal`) it is Y, signed by the view, otherwise the dominant horizontal
  axis (x or z). Right is the horizontal perpendicular. Up is right x
  forward, so it is world Y with a level-ish view and the horizontal
  direction the camera faces once forward has turned vertical.
- The maintainer's revision: up/down must follow the view too. Keeping them
  on world Y removed horizontal forward/back while looking up or down, so
  every direction now follows the same frame and none disappears at the
  steep boundary (up and forward trade places there).
- Decided per step from the current view: no gesture lock, no hysteresis. A
  turn changes the next step at once, and the radial menu blocks looking
  while it is open, so a burst cannot drift.
- The selected placement, corner 1, corner 2 and the whole area share the
  rule (`moveBy`).
- Camera-based: a detached Freelook or FreeCamera view owns the direction
  (`detachedViewRay`); otherwise the body's view.
- Default behavior, no setting yet. Add an on/off option only if horizontal
  building is disturbed in practice.
Built: `src/features/schematic/MoveDirection.h` (pure, tested in
SchematicTests), used by `SchematicActions.cpp` for the menu steppers and the
six directional keys. Contract: [SCHEMATIC.md](SCHEMATIC.md).
Explained (decided 2026-10-20): the directional key help now says that
looking steeply up or down moves forward/back on Y, and that up/down follow
the view and become horizontal then. No degree number is shown. A 45-degree
option is declined for now; the threshold stays one constant and can be
revisited if it feels wrong in use. The help text was added on 2026-10-20 and
awaits a look in game.

### L-237 Report a missing schematic structure without selecting it
Kind: Ready. Closed 2026-10-22: implemented by Codex and reviewed; checks
(a)-(d) passed on `1fd201b`, follow-up checks (e)-(h) on `1bf531e` (DLL
`E091F246…`). Spec agreed with
the maintainer 2026-10-22. Failure caching and read-only queries, the tested
per-generation notice rule, batched toast, Placed warning marker and HUD
message are implemented with English, Japanese and Simplified Chinese text.
User report 2026-10-21
during the L-234 missing-structure test: when a placement's `.mcstructure`
cannot be loaded, its ghosts are simply absent and the error appears only
after the placement is selected in the Schematics screen.

Found before implementation while specifying (2026-10-22): `session::load()`
(`src/features/schematic/SchematicSession.cpp`) caches only successes. A
failure is retried on every call, and `session::snapshot()` runs several
times per frame (ghost renderer, verify, map layers, target info), so a
missing file is stat-ed and a corrupt file is read, parsed and logged
several times per frame.

Do, in this order:

1. **Cache failures** in `SchematicSession.cpp`. `Cached` gains the error
   text (empty on success). A failed load stores an entry with a null
   structure, the error and `checked = now`, so the same two-second recheck
   applies; when the file's time changes or it reappears, the next check
   loads it again. `structure(file, &error)` and `load()` report the cached
   error. Log a failure only when a file's error text changes (first failure
   or a different reason), not on every recheck. `loaded()` keeps returning
   null for failures.
2. **Session query** (`SchematicSession.h/.cpp`): `std::string
   loadError(std::string const& relative)` returns the cached error without
   loading (empty when loaded or not yet tried), and `std::vector<Unreadable>
   unreadable()` lists the current placements whose file has a cached error:
   session `id`, `name`, `file`, error. Both only read the cache under the
   mutex.
3. **Pure notice rule**, new header `src/features/schematic/UnreadableNotice.h`
   (no game types) with tests in `tests/SchematicTests.cpp`: a small class
   fed each frame with the world generation, whether Schematics is on, and
   the unreadable placements (id + name). It returns the names to announce:
   - a placement is announced at most once per world generation (no repeat
     if it recovers and fails again in the same world);
   - a new generation (world join, session stop) forgets what was announced;
   - while Schematics is off nothing is announced or marked, so turning it on
     announces what is still unreadable;
   - everything newly unreadable in one frame is one announcement.
   Test those four rules plus the empty case.
4. **Announce** from `actions::adjustFrame` (`SchematicActions.cpp`, already
   called every UI frame on the client thread with `syncLifecycle`): feed the
   rule with `session::generation()`, `preferences().schematic.enabled` and
   `session::unreadable()`; when it returns names, show one
   `ui::showMessageToast` with `schematic.toast.unreadable`: the count, then
   at most two names joined with ", " ("、" is not needed: the same
   separator in every language), followed by "..." when there are more.
5. **Placed list marker** (`src/ui/SchematicsView.cpp`, the placement rows
   near the `label(...p.name...)` call): for a placement whose
   `session::loadError(p.file)` is not empty, draw a "!" in
   `palette::warning` before the name and shorten the name by its width.
   The detail pane already shows `schematic.notLoaded` with the reason for
   the selected placement; keep it.
6. **HUD** (`drawSchematicHud` in `src/features/information/InfoHud.cpp`):
   when the selected placement's structure is null and `loadError` is not
   empty, the layer cell shows `schematic.hud.unreadable` instead of the
   layer value (whatever the layer mode).
7. **Translations** (English, Japanese, then the Chinese row in the same
   order in `TranslationsZhCN.h`; add the format arguments to
   `tests/TranslationsTest.cpp`: count, then names):
   - `schematic.toast.unreadable`: "Can't read the file of {} placement(s): {}" /
     "設計図ファイルを読めない配置: {} 件（{}）" / "无法读取文件的放置: {} 个（{}）"
   - `schematic.hud.unreadable`: "File unreadable" / "ファイルを読めない" / "无法读取文件"

Not in scope: Settings rows (decided 2026-10-22: the toast and the list are
enough), drawing anything for the missing ghosts (the size is unknown without
the file), and the Files tab.

Done when: build and `LamiumTests` pass; one commit per logical step is fine
(cache + query, notice + toast, list + HUD); docs updated (this item's status,
SCHEMATIC.md's missing-structure note). Hand back for the in-game check:
(a) rename a placed schematic's file, join the world: one toast naming it,
"!" on its row, HUD (with it selected) says the file is unreadable;
(b) rename it back: the ghosts return within a few seconds, the marker goes;
(c) with Schematics off at join, no toast until it is turned on;
(d) the log has one line per failure, not one per frame.

Checks (a)-(d) passed on `1fd201b` (VALIDATION-LOG). **Follow-up, decided
with the maintainer 2026-10-22 from that check** (Ready, implemented;
in-game checks (e)-(h) pending): warning toasts and their timing/flag tests,
feature-prefixed text, yellow placement names, aligned error paragraphs,
off-state Check/Materials messages and current menu wording are implemented.

A. **Warning toasts.** Add a warning kind to the shared toast
   (`src/ui/Toast.h`: `Visible` gains `bool warning`; a new
   `ui::showWarningToast(message, seconds = 6)` next to `showMessageToast`
   in `InfoHud.cpp`/`Toast.h`). `drawInfoHud`'s toast block draws a warning
   **even when `runtime.ui.toggleToasts` is off**, with its text in
   `palette::warning` (keep `palette::dim` while it fades, as now). The
   schematic menu's toast note (`renderSchematicMenu`, `currentToggleToast`)
   uses `palette::warning` for a warning too. Add a Toast timing test for
   the flag. The unreadable notice uses `showWarningToast`; nothing else
   changes kind in this item (the paused-capability notice stays as it is,
   to be decided separately).
B. **Toast text names the feature.** Replace `schematic.toast.unreadable`:
   "Schematics: can't read the file of {} placement(s): {}" /
   "設計図: ファイルを読めない配置 {} 件（{}）" / "蓝图: 无法读取文件的放置 {} 个（{}）".
C. **Placed list:** the "!" and the name of an unreadable placement are both
   drawn in `palette::warning` (`SchematicsView.cpp`, the row added in L-237).
D. **Same position on every tab:** the Check tab's load-error paragraph
   (`case SchematicTab::Verify`, currently at `pvTop`) starts at
   `l.previewY` like the Materials and Placed tabs.
E. **Schematics off in Check and Materials:** while
   `preferences().schematic.enabled` is off, a selected, loadable placement
   no longer shows "Counting..." in the Check and Materials tabs (list empty
   message and detail pane); both show `schematic.offCheck` in
   `palette::faint` instead: "Schematics is off. Check and materials show
   once it is on." / "設計図はオフです。照合と資材はオンにすると表示されます。" /
   "蓝图已关闭。开启后才会显示核对和材料。". An unreadable file keeps its error
   first.
F. **Stale wording:** `schematic.off` still says the menu category is
   "Show"; it is now "Show and check" (L-238). English "(Show)" becomes
   "(Show and check)", Japanese 「表示」 becomes 「表示と照合」, Chinese “显示”
   becomes “显示与核对”.

Done when: build and `LamiumTests` pass; commits per logical change; this
item and SCHEMATIC.md updated. In-game check for the maintainer: (e) the
join toast is yellow, starts with "Schematics:"/"設計図:", stays about 6 s,
and shows with toggle toasts off; (f) the "!" row is yellow including the
name; (g) the load error sits at the same height on Placed, Check and
Materials; (h) with Schematics off, Check and Materials say it is off
instead of "Counting...".

### L-235 Key settings truncate long binding names and hide the full chord
Closed 2026-10-20: built and checked in game on `dcab6ce` (deployed DLL
SHA-256 `03aa48491a5d53cde4aca536a525985c791444b9bf5b1e04e148c6312f98dd00`):
hovering a clipped key cell shows the whole chord, short bindings show no
tip, a conflict tip (four caps plus the count) does not clip and keeps equal
padding. A long chord and a small window did not make the caps wrap; the wrap
path stays as a tested fallback.

Kind: Ready. Maintainer 2026-10-20, from assigning Left Alt + Wheel Up/Down
to the schematic away/closer actions: both rows drew "ALTERNATIVE + ..." and
the wheel direction was clipped, so two different bindings looked identical.
Hovering the key cell showed nothing unless the binding had a conflict.
Decided 2026-10-20:
- Shorten only the game's long modifier names: ALTERNATIVE becomes ALT,
  CONTROL becomes CTRL. SHIFT and ordinary keys keep the game's name; wheel
  directions keep "Wheel up/down" for now (icons only if they still read
  badly later).
- Hovering the key cell shows the full chord through the existing conflict
  tooltip only when the cell clips it; a short binding needs no tip. The
  keyboard-selected tip still appears only for conflicts.
- The tip is as wide as its content (the chord plus the conflict count),
  capped by the table width, so the count no longer squeezes the caps the row
  clipped. A chord that is still too long wraps onto another line, and the
  padding stays equal above and below.
- The capture footer names wheel directions, and says a wheel cannot be held
  for Hold actions (L-231).
- Left and right Alt remain one binding; the two sides are not distinguished.
Built 2026-10-20: `src/ui/KeyLabel.h` (pure, tested in BindingTests),
`src/ui/KeycapLines.h` (pure wrap, tested in SettingsTableTests),
`bindingChordName`, `keycapsWidth`/`keycapsWrap` and the tooltip in
SettingsTableView, and the capture footer text. The hover rule, width and
padding were revised after the maintainer's first in-game look: the tip
appeared for short bindings too, its bottom padding was larger than the top,
it had dropped the key caps, and a conflict tip clipped the chord it was meant
to reveal.
Renumbered from L-232 to L-235 on 2026-10-20: origin's merged PR #31 took
L-232 for the slime chunk indicator, and this item was still unpushed.

### L-93 Schematic: load, place, project, verify and list materials (experimental)
Archived initial-scope record, 2026-10-20: the initial feature shipped in 0.1.7 and the screen/rendering work in 0.1.8. Dated status statements below describe their original checkpoint. Current follow-ups remain under L-93 and their own L-items in BACKLOG.md; later fixes and coverage are in SCHEMATIC.md and VALIDATION.md.

2026-10-14 player-feedback follow-ups: mismatch highlight position (L-177),
piston texture/geometry (L-178), shulker facing (L-179), crash on opening
settings during checks (L-182), mismatch suppression (L-183), ghost data
inspection (L-184), permission-gated world load (L-185) and ghost
legibility/translucency comparison (L-186). All are open; no implementation
or release is selected by this documentation update.

Kind: First scope completed; follow-ups are Ready **(strong model)** or
Research where a new renderer path or behavior needs investigation.
Status: included in 0.1.7 (released 2026-10-07). Loading/placing, ghosts,
checking/materials, HUD/keys/target line, entities, area save, folder opening,
large-file warning, menu/adjust key and the final UI/drawing fixes have local
checks, including the release smoke test. Server and broader coverage remain
open; see [VALIDATION.md](VALIDATION.md).
Requirements, technical notes and the retained decision/build/research record:
[SCHEMATIC.md](SCHEMATIC.md).
Accepted follow-ups (2026-10-07; known gaps, not 0.1.7 blockers):
- Placement markers on the minimap/world map.
- A richer target-card line: expected block icon and differing state values
  (built 2026-10-08, checked in game the same day). Redesigned 2026-10-08
  (maintainer chose the agent's proposal): rows say what to do and take the
  verifier colors (red wrong/extra, yellow state, light blue missing); a
  wrong or extra block reads "Should be [icon] <block>"; each differing
  state is one row "<state>: <now> → <should be>" with readable names
  (L-112), and the block's own row for that state is not repeated.
  Built 2026-10-08 (`SchematicTarget.h`, tested) and checked in game the same
  day, including the drawn change arrow in English and Japanese.
- Beds sometimes drawing only one half or an outline; heads, doors and
  honey blocks still drawing as outlines.
- The Check tab's verifier-colored preview and the Files tab's rotatable
  3D preview.
- Neighbor-dependent ghost shapes using schematic neighbors (B1 done
  2026-10-09; doors draw, connections come from file states).
- The translucent ghost look.
- Entity name-tag distance and quantity.

Screen review (decided 2026-10-08; SCHEMATIC.md "Screen review against the
mockup", mock `docs/demos/schematic-screen.html`). Work order:
1. Screen and world, Ready (strong model), one in-game check for the step:
   placement frames in the world; the Placed list's selection bar and
   progress column; Files folders and columns; Check filter counts and
   differing states in the right pane; Materials sections, HUD switch,
   chest/stack amounts, missing items as slots and the ResourceCalculator
   button; entity ghosts as light-blue outline and faces, name tags without
   limits.
   Built 2026-10-08 and checked in game the same day (VALIDATION-LOG "L-93
   screen review step 1" and "refinements"); shipped in 0.1.8. Commits:
   placement frames (`273fb39`),
   the Placed progress column and selection bar, Files folders and columns
   (`e183d46`), Check filter counts and differing states (`926c9f6`),
   Materials sections, HUD switch, chest amounts, missing slots and the
   ResourceCalculator link (`8dd8457`), entity boxes with faces and every
   name shown (`98d9fe1`).
2. Research, after step 1 or alongside it: L-114 3D previews in the screen,
   L-115 entity models as ghosts, L-116 raw materials from the game's recipes.

0.1.8 rendering and compatibility work (plan 2026-10-09 in SCHEMATIC.md
"Rendering and compatibility plan for 0.1.8"): A saved-file version and a
vanilla-loader check, **done and checked in game 2026-10-09**; B1 schematic
neighbors (doors and their missing halves, mirror axes for block states and
entity facing), **done and checked in game 2026-10-09**; B2 render layers
(honey and slime drawn, blended ghosts and marks sorted), **done 2026-10-09**;
B3 liquids (sloped, flowing, waterlogged; previews too), **done 2026-10-09**;
deferred research (one item, maintainer 2026-10-09): translucency that does not
hide what is behind it, in the world pass (a blended material without depth
writes) and in the previews (water there is opaque), **done 2026-10-09**
(world: blended ghosts and marks drawn after the world's translucent
blocks; previews: shallow real depth, water translucent, cut-out limits
accepted); B4 block entity data (and Lamium saves keep it), **done and
checked in game 2026-10-09**; D waterlogging in the check, **done and checked
in game 2026-10-09**; C measured and tuned (30-50 -> 45-60 fps with several
large placements), **done 2026-10-09**
(B5's block-entity ghost lighting **done and checked 2026-10-09**; looking
straight down and Vibrant Visuals stay on the check list).
Reported 2026-10-09 (maintainer, `c48e59c`): water in waterlogged ghost
stairs showed through the stairs' covered sides (the ghost is translucent,
the real block is not) and its edge cells looked flowing; ghost beetroots
flickered. Built `62bff77`, not yet checked: a waterlogged cell's water is
not drawn on sides its own ghost covers entirely (`sidesCovered`, area of
the side's quads), and quads a ghost repeats (a face in both windings, as
crop planes are suspected to be) are dropped.
Checked 2026-10-10 on `62bff77`: no water through the stairs' sides, no
beetroot flicker. Left then, built `b40c543`: the pool
inside waterlogged stairs still showed the flowing texture (the texture
followed the slope; now it follows the flow, `liquids::flow`, tested: a
still pool leaning toward its walls keeps the still texture, a source at a
drop still flows); farmland beside schematic water flickered on its side
(a water face is no longer drawn against a ghost face in the same plane).
Both checked in game 2026-10-10 on `b40c543`.
Vibrant Visuals (2026-10-09): lines and mistake/selection faces keep their
colors there (done and checked); left, not scheduled: ghost faces untinted,
the honey ghost black, line width (the structure block's outline is wider).
Reported 2026-10-09: in the Files/Check previews biome-tinted blocks
(grass tops, leaves, vines) stayed untinted gray, tessellated above the
build limit where no biome answered. Fixed and checked the same day: cells
are tessellated at the top of the world around the player, so they take the
player's biome (chosen over a fixed tint). Leaves stayed gray (a real
mangrove leaf beside them was green): they draw in the seasons layers, which
the world colors in its own shader. Tinting only the seasons layers changed
nothing (`b02aea7`); built (not yet checked): blocks with a foliage tint
method (leaves, vines) take the renderer's biome tint (`BiomeColorSampling`,
as the minimap) on their still-gray vertices, in previews and world ghosts
alike: checked green (`dac35cb`, VALIDATION-LOG, one oddity noted there).
B-D: Research (strong model), approach left to the agent.

Pick the next follow-up with the maintainer; the list is not an implementation
order. Chosen 2026-10-07: placement markers on the minimap/world map,
built the same day (look in SCHEMATIC.md) and checked in game 2026-10-08.
Deeper map integration (toggling and editing placements from the map) is
part of L-111. Runtime
gaps stay under Pre-release checks and VALIDATION.

### L-191 Hide the hand and HUD in FreeCamera, and what can be operated there
Closed during documentation reconciliation, 2026-10-20: the local checks on `b5daa75` and `7d79bef` confirmed hand/hotbar hiding, whole-HUD hiding including Lamium, and restoration. The `d5978e2` local migration smoke also passed hiding/restoration. The built-but-unchecked status below is the original checkpoint, superseded by these results (VALIDATION.md / VALIDATION-LOG.md). Interaction options remain parked as L-25; broader coverage remains in VALIDATION.md.

Kind: Feature (was Design). Maintainer idea 2026-10-14.
Decided 2026-10-15 (maintainer, as proposed): a FreeCamera child setting
"Hide in FreeCamera": nothing / hand / hand and hotbar / whole HUD (as F1);
default hand. What FreeCamera lets the player do stays blocked as today;
allowing actions is L-25, later.
Status: built 2026-10-15, not checked in game (CAMERA.md "Hiding the hand
and HUD while flying").
In FreeCamera almost nothing can be operated, yet the first-person hand and
held item and the hotbar are still drawn. Consider hiding them while
FreeCamera is on (option, default to decide), together with the question of
which actions FreeCamera should allow (it blocks player interaction today,
CAMERA.md). Related drawing bugs: L-123 elytra, L-181 trident.

### L-216 World map region file format for the long term
Kind: Ready. Maintainer 2026-10-18: the saved map takes much more space than
the world (LamiumTest: 166 MB of map in 1,369 region files, 36 MB of world
`db`; 204 MB in 1,563 files against 44 MB on 2026-10-19). The current `LMR1`
format is uncompressed runs of (length, color, height) per region; natural
terrain changes color almost every column, so runs average about five
columns. The maintainer wanted a new format designed once for the long term,
without much more CPU.
Spec agreed 2026-10-19 and closed after that day's spec reviews; no further
spec reviews, the next step is building it (full contract: [MAP.md](MAP.md#region-file-format-lmr2-l-216-2026-10-19)):
- Regions keep the colors seen, as now; no block or biome storage (no
  recolor pass on a resource-pack change, no block-state conversions per
  game version). Those can come later as optional sections.
- `LMR2` files (`r.<x>.<z>.lm2`): a fixed magic, a format version and a
  section table; each section has its codec, "required" and "keep" flags
  and a CRC-32, with size limits checked before anything is allocated.
  Reading ends Loaded, Missing, Damaged or Refused; a future or unknown
  required format is Refused and never overwritten.
- Sections in version 1: `CHNK` (per chunk: recorded time in ms and the
  column revision), `COLR` (palette + indices), `HGHT` (wrapping height
  deltas), `OLDF` (the `.lmr` absorbed).
- A column revision constant is raised whenever the color or height
  calculation changes; chunks below it are shown and rescanned when loaded.
  Mixed scans and builds lower it, never raise it falsely.
- zstd level 3 per section, the only codec; no setting.
- Migration: `.lm2` is read first, then `.lmr`; regions read from `.lmr`
  are resaved, and the worker converts the rest in spare time (maintainer
  2026-10-19: the whole map, not only visited regions). Merging an older
  release's `.lmr` only fills unrecorded columns.
- Nether 16-block layers stay separate folders.
Measured 2026-10-19 (150 of LamiumTest's regions, Python): palette + height
deltas + zstd 3 averaged 22.7 KB per region (6.0x, about 35 MB for that
world) at about an eighth of zlib 6's CPU for a similar size.
Work (MAP.md "Code changes this needs"): the format and its tests in
`MapRegion.h`, reading/listing/saving/idle conversion in `MapStore.cpp`,
the produced-column mask and revision reports between `Minimap.cpp`, the
store and the scan grid, zstd as an xmake package with its notices. Then in
game: the map looks as before, the folder shrinks while playing, opening
speed, a world recorded by the previous release merging without losing
newer colors.

Implemented 2026-10-19 for PR review: the LMR2 codec and bounded file
reader, shared fill-only migration, guarded saves with delayed retry,
idle conversion, produced/scanned masks and revision reports. zstd v1.5.7
is pinned, statically linked and shipped with its BSD notice. Local
Lamium/LamiumTests/LamiumNativeTests builds, the Python/C++ fixture exchange,
game inventory, capability, package and archive checks passed. No in-game
validation or migration/performance measurement is claimed. Remaining
runtime checks are in BACKLOG's pre-release checks and MAP.md.

Validation follow-up (2026-10-19, after the implementation checkpoint
above): on `6ff0610`, the live filesystem audit migrated all 1,563 old
LamiumTest regions without recorded-column loss and reduced disk use by
about 83%. The maintainer subsequently found no conspicuous issue in
appearance, opening/zoom speed, Nether/cave display or retention after
rejoin. Evidence and limits: VALIDATION-LOG.md. Measured frame times,
older-build merges, runtime refusal/damage/save-failure paths and broader
server/layer coverage remain open; cave/Nether usefulness is Design L-228.
The review corrections to batching, legacy diagnostics and provisional
cave tint (column revision 2) have automated coverage but await a focused
live check. Current pending checks are in BACKLOG and MAP.md.

### L-228 Reconsider what cave and Nether maps should show
Kind: Ready (strong model). Done 2026-10-20: implemented on branch
`feat/map-cave-freecamera`, merged to `main` (`a5c8db3`); final checklist
passed in game 2026-10-20 (`7e0e2f9`). Maintainer feedback
2026-10-19 after the L-216 smoke check:
no conspicuous problem with that PR, but the cave/Nether view may not give
the player a useful account of the terrain. At roughly Y67 (selected band
Y64-79), the map is mostly gray with dark gaps, and expected lava lakes
below are hard to see. Around Y32 (band Y32-47), the visible footprint
becomes much smaller and the map is mostly lava. Both supplied screenshots
show the same 500-block scale and "Loading 4"; they do not establish a zoom
change or a completed image build.

Maintainer explanation 2026-10-20: the higher band had been recorded with a
larger render distance; the lower trip used a smaller render distance. Each
16-block band records only the chunks received at that time, so the same X/Z
can have a much smaller recorded footprint at another Y. The 500-block scale
is zoom, not coverage, and pending images can add temporary gaps. This is
expected under per-band recording, not a code defect.

Current behavior: cave columns are sampled from five blocks above to 24
below a held height; walls and drops are dark, and unavailable stand-ins
can also be drawn as rock. The world map stores Nether bands separately
every 16 blocks, and can follow the player's band automatically. These
rules explain possible contributors, not a verified diagnosis of each
pixel or of the smaller footprint. This feedback does not establish an
LMR2 storage regression.

Direction agreed with the maintainer 2026-10-19: represent the cavern the
player occupies, including the current ledge/path and the deeper floor or
lava within that open space. This applies to large Overworld caves as well
as the Nether; narrow caves must still communicate floors, walls and forks.
Moving up or down within the same space should preserve the recognizable
terrain outline, while separate vertically stacked spaces remain
distinguishable. Height differences should be readable, and unknown terrain,
walls and deep drops must be distinguishable. Use only received or recorded
terrain, within the existing client-only and bounded-work requirements.

Overlap priority agreed the same day: where a bridge or ledge overlaps a
lower floor, prefer the floor the player currently occupies. On a bridge,
preserve its path and show the deeper floor/lava in the surrounding open
parts; below it, prefer the lower floor. The exact visual treatment and
selection/transition algorithm remain open.

Scope extended by the maintainer the same day: persist explored Overworld
cave terrain and make it available for later world-map viewing, including
after leaving and rejoining. Retain the surface map alongside it. The
design must support revisiting vertically overlapping cave spaces without
erasing the surface or conflating those cave spaces. Before L-228 the build
recorded the Overworld surface only; this is new scope, not validated
behavior.

Interaction review, decided 2026-10-19: the reduced D control model in
[demos/cave-map-simple.html](demos/cave-map-simple.html) was accepted after
the A/B/C mockups were rejected as too complex. The world map's top bar has
one terrain state button; its panel opens directly under the button (it does
not close the waypoint list; it shifts left when space is tight). The panel
has cave on/off, broad on/off, height with `-`/input/`+`, and a
`自分の高さに合わせる` action. The map opens at the player's height; changing
the height fixes it; the action returns to the player's height. The slider,
recorded-height list, three-way mode and follow toggle were dropped. Layer
switches keep their names while they fit and become symbols only when the
bar is too narrow. The Nether starts broad, an Overworld cave starts in the
selected layer, and the minimap stays broad and follows the player. The map
does not switch cave/surface while open.

Storage and representation agreed for v1: record explored Overworld cave
terrain in 16-block bands alongside the retained surface map, using the
existing LMR2 folders (a new `overworld/y<Y>` family). Vertically
overlapping spaces inside one band are not distinguished; separate bands
are. The cave scan uses a deeper downward window for the occupied floor and
the lower floor/lava, with a bounded per-column read; the world map's layer
view dims floors outside the selected height, while broad view keeps them.

Implementation on branch `feat/map-cave-freecamera` (2026-10-19): `a7ad024`
deepens the cave read, `7bed82e` adds the Overworld cave bands, alternating
recording and focused images, `cfe2c43` replaces the Nether band control with
the D state button and panel, `bf4f325` addresses the first in-game feedback
and review findings, and `9675129` settles the display model (128-block cave
read, solid dim for far layer floors, three reads only: floor/wall/no-floor,
column revision 4) and adds the FreeCamera reference (camera for opening and
height matching, body for "Center on me"). `7e0e2f9` draws the camera as the
same green arrow, removes the return-to-camera action (reopen the map to
return), and resumes following when a manual edit lands on the reference
height. The camera arrow is display-only; closing and reopening the map
returns to the camera. Review follow-up `c67054d` (2026-10-20, checked in
game) opens the map at the stable cave band used for recording and checks
the body's own roof for body reference instead of the FreeCamera view.
`07f0dc1` (2026-10-20, checked in game) makes the render loop follow that
same stable reference; before it, a `+` that reached the stable band enabled
following and the next frame snapped back to the raw eye height
(64 -> 64 -> 64, 78 -> 79 -> 80 -> 78).
The owner extended this branch to cover the cave map and the FreeCamera map
reference in one change instead of separate PRs. Build, `LamiumTests` and
`LamiumNativeTests` pass. The final in-game checklist passed on `7e0e2f9`
(2026-10-20): green camera arrow, no return-to-camera action, automatic
follow resume at the reference height, and UI Profile 50/75/100. The
`c67054d` band/body fixes and the `07f0dc1` follow fix also passed in game
on 2026-10-20. Residual measured frame times and broader server coverage are
in BACKLOG's Pre-release checks. The earlier Y32/Y67 difference is explained
by per-band recording and render distance, not a code defect. No release is
chosen. Keep this follow-up separate from L-216. Direction: DESIGN.md, "Cave
and Nether terrain reference"; current behavior and evidence: MAP.md's "Cave
and Nether representation review" and VALIDATION-LOG.md, 2026-10-19.

### L-223 Position labels follow the game's own wording
Kind: Ready. Maintainer 2026-10-19: English showed "XYZ", "Position" and
"Block coordinates" side by side. Checked the game's 1.26.51 texts: its
Show Coordinates line is `map.position` "Position: %s, %s, %s" / 位置 / 位置,
the option is "Show coordinates" / 座標を表示 / 显示坐标; Java's debug screen
uses "XYZ", "Block" and "Targeted Block". Decided (option A): the
whole-number line is "Position" / 位置 / 位置, the decimal line "Exact
position" / 正確な位置 / 精确位置, the target card row "Position" / 位置 /
位置 (its switch "Show position"), "Target block" stays, Debug View keeps
"XYZ" and gets its own "Block" key. Settings keys unchanged. Checked in
game 2026-10-19 (`cdc02c5`): OK.

### L-218 Info HUD lines have no descriptions
Kind: Ready. Maintainer 2026-10-18: every Info HUD child row in the settings
shows the parent's text. Give each `information.*` / `hud.info.*` row under
Info HUD its own `help.<option id>` in English, Japanese and Simplified
Chinese, saying what the line shows and where its value comes from (body or
FreeCamera camera).
Status: written 2026-10-18 for all 28 rows (Info HUD switch excluded: it
uses `help.infoHud`); maintainer OK in game
2026-10-18.

### L-220 Calculator leaves the settings sidebar
Kind: Ready. Maintainer 2026-10-18: the calculator does not belong in the
sidebar's pinned tools. It stays openable from the settings screen: its row
gets an Open button in the switch column; its key still opens it from
gameplay. Checked in game 2026-10-18 (sidebar, Open button); maintainer the
same day: the row belongs in General, moved there; checked OK (`c4884e5`).

### L-208 Connected Textures: edges where glass turns a corner
Kind: Ready. User suggestion 2026-10-17; maintainer 2026-10-17: try the display option.
Decided with the maintainer 2026-10-18 (three separate cases):
1. Flat notch (always on, no option): where a face joins two sides but its
   diagonal neighbor is not the same block (a flat L seen from above, also
   with three or four joined sides), the face keeps the frame's one corner
   texel (one 16th), so the neighbors' borders meet without a gap. The first
   build (PR #16) kept a two-texel patch, which stuck out one texel along
   both borders.
2. Inside corner of glass blocks (option `visuals.connectedCorners`, "Edges
   at inside corners", default off): a side joins only while the neighbor's
   own face shows. Where walls turn inward that face is hidden behind the
   other wall, so both walls keep their border at the turn (a vertical line
   on each side of the inside corner). Straight runs and the outside of the
   turn are unchanged. Only the same block counts as hiding the face.
3. Glass panes (option `visuals.connectedPaneCorners`, "Edges at glass pane
   corners", default off): vanilla has no edge at an L-shaped pane either.
   The post's outside faces slide onto the tile border at their corner end
   (the whole two-texel window moves, so nothing stretches; the first build
   moved only the corner vertices and stretched it, found in game
   2026-10-18). The inside of the pane corner stays merged: an edge there
   was judged too much (maintainer 2026-10-18).
Cost: glass textures get 46 sprites each instead of 15 (corner variants).
Done 2026-10-18: merged from PR #19 (`l208-glass-corners`); the maintainer
checked all three cases and the option help texts in game the same day
(VALIDATION-LOG). The first build (PR #16) and the pane stretch fix of
PR #17 are folded into it; PR #17 was closed.

### L-169 Item Transfer — 0.2.2 part

Done 2026-10-15: implemented on c8c1f1b, built and LamiumTests passed; the
maintainer confirmed the normal-build smoke checklist all behaved as
expected. Windows x64 release client, traces/probes off, built and installed
DLL SHA-256 `2A6B0A52B397511108815C2BE4315EFBA84403343AAA242B632D2B9FEF6BE515`.
Unreleased. Chest/furnace/brewing/beacon smoke checks were requested;
the full gesture/screen matrix, servers and controlled failure cases remain
in BACKLOG's Pre-release checks and VALIDATION. Later L-169 Design parts
remain in BACKLOG. Evidence and implementation: ITEM-TRANSFER.md and
VALIDATION-LOG.md (2026-10-15).

**0.2.2 contract (decided 2026-10-15).** Gestures, in the meaning a widely
used Java inventory mod gives them (PROVENANCE: public key list only):
| Gesture | Effect |
|---|---|
| Ctrl + wheel | every stack of the hovered kind to the other side (up: into the container, down: into the inventory, as the plain wheel) |
| Ctrl + Shift + wheel | everything to the other side; from the inventory the hotbar stays |
| Alt + left click | as Ctrl + wheel, toward the other side from the clicked slot's side |
| Alt + Shift + left click | as Ctrl + Shift + wheel, from the clicked slot's side |
| Shift + right drag | from each passed slot, all but one item to the other side |

- Screens: every screen that holds items beside the player inventory: chest
  family, barrel, shulker box, ender chest, furnace, blast furnace, smoker,
  dispenser, dropper, hopper, brewing stand, crafter, beacon and any other
  found while testing. Record the list of checked screens in DESIGN.
- Slots with roles: send to the slot that accepts the item (fuel to the fuel
  slot, smeltable items to the input, bottles / ingredient / blaze powder to
  their brewing slots, the payment to the beacon slot); take from outputs only.
  An item a slot role cannot take is not moved; no valid destination does
  nothing. Where vanilla shift-click already routes by role, follow it.
- The existing four gestures keep their meaning and switches. Each new
  gesture gets its own child switch, on by default like the existing ones.
- Same safety as today: one request at a time, each confirmed from
  authoritative state; stop on screen close, focus loss, a cursor item, text
  editing, a rejected or timed-out request or a changed source count.
- Research first, report before building around it: whether Alt reaches the
  game in screens (if not, stop and ask; do not pick another key); whether
  vanilla's take-all / place-all / auto-place handlers do exactly the above
  and can be called from the client thread; that Shift + right drag has no
  vanilla meaning on Bedrock that players rely on.
- Not in 0.2.2 (maintainer 2026-10-15): Shift + click on an empty slot with
  items on the cursor, a wheel version of "all but one", drop gestures, W/S
  moves.
- Check in game per screen family and gesture, locally and on a server.

The investigation on e3f9276 confirmed Alt arrives, take-all/place-all are
cursor operations, and Shift transfer follows native auto-place orders.
Vanilla Shift + right drag moves a whole stack; the chosen enabled gesture
leaves one and its off switch preserves vanilla. Furnace slots may accept
items that cannot smelt/burn: native acceptance/routing is preserved without
a stricter Lamium recipe/fuel filter. The normal implementation discovers
native roles, filters out same-player-side fallbacks and output destinations,
and confirms per-request counts from reacquired authoritative slots.

### L-189 Fake Offhand fallback when primary right-click has no useful action
Done 2026-10-15: the target-specific primary uses passed locally on `a1a758e` (VALIDATION-LOG). Hoes, shears, honeycomb, crossbows, Riptide tridents, primary fireworks and servers stay in VALIDATION.
Kind: Feature (was Research, then Design). User feedback 2026-10-14.
Clarified by the maintainer 2026-10-15: the primary hand's item suppresses
the fake offhand whenever its kind has a right-click role, whether or not
it would act: bread at full hunger aimed at a block, a shovel aimed at stone.
Decided 2026-10-15 (maintainer): include as much as possible, using the
game's own checks where they exist. Status: built, not checked in game
(FAKE-OFFHAND.md "Target-specific primary uses"). Check in game: bread at
full hunger on stone; a shovel on stone (passes) and on grass (paths);
seeds on stone (passes) and on farmland (plants); bone meal on stone and on
grass; an axe on a log, on copper and on stone; a minecart on stone and on
a rail; a bow without arrows aimed at the sky; an empty bucket at the sky
and at water.
Original goal: if the main-hand right-click has no meaningful applicable action,
allow the configured Fake Offhand item to receive that use. Reported
examples are fireworks that cannot be used while the player is full or is
holding a shovel/axe; this is a generalized input priority rule, not a
firework-only exception.
Research the native action/result and target-interaction outcomes: a
non-mutating or nominally successful primary callback may not be a real
use; conversely a false return does not prove that trying the other hand
is safe. Never double-use, break valid block/entity interactions, fire
accidentally, overwrite the selected slot or assume server success.
Settle what counts as a genuine no-op before defining exact fallback order;
check air, blocks, entities, cooldowns and server correction.
[FAKE-OFFHAND.md](FAKE-OFFHAND.md).

### L-187 Increase the world map's maximum zoom-out
Done 2026-10-14: zoom out to the whole explored area (`85207e9`, seen on `9b3b8a9`); the zoom-in drawing fixes `00be8f0` and `f25188f` passed on `7548f0f`. First far build time on very large maps and servers stay in VALIDATION.
Kind: Feature. User feedback 2026-10-14; maintainer chose the range the
same day: zoom out until everything explored fits on one screen.
Status: checked 2026-10-14 on `9b3b8a9` (VALIDATION-LOG): whole explored
area on screen, small worlds unchanged. Open: zooming in after a far view
(`00be8f0` then `f25188f`, which fixes finer tiles never replacing the
coarse stand-ins) to check. Steps to
1/256 GUI units per block open as far as the layer's explored bounds need;
coarse images skip empty space and rebuild only changed quarters.
Check in game: zoom out on a large explored world (Overworld and a Nether
layer), the first far build time, waypoint/marker readability at the
farthest step, recording while open, and that a small world still stops at
1/8. Not chosen: auto-centering on the explored area. No release chosen.
This request is not to generate or retain more unexplored terrain.

### L-188 Let the settings key toggle the settings screen closed
Done 2026-10-14: `a123eba` passed on `9b3b8a9` (open/close, held L, typing, binding). A remapped key and the tool pages stay in VALIDATION.
Kind: Feature. User feedback 2026-10-14. Status: checked 2026-10-14 on
`9b3b8a9` (open/close, held L, typing, binding); a remapped key and the tool
pages remain in VALIDATION.
The default L shortcut closes Lamium Settings as well as opening it; the
chosen rule is in UX-FOLLOWUP.md "Settings key feedback". Check in game:
L opens and closes; holding L opens once; typing L in search, a number or
a name does not close; key capture of L still binds; the world map keeps
M/Esc; a remapped Settings key works the same; no input leaks into
gameplay after closing. Do not fold the
separate crash report (L-182) into a mere UX toggle fix.

### L-174 Chat messages placed like Java Edition, through the HUD layout
Done 2026-10-14: initial native chat placement (`5b5750c`) and the
component-render expiry fix (`b28a40d`) passed the maintainer's checklists,
including wrapped long messages with no flicker. The chosen default is
flush left with the bottom 44 GUI units above the screen edge: two default
Durability HUD rows, their bottom inset and a 4-unit gap. Existing saved
placements remain intact. Experimental removed after the checks.
Reset/fresh-settings default smoke and broader UI/pack/server checks remain
in BACKLOG Pre-release checks and VALIDATION; no release selected.
Current contract: [CHAT-POSITION.md](CHAT-POSITION.md).

Follow-up 2026-10-14: reset to the final default passed on `dc3ac45`, DLL
SHA-256 `f0f51371a77025a533a73730630cf05f8c1bc66d650c7dce53c9b8716a10a0fb`.
The maintainer accepted closing L-174. Fresh settings and broader coverage
remain in VALIDATION; the reset smoke gap above is now closed.

Implementation history below retains the superseded defect/next-step state.

Kind: Research, then Ready (strong model). Chosen 2026-10-14; no release selected.
Goal: show the ordinary chat messages at the bottom left as on Java
Edition, ideally placed by drag in Lamium's HUD layout.
Proposal (not decided): only the message display at first (not the chat
input screen): Vanilla or Custom, Custom with a bottom-left preset and free
placement; width, line count and scale later.
Research first: `hud_screen.json` has a `chat_panel`; Lamium already finds
vanilla HUD controls with `VisualTree::getControlByName()`. Find whether
that panel alone can be moved while vanilla keeps its messages,
background and fading, and when to apply the position (UIControl position
API and timing unknown). Moving the whole `chat_stack` would move other
elements (the coordinates). Check HUD re-layout, resource packs, screen
sizes and GUI scales, and the return to vanilla when off. A chat of
Lamium's own is out of scope.
Decided (maintainer 2026-10-14): first version has a saved, default-off
Chat position switch, moves the ordinary messages to the bottom left on
enable, and allows free placement in HUD layout. Background, font size,
shadow and fading stay vanilla; the chat input screen is outside this scope.
No width, line-count or appearance controls in this step. Research and
the first implementation checkpoint: [CHAT-POSITION.md](CHAT-POSITION.md).
Current (2026-10-14): the maintainer passed the three-step checklist on
`5b5750c` (text/background placement and fading, layout/save/reset, OFF and
coordinates/input unaffected), but remaining messages briefly flicker when
an older message disappears. Fix in progress: apply translation at the
native component draw, after layout, without forcing all position caches
dirty each frame. Runtime recheck pending; keep this item open.

### L-172 Text fields: caret movement, paste and click
Done 2026-10-13: shared `TextEdit` with caret, Delete, paste, copy and click (`94e48b5`); caret in the spacing column and typed world map coordinates (`ed4a4e7`); checked in game. The IME window stays at the field's left edge (no SDK call moves it).
Kind: Ready (strong model), decided with the maintainer 2026-10-12.
Problem: every Lamium text field (settings search, number fields, Shapes,
Waypoints and Schematics names, save prompts, the world map search) only
appends at the end; Backspace removes the last character and select-all
replaces everything. There is no caret position, no Left/Right, Home/End or
Delete, and no paste (`SearchQuery` and `NumberInput` in `src/ui/`).
Scope (maintainer: movement, paste and click):
- One shared pure editor (header, tested) with a caret kept on UTF-8
  codepoint boundaries: insert at the caret, Backspace before it, Delete
  after it, Left/Right by codepoint, Ctrl+Left/Right by word, Home/End,
  Ctrl+V paste (control characters dropped, length limits kept), Ctrl+C
  copy of the whole field (`app/Desktop` already writes the clipboard;
  reading it is new), select-all kept as today.
- IME: a composition's backspaces act before the caret, and the native
  keyboard's caret position (`syncTextKeyboard`) follows the caret.
- Click inside a field puts the caret at the nearest character boundary
  (text width from the same measure the widgets use).
- The caret is drawn at its position; long text scrolls to keep it visible.
- Every existing field moves to the shared editor; `NumberInput` keeps its
  digit rules on top of it.
Later: selections (Shift + arrows, Shift + Home/End, drag) and replacing a
selection.
Built 2026-10-13: `TextEdit` (`src/ui/TextEdit.h`) under `SearchQuery` and
`NumberInput`; the settings screen routes the caret keys, Delete and
Ctrl+V/C to the focused field and a click inside it to `placeCaret`; one
`drawEditField` draws every field (selection highlight instead of the old
"[...]" brackets and "_" markers). Not possible: moving the IME window with
the caret (the SDK's keyboard takes a position only when it is enabled), so
it stays at the field's left edge.
Tests: insert/delete around multi-byte characters, word jumps over spaces
and punctuation, paste limits, IME backspace sequences at a middle caret.

### L-159 Find actions by key in Hotkeys
Done 2026-10-12: Find by key, then the follow-up (button order, focus after a find, binding editor buttons on the edited row; `e93606e`); all checked in game.
Kind: Ready. Requested by the maintainer 2026-10-12 ("what did I put on Z?");
they chose press-a-key over typing key names or a `key:` syntax. Contract in
DESIGN.md (Settings screen, resets paragraph).
Built 2026-10-12: `chordUses` (Binding.h) and a `KeyFilter` for
`buildSettingsRows`; the screen reuses the binding capture path and applies
the found inputs with the next frame, outside input events. Vanilla
bindings are not searched. Tests cover matching, default bindings and the
button's place. Checked in game 2026-10-12 (`c4ac083`): all OK.
Follow-up decided 2026-10-12 (DESIGN.md): swap the buttons so Find by key
sits next to the key column and Reset all keys left of it; after a find the
search field takes focus with "Key: Z" as one unit (Backspace clears it,
typing starts a text search, Esc clears, Down/Enter go to results); the
binding editor's Clear/Reset/Cancel move from the footer onto the edited
row next to its key cell, replacing its switch while editing (A in
[demos/binding-editor.html](demos/binding-editor.html); footer hint kept).
Built 2026-10-12; in-game check pending.

### L-135 Leave the PDB out of the release ZIP (with 0.2.0)
Done 2026-10-11: v0.2.0 released with the ZIP (1.8 MB, no PDB) and `Lamium-0.2.0-client-windows-x64.pdb.zip` (9.7 MB) as its own asset; the release script checked GUID and age.
Kind: Distribution. Decided by the maintainer 2026-10-11.
Status: built 2026-10-11 (`d274227`); the first release made this way is
the check. The symbols asset is `Lamium-<version>-client-windows-x64.pdb.zip`
(the PDB compressed: 17.6 MB instead of 170 MB for 0.1.9's); the identity
check is `scripts/PdbIdentity.ps1`.
`Lamium.pdb` is about 90% of the release ZIP and grows each release
(compressed 13.2 MB in 0.1.7, 15.6 MB in 0.1.8, 17.3 MB in 0.1.9; the DLL is
1.8 MB). No crash report has arrived, and a description of the situation is
expected to be enough to reproduce one. From 0.2.0 the ZIP ships without the
PDB (`scripts/New-ReleaseArchive.ps1`, `Check-Package.ps1`, DISTRIBUTION.md,
the LIP package contents). The PDB is attached to the GitHub release as a
separate asset (`Lamium-<version>-client-windows-x64.pdb` or similar,
decided 2026-10-11), so LIP and LeviLauncher install only the DLL while the
symbols stay available for crash addresses. CI checks that the PDB belongs
to the same build as the DLL in the ZIP (matching debug GUID and age).

### L-139 Map layers (0.2.0, L-111 B)
Done 2026-10-11: session ids, mark keys and layers (`534134f`, `7f85e7d`, `cf54db8`), names on hover (`9e675cc`); passed the 0.2.0 regression checks and the outside reviews (2026-10-11).
Kind: Feature **(strong model)**. Step 4 of the 0.2.0 order (L-111), after
the WorldMap.cpp and SettingsScreen.cpp tidy-up (step 3).
Status: built 2026-10-11, in-game check pending (0.2.0 regression checks).
Session ids (`534134f`, app/SessionIds.h), selection and menus by mark key
(`7f85e7d`), layers (`cf54db8`). Settled in the step: marks are
`MarkKey{layer, id}` (MapMarks.h); under the cursor waypoints and the death
point come first, then placements, then shapes, the smallest footprint
first within a layer (`markAt`, tested); a provider that turns off
(Schematics off, shapes not drawn) returns no marks, and a selection or
menu on a mark that is gone is dropped; the minimap only draws, the world
map hit-tests and acts. World map actions: placements select (also the
schematic's selected placement), show/hide, open in the Placed tab; shapes
show/hide, open in the Shapes view; the side panel shows the selected
footprint's extent, a Show switch and the open button. Shape boxes are
cached by the overlay until a shape changes; placements (at most 64) are
listed per frame. Shapes on the minimap: map option "Shapes", off by
default. Not done: one combined side-panel list per layer (the list stays
waypoints; placements and shapes are selected on the map).
Follow-up (maintainer 2026-10-11, 0.2.0): names crowded where marks
overlap, so the world map shows a name on hover, for the selection and an
open menu; option "Always show names" (off by default). Built and passed
2026-10-11 (`9e675cc`).
One list of things drawn on the minimap and the world map, filled by the
features that own them (INTEGRATION.md B): waypoints and the death point
(as today), schematic placements first, then Shapes. On the world map they
share one way of selecting and acting (show/hide, select, the feature's own
actions); the minimap only draws.
Design points to settle in the step (outside review 2026-10-11):
- Marks are identified by a stable id from their provider, never by list
  index, so adding or removing one cannot retarget a selection. Today only
  Shapes have one (`ShapeId`, in memory, not saved); `Waypoint` and
  `SavedPlacement` are plain vector entries (`PlacementSet::selected` is an
  index) and the death point is a single optional. Proposed: each provider
  assigns a session id when an entry is loaded or added, never reused within
  the session and unchanged by reordering, edits or other deletions; the
  death point uses a fixed key. Ids are not saved (selections reset on world
  exit anyway), so no file format changes; saving them is a separate
  decision if a cross-session reference is ever needed. Tests: two
  waypoints or placements with the same name and position; deleting another
  entry while one is selected; editing a map-selected placement from the
  schematic screen; world exit and rejoin clear the selection; an action on
  an id that no longer exists does nothing.
- A fixed priority when marks of different layers overlap under the cursor.
- When a provider turns off (feature disabled, world exit, dimension
  change), its marks, selection and open actions are dropped.
- What the minimap and world map share (marks, hit tests) and what stays
  apart (the world map's panel and menus).
- Drawing cost with many placements or shapes: footprints are computed when
  an entry changes and cached, not rebuilt every frame; drawing uses the
  visible range and is bounded per frame.
- Shapes on the minimap: a setting (decided 2026-10-11), since many shapes
  would crowd it.
Does not share: storage (waypoints, placements and shapes keep their own
files). Keeps: today's waypoint and death-point behavior unchanged.

### L-145 Info HUD hides the game's paper doll where it covers it
Done 2026-10-11: the paper doll and the position line hidden where the Info HUD covers them (`df79b8d`, `f822b70`, checked).
Kind: Feature (small). Maintainer 2026-10-11 (after L-143): at the default
place the Info HUD overlaps the paper doll. Chosen: an Info HUD option
"Hide the game's paper doll under it", on by default, that hides the doll
only while the drawn Info HUD box meets the doll's area (its `hud_player`
control widened by one control width each side, from the top of the
screen down to the chat's 50-unit room for it; `PaperDoll.h`, tested);
placed elsewhere nothing changes. Shares the L-143 paper doll hook.
Passed on `df79b8d` (2026-10-11). Follow-up the same day: the game's
position line (Show Coordinates) overlapped the Info HUD too, so the
option also hides the `player_position` control where the Info HUD box
meets it (label "Hide the game's paper doll and position under it").
In-game check pending.

### L-144 The game's hotbar in the HUD layout editor
Done 2026-10-11: the hotbar's images in the HUD layout editor (`07afa45`, checked on `9c173b7`).
Kind: Feature (small). Reported on Discord after 0.1.9 (maintainer's notes,
2026-10-11): the editor shows Lamium's elements but not the hotbar, so
elements are placed blind next to it. Built 2026-10-11: the editor draws
the hotbar's images (not an element, not clickable) where the game last
laid it out on a screen of the same size (also Pocket UI), else at the
desktop place (182x22, bottom center). In-game check pending.

### L-143 Debug View hides the game's position line and paper doll
Done 2026-10-11: Debug View hides the game's position line and paper doll while open (`535267a`, checked on `9c173b7`).
Kind: Feature (small). Reported on Discord after 0.1.9 (maintainer's notes,
2026-10-11): with Debug View open, the game's own coordinates and paper
doll still show in the top left under it. Built 2026-10-11: Debug View
option "Hide the game's position and paper doll while open" (on by
default); while Debug View is shown the `player_position` panel and the
HUD paper doll (`HudPlayerRenderer`) are not drawn. The game's options are
not touched, so nothing needs restoring. In-game check pending.

### L-140 Zoom: Wide-angle view as a setting (0.2.0)
Done 2026-10-11: Zoom setting Wide-angle view, off by default (`cea3014`, checked on `9e675cc`).
Kind: Feature (small). Decided by the maintainer 2026-10-11 (noted in their
notes first, chosen for 0.2.0 the same day). Most players use Zoom to
magnify, so the wide view below 1x becomes an explicit choice.
- Zoom setting "Wide-angle view" (広角表示, 广角视野), off by default; help
  "Allow the zoom wheel to go below 1x for a wider view."
- Off: the wheel goes 1x-50x. On: 0.5x-50x as before (the 160 degree
  limit stays). The initial magnification (2x-50x) and its default do not
  change; the stop on exactly 1x stays.
- Turning it off brings a kept level below 1x up to 1x, so the old wide
  level does not return.
Status: built 2026-10-11 (`cea3014`); passed on `9e675cc` (stop at 1x off,
0.5x on, 1x after turning it off). Not reported separately: toggle mode,
saving across a restart.

### L-131 Player list: players in other dimensions and permission marks
Done 2026-10-10: built (`4ad4e18`) and checked in game on BDS and a PC-hosted LAN world with a phone (VALIDATION-LOG).
Kind: Research. Split from L-128 on 2026-10-10 (maintainer: later, but find out).
Status: open.
The player list shows dimension and distance only for players known to share
your dimension: loaded players and Locator Bar positions, which the server
sends only for your dimension. Find out whether the client can tell which
dimension other players are in (packets, the player list, Locator Bar data
when they leave), and whether it knows other players' permission level
(operator, member, visitor) for the vanilla `permissions_op_crown`,
`permissions_member_star` and `permissions_visitor_hand` marks. Today only the
host is marked (singleplayer: you; a dedicated server: nobody).
Trace 2026-10-10 (`3adc21c`, BDS, the maintainer changing dimension): the
client holds every listed player's permission (all members there, you too);
Locator Bar data gives a position for players in your dimension and turns to
"hidden" when either side leaves it ("absent" for players never seen). So the
current dimension of others is unknowable, but where they were last seen is.
Decided (maintainer): show the last-seen dimension faded, without distance;
mark operators (crown), visitors (hand) and custom (dots), members only with
the "Member marks" option (default off); no separate host mark. Built
2026-10-10; permission changes not yet traced (a LAN world with a phone can
do it).

### L-128 Player list while a key is held
Done 2026-10-10: built (`d8c16d2`, fixes `f7e6f87`, `ea0ade4`) and checked in
game (VALIDATION-LOG). Dimensions and distances are known only for players
in your dimension (loaded or Locator Bar); the rest went to L-131.
Kind: Design. From the maintainer's notes and a user request ("tab for the
player list", 2026-10-09); not chosen for building yet.
Status: spec decided 2026-10-10 (mockup `docs/demos/player-list.html`, fifth
version); building.
Spec (maintainer, 2026-10-10):
- Hold a key (default Tab, rebindable) to show the players the server lists
  (`Level::getPlayerList()`), not only those in render distance. Hidden while
  chat or a screen is open. The parent row has only the key (like Zoom).
- One row per player: face from the skin, name, host crown
  (`textures/ui/permissions_op_crown`, loaded at runtime), platform, dimension,
  distance. Platform, dimension and distance are child switches, all on by
  default. Columns are aligned across the list; an empty value keeps its
  column width; distance is right-aligned.
- Platform as short text from `BuildPlatform`: Win, Mac, Linux, Android,
  iOS, Fire, PS, Switch, Xbox, Server, ? (logos are not drawn).
- Dimension as the item icon of grass block / netherrack / end stone through
  the shared icon path; unknown stays blank (how far it is known: Research).
- Distance "123 m" for players in your dimension when known (loaded entity
  or locator data), "—" when unknown; players in another dimension show no
  distance (conversion out of scope).
- Self first in the accent color, then names case-insensitively. Names cut
  with an ellipsis at 16 characters' width (10 when one column does not fit).
  Count header. Up to 20 rows per column, as many columns as fit, then "and
  N more"; no scrolling. Works in singleplayer.
- HUD layout element at the top center; background card by default (card /
  per line / none). No Minecraft texture is copied into the repository.
While a key is held, show the players currently in the world. Default key
candidate Tab, freely rebindable through Lamium's key settings (never fixed);
check conflicts with vanilla and Lamium keys. Names first; head icons or
latency if the client has them. Source: the player list the server sends the
client (`Level::getPlayerList()`, already read by the minimap), not the
entities in render distance; do not guess missing data. Open: layout, wrap or
scroll for many players, what singleplayer and different servers provide.

### L-130 Search recipes and items by English names too
Done 2026-10-10: built (`8488e3b`, fix `3646a11`) and checked in game. Furnace-type screens stay out of this item.
Kind: Research, then Design. Promoted by the maintainer 2026-10-09 from
their notes; scope and release not decided.
Spec (decided 2026-10-10, maintainer accepted the agent's proposal): one
switch "Search English names too", default on; it only adds matches, never
hides what vanilla finds. A query matches when every word appears in the
item's English display name (the game's en_US strings, resource packs
included) or its identifier without the namespace (`oak_planks`, with `_`
read as a space), ignoring case. First scope: the crafting screen's recipe
book, crafting table and creative search (one filter); furnace-type screens
later. Not included: kana readings, names given in an anvil.
Status: open. SDK findings 2026-10-10 (headers only, nothing run): the
crafting screen filters each item through
`CraftingContainerManagerModel::_filterByText(ItemInstance const&,
TextSearchMode)` returning `FilterResult` (Show/Hide...), with the typed
text in `mSearchString`/`mCaseFoldedSearchString`; a hook could retry a
hidden item against its English name. English strings:
`getI18n().getLocaleFor("en_US")` gives a `Localization` whose `get(id,
out, params)` looks up a key (the item's description id + ".name").
`FurnaceContainerManagerModel` has its own search string. The OreUI recipe
book is not covered by this path. Next: a trace build to check that the
en_US locale is loaded while another language is active.
Keep the game's display language but let recipe and item search also match
English names. Candidate modes: Game language (vanilla), English, Both; the
modes are a proposal, not a decision. First target: crafting screen and
recipe book. Research first: where the search filters
(`CraftingContainerManagerModel::setSearchString()` / `_filterByText()`, the
OreUI recipe book search), whether the `en_US` names can be read
(`I18n::getLocaleFor("en_US")`, item description ids) with resource packs,
reload on language or pack change, and items whose description id does not
match the shown name (potions, custom names). Prefer adding English matches
at the filter over reimplementing the game's search. Open: UI, modes and
furnace-type screens.

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

### L-294 Water icons in Lamium screens draw untinted white

Kind: Design. Maintainer report 2026-11-01 (clarified the same day): the
water icon in the target card and in the schematic materials list is
white. Ghosts in the world and the 3D preview are colored correctly. The
icon is the water block's own item drawn by the game's item renderer
(`describe` in `GhostBlocks.cpp`, `blockIcon` in `TargetInfo.cpp`), and
water's texture is gray until the game tints it by biome, which the item
render does not do. Faithful to the texture, but it reads wrong.
Decided 2026-11-01 (maintainer): tint the water icon water blue, rather
than showing a bucket or leaving it. Lava's texture is already colored.
Approach (not built): water and flowing water draw as the water block's
own texture through the texture icon path (`imageUv`, `Widgets.cpp`),
whose `ui_textured_and_glcolor` flush multiplies by a color; pass the
same fixed water color the world and preview shells use (`liquidShell`,
`GhostBlocks.cpp`), shared from one constant, not the biome color. The
target card already has texture icons (`blockIcon`); the materials list
draws item icons only and needs the texture case. Kind: Ready (strong
model) once this approach is accepted.
Status 2026-11-01: built (`1f392e99`), then animated: stepping the
source file's frames (`7c555d9b`) was choppy, so water draws its
`atlas.terrain` tile, which the game animates and blends (`f08f04a0`).
`waterColor` (`LiquidName.h`) is shared with `liquidShell`;
`textureIcon`/`blockIcon` (`TargetInfo.cpp`) give water a tinted texture
icon, and `drawWaterIcon` (`InfoHud.cpp`) draws a water stack that way in
the card's schematic rows and every SchematicsView item icon (materials,
Check). In game: the card aimed at water or a water ghost, and the
materials list of a schematic with water, show a blue water icon.

Closed 2026-11-01: checked in game on `f08f04a0` (local) -
VALIDATION-LOG 2026-11-01.

### L-264 Target View: schematic water and flowing water
Kind: Design details, then Research. User feedback 2026-10-26 (same thread
as L-262): show in Target View whether a schematic cell holds a water
source or flowing water, and how far it flows.
Decided (maintainer 2026-10-26): ghost liquids are always targetable when
the crosshair meets them (the card shows the liquid and source/flow level
from `liquid_depth`). Targeting real water and lava is a separate option,
off by default; with it off the crosshair passes through real liquids as in
vanilla.
Open: how a ghost liquid in front of a real block or another ghost is
chosen, the waterlogged second layer (a ghost block that should also hold
water), what "flow level" reads as in all four languages, and the new
option's place and help. Research: the liquid ray test for real liquids.
Release target: 0.3.1, batch L-266.
Closed 2026-10-26 (maintainer): the waterlogged second layer gets no
Target View display of its own; the check's `waterlogged` state row
already tells whether a cell's water is right, without more on the card.
The rest was built and checked in the L-266 batch (`0f06b820`, VALIDATION-LOG
"L-266 schematic batch, stages 1 to 3").

### L-258 Fuller schematic check controls (reporter's request)
Closed 2026-10-26: answered by the reporter on 2026-10-26:
ignore signal state, plant growth and lit/unlit forms (L-262), pick the
aimed ghost's block (L-263), schematic water in Target View (L-264).

Kind: Design, waiting for the reporter. Split from L-252 on 2026-10-26: the
reporter of L-252 appears to want more than hiding the marks (the maintainer
is asking them for details). Record their answer here before any design;
L-183 (temporarily disregard selected mismatches) may overlap.

### L-238 One set of schematic operations for the menu and the keys
Kind: Design (strong model). Maintainer 2026-10-22: make the complex
schematic operations easy to understand and use whether the player works
from the radial menu, from keys, or both; neither path may be required for
an operation. The adopted principles, menu layout and key groups are in
[SCHEMATIC-CONTROLS.md](SCHEMATIC-CONTROLS.md) ("Menu and key parity").
In short: every operation is reachable from the menu alone and from keys
alone; accelerators for one path (open menu, Repeat, cycle target) need no
counterpart; the menu and the key settings share names, groups and order;
a menu item shows its bound key. The menu goes from eight categories to six
of five or six items; eight new actions are appended, all unbound. Default
keys stay Alt+Wheel only. Watch long names: every ring plate takes the
widest label, so one long label grows the whole ring (see the spec).
Implemented 2026-10-22 on `main` (as-built notes in the spec); build and
pure tests pass, including the parity check and a long-name check that the
ring fits 480x253 GUI units in English, Japanese and Chinese. Closed
2026-10-22: in-game check passed on `41d3ea7` (DLL `CC1A5422…`). Further
adjustments are expected only from use and get their own items.

### L-234 Replace the six-direction layer model with a cut plane
Kind: Design. Closed 2026-10-22: merged in PR #37 (merge commit `75152f1`)
after two review-fix rounds, each checked in game (last on `27311d8`, DLL
`5DE1B13A…`). First in-game check 2026-10-21 on DLL
`109C3201AB961F9231F4C66B8FDA8927D5C3FA97E05A3E971F58AB00A2AB585C`. Maintainer 2026-10-20: the six
directions will not last; move to Axis (Y/X/Z) + Cut + Keep + Mode as
recorded in [SCHEMATIC-CONTROLS.md](SCHEMATIC-CONTROLS.md). Cut is a
structure-local offset along the axis, so moving, rotating or mirroring a
placement keeps the same build cells; check and materials keep reading the
same region. A single plane cannot express a compound X+Y cut; documented
limitation.
Decided 2026-10-20: storage keeps document `version: 1` with a per-placement
`layerView` object (the old six-direction `layerAxis` cannot be reused) and
always dual-writes the exact legacy fields when the size is known; the old
fields are placed-box based and are mapped through the current rotation and
mirror. Cut edits wait until the size is known, so the legacy fields never go
stale. The Layers menu is Axis / Cut / Mode / Keep / Cut here; Show all is
dropped because Mode has All. Keep is a stepper. Cut here keeps the axis and
keep and sets the cut at the view source's layer (switching All to Only).
Changing the axis keeps the cut, clamped to the new extent. Check and
materials keep reading the same region. Decided 2026-10-21: the view slice is
removed (it duplicated Axis + Cut here and changed Only/Keep as a side
effect); rotation and mirror are structure-attached.
Built: pure conversion from every old direction and mode, the `layerView`
storage form with legacy dual-write, the structure-local Layers menu, and the
HUD/placed-screen values (Axis, Cut, Mode and Keep). Layer toasts name the
operation and value; the ghost key only covers visible layer fields. Tests
cover the all-direction/mode cell match under every rotation and mirror, the
legacy round-trip, missing-structure saves and placement-attached cuts.
In-game check passed: structure-local cuts, axis changes keeping the cut,
attached rotation/mirror, the five-item Layers menu, the three layer keys,
operation-naming toasts, and no rebuild for layer fields that do not affect
visibility. A new-form cut plane blocks turning, resetting, layer keys and cut
number input while the structure is missing; the saved layer view stayed put.
A missing structure is still only reported after selecting the placement, not
on world join or in the Placed list (L-237). L-231 and L-234 are merged.
L-236 covers per-section ghost reuse.

### L-231 Make schematic controls easier to find and review directional keys
Kind: Design. Combines two maintainer UX notes from 2026-10-20 in the
[Notion idea inbox](https://app.notion.com/p/3f7c12ab5b2d8121b2a0db32608f6b7a).
Closed 2026-10-21: merged in PR #36 (merge commit `780bfba`). Build and pure
tests pass, and the in-game checklist passed on build `89c7925` (DLL
`20580B81…`). The Open button part was checked in game earlier; the control
rework is in main.
Decided there: one move target shared by keys and menu (placement, corner 1,
corner 2, area) with a target key and a looked-at target; cycle only valid
targets, with an explicit no-target state and fallback order after delete or
clear; the looked-at selection follows the drawn camera ray, picks a corner
cell even in the air or behind see-through blocks, then the nearest placement
box, then the save area box, with fully opaque blocks hiding targets unless
the target overlaps that block; repeat repeats only the last menu adjustment
(movement, rotation/mirror, layer index) and not key actions, stores the
adjustment kind and lets the wheel choose the sign, and is cleared when the
target changes; the "Repeat every menu step" option (default off) adds the
layer axis and mode; corner keys stay separate; the only default binding is
Alt+Wheel up/down for forward/back; corner defaults and an edit mode are
postponed; the Area category's move items may become redundant.
Polish adopted 2026-10-21: the settings group is "Move target and placement";
the target key is "Select the next move target" next to "Select the next
placement"; the failure message names the move target; setting corners uses
the camera's view, and move-to-feet/layer-here use the camera's virtual feet
in Freelook/FreeCamera; the schematic menu's small lower-right layout is the
default.
L-234 follows as a separate step.
Existing controls include wheel steppers in the radial menu, a held Repeat
the last adjustment key plus wheel, six one-block directional Press actions
and move-to-feet. Dedicated actions are unbound by default and grouped as
detail shortcuts. Whether users know the wheel/repeat paths is unconfirmed.
Goal: make the usual placement adjustment discoverable and explain what it
will do, without making users bind or learn every directional action.
Review menu hints, key-setting labels/help and the operation guide, including
whether their text fits. Separate a missing capability (L-230's Y-axis
movement) from an existing operation that needs a clearer explanation.
Additional maintainer feedback, 2026-10-20: settings offers a direct Open
button for the calculator (L-220), but no equivalent button for the schematic
radial menu. Decided 2026-10-20: add direct Open buttons for tools that lead
to a separate screen, including the schematic menu, as a way to try a tool
without binding a key; do not add a new section for them. Sidebar entries
exist for most tools, but a first-time user may not connect the sidebar to a
different screen. If text buttons crowd the rows, use a link-style Open icon
instead.
Placement (maintainer 2026-10-20): the Open button belongs on the opener's own
action row, not on a feature row with a switch, so no row needs both. The
world map's opener is the "Open the world map" action row (M) under the World
map feature; that feature row keeps the map switch and its toggle key. The
schematic menu and calculator are features whose primary action is the
opener, have no switch, and carry the button on the feature row. Sidebar
tools have the same opener action rows (Open shapes, Open waypoints, Open the
schematics screen, Open hotkeys, Open HUD layout).
Layout state: the State column is 24 px, enough for a switch but not for the
word "Open"; the calculator's button therefore overlapped its own frame. The
button is 38 px wide, borrowing the gap and a little of the name cell, and
passed its in-game look.
Decided 2026-10-20 (final): (b) the sidebar tools are included; the schematic
menu closes the settings view and opens over the world; keep the text
"Open", not an icon. Only the main opener gets a button; the four Schematics
tab variants keep their keys.
Built 2026-10-20: `opensTool()` lists the eight main openers (shapes,
waypoints, schematics screen, hotkeys, HUD layout, world map, calculator,
schematic menu) and is tested in SettingsRowsTests. Their action rows, and
the calculator/schematic-menu feature rows, draw the 38 px "Open" button in
the free State cell; `openTool()` takes the in-settings path for the views,
the map view for the world map, and the radial menu for the schematic menu.
The radial menu is a mode of the same screen, so entering it hides the table
and shows the menu over the same world background; closing the menu closes
the screen, as when it is opened from gameplay. The in-game look passed.
Review follow-up 2026-10-20: the Open World map button now goes through
`selectNav(worldMapNav)`, so outside a world it refuses with `worldOnly` like
the sidebar instead of opening the map; this passed in game the same day.
Open: whether the buttons crowd the rows once seen.
Wheel chords (maintainer 2026-10-20): a Press action can already be bound to
a wheel chord in the key settings; Left Alt + Wheel Up/Down assigned to the
schematic away/closer actions fired as expected. Hold actions reject a wheel
token by design. So the wheel path is a discoverability gap, not a missing
capability: the capture footer now names wheel directions and says a wheel
cannot be held. Name truncation and the hover text were fixed as L-235
(BACKLOG-DONE.md).
Open: whether directional keys earn their UI weight for precise one-shot or
macro use; whether the menu/wheel/repeat workflow should be the primary
path; and which explanation belongs where. Preserve existing saved action
IDs and bindings; do not remove shortcuts merely because their use is
unmeasured. Decide retention/grouping with the maintainer before changing
the UI. Item-based operation or a dedicated edit mode stays an unchosen
long-term Notion idea, not an implicit part of this task. L-111 may reuse
settled spatial controls later.
Files to inspect: `src/input/Binding.h`, `src/ui/SettingsRows.h`, menu/input
code and three-language help; [SCHEMATIC.md](SCHEMATIC.md) owns the contract.

### L-111 Integration between features — 0.2.0 plan
Archived 2026-10-20: the required 0.2.0 steps passed the recorded regression checks and two outside reviews and shipped on 2026-10-11. The old pending-release statuses below are superseded. Optional integration and the 2026-10-13 spatial-control input remain open under L-111 in BACKLOG.md.

Kind: Tracking item for 0.2.0 (the Design is settled below; the building
steps are their own items: L-136, L-138, L-139, L-134, L-137, L-135).
Raised by the maintainer 2026-10-08 after checking the 2026-10-07 batch.
Why: the features have matured on their own, and the links between them have
weakened recently. Lamium is one mod, so they can work together more.
Examples from the maintainer:
- Map and Schematics: placements show on the maps (L-93 follow-up), but the
  map cannot toggle their display or edit them the way it edits waypoints.
  The same applies to Shapes.
- The radial menu built for Schematics' many operations could serve the
  whole mod's controls.
- Block Restrictions (L-15) and similar overlays share drawing problems
  (L-110).
Constraints: the module split has real benefits (independent lifecycles,
fail-open per feature, smaller blast radius). Decide with the maintainer
what to integrate, the risks of each step and the order, before any
implementation. Output: a short plan (possibly a demo) that turns into Ready
items.
Status 2026-10-11: steps 1-6 built on main (see the 0.2.0 regression
checks); the in-game checks per step are what is left before the release.
Earlier status: agent's proposal written 2026-10-08 in [INTEGRATION.md](INTEGRATION.md)
(shared parts features register into: face drawing, map layers, a Lamium
radial menu, looked-at selection, settings cross-links; suggested order and
open questions).
Decided 2026-10-11 (maintainer, on the agent's recommendation): 0.2.0's
theme is clean-up and integration together.
- Boundary: shared parts that features register into (INTEGRATION.md
  principle); no feature merges outright.
- Order (revised 2026-10-11 after an outside review): tidy the place an
  integration touches right before it, in separate commits from the
  integration itself (a behavior change must be traceable to one of them,
  as the L-73 periodic-input regression showed).
  1. L-136 split GhostRenderer.cpp (on its own, behavior unchanged).
  2. L-138 shared face drawing; move Shapes and the breaking restriction
     onto it, and the schematic area frame only.
  3. Tidy what map layers touch: WorldMap.cpp's selection and action model,
     and the Shapes view and waypoint list state in SettingsScreen.cpp (the
     first part of L-134).
  4. L-139 map layers: schematic placements first, then Shapes.
  5. The rest of L-134: Hotkeys, the schematic view and the plain settings
     table.
  6. 0.2.0 regression checks (Pre-release checks), L-137's required part,
     L-135 PDB as a separate asset.
  L-137's inventory runs alongside from the start.
- Required for 0.2.0: L-136, L-138, L-139, L-134, L-135 and L-137's required
  part. C radial menu, D looked-at selection and E settings cross-links go
  in when the maintainer feels like it, in 0.2.0 or later (C needs a demo
  first). Effort is not a reason to cut scope (maintainer 2026-10-11:
  agents make it cheap); behavior risk is what the order and checks guard.
- Fail-open is per risky capability, the same rule as L-137, for features
  and shared parts alike: a native path that cannot be verified stops for
  every feature that uses it, while pure data handling and unrelated paths
  keep working (a map layer's data does not stop because a render path
  failed). A feature that fails leaves the shared part working for others.
- Each step is checked in game and recorded (VALIDATION-LOG) before the next
  starts, so a regression is bounded to one step's commits.
- Shapes on the minimap are an option (maintainer 2026-10-11): many shapes
  would crowd it. Still open, decided when the step comes: one Lamium menu
  key or per-feature keys into the same menu (C).
Input from the maintainer's notes (2026-10-13): when levelling ground the
existing features would help but take too long to set up. Aim for calling
up the needed action while working rather than configuring a feature
first. The schematic's spatial operations (a base point from the looked-at
block or the feet, two points for a range, a Y boundary moved with the
wheel, the radial menu and "repeat the last adjustment with the wheel")
could serve Breaking restrictions and Shapes too. Keep features
independent; never mix "the layers shown" with "the region where an action
is allowed"; key conflicts, contexts and UI weight to be checked.

### L-169 Item Transfer — 0.2.3 part and planning history
Archived 2026-10-20: the held-stack and drop gestures shipped in 0.2.3; the normal local chest checklist passed on `9da5d9d` (2026-10-15). The earlier 0.2.2 completion record remains under Ready. The dated proposals and handoff below are history; current unchosen extensions remain in BACKLOG.md and runtime gaps in VALIDATION.md / ITEM-TRANSFER.md.

Rework for 0.2.2 (maintainer 2026-10-15, built by Claude; passed locally
and on BDS on `37c3745`, VALIDATION-LOG): the 0.2.2 gestures moved items visibly one by one because each request
waited for its answer. A trace of a resource pack's bulk move (VALIDATION-LOG
2026-10-15) showed vanilla auto-place called for every source slot in one
frame and answered in one response. Requests now go out together, up to 64
per tick, and are confirmed once (DESIGN "Inventory transfer"). Check in
game before releasing 0.2.2: chest everything both ways, matching (coal),
hotbar kept, Shift + right drag, a furnace and a brewing stand: passed
locally on `7586995` (maintainer); a server is not checked. Follow-up the same
day: "everything" with a hotbar slot hovered moves the hotbar alone, and
in the inventory screen the whole hotbar side: passed on `37c3745`, with
both checklists repeated on BDS without problems.
Kind: Design for the remaining parts. Requested 2026-10-12; extends
L-41/L-103 (DESIGN "Inventory transfer"). The selected 0.2.2 part is done
and locally smoke-tested on c8c1f1b (2026-10-15), unreleased; its contract
and completion record moved to [BACKLOG-DONE.md](BACKLOG-DONE.md#l-169-item-transfer--022-part).
Implementation and evidence: [ITEM-TRANSFER.md](ITEM-TRANSFER.md).
Broader screen/server and edge-case coverage stays in Pre-release checks.
The proposals below are retained history for later parts; they do not
override the completed 0.2.2 contract or authorize additional implementation.

Goals:
1. Screens that do not work today: dispenser, dropper, hopper, the furnace
   family (furnace, blast furnace, smoker), brewing stand, and others found
   while testing (crafter, beacon, shulker box to check). Check each first:
   the current transfer acts only in "storage screens".
2. Bulk transfer: move everything, or everything of one kind, between the
   container and the inventory with one action.
Vanilla already has take-all and place-all handlers in the container
screens (`ContainerScreenController::_handleTakeAll`, `_handlePlaceAll`,
`_handleAutoPlace(amount, ...)`, the per-screen `_registerAutoPlaceOrder`
and `_registerCoalesceOrder`); the maintainer reports that only a
controller binding reaches them and that a resource pack can show them.
Research first whether Lamium can call them from a key or gesture (queued
to the client thread, AGENTS rule 2) and what they move exactly; prefer
them over Lamium-built sweeps where they do the right thing.
Agent's proposal (not decided):
- Routing for slots with roles: furnaces send fuel to the fuel slot and
  smeltable items to the input (an item that is both: input first, as
  vanilla's shift-click; to check), and only take from the output; brewing
  stands send bottles, the ingredient and blaze powder to their slots. The
  existing four gestures keep their meaning; a gesture with no valid
  destination does nothing.
- Bulk: a key or modified click on the container side "take all", on the
  inventory side "put all"; with the hovered item, only that kind. An
  option keeps the hotbar out of "put all" (default: hotbar kept).
- Same safety as today: one request at a time, confirmed from
  authoritative state, stops on screen close, cursor item or a rejected
  request.
Gestures (maintainer 2026-10-12: base them on a widely used Java
inventory-gesture set, read from its public description only, PROVENANCE;
also bulk "matching" moves and drop operations). Today's four gestures
already match that set. Proposed additions (agent, not decided):
| Gesture | Effect |
|---|---|
| Ctrl + wheel | every stack of the hovered kind to the other side |
| Ctrl + Shift + wheel | everything to the other side |
| Alt + left click | same as Ctrl + wheel, as a click |
| Alt + Shift + left click | same as Ctrl + Shift + wheel, as a click |
| Shift + right drag | all but one item from each passed slot (vanilla's Shift + right drag today) |
| Shift + click an empty slot, items on the cursor | every matching item into that side |
| Shift + click outside the window, items on the cursor | drop every matching item in the inventory |
| Drop key + Ctrl + drag | drop one item from each passed slot |
| Drop key + Shift + right drag | drop all but one from each passed slot |
| Drop key + Shift + left drag | drop everything in each passed slot |
| W or S held + Shift/Ctrl click (and drag) | move the stack (or one item) up or down within the inventory |
Drops go through vanilla's own drop request and are confirmed like moves;
each drop gesture has its own switch, and "drop every matching item"
default off (it can throw away a whole inventory). Check on Bedrock before
choosing: whether Alt reaches the game in screens, what the drop key does
in screens today, and that W/S are free while a screen is open.
Decided (maintainer 2026-10-12): take the matching/everything moves (Ctrl
+ wheel, Ctrl + Shift + wheel, the Alt clicks, Shift + click on an empty
slot), all-but-one (Shift + right drag) and the drop gestures; not the W/S
moves for now. First screens: the furnace family and dispenser, dropper and
hopper; the brewing stand next.
Open (maintainer): defaults per gesture (agent: on, except "drop every
matching item" off), hotbar handling in "everything" (agent: hotbar kept),
and whether "fill existing stacks only" is wanted as well.

Update 2026-10-15 (maintainer): L-154 is folded in here and the work goes to
another agent; this entry is its brief. In scope for the next release:
1. Matching moves and all-but-one, as decided on 2026-10-12 (Ctrl + wheel,
   Ctrl + Shift + wheel, the Alt clicks, Shift + click on an empty slot,
   Shift + right drag). On 2026-10-15 the maintainer also agreed to "move
   every item of the hovered kind" and "move the stack but leave one", which
   the gestures above already cover. The agent's 2026-10-15 alternatives,
   Shift + double click (every item of the kind) and Ctrl + Shift + click
   (the stack but one), are **not decided**: ask whether they are wanted as
   extra bindings before adding them.
2. More screens and slot roles: furnace, blast furnace, smoker, dispenser,
   dropper and hopper first, then the brewing stand; check crafter, beacon
   and shulker box. Route by slot role (fuel, input, output; bottles,
   ingredient, blaze powder) as proposed above, taking only from outputs.
3. Drop gestures as decided, each with its own switch ("drop every matching
   item" default off unless the maintainer says otherwise).
Still open for the maintainer: the defaults per gesture, the hotbar in
"everything", "fill existing stacks only", and the two 2026-10-15
alternatives.

Update 2026-10-15 (maintainer, later): the gesture set itself is not final.
The maintainer will settle it with the implementing agent, comparing it with
reference mods (listed in PROVENANCE.md, reference only) and with how it
plays. Treat the gestures above as the starting proposal, not a contract.

Update 2026-10-15 (maintainer, answering the implementing agent): this
round builds (1) Shift + click on an empty slot with items on the cursor:
every matching item into that slot's side, and (2) the drop gestures
listed above (drop key + Ctrl drag: one from each slot; drop key + Shift +
right drag: all but one; drop key + Shift + left drag: everything; Shift +
click outside the window with items on the cursor: drop every matching
item). Each gets its own switch and **all default on**, including "drop
every matching item" (the maintainer's choice over the agent's proposal of
off). Not this round: the Shift + double click and Ctrl + Shift + click
alternatives and "fill existing stacks only". Research what the drop key
does in screens today before building the drop gestures.
For (1) the cursor stack goes into the clicked empty slot first, then every
matching item from the other side follows to that side; the hand ends
empty (maintainer 2026-10-15). Research build: `f3501da` with
`--transfer_trace=y` logs vanilla drops (raw ItemTransferAmount bytes, whose
fields the SDK does not name) and keys in screens; its log decides how the
drop gestures call vanilla's drop.
Trace result (2026-10-15, VALIDATION-LOG): the drop key over a slot drops
one, Ctrl + drop key the stack, a left click outside the window drops the
held stack and a right click one item; all go through the same drop with
bytes `01000000 02000000 00` (one) or `01000000 00000000 00` (stack). Shift
+ click on an empty slot with items held did nothing in vanilla (an
auto-place that moves nothing). Built on that basis in `9da5d9d`; the
maintainer's chest checklist passed the same day (ITEM-TRANSFER.md "Held
stack and drops"). Other screens and servers go to Pre-release checks.

Handoff for the implementing agent (read AGENTS.md first; implementation of
the 0.2.2 contract may go to another agent; product questions come back to
the maintainer):
- Current behaviour and rules: DESIGN.md "Inventory transfer" (the four
  gestures, one request at a time, stop conditions); L-41 and L-103 in
  BACKLOG-DONE.md; VALIDATION.md rows for Inventory transfer.
- Code: `src/features/inventory/transfer/TransferGesture.h` (pure gesture
  and side rules, tests in `tests/TransferGestureTests.cpp`);
  `src/features/inventory/game/TransferSession.cpp` (requests and their
  confirmation); `src/features/inventory/game/ScreenTracker.cpp` (which
  screen is open; keeps `_isContainerScreen()` controllers, including the
  traced furnace, brewing stand and beacon; sorting has its own type guard);
  `InventoryMove.cpp` and `RequestTracker.cpp` for the move requests;
  `TransferRoutes.cpp` discovers native roles and scopes destination filtering.
- Keep pure planning (which slots, in which order, by slot role) in headers
  with tests; game glue in .cpp. Never touch inventories from the input
  event (queue to the client thread, AGENTS rule 2); confirm each move from
  authoritative state, not from the request (rule 9).
- New settings follow AGENTS rule 4 (Settings, Options, SettingsStore,
  SettingsRows, three languages); a new hook gets a GAME-UPDATES.md entry.
- Research before building: whether vanilla's take-all / place-all /
  auto-place handlers (`_handleTakeAll`, `_handlePlaceAll`,
  `_handleAutoPlace`) can be called and what they move; whether Alt reaches
  the game in screens; what the drop key does in screens today.
- End with a short in-game checklist per screen and gesture (local and a
  server), as AGENTS.md asks.

### L-154 Additional inventory transfer gestures
Archived as merged, 2026-10-20: L-169 owns this work. Its matching, keep-one, held-stack and drop gestures shipped in 0.2.2/0.2.3; extra gesture choices remain Design in L-169. This is not a separate open task.

Merged into L-169 on 2026-10-15 (maintainer): the matching and all-but-one
moves, the drop gestures and more screens are specified there; the work goes
to another agent. Earlier notes kept below.
Kind: Design; choose a concrete gesture before implementation. Bulk
transfer and more screens moved to L-169 (2026-10-12).
Candidates: matching-item bulk transfer, leaving one item behind, modified
clicks, dragging to drop items and keyboard-directed moves. Do not assign
chords or change existing gestures yet. Clarify source/destination, cursor
ownership, modifier conflicts and authoritative completion for each chosen
operation. Dropping needs an explicit user-facing design. Trading support
and Mass Craft remain separate workflows, not implicit scope of this item.
Not implemented; preserve the current storage and inventory-screen behavior.

### L-219 Info HUD coordinate lines and the aimed block's position
Kind: Design. Maintainer 2026-10-18: the "Coordinates" (exact position) and
"Block" (the block the player stands in, whole numbers as the game's own
position line shows) rows explain little, and "Block" is the wrong name.
Also wanted: an Info HUD line with the aimed block's coordinates that works
without Target View. Names and descriptions are the maintainer's choice;
existing settings keys keep their meaning, a new key is tolerated when
missing.
Decided 2026-10-18 (maintainer took the proposed names): "Block" becomes
"Block coordinates" / ブロック座標 (key `information.block` unchanged);
"Coordinates" keeps its name; new line "Target block" / 照準先のブロック
(`information.targetBlock`, default off) shows only the aimed block's
coordinates, looks as far as Target Info's Range, uses the card's pick (the
game's hit from the body, L-217) without schematic ghosts, and is hidden
with nothing aimed at or an entity. Checked in game 2026-10-18 (`cdd2bcd`):
names and values OK; the maintainer asked to revisit the order, and took the
proposed grouping (DESIGN "Info lines") for the settings rows and the
default HUD order. A saved order is not rewritten; a line missing from it
now goes after its default neighbor instead of the end. In game
(`a5edf9a`) the settings rows still showed the saved HUD order: they had
always followed it, and "Reset this element" does not reset the line order
(only the whole-layout reset does). Maintainer 2026-10-18: the settings
rows keep the grouped catalog order, independent of the HUD order.
Checked in game 2026-10-18 (`7856e1e`, reset of the order `5a99b86`): OK.

### L-214 FreeCamera: how the underground view switches
Kind: Design. Maintainer 2026-10-18: since L-207 the spectator culler is
used only while the camera is inside terrain, which is the cheaper choice,
but entering even a small cave rebuilds the view, and some players may
prefer the old way (spectator culler for the whole session, heavier, no
rebuilds). Options: (1) hysteresis, no new setting: switch to spectator on
entering terrain and back only after the camera has been outside for a few
seconds; (2) a child setting choosing "always" (old) or "only inside
terrain" (current). Proposed: try (1) first, add (2) if preferences still
split. Maintainer 2026-10-18: try (1).
Status: built 2026-10-18 on `l214-spectator-hold`: the spectator culler
stays for 3 s after the camera leaves terrain (`SpectatorHold`, tested);
only the primary camera's requests move the hold; ending FreeCamera drops it
at once. Checked in game 2026-10-18 (maintainer): fine; since the right
length depends on render distance and hardware, it became a child setting
of FreeCamera, "Keep underground view" (`camera.freeCameraUndergroundHold`):
0, 1, 3 (default), 5, 10, 30 s or Always (the pre-L-207 behavior), which
also covers option (2). The setting is not checked in game yet.
Done 2026-10-18: merged from PR #24 after the maintainer's in-game checks of
the 3 s hold and of the "Keep underground view" setting (VALIDATION-LOG).

### L-204 Placements and shapes in the world map's side panel list
Status (2026-10-15): the maintainer chose a menu in the panel header
instead of the tabs below and accepted the rest of the proposal (DESIGN
"World map side panel lists"); built in `34d71ca`, passed in game the
same day (VALIDATION-LOG). Rows sort
nearest first like waypoints, not in the feature screens' order.
Kind: Design. Raised by the maintainer 2026-10-15 after the L-146 check:
the side panel's list could show schematic placements and shapes as well as
waypoints, so they can be found and opened without spotting them on the
map. Today the list holds waypoints and the death point; a placement or
shape is reached by clicking it on the map. Open questions for the
maintainer before building: one mixed list or a section (or a filter) per
layer; whether the L-146 layer switches also hide their entries from the
list; whether marks from other dimensions are listed; what a row's switch
does (show/hide, as for waypoints) and what clicking a row does (center the
map and select, as for waypoints).
Maintainer 2026-10-15: separate the layers, with a switch-like control,
since the lists can grow long; asked for the agent's proposal first.
Agent's proposal (not decided):
- A three-part tab row at the top of the side panel, Waypoints /
  Schematics / Shapes, each with its count; the panel shows one layer's
  list. The choice is remembered like the panel itself. Selecting a mark
  on the map switches to its tab. "+" (add a waypoint here) shows only on
  the Waypoints tab.
- Rows as for waypoints: the layer's symbol, the name, the distance when
  the player is in that dimension, and the show/hide switch; a click
  centers the map on it and selects it, with the existing details and
  "Open" button below the list.
- Only the dimension the map shows, as the map marks do; the tab count
  counts those.
- A layer switched off in the top bar keeps its tab and list (the list is
  where it is found); a faint line at the top says it is hidden on the map.
- Order: the order of the feature's own screen (Placed tab, Shapes view);
  a search field when lists grow, later and only if asked.

### L-146 Layer switches on the world map (with the next integration review)
Status: done 2026-10-15. Maintainer chose the look and behavior the same
day (DESIGN "World map layer switches"); built in `5928818` and the
maintainer's in-game checklist passed (VALIDATION-LOG 2026-10-15). The list
of placements and shapes in the side panel became L-204.
Kind: Design. Raised by the maintainer 2026-10-11 before the 0.2.0 release;
parked until the links between features are reviewed again (after 0.2.0).
Today placements and Shapes always show on the world map while their
feature draws them; a hidden one stays faint there so it can be shown
again, so the only way to clear them off the map is turning the feature
off. Agent's proposal: switches for Waypoints, Schematics and Shapes in the
world map's top bar (remembered across openings), rather than a settings
option, since the map is where that choice is made. Add when players ask
for it or when the integration review takes it up.

### L-175 Lamium settings outside a world (title screen)
Done 2026-10-14 (maintainer checks, VALIDATION-LOG): the settings key opens
the Lamium settings on the title screen (`start_screen`) and on OreUI pages
before a world is joined (not the dressing room or Marketplace, which the
maintainer accepted). The title screen also shows "Lamium <version>" on a
dark band stacked on LeviLamina's version line, matched to it from
screenshots; clicking the band opens the settings. Shapes, waypoints,
schematics, the world map and the HUD layout editor are dimmed outside a
world and say "Available inside a world."; settings, keys, the calculator
and the game language work there. Lamium's client-thread queue
(`app/ClientTasks`) now drains from `MinecraftGame::update` before a world
is joined, where LeviLamina's executor does not run; input actions and the
language apply use it. In-world switching keeps its 10 s interval; outside a
world there is none.
Not offered outside a world: the HUD layout editor (it previews the in-world
HUD). Original request:
Kind: Design, then Build. From the maintainer (2026-10-14); not chosen for
building, no release.
Goal: open the same Lamium settings screen from the title screen (and
other menus outside a world) and change settings there, instead of only
inside a world.
Questions: how it is opened there (key, a button on the title screen),
which views make sense without a world (waypoints, schematics, shapes and
the world map need world data; settings, keys and HUD layout do not), and
what the HUD layout editor previews without a HUD. Lamium's screen is
drawn from in-world UI render events today; check what renders on the
title screen (OreUI) before designing.

### L-171 Calculator
Done 2026-10-13: the two-column calculator (calculator-v3.html "B"): field, result with one split copy button, chest amount, a six-column scientific key grid, twenty history cards scrolled smoothly; opened by its key or the settings sidebar (`e664562` .. `de9792d`); checked in game in English and Japanese. Container screens are out of scope.
Kind: Ready (strong model); designed with the maintainer 2026-10-12
(demos/calculator.html). L-172's text editing is in (2026-10-13). Not
scheduled, no release.
Goal: a quick calculator inside the game; playing often needs arithmetic
(stacks, coordinates, farm rates) and there is no way to do it in game now.
Agent's proposal (not decided):
- A hotkey (unbound) opens a small Lamium panel with one text field; the
  result updates while typing. Enter keeps the line in a short history
  (about 10, this session only), a button copies the result. Nothing is
  ever sent to chat or the server.
- Arithmetic: + - * / % ^, parentheses, decimals, `ans` for the last
  result; functions sqrt, abs, floor, ceil, round, min, max.
- Minecraft units as words after a number: `st` (64), `ch` (a chest, 27
  stacks), `sb` (a shulker box, the same as `ch`), `lc` (a large chest, 54
  stacks). Results of 64 or more are restated the way Schematic's material
  amounts are ("39 chests + 7 stacks + 46"), through the same pure helper
  (`MaterialAmount.h`, moved somewhere shared). Ticks and seconds (20 per
  second). Overworld/Nether coordinates (x8, /8) as functions.
- Optional: the player's `x y z` as variables.
- Pure expression parser and unit formatting in a header with tests; the
  panel uses the existing text field (IME already works, L-84).
Decided (maintainer 2026-10-12): a panel opened by a hotkey, not chat. It
needs an agreed mockup first: [demos/calculator.html](demos/calculator.html)
(A centered window, B tall panel on the right, C one line at the bottom;
over gameplay and over a chest screen).
Decided (maintainer 2026-10-12): restatements only for results of 64 or
more, written like Schematic's materials (chests of 27 stacks, stacks,
items; the hint says a chest holds as much as a shulker box); ASCII unit
words `st` `ch` `sb` `lc` (no `dc`); `x y z` as the player's position; 10
history lines kept for the session only.
Opening over container screens is wanted but is a second step: first find
how to keep E, Esc, number keys and the drop key from reaching the vanilla
screen while the panel has the keyboard. Variant A (maintainer 2026-10-12): a
centered window near the top (result, one restatement line, 3 history
lines, older ones through Up/Down); when it opens over a container screen
(second step) it sits left of that window so it does not cover it. Text
editing comes from L-172, so L-172 goes first. Spec complete; Ready
(strong model) once L-172 is in.
Built 2026-10-13 (`e664562`, `bef8059`): variant A, the sidebar item
"Calculator" (opens it; Esc returns), checked in game (VALIDATION-LOG).
Maintainer after use: the hints are too small and cut off; copying only the
value is not enough (the line and the chest amount too); the history lists
newest first while Up goes back, which reads upside down; only three lines
are visible; with the chest-side placement moot, the panel can be larger.
Second mockup: [demos/calculator-v2.html](demos/calculator-v2.html).
Decided (maintainer 2026-10-13, after that mockup): container screens are
out of scope (the second step is dropped); make an ordinary, easy
calculator and use as much of the screen as it needs. Layout 2, two
columns: left the field, the result with its chest amount, copy buttons
(value / "expression = value" / chest amount; Ctrl+C value, Ctrl+Shift+C
the line) and buttons: digits, + - * / ( ) ^ % ans, C, backspace, = (Enter),
a row of st ch lc and x y z, and "fn" switching the digit block to sqrt abs
floor ceil round min max nether ow ticks secs; buttons insert at the caret.
Right the history as cards (expression, = value, chest amount), newest at
the bottom, scrolled, 20 cards for the session only; a click loads a card,
hovering shows its copy buttons; Up goes up through them. Help behind a ?
button, with short tips on the buttons. Third mockup:
[demos/calculator-v3.html](demos/calculator-v3.html).
Decided (maintainer 2026-10-13, on the third mockup): copying sits beside
what it copies, each button saying "Copy": "Copy line" by the field
("20 lc - 1234 = 67,886"), "Copy" by the result (the value) and by the
chest amount (its text); a note "Copied: ..." follows; history cards show
the same buttons on hover. The space under the buttons shows coordinates:
here, and the other dimension's place (Nether: /8 rounded down; Overworld:
x8; the End: here only); a click inserts a number. No item list, no
reference table.
Revised the same day (maintainer, superseding the two points above and
"fn"): one copy button per result, split: the body copies the value, its
arrow offers "Copy the line" and "Copy the chests" (history cards show the
same button on hover). No coordinates block and no variables list; the
function buttons fill that space on the same page (sqrt abs round floor
ceil / min max nether ow , / ticks secs pi), so there is no fn switch;
x y z stay in the row with st ch lc.
Final layout (maintainer 2026-10-13, calculator-v3.html "B"): one
six-column grid of equal keys like a scientific calculator: st ch lc x y z
on top, then sqrt abs ( ) C backspace / round floor 7 8 9 / / ceil min 4 5
6 * / max ^ 1 2 3 - / nether ow 0 . ans + / ticks secs , pi % =; keys
grow taller to fill the column. No switch to a plain calculator (add a
"hide functions" option only if asked).

### L-147 Status Effects HUD
Done 2026-10-12: built and checked in game locally through `ee84063` (VALIDATION-LOG); beacon effects, Pocket UI and servers left for Pre-release checks.
Kind: Design, with Research for native data and vanilla HUD visibility.
Status: imported from the maintainer's notes on 2026-10-12; not implemented.
Goal: make active effect names, levels and remaining time readable in play
and movable through the existing HUD Layout Editor.
- Proposed: replace only vanilla effect HUD drawing with a compact list of
  vanilla icons, names, levels and M:SS times. An icons-and-time style is an
  alternative; infinite effects and short-time warning presentation need a
  design decision. Do not change effect application or time progression.
- Reuse the editor's anchors, dragging, scale, background, shadow and saved
  layout. Show sample effects when editing without active effects. Append
  a new HUD element ID; never reorder existing IDs.
- Research safe local-player effect enumeration, amplifier, remaining ticks
  and infinite duration, then suppression of only the vanilla effect HUD.
  `mob_effects_renderer` is an investigation lead, not a verified hook.
  Leave vanilla visible if data or the replacement path cannot be verified.
- Open: default switch, initial position, list versus icons, ordering,
  overflow, long names and refresh behavior on effect renewal. Review a
  mockup before implementation; keep the first version small.
- Checks when built: all three languages, GUI scales, Desktop/Pocket UI,
  editor preview/save, F1, Debug View, inventory screens, renewal/removal,
  world/dimension exit, resource packs and servers. No runtime evidence yet.
- 2026-10-12: mockup [demos/status-effects-hud.html](demos/status-effects-hud.html)
  (A list, B icons with times, C icon-and-time column; backgrounds, position,
  short-time warning, order, overflow, language) under review.
- Maintainer 2026-10-12: default position right middle (top right and left
  middle already hold Lamium HUDs; vanilla shows effects at the right
  middle). Asked for the settings rows, vanilla HUD suppression and the
  agent's picks in the mockup; added (A list with a "Show names" child that
  gives C, hide vanilla icons while on, color-only warning under 10 s except
  beacon effects, shortest first with infinite last, show all, default off).
  Maintainer: the picks are fine (decided 2026-10-12). Next: research the
  local player's effect list (amplifier, remaining ticks, infinite, beacon
  ambient flag) and how to hide only vanilla's effect icons; then build.
- Research and first build 2026-10-12 (not checked in game). Data: the
  local player's `MobEffectsComponent` (id, duration ticks, amplifier,
  ambient), names from `MobEffect::mDescriptionId` through the game's
  language, icons from `mIconName` (resolved to `textures/ui/<name>`, where
  the vanilla pack keeps `*_effect` icons; a missing texture draws no icon).
  Infinite is assumed to be a negative duration and shows as the infinity
  sign; whether the client counts the time down between server updates is
  unverified. Vanilla icons: `HudMobEffectsRenderer::render` (the hud
  screen's `mob_effects_renderer`) is skipped only in frames where the HUD
  read the component; not gated (skipping a draw writes nothing). New
  element `StatusEffects` (appended), right middle 48 below center (under
  the automation status), card, per-line allowed; `Toggle the Status effects
  HUD` action appended, unbound. Harmful names in the heart red, running-out
  times in the warning color. To check: names/levels/times, ∞ glyph in the
  game font, beacon effects, renewal/removal, vanilla icons gone only while
  on, inventory screen effects untouched, Pocket UI, editor sample, F1.
- First check 2026-10-12 (`95a34d5`): names, levels, order, times and the
  vanilla icons hidden work; no icons drew (the effect's `mIconName` did
  not resolve to a texture) and the infinity sign is missing from the
  Japanese font. Changed: icons from a table of the vanilla pack's
  `textures/ui/<name>_effect` by effect id (instant health/damage and
  saturation have none), and infinite effects read "Infinite" / "無期限" /
  "无限". Checked in game 2026-10-12 (`c670147`): icons and the word show.
- Maintainer: levels looked mixed (V, then 11) and the names-off rows did
  not line up. Decided 2026-10-12: Arabic numerals (level 1 blank) in a
  column of their own; columns laid out by width with no separators: icon,
  name (left), level and time (right-aligned); an unused column takes no
  room; per-line bands span the table. Checked in game 2026-10-12
  (`cd738e5`). Then decided: level 1 shows "1" too. Unchecked.


### L-150 Arrow-count HUD
Done 2026-10-12: ammo counts in the Durability HUD, built and checked in game locally through `ee84063` (VALIDATION-LOG); servers left for Pre-release checks.
Kind: Design (small), then Ready after counting and visibility are agreed.
Status: imported from the maintainer's notes on 2026-10-12; not implemented.
Goal: show the arrows available to the player without opening inventory.
- Proposed: an inventory-wide total, perhaps visible only while holding a
  bow or crossbow. This is a display feature, not offhand replenishment.
- Open: ordinary/tipped arrow grouping, offhand and loaded-crossbow contents,
  visibility condition, and a line in an existing HUD versus a new element.
  Review overlap with Inventory HUD before choosing presentation.
- Keep counting bounded; show only client-observed contents. Test the agreed
  pure counting rules and later verify consumption/refill and HUD behavior.
- Scope survey (2026-10-12, maintainer asked to consider more than arrows):
  bows use arrows and tipped arrows (Bedrock has no spectral arrows);
  crossbows also load firework rockets; rockets boost elytra flight;
  throwables are their own ammo (snowballs, eggs, ender pearls, wind
  charges, splash and lingering potions, bottles o' enchanting), where the
  hotbar shows only the held stack's count; tridents need no ammo. Options
  under discussion: what to count per held item, and where to show it (the
  Durability HUD's held-item row, beside the hotbar slot, or a new element).
- Maintainer 2026-10-12: extend the Durability HUD rather than a new element:
  with a bow the arrows, with a crossbow arrows and fireworks, with elytra the
  fireworks. Tipped arrows count with plain arrows (showing them separately
  is wanted if feasible; a later mockup may settle it).
- Mockup [demos/durability-ammo.html](demos/durability-ammo.html): ammo on a
  small line under the weapon row (on the same row it ran into the hotbar at
  the bottom-left default), tipped arrows as total / total with tipped /
  per kind, a mark on a loaded crossbow, the elytra row while gliding even
  with armor rows off. Under review.
- Decided 2026-10-12 (maintainer): the picks, except the ammo goes on the
  same row, right of the durability. Total only for tipped arrows, red 0,
  a mark on a loaded crossbow's icon, the elytra row while gliding even with
  armor rows off, a "Show ammo" child of Durability HUD (default on). The
  mockup showed a crossbow row reaching the hotbar at the bottom-left
  default; keep the ammo text compact and check the overlap when built.
- Maintainer after checking in game: the real HUD has more room than the
  mockup; no need to shrink the text. Also asked for a per-line background
  for the Durability HUD (only card and none so far); built 2026-10-12
  (band per row, right-aligned on the right side, as the Info HUD).
  First build: "Per line" could not be chosen (settings normalization still
  limited Durability to card/none); fixed, with a test that every offered
  choice survives normalization. Checked in game 2026-10-12: per line works,
  bands line up on the right edge when moved to the right.
- Built 2026-10-12: a "Show ammo" child (default on). Bow rows show the
  inventory-wide arrow total (offhand included, tipped arrows counted with
  plain ones), crossbow rows arrows and fireworks, elytra rows fireworks; a
  zero is drawn in the bar's empty red. A loaded crossbow gets its charge as
  a half-size icon on the lower right of its icon (not counted). The elytra
  row also shows while gliding with armor rows off (only with ammo on).
  Counting is pure (`DurabilityHud.h`, tested) and runs over 37 slots only
  while a row needs it.
- Checked in game 2026-10-12 (`95a34d5`): counts, crossbow mark and gliding
  row work. Maintainer: ammo icons looked larger than the tools, the red zero
  did not match the yellow full used-slot counter, and the gliding elytra
  row deserves its own switch. Changed: ammo icons at three quarters, zero
  in the warning color, a "Elytra while gliding" child (default on) instead
  of tying the gliding row to "Show ammo". Unchecked.
- Checked 2026-10-12 (`c670147`). Measured on the earlier screenshot, the
  ammo icons were about 1.2 times the tools though drawn at the same scale:
  the freshly made icon stacks kept the pickup pop the HUD clears on its
  copies. Now cleared (also on the crossbow mark) and drawn 1:1, with the
  same 4-unit gap before the count as the tools. Maintainer: split armor
  and elytra instead of a gliding row: "Armor" covers helmet, chestplate,
  leggings, boots; "Elytra" (default on) shows a worn elytra at the chest
  position. Checked in game 2026-10-12 (`bc3e830`): same size as the tools,
  the gap, and the separate switches work.
- Maintainer 2026-10-12: the gap between a crossbow's arrows and fireworks
  was too wide (8 units); now 6 between kinds, 8 after the durability.
  Unchecked.


### L-120 Entity counts by kind in Debug View
Done 2026-10-11: built (`75620ed`) and checked in game locally and on a server (VALIDATION-LOG).
Kind: Design. From the maintainer's notes (2026-10-09); extends L-57.
Status: built (`75620ed`). Decided 2026-10-11 with the
mockup [demos/entity-counts.html](demos/entity-counts.html): the breakdown
is one line under the total (setting, default on); the most common types,
top 5 by default (off / 5 / 10), close the left column (the maintainer
preferred the left over the right column), with game names in the game
standard labels and identifiers without `minecraft:` in the Java F3 style.
Kinds come from the actor's categories: Player, ItemEntity, Monster, Mob,
else other. A Java-like pie chart was judged too much for now.
Keep the `E:` total and optionally break it down without double counting,
in this order: players, dropped items, hostile, passive, other, so the parts
add up to `E:`; optionally per identifier (`getTypeName()`, add-on entities
included). Same source and cadence as the L-57 count (the client's actor
list for the player's dimension, once a second). Dropped items count
entities, not stack sizes; block entities are not actors. Open: a separate
small panel or lines inside Debug View, sorting and how many identifiers.

### L-127 Inventory HUD and slot counter
Done 2026-10-11: built (`1789bee` to `7906581`) and checked in game by the maintainer (VALIDATION-LOG).
Kind: Feature; Design done (maintainer 2026-10-10), revised after the first
in-game use (2026-10-11).
Status: built (1789bee, 603ca1b); first build checked in game 2026-10-11.
Design settled 2026-10-11 (mockup version 3); the latest build is unchecked.
Mockup: [demos/inventory-hud.html](demos/inventory-hud.html) (version 3).
Two HUD layout elements, each with its own switch (both default off) and a
toggle hotkey (unbound by default):
- Inventory grid: the main 27 slots as 9x3 in the vanilla inventory screen's
  order; a setting adds the hotbar (36) as one more row below a gap.
  Armor and offhand are not included (equipment has its own display). Every
  slot has a faint frame around the whole 18-unit cell, clear of the icon
  (look D; a frame on the icon's edge felt cramped); empty slots show only
  that. Items use the shared
  vanilla-like icon path (`inspection/render/ItemIcon`) with stack counts,
  durability bars and glint; the shield glint is still missing (L-91).
- Used slots (使用中の枠, 已用格子): a separate small element showing
  used/total such as "13/36", in the palette's warning color when full. The
  total shows the counted range and makes the purpose plain (chosen over a
  free count, 2026-10-11). It works with the grid off.
- Size: one slot is 18 units like the hotbar at 100 %; the layout editor's
  75-150 % scale applies. Backgrounds: card / none (per line looked the same
  and was dropped, 2026-10-11).
- Shown whenever switched on, hidden while any screen is open.
- Default position: bottom right, the used-slot counter in the corner and the grid
  just above it (chosen in game 2026-10-11; clear of the hotbar).
- Hotbar: one "include the hotbar" switch per element; the grid shows the
  main 27 by default, the counter counts all 36 (decided 2026-10-11).

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

### L-256 Schematic ghosts: lower the in-world drawing cost
Kind: Research, then Ready steps. Maintainer note 2026-10-26 after 0.3.0:
drawing ghosts in the world is heavy, and seems heavier with many check
mistakes. The last measurements (SCHEMATIC.md C, 2026-10-09; L-105 and
VALIDATION-LOG "Perf trace of `2965ec8`", 2026-10-14: 3086 sections, 4-6
ms a frame standing still, mostly per-section draws) predate the bar
outlines of L-186 and the later blended-mesh changes (L-192/L-193, mistake
boxes in that mesh), so they no longer describe the current cost. The
candidates below come from reading the code, not from a measurement.
Related: L-105 (whole-game profiling), L-236 (rebuilds when the cut moves).
Step 1 (first, Research): measure again with `xmake f
--schematic_perf_trace=y` on current main: a large placement with no
mistakes, one with many mistakes (red/yellow areas), and both with Outline
width Thin. Record frame time per stage (faces, lines, blended, check/scan,
rebuilds, re-sorts) in VALIDATION-LOG, then pick the steps below by cost.
Candidates (`GhostMesh.cpp`, `GhostRenderer.cpp`):
1. Outlines: Normal/Thick width draws each edge as 32 vertices
   (`faces::barQuads`, four double-sided quads), 384 per outlined cell,
   and an edge shared by adjacent ghosts is drawn once per ghost (only a
   weaker color rank is skipped). Draw each edge once.
2. Mistake boxes: faces hidden between touching marks are collapsed to a
   point, not removed, so dense mistake areas still sort, merge and upload
   them in the blended mesh; above 20000 quads that mesh re-sorts and
   uploads every 0.25 s while moving. Remove collapsed quads and merge
   same-color runs into larger faces (marks use one texel); every mark
   with an open face also adds an outline (candidate 1 applies).
3. Collapsed quads elsewhere (`dropDuplicateQuads`, ghost-to-ghost culling)
   stay in the face and blended meshes: compact them before upload,
   keeping every vertex stream aligned.
4. Draw calls: one face draw per 8-block section (about 4900 sections cost
   1.9 ms on 2026-10-09). Keep 8-block rebuilds but draw static sections
   in larger groups.
5. The verification scan (about 1 ms on 2026-10-09) is limited by count,
   not time: give it a time budget. (Done, `bd10d7d4`.)
6. Rebuilds around the camera while walking (found 2026-10-26): each
   new camera cell made every section within two cells due. Done for
   walking outside ghosts (`7c0a7cb2`: only sections with a ghost drawn
   within two cells; about 280 rebuilds per 5 s down to 0-15).
7. Walking inside ghost blocks (found 2026-10-26, long-standing): each
   cell crossed still rebuilds 1-8 dense sections at 15-35 ms each. Ideas:
   (a) build the camera-dependent cells (within two cells of the camera)
   as a small mesh of their own and keep sections camera-independent,
   which needs the camera cell's ghost left out of its section without a
   rebuild (Research); (b) wait until the camera has stayed in a cell for
   a moment before rebuilding (cheap, but faces near the camera may look
   wrong briefly while moving).
   Chosen 2026-10-26 (maintainer): (a). Research done the same day, from
   `GhostMesh.cpp`/`GhostRenderer.cpp` at `28e43baf`:
   - Every camera effect is confined to ghost cells within two cells (per
     axis) of the eye or feet cell (`BuildCamera::cells`): the enclosed
     skip (`camera.near(cell, 2)`), `cullAgainstGhosts` (`nearCamera`,
     `beyond` from the eye), the tessellator's neighbor answers
     (`ghostNeighbor`, reach 1), `dropBackFaces` in the camera cells, and
     the liquid shells through those neighbor answers. Lines and mistake
     marks do not depend on the camera.
   - Plan: build sections with an empty `BuildCamera` (no cells), so they
     never depend on the camera, and record each cell's first vertex in
     the face mesh (cells are appended x, y, z with z fastest; liquids go
     to the blended tessellator). A section touching the near box (the
     union of the two 5x5x5 boxes) is drawn in vertex ranges that skip the
     box's cells (`renderMesh` takes a start and count; at most about 30
     runs, for at most 8 sections).
   - A near mesh per placement holds the box's ghost cells built with the
     real camera (about 150 cells instead of up to 8 dense sections of 512),
     rebuilt when the camera cell changes, drawn after the sections.
   - Blended quads are re-sorted far to near at build time, which loses
     the cell order: give `BlendQuads` a cell index per quad, leave the
     near box's quads out when combining/sorting (and draw a section's
     own blended mesh from the filtered CPU quads when it touches the
     box), and add the near mesh's blended quads.
   - Then the camera-cell marking in `drawPlacements` (`7c0a7cb2`) goes.
   - Check in game against L-192/L-193 (no flicker between sections or
     placements), inside doors/stairs/spawners (L-178 back faces), glass,
     waterlogged blocks, and the trace numbers while walking inside
     ghosts (16-38 ms frames now).
   Built 2026-10-26, not yet checked in game: `NearCells.h` (box and
   vertex runs, tested); sections are built with no camera cells and
   record each cell's first face and ghost-outline vertex and each blended
   quad's cell; `buildNear` builds the near box per placement with the
   camera (faces, blended quads, ghost outlines ranked against mistake
   outlines in and one cell around the box, block entities; no marks),
   rebuilt on a new camera cell, when a section it covers is rebuilt, or
   while a chunk there was incomplete. Sections touching the box draw
   faces and ghost outlines in runs (assumes `renderMesh`'s start offset
   counts vertices: if near ghosts look scrambled, missing or doubled,
   that is the first suspect) and leave out the box's entities and blended
   quads; the sorted blended mesh re-sorts at once on a new box. The trace
   logs "near rebuilds". The camera-cell marking of `7c0a7cb2` is gone.
   Checked 2026-10-26 (VALIDATION-LOG "L-256 fifth perf trace"): no
   section rebuilds inside ghosts, but one near rebuild costs 24-27 ms in
   dense interiors (0.1-0.5 ms in open air), 11-17 a second when flying
   fast, so frame rate inside is unchanged (41-66 fps); standing and
   outside unchanged. That is about 150 us per box cell against about 30
   us per cell in a section build, where enclosed cells are skipped and
   opaque ghost neighbors make the tessellator leave faces out; in the box
   every cell is drawn with all its faces and each face goes through
   `cullAgainstGhosts`. Maintainer candidates, with notes from the code:
   (1) rebuild only the slab that enters: not enough on its own, because
   one cell of camera movement changes the rules for most of the box (the
   pairs within one cell, which face is beyond, the camera cells' back
   faces), only the enclosed skip moves slab by slab; (2) keep each cell's
   camera-independent work between near rebuilds (lookups, shapes, or the
   cell tessellated once in the near form, with only the face drops
   redone per camera cell; cells at the box edge must still look the
   same as today); (3) rank against mistake outlines only when they
   change; (4) a time budget per frame, drawing the old near meshes and
   box until the new ones are complete (no holes; near cells briefly keep
   the old camera's rules when flying fast: a visible trade-off for the
   maintainer). Next: time the parts of a near rebuild in the trace
   (halo and ranking, tessellation, face drops, upload, cells drawn) and
   choose from that.
   Measured 2026-10-26 (VALIDATION-LOG "L-256 sixth perf trace"): of a
   24-26 ms near rebuild inside, 21-24 ms are outlines at Normal width
   (1.7-1.8 ms at Thin), 2-3 ms tessellation, the rest under 0.3 ms.
   Outline vertices went through `Tessellator::vertex` one by one (about
   0.3 us each, 32 per edge for bars). Built next (not yet checked): only
   the first primitive goes through `vertex()`, the rest are copied into
   the vertex streams (`drawSameColor`, the way the sorted blended mesh is
   made); this also speeds up section builds. Checked 2026-10-26
   (VALIDATION-LOG "L-256 seventh perf trace", `6e7b96e`): a near rebuild
   inside is 3.1-4.1 ms (was 24-26), outlines about 1 ms, tessellation
   1.9-2.8 ms; ghost pass inside 3.7-5.0 ms, worst frames 11-15 ms (31-32);
   section builds while loading at most 7 ms (35); outline look unchanged
   in every width and graphics mode. A/B against `main` (`86723d2e`) the
   same day: the z-fighting seen from inside a sandstone wall, a stair and
   farmland (and the next block's faces) is new on this branch; the cross
   plants' flicker is not (L-268). Suspect: `renderMesh` reads the start
   of a run in a quad list as an index (six per quad), so runs began
   early and the near cells' faces were drawn by both the sections and
   the near meshes, at different origins. Built next (not yet checked):
   runs in quad lists start at `quadStart` (first vertex / 4 * 6); the
   count stays vertices. If that is wrong, faces near the camera go
   missing or doubled elsewhere in the sections touching the box.
   Checked the same day (VALIDATION-LOG "L-256 near-box z-fighting"):
   unchanged with `quadStart`; with the probe build (sections touching
   the box not drawn) the camera's block is hidden as on `main` and the
   near meshes' counts are right. So a section cannot be drawn in parts
   with `renderMesh`'s start and count as used here (how the engine reads
   them is unknown); runs do not work and the near cells need another way
   out of the sections, or item 7 (a) is dropped.
   Decided 2026-10-26 (maintainer): drop 7 (a) and keep `main`'s camera
   rebuilds (`7c0a7cb2`) with the outlines copied in bulk, which were most
   of a rebuild's cost; measure inside ghosts again. The near mesh, the
   runs, `NearCells.h` and the probe option were removed; the design notes
   above stay as the record. If walking inside is still heavy, the next
   ideas are copies of the touching sections without the near cells
   (needs their vertex data kept) or 7 (b). Checked 2026-10-26
   (VALIDATION-LOG "L-256 eighth perf trace", `cbf1742`): inside ghosts a
   section build is 1.4 ms on average (was 7-16), at most 4-5.7 ms (was
   17-39); ghost pass 3.8 ms median (6.5-15.6), worst frame 15.3 ms
   (42.8), 86 fps median (42-69); no z-fighting, outlines and waterlogged
   blocks unchanged. Item 7 is done this way. Not done: caching outlines
   per cell (their bounds come from the camera-dependent tessellation),
   dropping the reversed bar quads (the material may cull either winding),
   a wider box with hysteresis (the rules inside the box change with every
   camera cell, so a larger box does not need fewer rebuilds). Tessellation
   (about 2.4 ms) is the next part if more is needed.
   Visual issues seen on that build, not yet compared with `main`:
   z-fighting or unstable order inside a stair block, at a door seen from
   inside a block, on cross-shaped plants (dead bush).
Constraints: no visible change (outline look, mistake colors, blended
order must not flicker again, L-192/L-193); fail open as today. Validate
each step in game against the step 1 numbers.
Step 1 done 2026-10-26 (VALIDATION-LOG "L-256 schematic ghost perf
trace"): 2428 sections, ghost pass 3.7-4.8 ms standing still. By cost:
the check/scan about 1.05-1.25 ms every frame (candidate 5); about 1 ms
outside every measured stage (find it first: per-section bookkeeping over
all sections is the suspect); faces 1.0-1.5 ms (candidate 4); lines
0.3-0.7 ms, the same for Thin and Normal, so it is the number of line
meshes, not candidate 1's vertex count; many mistakes add 0.25 ms standing
and re-sort the blended mesh every frame while walking (candidate 2).
Proposed order: account for the missing 1 ms, give the scan a time
budget, then fewer draws for faces and lines, then the mistake boxes.
Candidate 1 drops to last.
Done 2026-10-26: the scan is capped at 0.5 ms a frame (`bd10d7d4`, about
-0.5 ms) and the missing millisecond was the per-frame walk over every
section, now 0.07 ms (`697c2475`). Standing still the pass is about 3.05
ms (was 4.5). New finding: while walking, rebuilding the sections around
the camera's old and new cells costs 3-5 ms a frame with 25-30 ms peaks,
more than any drawing stage (6. below). No release target selected.
Closed 2026-10-26 (maintainer): done. Kept from the work: the scan's time
cap (`bd10d7d4`), the walk fix (`697c2475`), the narrowed camera rebuilds
(`7c0a7cb2`) and outline vertices copied in bulk (`6e7b96e`); the near
mesh of item 7 (a) was built, measured and dropped (`cbf1742`). Result on
`cbf1742`: standing still about 3 ms ghost pass, inside ghosts 3.8 ms and
86 fps, section builds at most about 5 ms. Candidates 1, 2 and 4 were not
needed; reopen as a new item if a later measurement asks for them.

### L-166 Villager trade helpers: part 1
Done 2026-10-13: `0193c02` implements the Shift + right-click row trigger;
`481dc7d` prevents native refill from dropping existing ingredient overflow.
The normal build passes local/BDS trading, sold-out/material stops, full
inventory, two-input enchanted books, settings-off, controlled response delay,
focus/close interruption and re-entry observations. A delayed partial run
stays stopped through a Nether round trip. Build and pure/native tests pass.
Evidence: VALIDATION-LOG; contract: [TRADE-HELPERS.md](TRADE-HELPERS.md).
No release selected. Network RTT/loss, exact stop-phase isolation, forced
world exit during a pending response, price changes and other servers stay
in pre-release checks. Optional part 2 remains undecided in BACKLOG.

Earlier backlog checkpoint retained below; its pending status is superseded
by this completion record and current VALIDATION coverage.

Kind: Research (strong model) for part 1; Design for optional follow-ups.
Imported from the maintainer's notes (2026-09-25) on 2026-10-12.
Status 2026-10-13: after the initial diagnostic checkpoint, the maintainer
authorized agent Computer Use for runtime checks and completing part 1.
The vanilla request path and predicted client offer uses are observed locally;
the one-at-a-time implementation and its Shift + right-click trigger
(remapped at the row's input component) are locally verified. Normal BDS
`481dc7d` buys one axe with plain right click, then the remaining 11 with
Shift + right click; reopening preserves sold-out state. The input-return
preflight fixes full-inventory ingredient drops: the repeated gesture leaves
both stacks unchanged and no dropped items. Settings-off keeps modified row
clicks inert; a two-input book offer produces eight enchanted books, consumes
both costs exactly and stops when ordinary books run out. Delayed responses
and interruptions remain; L-166 is not complete.
Completion requires matching request acknowledgments and inventory deltas,
with server offer state checked by reopening on BDS. Research and checklist:
[TRADE-HELPERS.md](TRADE-HELPERS.md). Part 2 remains undecided; no release.
Builds on L-129 (every trader level shown); separate from L-154's inventory
gestures.
Goal: fewer clicks for repeated trades in the vanilla trade screen.
Agent's proposal (not decided):
1. Trade repeatedly: a modifier click on a trade in the list (gesture open,
   below) trades it as many times as allowed: until the offer is sold out
   (uses reach max uses), the inputs in the inventory run out, the output
   no longer fits, or a limit. Each round goes through vanilla's own trade
   request (the server-side trade request already carries a craft count,
   `CraftHandlerTrade::mNumCrafts`), Lamium does not build trade
   transactions, and the next round starts only after the inventory and the
   offer's uses changed on authoritative state. Locked trades (L-129) are
   never traded. The output is never dropped: stop when it would not fit.
2. Later, only if wanted: remember a favourite trade per profession and
   select it when the screen opens; show the remaining uses before an offer
   sells out.
Out of scope: trading with several villagers at once, moving villagers,
restock timers, anything that changes prices.
Research entry: `Trade2ScreenController` (`_onContainerSlotPressed`, the
`TradeItemCollections`, `_getBaseItemCountForTrade`), `Trade2ContainerManagerModel`,
`UpdateTradePacket` after each trade (does `uses` update, how fast).
Validation: local world and BDS, with latency; sold-out mid-run, full
inventory, closing the screen mid-run, two-input trades, enchanted books.
Decided (maintainer 2026-10-12, trigger revised 2026-10-13): Shift + right click
on a trade row trades it as often as allowed; left click and plain right click
stay vanilla (Shift + right click on a row does nothing in vanilla); default on.
Open (maintainer): whether part 2 is wanted at all.

### L-164 Connected Textures: one quad per face with generated sprites
Done 2026-10-13: every connected face is one quad with a sprite generated into the terrain atlas, with texture sets for Vibrant Visuals (`028b1ed`, `6274b53`); split cells and the pane stretch removed; checked in game.
Kind: Research (strong model), then a rebuild of L-96's method layer.
Raised by the maintainer 2026-10-12: the per-block fixes keep running into
the same dark shading (L-133 panes, L-156 bookshelves under Vibrant
Visuals); should Lamium take a general connected-texture design instead
(the maintainer referred to Continuity for Java)?
Finding: every dark result so far appears only where Lamium splits one face
into several quads (split cells for bookshelves and sandstone, split panes).
Unsplit faces never showed it. Four pane rounds could not find which value
the renderer interpolates across the extra vertices, and the cause sits in
render code whose inputs the SDK does not show.
What a general design changes: the face keeps vanilla's single quad and
only its texture changes, to a sprite chosen from the neighbor mask. That
removes the split as a class of problem instead of chasing each case, and
makes blocks a matter of rule data. What Lamium does not take over: reading
other connected-texture formats from resource packs (Bedrock packs do not
carry them; a large scope with no current demand).
Proposal (agent, 2026-10-12; adopt after the spike):
1. Sprites generated from the loaded texture, not shipped: at atlas build,
   each rule texture gets one variant per needed mask, baked with today's
   look (glass: the border strips refilled from the middle texels; 15 extra
   variants for the 4-side mask; bookshelves left/right: 3; sandstone top:
   1). Resource-pack textures are used automatically and no game texture is
   redistributed. The terrain atlas already supports images made at build
   time (`TextureAtlas::mRuntimeImageGenerators`,
   `RuntimeImageGeneratorInfo` with a generator callback); atlas names
   resolve through `BlockGraphics::getTextureUVCoordinateSet`.
2. Tessellation: the face hooks pass the variant's UV set to the original
   face call; no extra quads. Panes use the same variants for their glass
   faces, which also replaces the one-texel stretch.
3. Pure rule table (`tests/`): block -> texture slots, connection predicate,
   method (4-side mask, horizontal, vertical, top only; the 47-tile mask
   with inner corners only if a block needs it), border widths and fill
   sources. Adding a block is a table row.
4. Variants exist whenever the game version is verified, so the switch
   still only rebuilds chunks. Large textures (above 64 px) skip
   generation and draw vanilla, to bound atlas memory.
Spike (stop and report after two failed rounds): register one generated
sprite for plain glass, draw a glass face with it, and check normal and
Vibrant Visuals, a resource pack, a resource reload, and the game's mips.
Vibrant Visuals also needs the texture set (normal/MER maps) of a variant;
find whether `PBRTextureDataManager` / `TextureAtlasItemTextureSetTranslation`
gives generated sprites one, or the variant must copy the base texture's.
Spike status (2026-10-12, VALIDATION-LOG): a generated sprite works for
plain glass, also under Vibrant Visuals (round 3, `67e9f26`). How: add a
`texture_data` entry in `TextureAtlas::_createAtlas` for the terrain atlas,
answer `ResourcePackManager::getPackStackIndexOfResource` for the sprite's
image extensions with the base texture's pack, and serve a PNG from
`load(ResourceLocationPair, ..., extensions)`; the texture group cache is
not used by the atlas. Next: a bookshelf side variant under Vibrant
Visuals (PBR texture sets, and the L-156 question).
Round 4 (`d126608`, 2026-10-12): bookshelf sides joined on both sides drawn
with a sprite show no dark bands under Vibrant Visuals, while a one-sided
(split) join next to them does: the split is the trigger of L-156. The
sprites carry no texture set (the bases have one in pack 13) and no
surface difference was seen. Spike answered: adopt the generated-sprite
method. A real build should give each sprite a texture set derived from
the base's (color pointing at the sprite, MER/normal maps baked the same
way) so blocks with stronger PBR maps match.
If generated sprites cannot be added safely: keep split cells only where
the dark shading has not been seen, and use the stretch under Vibrant
Visuals (choice for the maintainer then).
After it holds: move glass, panes, bookshelves and sandstone over, delete
the split-cell code, and recheck L-133 and L-156 against it before closing
them.

### L-133 Connected Textures: split glass panes without dark halves
Done 2026-10-13 by L-164: panes keep vanilla's quads and show glass sprites; no dark half, no stretch (checked in game).
Kind: Research. Opened 2026-10-11 from L-96. Linked: L-156 (split
bookshelves dark under Vibrant Visuals with the same look, 2026-10-12) and
L-164 (one quad per face with generated sprites, which avoids the split).
Status: parked 2026-10-11 after four trial rounds; panes ship with the stretch.
Panes draw joined glass by moving the texture inward (a 1-texel stretch,
`4ef3d51`); glass blocks, bookshelves and sandstone draw split at the
texture's scale. Split panes (`17cedc6`..`20b9059`) looked right up close but
translucent panes blended dark on one half of each block depending on the
view: facing south at an east-west pane the west half, facing east at a
north-south pane the south half; facing north or west nothing. Night Vision
hid it. Ruled out with dumps and trial builds: the split values (positions,
UVs, colors, light, normals, facing), redrawing the pane versus appending
copies to the tessellator, and giving every cell its face's quad info
(center included). The likely cause is the translucent face sorting and
culling (`RenderChunkSorter::sortAndCullFaces`, `FaceInfo` with a `reverse`
bit, `RenderChunkGeometry` face sorting metadata); its inputs are not
visible in the SDK headers. Stained glass blocks, also split and blended,
show nothing. A stained pane cross shows seams to its arms even with the
feature off: vanilla, not Lamium's.
Trials 2026-10-11 (`99a3089`..`84f4335`, split panes restored, then reverted):
the dark half is the arm whose glass is cut at a joined edge (west arm of
east-west panes, south arm of north-south ones). Reversing the quads inside
the appended copies, keeping the largest cell in the first draw's place, and
showing every cell its own texels all stayed dark; appended copies folded to
a point with unsplit glass did not. Every quad has `facing` 6 and `twoFace`
0. Up close the dark band is a smooth gradient, darkest at the seam about
two texels from the joined edge, as if a value interpolated from the seam
vertices. So splitting a pane's glass face into quads is the trigger, not
the copies, their order, sort data or texels; the vertex data Lamium writes
matches vanilla. Hypothesis, unchecked: the game derives per-vertex lighting
from vertex positions after tessellation (the `uv1` values are near 0 in
daylight), and the new mid-face vertices get dark values. Parked: the gain
over the one-texel stretch is small.

### L-148 Hide the Nether portal overlay
Done 2026-10-12: Hide effects child "Nether portal screen" (default on), the portal effect time set back to 0 after each tick (`cd47a29`); checked in game.
Kind: Research, then Design/Ready once the drawing route is established.
Status: imported from the maintainer's notes on 2026-10-12; not implemented.
Goal: optionally hide the purple screen overlay without changing portal
contact, the time in the portal or dimension transfer.
- Proposed home: a child of Hide effects, independent of L-149. Its proposed
  default is off; settle this explicitly against the existing rule that
  Hide effects children default selected before implementation.
- Identify the purple overlay's draw route; existing nausea hiding does not
  establish portal support. L-79's unresolved frame routes are a caution.
- Leave distortion control to vanilla accessibility settings unless a
  separate need is established. Do not change portal or server state.
- Checks when built: hiding/restoration with child and master, other effects,
  portal entry/exit and dimension transitions, supported graphics modes and
  resource packs. Unsupported routes retain vanilla drawing.
- Built 2026-10-12: Hide effects child "Nether portal screen" (default on,
  key unbound); after `LocalPlayer::normalTick` the portal effect time and its
  previous value go back to 0 (gate "Hide effects (portal screen)"). In-game
  check of the normal build pending.
- Leads (SDK, 2026-10-12, unverified): `LocalPlayer::mPortalEffectTime` /
  `mOPortalEffectTime` (likely the overlay's strength), `mEnablePortalEffect`,
  and `LocalPlayer::handleInsidePortal`. Trace build `--portal_trace=y`
  (`src/features/visuals/PortalTrace.cpp`) logs them per tick with the top
  screen name; awaiting a run through a portal.
- Trace run 2026-10-12 (survival, local): touching the portal,
  `handleInsidePortal` sets `mEnablePortalEffect`; from the next
  `normalTick` the effect time rises 0.0125 per tick (0 to 1 in about 4 s,
  transfer at 1.0), and in that same tick the open inventory closed
  (inventory_screen -> hud_screen). Next trace build: the effect time is set
  back to 0 after every tick, to see whether the overlay goes, the inventory
  stays open and the transfer still happens.
- Result 2026-10-12 (maintainer, survival): with the effect time zeroed after
  every tick the purple overlay is gone and the transfer works as before; the
  inventory is still closed on entry and cannot be opened inside. So hiding
  the overlay is feasible this way (a client-only value; writing it needs a
  version gate). The closing is separate (L-149).

### L-149 Allow inventory access in portals
Parked 2026-10-12 (maintainer) after research: keeping an inventory opened before entering works (answer `Actor::isInsidePortal` false for the local player; local and BDS), but opening one inside cannot: the client sends the same `InteractPacket` in and out of a portal and the server does not open it inside. The maintainer did not want the keep-open-only version. Idea raised the same day: a Lamium-drawn inventory instead of the vanilla screen (not started; see the discussion in L-132/L-149 notes).
Kind: Research, with Design if screen behavior choices remain.
Status: imported from the maintainer's notes on 2026-10-12; not implemented.
Goal: open and use the ordinary inventory while inside a Nether portal.
- First reproduce the current restriction and distinguish input suppression,
  refusal to open, forced closing and server constraints. Screen tracking
  and inventory controllers are leads, not proof of feasibility.
- Proposed: an independent Inventory setting, default off. Never fake or
  alter portal contact, transfer state or packets to bypass server authority.
  Accept only a local client UI/input change that preserves game semantics.
- On dimension transfer safely close/discard the old screen and outstanding
  operations. Do not leave ghost items or stale controller state.
- Checks when built: local worlds and BDS, opening and keeping the screen,
  authoritative item movement/swaps/crafting, before/during transfer,
  return through the portal, disable and restoration after leaving it.
  Defer if feasibility requires deep invasive hooks or state divergence.
- 2026-10-12: the L-148 portal trace also logs the top screen around
  `handleInsidePortal`, to see where and when the inventory is closed.
- The inventory closed after `handleInsidePortal` returned (still open there)
  and before the next `normalTick`; zeroing the effect time did not stop it.
  Next trace: `mEnablePortalEffect` cleared right after `handleInsidePortal`.
- Result (maintainer, 2026-10-12): still closed on entry and refused inside;
  transfer time unchanged. At the close the log showed the flag off and
  "inside" false, so neither drives it. Next trace: `Player::
  canOpenContainerScreen` logged when it refuses and answered yes for the
  local player (a likely check for both opening and closing).
- Result: it was never called in a portal; still closed and refused. Next
  trace: `SceneStack::_schedulePopScreen` with the caller stack (offsets in
  the game module, beside the offsets of candidate functions), the
  inventory key handler and `LocalPlayer::openInventory`.
- Result (2026-10-12): in a portal the inventory key handler runs but never
  reaches `LocalPlayer::openInventory` (outside it does), so the refusal is
  inside `ClientInputCallbacks::handleInventoryButtonPress`. The close comes
  from a UI tick path (`_schedulePopScreen` via game offsets +22415a0,
  +4df8519, +4e17616), not from player code; a chat close took another path.
  Next trace names those offsets against 143 candidate functions (every
  screen controller `$tick`, `ClientInputCallbacks`, `LocalPlayer`).
- Naming failed twice: Lamium's game-function pointers and LeviLamina's
  symbol lookup both return stubs, not game addresses. Next trace follows
  the stubs and also tries a fix (trace builds only): when the inventory
  key changes nothing near a portal, call `LocalPlayer::openInventory`; and
  near a portal refuse `_schedulePopScreen` on the inventory screen unless
  Esc or E is down.
- Result (maintainer, 2026-10-12): with the close refused the inventory
  stays open in the portal and Esc/E still close it, but items cannot be
  moved; E opened it for an instant and it closed again (the opening E press
  still counted as the player closing); on transfer it closed; no items lost
  or duplicated. Naming still failed (only stubs resolved). Next trace: the
  container model's `isValid` (base and crafting) logged and answered valid
  near a portal, the likely check behind both the close and the refused
  moves; E ignored as a close for 500 ms after Lamium opens the screen.
- Result: the screen stays open (E and entry) and Esc/E close it, but items
  and the 2x2 crafting still do not move; the model's `isValid` never said
  no; 466 closes were refused, i.e. the screen kept asking to close every
  frame. Auto Elytra swapped items in the portal, so the server accepts
  moves there: the block is client UI. Next trace: the screens' own
  `_isStillValid` (container and crafting controllers), answered valid near
  a portal.
- Result: answering `CraftingScreenController::_isStillValid` valid keeps the
  inventory open in the portal and items and 2x2 crafting work when it was
  opened before entering; opened inside with Lamium's direct
  `openInventory` it stays but items do not move (the server likely never
  opened it). Read from the game file via the open screen's vtable (slot
  39, rva 4e18b20): it says no when the player's `InsideBlockComponent`
  block is one of two block types (rva 1a1fd40; likely Nether and End
  portal, likely `Actor::isInsidePortal`). Next trace: `Actor::isInsidePortal`
  answered false for the local player on the client tick thread, with the
  other workarounds off, to see whether the vanilla E key then opens a
  usable inventory that stays open.
- Result (maintainer, 2026-10-12): with only `isInsidePortal` answered
  false, an inventory opened before entering stays open in the portal and
  items and 2x2 crafting work; Esc/E close it; transfer still works and
  closes it; nothing lost or duplicated. The E key inside still opens
  nothing. Hypothesis: the server opens the inventory (the client's
  `openInventory` came from another tick path, not the key handler) and
  refuses while its copy of the player is in the portal. Next trace logs
  what the client sends after the key, inside and outside a portal.

### L-96 Connected textures, starting with glass
Done 2026-10-11: glass, glass panes, bookshelves and sandstone behind the
Experimental "Connected Textures" switch, checked in game (VALIDATION-LOG).
Not checked: Vibrant Visuals, resource packs, servers, the unverified-version
fallback (VALIDATION.md).
Kind: Research **(strong model)** first, then Design for the settings.
Taken up 2026-10-05 after a public request.
Status: open. Steps 1-4 done for glass blocks (2026-10-11, `8c3d21a`,
checked in game): the "Connected Textures" switch (つながるテクスチャ,
visuals.connectedTextures, default off, Experimental, toggle key) trims clear,
same-color stained and tinted glass, rebuilds loaded chunks on toggle and
stays vanilla on an unverified game executable. Step 5 done for glass panes
(2026-10-11, `3e76e7a`, checked in game): panes share one glass texture
between the center post and the arms, so they are adjusted per part on the
mesh vanilla built (`tessellateDoubleThinFenceInWorld`; the glass rectangle
from `BlockGraphics::getTexture` slot 0, the thin edge is slot 5): a glass
face's side border moves inward where its block edge touches the same pane;
its top (bottom) border and the thin top (bottom) face go where the pane above
(below) has the same part (center post, or the arm in that direction, from
`ThinFenceBlock::addAABBs`; an L's boxes leave out the drawn center). Trimming
the shared texture, and folding by collision boxes, failed in game first
(trial rounds 1-9, `52fb6eb`..`473b4f2`).
Bookshelves and sandstone, and the split method (2026-10-11, maintainer:
connected bookshelves lose the divider between side-by-side shelves, stacked
sandstone loses the band between blocks). Cropping stretched the texture,
which showed while a block was placed and looked wrong on sandstone, so every
block rule now draws split (`ec5aa69`..`a339a6d`): the face is drawn once per
cell and each copy reshaped, the dropped border becoming a strip of other
texels at the texture's own scale, with the face's corner color and light
interpolated per cell. Rules: glass drops 2 texels on joined sides filled
from the middle (texels 7-8) because a 1-texel strip next to the frame let
far, mipmapped glass show the frame again; bookshelves drop the 1-texel side
column, filled from the column next to it (books stay aligned up close);
sandstone and red sandstone drop the 4-texel top band under another one,
filled from the middle rock (rows 8-11). Panes were moved to the split method too
(`17cedc6`): a joined pane is drawn once more per extra cell its most-split
face needs and each copy reshaped, unused copies folded. More blocks only on
request. Moved ahead of the Schematic follow-ups and the
restriction redesign (maintainer 2026-10-11: higher demand).
What it is for: blocks next to a block of the same kind draw as one surface,
without the border line between them. Start with glass, then stained glass of
the same color, then glass panes; panes joined to blocks, different colors
and border options come later.
Approach to test (a feasibility hint is recorded in PROVENANCE.md): change how
chunk faces are tessellated, look at the neighbor through the block source and
crop the texture coordinates at the touching edge; changing the setting marks
chunks dirty so they rebuild. The schematic ghosts already drive
`BlockTessellator::tessellateInWorld`, `Tessellator` and render materials, so
the feasibility is high; the difference is that this changes vanilla's chunk
meshes.
Constraints:
- This changes vanilla chunk tessellation, which is more version-sensitive
  than the schematic ghosts' private tessellator. Capability-gated and fail
  open: on an unverified game version do nothing.
- Bound the neighbor lookups; a toggle rebuilds chunks.
- Glass is the research target. Resource-pack-defined tile sets are not
  promised. Widen beyond glass only after surveying which vanilla blocks have
  an inner border line.
Layering (proposed 2026-10-07, from the maintainer's notes): the first
user-visible feature stays glass, but the code is split so it can grow
without a glass-only hack that mixes rendering and connection rules.
1. Render backend: from `BlockTessellator` / `Tessellator`, get the block,
   position, face, original texture and block source. Everything
   version-sensitive stays here.
2. Connection core, pure and tested (`tests/`): does this neighbor connect
   (first: the same block; stained glass only with the same color), and the
   face-neighbor mask in the face's plane (up/down/left/right, and the four
   corners for later methods).
3. Method: only "edge trim" at first, cropping the border UVs on connected
   sides of the loaded (vanilla or resource-pack) texture. No extra
   textures and no properties parser.
4. Later methods (a 47-tile set chosen from the 8-neighbor mask, horizontal,
   vertical) only when a block that needs another sprite is wanted
   (bookshelf, sandstone). No compatibility with other connected-texture
   formats is claimed until it is built.
5. Pane geometry and hidden inner faces are a separate backend from the
   texture choice; do not force them into the cube-face path.
Settings name: "Connected Textures", with glass as its first target. No
rule table or resource-pack format is published at first.
Order:
1. Research spike: on one face of plain glass, get block, position, face,
   texture and block source reliably from the tessellator.
2. Connection core and edge trim as pure logic with tests.
3. Glass blocks: clear glass, then stained glass (same color only by
   default; different colors connecting is not the default).
4. Lifecycle: block updates, chunk and subchunk borders, chunk load, setting
   on/off, resource-pack reload, world and dimension change (the setting
   marks render chunks dirty).
5. Glass panes through their own tessellation backend.
6. Optional 47-tile spike after glass is stable: the pure 8-neighbor to
   47-pattern mapping and a replacement-texture path, to decide whether
   Lamium should grow a general connected-texture engine.
Spike result (2026-10-11, `xmake f --ctm_trace=y`,
`src/features/visuals/ConnectedTexturesTrace.cpp`, `b3d93c7`): hooking
`BlockTessellator::tessellateBlockInWorld` (world `BlockPos`, `mRegion`, the
block) and the six face calls `tessellateFaceDown/FaceUp/North/South/West/East`
(the face's `TextureUVCoordinateSet`, `p` = the world position) and passing a
copy of the UV set with one texel cut on each side whose in-plane neighbor is
the same block type removes the inner lines of glass and same-color stained
glass; the outer frame stays and different colors keep their line. The
in-plane side guesses held on all six faces (seen from outside: north left
+X, south left -X, east left +Z, west left -Z, top of side faces +Y; up face
left -X top -Z, down face left -X top +Z). About 50x50 glass showed no
slowdown. Not yet: a setting and chunk rebuild on toggle, panes, Vibrant
Visuals, resource packs with other border widths, servers.
Research output: the function(s) that can be intercepted on this game
version, whether the crop works for glass and panes, the cost on a large
view distance, and how it behaves with Vibrant Visuals / Deferred rendering.


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

### L-158 Prevent missing and inconsistent settings/action labels
Done 2026-10-12: shared label resolution for search, toasts and the paused notice, catalog-wide label tests (`be0b1f1`); checked in game. Not done: shared references for repeated feature names in toggle labels (needs a wording decision; open a new item if wanted).
Kind: Refactor (strong model), with Design for any user-visible wording change.
Status: first pass built 2026-10-12, in-game check pending. Audit: action
labels resolve through `actionTranslationKey` (SettingsRows.h); settings search
built its own `key.Lamium.<id>` keys instead, so a child row's shown label
(e.g. "Increase speed") did not match; toggle toasts and the paused-features
notice built feature keys by hand. Done: search uses the shared resolver (child
label plus the Hotkeys name as an alias); `featureNameKey` and `toggleToastKey`
serve toasts and the notice; SettingsRowsTests checks from the catalogs, in all
three languages, every section, feature name/description, action label (both
contexts), toggle toast name, paused feature name and every choice value. No
wording changed. Left: the shared-reference idea for repeated feature names in
toggle labels (not started; needs a wording decision first).
Keep separate feature and action labels: a feature name and commands such as
increase speed, rotate or open a view have different purposes. Do not merge
all labels or mechanically construct every action name from the feature name.
The goal is to prevent missing required translations and accidental wording
drift between settings, search results and Hotkeys, while retaining deliberate
context differences (for example a short child label versus a standalone name).
- Audit the feature/option/action metadata and every place that resolves their
  names. Use a shared name-resolution policy for displayed and searchable
  labels, with explicit context where needed. Search may still include IDs,
  descriptions and other aliases; those are not replacements for display names.
- Check required keys from the registered UI/action catalogs in English,
  Japanese and Simplified Chinese, rather than checking only entries already
  present in the translation tables. Cover active actions, primary actions and
  action labels attached to option rows; explicitly exclude retired actions.
- Add regression coverage that fails when a new active action lacks its label
  and exercises the real label resolution in settings/search/Hotkeys. Preserve
  intended context-specific names rather than requiring all strings to match.
- Consider reuse or shared references for repeated feature names in simple
  toggle labels, without assuming one grammatical template works in all three
  languages. Choose the implementation after the audit; avoid a broad rewrite.
- Preserve saved action IDs, settings format and binding behavior. Fix L-155
  independently; this later work must not delay the immediate missing-key fix.
Acceptance: a missing required label fails automated checks, supported UI
paths do not show raw keys, and shared naming changes reach search/display
consistently. Validate any changed wording in all three languages in game.

### L-137 Game update readiness (with 0.2.0)
Done 2026-10-11: inventory and playbook (GAME-UPDATES.md), nine gates, the unverified test build, release check and in-game notices of paused features; passed the 0.2.0 regression checks and the outside reviews (2026-10-11), the fail-open build included. Each later game version runs the playbook.
Kind: Refactor, then Research for each new game version. Chosen by the
maintainer 2026-10-11 as part of the 0.2.0 clean-up; scope corrected the
same day.
Lamium depends on the game through hooks (205 in 45 files, 2026-10-11),
game structures it reads or writes by member, and game behavior. 88 of 308
source files include game headers; GhostRenderer.cpp alone includes 70.
What actually breaks on an update (checked 2026-10-11):
- Member layouts come from the SDK headers, which are regenerated for each
  game version, and only one place hard-codes a layout
  (`StackClassifier.cpp`, the shulker box item vtable). Rebuilding against
  the new SDK follows layout changes; removed or retyped members fail to
  compile. An old Lamium on a new game is unlikely: `tooth.json` pins
  LeviLamina `26.51.*`, and LeviLamina itself is tied to a game version.
- What a rebuild cannot catch: changed meaning (vertex data whose fields
  mean something else, packets handled differently, render behavior such as
  the Vibrant Visuals fog), SDK header mistakes, and a different game
  executable under the same loader.
`verifiedGameExecutable()` gates four render paths today.
Status (2026-10-11): the required part is built; its in-game check (the
fail-open build) is in the 0.2.0 regression checks. Inventory and
playbook: [GAME-UPDATES.md](GAME-UPDATES.md), checked by
`scripts/Check-GameInventory.ps1` in CI. Gates: `versionSensitiveAllowed()`
(`app/Versions.h`, `44f48a8`) on nine capabilities; test build
`--unverified_game=y`.
Discoverability (2026-10-11): the SDK lines in `xmake.lua` and
`packages/levilamina-client-sdk.lua`, the `verifiedGame` constant and
AGENTS.md rule 8 point to the playbook; `New-ReleaseArchive.ps1` warns when
the SDK's game version differs from `verifiedGame` and refuses a tag build.
Players see the pause (maintainer chose the agent's proposal 2026-10-11,
`1da4316`): toggle toasts and settings feature rows add "paused" or
"partly paused", the settings header shows "Some features paused" in the
warning color with the reason in its tooltip, and the first HUD frame after
joining a world shows an 8-second toast naming the paused features in use.
Nothing shows on the verified game. Passed on the unverified build of
`ddac26c` (2026-10-11; the join toast wraps, only the note is yellow).
Required for 0.2.0:
1. Inventory, kept next to the code (script-checked where possible): per
   feature, its hooks and the game meanings it relies on (vertex fields,
   packet order, render state), marking the ones a rebuild cannot catch.
2. Playbook for a new Minecraft or LeviLamina version: SDK bump, build,
   inventory-driven in-game checks, widening the verified version
   (DISTRIBUTION.md or a new doc).
3. Gate the risky operations, not whole features: a path that writes game
   memory or relies on an unchecked meaning checks the verified version at
   that path and fails open there, so safe parts of the same feature keep
   working.
4. A way to test the gates without an unsupported game: a build option
   (like `--feature_skip`) that makes the version check fail, so the
   fail-open branches can be run and checked in game.
Later, per game version: run the playbook and extend the gates as the
inventory shows.
L-134 and L-136 help by separating game glue from pure logic, so an update
touches the glue and the tests still vouch for the logic.

### L-138 Shared face drawing for cell overlays (0.2.0, L-111 A)
Done 2026-10-11: CellSurface/CellMesh for Shapes, the breaking region and the save area (`261c0e4`..`8a4c71a`); passed the 0.2.0 regression checks and the outside reviews (2026-10-11).
Kind: Refactor **(strong model)**. Step 2 of the 0.2.0 order (L-111).
Status: built 2026-10-11, in-game check pending (0.2.0 regression checks).
`overlay/CellSurface.h` (pure, tested) builds the faces on the cells' own
planes and the outline; `overlay/CellMesh` uploads and draws them with the
face material, the outline coloring (vertex or shader color) and the pull.
One feature per commit: Shapes (`d4a83d1`), the breaking region
(`dd1f58c`), the schematic save area (`8a4c71a`; corners now on the block's
planes with a nearer pull than its outline, `depth::markPull`).
`overlay/Depth.h` already holds the depth rules every face overlay follows
(L-110). Add one builder for the faces and outline of a set of cells that
Shapes, the breaking restriction and the schematic area frame call.
Shares: face and outline geometry for cells, the depth/inset rules, color
and alpha handling. Does not share: the schematic ghost blocks (real block
shapes, liquids, translucency, neighbor-dependent meshes stay in the
schematic renderer), per-feature colors and when each overlay is shown.
Keeps: each overlay's current look (no z-fighting, no flicker, no inset),
per-feature on/off and fail-open. One feature moves per commit, with an
in-game check of overlapping Shapes, restriction and schematic frames.

### L-136 Split GhostRenderer.cpp by responsibility (with 0.2.0)
Done 2026-10-11: split (`1a2c231`, `3ec2498`); passed the 0.2.0 regression checks and the outside reviews (2026-10-11).
Kind: Refactor **(strong model)**. Chosen by the maintainer 2026-10-11 after
an outside size review (SettingsScreen.cpp, GhostRenderer.cpp, WorldMap.cpp,
InfoHud.cpp, Translations.h); planned with L-134 and L-111.
Status: built 2026-10-11, in-game check pending (0.2.0 regression checks).
`1a2c231`: GhostRenderer.cpp (the pass, 668 lines), GhostMesh, GhostVerify,
AreaSave, GhostMarks, GhostBlocks, GhostActors, ResolvedPlacement; the pass
owns the sections and resolved placements and hands them to the parts.
`3ec2498`: classification (`classifyReading`, Verify.h), face culling
(`dropFace`), back faces and blend order (`facesAway`, `farToNear`,
GhostFaces.h) in tested headers. WorldMap.cpp and InfoHud.cpp were not
split (the map got its layer model in L-139 instead).
`src/features/schematic/GhostRenderer.cpp` is 2,792 lines (701 on
2026-10-05, 1,693 on 2026-10-08) and changed in 107 commits since
2026-10-01. Despite its name it holds more than drawing: the ghost mesh
build (neighbors, culling against ghosts, back faces, duplicate quads,
liquids, layers and quad order), the verification scan and progress
(cell classification, entity checks, mismatches), the area save (stepped
save, vanilla load self-check), block entity loading, the in-world drawing
(selection, placement frames, waiting columns, entities, name tags) and
the performance report. Split along those lines into files that keep
behavior unchanged, with the pure parts (classification, culling rules,
quad ordering) in headers with tests. Each part owns its state, and resource
creation and release (meshes, scans, saves) keep one clear owner and the
current timing across world exit and dimension change. Every step is
checked in game against the L-93 rendering checks (doors, beds, panes, liquids, honey/slime, block
entities, entity models, large placements' frame rate). WorldMap.cpp
(~1,240 lines) and InfoHud.cpp (~1,400) are worth a look in the same pass;
Translations.h is data and stays as it is.

### L-134 Split SettingsScreen.cpp (with 0.2.0)
Done 2026-10-11: split (`097d32a`, `b612e94`, `22b081a`); passed the 0.2.0 regression checks and the outside reviews (2026-10-11).
Kind: Refactor. Raised by the maintainer 2026-10-11; planned together with
L-111 integration and other 0.2.0 clean-up, not before.
Status: built 2026-10-11, in-game check pending (0.2.0 regression checks).
Part 1 (`097d32a`): ShapesView and WaypointsView; part 2: SchematicsView
(`b612e94`), SettingsTableView with Hotkeys and key capture (`22b081a`).
SettingsScreen.cpp keeps the scene, input queues, prompts, the schematic
menu and the world map hand-over (1,371 lines). Each view owns its state;
what the core offers them is ScreenParts.h (footer message, the number
being typed, navigation, closing, where the player stands); navigation item
ids are in SettingsNavigation.h; shared list widgets in ListViewWidgets.
The cut was made by the agent without a separate plan with the
maintainer (the goal set on 2026-10-11 asked for 0.2.0's required items).
`src/ui/SettingsScreen.cpp` is 4585 lines (2026-10-11), the largest file by
far, and changed in 131 commits since 2026-09-01. It mixes the settings rows
and navigation, key capture and the Hotkeys view, text and number editing,
the Shapes editor (drafts, swatches, docked rendering), the waypoint list,
the map cache controls and the version tip. L-73 (BACKLOG-DONE, item D)
dropped this split on 2026-09-30 at 1,878 lines: the Shapes view and the
input listeners shared 20+ screen-wide variables, so a file split would only
have moved text behind a header of shared variables ("split it when the
screen grows again, after grouping its state first"). Group the state
first, then split by those parts into files that keep behavior unchanged;
pure layout and state logic moves into headers with tests where it can
(AGENTS.md rule 1). Plan the cut with the
maintainer before moving code: the split should match how L-111 regroups
features. It runs in two parts (L-111 order): the Shapes view and waypoint
list state before L-139 map layers, the rest (Hotkeys, the schematic view,
the plain settings table) after. Each part owns its state and declares what
others may read; moving the globals into one big context struct passed
everywhere is not a split (outside review 2026-10-11). In game after each
part: search, number entry, key binding and capture, shape editing,
waypoints, HUD layout, Esc/Enter/Tab while editing (the L-73 regression
area).

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


---

## Archived release and validation checkpoints

0.3.1 released 2026-10-27 (`v0.3.1`, asset SHA-256 `D2BA64271645CA4910123C4C8655441ADFCEFABF8AE5DF3D412C257F3E1C00F7`, 28 entries, `manifest.json` 0.3.1).
Its user-facing changes are the release notes; the checks still owed are L-274's refused-pick line (a
server that refuses the give or the move) and L-276's save-failure path (read-only `waypoints.json`).
No server checks were made for this release.

### 0.3.0 pre-release checks — retained 2026-10-26

Moved from BACKLOG.md after the 0.3.0 release; the remaining gaps are
rows in BACKLOG's "Gaps carried from released builds" table.

- **Maps (L-205/L-206/L-210/L-211/L-215/L-225/L-226):** local scan/image
  fixes and the 96-chunk scan-grid checks are recorded in VALIDATION.
  Remaining: compare frame times with recording off/on at 32/64/96 chunks,
  minimap off/on, nearby edits and movement during a wide pass, and broader
  servers. Only client-received terrain can be recorded; unreceived terrain
  is not a promise to fill the entire configured radius.
- **L-228 cave/Nether map:** final checklist passed on `7e0e2f9`;
  band/body follow-up `c67054d` and height-follow fix `07f0dc1` passed on
  2026-10-20. Remaining: measured frame times, broader servers and a live
  check of pending images/per-band coverage if the footprint difference
  recurs. Completed implementation is in BACKLOG-DONE.md.
- **L-216 LMR2 regions:** file/log checks on `6ff0610` confirmed all 1,563
  old LamiumTest regions migrated without recorded-column loss, including
  unrescanned regions, with about 83% less disk use. Maintainer local
  appearance, opening/zoom, cave/Nether and rejoin smoke found no abnormality
  attributed to L-216. Still open in game: older-build fill-only merges,
  refusal without overwrite, damaged-file recovery, 10 s failed-save retry,
  broader servers and measured frame times. Review follow-ups (corrected
  save batching, separate legacy warnings, cave provisional tint/revision-2)
  need a focused live check; the reported smoke was for `6ff0610`.
- **FreeCamera (L-207/L-213/L-214):** open-ground culler cost, switching
  stability and underground-hold behavior have local results. Frame times
  per hold choice, servers and broader graphics/resource/lifecycle cases
  remain open. The sky-only loaded-area boundary failure is open bug L-227.
- **Connected Textures corner edges (L-208):** local options/help checks
  passed; resource packs, live resource reload and servers remain open.
- **Target/HUD/settings (L-217 to L-224):** recorded local checks cover the
  target ray/ghost-edge fixes, coordinate wording/descriptions, HUD-editor
  chat/list fixes and calculator entry. Use VALIDATION's remaining language,
  size, input and server gaps for the changed-feature smoke.

### Notion reconciliation — 2026-10-20

Moved from BACKLOG's execution order on 2026-10-26.

Notion reconciliation, 2026-10-20: four current schematic notes from the
[idea inbox](https://app.notion.com/p/3f7c12ab5b2d8121b2a0db32608f6b7a)
are tracked as L-229/L-230/L-231 (the two controls-UX notes share L-231).
The item/edit-mode proposal still waits for feedback; unchosen ideas such
as fall-rescue elytra, effect-only particles, rain sound, distortion
presets, reserved Fake Offhand slots and advanced subtitles stay in Notion.
This reconciliation selects no new implementation or release.

### BACKLOG pre-release snapshot — 2026-10-20

Retained from BACKLOG.md during reconciliation, with headings nested under
this snapshot. The dated pending/next-release wording below is historical,
not an unmet gate for a published version. Current release policy and
remaining checks are in BACKLOG.md; current runtime coverage is in
VALIDATION.md. No new game result is established by this move.

#### Pre-release checks

Remaining coverage to consider for the next changed-feature smoke test. These
include trace-only and local-only results; VALIDATION.md owns the per-feature
gaps. The Release policy above does not require a full regression or closing
every server gap before a release.

##### Since 0.2.3 (unreleased)

L-228 cave/Nether map: implementation and final in-game checklist passed on
`7e0e2f9`; band/body follow-up `c67054d` and height-follow fix `07f0dc1`
passed on 2026-10-20. The feature is on `main` and moved to
BACKLOG-DONE.md. Remaining: measured frame times, broader server coverage,
and a live check of pending images/per-band coverage if the footprint
difference recurs.

L-216 LMR2 region files: live filesystem/log checks on `6ff0610` confirmed
all 1,563 old LamiumTest regions migrated with no recorded-column loss,
including regions not rescanned, and about 83% less disk use. Remaining:
the older-build fill-only merge, refusal without overwrite, damaged-file
recovery and the 10 s failed-save retry in game, broader server coverage
and measured frame times. The maintainer's local appearance, opening/zoom,
Nether/cave display and rejoin-retention smoke check found no abnormality
attributed to L-216. Evidence and scope: VALIDATION-LOG.md; implementation:
MAP.md. Review follow-up: the corrected save batching, separate legacy
warnings and cave provisional tint/revision-2 paths still need a focused live
check; the reported smoke result above is for `6ff0610`.

##### Shipped in 0.2.3 (smoke-tested; remaining gaps in VALIDATION.md)

L-169 held stack and drop gestures (`9da5d9d`, chest checklist passed): try
the inventory screen, a furnace or another screen with slot roles, and a
server; the drop key rebound away from Q. L-146 layer switches (passed
locally): the two-row top bar on a narrow window.

L-198 through L-202: the maintainer completed the local normal-build checklist
1-14 on `d5978e2` (2026-10-15), except server checks, and found two issues:
L-197 help presentation and L-203 chat-return placement timing. Both follow-up
checks passed locally on `f8c714b`. The replacing L-197 in-place footer
expansion passed its focused check on `c4b8f01`; other sizes, languages and
editing states while expanded remain unchecked. Existing broader feature gaps remain in VALIDATION.
Server/server-pack checks remain open. A separately requested forced-unverified
test build must verify sensitive paths stay vanilla with matching UI/notices
while independent paths work. Injected hook/cleanup failures and their partial-
loss/restart UI remain unverified in game; pure tests are not runtime evidence.

##### Shipped in 0.2.2 (smoke-tested; remaining gaps in VALIDATION.md)

L-169 Item Transfer: the 0.2.2 part passed the maintainer's normal-build
smoke checklist on c8c1f1b (five new gestures/switches and chest/furnace/
brewing/beacon checks). Its batched-request rework and hotbar section passed
locally and on BDS on `37c3745` (2026-10-15). Full gesture/screen combinations, individually
itemized edge cases, other screen families, controlled rejection/timeouts
and world/dimension interruption, unverified-game fallback, packs and
servers remain coverage in VALIDATION and ITEM-TRANSFER.md. Development is
complete; the release build still needs the usual load/settings/changed-
feature smoke check before tagging. Later Design scope remains in L-169.

L-174 Chat position: native placement and the expiry/long-message fix
passed the maintainer's checks, including reset to the chosen flush-left,
bottom offset -44 default on `dc3ac45`. Fresh settings and broader layout,
pack and server coverage remain in VALIDATION (CHAT-POSITION.md).

L-182 (schematic preview crash after rejoining a world): fixed in
`95a09e0`; the reporter's doubt likely came from not knowing that rejoining
the same world is enough (maintainer 2026-10-15). No further action unless
it recurs after the next release.

##### Shipped in 0.2.1 (smoke-tested; remaining gaps in VALIDATION.md)

Changed features for the next smoke test; coverage is in VALIDATION.md:
Edge Guard on a server (L-160, L-161 margin setting, L-162 replay/server input), Find by key (L-159),
Chunk Borders and Hitboxes on walls (L-157), the Status effects HUD and
ammo counts (below), the Nether portal screen (L-148), Connected Textures
drawn with generated sprites and texture sets (L-164: resource packs and a
reload while in a world are unchecked), text fields with a caret (L-172)
and the calculator (L-171), repeated villager trading (L-166 part 1,
normal local/BDS checks complete), the Game language list (L-167; in-world
hang once, 10 s interval), Skip server resource packs (L-167, BDS cosmetic
pack only), pack images read from the packs (texture cache fix), and the
settings outside a world with the title screen entry (L-175). A release ZIP
is built from main as usual.

##### L-162 Edge Guard follow-up coverage (2026-10-13)

- Longer actual play and other servers; slippery blocks remain a known limit.
- The final run has twelve corrections, nine guarded replays and no fall
  dump. It does not guarantee protection if a server correction already puts
  the feet off support; incoming corrections are not clamped. The earlier
  +0.178 slab-bridging correction was not repeated at its identical location.
  If a fall recurs, preserve its environment/build/log before reopening work.

##### L-166 repeated trading follow-up coverage (2026-10-13)

- Chosen part 1 is complete and checked locally and on the normal BDS build;
  include it in the next changed-feature release smoke. No release selected.
- Controlled response delays pause MC_SERVER with communication running.
  Real network RTT/loss and other servers are untested. Normal logs do not
  isolate the exact timeout phase or dimension cancellation from an earlier
  stop; forced world exit with a pending response remains unchecked.
- Discount/demand price changes and longer actual play remain coverage limits.
  Favourites and remaining-use UI are separate undecided scope below.

##### L-147 and L-150 (2026-10-12, local only)

- Status effects HUD: beacon effects (no warning color), a server's
  effect updates (does the time count down between updates), Pocket UI,
  the layout editor sample, F1 and the inventory screen's own effect list.
- Durability HUD ammo: counts on a server after shooting and picking up.

##### Released builds and registry follow-up

The following are released-build checkpoints and listing follow-ups, not
unmet gates for versions already published. Registry pickup for 0.1.4 to
0.1.7 is confirmed (maintainer 2026-10-10); 0.1.8's registry PR
(LiteLDev/lipr, "add github.com/amatouhake/Lamium@0.1.8") was merged
2026-10-09 UTC.

- 0.2.1 was released on 2026-10-14 (`v0.2.1` at `eb949dc`; main and tag CI
  passed): ZIP SHA-256 `d4bf5270...40cd` (2.1 MB), symbols
  `Lamium-0.2.1-client-windows-x64.pdb.zip` SHA-256 `eec760b6...2fdb`; the
  GitHub asset digests match. L-175 settings from the title screen, L-171
  calculator, L-172 text fields, L-166 repeated trades, L-147 Status effects
  HUD, L-150 ammo counts, L-148 portal screen, L-164 Connected Textures
  sprites (with L-133, L-156), Edge Guard on servers (L-160 to L-162), L-159
  Find by key, L-157, L-155/L-158 labels, L-167 Game language and Skip
  server resource packs, and the pack texture cache fix. The maintainer's
  smoke test passed on the release-ZIP DLL `75f0046c...f47c`. No settings
  migration. After tagging: check that the registry PR picks up `v0.2.1`
  and that LeviLauncher/Bedrinth offer it.
- 0.2.0 was released on 2026-10-11 (`v0.2.0` at the commit recording this;
  built from `6f77159`): ZIP SHA-256 `4cc9f3cd...a1555` (1.8 MB), symbols
  `Lamium-0.2.0-client-windows-x64.pdb.zip` SHA-256 `a0f41454...2131c`
  (first release with the PDB as its own asset, L-135; GUID and age match
  the DLL). Clean-up and integration (L-134, L-136, L-137, L-138, L-139),
  L-140 Zoom Wide-angle view, map names on hover, L-141 overlapping ghosts,
  L-142 glint fix, L-143/L-145 vanilla HUD under Debug View and the Info
  HUD, L-144 hotbar in the HUD editor, paused-feature notices. The
  maintainer's smoke test passed on the release-ZIP DLL `71db2f0f...ae49c`.
  No settings migration. Tag CI passed; the GitHub release carries both
  assets with matching digests. LeviLauncher offers 0.2.0 (maintainer
  2026-10-12); Bedrinth not checked.
- 0.1.9 was released on 2026-10-11 (`v0.1.9` at `8bf28c0`; asset SHA-256
  `c0bbc358...a4b`): L-96 Connected Textures (panes with the one-texel
  stretch; split panes parked as L-133), L-128 player list, L-127 inventory
  HUD and used-slots counter, L-129 locked trades, L-130 English search,
  L-120 entity counts, L-125/L-126 FreeCamera readouts, waypoints and leave
  on hit, L-121 even Night Vision, the Vibrant Visuals distance fog fix and
  vanilla-batched item icons. The maintainer's smoke test passed on the
  release-ZIP DLL `0bc5eac6...079`. No settings migration. Tag CI was
  running when this was written. After tagging: check that the registry PR
  picks up `v0.1.9` and that LeviLauncher/Bedrinth offer it.
- 0.1.8 was released on 2026-10-10 (`v0.1.8` at `e175eed`, tag CI passed;
  asset SHA-256 `b99f9f27...e300da4b`): L-93 Schematics rendering and
  compatibility (version 2 saves, neighbors, render layers, liquids, block
  entity data, performance), L-114 3D previews, L-115 entity models, the
  screen review, map markers and target-card rows; L-118 distance fog
  (selected by default); L-109 death layout restore; L-15 step 1 breaking
  (to be redesigned, maintainer 2026-10-10); L-37 FreeCamera underground
  terrain (landed after the 0.1.7 tag; added to the notes after
  publishing); L-122 Fake Offhand fireworks;
  L-106/L-107 F1; L-108. The maintainer's smoke test passed on the
  release-ZIP DLL `1e2f4e6f...37ae55a2c`. No settings migration. After
  tagging: check that the registry PR picks up `v0.1.8` and that
  LeviLauncher/Bedrinth offer it.
- 0.1.7 was released on 2026-10-07 (`v0.1.7`, tag CI passed; asset SHA-256
  `2e0926e0...93b013b5`): L-93 Schematics (experimental), L-67 Weapon
  Switch, L-97 fixed fetch slot, L-94 offhand swap (`F`), L-95 Fake Offhand
  item use, L-102/L-103 Hand Restock threshold/order and inventory-screen
  transfer, L-104 map follow-ups, L-89 distant players, L-98 HUD density,
  L-99 Zoom below 2x, L-101 version display. The maintainer's smoke test
  passed on the release-ZIP DLL `2c3e7546...fd903806b` (`b3c6555`). No
  settings migration. Schematic gaps (servers,
  other dimensions, large files, block entities from files, half-drawn beds)
  are in VALIDATION.md and the README's Known issues.
- 0.1.6 was released on 2026-10-02 (`v0.1.6`, tag CI passed; asset SHA-256
  `3d864ca1...3946554f`): L-90 Simplified Chinese (first AI-assisted
  translation, corrections welcome), L-88 target hearts, L-75 offhand slot,
  L-63 saturation, L-64/L-92 food values and durability inside the vanilla
  tooltip. The maintainer's smoke test passed on the release-ZIP DLL
  `86f5baf0...2587883f` (`b71c9f8`). No settings migration.
- 0.1.5 was released on 2026-10-02 (`v0.1.5`, tag CI passed) at the maintainer's request,
  with the map (L-60, L-85, L-87) as its main change. The release build
  `6add9b9` (DLL `0fae1c58...dc657614c`, from the release ZIP) was deployed
  for a smoke test; no separate result was reported before tagging.
- 0.1.4 was released on 2026-09-30 (`v0.1.4`, tag CI passed). Its smoke test
  passed on the release build `4d25424`: the 0.1.4 version, Hand Restock
  (L-66) and offhand totems (L-68), Tool Protection (L-62), Tool Switch fetch
  (L-69), Auto Elytra (L-70) and the L-02 dedicated openers. Still open:
  Hand Restock on BDS and with real latency. The coverage gaps below carried
  past 0.1.4; recheck the relevant ones before the next release.

##### 0.2.0 regression checks

0.2.0 changes shared parts under existing features, so its smoke test also
covers (added 2026-10-11 from an outside review). Built on main 2026-10-11;
first deployed build `d274227` (DLL `c7163916...e084c468`). The steps were
built back to back without an in-game check in between, so each check names
the commits a regression would sit in (L-111 rule: one step's commits).
Check 1 (normal build, `2aea267`) reported passing on 2026-10-11
(VALIDATION-LOG), its two fixes too (`ca83014` Files names, `376cfae` map
name plates); the fail-open build (`891b798`) passed the same day. Left:
a code review of the clean-up steps before they move to done.
Review 2026-10-11 (two outside reviews of `v0.1.9..c2bbbe9`, findings
checked against the code by the agent): no blocking problem in L-134,
L-136, L-138, L-139 or L-137. Fixed: the overlap key missed an earlier
placement's file changing on disk, waterlogged water ignored the overlap,
and the overlap check now only looks at placements whose boxes meet
(`15e5b4a`); the release check compares README's full game version too and
GAME-UPDATES.md says the ghost gate also stops the in-world mistake
outlines (`0b3e9a4`). Not changed: the Placed tab still acts on list
indices while the screen is open (nothing else changes the list then);
"Open in Placed" from the map selects the placement, as every pick in that
tab does; the gate takes a lock per query only on an unverified game.
The steps:
- L-136 (`1a2c231`, `3ec2498`): schematic ghosts look and behave as on
  0.1.9 (doors, beds, panes, liquids, honey/slime, block entities, entity
  models, mistakes, large placements' frame rate), saving an area, the
  Check list and progress.
- L-138 (`261c0e4`, `d4a83d1`, `dd1f58c`, `8a4c71a`): Shapes and the
  breaking region look as before; the save area's corners now sit on the
  block's own planes, drawn over the white outline (no longer 0.025 out),
  with the overlay face alpha (look change to judge).
- L-139 groundwork (`534134f`, `7f85e7d`): waypoint editing, hiding,
  deleting and keeping the death point from the world map and the
  Waypoints screen act on the right entry, also with two waypoints of the
  same name and place.
- L-134 (`097d32a`, `b612e94`, `22b081a`): the settings screen after the
  split (below).
- L-137 (`44f48a8`): the gated paths behave as before on the verified game
  (Night Vision even brightness, Edge Guard, fog/medium/weather/nausea
  hiding, Connected Textures, FreeCamera terrain, schematic ghosts and
  entity models).
- Overlapping Shapes, breaking restriction and schematic frames: no missing
  faces, flicker or depth inversions (L-138).
- Map layers stay in sync with the data of the features that own them;
  selecting, hiding and acting from the world map change the right
  waypoint, placement or shape (L-139).
- Turning each feature off alone leaves other layers, overlays and input
  working.
- World exit and dimension change leave no stale selection, marks or draw
  resources.
- Settings input after the L-134 split: editing, saving, key capture and
  returning behave as before.
- The gated risky paths fail open without a crash on a build that forces
  the version check to fail (L-137): `xmake f ... --unverified_game=y`,
  each capability in GAME-UPDATES.md logs "stays vanilla" once.
- The release's PDB matches its DLL (L-135).

##### Pending feature checks

Server use of Weapon Switch, offhand swap, Fake Offhand, Hand Restock
(threshold/order, inventory-screen transfer) and fixed-slot fetch was
reported working on 2026-10-10 (everyday use, not every listed step); gaps
found later are handled as bugs.

- FreeCamera underground visibility (L-37): the 2026-10-07 native-request
  adapter passed cave drawing in a local world and on BDS on `d56b81e`;
  the log confirms the native 3 -> 5 replacement. Normal-view and player
  control restoration also passed. The follow-up confirms intended Hold/Toggle
  operation, camera retention through menus/focus loss and OFF on dimension
  travel. Other players' view of the body, world-exit/death regression coverage
  for the adapter, controllers and surface/shadow/graphics coverage remain open.
  See CAMERA.md and VALIDATION.md.
- FreeCamera speed controls (L-26): five-step adjustment and speed keys passed
  on `7e72244`; revised labels and forward-only sprint follow-up passed on
  `41b1ff6`. Restart persistence and detailed input/menu/focus combinations
  were not reported separately.
- FreeCamera Position reference (L-76): Player remains the default; World
  compensates body movement, with live switching preserving the camera target.
  Position retention, live switching and release passed on `43c4211`; rapid
  elytra/body movement fix (L-77), switching and release passed on `d20fdf8`.
  Lamium menus, inventory/window movement and release passed on `d3f0293`
  (L-78). Check target readouts, Hold input ownership and session cleanup;
  see CAMERA.md.
- Hide effects first step (L-42): master/rain-splash follow-up passed on
  `41b1ff6`. Remaining coverage: restart persistence, child keys while the
  master is off, both pipelines, ambient layers, resource packs, graphics
  modes and world/dimension transitions; see VISUAL-EFFECTS.md.
- Boss bars (L-42): hiding and switch behavior passed on `d20fdf8`.
  Check the optional key separately, other HUD elements, restart persistence
  and additional resource packs/graphics modes on a trace-disabled build.
- Nausea color (L-42): child/key/master hiding/restoration and unchanged
  effect/icon/vanilla preference passed on normal build `b239eb9`. Remaining:
  restart persistence, additional packs/modes and lifecycle/owner cases.

---
