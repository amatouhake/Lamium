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
  the README feature list and Known issues. A full regression is not required.
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

Keep this section short. It is only the ordering layer; task details and status
live in the L-items below. If this summary ever disagrees with an L-item, the
L-item wins. Every entry names what the task is, not only its number.

0. **0.2.3 released** on 2026-10-15 (`v0.2.3`, from `88462d5`). Next work
   follows the items below.
1. **Small and medium features and fixes** (maintainer 2026-10-10: these
   come before large features and L-111 integration for now):
   - Bugs: L-177, L-181 (player reports, need reproductions or an agreed
     rule); L-192, L-193, L-194 schematic drawing (maintainer 2026-10-15:
     schematic drawing work pauses here; far-away cases remain); L-123 elytra stretched on the inventory model after FreeCamera and
     L-91 shield glint in Lamium's icons (both parked); L-132 moving with
     the inventory screen open (paused after eight rounds); L-205 world map
     carpet colors and L-207 FreeCamera frame rate (2026-10-17 player
     reports, need reproductions).
   - L-196 license/provenance follow-up: verify notices and distribution
     evidence, then make the supported documentation/packaging changes.
   - Specs to agree with the maintainer before building: L-165 Mass Craft,
     L-167 in-world language (chosen: native-path research in progress), L-168 trade
     finder, L-169's later parts (drops and the rest), L-170
     enchantment order planner, L-173 quick shapes and beacon range, L-163
     a Lamium-drawn inventory screen. L-151 to L-154
     stay later. This list approves no implementation and sets no release.
   - Done since 0.2.0 (BACKLOG-DONE): L-155, L-157 to L-161, L-147, L-148,
     L-150, L-164 with L-133 and L-156, L-171 calculator, L-172 text
     fields, L-166 repeated villager trades (part 1), L-174 chat position,
     L-176, L-179, L-187, L-188, L-180, L-182, L-178, L-189.
2. **Schematic — L-93 follow-ups:** the screen review and the 0.1.8
   rendering work shipped in 0.1.8. Open: L-114 the Check tab preview's
   mistake look and see-through emphasis, L-115 entity models beyond the
   light-blue compromise (real skins, details such as cushion colors),
   L-116 raw materials from the game's recipes, L-117 Japanese name tags.
   Choose with the maintainer; SCHEMATIC.md retains the contract and build
   record. Server/broader coverage remains open.
3. **Placement and breaking — L-15 restrictions and L-59 held placement
   style:** L-15 breaking shipped in 0.1.8; the maintainer will redesign
   it, and placement waits for that Design. L-59 waits for the maintainer's
   go.
4. **0.2.0: clean-up and integration (L-111 integration between
   features, decided 2026-10-11):** the required items are done
   (BACKLOG-DONE, 2026-10-11): L-136 GhostRenderer.cpp split, L-138 shared
   face drawing, the map and settings tidy-up, L-139 map layers, L-134
   SettingsScreen.cpp split and L-137 game update readiness, after both
   regression checks and two outside reviews. Released as 0.2.0 on
   2026-10-11 (L-135 PDB as a separate asset done with it). Maintainer
   2026-10-11: no 0.2.0 tag for a while; 0.2.0 also gets a review of the
   features from the player's side, integration and links between
   features. Done and checked 2026-10-11 (BACKLOG-DONE): L-140 Zoom
   Wide-angle view, world map names on hover (L-139 follow-up), L-141
   overlapping ghost flicker, the 0.1.9 Discord reports L-142 glint,
   L-143 Debug View and vanilla HUD, L-144 hotbar in the HUD editor, L-145
   Info HUD over the paper doll and position line.
   Maintainer 2026-10-11: clean-up items move to BACKLOG-DONE only after a
   code review finds no problems, not on the in-game check alone. The
   radial menu and the smaller links are optional. Order and boundary:
   L-111.
5. **Research when convenient:** L-79 carved pumpkin and spyglass frame draw
   path (cheap-model friendly
   trace/test steps), L-71 starting a glide from the mod, L-30 Ender Dragon
   part hitboxes, L-33 mob growth and
   breeding timers, L-105 performance profiling (measure before any
   optimization).
