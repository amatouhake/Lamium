# Archived release and validation checkpoints

0.4.0 released 2026-11-01 (`v0.4.0` on `029638a5`, built and attached by the tag's run 41314557233, asset SHA-256 `36930FBFB2AE51196D1B79843F6AFB37413988CC8F9686C3E3EB3199617EC00A`, 28 entries, `manifest.json` 0.4.0).
Its user-facing changes are the release notes. Smoke-tested with the commit's CI package; checks still owed are in
BACKLOG "Since 0.4.0" (LeviLauncher install of the published ZIP, listing text). No server checks were made for this release.

0.3.1 released 2026-10-27 (`v0.3.1`, asset SHA-256 `D2BA64271645CA4910123C4C8655441ADFCEFABF8AE5DF3D412C257F3E1C00F7`, 28 entries, `manifest.json` 0.3.1).
Its user-facing changes are the release notes; the checks still owed are L-274's refused-pick line (a
server that refuses the give or the move) and L-276's save-failure path (read-only `waypoints.json`).
No server checks were made for this release.

## 0.3.0 pre-release checks — retained 2026-10-26

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

## Notion reconciliation — 2026-10-20

Moved from BACKLOG's execution order on 2026-10-26.

Notion reconciliation, 2026-10-20: four current schematic notes from the
[idea inbox](https://app.notion.com/p/3f7c12ab5b2d8121b2a0db32608f6b7a)
are tracked as L-229/L-230/L-231 (the two controls-UX notes share L-231).
The item/edit-mode proposal still waits for feedback; unchosen ideas such
as fall-rescue elytra, effect-only particles, rain sound, distortion
presets, reserved Fake Offhand slots and advanced subtitles stay in Notion.
This reconciliation selects no new implementation or release.

## BACKLOG pre-release snapshot — 2026-10-20

Retained from BACKLOG.md during reconciliation, with headings nested under
this snapshot. The dated pending/next-release wording below is historical,
not an unmet gate for a published version. Current release policy and
remaining checks are in BACKLOG.md; current runtime coverage is in
VALIDATION.md. No new game result is established by this move.

### Pre-release checks

Remaining coverage to consider for the next changed-feature smoke test. These
include trace-only and local-only results; VALIDATION.md owns the per-feature
gaps. The Release policy above does not require a full regression or closing
every server gap before a release.

#### Since 0.2.3 (unreleased)

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

#### Shipped in 0.2.3 (smoke-tested; remaining gaps in VALIDATION.md)

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

#### Shipped in 0.2.2 (smoke-tested; remaining gaps in VALIDATION.md)

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

#### Shipped in 0.2.1 (smoke-tested; remaining gaps in VALIDATION.md)

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

#### L-162 Edge Guard follow-up coverage (2026-10-13)

- Longer actual play and other servers; slippery blocks remain a known limit.
- The final run has twelve corrections, nine guarded replays and no fall
  dump. It does not guarantee protection if a server correction already puts
  the feet off support; incoming corrections are not clamped. The earlier
  +0.178 slab-bridging correction was not repeated at its identical location.
  If a fall recurs, preserve its environment/build/log before reopening work.

#### L-166 repeated trading follow-up coverage (2026-10-13)

- Chosen part 1 is complete and checked locally and on the normal BDS build;
  include it in the next changed-feature release smoke. No release selected.
- Controlled response delays pause MC_SERVER with communication running.
  Real network RTT/loss and other servers are untested. Normal logs do not
  isolate the exact timeout phase or dimension cancellation from an earlier
  stop; forced world exit with a pending response remains unchecked.
- Discount/demand price changes and longer actual play remain coverage limits.
  Favourites and remaining-use UI are separate undecided scope below.

#### L-147 and L-150 (2026-10-12, local only)

- Status effects HUD: beacon effects (no warning color), a server's
  effect updates (does the time count down between updates), Pocket UI,
  the layout editor sample, F1 and the inventory screen's own effect list.
- Durability HUD ammo: counts on a server after shooting and picking up.

#### Released builds and registry follow-up

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

#### 0.2.0 regression checks

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

#### Pending feature checks

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
