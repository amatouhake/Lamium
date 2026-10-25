# Lamium backlog

Work the maintainer has decided to pursue. Ideas are discussed first and
enter this file only once they are to be worked on; each gets the next free
`L-` number, a kind and its open questions. A task with an open user-visible
choice is **Design**, whoever writes it down.

Each task has a **kind**, which decides who should pick it up:

| Kind | Meaning | Who |
|---|---|---|
| **Ready** | Spec is complete; mostly pure logic + tests + small glue | Any agent, including cheap models |
| **Design** | A user-visible choice is open. Output is a spec (often a web demo) that turns into Ready tasks | Maintainer + strong model |
| **Research** | Needs native reverse engineering, trace builds or runtime-driven debugging | Strong model; cheap models may only collect traces |
| **Refactor** | Preserves behavior while changing internal structure | Follow the item's model and validation requirements |

Ready tasks marked **(strong model)** are fully specified but visual or
cross-cutting enough that a strong model should implement them.

A large or open-ended feature (Map, placement and breaking features,
Schematic) starts with a conversation with the maintainer about what it
should be - purpose, scope, what is left out - before any spec, spike or
mockup (decided 2026-09-28).

**Bugs** (something that ships behaves wrongly) are listed first in their own
section and are fixed before new features. Each bug still has a kind that
decides who picks it up.

Finished and closed items live in [BACKLOG-DONE.md](BACKLOG-DONE.md) with
their full history. An `L-` number referenced elsewhere that is not in this
file is there.

Long feature contracts and dated build/research records are linked from the
L-item ([MAP.md](MAP.md), [SCHEMATIC.md](SCHEMATIC.md)); current status and
remaining work stay here. Document roles: [README.md](README.md).

## Release policy (decided 2026-09-28)

Lamium is developed at the maintainer's pace, for their own use first, and
published for whoever wants it. There is no "first release" gate.

- Release when a meaningful set of changes has landed and main builds, passes
  the tests and has no known crash. Before tagging, smoke-test the release
  build (the mod loads, settings open, the changed features work) and update
  the README feature tables and Known issues, and [FEATURES.md](FEATURES.md).
  A full regression is not required.
- From 0.1.4 on, releases are ordinary GitHub releases, not marked
  pre-release: the 0.x version, the README status and the Experimental badges
  say what is unfinished.
- Pushing a `v<version>` tag is public distribution. The LIP registry picks up
  every semver `v*` tag, including `-rc.N` style prerelease tags, whatever the
  GitHub pre-release flag says, and LeviLauncher offers it. A build meant only
  for testing is shared as a CI artifact or branch build without a tag, not as
  a GitHub pre-release.
- The version is set in `xmake.lua` and `tooth.json`. The asset is
  `Lamium-<version>-client-windows-x64.zip`, built by
  `scripts/New-ReleaseArchive.ps1` (never by hand); the folder inside stays
  `Lamium/`, and release notes name the asset the same way. The LIP registry
  adds each `v*` tag through a registry PR, so Bedrinth and LeviLauncher pick
  up a release without a registration step. Package rules and the release
  checklist: [DISTRIBUTION.md](DISTRIBUTION.md).
- Large features may start at any time. They land on main in steps, default
  off and with the Experimental badge, so main stays releasable while they
  grow. A step that is not usable yet stays out of the settings screen (or
  behind a build option) instead of being shown half-working.

## Current execution order

Keep this section short: ordering lives here; status, contracts and open
choices live in the L-items. An L-item wins if a summary drifts.