6. **Before a release:** the pre-release checks below. 0.2.1 was released
   on 2026-10-14 (0.2.0 on 2026-10-11), 0.2.2 and 0.2.3 on 2026-10-15. No
   date for the next one.
   Server checks stay listed below as known gaps (the Release
   policy does not require a full regression).

The 2026-10-14 player-feedback triage is recorded as L-177 through L-189:
reported problems are unverified and proposed behavior is still Design/Research.
These entries do not select an implementation, priority or release.

The 2026-10-17 player-feedback triage is recorded as L-205 through L-208:
reported problems are unverified and proposed behavior is still Design/Research.
These entries do not select an implementation, priority or release.

Ideas that are not yet chosen (for example a fall-rescue elytra) stay in
the maintainer's notes (the Notion idea inbox) and enter
this file once chosen. The candidates imported on 2026-10-12 are now
tracked here for design/research; their open choices remain open.
Mass Craft (L-165) ranks below Map because resource packs already cover part of it.

Task-picking rule: bugs first; otherwise work on what the execution order
names. Cheap models skip strong-model, Design and Research work. Follow the
L-item's dependencies and model/validation requirements. When a task is done,
update its status and relevant feature doc, then move it to BACKLOG-DONE.md;
do not duplicate task details into this summary.

---

## Pre-release checks

Remaining coverage to consider for the next changed-feature smoke test. These
include trace-only and local-only results; VALIDATION.md owns the per-feature
gaps. The Release policy above does not require a full regression or closing
every server gap before a release.

### Since 0.2.3 (unreleased)

Nothing yet.

### Shipped in 0.2.3 (smoke-tested; remaining gaps in VALIDATION.md)

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

### Shipped in 0.2.2 (smoke-tested; remaining gaps in VALIDATION.md)

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

### Shipped in 0.2.1 (smoke-tested; remaining gaps in VALIDATION.md)

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

### L-162 Edge Guard follow-up coverage (2026-10-13)

- Longer actual play and other servers; slippery blocks remain a known limit.
- The final run has twelve corrections, nine guarded replays and no fall
  dump. It does not guarantee protection if a server correction already puts
  the feet off support; incoming corrections are not clamped. The earlier
  +0.178 slab-bridging correction was not repeated at its identical location.
  If a fall recurs, preserve its environment/build/log before reopening work.

### L-166 repeated trading follow-up coverage (2026-10-13)

- Chosen part 1 is complete and checked locally and on the normal BDS build;
  include it in the next changed-feature release smoke. No release selected.
- Controlled response delays pause MC_SERVER with communication running.
  Real network RTT/loss and other servers are untested. Normal logs do not
  isolate the exact timeout phase or dimension cancellation from an earlier
  stop; forced world exit with a pending response remains unchecked.
- Discount/demand price changes and longer actual play remain coverage limits.
  Favourites and remaining-use UI are separate undecided scope below.

### L-147 and L-150 (2026-10-12, local only)

- Status effects HUD: beacon effects (no warning color), a server's
  effect updates (does the time count down between updates), Pocket UI,
  the layout editor sample, F1 and the inventory screen's own effect list.
- Durability HUD ammo: counts on a server after shooting and picking up.

### Released builds and registry follow-up

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

### 0.2.0 regression checks

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

### Pending feature checks

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

## Bugs


### L-205 Carpets do not draw with their colors on the world map
Kind: Bug / Research. User report 2026-10-17.
Status: unverified; no reproduction yet.
A build made of wool carpet appears on the world map without the colors the
carpets should draw (screenshots in the report). Reproduce locally with a
small patch of each carpet color; if it does not reproduce, ask the reporter
to keep the world. Check where the color is lost (recording or drawing) and
which carpets are affected. [MAP.md](MAP.md)

### L-207 FreeCamera halves the frame rate
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

### L-181 FreeCamera trident interaction affects the player unexpectedly
Kind: Bug / Research, low priority (maintainer 2026-10-15: drawing only,
harmless). User report 2026-10-14; not yet reproduced.
Clarified 2026-10-14: nothing hits the player. In FreeCamera the trident in
the player's hand is drawn the wrong way round, so it looks stuck in the
player. A drawing problem like L-123 (stale pose state while the body is
out of the camera's normal view); check whether the use/charge pose or the
held-item transform is the stale part.
Status: open. Using FreeCamera with a trident held was reported to leave the
player struck by the trident. Capture the exact sequence (enter camera,
held/used/thrown trident, Riptide or ordinary throw, local/server) and
distinguish blocked input, borrowed camera position and server-authoritative
projectile behavior. In FreeCamera, player interaction suppression and
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

### L-206 World map recording range is a fixed radius instead of the view distance
Kind: Ready. User report 2026-10-17; maintainer 2026-10-17: fix it, then
approved PR #13's range rule and merging after review fixes and main sync.
Implemented: `record()` follows the client's view distance, retaining the
384-block width as a floor and the reported maximum (at most 96 chunks) as
a ceiling. No new settings rows. Display retention and time budgets stay
bounded; a persistent sweep reaches the far rings despite cache eviction,
alternating with nearby refreshes so local edits, movement and cave layers
do not wait for the wide pass. The scheduler is pure and tested, including
one-chunk budgets and resource waits. Details: MAP.md, L-206 current notes.
Remaining: in-game recording coverage and frame times at 32/64/96 chunks,
with the minimap off as well as on; near changes and movement during a wide
pass. Build/tests alone do not establish these runtime results.

### L-208 Connected Textures: an option to keep an edge where glass does not continue straight
Kind: Design. User suggestion 2026-10-17; maintainer 2026-10-17: try the display option.
Goal: an option to draw an edge/border where glass does not continue
straight (at corners), for glass blocks and glass panes.
Current: a face drops its border on the sides where the neighbor in the
face's plane is the same block; a pane's L-corner post merges completely
(screenshots in the report).
Status: built 2026-10-18. `visuals.connectedCorners` ("Corner edges",
default on, under the experimental switch) keeps the border at a glass
block's two-side corner (a corner-variant sprite with the source texels
kept where the joined strips meet) and makes an L-shaped pane's corner
post reach the tile border at its two outside faces. Straight and fully
surrounded poses are unchanged.
Checked in game 2026-10-18 (local world): with the option on the border
bridges the turn and with it off the lines stop short at the same corner;
a pixel diff between the two states changes only at the corner (43 px,
peak 97) while straight runs and controls show none.
Open: corner poses of three- and four-side joins; the generated-sprite
cost of corner variants.

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
MAP.md); not checked in game. (b) open: the 30 s diagnostics line now counts
the recording range as unseen / missing / partial / complete. Check whether
partial edge chunks complete while standing still before deciding anything;
forcing sections to load costs frame rate and is not a fix (maintainer).
Check in game: fly a straight line at 32+ chunks view distance, stop, open
the world map: no arcs or wedges of black inside the range; the partial
count in the log drops while standing; frame time unchanged.

### L-196 Review license notices, source access and provenance classification
Kind: Research, then Ready for changes supported by the findings.
Imported at the maintainer's request on 2026-10-15 from the current-tree
follow-up in the [Notion license/provenance review](https://app.notion.com/p/3e7c12ab5b2d81f09eb2d7bf61cd81a2).
Status: accepted for investigation; no audit or implementation completed by
this import, no release chosen. The source review covered public main /
v0.2.1; recheck the current tree and distributed version when doing the work.
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

### L-191 Hide the hand and HUD in FreeCamera, and what can be operated there
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
Kind: Design / Research for saved data inspection; the target-card part is
built and awaits in-game checks.
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

### L-169 Item Transfer in more screens, and bulk transfer
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
Kind: Research, then Design (strong model). Imported from the maintainer's
notes (2026-09-25) on 2026-10-12. Chosen 2026-10-13: language first;
native-path research in progress, no release selected.
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

### L-111 Integration between features (0.2.0)
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

### L-105 Performance: find the real bottleneck before optimizing
Kind: Research **(strong model)**. Chosen by the maintainer 2026-10-07 from
Measured 2026-10-14 (VALIDATION-LOG, schematic perf trace): with 3086
ghost sections shown the ghost pass costs 4-6 ms a frame standing still with
nothing rebuilt, mostly per-section draws (faces, lines, block actors,
entities); a candidate is merging sections into fewer meshes.
their notes.
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

### L-154 Additional inventory transfer gestures
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