1. **Bugs first.** L-227 FreeCamera shows only the sky
   outside loaded terrain (Research; new native experiment needs the
   maintainer's go-ahead). L-177 mismatch-highlight fix is built and awaits
   the reporter's next schematic session, not another implementation.
   Parked: L-192/L-193/L-194 schematic drawing, L-123 elytra pose,
   L-91 shield glint and L-132 inventory-screen movement; respect each
   item's stop/resume conditions. L-181 trident pose is low priority.
2. **Small and medium work before large features/integration** (maintainer
   2026-10-10): L-196 license/provenance evidence; L-231 controls UX and
   L-234 cut-plane layers are merged; L-238 menu/key parity is done;
   L-237 missing-structure notice is done; L-236 is a follow-up; L-232 slime chunk indicator (needs in-game
   verification of the rule first); remaining Design/Research in L-166 trade
   helpers,
   L-167 resource packs, L-168 trade finder, L-169 transfer extensions,
   L-170 enchantment planner, L-173 quick shapes, L-190 list selection and
   L-163 a Lamium inventory screen. Agree open choices before building.
   L-206 recording-range implementation is built; its remaining work is
   runtime coverage. L-165 Mass Craft remains below Map and needs a scope
   conversation; existing packs already cover part of that need.
3. **Schematic follow-ups (L-93):** choose among L-114/L-115/L-116,
   L-183/L-184/L-185/L-186 and the remaining name-tag issue L-117.
   The initial feature and 0.1.8 work are complete, not new tasks.
4. **Placement/breaking:** L-15 breaking redesign before placement;
   L-59 held-placement style waits for the maintainer's go-ahead.
5. **Optional integration (L-111):** shared spatial actions, a common menu,
   selection and cross-links; the required 0.2.0 steps are complete.
6. **Research when convenient:** L-79 pumpkin/spyglass frames, L-71 glide
   initiation, L-30 dragon hitboxes, L-33 mob timers, L-105 profiling.
   L-151/L-152/L-153 and the previously parked work remain later.
7. **Before the next release:** changed-feature smoke and the relevant
   Pre-release checks below. Latest release: 0.3.0, 2026-10-22 (`v0.3.0`).
   No next date is selected; a full regression is not a release gate.

Notion reconciliation, 2026-10-20: four current schematic notes from the
[idea inbox](https://app.notion.com/p/3f7c12ab5b2d8121b2a0db32608f6b7a)
are tracked as L-229/L-230/L-231 (the two controls-UX notes share L-231).
The item/edit-mode proposal still waits for feedback; unchosen ideas such
as fall-rescue elytra, effect-only particles, rain sound, distortion
presets, reserved Fake Offhand slots and advanced subtitles stay in Notion.
This reconciliation selects no new implementation or release.

Task-picking rule: bugs first, then this order; follow each item's model,
dependencies and validation requirements. Cheap models skip Design,
Research and strong-model work. Completed work moves to BACKLOG-DONE.md;
remaining runtime gaps go below and in VALIDATION.md.

---

## Pre-release checks

The [Release policy](#release-policy-decided-2026-09-28) requires a smoke
check of the actual release build: load, settings and changed features.
Choose relevant checks from these known gaps; a full regression and every
server case are not release gates. [VALIDATION.md](VALIDATION.md) owns
current per-feature coverage; VALIDATION-LOG.md owns build/hash evidence.
Do not infer game success from a build or unit-test result.

### In 0.3.0 (since 0.2.3)

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

### Gaps carried from released builds

These are follow-up coverage, not unfinished implementations or release
gates for versions already published. Full old release/smoke/regression
checkpoints are retained in BACKLOG-DONE.md, under "Archived release and
validation checkpoints" (snapshot 2026-10-20).

| Area | Remaining targeted checks |
|---|---|
| Item Transfer, L-169 | Held-stack/drop gestures passed the local chest checklist; try inventory and slot-role screens, servers and a rebound drop key. Full gesture/screen matrix, cursor/hotbar/keep-one/re-entry/saved-switch cases, controlled rejection/timeouts, world/dimension interruption, unverified-game fallback and packs remain. See ITEM-TRANSFER.md. |
| Shared capability migration, L-198 to L-202 | Server/server-pack smoke. A separately requested forced-unverified build should check vanilla fallback and matching UI/notices while independent paths work. Injected hook/cleanup failures and partial-loss/restart UI remain unverified in game. |
| Settings/help and map layers, L-197/L-146 | Broader sizes, languages and editing states; narrow-window two-row map bar. Use current DESIGN and VALIDATION, not superseded footer-expansion checkpoints. |
| Chat position, L-174/L-203 | Fresh settings, GUI scales/Pocket UI, packs, world/dimension/focus and servers beyond the recorded local checks. See CHAT-POSITION.md. |
| Edge Guard, L-162 | Longer play, other servers, slippery blocks; correction that already puts feet off support remains a limit. Preserve environment/build/log if a fall recurs. |
| Repeated trading, L-166 part 1 | Real RTT/loss and other servers, price changes, forced exit with a pending response, timeout/dimension cancellation isolated from earlier stops; controlled MC_SERVER delays do not establish network behavior. |
| Effects/ammo HUD, L-147/L-150 | Beacon effects, server updates/counts, Pocket UI, layout sample, F1/inventory interplay. |
| In-world language/server packs, L-167 | English Search and schematic ghosts after a change, broader servers/packs, rapid switching/hang risk and the 10 s interval. Cosmetic BDS evidence does not establish all pack types. See LANGUAGE.md. |
| Camera and visual effects | FreeCamera world-exit/death/controller/body-view and hold-input cases, speed restart persistence; Hide effects restart/keys/master, ambient layers, packs, graphics modes and lifecycle. Detailed coverage is in VALIDATION.md, CAMERA.md and VISUAL-EFFECTS.md. |
| Distribution listings | 0.1.4–0.1.8 registry pickup is recorded; LeviLauncher 0.2.0 was seen. Later registry/LeviLauncher/Bedrinth checks remain unrecorded here. Verify the version being released using DISTRIBUTION.md rather than reopening old release gates. |

---

## Bugs

### L-247 Status effects HUD reorders rapidly under beacon effects
Kind: Bug / Design. Maintainer note 2026-10-22 after 0.3.0; not fixed in
that release. `statusEffects::order()` in
`src/features/information/StatusEffects.h` sorts by remaining ticks each
update. Beacon effects regularly refresh their durations, so their rows
repeatedly trade places and the HUD is hard to read.
Goal: keep rows visually stable across duration refreshes, without hiding
new effects, expiry or changing countdown values.
Open: settle an ordering rule with the maintainer (fixed effect-ID order,
preserve order while active, or a rule that treats beacon/ambient effects
separately); decide how ordinary potion effects should be ordered. Test
beacon renewals mixed with normal effects, new/removed effects and server
updates; confirm in game at a beacon. No release target selected.

### L-248 Shapes creation uses the body instead of the active camera
Kind: Bug / Design. Maintainer note 2026-10-22 after 0.3.0; not fixed in
that release. `referencePoint()` in `src/ui/ShapesView.cpp` takes
`LocalPlayer::getFeetPos()` for Standing block and Exact position. With
FreeCamera detached from the player, creating a shape (and Move here)
therefore uses the body's position rather than the location being viewed.
Goal: use the active camera's position when FreeCamera owns the view,
preserving normal player-relative behavior outside FreeCamera.
Open: confirm which other camera modes, if any, should be view-relative;
check Target block picking and its missing-target fallback as well as
snapping and dimension changes. Validate new shapes and Move here in game
with FreeCamera detached and with ordinary player view. No release target
selected.

### L-243 Large terrain gaps that only reload when the player comes near
Kind: Bug / Research, not reproduced; watch item. Maintainer report
2026-10-22 on `fca2ef9` (0.3.0), local world, after dimension
changes, world map teleports and FreeCamera use: large areas of terrain,
near and far, are missing (sky shows through the ground). Looking at them
does not bring them back; only walking or flying near reloads them. Earlier
rendering glitches (distance fog hiding) recovered when looked at; this
does not, and was not seen before.
Not yet known: which change since 0.2.3 is involved (candidates: the
FreeCamera spectator culler gate and underground hold L-207/L-213/L-214, the
distance fog change L-212, map recording now centred on the camera
L-241, or teleports themselves); whether it happens without FreeCamera,
with the world map recording off, with Hide effects off, or on 0.2.3.
First step: the maintainer's A/B answers, then a bounded trace if needed.
Related: L-227 (FreeCamera far out shows only the sky).
A/B the same day: with everything on and with FreeCamera, map recording
and Hide effects each used or avoided, already generated Overworld chunks
never failed to load again; the gaps did not come back under any condition.
Not a 0.3.0 blocker while it cannot be reproduced; reopen with the
conditions if it returns.
Seen while trying (2026-10-22), likely vanilla: in the Nether, walking
toward chunks that were never generated only generates them out to about
the fog distance around the body, whatever the render distance; FreeCamera
moving alone generates nothing, which with fog hidden looks like missing
terrain. Chunk generation follows the body; not a Lamium change.

### L-244 FreeCamera keeps the inside-a-block view after starting in a block
Kind: Bug. Maintainer report 2026-10-22: start FreeCamera while the body's
head is inside a block (buried in the ground); the camera then shows the
inside-a-block screen effect the whole time, also while flying in the air.
The effect should follow the camera, not the body, or be suppressed while
FreeCamera owns the view. Not a 0.3.0 blocker (leave the block first).

### L-245 Ghost face disappears next to real glass or a beacon from some angles

Kind: Bug / Research, low priority. Maintainer report 2026-10-22 (0.3.0,
`2900d1dc`): a white stained glass ghost next to real white stained glass
or a beacon shows no face at the shared plane from some angles.
`cullAgainstGhosts` treats a real blended full-support block as covering
the ghost face (`realOpaque`), assuming the real face is drawn. The game
also culls the real block's back face from inside it or at some angles, and
draws a beacon beam similarly, so both faces can be absent. Same family as
L-192/L-193/L-194 (real see-through blocks and ghost faces). Not a 0.3.0
blocker; record before changing the face rule.
First step: reproduce with one ghost and one real glass/beacon block, note
the camera side, then decide whether to keep the ghost face where the real
block's face is not visible from the camera.

### L-241 Pre-0.3.0 review: settings, map and schematic fixes
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

### L-240 Death layout restore sometimes restores unexpectedly or wrongly
Kind: Bug / Research. Maintainer report 2026-10-22 (everyday use, builds not
recorded): Death layout restore sometimes acts after a death the player does
not recall ("did I die there?"), and when it restores, the result sometimes
looks wrong. Hard to reproduce or pin down because deaths and pickups are
hard to observe. Keeps its Experimental badge in 0.3.0 (DESIGN.md).
First step: decide what to observe (which death a restore belongs to, the
recorded layout versus the pickups applied) and whether a bounded trace
build or an in-game log line per restore is the cheapest way to catch it;
no fix before a concrete case.

### L-227 FreeCamera outside the loaded area shows only the sky
Kind: Bug / Research. Maintainer 2026-10-19 (`9a565f0`): with FreeCamera
flown out past the render distance, all terrain disappears, also the loaded
terrain near the player that should still be in view; only the sky and
environment draw.
Rejected 2026-10-19: the L-207 culler gate. With the underground hold set
to Always (the spectator culler for the whole session, as before L-207)
the maintainer still saw only the sky.
Where (maintainer, by eye): about one chunk past the edge of the loaded
area, all at once, while the loaded terrain is still well inside the
camera's render radius. So something keyed to the camera's own position
having no chunk, not the render radius.
SDK (26.51.5 headers, 2026-10-19): the culler classes (`LevelCullerBase`,
`LevelCullerDistanceField*`, `LevelCullerManhattan`, `LevelCullerNone`,
`VisibilityExtimator`) are empty there, so the walk cannot be read.
`LevelRendererCamera` has the camera-centered view area (`mViewArea`,
`updateViewArea`/`setViewArea`, `mViewAreaDistance`), `mLastCameraPos`,
`shouldCullThisFrame(lastNumRenderChunksVisibleFromCullingPoint)` and
`_createCuller(type, ...)`. Candidates to test with a trace build, not
decided: (a) when the camera's chunk is not loaded, request another culler
type through the L-37 hook (types other than 3 and 5 are not mapped yet);
(b) find what the "culling point" is and whether it can stay at the nearest
loaded chunk. Each is a new game dependency (GAME-UPDATES.md) and needs the
maintainer's go-ahead. Terrain beyond the loaded area stays out of scope
(CAMERA.md). [CAMERA.md](CAMERA.md)

### L-177 Schematic mismatch highlight appears at the wrong position
Kind: Bug / Research. User report 2026-10-14.
Status: cause found and fix built 2026-10-14 (`0235527`), not checked in game.
Maintainer 2026-10-15: checked when the reporter next uses the schematic
features; no dedicated check before that.
Details from the reporter: with a layer view, Show in world / Nearest
mistake lit a cell far above the shown ghosts; showing it again was right.
Changing layers or moving a placement restarts the check, but the last
complete result stayed published, so the pointer used a cell from the
earlier layers. The result is now kept for the list but marked stale, and
Show in world / Nearest mistake say "still counting" until the new pass
finishes. Check in game: change the layer, at once press Nearest mistake
(counting), then after the pass it lights a shown cell.
Original report: a highlighted discrepancy in a placed schematic can appear at
a different world position from the corresponding incorrect block.
Investigate the check result's cell coordinates, placement translation,
rotation/mirroring, layer filters and overlay transform separately; do not
assume the check data itself is wrong. Compare Check rows, world-space
highlights and targeted blocks across transformed placements. Record a
reproduction before choosing a fix. [SCHEMATIC.md](SCHEMATIC.md).

### L-181 FreeCamera draws the held trident the wrong way round
Kind: Bug / Research, low priority (maintainer 2026-10-15: drawing only,
harmless). User report 2026-10-14; not yet reproduced.
Clarified 2026-10-14: nothing hits the player. In FreeCamera the trident in
the player's hand is drawn the wrong way round, so it looks stuck in the
player. A drawing problem like L-123 (stale pose state while the body is
out of the camera's normal view); check whether the use/charge pose or the
held-item transform is the stale part.
Status: open drawing issue; the original "struck by the trident" report
is superseded by the clarification above. Capture the held/use/charge
pose, camera/body visibility and local/server sequence when resuming. In FreeCamera, player interaction suppression and
restoration must remain intact. [CAMERA.md](CAMERA.md).

### L-123 FreeCamera stretches the worn elytra on the inventory player model
Kind: Bug, low priority. Reported from use 2026-10-09 (maintainer's notes).
Parked 2026-10-10 (maintainer) after two culling rounds; resume from the next ideas below.
Status: open. Checked 2026-10-10 (VALIDATION-LOG): wrong when the body is
out of view (camera near), right when the body is in view (camera far), so
the inventory model reuses pose state that only the world render of the
body updates. Next: Research which state (elytra wing animation) is left
stale and whether it can be refreshed for the inventory model alone.
Two rounds 2026-10-10, both reverted: keeping the body "visible" to the
camera's `LevelRendererCamera::isAABBVisible` (round 1: never matched on the
render thread) and to both `isAABBVisible` and `cullerIsVisible` on any
thread (round 2: 1751/249 calls, 10/22 near the body, the point test was
overridden) left the elytra stretched. So these camera tests are not what
skips the body's pose update. Stopped after two rounds. Next ideas (not
started): find the entity render queue's own visibility step
(`LevelRendererCamera::queueRenderEntities` and what it calls), or refresh
the pose on the inventory model's side (the paper doll render path,
`GeometryAtlas::DollRenderContextImpl::update`, `PaperDollData`).
Reproduction (maintainer 2026-10-10): wear an elytra, turn FreeCamera on,
move the camera away from the body, open the inventory. The elytra on the
player's 3D model in the inventory screen draws simplified and stretched
downward. The elytra slot icon is not affected. Whether pose, animation
state, level of detail by camera distance or the render context left by
FreeCamera causes it is unknown; a first check is whether the distance
matters (camera near the body vs far).

### L-117 Schematic entity name tags render badly in Japanese
Kind: Bug, low priority. Reported by the maintainer 2026-10-08 (L-93 checks).
Status: partly fixed (`47534d7`, checked in game 2026-10-08): the colored
fringes are gone. Not solved: Japanese text in these tags draws dark gray
instead of white; a white dark color brought the fringes back. The
maintainer accepts it for now but keeps the item open. World name tags
appear only over entities drawn as dashed frames (no model, L-115); they are
drawn in `GhostRenderer.cpp` `drawNameTags` (Japanese glyphs from sheet 48,
type 3, scale 1.333, with the font's own material).

---

### L-192 Glass ghost brightness changes by section with the camera position
Kind: Bug / Research. Maintainer report 2026-10-14 (VALIDATION-LOG).
Status update 2026-10-14 (2): the real see-through part passed; darkening left where blended quads overlap; fixed by sorting all blended quads together from the current camera, not checked.
Status update 2026-10-14: happens only where a real see-through block is below the ghost; fix built (real see-through faces hide the ghost face in their plane), not checked in game.
Status: open. More evidence 2026-10-14: only the current camera position
matters (not where the camera was), so the build-time sort hypothesis below
does not fit; with the camera below the floor both sides darken.
White stained glass ghosts turn darker or lighter in
world-aligned groups that look like the 8-block ghost sections, depending on
where the camera is, not which way it looks; moving the placement keeps the
dark area at the same world coordinates.
Hypothesis (not checked): blended ghosts draw both sides without depth
writes, so a glass cell's top and bottom faces both blend and the one drawn
last dominates. Each section sorts its quads far to near from the camera
position when it was built, and sections rebuild at different times, so
sections built with the camera on different sides of the faces end up in a
different order. Candidate fixes: drop back faces of blended ghosts (as the
game does for glass) and rebuild a section when the camera crosses its
planes, or re-sort blended quads when the camera has moved. Check first
whether walking below and back above the floor flips which sections are dark.

### L-193 Glass ghosts of two adjacent placements flicker at their border
Kind: Bug. Split from L-178 on 2026-10-14.
Status update 2026-10-14 (2): left where two placements' see-through quads overlap on screen; same global sort fix as L-192, not checked.
Status update 2026-10-14: does not happen with one placement; the flicker needs real see-through blocks below too; same fix as L-192, not checked in game.
Status: open. `pistons` (8x3x7 at -95 84 -60) and `shulkers` (5x5x5 at -87 84
-63) touch across x = -87; their white glass ghosts at y 84, z -60 and -59
face each other and flicker. Cause found from the log (VALIDATION-LOG):
next to the camera the pairing rule keeps one face of a shared plane, and
the two placements' sections, built with the camera on different sides,
each kept theirs. Fix built 2026-10-14: same-kind see-through pairs drop
both faces even next to the camera; the flicker remained (VALIDATION-LOG),
also where no faces meet, alternating between two looks. A perf trace build
checks whether sections keep being rebuilt.

### L-194 Save corner marks over real glass or water change with the camera
Kind: Bug / Research, low priority. Maintainer report 2026-10-14/15; parked
2026-10-15 after three tries (VALIDATION-LOG).
Status: open. A save corner mark (red or blue) in or against a real
see-through block (glass, water) looks different from either side of a
plane that lies on Chunk Borders lines: from one side the real water
behind the mark is hidden, from the other it shows through. Not over air
or opaque blocks; the same in Fancy and Simple graphics. Tried without
effect: drawing such marks in the block entity alpha pass, in the name tag
pass, and lifting the faces 0.02 out of the cell (also looked lifted over
stone). Kept: only the seen sides are drawn, and corner 1 is pulled nearer
than corner 2 (their shared edge lines no longer trade places). Next idea:
find where the game draws translucent terrain relative to the camera's
chunk, or draw the faces with a material that does not write depth.

## Feature work and research

### L-246 Option to trigger Auto Elytra with Fake Offhand fireworks
Kind: Design. Maintainer idea 2026-10-22 after 0.3.0; no implementation
or release selected. The existing firework-jump trigger in
`src/features/interaction/ElytraSwap.cpp` checks only the selected
main-hand hotbar slot (`holdingFireworks()`). A firework rocket assigned
to Fake Offhand does not qualify.
Goal: optionally equip an elytra on jump when Fake Offhand holds fireworks,
even if the main hand holds another item. Preserve the existing behavior
when fireworks are in the main hand and the chestplate return after landing.
Open: extend the existing Firework jump setting or add a separate opt-in
setting (and decide its default); how to handle disabled Fake Offhand,
missing/empty rocket stacks and other trigger restrictions. This is an
equipment trigger, not automatic firework use or guaranteed glide start
(see L-71). Choose the settings behavior with the maintainer first, then
test main-hand/Fake Offhand combinations and verify in game.

### L-232 Slime chunk indicator (Bedrock slime chunks do not depend on the seed)
Kind: Ready (HUD lines only; map and overlay deferred). Reported
2026-10-19 as issue #30 (the reporter offers a PR: HUD line first, overlay
separately). Chosen 2026-10-19; nothing built.
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
View only; the issue reporter may open a PR for that. The world map and
minimap, and any world overlay, are out of scope for now: the map design is
still undecided and changing, so revisit them later as a separate item.
Step 1 (Ready): pure `isSlimeChunk(x, z)` in a header with tests using the
list above, plus one line in each of the two HUDs (the Info HUD line is
opt-in, default off, and cheap; follow Settings/Translations rules in
AGENTS.md, three languages). Also correct the `help.debugView` text. Do not
ship the lines until the maintainer has seen the rule match real slime
spawns in game.
Open (maintainer): the line wording (yes/no only, or with the Y limit), and
whether the Y limit text is worth showing before the boundary (40 vs 39) is
verified. Record the formula's source in [PROVENANCE.md](PROVENANCE.md)
before using it; do not copy the reporter's code verbatim.

### L-233 Make inventory transfer gestures rebindable
Kind: Design, future. Maintainer 2026-10-20: now that a wheel direction can
be part of a binding, the transfer gestures should be configurable instead
of fixed to Shift/Ctrl/Alt plus wheel, drag or click. Not urgent; nothing is
blocked today.
Current model: `TransferGesture.h` derives 14 gestures (one/stack/matching/
all on the wheel, stack/one/keep-one/drop drags, matching/all clicks, cursor
matching) from hardcoded modifiers; the settings only enable or disable each
gesture.
Open: which gestures to expose and which model to use. Wheel gestures fit
the existing Press-plus-wheel chords. Drags are press-move-release and clicks
carry cursor state, so plain chords cannot express them without a gesture
model. Also decide modifier roles, defaults matching today, the container-
only context, conflicts with other actions, and whether to start with the
wheel subset only. L-169 owns transfer extensions; do not start without
choosing the subset and model.

### L-236 Rebuild only the ghost sections whose visible layer changed
Kind: Design, low priority. Performance follow-up split from L-234
(2026-10-21). Maintainer check 2026-10-22 on a desert village: stepping the
cut under Only is fine; under Up to the redraw after each step takes a
noticeable while. Not urgent: drawing a large schematic whole is heavy
anyway, so day-to-day work likely uses Only. A cheaper first step if it
returns: keep the old sections drawn until their replacements are built.
The ghost cache key is per placement, so moving the cut discards and
rebuilds every 8-block section (and re-resolves the palette), including sections whose shown cells did not change. Give
each section a layer visibility signature (fully shown, fully hidden, or the
cut boundary inside it) and reuse unchanged sections when the cut moves;
changing an unused field (Keep under Only, Axis/Cut under All) already avoids
a rebuild because the key only covers visible fields. Needs a pure
section-state helper and tests, renderer cache changes, and an in-game frame
check on a large placement while stepping the cut. Not part of the current
L-234 branch.

### L-206 World map recording range is a fixed radius instead of the view distance
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

### L-196 Review license notices, source access and provenance classification
Kind: Research, then Ready for changes supported by the findings.
Imported at the maintainer's request on 2026-10-15 from the current-tree
follow-up in the [Notion license/provenance review](https://app.notion.com/p/3e7c12ab5b2d81f09eb2d7bf61cd81a2).
Status: reviewed 2026-10-20 (findings below); P3-P5 done, P1 and P2 have
outstanding items, mostly outside the repository. No release chosen. The
source proposal covered public main / v0.2.1.
Keep `LGPL-3.0-only` and the existing source-use boundaries in
[PROVENANCE.md](PROVENANCE.md). This task does not approve importing outside
source or changing the project license. Historical/private migration
records stay outside the public repository.

Work in the source proposal's priority order:

1. **P1: independent-project notice.** Read LeviLamina's current Usage
   Guidelines (English and Chinese) and EULA before deciding the required
   wording. Check README, release descriptions and Bedrinth/LeviLauncher
   listing text for a clear statement that Lamium is an independent
   community project, not an official LeviMC product and not affiliated
   with or endorsed by LeviMC. Add a supported notice where missing; record
   which external surfaces were checked or still need an update. Do not
   treat the proposal's missing-notice observation as a verified violation.
2. **P2: corresponding source and distribution.** For GitHub Releases, LIP,
   Bedrinth and LeviLauncher, trace the distributed DLL to its exact source
   tag/commit, build instructions and dependency versions. Review SDK header
   terms, generated import libraries and linking/distribution conditions.
   Inspect the actual archive and its notices. If needed, add a source guide
   to the ZIP or release text and enforce it in the package checks. A source
   URL alone is not evidence that the whole distribution review is complete.
3. **P3: earlier authored components, one-time check.** Check authorship and
   third-party contributions in LaminaPeek, LaminaSort and LaminaView for
   the code actually used by Lamium. Verify retained copyright/MIT notices
   where required and record the scope and evidence in PROVENANCE / notices.
   Do not assume sole ownership from the repository owner or an agent's
   generated-code label.
4. **P4: SPDX metadata.** Confirm the installed xmake accepts
   `LGPL-3.0-only`, including dependency compatibility checks, before replacing
   `set_license("LGPL-3.0")`. Keep project metadata and notices consistent.
5. **P5: dependency classification.** Clarify direct source copies versus
   external packages/header-only dependencies compiled into the DLL, notably
   nlohmann/json in PROVENANCE group 2. Preserve its notice and the rule that
   new source incorporation needs an explicit provenance record.

Quick Container remains reference-only under the existing PROVENANCE entry;
no additional action is proposed for the already recorded behavior-only
L-169 observation. Its pack files/source/assets must not be opened for this
task. Comparisons and legal analyses remain in external research notes;
repository docs record concrete dependency facts, decisions and work status.

Files to inspect: `README.md`, `THIRD_PARTY_NOTICES.md`,
`docs/PROVENANCE.md`, `docs/DISTRIBUTION.md`, `xmake.lua`, release workflow,
`scripts/Check-Package.ps1`, `scripts/New-ReleaseArchive.ps1` and package
metadata. Acceptance: dated source/evidence references and resolved or
explicitly outstanding findings for P1-P5; build and LamiumTests pass for
changes, package/archive checks for packaging changes. Do not claim a full
similarity audit or legal clearance from this bounded review.

Findings (review of 2026-10-20 against main `f37d3d2` and release v0.2.3;
not a legal opinion):

- **P1, partly done.** Sources read: LeviLamina `main` at `4ca1356`,
  `EULA.en.md`/`EULA.zh.md` (version 0.2.0, 2025-01-20) and
  `docs/main/contents/common_guides/usage_guidelines.{en,zh}.md` (first
  official version 2025-01-04; the Chinese text prevails). The EULA covers
  LeviMC's closed-source software (PreLoader, PeEditor); Lamium does not
  ship it. The usage guidelines ask anyone using the LeviLamina name to
  avoid implying official status and to show, prominently on products,
  listings and descriptions: "Not an official LeviMC product. Not approved
  by LeviMC and not associated with LeviMC." (zh: "非官方 LeviMC 产品。未获得
  LeviMC 批准，与 LeviMC 无关联。"), and to state who is responsible and how
  to reach them. Before this review neither the README nor the v0.2.3
  release text carried it. Done: the README opens with the notice and the
  responsible maintainer and contact; `tooth.json`'s description carries it
  (reaches LIP, Bedrinth and LeviLauncher with the next tag); the DISTRIBUTION
  release checklist requires it in release notes. Outstanding: the published
  v0.2.0-v0.2.3 release texts (the maintainer edits them on GitHub or lets
  the next release carry it); the Bedrinth and LeviLauncher pages could not
  be opened from the review environment and need a look after the next tag;
  whether an in-game notice is wanted (a UI choice, not decided here);
  Minecraft's own usage guidelines, which LeviMC's guidelines defer to for
  sharing, could not be fetched and their disclaimer is not yet checked.
- **P2, partly done.** Inspected the published
  `Lamium-0.2.3-client-windows-x64.zip` (SHA-256
  `14c87daeb55e28b55bee87e5e0e2234cd232435e3ac804a5e5adf8bbb4ef9f05`, as
  GitHub's asset digest): `Lamium.dll`, `manifest.json` (0.2.3), COPYING,
  COPYING.LESSER, THIRD_PARTY_NOTICES.md and 22 license files; no Minecraft
  or LeviLamina binaries, no `.lib`. Its DLL imports `LeviLamina.dll`,
  `bedrock_runtime.dll`, the MSVC runtime and Windows system DLLs only. Tag
  v0.2.3 is `1040eaf`; LIP/Bedrinth/LeviLauncher install the same asset
  (tooth.json URL). Zstandard is absent from that DLL and its notice; it
  arrived after 0.2.3 with its notice. The build tools found: LeviBuildScript
  0.6.1's link rule runs LiteLDev's prelink v0.8.6 (binary release, no
  license file found) on bedrock-runtime-data (MIT) to make
  `bedrock_runtime_api.lib`; the SDK recipe makes `LeviLamina.lib` from the
  release DLL. Done: THIRD_PARTY_NOTICES (shipped in the ZIP) names the
  source repository, the matching tag, the build steps and the lock file,
  and records both generated import libraries; PROVENANCE says how each
  dependency enters the build. Outstanding: the terms of the SDK's `mc/`
  headers and of prelink's output (no license text found for either beyond
  LeviLamina's COPYING); whether the release assets are built from the tag
  by CI or locally (to state in DISTRIBUTION); a package-check rule that the
  source section stays was not added (the shipped file is already compared
  with the tracked one by `Check-Package.ps1`).
- **P3, done.** LaminaPeek, LaminaSort and LaminaView: history and licenses
  recorded in PROVENANCE group 2 and THIRD_PARTY_NOTICES. No third-party
  human contribution after each repository's start was found; LaminaPeek's
  older history is levilamina-mod-template's (CC0-1.0 since 2023-12-10).
- **P4, done.** In xmake v3.1.1 (the CI version) and v3.1.2,
  `set_license` stores the string unchecked and the package license check
  only tests the `LGPL-` / `GPL-` prefix (`core/base/license.lua`), so
  `LGPL-3.0-only` behaves as `LGPL-3.0` did. Changed in `xmake.lua`; CI run
  821 on `cb1b95a` built, tested and package-checked it on Windows.
- **P5, done.** nlohmann/json moved from PROVENANCE group 2 to group 1: it is
  a header-only xmake package compiled into the DLL, not source copied into
  the repository. No outside source file was found under `src/`, `tests/` or
  `scripts/`; `src/app/MemoryOperators.cpp` is the template's two-line
  memory-operator file, now listed with the template.

### L-190 Select several rows at once in list screens
Kind: Design. Maintainer idea 2026-10-14; no implementation or release chosen.
Goal: in list screens (waypoints, the schematic Check list and similar lists)
select several rows with Ctrl/Shift + click or by dragging, and act on them
together (for example hide, delete or show in world).
Open: which lists and which bulk actions; how Ctrl (toggle one) and Shift
(range) combine with the current single selection and the detail pane;
drag selection versus dragging to scroll or reorder; confirmation for
destructive bulk actions; keyboard equivalents.

### L-183 Temporarily disregard selected schematic check mismatches
Kind: Design, then Ready/Research depending on the chosen check state.
User feedback clarified 2026-10-14; not chosen for implementation or a release.
Goal: while building redstone and other stateful structures, temporarily
exclude selected expected-vs-actual discrepancies from the check results.
A block with a temporarily different powered/open/extended state can be
intentional during construction; the schematic file and physical blocks do
not change. This is **check-result suppression**, not hiding the ghost or
automatically relaxing every BlockState comparison.
Open: scope (one world cell, selected group, or a state key), persistence
(until manual reset, reload, or session end), whether ignored rows contribute
to completion totals/HUD/nearest-mistake, and how to unignore/review them.
Keep the original strict comparison available. [SCHEMATIC.md](SCHEMATIC.md).

### L-184 Inspect ghost block data and schematic container contents
Kind: Design / Research for saved data inspection. The target-card part
has local checks: ghost/real targets on `b5daa75`, followed by the ray and
ghost-edge fixes L-217/L-222/L-224 (last `30b66ce`, 2026-10-19). Saved
container/Block Entity inspection remains open. The sequence below is
the earlier 2026-10-15 target-card checkpoint, not a new implementation.
Target-card history 2026-10-15: the ghost-as-target card with a "Ghost of
<placement>" row was built first, then replaced by an upper/lower section
(`334889a`, `0b89cd2`) and a None / Stacked / Side by side choice (`771ba02`).
The maintainer then found those sections hard to read and decided: keep the
ordinary single-card layout and polish its details; keep the Target Info
setting (now an on/off "Show schematic information") and the ghost target.
Built: ghost name in light blue with "Schematic: Not placed yet" (no placement
name), difference rows first under the header, the cell compared live, the
setting saved with compatibility for the test builds' numbers (DESIGN "Target
and Schematics"). Awaiting in-game checks: ghost in the air and in front of a
real block, wrong/extra/state rows, the setting off, short screens.
Container inspection and saved Block Entity data remain Design / Research.
Goal: inspect the ghost under the crosshair: expected block name, facing,
BlockState and available saved Block Entity data. Provide a way to
understand inventory contents of barrels/hoppers and other data relevant
to redstone output (for example comparator signal), without assuming an
on-world text overlay is the right presentation for all of it.
Open: compact target-card details versus an expanded inspection view;
how to display inventory slots, NBT fields and signal values; what is
derivable from the saved file versus current world state; absent or
unsupported data and transform-aware directions. Do not promise data that
the client/schematic never stored. [SCHEMATIC.md](SCHEMATIC.md).

### L-185 Instantly load a schematic into the world when cheats allow it
Kind: Design, then Research (strong model). User feedback 2026-10-14;
not approved for implementation or release.
Goal: explicitly materialize an entire chosen placed schematic in the
world, analogous to the game's structure-block Load action, rather than
manually building ghost cells.
Open: exact permission/capability checks for local and server worlds,
world/block replacement policy, entities, block entities and inventory data,
size/loaded-chunk limits, placement transformations and failure/undo UX.
Never silently modify the world, use a client-side visual as proof of server
acceptance or bypass permission checks. [SCHEMATIC.md](SCHEMATIC.md).

### L-186 Improve schematic ghost legibility and reconsider translucency
Kind: Design / Research (strong model). User feedback 2026-10-14;
no renderer or release choice yet.
Part built 2026-10-15 (maintainer: choice (c) plus an outline width): a
"Ghost tint" setting (strong = the earlier look, weak = default, none) and a
"Ghost outlines" width (thin = one pixel, default; normal and thick draw each
axis-aligned edge again at four offsets across it). Not checked in game.
Later the same day normal/thick became solid bars (`7d79bef`, passed). Bars
of neighbors with different check results flickered; they now sit on the
shared edge and a stronger result's bar is slightly wider: wrong block >
unknown block > wrong state > ghost/missing. Wrong beside missing passed;
wrong-state stairs over a missing block still fought, so within a section only
the strongest copy of a shared edge is drawn and the width step grew to
0.002 (passed, `ee196c6`).
Ghost tint (maintainer, 2026-10-15): the three choices changed the look too
sharply, so it is now a 0-100% slider in 5% steps; 100% is the former
strong, 40% about the former weak, 0% none. Saved as `ghostTintPercent`; the
former `ghostTint` choice maps to 100/40/0. Passed in game (`ee196c6`). The
maintainer then chose 70% tint and Normal outlines as the defaults: a little
rugged, but the block and whether it is a ghost read best there.
Translucent ghosts stay open (maintainer: after the release).
Goal: distinguish ghost blocks from real blocks while keeping their native
colors readable. In the reported screenshot a pink concrete ghost overlapped
a light-blue overlay enough that the pink color, and even whether it was a
ghost, became unclear. Investigate the **current** light-blue outline/tint,
line density, face shading, overlap/occlusion and depth behavior first;
compare with translucent ghosts (previously not selected) as an optional
alternative rather than presuming translucency is better.
Evaluate dense redstone builds, ghost-on-ghost overlap, real/ghost boundaries,
opaque/cut-out/blended/water blocks and ordinary/Vibrant Visuals graphics.
Consider mode/opacity controls only after comparison; avoid changing the
existing default without the maintainer's decision. Related but distinct:
L-114's Check-tab 3D preview mistake styling.
[SCHEMATIC.md](SCHEMATIC.md).

### L-173 Quick shapes: a beacon's range at a glance, shapes that are not saved
Kind: Design. From the maintainer's notes (2026-10-13); not chosen for
building, no release.
Goal: see a beacon's range without making a shape (kind, center, radius,
saving).
Proposal (not decided): looking at a beacon, one action shows its range as
an overlay; the pyramid's levels set the size where they can be read, else
the shown size is marked as assumed. Shapes gain temporary ("quick")
shapes that are drawn but not saved, and can be kept as named shapes when
wanted; the same works for distances, radii and building guides.
Open: the range's vertical extent and edges as Bedrock behaves (check in
game), how long a quick shape lives (until cleared, rejoin, dimension
change), and where the action lives (key, radial menu). The Shapes editing
and saving flow stays as it is.

### L-170 Enchantment order planner
Kind: Design, then Research (strong model). Requested by the maintainer
2026-10-12; not chosen for building, no release.
Goal: given an item and the enchantments wanted, show the order of anvil
combinations that costs the least experience and avoids steps the anvil
refuses as too expensive.
Agent's proposal (not decided):
- Data from the game, so new enchantments need no Lamium update: the
  registry `Enchant::mEnchants()` gives each enchantment's id, max level
  (`getMaxLevel`), rarity (`Frequency`, which sets the anvil multiplier),
  compatibility (`isCompatibleWith`) and whether it can go on the item.
- Anvil rules (prior-work penalty doubling per use, per-level multipliers,
  books against items, the refusal threshold) as a pure, tested table.
  Bedrock's rules are not assumed to equal another edition's: check every
  rule against the cost the anvil screen shows (`AnvilScreenController`)
  before trusting the planner.
- Solver: a pure, bounded search over combination trees (exhaustive up to
  about 10 inputs, which covers every real item), minimizing total levels
  with no step at or above the refusal threshold; ties prefer fewer steps.
- UI: a Lamium screen; pick the item (held item preselected), tick the
  enchantments and levels (incompatible ones greyed), optionally "use the
  books I carry" (read from the inventory, with their existing prior-work
  penalty). The result is a numbered list of steps with each step's cost
  and the total.
- Later, if wanted: highlight the next pair in the anvil screen.
Decided (maintainer 2026-10-12): the first version plans from fresh books
only (no prior-work penalty on the inputs); planning from the books in the
inventory can come later.
Decided (maintainer 2026-10-12): an unbound hotkey (and the settings
screen) opens it with the held item preselected; anything inside the anvil
screen comes later.
Open (maintainer): whether the anvil highlight is wanted later.

### L-169 Item Transfer: remaining gesture and screen extensions
Kind: Design. Current status reconciled 2026-10-20: matching/everything,
keep-one and native slot-role routing shipped in 0.2.2; held-stack matching
and the four drop gestures shipped in 0.2.3. The former passed locally and
on BDS on `37c3745`; the latter passed the local chest checklist on
`9da5d9d` (2026-10-15). All five held-stack/drop switches default on, as
chosen by the maintainer. L-154 is merged here and archived.
Current behavior and authoritative confirmation rules: DESIGN.md "Inventory
transfer" and [ITEM-TRANSFER.md](ITEM-TRANSFER.md). Completion and the
superseded proposal/handoff are retained in BACKLOG-DONE.md (L-169's 0.2.2
and 0.2.3 records). Do not implement the old proposal again.

Unchosen extensions, with no implementation or release approved:
- Extra Shift + double-click (matching) or Ctrl + Shift + click (leave one)
  bindings; decide whether they add value beside the shipped gestures.
- "Fill existing stacks only" and W/S-directed moves; the latter were
  explicitly deferred, not selected.
- Additional screen families: reproduce a specific unsupported operation
  and confirm its native slot roles before extending coverage.
The full gesture/screen matrix, drop-key rebinding, controlled rejection or
timeout, lifecycle interruption, unverified-game fallback, packs and wider
server checks are validation gaps, not unfinished gesture implementation;
see Pre-release checks and VALIDATION.md.

### L-168 Trade finder: reroll a villager until a wanted trade appears
Kind: Design, then Research (strong model). Requested by the maintainer
2026-10-12 for later; separate from L-166 (trading once a good villager is
found). Not chosen for building, no release.
Goal: find a wanted trade (for example a librarian's enchanted book at a
given level and price) by resetting a villager's profession again and
again, for every profession, inside Lamium's UI.
Background: a villager that has never been traded with loses its
profession when its workstation is broken and rolls new trades when it
takes one again. On Bedrock the client receives every trade of every level
when the trade screen opens (L-129 research), so even level-5 offers can be
checked at level 1. Research first: when trades are rolled on 26.51 (on
taking the profession or on the first opening), how long the villager takes
to claim a placed workstation, and whether a server sends the same.
Agent's proposal (not decided), in two stages:
1. Assist (no automation): a list of wanted offers (item, enchantment and
   level for books, maximum price). Each time a trade screen opens, Lamium
   checks the received offers (L-129's parsing) and announces a match with
   a toast and sound; otherwise it says what was offered. The player breaks
   and places the workstation by hand.
2. Loop (only if wanted): with the player looking at a villager and its
   workstation, Lamium repeats break, place, open the trade screen, read,
   close, until a match or a stop. Every step goes through vanilla's own
   actions (break and build), waits for authoritative results, and stops on
   any player input, a missing workstation item, a different villager, a
   traded (locked) villager, low durability (L-62), or world/dimension
   change. Default off; help text warns that servers may treat the loop as
   automation.
Decided (maintainer 2026-10-12): stage 2, the loop, is wanted. The list of
wanted offers is kept per world or server (keyed like the map's per-world
data), not shared.
Open (maintainer): how wanted offers are entered (agent: a picker from the
game's item and enchantment lists), and whether matches are kept as a short
log.

### L-167 Change the language and resource packs from inside a world
Kind: Research, then Design (strong model) for the remaining pack work.
Current status, 2026-10-20: Game language and Skip server resource packs
shipped in 0.2.1; language was checked locally and server-pack skipping
with a cosmetic BDS pack. Broader coverage is in VALIDATION.md. General
stack editing and applying the player's global packs while skipping server
packs remain unchosen Research/Design. The dated research below is history.
Imported from the maintainer's notes (2026-09-25) on 2026-10-12.
Goal: switch the game's display language, and look at or change the global
resource pack stack, without leaving the world or server first.
Research first, before any spec: what 26.51 already allows from the pause
menu's settings (language list, Global Resources) and where it stops. Do
not build what vanilla already does.
Agent's proposal (not decided), in two parts that are judged separately:
1. Language (smaller): if vanilla refuses in-world, offer the same language
   list from Lamium (a settings row or the pause menu) and apply it through
   the game's own path (`GeneralSettingsScreenController::_chooseLanguage`,
   `MinecraftGame::onLanguageChanged`, `ResourcePackManager::onLanguageChanged`,
   `FontRepository::onLanguageChanged`), never by writing option memory.
   Lamium's own UI follows the new language on the next frame. Check that
   item names, signs, chat and open screens refresh, and that L-130 English
   search still works.
2. Resource packs (larger): first a read-only view of the active global
   stack in-world. Changing it is not hot reloaded: queue the change, leave
   the world through vanilla's exit, apply the stack
   (`ResourcePackManager::setStack`, the same path as the Global Resources
   screen), then rejoin the same local world. Rejoining a server is only
   offered if the game keeps its address without Lamium storing it. World
   and server packs are never changed or skipped.
Decided (maintainer 2026-10-12): the language part first; resource packs
wait until it is done and seen.
Decided (maintainer 2026-10-13): General settings has a "Game language"
entry opening the game's language list. The maintainer confirms the vanilla
pause settings have no Language entry on 1.26.51.01. This establishes the
missing entry, not the safety of applying a language during play.
Current research: `language_trace` observes vanilla selection, the saved
option, I18n, game/font/pack observers and pack stack changes without changing
any return value or argument. The title-screen baseline is recorded.
Language part done 2026-10-13 (`9651b4e`, seen in a local world, kept
after restart): General settings > Game language. Not separately seen:
English Search after a change, schematic ghosts, BDS. All 29 game
languages listed since `cedccf6` (seen).
Resource packs, vanilla baseline (2026-10-13, VALIDATION-LOG): in-world
Settings > Global Resources lists the active packs but offers no control to
change them, and no stack change happens in the world. Vanilla's in-world
JSON settings also drops the Language tab with a static
`not $is_pregame` condition; the controller still holds the language list.
Server packs (2026-10-14): accepting a required pack and not applying it
locally works on BDS for a cosmetic test pack (research build only); in-world
Global Resources then shows a gray page. Hang: 12 in-world switches in 23 s
froze the game; the title screen does not (VALIDATION-LOG).
Decided (maintainer 2026-10-14): the language entry stays in Lamium's
General settings, drawn inside the settings frame; in-world switching stays
with one change per 10 s and a notice (experimental). Server packs: one
Lamium switch "do not apply server resource packs" for remote servers,
taking effect on the next join, with a notice when packs were left out
(the usual Lamium message toast, or a chat line only the player sees; the
toast is the agent's default). Per-server and per-pack choice are not
wanted now. The look is vanilla: on a required-pack server vanilla also
drops the player's global packs and shows a gray Global Resources page
(confirmed on unmodded Minecraft). Built 2026-10-14, not yet seen in game.
Later (maintainer 2026-10-14): show the player's own global packs instead
of vanilla while skipping a server's packs; needs research on reapplying
the Global stack during a server session.
Contract, findings and next checks: [LANGUAGE.md](LANGUAGE.md).
Added research request (maintainer 2026-10-13): whether a server-required
resource pack can be disabled locally without general pack-stack editing.
Separate receiving/accepting packs from applying their visual resources.
This request approved feasibility research only; the switch above was
decided after the research (2026-10-14). The download/acceptance handshake
is never changed or skipped; only local application is.
Fail open: if the game's own apply path cannot be found, show nothing.

### L-166 Villager trade helpers: favourites and assisted trading
Kind: Design, then Research for optional part 2. Requested by the maintainer
2026-10-14; no implementation or release approved for part 2. Part 1
(Shift + right-click repeated trading) shipped in 0.2.1. Keep that gesture
and ordinary left/plain right click.
Part 1 record: [BACKLOG-DONE](BACKLOG-DONE.md#l-166-villager-trade-helpers-part-1);
runtime contract: [TRADE-HELPERS.md](TRADE-HELPERS.md); tests: VALIDATION.md.
Sources of the idea: PROVENANCE.md (reference only).

Goal: make a familiar villager's useful trades one action rather than manually
finding and repeating rows every visit. Prefer small extensions to the vanilla
trade screen, not a replacement screen or more one-off feature toggles.
Automation stays on the **current trade screen**: Lamium never scans for,
walks to or opens other villagers. Remaining uses and bounded trade counts
are presentation candidates; keep the vanilla-looking UI and existing
behaviours.

Proposed part 2, in incremental stages (not yet decided):
1. **Favourite offers.** Toggle a star/marker on a trade row, possibly via
   middle-click (Shift + middle-click for shared favourites). Verify the
   input mappings, other mods and vanilla pick-block behaviour before choosing
   gestures. Avoid reordering vanilla rows at first. Investigate scope:
   per-villager favourites plus profession-wide defaults, with individual
   favourites taking precedence where set; consider whether an additional
   all-professions global scope is useful. Persist per world/server, as with
   other world-specific Lamium settings. Research a stable villager identity
   across sessions/servers; do not persist by a transient actor runtime ID.
   Match offers by verified contents (including enchantments and trade tier),
   not an unchecked row index; account for duplicate offers and dynamic prices.
2. **Trade all favourites on demand.** A separate unbound action/key processes
   the current villager's matched, unlocked favourites in a deterministic
   order, as far as stock, materials and output space permit. Reuse part 1's
   vanilla trade path and per-request authoritative confirmation. Never fall
   back to an unfavourited row if none can be traded. Leave the existing
   Shift + right-click one-row action unchanged.
3. **Optional automatic run and close.** Option (default OFF): when the
   player opens a trade screen, identify favourites and run the same queue
   without another click. Separate option (default OFF): close the screen
   *after* a successful, normally completed favourite run. Proposed guard:
   at least one confirmed trade, no further actionable favourites, and no
   pending requests. No favourites, manual interruption, inventory full,
   mismatched offers, timeout or other abnormal stop leaves the screen open.
   Never open a different villager or drop items to continue.
4. **Faster request batching (Research, optional).** The L-166 trace in
   TRADE-HELPERS.md confirmed vanilla result-slot Shift + click can submit
   multiple crafts in one request, limited by ingredients already in the
   trade input slots; it does not refill from inventory. Explore a verified
   refill -> vanilla multi-craft -> acknowledge -> revalidate cycle rather
   than one craft per round. Benchmark against the shipped implementation
   on local worlds and BDS before adopting. Do not synthesize custom trade
   packets or send more work while a response is pending.
5. **Optional display.** Consider remaining available uses, actionable
   trade count and minimal in-progress feedback; keep favourites readable
   without obscuring locked offers and their tooltips (L-129). Exact trade
   count limits and price caps are candidates, not committed features.

Safety and validation gates:
- Reconfirm the chosen villager, offer, costs, lock/stock state, inventory
  capacity and request result at every step. Stop on failed/untracked/stale
  responses, price or recipe changes, insufficient space, screen/focus/world/
  dimension change, settings-off or player input. Never drop/overflow output.
- Check local and BDS: two-input enchanted books, restocks/level changes,
  discounted/demand-adjusted prices, identical-looking offers, sold-out rows,
  no materials, full inventory, interrupted close, network delay/loss and
  server rejoin. If a stable persistent identity or reliable vanilla input
  hook is unavailable, fall back to an explicit/smaller manual mode.
- Keep **L-168 Trade finder** separate: L-168 finds a wanted offer by
  rerolling a villager's profession; L-166 performs trades once the offer
  exists. Share offer matching if practical, not automation of workstation
  placement or profession changes.

Open for the maintainer: favourite scope and identity, gestures, queue order,
automatic-run/close UX and success criterion, whether use counts and caps are
worth the UI, and whether batch optimisation justifies its compatibility
surface. Treat every item above as a design candidate, not permission to
build it.

### L-165 Mass Craft: craft a recipe many times at once
Kind: Design, then Research (strong model). In the notes since 2026-09-22;
imported on 2026-10-12. Not chosen for building, no release. It still needs
the conversation about purpose and scope that a larger feature starts with
(see the top of this file).
Goal: turn a whole inventory of ingredients into products with one action
(logs into planks, iron into blocks), instead of repeating fill-the-grid and
take-the-output by hand.
Vanilla baseline to measure first: what one recipe-book click and one
shift-take of the output already do on 26.51 (how many crafts per action,
where the output goes, what happens when it does not fit). Craft requests
already carry a craft count (`ItemStackRequestActionCraftBase::mNumCrafts`),
so vanilla may already do more per action than it looks.
Agent's proposal (not decided):
- A "Craft all" action on a recipe in the recipe book repeats vanilla's own
  steps (`CraftingScreenController::_handleRecipeSelect`, then
  `_handleCraftItem(ItemCraftType::All)`) until the ingredients run out,
  the output would not fit or the player stops it. Lamium never builds
  crafting transactions; the server validates every round.
- One round at a time: the next starts only after the inventory changed as
  expected (ingredients down, output up) on authoritative state; a mismatch
  or timeout stops with a toast. A per-tick bound keeps the rate modest.
- Stops on: screen closed, any click or key by the player, focus loss,
  world or dimension change.
- Never drops items: check room for the output before each round.
- Screens: the 2x2 inventory grid and the crafting table first. Stonecutter,
  loom, smithing and furnaces are later or out.
- A small progress toast ("Crafting 320/640"), and the total at the end.
Watch out for: recipes with leftovers (buckets, bottles), recipes that
accept several ingredient kinds (any log), the OreUI recipe book versus the
JSON UI screen (L-130 found only the latter covered), servers that limit
request rates (help-text warning like Auto Attack's).
Decided (maintainer 2026-10-12): Ctrl + click on a recipe crafts all
(no count in the first version), default off: Ctrl + click lies close to
Shift + click and an accidental mass craft would hurt. Crafting from nearby
chests is out.
Open (maintainer): whether a "craft the last recipe again" key is wanted
(agent: not in the first version).

### L-163 A Lamium-drawn inventory screen
Kind: Research, then Design (strong model). Raised by the maintainer
2026-10-12 after L-149 (opening the vanilla inventory in a portal is refused
by the server) and L-132 (moving with the vanilla inventory open failed).
Not started; recorded for later.
Idea: draw an inventory-like screen in Lamium and move items through the
same client inventory path Lamium already uses without a screen
(`inventory::game` moves; Auto Elytra and Hand Restock). Evidence: Auto
Elytra swapped items while the player stood in a portal (local world), and
Hand Restock works on BDS. Such a screen could open in portals and let the
player move (a different input routing than the vanilla screen).
Costs and risks: slot rendering (Inventory HUD parts exist), click, right
click split, shift click, drag spreading, number keys, armor and offhand
slots, tooltips; 2x2 crafting is much harder and should stay out at first;
loss or duplication if a move is not confirmed from authoritative state
(DESIGN engineering rules); servers with their own anti-cheat may refuse;
version sensitivity.
Suggested order: (1) a spike moving one item from a Lamium screen in a
portal and while walking, locally and on BDS; (2) if it holds, a minimal
screen (main inventory, hotbar, armor, offhand; click and shift click; no
crafting); (3) a mockup agreed before the real build.

### L-132 Move while the inventory screen is open
Kind: Research, then Feature. Requested by the maintainer 2026-10-10;
direction agreed the same day.
Status: open; paused 2026-10-10 after eight trial rounds without smooth
movement (findings below).
While the player's own inventory screen is open, keep walking, jumping and
sprinting from the bound movement keys so items and armor can be arranged
on the move. The camera cannot turn (the mouse belongs to the screen).
- Only the player inventory screen at first; containers (chests etc.) and
  other block screens come later, if at all.
- Sneak is not passed through: Shift is shift-click (quick move) in the
  inventory.
- Paused while a text field has focus (creative search, recipe book search)
  and on focus loss.
- Default off; the help text warns that some servers may treat moving with
  an open inventory as cheating (like Auto Attack's warning).
- Research first: where the game stops reading movement input while a screen
  is open (`ClientInputHandler`, `ClientMoveInputHandler`,
  `ClientInputMappingFactory::_createScreen*Mapping`, `MoveInputComponent`)
  and the least invasive place to feed the movement keys back in, without
  touching game state from the window procedure (AGENTS.md rule 2).
- Trial record (2026-10-10, `xmake f --inventorymove_trace=y`,
  `src/features/interaction/InventoryMoveTrace.cpp`, single player on
  1.26.51.01; commits 7e56cc8..40de3e3, last DLL 9edd5c65...):
  - `ClientInputUpdateSystem::extractRawHIDInput` keeps running (~20/s)
    with `inventory_screen` open; vanilla's bindings read from
    `getOptions().getCurrentKeyboardRemapping()` (`key.forward` = 87, ...,
    `key.sprint` = 17). Bedrock's move x is positive to the left.
  - Raw flags written after extraction are gone again by
    `PlayerMovement::calculateMoveVector` (two calls per tick: the
    `RawMoveInputComponent` state and a second state, likely the local
    server's). No movement.
  - Writing the flags into `MoveInputComponent`'s states after
    `inputHandlerUpdatePlayerState` (and forcing the vector) reaches
    `calculateMoveVector`, yet `MoveInputComponent::mMove` and the built
    `PlayerAuthInputPacket` stay zero: the player moved only through server
    corrections (~2/s), in jerks; no jump. The injected state flags also
    persisted after closing the screen (kept walking). Patching the auth
    packet's move vector and input flags did not smooth it.
  - Not the cause: `MoveInputComponent::Flag::MoveInputStateLocked` and the
    client input locks are clear; `UIScene::absorbsInput` forced false for
    the inventory changes nothing; copying the gameplay key bindings
    (`button.up/down/left/right/jump/sprint` from `gamePlayNormal`) into the
    `screen*` input mappings changes nothing.
  - Next guesses: find where `mMove` is zeroed while a non-play screen is
    on top (inside `ClientInputUpdateSystemInternal::tickUpdateClientInputView`,
    perhaps via `VanillaClientGameplayComponent` or a play-screen check);
    or whether the input handler only routes movement button events to
    `ClientMoveInputHandler` while the gameplay mapping is the active one
    (`ClientInputMappingFactory::_activateMapping`).

### L-59 Held placement style: vanilla, Java-like or fast
Kind: Design done (discussion with the maintainer, 2026-09-28); Research
first, then Ready **(strong model)**. Started as "keep placing across a left
click" after L-49 closed as vanilla parity.
Status: planning. Nothing is built, and no step starts until the maintainer
says so.
What it is for: building bridges and long straight lines quickly by holding
the use button with a block. Block placement only; eating, bows and buckets
keep vanilla behavior.

#### Spec (decided 2026-09-28)
- One setting, "Held placement", under Actions: Vanilla (default), Java-like,
  Fast. Default Vanilla because players expecting Bedrock would be
  surprised.
- Vanilla: Bedrock's own held build session, unchanged.
- Java-like: while the use button is held with a block, place on whatever
  face the crosshair targets at Java's fixed interval (every 4 ticks):
  no direction lock and no placing into air; nothing is placed when the
  crosshair is not on a face. An intervening left click does not
  end it (the original L-59 request): placing resumes while right is still
  held.
- Fast: while right is held, place on each new block face the crosshair
  crosses, capped per tick (a setting with a modest default). The help text
  warns that some servers may treat it as unfair. Also survives a left
  click.
- Both non-vanilla styles respect the placement restriction (L-15) when it
  is on, which turns Fast into "paint a flat floor".
- Interactive blocks (chests, doors, buttons), ordinary attacks, Fake
  Offhand and custom activation chords keep their current behavior.

#### Steps
External behavior research suggests the following hypotheses, but they are
not treated as facts until Lamium traces current Bedrock: while the button is
held, Bedrock may lock the build direction set by the first placement; blocks
hovered outside that direction may be ignored; the session may place into air
in front of the last block while bridging; and it may place as soon as a new
position is valid instead of on a fixed interval. The Java-like mode defined
above places on the currently targeted face every 4 ticks and does nothing
when no valid face is targeted. Hypothesis sources (Java mods describing
Bedrock, behavior only): modrinth.com/mod/pro-placer and
github.com/squeeglii/BridgingMod/issues/13.

1. Research: record Bedrock's held build session with the L-49 trace
   (`research_trace`, `FakeOffhandTrace.cpp`) and confirm or correct each
   point above: the direction lock (what sets it, which positions it
   accepts), placing into air along it, the timing, and what ends the
   session. Lamium already hooks `GameMode::buildBlock` and
   `SurvivalMode::buildBlock` for detached-camera interaction; test whether a
   discrete ordinary build action is sufficient for one server-authoritative
   placement, and map any required start/stop lifecycle. Do not synthesize a
   custom packet merely to imitate held input. Report before building.
2. Java-like style (Ready after step 1), with tests for the interval and the
   left-click rule.
3. Fast style with the per-tick cap; then combine both with L-15's placement
   restriction once that exists.

### L-15 Breaking and placement restrictions
Kind: Design (reopened). Breaking shipped in 0.1.8 and will be redesigned;
placement waits for that Design and then needs Research. The 2026-09-28
plan below replaced the old Breaking Restriction (capture/reset keys).
Status: step 1 (breaking) built 2026-10-07 with L-73 step 13 and checked in
game 2026-10-08 (VALIDATION-LOG). Implementation notes: RESTRICTIONS.md.
Shipped in 0.1.8; the maintainer is not yet satisfied with it and expects to
redesign breaking too (2026-10-10).
Reopened for Design (maintainer, 2026-10-08): the plan dates from 2026-09-28
and should be rethought before placement is built. Problems seen: too many
modes to cycle through, and the placement mode has no key (and does nothing
yet). Steps 2-3 below wait for that rethink; the breaking behavior that
passed stays until it is replaced. Faces z-fighting: L-110 (fixed).
Decided 2026-10-07: breaking keeps the existing Breaking Restriction toggle
and Cycle Breaking Mode bindings (no new default keys); the saved breaking
mode carries over unchanged and Height band is added to the list.
What it is for: leveling ground and digging tunnels without breaking past a
chosen level or face, and laying floors, walls and roofs flat without
placing outside them. Placing with a chosen facing is not wanted for now.

#### Spec (decided 2026-09-28)
Anchor and lifetime
- The anchor is the block where the button press starts (breaking: the first
  block mined; placement: the first block placed). The restriction holds
  while that button stays held and ends on release. The capture and reset
  keys go away.
- Rejected blocks never end the physical hold: when the crosshair comes back
  to an allowed block, breaking or placing continues (L-36 already does this
  for breaking).
Breaking modes
- Layer: only blocks at the first block's Y.
- Height band: from the feet level up N blocks (default 2, a setting); this
  mode is anchored at the feet, not the first block. For 2-high tunnels and
  wide leveling.
- Plane: the plane of the first block's mined face.
- Line and column: straight on from the first block, or vertical through it.
Placement modes (chosen separately from breaking, so leveling and laying a
floor can run at once)
- Layer (same Y as the first placed block), plane (a wall: the vertical plane
  through it, oriented by the first clicked face), line, column.
Controls and feedback
- Breaking and placement each have a switch with a key and a "next mode"
  key (no default keys decided yet); while on, the Status element shows the
  mode.
- While the button is held, the allowed region shows as faint faces like
  Shapes, skipping the targeted block so the vanilla outline stays visible.
  A rejected block is silent: no sound, no toast.
- Later, after the basic modes work: shape-linked modes (inside a shape, on
  its surface).
Settings and ids
- The Actions category keeps one "Block Restrictions" group. The old capture
  and reset actions stay in `enum Action` (ids are saved) but are no longer
  shown or dispatched; the old breaking mode setting maps to the new list.
  Say so in the settings migration notes before changing the store.

#### Steps
1. Breaking (Ready, strong model): press-anchored lifetime, the four modes
   plus height band, the allowed-region faces, Status line, removal of the
   capture/reset flow. Pure region predicates already exist
   (RESTRICTIONS.md); extend them and their tests. In game: every mode in
   survival and creative, held-button target changes, Tool Switch together,
   world exit and dimension change.
2. Placement gate (Research): Lamium already has cancellable
   `GameMode::buildBlock` / `SurvivalMode::buildBlock` hooks in the
   detached-camera interaction guard. Determine whether that boundary can
   reject the whole placement before mutation and, separately, how to derive
   the actual destination cell/state using vanilla placement semantics rather
   than assuming clicked-block + face is always correct. Trace ordinary
   blocks, replaceable vegetation, slabs/snow, doors/beds, signs, redstone,
   waterlogged/merge cases and edge placements without touching container use
   or buckets. Prefer a vanilla placement-prediction API when available.
   RESTRICTIONS.md lists the existing trace candidates. Stop and report if no
   safe path exists.
3. Placement modes (Ready once step 2 finds a path): the four modes, anchor
   on the first placed block, faces and Status line.
Input from the maintainer's notes (2026-10-13), for the redesign: protect
by a fixed height chosen on the spot. From the looked-at block or the feet,
"keep everything at or below this Y" or "break only above this Y", with the
boundary plane previewed and moved with the wheel. Unlike the press anchor
or the height band, it stays in world coordinates until cleared. Open:
clearing it, dimension changes, combining it with fixed ranges; ordinary
mining and the existing modes must keep working.

### L-91 Shield glint missing in Lamium's item icons
Kind: Research. Found by the maintainer 2026-10-02 while checking L-75;
leather added 2026-10-06 (found under L-61).
Status: parked 2026-10-06 after a trace and three in-game experiments; the
Update 2026-10-10: leather layers (and fence gates, L-119) fixed by the
shared-mesh icon path `inspection/render/ItemIcon` (checked). Update
2026-10-11 (L-142): glinting items went back to the direct path for their
glint, so an enchanted leather piece shows its undyeable parts
see-through again (maintainer: record it, not now). Left: the
enchanted shield's glint; one round failed and is parked; the research
record and the next idea are in BACKLOG-DONE under L-119.
cause is known, the fix needs a different draw path (below). Known issue.
Symptoms: an enchanted shield shows no glint in container previews and the
L-75 offhand slot; dyed or undyed leather armor loses its undyeable layer in
container previews and the durability HUD. Vanilla slots show both.
Findings (VALIDATION-LOG 2026-10-06, `icon_trace`):
- Lamium draws icons with `ItemRenderer::renderGuiItemNew` (plus a foil call
  when `Item::isGlint`). Vanilla slots are `InventoryItemRenderer`, a UI
  custom renderer whose passes the UI batch prepares (material per pass, two
  texture slots) before calling `renderGuiItemInChunk`.
- Leather: one slot pass, UI material `Item` (13), chunk type 2, and one
  `iconBlit` with exactly the arguments Lamium's call makes. With
  `renderGuiItemNew` the undyeable pixels are dropped by every material
  tried: the plain icon material, the multi-color tint material (which tints
  the dyeable pixels with the *secondary* color) and the UI `Item` material;
  the entity change-color material draws the transparent background too.
  Vanilla's own equip animation, which uses `renderGuiItemNew`, shows the
  same gap (maintainer's recording, 2026-10-06). So this call cannot draw
  the layer; it is not a missing argument.
- Glint: flat icons get three slot passes (`ItemGlintStencil` chunk 2,
  `InventoryItemGlint` chunk 4, `ItemUnglintStencil` chunk 5); Lamium's single
  foil call at 1.35 matches them. The shield is a model (`Shield` material,
  chunk 7) and the foil call draws nothing for it.
- Calling `renderGuiItemInChunk` outside a slot drew a flat tint square
  (L-61), because the batch setup is missing.
Next step if resumed (a larger change, maintainer's call): draw these icons
through the slot path, e.g. drive an `InventoryItemRenderer` (or its pass
setup) from Lamium, which could fix the shield glint too. Keep `isGlint` as
the glint predicate; do not replace it with `isEnchanted`.

### L-79 Carved pumpkin and spyglass frame draw path
Kind: Research. Cheap models may run the steps below and report; implementing
a hide switch needs a strong model. Parked from L-42 on 2026-09-30.
Goal: find a typed, verifiable way to skip only the carved pumpkin overlay and
the spyglass frame while keeping the pumpkin worn and the spyglass zoom.
Known (VISUAL-EFFECTS.md "Gated frame trace" and after): neither frame passes
through the three `Mesh::renderMesh` overloads, `ScreenRenderer::blit`,
`Tessellator::triggerIntercept` or the UI render context
(`getTexture`/`drawImage`/`flushImages`), even counted per frame. The frost
frame and nausea color do pass through `Mesh::renderMesh`. The vanilla pack
has `textures/misc/pumpkinblur` and `textures/ui/spyglass_scope`; the vanilla
UI definitions reference neither. `FullScreenEffectRenderer` and
`OnCameraEffectRenderer` (members of `InGamePlayScreen`) are opaque in SDK
26.51.5.
Steps, one at a time, each with a written hypothesis first:
1. Texture test (no code): a local test pack whose `pumpkinblur.png` and
   `spyglass_scope.png` are fully transparent. If the frames vanish, the frames
   sample those textures and a texture-load route (return a transparent
   texture while the switch is on, then reload) becomes the candidate; if not,
   find which texture they use.
2. Search the SDK headers for other typed draw entry points not yet traced
   (dragon frame builder, `mce::MeshHelpers`, render-graph passes) and add
   them to the effects trace's gate/count comparison; never read or guess
   opaque layouts.
3. Record each result in VISUAL-EFFECTS.md and VALIDATION-LOG.md.
Stop and hand back after two runtime rounds without a new candidate.

### L-71 Start an elytra glide from the mod
Kind: Research (cheap models may collect traces). Split from L-70 on
2026-09-30; low priority.
Status: open.
After L-70 puts an elytra on in mid-air, calling `Player::tryStartGliding`
on the swap tick and the next three ticks always returned false (traces at
5728561 and 05648bf), and without a worn elytra vanilla never calls it. Find
what vanilla's own jump-to-glide path checks and sends (input flags, the
start-glide auth input action, equipment sync) and whether the client can
start a glide right after the swap. No faked flags or packets: only a
vanilla path that the server accepts.

### L-93 Schematic: load, place, project, verify and list materials (experimental)
Kind: Follow-up tracking; each L-item below owns its kind and open choices.
The initial scope shipped in 0.1.7; the screen review and 0.1.8 rendering/
compatibility work are complete. Their full record is in BACKLOG-DONE.md
(L-93); [SCHEMATIC.md](SCHEMATIC.md) owns the feature contract and technical
history. Runtime coverage remains in VALIDATION.md.

Open work, to choose with the maintainer rather than implement as one batch:
- Bugs: L-177 stale mismatch highlight (fix built, reporter check pending),
  L-117 Japanese entity-name text, and paused drawing work L-192/L-193/L-194.
- Controls: L-230 view-directed movement (done),
  L-231 control rework (merged in PR #36), L-234 cut-plane layer model
  (merged in PR #37; L-236 follow-up; L-237 done) and L-238 menu/key parity. The design is in
  [SCHEMATIC-CONTROLS.md](SCHEMATIC-CONTROLS.md).
- Inspection and checking: L-183 mismatch suppression, L-184 saved ghost/
  container data, L-190 multi-selection; permission-gated world load L-185.
- Rendering and materials: L-114 Check-preview mistake styling/interaction,
  L-115 richer entity models, L-116 recipe-derived raw materials, and
  L-186 the remaining ghost-legibility/translucency design.
L-178 piston ghosts, L-179 shulker facing and L-182 rejoin-preview crash are
closed; their records are in BACKLOG-DONE.md. Map-layer integration shipped
under L-139; the remaining shared controls are L-111.

### L-30 Ender Dragon multipart hitboxes on Bedrock
Status: research.
The maintainer wants the hitbox overlay to distinguish the dragon's damageable
parts (at minimum head vs body/rest, ideally every real part exposed by the
client) instead of drawing only the dragon's coarse actor AABB.

Java F3+B displays eight damageable sub-hitboxes (head, neck/body, wings and
tail parts). Current public documentation also distinguishes Bedrock head-hit
damage behavior, but that does **not** prove that the Bedrock 26.51.5 client
exposes Java-style part entities or stable per-part AABBs.

- Inspect the current client SDK/symbols and, if needed, a bounded runtime trace
  for dragon-specific part/AABB data used by targeting or damage.
- Prefer the actual client damage/targeting boxes. Do not derive boxes from
  render bones or copy Java dimensions merely to look similar.
- If stable parts are exposed, feed them to the normal hitbox overlay and label
  or color enough to distinguish the head from the other parts. If all eight
  parts are available, render all eight.
- If Bedrock exposes only a coarse box or an opaque internal head test, record
  that limit and leave L-11's ordinary entity hitboxes unchanged.
- Validate against a real Ender Dragon in the End; summoned/custom entities are
  not sufficient evidence for vanilla dragon part behavior.

References for expected Java behavior / Bedrock uncertainty:
https://minecraft.wiki/w/Ender_Dragon
https://minecraft.wiki/w/Tutorial:Hitboxes

### L-33 Mob growth and breeding timers in the target card
Status: research (asked 2026-09-24; not part of L-08).
The maintainer wants the time until a baby mob grows up and the remaining
breeding cooldown. The client SDK has `AgeableComponent::mAge` and
`BreedableComponent::mBreedCooldown`/`mLoveTimer`, but these are behavior
(server-side) components; the client-side actor only receives the synced
`Baby` and `Inlove` flags. Find out:
- whether the client actor carries these components at all (likely not);
- in a local world, whether the in-process server actor with the same
  unique ID can be read safely from the client thread (singleplayer only);
- otherwise show only "baby" / "in love" states, and say so in the help text.
Estimating from observed events (feeding speeds growth up) is not accurate
enough to show as a time.

### L-114 3D previews in the schematic screen
Kind: Research **(strong model)**. Chosen 2026-10-08 (L-93 screen review).
Status: open for the Check tab. The Files preview and the Check tab's
colored preview are built, checked and shipped in 0.1.8. Not done: the Check
tab preview's mistake look and see-through emphasis (decided in the Check
tab review below, then deferred to the 0.1.8 rendering work, which shipped
without them), plus moving the zoomed view and drawing entities, water and
block entities in the previews.
History: Files preview built and checked in game 2026-10-08 (`f2fbf49`);
Check tab colored preview built and checked (`1c0b8bc`). Check tab review
decided 2026-10-08 (SCHEMATIC.md "Check tab review"): layout A, chips drive
the preview, wheel zoom, layers in the preview; the mistake look and
see-through emphasis wait for the 0.1.8 rendering work.
Built and checked 2026-10-09 (`511cb35`): layout A, chips drive the
preview, wheel zoom held at the size that fits the box (the UI scissor
does not clip the mesh), Shift+wheel peels layers from the side chosen when
peeling starts, click to inspect (Files names the block; Check selects its
row or checks the cell on the spot), the Check list keeps the nearest 2000
of each kind. Left for later: moving the zoomed view, entities, water and
block entities in the preview, the mistake look and see-through emphasis. Not drawn yet:
entities, water, block entities (chest, ender chest, shulker box), honey
block and others without an in-world mesh.
How it works (`Preview.cpp`, pure parts in `PreviewView.h`): visible blocks
are tessellated in-world at a spot above the build limit (no real
neighbor or light), moved to their cell, and drawn with the ghosts' block
material into a scissored box. The UI pass keeps the first fragment at a
spot and has no usable depth (giving vertices depth cut blocks apart), so
every quad is sorted near to far for the view's octant; the quads are kept
and only re-sorted when the view turns or recolored when the check
changes. Faces touching a block that covers them (opaque, drawn on this
path) are dropped. Normals all point up and faces are darkened by
direction (lighting through the flattening matrix flashed). Builds 3000
blocks a frame, up to 120000 visible blocks. Drag turns it.
Goal: a rotatable 3D preview of a file in the Files tab and of the selected
placement colored by verifier state in the Check tab (the selected row lit).
Vanilla draws 3D inside UI (the structure block screen), so find that path
first; the ghosts already tessellate schematic blocks into meshes. Stop and
report after two runtime rounds without a working path.

### L-115 Entity ghosts drawn as models
Kind: Research **(strong model)**. Chosen 2026-10-08 (L-93 screen review).
Status: open. A first version is built, checked in game 2026-10-08
(`EntityModels.cpp`, rounds on trace builds up to `ef9bc54`) and shipped in
0.1.8, but it is a compromise (maintainer 2026-10-10): many entities draw
light-blue faces instead of their skin, and details such as cushion
colors are not drawn. Known limits are listed below.
Draw missing schematic entities as their models with the light-blue outline,
not translucent, without a live entity.

How it works:
- Model: `ActorRenderDispatcher::getDataDrivenRenderer(id)` by the entity
  identifier gives the model and default skin, drawn with the renderer's
  `mEntityAlphatestMaterial`. `Model` holds several geometries (adult, baby,
  charged, variants): draw "default", else the first that is not
  baby/charged. Entities without a model keep the dashed frame and the only
  world name tags (logged once per identifier). At most 64 models a frame.
- Placement: `compileCubes` (untransformed, once per part) emits each part's
  quads relative to its bone pivot (`BoneOrientation::mPivot`, absolute) with
  y flipped, cube rotations and inflation applied. Parts are chained in the
  stored y-up space, each turned about its bone pivot; geometry x is mirrored
  in the world, as the game draws it. Rotations in that stored space: x and
  z turn the other way, y as is. Outlines follow the compiled quads.
  `translateTo` and `ModelPart::mRot` are not used (the game writes a live
  entity's pose into the shared model: posing one armor stand moved every
  ghost).
- Pose: bone rest rotations plus the constant parts of the entity's own
  `animation.<name>.*` setup/general animations (an armor stand's
  `default_pose`), rotations and position offsets, only from animations whose
  bones are all in the model (a horse's legacy setup moved the new head).
  Molang is evaluated only when it needs nothing from the entity
  (`RestPose.h`, tested): numbers, `this` as the bone's rest value,
  arithmetic. Legacy `.v1.0` copies are skipped when the current one exists.
  A bone the entity's own animations leave alone takes a rotation that two
  or more other entities' setup/general animations agree on, when all their
  bones are in the model (the witch's crossed arms come from the villager's).
  Borrowing a whole animation family by bone fit was tried and removed: it
  gave traders, strays and polar bears the sheep's head offset.
- Skin: the default skin is one texture of the entity's set; when it is a
  layer (armor, decor, markings, profession, `_none`, baby, saddle, overlay)
  the model draws light-blue faces with the overlay face material
  (`overlay/FaceMaterial.h`, per graphics mode). Horses, donkeys, mules,
  llamas, trader llamas, villagers and rabbits do.
- The saved `Rotation` yaw turns with the placement (`toWorldYaw`, tested).
- `xmake f --schematic_model_trace=y` logs each model's geometries, skin,
  animations, pose and per-part pivots once.

Checked in game: armor stand (default pose, arms on the right sides),
chicken, cow, creeper, witch, wolf, pig, polar bear, turtle, camel, frog,
wandering trader, stray, zombie, drowned, horse family (tinted). Known
limits:
- Poses that need the entity stay at rest: a wolf's tail hangs straight down
  into the body (`query.tail_angle`); zombie-like arms hang down, so a
  drowned's sleeves flicker against its jacket; sitting, walking.
- Layered skins beyond the default: a sheep's wool on head and legs is not
  drawn; variants (cat, rabbit, villager profession) use the default skin.
- An armor stand's own pose (`Pose.PoseIndex` in its saved data) is not
  read. Animations are found by name, so an entity animated under another
  name (donkey: horse) stays unposed.
- Light-blue faces flicker more in Simple graphics (lightning material).
- Many mobs (water, flying, projectiles) were not in the test schematic.

### L-116 Raw materials from the game's recipes
Kind: Research, then Design. Chosen 2026-10-08 (L-93 screen review).
Status: open.
Show the raw materials needed for a schematic's missing blocks (logs for
planks, cobblestone for stone bricks) in the Materials tab, computed from the
recipes the client holds (`Level::getRecipes()`: crafting, furnace,
stonecutter), not from an outside calculator (ResourceCalculator's data is
GPL-3.0; see SCHEMATIC.md). First find what the client actually holds and
how ingredients with several choices (any log, tags) and multi-step chains
(cobblestone, stone, stone bricks) appear; then decide with the maintainer
which recipe to prefer when several make the same item. The calculation is
pure logic with tests.

### L-111 Integration between features: remaining shared controls
Kind: Design. The required 0.2.0 work (L-134/L-135/L-136/L-137/L-138/L-139)
passed the recorded regression checks and two outside reviews, then shipped
on 2026-10-11. Its plan and release/regression checkpoints are retained in
BACKLOG-DONE.md. No 0.2.0 work remains to be released.

Open optional scope (no implementation or release chosen): a common Lamium
radial menu, looked-at selection and settings cross-links (C/D/E in
[INTEGRATION.md](INTEGRATION.md)). A menu needs an agreed demo first; one
common opener versus per-feature openers remains undecided.
Maintainer input, 2026-10-13: make spatial actions usable while working,
without setting up a feature first. Candidate shared operations are a base
point from the looked-at block or feet, two range corners, a Y boundary
adjusted by the wheel, and repeating the last adjustment. L-173 quick shapes
and L-15 restrictions own their behavior; L-230/L-231 first address the
schematic controls themselves.

Keep independent feature lifecycles and shared parts that features register
into; do not merge features. Keep display layers distinct from allowed-action
regions. Check input contexts, binding conflicts and UI weight with the
maintainer. Tidy touched code separately from behavior changes, preserve
per-capability fail-open, and check each step in game before the next.

### L-105 Performance: find the real bottleneck before optimizing
Kind: Research **(strong model)**. Chosen by the maintainer 2026-10-07 from their notes.
Measured 2026-10-14 (VALIDATION-LOG, schematic perf trace): with 3086
ghost sections shown the ghost pass costs 4-6 ms a frame standing still with
nothing rebuilt, mostly per-section draws (faces, lines, block actors,
entities); a candidate is merging sections into fewer meshes.
Status: open. Step 1 only; no optimization is built until it names a
bottleneck.
Why: Hide particles / weather / overlays avoid some drawing, but they are
situational wins, not a way to large frame-time gains. Bedrock is already
native C++ on RenderDragon, so gains that come easily on other platforms may
already be present; measure what is expensive here first. The ideas worth
testing are known in general terms (sources for the hypotheses are in
PROVENANCE.md): rebuild terrain meshes less often or off the critical path,
submit many small draws as fewer batches, skip entities and block entities
that cannot be seen, and cache or make event-driven repeated per-tick work.
Direction: Lamium does not become a renderer replacement. Prefer bounded,
measurable substitutions that skip, cache, batch or defer one expensive
vanilla path while keeping vanilla behavior, capability-gated and fail open
like every version-sensitive path.
Steps:
1. Profiler spike (a build option, like the trace options, never on in a
   release): measure per frame, in heavy real scenes (a village, a large
   farm, a storage room, flying fast over new terrain), frame time and its
   1 % lows, and as far as hooks allow: terrain/chunk drawing, chunk mesh
   rebuild count and time (and on which thread), entity and block-entity
   drawing, particles, HUD/UI, mesh draw-call counts. Record results in
   VALIDATION-LOG.md.
2. Pick at most one or two dominant costs and write a bounded candidate per
   cost (what is skipped, cached, batched or deferred; how vanilla behavior
   is kept; how it fails open). Each becomes its own L-item with the
   maintainer.
Areas to look at first:
- Chunk mesh rebuild scheduling: what triggers rebuilds, how much runs on the
  render or main thread, whether some can be deferred without gameplay
  change. A likely cause of stutter while turning or moving.
- Draw submission: whether meshes, entities, block entities, particles or
  UI still issue many small draws or state changes.
- Visibility: whether entities and block entities behind walls, underground
  or in other rooms still cost a lot. Conservative culling could matter in
  farms, villages and storage.
- Repeated simulation work (lookups, per-tick scans, collision checks,
  short-lived allocations), only after the profile points there.
- Particles: Hide particles skips drawing, not necessarily their creation
  and updates. A "skip particle processing" path may help particle-heavy
  scenes; it is not the main route.
Success: better frame time and 1 % lows in heavy scenes, not a higher
average in an empty world. One proven large bottleneck beats many guessed
micro-optimizations.

---

## Later / parked

The following notes were imported on 2026-10-12 for later consideration.
Documentation tracking does not approve implementation or a release target.
L-154 was merged into L-169 and is archived in BACKLOG-DONE.md.
Fake Offhand reserved-slot protection is excluded from this import; the
undecided historical idea remains in L-94's record.

### L-151 Show the player's body in first person
Kind: Research, with Design before implementation. Low priority.
Goal: optionally show the body when looking down; proposed default off.
Investigate whether the detached camera's player-model component can be
applied locally without a deeper renderer change. Check head/face clipping,
duplicate vanilla arms/items, armor and elytra animations, perspective and
FreeCamera transitions, and restoration. L-123 is a related unresolved
rendering risk. Defer if many special cases or deep hooks are needed.
No implementation or runtime evidence; not required for integration work.

### L-152 Preview held map items in inventory
Kind: Design, then Research for client-observed map data and rendering.
Goal: make a map item easier to inspect from inventory without equipping it.
Open: hover preview versus explicit action, preview size and zoom, empty or
unreceived map data, and supported screens. Keep this distinct from the
world map; never infer unexplored contents. No implementation commitment.

### L-153 Prefer the last pressed opposing movement key
Kind: Design, then Research for the native input boundary.
Goal: optionally resolve simultaneous W/S or A/D input to the last pressed
direction. Open: default, release/resume semantics and interaction with
remapped keys, controllers, permanent movement and detached cameras.
Model event sequences in pure logic before touching gameplay input; menus
and focus changes must not leave stale held inputs. Not implemented.

### Previously parked work

- L-19 Freelook in multiplayer, riding, dimension change, controller: runtime
  checks only, no code expected.
- L-25 Detached-camera interaction options (parked, after L-18; agreed
  2026-09-24, implement only once FreeCamera proves viable). While detached,
  attack/use are fully blocked and clicks still swing the arm. Desired shape:
  per-feature detail settings, FreeCamera x {attack, use/place/interact,
  break} and Freelook x {attack, use/place/interact, break} (6 toggles,
  all default off). Aim follows the body, never the detached view; state
  this in the help text. Movement freeze (FreeCamera) and movement keep
  (Freelook) stay non-optional. Swing suppression (no arm swing while
  detached) is a separate Research item: find the swing trigger first.
- L-29 Hide the hotbar while detached (parked, after L-18). Requested
  2026-09-24: an option to hide the hotbar while FreeCamera is active
  (looking-only flight needs no hotbar). Find the vanilla hotbar render entry
  first; Freelook is out of scope unless trivially shared.
- L-86 Radar faces follow-up (after L-85; noted 2026-10-02): mobs without
  a "head" part (silverfish, tadpole) could use the whole model seen from
  the front; rotated head bones (camel, hoglin) need the bone rotation in
  the front view; check the remaining mob kinds (only about 25 of 80+ were
  seen); decide whether faces become the default once they hold up.
- L-21 Shape color picker or more colors: only if the four colors prove
  insufficient.
- External PR #10 (Spanish localization and map daylight tint, opened
  2026-10-07): the maintainer asked on 2026-10-08 for one PR per feature,
  translations updated to the latest main, Night Vision and unaffected
  markers for the map tint, and the provenance statement. Nothing is
  discussed further until it is split.
- Mass Craft became L-165 (2026-10-12). (Schematic became L-93; Fast
  Attack/Use became L-34; Scroll Transfer became L-41.)
