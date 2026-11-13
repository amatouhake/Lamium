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
  A full regression is not required. From 0.4.0 the smoke test uses the
  commit's CI package and the tag's CI run attaches the assets to a draft
  release ([DISTRIBUTION.md](DISTRIBUTION.md#release-flow-l-298-from-032)).
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
- The Experimental badge comes off a feature when all three hold (decided
  2026-11-13): it has been in two or more releases, no bug item is open
  against it, and no change to how it behaves is planned. The maintainer
  decides per feature; meeting the three is what makes it a candidate.
- Large features may start at any time. They land on main in steps, default
  off and with the Experimental badge, so main stays releasable while they
  grow. A step that is not usable yet stays out of the settings screen (or
  behind a build option) instead of being shown half-working.

## Current execution order

Keep this section short: ordering lives here; status, contracts and open
choices live in the L-items. An L-item wins if a summary drifts.

1. **Bugs first.** Open: L-331 (Connected Textures pane edges; Research), L-324 (custom blocks brown on the map;
   Research), L-227 (FreeCamera shows only the sky outside loaded
   terrain; Research). What waits for more game symbols is collected in L-315.
   Parked: L-194 save corner marks, L-123 elytra pose, L-91 shield glint,
   L-132 inventory-screen movement, L-244 FreeCamera started inside a block,
   L-287 minimap arrow look; respect each item's stop/resume conditions. L-181
   trident pose is low priority.

2. **Small and medium work before large features/integration** (maintainer
   2026-10-10): the L-266 schematic follow-up batch closed with 0.3.1 except
   L-265 placement assist (Design). Remaining Design/Research: L-166 trade
   helpers, L-167 resource packs, L-168 trade finder, L-169 transfer extensions,
   L-170 enchantment planner, L-173 quick shapes, L-190 list selection, L-163 a
   Lamium inventory screen, L-249 simulation distance and the 2026-11-07 reports L-317 (Nether
   minimap under forced Surface) and L-319 (item details order). In progress:
   L-320 block highlight (built and checked in steps; its open runtime checks
   are in VALIDATION.md). Agree open choices before
   building. L-165 Mass Craft remains below Map and needs a scope conversation;
   existing packs already cover part of that need.
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
   Pre-release checks below. Latest release: 0.6.0, 2026-11-12 (`v0.6.0`). A full
   regression is not a release gate.

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

### Since 0.6.0

- L-327: the "restore ended" toast was confirmed on `786424c4`; the help
  text naming 32 blocks was not looked at.

### Left from 0.6.0

The tag (VALIDATION-LOG 2026-11-12; release published the same day): the published ZIP, copied by hand,
loads and passed a short smoke; Bedrinth and LeviLauncher list 0.6.0
(VALIDATION-LOG 2026-11-12). An install through LeviLauncher or LIP is not checked.

Passed on `bb1cc4be` (VALIDATION-LOG): load, settings, Block highlight (L-320),
the HUD layout editor (L-321), Tool Switch's drop priority (L-318), a clean
exit on LeviLamina 26.60.4. Not checked: a settings file from 0.5.1 with only
Chat position on, which the release notes describe from the code; a label
with a zero-wide character aligned right or centered. Other open gaps are in
VALIDATION.md and the table below.

### Left from 0.5.1

The tag (VALIDATION-LOG 2026-11-07): the published ZIP loads through LeviLauncher, and
the LIP registry, Bedrinth and LeviLauncher offer it. The smoke used the commit's CI
package, not this ZIP.

Passed on `406707d4` (VALIDATION-LOG): L-308, L-311 and L-313's map key, Record switch
and settings row. Their server cases are not covered.

From 2026-11-06 (VALIDATION-LOG):

- Book paste (L-259) and the minimum LeviLamina 26.60.3 shipped in 0.5.1. Worth a look:
  a server, a replaced selection, the 50-page refusal's toast.
- Target distance (L-308): frame times are not measured. The worst case is FreeCamera
  at the full range, looking where nothing is hit.
- L-307's bad-zlib and size-limit paths through the
  real inflater stay unchecked.

Left from the 0.5.0 tag (L-305, VALIDATION-LOG 2026-11-04):

- The published ZIP loads through LeviLauncher on a fresh instance with LeviLamina
  installed first, and LIP offers it there (`tooth.json`'s range changed).

Carried from 0.4.0:

- Schematic check of block entity data (L-303): a smoke of the release
  build (a changed sign or note block is a wrong state with a yellow mark;
  "Ignore block entities" clears it). Unchecked so far: "Ignore sign text"
  on its own; banners, beds and pot sherds changed by hand; a lectern page
  turned by another player; scan time with many block entities; servers.

Left from the 0.4.0 tag (VALIDATION-LOG 2026-11-01):

- Listings (L-196): the Bedrinth and LeviLauncher pages show `tooth.json`'s
  description with both the LeviMC and the Minecraft notices.

### Gaps carried from released builds

These are follow-up coverage, not unfinished implementations or release
gates for versions already published. Full old release/smoke/regression
checkpoints are retained in BACKLOG-DONE.md, under "Archived release and
validation checkpoints" (snapshot 2026-10-20).

| Area | Remaining targeted checks |
|---|---|
| Maps, L-205/L-206/L-210/L-211/L-215/L-225/L-226/L-228 | Frame times with recording off/on at 32/64/96 chunks and minimap off/on; nearby edits and movement during a wide pass; cave/Nether frame times and per-band coverage if the footprint difference recurs; broader servers. Only client-received terrain can be recorded. |
| LMR2 regions, L-216 | Older-build fill-only merges, refusal without overwrite, damaged-file recovery, 10 s failed-save retry, measured frame times, broader servers; a focused live check of the review follow-ups (save batching, legacy warnings, cave provisional tint). |
| Map storage on servers, L-241 (c) | A server without map storage keeps its map while moving far and back, and the temporary folder is gone after leaving. |
| FreeCamera, L-207/L-213/L-214 | Frame times per underground-hold choice, servers, broader graphics/resource/lifecycle cases. The loaded-area sky bug is L-227. |
| Connected Textures corners, L-208 | Resource packs, live resource reload, servers. |
| Target/HUD/settings, L-217 to L-224 | Remaining language, size, input and server gaps in VALIDATION.md. |
| Item Transfer, L-169 | Held-stack/drop gestures passed the local chest checklist; try inventory and slot-role screens, servers and a rebound drop key. Full gesture/screen matrix, cursor/hotbar/keep-one/re-entry/saved-switch cases, controlled rejection/timeouts, world/dimension interruption, unverified-game fallback and packs remain. See ITEM-TRANSFER.md. |
| Shared capability migration, L-198 to L-202 | Server/server-pack smoke. A separately requested forced-unverified build should check vanilla fallback and matching UI/notices while independent paths work. Injected hook/cleanup failures and partial-loss/restart UI remain unverified in game. |
| Settings/help and map layers, L-197/L-146 | Broader sizes, languages and editing states; narrow-window two-row map bar. Use current DESIGN and VALIDATION, not superseded footer-expansion checkpoints. |
| HUD layout editor rework, L-321 | Each element at 50 % and 200 % (text, icons, minimap), the toolbar's Reset and shadow button, saved 75-150 % layouts, Pocket UI, other GUI scales, text fit in other languages. |
| Game HUD placement as one feature, L-321 | A settings file with only Chat position on or off, fresh settings, the keys and their toasts, paused notices, other languages. |
| Scoreboard position and scale, chat scale, L-321 | Pocket UI, other GUI scales, long lists, servers and their custom sidebars; chat at 200 %, fading and many messages at a scale. |
| Paper doll position, L-309 | Pocket UI (the game's place there is 15, 25, Lamium's default the desktop 15, 15), other GUI scales, servers, the doll hidden under the Info HUD at its placed box. Known limit: overlapping another element it may not be shown or is not drawn behind it. |
| Chat position, L-174/L-203 | Fresh settings, GUI scales/Pocket UI, packs, world/dimension/focus and servers beyond the recorded local checks. See CHAT-POSITION.md. |
| Edge Guard, L-162 | Longer play, other servers, slippery blocks; correction that already puts feet off support remains a limit. Preserve environment/build/log if a fall recurs. |
| Repeated trading, L-166 part 1 | Real RTT/loss and other servers, price changes, forced exit with a pending response, timeout/dimension cancellation isolated from earlier stops; controlled MC_SERVER delays do not establish network behavior. |
| Effects/ammo HUD, L-147/L-150 | Beacon effects, server updates/counts, Pocket UI, layout sample, F1/inventory interplay. |
| In-world language/server packs, L-167 | English Search and schematic ghosts after a change, broader servers/packs, rapid switching/hang risk and the 10 s interval. Cosmetic BDS evidence does not establish all pack types. See LANGUAGE.md. |
| Camera and visual effects | FreeCamera world-exit/death/controller/body-view and hold-input cases, speed restart persistence; Hide effects restart/keys/master, ambient layers, packs, graphics modes and lifecycle. Detailed coverage is in VALIDATION.md, CAMERA.md and VISUAL-EFFECTS.md. |
| Schematic ghost pick in creative, L-263 | A block not carried is given into the hotbar (empty selected slot, another empty slot, full hotbar), unlisted items (water, lava, structure block) do nothing, carried and survival picks unchanged; also on a server. |
| Distribution listings | 0.1.4–0.1.8 registry pickup is recorded; LeviLauncher 0.2.0 was seen; 0.5.1 is offered by the LIP registry, Bedrinth and LeviLauncher (VALIDATION-LOG 2026-11-07). Verify the version being released using DISTRIBUTION.md rather than reopening old release gates. |

---

## Bugs

### L-331 Connected Textures: glass pane edges are uneven in less simple arrangements
Kind: Research. Maintainer report 2026-11-13 (`786424c4`, with a
screenshot kept outside the repository).
With "Edges at glass pane corners" on and the inside edges ("Edges at
inward corners") off, panes laid out in more than a flat run - an L with
further panes above and beside it, panes meeting at different heights -
show a border on some sides and none on others that look alike. Flat rows
and simple walls look right.
To find out first: which neighbor arrangement each wrong side has (the
target card's `minecraft:connection_*` states help), and whether the rule
that decides an edge looks only at the pane's own connections or also at
the panes above and below. Then the rule for an outside corner is stated
again in Lamium's terms before any code changes.
Connected Textures has its Experimental badge back while this is open.

### L-324 Map: custom blocks are drawn in a dirt-like brown
Kind: Research. Issue #76 (2026-11-11), accepted by the maintainer
2026-11-13.
On the minimap and world map, add-on blocks show brown instead of their
own color, while the game's map item shows the color of their
`minecraft:map_color` component (reported on 0.6.0, game 26.60, with
steps: an add-on with such blocks, a filled area, compare with a map item).
How Lamium colors a block (`Minimap.cpp`, `blockLook` / `blockColor`): the
average of the top texture from `BlockGraphics`, times the biome tint; the
game's `getMapColor` is used only when no texture color was found.
Hypothesis, not checked: for add-on blocks `BlockGraphics` hands back a
default texture, so a color is found and it is the wrong one. First step:
reproduce with an add-on and read which texture path the block resolves to
(the map's diagnostics log the first texture loads). Then either fall back
to `getMapColor` for blocks whose texture is that default, or find the
block's real top texture; the maintainer chooses once the cause is known.

### L-299 Screenless inventory moves can leave a slot stuck when client and server desync
Kind: Research.
Origin: a Discord report (2026-10-31, reporter not named) said Hand
Restock "locks" the hotbar after placing a lava bucket. The
maintainer could not reproduce it in single-player or on a BDS
(1.26.51) with matched settings, and closed the thread on
2026-11-01 as not reproducible: the stated conditions kept changing
and the requested log was never sent. Reopening was gated on exact
steps that anyone can reproduce using publicly available releases
only, with that session's log and a short video.

Why this is tracked anyway, and not as a Hand Restock bug: another
mod developer noted on Discord (2026-11-02) that this class of
item lock-up is generally a client/server desync. It can happen in
a local world too, because a local world still runs on an
integrated server; it showed up often in the redstone era before
native client mods, always when acting too fast, and item pickup
and consumption gave the same trouble in recent mod work. That matches Lamium's own records for the shared
screenless-move path, which Hand Restock, Transfer, Sort and the
equipment swaps all use (`game::beginTransfer`, `movePair`):

- HAND-RESTOCK.md ("Ordering the move"): the server runs a legacy
  use in its next tick but applies an inventory transaction on
  receipt, so a move sent right after a use reached the server first
  and was rejected on BDS (`a41f0f2`). The move therefore waits for
  a server update showing the consumption; a local prediction or a
  sent transaction is not server confirmation.
- DESIGN.md (bulk moves, L-169): a traced resource-pack bulk move
  sent up to 44 requests in one frame (2026-10-15); Lamium waits
  for all responses and checks touched slots against the
  authoritative state. A rejected or timed-out request, an
  unexpected prediction or a changed source stops the operation and
  drops the queue; vanilla rolls back its rejected predictions.
- The reporter's own session showed the shared layer's log line
  "Inventory transfer stopped: response 2" — `TransferSession.cpp`
  stopping with "request not accepted" when the result is not
  Accepted. The log's source is the shared transfer session, not
  Hand Restock itself.
- README's known limits already say rejected moves on laggy servers
  are untested for Inventory transfer, and that server timing and
  very fast use may make Hand Restock skip a refill.
- Maintainer's own experience while building the shared
  screenless-move foundation under Hand Restock (recalled
  2026-11-02): a desync lock of this shape did occur, and leaving
  and rejoining the world cleared it. Whether anything short of a
  relog also clears it was not established.

Direction (maintainer, 2026-11-02): the desync predates native
client mods — fast vanilla play could already produce it — so
suppressing occurrence is not the promising line; Lamium's own
moves can at most avoid adding to it. The promising line is
repair after the fact. Deliberately re-trigger the desync seen
while building the shared foundation, observe exactly which state
is stale, and catalogue recovery steps from lightest to heaviest.
If a safe in-session resync exists, shipping it as a recovery
feature is on the table. Any such feature may only restore from
authoritative server state — never synthesize, rewrite or forge
items — and must fail open to vanilla.

Research questions, in order:
1. When a transfer is not accepted, or a server correction lands in
   the middle of an operation, can any Lamium state outlive the
   failure — a pending operation, a transfer token, a predicted
   snapshot — and make a slot behave as locked afterwards? The
   decided rules say operations stop and queues drop; verify the
   failure paths actually release everything.
2. Does vanilla alone, with no Lamium, leave the same stuck slot
   under fast repeated use/pickup/consumption? The comparison is
   for learning the shape of the residue, not for assigning
   blame: occurrence in vanilla is expected, and it is what makes
   the recovery line the product of this item.
3. Which recovery clears a stuck slot, and what is the lightest
   reliable one? A relog is already known to clear it (the
   maintainer's experience above), so in that case the
   authoritative state was intact and the residue lived in
   session state. Test the lighter steps — reopening the
   inventory, interacting with the slot, changing dimension —
   and record which ones work; each working step narrows where
   the residue sits and is a candidate for what a recovery
   feature would do. This is why the original thread asked it
   first.
4. Can a stuck slot be detected from client-visible state, and can
   the client obtain a fresh authoritative inventory snapshot
   in-session — the thing a relog does — without leaving the
   world? These two answers decide whether a recovery feature is
   feasible at all, and whether it would be automatic or a
   manual action.

Gate: no fix work from the original report; it does not meet the
2026-11-01 reopening condition. The research above starts from
the maintainer's own deliberate re-triggering, or from a public
reproduction, or from a session log that pairs the "Inventory
transfer stopped" line with the state of the stuck slot in the
same session.

Check if a reproduction is found: the stuck slot's contents before
and after each recovery step; whether another Lamium operation owns
the transfer tracker at that moment; the same fast-use sequence
with each screenless-move feature on and off in turn; local world
and BDS.
No release target selected.

Code read for question 1, 2026-11-12 (reading only, nothing run or
changed; `8cdfe543`):
- The transfer tracker (`RequestTracker.cpp`) has one owner at a time
  and is released only by `cancelTransfer`. Every owner releases it
  on each path read: Hand Restock (`cancel`, `failure`, the 1 s
  observation deadline, context change in `tick`, focus loss), Sort
  and the screen transfer session (rejected, untracked or timed-out
  result; `ScreenTracker` cancels both on screen close, change, focus
  loss and world exit), Repeat trades (`clear` on a failed reply, a
  changed job and an exception). A response that never arrives ends
  as `TimedOut` after 5 s. No path was found that keeps the tracker
  owned after a failure, so a leaked token is not a likely cause.
- If a token did leak, the effect would be Lamium's own moves
  refusing ("begin-token-unavailable", "another inventory request owns
  the tracker"), not a locked vanilla slot. That does not match the
  report's wording.
- Two places where a client-only slot state could come from, both in
  `game::movePair` / `giveCreative` (`InventoryMove.cpp`), not
  confirmed at runtime:
  1. Both slots are set through the client's setters first. If no
     transaction is pending afterwards, the function logs
     "recorded-nothing-pending" (trace builds only) and still returns
     true. Whether the setters sent the actions themselves in that
     case, or nothing was sent and the two slots now differ from the
     server, is not recorded anywhere in the docs.
  2. Seven callers (Tool guard, Elytra swap, Bring to hand, Death
     restore, Offhand swap, Tool switch, Weapon switch) send their move
     with no response tracking, unlike Hand Restock. If the server
     rejects such a legacy transaction, Lamium never learns it and
     relies on vanilla's correction arriving.
- Suggested next step when the maintainer re-triggers the desync: a
  `restock_trace` build already logs every `Move:` stage, so one
  session log shows whether "recorded-nothing-pending" or a rejected
  response precedes the stuck slot. No new hook is needed for that.


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
Kind: Bug, parked as research 2026-10-27 (see the end of this item). Maintainer report 2026-10-22: start FreeCamera while the body's
head is inside a block (buried in the ground); the camera then shows the
inside-a-block screen effect the whole time, also while flying in the air.
The effect should follow the camera, not the body, or be suppressed while
FreeCamera owns the view. Not a 0.3.0 blocker (leave the block first).
Decided (maintainer 2026-10-26): while FreeCamera owns the view, the
inside-a-block screen effect is not drawn; outside FreeCamera it stays
vanilla. Find the game path that draws it (record it in GAME-UPDATES.md),
gate the suppression with `versionSensitiveAllowed()` and fail open to
vanilla. Check in game: start FreeCamera with the head buried and fly out;
leave FreeCamera while still buried (the effect returns); Freelook and the
ordinary view unchanged. No release target selected.
Built 2026-10-27 (`29022178`), not yet checked in game: the view the game
builds for the local player (`LevelRendererPlayer::createViewRenderObject`)
carries `ViewRenderData::mDrawInsideCubes`; while FreeCamera owns the view
`FreeCameraInsideBlock.cpp` clears that flag on the frame's own copy, gated
(FreeCamera view) and fail-open. The SDK's `InsideCubeRenderer` and
`InsideBlockEffect` declarations are empty, so this flag is the only
reachable path (GAME-UPDATES.md). One log line "FreeCamera view:
inside-a-block effect not drawn" marks the first frame suppression reached;
the four checks above remain. The hook returns `ViewRenderObject` by value
(13 KB, `MCAPI`), so the game has to pass the sret through on every call: a
crash on world entry would point at that return path first.
Checked in game 2026-10-27 (merged line `3ac38a3a`, DLL `81006cda`): the
effect stayed after flying out of the block, so the shipped clear did not
work. A probe build (`e88bb2a7`) logged once per 60 frames whether FreeCamera
owned the view, the incoming flag and whether it was cleared: while FreeCamera
flew in the open the answer was `suppress true, vanilla true, cleared true`
every line - the flag was cleared on every frame and the effect still drew -
and the actor answered `getInsideBlock()` = air. `mDrawInsideCubes` is
therefore not what drives the effect. A second probe (`68e32ba3`) returned
null from `Actor::getInsideBlock()` while FreeCamera owned the view: the game
crashed at once (0xC0000005, `mov rax, [rax+0x68]`, RAX = 0;
`logs/crash/trace_2026-10-27_10-13-33.log`), so that answer is dereferenced
without a null check elsewhere and cannot be suppressed.
Parked 2026-10-27 (maintainer): not fixable through the exposed SDK paths -
the flag is not the driver and the actor answer may not be nulled - without
reverse-engineering the renderer that consumes it. Left vanilla, not a
release blocker (leaving FreeCamera once restores the view). The hook and its
`FreeCamera view` capability, all four translations, the test and the docs
were removed 2026-10-27 (`73584825`): it was a no-op in practice and its
"effect not drawn" log line could be read as success. The item stays parked
as research; the crash evidence above still stands as the reason the actor
path is closed.

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

### L-257 FreeCamera culler reads blocks on the render thread
Kind: Research. Split from L-242 (pre-0.3.0 review) on 2026-10-26. The
FreeCamera culler hook reads `BlockSource::getBlock` on the render thread.
Confirm whether that read is thread safe (or how vanilla reads blocks there)
before changing it; do not change it without runtime proof.

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

Header re-read 2026-11-12 (SDK 26.60.4; reading only, nothing built or run):
- Confirmed in headers: the culler classes are still empty, and
  `LevelCullerType` is still an enum without names, so only 3 and 5 are
  known. `shouldCullThisFrame` and any "culling point" name are not in the
  26.60.4 client headers; candidate (b) has no symbol to start from.
- Confirmed in headers: `LevelRendererCamera` also has `mViewRegion`
  (`OwnerPtr<BlockSource>`), `mCameraPos`, `mCameraTargetPos`,
  `mViewAreaSide`/`mViewAreaHeight`/`mViewAreaChanged`, `mForceCulling`,
  `mRecullWhenNotBusy`, `mShowSky`, and the read-only queries
  `cullerIsVisible(point, radius)` (MCAPI), `isAABBVisible(aabb, fast)` and
  `getRenderChunkInstancedAtShared(SubChunkPos)`. The culler class list
  names the kinds that exist: None, NoneAsync, Manhattan, CachedStep,
  DistanceField (Perspective/Orthographic), Shadow.
- Confirmed in Lamium: FreeCamera moves the camera through
  `CameraAPI::$tryGetActorInterpolatedPosition` and `setupCamera`, so the
  view matrix and whatever the culler starts from get the same position;
  they cannot be split at those hooks. Outside the loaded area the hook's
  `inside` test is false (no chunk), so the choice there is 3, or 5 with the
  hold on Always; the maintainer saw the sky with both.
- Hypothesis (not confirmed): the visibility walk starts at the camera's
  render chunk, and a render chunk with no level chunk behind it has no
  visibility data, so the walk ends at once. That matches "one chunk past
  the edge, all at once" and both culler types failing.
- Suggested order, each step a trace build that only reads (needs the
  maintainer's go-ahead as a game dependency; no behavior change):
  1. In the existing `TerrainPreRender` hook, once per second while
     FreeCamera is on, log `mCameraPos`, `mViewAreaDistance`, `mViewAreaSide`,
     `mLastCullerType`, `mForceCulling`, `mShowSky`, whether
     `getRenderChunkInstancedAtShared` at the camera's sub-chunk and at the
     player's is non-null, and `cullerIsVisible(playerPos, 8)`. Compare one
     chunk inside the edge with one chunk outside. This separates "the view
     area no longer holds the loaded chunks", "the culler rejects them" and
     "they pass the culler but are not queued".
  2. Map type numbers to culler kinds: request each value 0..7 for one
     second in a flat test world and log the class name of `mLevelCuller`
     read from its vtable's RTTI. Only then is candidate (a) concrete: ask
     for the None kind while the camera's column has no chunk.
  3. If 1 shows the culler rejecting loaded chunks and 2 finds a None kind,
     try (a) behind `versionSensitiveAllowed`, checking the frame time: a
     culler that rejects nothing draws every loaded chunk.
- Open for the maintainer: whether this is worth two trace rounds, given
  that flying past the loaded area is rare and the terrain there is out of
  scope anyway. A cheaper alternative that needs no game dependency is a
  product choice, not decided here: stop the camera at the loaded edge, or
  show a notice when it leaves.

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

### L-330 Radar: mob icons that draw wrongly, then icons by default
Kind: Research, then Design. Maintainer 2026-11-13: some mobs' icons on
the radar do not draw correctly, so the radar keeps its Experimental badge.
First find which mobs draw wrongly and why (`features/map/RadarFaces.cpp`;
the earlier note "L-86 Radar faces follow-up" under Later lists mobs
without a face). If the icons can be made right, the maintainer wants the
radar's default changed from dots to icons (`map.radarFaces`, off today);
that default is decided once the icons are confirmed in game.

### L-332 Death points: grow them as their own feature, or make them simpler
Kind: Design (conversation first). Maintainer 2026-11-13, after confirming
L-328: the death waypoint follows only the latest death, so it does not
serve as a record of deaths (which death, where the one before was); a
moved one loses its cross but is still named "Death point"; perhaps death
points should not have been pressed into the waypoints' frame, but grown
as their own feature with a natural link to waypoints, or made simpler.
The agent's view, not decided: make it simpler by taking the special
rules away rather than adding a record of deaths. (1) A death's waypoint
is an ordinary waypoint from the moment it is added; the cross is only its
mark and stays whatever is edited, so nothing changes shape by being
moved. (2) Death layout restore stops depending on the waypoint: it has
the death's place itself and ends when everything is back, at the next
death, or 32 blocks away after the player came back to it, so it also
works with Waypoints off. (3) A lasting record of deaths (numbered, dated)
is a different feature and is not started as a by-product.

### L-325 Zoom: return to the set magnification on release
Kind: Design (small). Issue #74 (2026-11-10); the maintainer 2026-11-13:
such a setting may exist.
Today the wheel's magnification is kept for the next press (`ZoomState`:
`release` keeps the target; only a changed setting or `reset` returns to
it). Add an option under Zoom that makes every press start from the set
magnification. Open: its name and its default. From the same issue, not
taken: easing on press and release (tried and dropped 2026-10-06, see
`ZoomState::press`), and settings for animation speed and per-behavior
toggles.

### L-323 Game HUD placement: boss bars
Kind: Research, then Design. Asked for by the maintainer 2026-11-13.
Game HUD placement moves and scales the chat, the paper doll and the
scoreboard; add the boss bars as a fourth part with its own switch and its
place in the layout editor. Today Lamium only hides them (`bossBarsBit` in
`features/visuals/EffectVisibility.h`, matching `boss_health_panel` /
`boss_hud_panel`). To find out first: which control carries the whole stack
of bars, how its size changes with the number of bars and with the text
above each bar, and whether moving it holds across screen and GUI scale
changes as the other three parts do. Open choices after that: the anchor
when the number of bars changes, and how it behaves with the hide switch.

### L-320 Highlight designated block types in the world
Kind: Design. Maintainer's idea 2026-11-07 (multiplayer-session feedback).
Designate block types (diamond ore, ancient debris, anything) and render
them emphasised in the world so they are not overlooked. On Java Edition
this is done with a resource pack; Bedrock Edition cannot switch resource
packs from inside a world, so here it would be a client feature.
Designation happens in the world: aim at a block and add its type with a
key; a list manages the designated types.
Decided in the same discussion: emphasis only, on blocks that are actually
visible — nothing is ever shown through walls; X-ray-style display is out.
Data: only blocks in chunks the client has received, scanned live like
the light overlay; never the recorded map data, which may be stale.
Rendering builds on the overlay foundation (light overlay / Shapes);
matching kinship: the schematic check.
Decided 2026-11-08 (maintainer): the point is finding blocks that are far
away or awkwardly placed, so a scanned overlay (Shapes-style, radius 64 at
most) is too short. Instead the highlight is built into the terrain mesh,
in the same tessellator hook as Connected Textures: a designated block's
vertices are tinted toward its type's color and drawn at full light so it
shows in the dark. Reach is the render distance, with no scan and no
per-frame cost; hidden faces are culled by the game itself, so nothing
shows through walls. Changing the list or the switch rebuilds the chunks
once. No outline; blocks drawn as block entities (chests) are not covered.
Matching by block name only (states ignored; deepslate ores are their own
names). Colors per type from the waypoint palette, the next free one on
adding (superseded below: one rainbow for all). Designation: a key adds the aimed block's type, or removes it if
already listed; the list is global (settings), not per world.
Steps: (1) the hook, settings, key and rebuild, with full light and the
tint checked in game (UV1 as the light map is a hypothesis; Vibrant
Visuals may need MERS emissive); (2) a list view to see, recolor and
remove types.
2026-11-08 in-game result (build 2d14c846): the tint shows; raising UV1 to
.94 made the block darker (UV1 read ~0 to .0002 in a dark cave, so its
scale is not 0..1) and Night Vision washes the tint out. Maintainer chose
a glow overlay on top: the tessellator hook records each designated
block's drawn faces per chunk section (`RenderChunkBuilder::build`), and
the world overlay draws them unlit with a pulse in the type's color plus
an outline — no scan, render-distance reach, visible in the dark and under
Night Vision. The UV1 raise is dropped; the mesh tint stays as the base.
Animation reference: resource packs that do this tend to use flashy,
animated looks. Persistence: the list stays until removed; forgetting a
type is answered by the list view (step 2), not by expiry.
2026-11-08, after the glow worked (`3614d9da`): the maintainer wants the
"gaming" look and expects per-type colors to go unused. The glow is now a
rainbow flowing across the world (hue from time and position), the same
for every type; the mesh tint is dropped so the texture stays vanilla
under the translucent glow. Saved per-type colors are kept in the file
but no longer used; the list view will not offer recoloring. Still open: the tint strength; whether a schematic
placement's blocks share this list (default: no, they stay with the
schematic check).
2026-11-08: open problems from use - the designated types cannot be seen
anywhere, and a type not yet found in the world cannot be added. Step 2
becomes a list screen that also adds types from all block types; mockup
[block-highlight.html](demos/block-highlight.html) (A search list vs B icon
grid, display-name search, a nearby count, the deepslate sibling line, the
toast and target-card line), awaiting the maintainer's choice.
Decided 2026-11-08 (maintainer): A, the search list, matching both the
display name (current language) and the ID; the rest left to the
implementer. The list opens through an action row under Block highlight
with the same Open button as the other tool openers (Shapes, World map,
Hotkeys; its key is unbound by default) and replaces the settings rows
like the game-language list, with a way back to the section. Implementer's
choices: left the listed types in the order added (icon, name, ID, a
"nearby" count, two-press remove); right a search field over every
registered block type with the listed ones marked, clicking a row adds or
removes; no "not covered" marking (the block-entity flag does not tell
which blocks the terrain mesh draws), the footer says chests and the like
are not covered instead. Nearby = recorded marks of that type: blocks in
built sections with at least one face not covered by an opaque full
block, so it counts buried-but-exposed cave walls too, not "visible from
here". After adding a type, a line offers its deepslate sibling (or the
plain one) when that name exists and is not listed. The target card gets
a "Highlight: listed" row for a listed block. The list needs a world
(icons come from the world's items).
2026-11-08: the list passed in game (`983bf80f`, VALIDATION-LOG).
Decided 2026-11-08 (maintainer): a schematic placement's blocks do not
share this list; the open question above is closed (no). Reported the same
day: with FreeCamera the key took the body's aimed block, not the camera's.
Now, while Freelook or FreeCamera owns the view, the key takes the block the
target card picks along the camera's ray, as far as the card reaches; from
the body it keeps the game's own hit.
Decided 2026-11-09 (maintainer), revising "nothing is ever shown through
walls": an option, off by default, draws the recorded faces through walls
too. Recorded faces are already only those not covered by an opaque full
block (touching air, water, lava, leaves, slabs and the like), which is
what a FreeCamera can see anyway; blocks buried on every side have no
recorded face and never show. Look (implementer): faces behind terrain are
drawn faint without edges under the normal glow, which keeps its strength
where the face is in sight. No material without a depth test is known, so
the faint pass moves each vertex along its own ray to within about two
blocks of the eye: the screen position is unchanged and terrain no longer
occludes it. The face cap stays as is (it already counts hidden faces).
Decided 2026-11-09 (maintainer), look and cost: both switches stay off by
default (Experimental; Through walls also because servers may treat it as
X-ray). One "Color" row: Rainbow (default, the flowing, pulsing look) /
Cyan / Yellow / Pink / White (the Shapes palette); a single color is still,
with no pulse. One "Strength" slider, 0-100 % in steps of 5, default 50 %
(today's look); it scales the faces of every color, 100 % about doubles
them, 0 % leaves the edges alone; the through-walls faces keep their fixed
share of it. No speed, edge or pulse switches and no separate through-walls
strength. The glow does not follow the Lamium-wide Animations setting
(that one is for UI motion); a still color is the way to stop the motion.
Cost: a rainbow rebuilds and uploads every face each frame (up to
`maxGlowFaces`); a single color keeps its meshes and rebuilds them only
when the faces, the color, the strength or the material change. The
through-walls faces depend on the eye, so they are rebuilt whenever the
camera moves, in either look.
2026-11-09: checked in game (`03034955`); with deepslate listed about 70 FPS
rainbow vs 165 FPS still. The rainbow now refreshes its faces in batches of
1024, a few per frame, each about 30 times a second (`rainbowRefreshHz`; 20 kept by the maintainer after the 2026-11-09 check: about 100 FPS, no visible steps; the game's texture animations also step once per tick, 20 times a second, e.g. fire: 32 frames in 1.6 s), so
the work per frame is even and falls as FPS rises; a full rebuild remains
when the faces are rechosen (a capped list after moving 8 blocks).
2026-11-11, from the 0.6.0 smoke (maintainer): the deepslate sibling offer is
removed. Both forms are in the suggestions and in any search, and the row
it added moved the results under the pointer, inviting a wrong click.
Reported the same day: cactus and glass panes did not glow. Only
`tessellateBlockInWorld` (full cubes) was hooked; the shape dispatcher
`tessellateInWorld` is hooked too now, and a non-cube block gets the same
full-cell glow (checked in game the same day). Chests and shulker boxes
add no vertices (block entities) but the dispatcher is called for them: a
shape that drew no cube part is marked by its uncovered faces alone, and
they glow too (checked). Suggestions (maintainer): keep them, add the wet
sponge after the ores.
The "10 件" overlap was not about digits: the game's font measures "件" and
"渡" 0 wide and draws them one character left of the pen (the two found
among Lamium's Japanese strings; log in VALIDATION-LOG). Labels now measure
each non-ASCII character once per font, give a zero-wide one a full width
and draw it as its own run (`labelRuns`, TextFit.h). Checked in game
(`2a75b760`, VALIDATION-LOG).

### L-319 Item details: Work Penalty first, then the unshown rest
Kind: Design. Maintainer's note 2026-11-07 (multiplayer-session feedback).
Target Info gives blocks a rich card, while items only get container
contents, durability and food values in the tooltip. Main ask: show the
anvil Work Penalty — the item's RepairCost as the level surcharge it adds
to any anvil operation (a rename alone costs penalty + 1) and the
prior-use count behind it (penalty = 2^n - 1). Check both against the cost
the anvil screen shows before trusting them, as L-170 says (the 2^n - 1 rule
was read from the screen 2026-11-12, VALIDATION-LOG). Do not predict a whole
operation's cost; that depends on the other item and the operation.
Then investigate what else the client knows about an item but Lamium does
not show, and show that too. Candidates to verify in code before listing
them as displayable: per-instance data (suspicious stew effect; firework
detail vanilla does not print) and item-definition data (attack damage
and speed, armour value, fuel burn time, anvil repair material,
enchantability, max stack). A derived value (grindstone experience
return) needs its own call — deterministic formulas only. Home: the item
tooltip, beside durability and food values.
Header read 2026-11-12 (SDK 26.60.4; reading only, nothing run):
- Linkable or virtual, so readable without a symbol request:
  `ItemStackBase::getBaseRepairCost()` (the Work Penalty) and
  `getMaxStackSize()` (`MCAPI`); `Item::getAttackDamage()`,
  `getArmorValue()`, `getEnchantValue()`, `getEnchantSlot()`,
  `getMaxDamage()` and `isValidRepairItem(...)` (virtual).
- Not found as a linkable accessor: fuel burn time (only
  `FuelItemComponent::mFuelDuration` for component items, and the brewing
  stand's `PotionBrewing::getFuelValue`, `MCNAPI`), attack speed, and a
  reader for the suspicious stew's effect (`SuspiciousStewItem` has only the
  type enum). These stay off the list until a source is found.
- Pure part done: `enchantPlan::priorUses(penalty)` in
  `src/features/inventory/EnchantPlan.h` gives the prior-use count for a
  stored surcharge, or nothing when the value is not 2^n - 1. Not shown
  anywhere yet.
Decided by the maintainer 2026-11-13, built the same day and confirmed by
the maintainer in game (`8a3f8cca`, VALIDATION-LOG: one, two and three
anvil uses agreeing with the anvil's cost, nothing on a fresh item, a line
on a merged book, the setting off):
- First only the work penalty; the other values wait for a look at what
  is worth showing and what the game already prints.
- One line under the durability line, in the same grey: "Work penalty: +3
  (2 anvil uses)". Nothing for an item whose stored cost is 0. A cost
  that is not 2^n - 1 is shown alone, without a count (pure
  `enchantPlan::workPenalty`).
- A setting of its own, "Work penalty", on by default, a feature row
  beside Durability numbers and Food values with its toggle key (unbound).
- It is read as the enchantment order reads it and asks the same
  capability (`EnchantOrder`): on a game version that is not verified the
  line is left out.
Still open, not started:
- Which definition values to show. Recommended: only values that differ
  from "nothing" (attack damage above 0, armour above 0), after checking
  what the game's own tooltip already prints; enchantability and max
  stack are rarely useful and can wait.
The maintainer's direction 2026-11-13 (the work stops here for now; not
decisions):
- The rest is meant wide: from attack damage and armour to what the game
  folds into "+NBT", as much about an item as can be shown.
- Durability numbers, Food values and Work penalty all add a line to an
  item in an inventory, so they may belong together in the settings. The
  doubt was whether Food values also covers the HUD: it does not. Food
  values (`inspection.foodValues`) is the tooltip line only; the hunger
  bar's outline and its preview of held food are the separate feature
  Saturation (`information.saturation`, `information.saturationPreview`).
  The agent's view: group them when the wider values arrive, as one
  feature with a switch per line; the settings file's keys and the three
  toggle keys can stay as they are, since only the rows' grouping changes.
Decided by the maintainer 2026-11-13 (later the same day) and built as a
trial, not yet checked in game:
- All four groups are shown: armour values, fuel, the ID with tags and
  stored data, and the other definition values. Always shown for now; what
  to do if a tooltip gets too long is decided after seeing it.
- One setting, "Item details" (`inspection.itemDetails`, on by default,
  toggle key unbound), a feature row after Work penalty. The agent's
  choice for the trial; a switch per line waits for the grouping above.
- The lines, after the work penalty and before the food icons (pure
  `inspection/ItemDetails.h`): grey for armour, armour toughness, knockback
  resistance (%), enchantability, a stack limit between 2 and 63, cooldown
  (s), fuel as items smelted; dark grey for the ID ("#N" for a data value
  other than 0), the tags (wrapped, four lines at most) and the top level
  of the stored data (eight entries, then a count; compounds and lists by
  size only, so a full shulker box is not serialised each frame).
- Fuel is `FurnaceBlockActor::getItemBurnDuration(item, 200)` divided by
  200. What the second argument means was not checked: coal must read 8.
- Left out: the time an item takes to use (`Item::getMaxUseDuration` wants
  an `ItemStack` the tooltip does not always have), attack speed and the
  suspicious stew's effect (no reader found), and attack damage (the game
  prints it; to confirm in game).
To see in game: the values against known ones (netherite chestplate 8 / 3 /
10%, coal 8, ender pearl stack 16 and cooldown 1 s), how long the tooltip
gets on an enchanted tool and a shulker box, and whether any line repeats
what the game already prints.
Seen by the maintainer 2026-11-13 on `63c672ca` (VALIDATION-LOG) and
changed the same day, not yet checked in game:
- The armour lines are gone: the game prints the same three values.
- Coal and charcoal showed no fuel while logs (1.5) and a lava bucket (100)
  did. Where the furnace's function answers 0 the burn time is now read
  from the item's `minecraft:fuel` component (seconds).
- Stored data is told whole (a picked piston hid one entry behind the
  count): compounds and lists are walked, each level indented, up to 48
  lines and 4096 entries walked. The maintainer: first show all that can
  be read, calm the look afterwards.
- The food icons sit above the detail lines (`findGlyphRun` takes the last
  line made of glyphs alone, no longer only the text's last line).
- A suspicious stew's effect, which the game prints only in Creative, is
  added from `Item::buildEffectDescriptionName(stack, true)` when the
  tooltip does not already hold it. Asked for that one item only.
- Use time is read from the `minecraft:use_modifiers` component (seconds),
  so only component items have it; values above 60 s are not told.
- Attack speed is dropped from the list: the game has no attack cooldown.
Open: "Enchantability: 15" tells that an item can be enchanted but not
what the number means to a player; the maintainer keeps the line and may
want a better wording. Tooltip length waits until the repeats are gone.
Seen by the maintainer 2026-11-13 on `6f70bdb2` (VALIDATION-LOG): coal and
charcoal read 8, the picked piston's data is whole, the stew's effect shows
in Survival, the food icons sit right above the details. Changed the same
day, not yet checked in game:
- The stew's effect came without the game's colour and was repeated in
  Creative, where the game prints it itself. The line is now compared and
  shown without colour codes, and coloured as the game does: red for a
  harmful effect, blue otherwise, white when the effect cannot be told.
  Harmful is `MobEffect::mIsHarmful` of the effect whose translated name
  the line starts with.
- No use time showed on a bow, bread or a milk bucket: they have no
  `minecraft:use_modifiers` component. `Item::mMaxUseDuration` is read
  first now, the component only where that is 0.
Seen by the maintainer 2026-11-13 on `70de5f54` (VALIDATION-LOG): the
stew's effect red or blue in Survival and not repeated in Creative; use
time on bread and a milk bucket (1.6 s), dried kelp (0.8 s) and a crossbow
(1.25 s), none on a bow. Changed the same day, not yet checked in game:
- In Survival the stew's effect sat under the food icons and lacked the
  game's second line ("-0.5 Attack Damage" under Weakness). The effect
  now goes above the food icons, where the game puts it, followed by one
  line per attribute the effect changes: `MobEffect::mAttributeModifiers`
  with `getAttributeModifierValue(0, ...)`, named by the game's
  `attribute.name.<attribute>` text and left out where that text is
  missing. The first level is assumed, which is what a stew gives.
  Confirmed by the maintainer the same day on `becfac62` (VALIDATION-LOG):
  Weakness in Survival reads as in Creative, an effect without an
  attribute adds no line, Creative shows nothing twice.
Decided with the maintainer 2026-11-13 (the design talk), built the same
day, not yet checked in game:
- Two forms. Short, by default: the value lines, the ID and the tags as
  before, and the stored data as one line with its count ("NBT: 12
  entries", first worded "Stored data"). Long, while a key is held: the count gives way to the data.
- The long form is the raw content, nothing left out, but not a tree:
  each top-level entry is one line, a compound or list as its whole text
  on that line, cut at 60 characters; at most 24 entries, then a count. A
  shulker box's items are one cut line: seeing them is the container
  preview's job. The indented tree is not taken for now.
- The key is an action of its own ("Show NBT in tooltips while held",
  `itemdetailsmore`), held, Alt by default. The agent's choice among
  keys that all mean something in an inventory: Shift and Ctrl are held
  for nearly every transfer, Alt only for "every item of a kind" clicks,
  so the tooltip changes least often by accident. The maintainer is not
  against Alt but has not settled it.
- Enchantability keeps its line ("Enchantability: 15"); what the number
  means is told in the setting's help, not in the tooltip.
- No scrolling: with one line per entry it is not needed, and the wheel
  stays Inventory transfer's.
Seen by the maintainer 2026-11-13 on `83bddd4c` (VALIDATION-LOG): the
count line shows and Alt + click still moves every item of a kind, but the
long form never came, with Alt or another key. Changed the same day, not
yet checked in game:
- The key was only allowed during gameplay, like most actions, and
  tooltips are in container screens. It is now a container-only key, as
  Sort is (`containerOnly` in `Binding.h`), so it also only meets
  container keys in the Hotkeys conflict list. If the long form still does
  not come, the game keeps a slot's tooltip text instead of asking each
  frame, and the switch has to move to where the text is drawn.
- Tags that wrap were ragged: the first row after the label, the others
  indented two spaces. One row stays beside the label; more go under it,
  every row indented alike.
- "Stored data" was Lamium's own word. The game has no player-facing name
  for it; its language file only says "NBT" (a command message), so the
  line and the key say NBT.
Seen by the maintainer 2026-11-13 on `28d12745`: wrapped tags line up;
the long form still never comes, held before or after the pointer reaches
the item. A second gameplay-only check sat in `executeAction`
(`Actions.cpp`), ahead of the line that sets the held state; the press is
now handled before it, beside Sort. Not yet checked in game.
Seen by the maintainer 2026-11-13 on `f3b0e62f` (VALIDATION-LOG): the key
works, but the form only changes when the pointer moves to another item,
on pressing and on releasing; the long form of a shulker box fits the
screen and is enough to look into a structure or a picked piston. Changed
the same day, not yet checked in game:
- The game asks for a slot's hover text once, when the pointer reaches it.
  Both forms are now built then and remembered by their text, and the
  tooltip's painter (`TooltipPainter`) puts the other one in for the draw
  when the key's state differs: `HoverRenderer::mFormattedContent` and
  `mBoxDimensions`, written for one draw and put back. The new box keeps
  the room the game left around the text and takes the other text's lines
  and widest line (pure `details::resized`). Its place is not recomputed:
  a long form opened near the screen's edge may run off it until the
  pointer moves. Gated by a capability of its own (`TooltipForms`); on a
  game that is not verified the form follows the key at the next hover.
Seen by the maintainer 2026-11-13 on `bde4219b` (VALIDATION-LOG): the
form changes at once on pressing and releasing, the food icons stay in
place in the long form, but the box keeps the size of the text the game
had: `mBoxDimensions` is not what the draw uses. Changed the same day, not
yet checked in game: the hover control's own `mSize` is set to the new box
and its `mCachedPosition` moved back so the box ends inside the screen
(the outermost parent's size; pure `details::keptInside`), both put back
after the draw, as Scoreboard position does with its control.
Seen by the maintainer 2026-11-13 on `d9db34bb`: the box still does not
change with Alt. Neither the renderer's `mBoxDimensions` nor the control's
`mSize` draws the frame. Third try, not yet checked in game: the frame is
the renderer's `NinePatchLayer` (`mLayer`), which holds its size (`w`,
`h`) and nine cached quads; for the draw the quads' far coordinates are
moved by the growth (pure `details::stretched`) and put back. The first
six swaps log what the game holds ("Tooltip forms: ..." in lamium.log:
control, box, cursor, offset, frame size and quads) so the next step can
be read from the log if this fails too; take the line out once the frame
follows.
Seen by the maintainer 2026-11-13 on `ff043a26` (VALIDATION-LOG): the
frame now takes the long form's size on Alt, but not its place: the game
places a tooltip for the text it has when the pointer arrives, so a form
changed afterwards runs off the screen, leaves its text, or sits away from
the item. Held or not held from the start, both forms look right.
Decided by the maintainer the same day, built, not yet checked in game:
- No key. Switching inside the inventory is hard to understand, and with
  one line per entry the long form is rarely too tall. The held action
  (never pushed, so no saved id is left behind), the swap while drawing,
  its capability and its log lines are removed; the game lays out
  whatever text it is given.
- Two detail toggles under Item details, both off by default: "Show tags"
  (`inspection.itemDetailsTags`) and "Show NBT"
  (`inspection.itemDetailsNbt`). No count-only form for NBT: the count
  alone is seldom wanted, and two states make it a toggle.
- The value lines and the ID stay under the one Item details switch.
Confirmed by the maintainer the same day on `a3a8dcca` (VALIDATION-LOG).
For a later session, the maintainer's wish: look at Durability numbers,
Food values, Work penalty and Item details (with its two toggles) with
fresh eyes and decide how these tooltip settings should be grouped and
split. Not to be decided as a by-product of other work.
Asked by the maintainer the same day: hiding the game's list of contained
item names in a shulker box's tooltip, since the preview shows the same.
That exists: "Hide Shulker contents text" under Container previews, off by
default. Whether it should be on by default is the maintainer's call.
Decided by the maintainer 2026-11-13: on by default, since the shulker box
preview is on by default and shows the same contents (a saved choice is
kept; only installs without one change). The same day the rows were renamed
from "Shulker" to "shulker box" in English and Japanese ("Shulker box
previews", "Show empty shulker boxes", "Hide shulker box contents text" and
their help), because "Shulker" alone reads as the mob; the Chinese rows
already said "shulker box". Built and confirmed 2026-11-13 (`26ebab3a`).
Decided by the maintainer 2026-11-13 in that session, looking only at what
each line shows and for whom, built the same day and confirmed by the
maintainer on `e6562d2e` (VALIDATION-LOG):
- One heading, "Item details", without a switch or key of its own (as Map
  text is), and a child switch per kind of line in the tooltip's order:
  Durability numbers, Work penalty, Suspicious stew effect, Food values,
  Property values, Show ID, Show tags, Show NBT. The four feature rows are
  gone; their toggle keys sit on the child rows.
- The old Item details switch held three unlike things and is split. The
  stew's effect tells what the game hides in Survival, so a player may not
  want it (`inspection.itemDetailsStew`). The ID is the one line on every
  item and is for commands and packs (`inspection.itemDetailsId`). The five
  values of an item's kind stay together as Property values, which keeps
  `inspection.itemDetails` and the `itemdetails` key. Durability and Work
  penalty both tell the one item's state but are read by different players,
  so they stay apart.
- Nothing saved changes meaning: no settings key or action id is renamed.
  The two new keys, when a file lacks them, take the file's
  `inspection.itemDetails`, so nothing appears that was switched off. Tags
  and NBT no longer need Item details on.
- Not asked, left as they were: the ID stays on by default, fuel stays
  inside Property values.
Decided by the maintainer 2026-11-13 (later), built the same day and seen
by the maintainer on `988806af` (VALIDATION-LOG):
- Item details gets a main switch (`inspection.itemTooltip`, on by
  default), as Hide effects has: off leaves the game's own tooltip, the
  child switches keep their values and are marked "Main switch off". Its
  use is comparing with what the game itself shows.
- Every row can have a key: the main switch (`itemtooltip`) and the four
  children that had none (`itemstew`, `itemid`, `itemtags`, `itemnbt`).
  All unbound. F3+H is not a default: a player who knows that key from
  Java Edition expects the ID and technical lines, not this switch.
- The ID stays on by default: it is how a player finds an item's English
  name for a search or a command.
- The help shown on a child while a main switch is off named Hide effects
  for every feature; it now says "this feature".
Known and left: the toggle keys only work outside screens, so a key is
pressed before the inventory is opened.
Seen by the maintainer on `988806af`, three doubts, answered the same day:
- The toast of a child's key drew the main switch's state. Fixed
  (`cd71814f`) for every key that sits on a child switch's row.
- "(Main switch off)" was added to the child's name. Decided: names are not
  rewritten; the row is dimmed and the footer says why (DESIGN, Components).
- Only Hide effects, Game HUD placement and Item details dimmed their
  children. Decided: every feature with a switch does. Checked in code for
  the children that might act with the feature off (the stew preview of
  Saturation, the death point of Waypoints, the mark layers of Map marks,
  the hide options of Info HUD, Debug View and Status effects); only the
  death point does, so Waypoints is left undimmed. The rest of the child
  settings were not read one by one. Confirmed by the maintainer on
  `d80c18ca` (VALIDATION-LOG).
Three more doubts from that check, decided by the maintainer the same day
(DESIGN: Components, Toast), built, not yet checked in game:
- A child's toast did not say which feature it belongs to. It reads
  "Feature > Child" now, from the settings screen's labels; the toast-only
  names are removed.
- Keys bound alike all fired but each toast replaced the one before. The
  toggles of one key press share the toast, a row each (four, then a count).
- Waypoints' switch only hid the markers while a death point was still
  recorded. It is the whole feature's switch now, so Waypoints dims its
  children like every other feature.
Confirmed by the maintainer on `b8ddb003` (VALIDATION-LOG). The sign
between the names was "›": it drew, but in another font in English and
low in Japanese, so it is ">" now (confirmed on `5220a3db`). The toast's
text sat at its row's top, about two units above the switch's middle; it
is centered in the row now, as a HUD line with a background is. Not yet
checked in game.
Decided by the maintainer the same day, built, not yet checked in game:
the raise of Latin runs, a hidden 1.5 units in Japanese only, becomes the
setting "Latin text raise" (DESIGN, Text and languages). In the
screenshots Japanese looked raised too far and Chinese, with no raise, had
Latin runs low. Open: the Automatic values per language, to be read off
the setting by the maintainer (Japanese now 1.5, Chinese 0).
Seen by the maintainer on `dba113dd` (VALIDATION-LOG): Japanese is right at
1; in Chinese a raised Latin run changes font and width, because a run
drawn apart from the hanzi gets the pixel font, so the setting cannot do
there what it says. The toast's centered text was right in English and a
unit low in Japanese and Chinese; it starts a unit higher in those now
(`rowTextLift`), not yet checked in game.
Decided by the maintainer the same day: the setting is removed (it was
never released, so `interface.latinRaise` needs no migration) and the raise
stays Japanese-only, now 1 unit instead of 1.5 (DESIGN, Text and
languages). Confirmed by the maintainer on `fab713d8` (VALIDATION-LOG),
with the toast's text beside its switch in Japanese and Chinese.
Still open: the enchantability wording if the help text is not enough.

### L-317 Forcing the Surface view leaves the minimap nearly useless in the Nether
Kind: Design. Reported from play 2026-11-07 (multiplayer-session feedback;
the maintainer's own play agrees).
With the minimap view forced to Surface (L-253), the Nether also uses the
surface view, so only the bedrock ceiling is visible and the minimap
loses almost all meaning there. Recorded as a problem: doing nothing is
not the answer, but no candidate is good enough yet — the solution is
deferred.
Candidates: (a) in the Nether only, a forced Surface falls back to
Automatic; (b) let the view choice be set per dimension; (c) a roof-aware
forced Surface (cave view while a ceiling is above the player) — close
to what Automatic already does, so it may only duplicate it.
Counterweight: forced Surface has real uses in the Nether — creative
work that breaks the bedrock ceiling and builds custom spaces. A fix
that quietly erases the force in those cases trades one complaint for
another. L-253's checked behaviour stands until a candidate survives
this.

### L-315 Finish what waits for more game symbols
Kind: Blocked on a symbol request. Kept as one item so a single request to
`LiteLDev/mcapi-requests` covers it; the maintainer prefers not to file one now
(2026-11-06), to keep requests to LiteLDev infrequent. Nothing here is urgent.
- Book paste as one continuous sheet (L-259, BACKLOG-DONE): a paste turns to where it
  ended, so repeated pastes stay in order, and later pages reflow so no gaps stay at
  the seams. Likely needs the game's page turn, `_setPageText` or
  `BookScreenManager::editPage`, `deletePage` and `TextEditContext::feedCaretUpdate`;
  find the page turn's call first. Writing `mCurrentPage` does not work: it corrupted
  a book (VALIDATION-LOG).
- Ghost note block instrument: `Block::tryGetComponent<BlockInstrumentComponent>` does
  not link, and the request form rejects it (L-305 item 4 has the details). Its checker
  ignores template arguments, so this needs a note to the maintainers, not the form.
- Book paste's trace hooks held back by `LAMIUM_MCAPI_PENDING`: only if another trace
  is needed.
Before filing, confirm by linking: an SDK update may already have changed these.
Header check 2026-11-12 (SDK 26.60.4, the same SDK as L-305; nothing linked or run):
`_setPageText`, `BookScreenManager::editPage`/`deletePage` and
`TextEditContext::feedCaretUpdate` are still declared `MCNAPI`, so nothing has
changed since L-305. `BookScreenController` declares no page-turn member at all
(only `_tryFirePageTurnSound`), so the turn is probably a button handler
registered in the constructor (a guess from the header); a request would have to
name that handler's address, not a member function.

### L-249 Show the chunks within simulation distance
Kind: Design / Research. User request 2026-10-23 after 0.3.0: show which
chunks are within the simulation distance (where mobs, crops and redstone
tick). Chunk Borders (`src/overlay/ChunkBorders.h`) draws only the borders
around the player; nothing shows the simulated area.
Goal: let the player see which chunks are simulated, without implying more
than the game guarantees.
Open: where the simulation distance comes from (the world setting is known
locally; on servers it may not reach the client) and whether the client can
read it from game state; how to show it (Chunk Borders option, minimap
overlay or world map layer); behavior on servers where it is unknown.
Research the source first, then agree the presentation with the maintainer.
No release target selected.
Header read 2026-11-12 (SDK 26.60.4; reading only, nothing built or run):
- Confirmed in headers: `ILevel::getChunkTickRange()` (virtual, so callable
  on the client level without a new symbol) and
  `getChunkTickRangeManager()`, whose `ChunkTickRangeManager` holds
  `mChunkTickRange` and has `isChunkInTickRange(LevelChunk, minDistToPlayer)`
  (`MCNAPI`). `LevelSettings::mServerChunkTickRange` and
  `LevelData::mServerTickRange` hold the same number as data.
- Hypothesis (not confirmed): the range is part of the level settings the
  server sends at join, so a client on a server has it too. If so, "unknown
  on servers" may not happen at all.
- First check, read only: log `getChunkTickRange()` on the client level in a
  local world at simulation distance 4 and 8, then on a server whose
  distance is known. One log line at world join is enough.
- Not known from headers: the simulated area's shape (square, circle or the
  chunk-distance rule `isChunkInTickRange` uses) and whether it is centred on
  each player. `isChunkInTickRange` could answer per chunk, but it does not
  link (`MCNAPI`); the shape must be checked in game or the function
  requested.
- Open choices for the maintainer, with the agent's recommendation (not
  decided): where to show it. Recommended: a Chunk Borders option that
  tints the border lines of simulated chunks, because that overlay already
  draws per chunk around the player and needs no new screen; the minimap or
  world map layer can follow once the shape is confirmed. When the range
  cannot be read: recommended to draw nothing and say so in the option's
  description, rather than assume a value.
Probe 2026-11-12 (`world_probe` build, agent-run, VALIDATION-LOG):
`getChunkTickRange()` on the client level returns 4 in a local world.
Whether that follows the world's setting (not the default) and what a
server gives are the two checks left before the option is built.


### L-265 Schematic placement assist: place the ghost's block in its state
Kind: Design / Research (strong model). Maintainer note 2026-10-26 while
deciding L-263: a mode in which using a block on a ghost cell places the
schematic's block there with the schematic's facing and states.
Open: whether to do it at all; which states can be produced by the
player's own placement (facing from look direction, half, slab type) and
which would need a faked look or a server-visible trick; what servers and
anti-cheat accept; how it relates to the vanilla place input and to L-263
(it reuses L-263's "bring the block into the hand" step); refusal when the
block is not carried. Do not change gameplay packets beyond what the
player could do by hand without a separate decision.
No release target selected.

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
Helper done 2026-11-12 (pure logic and tests only; the renderer's cache is
unchanged): `src/features/schematic/SectionLayers.h` gives a section's layer
state under a `PlacedSlab` as a comparable value: hidden, whole, or the shown
range on the cut's axis. Whole and hidden compare equal across axes, so only
sections holding the cut differ when it moves. `tests/SectionLayersTests.cpp`.
For the renderer step (not started): the section mesh reads a halo of
neighbour cells (`GhostMesh.cpp`, `halo`), so the range passed in must be
widened by it, or a section next to the cut would keep faces toward a layer
that has turned hidden; the test file has that case. The cache key then
needs the section's value instead of the placement's whole layer view, and
palette resolution should move out of the per-section rebuild.

### L-190 Select several rows at once in list screens
Kind: Design for the whole; the first step is a trial on branch
`l-190-waypoint-selection`, not merged. Maintainer idea 2026-10-14.
Maintainer 2026-11-06, after using it: a good trial, but Lamium does not say it
supports multiple selection until the lists it is needed for have it, those a player
adds to without limit: Shapes, schematic placements and the map's side panel. Merge
and announce together. `ui/RowSelection.h` is the shared part; each list's bulk
actions are decided with it.
Decided (maintainer 2026-11-06), no file format change: the Waypoints screen first;
Ctrl + click toggles a row, Shift + click takes a range, no drag selection; show,
hide and delete together, delete with a second press. The death point stays single.
Other lists follow once this one has been used.
Goal: in list screens (waypoints, the schematic Check list and similar lists)
select several rows with Ctrl/Shift + click or by dragging, and act on them
together (for example hide, delete or show in world).
Open: which lists and which bulk actions; how Ctrl (toggle one) and Shift
(range) combine with the current single selection and the detail pane;
drag selection versus dragging to scroll or reorder; confirmation for
destructive bulk actions; keyboard equivalents.

### L-183 Temporarily disregard selected schematic check mismatches
Kind: Design, after 0.5.1. User feedback clarified 2026-10-14.
Decided (maintainer 2026-11-06): the choice persists across rejoining, so there
is no unsaved first version. Per placement in its saved file, as an optional key
in schematic coordinates, so moving or turning the placement keeps it. The unit is
one difference on one cell (this cell's `powered`), so another block there still
shows as a mistake. A count of what is ignored stays visible, with a list to undo
one or all. Entries the structure no longer has are ignored; the number is capped.
Older builds that save the placement may drop the key. The word "temporarily" goes.
Proposed, to settle before building: ignored differences count as matching in the
totals, and the choice is made from the Check list's row first.
Goal: while building redstone and other stateful structures, temporarily
exclude selected expected-vs-actual discrepancies from the check results.
A block with a temporarily different powered/open/extended state can be
intentional during construction; the schematic file and physical blocks do
not change. This is **check-result suppression**, not hiding the ghost or
automatically relaxing every BlockState comparison.
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
Built 2026-11-03: the ghost's saved block entity data shows as target-card rows
under the same rules a real block's rows use (L-300/L-301) - sign text, colour,
glow and waxed state, a jukebox record, a lectern book and page, a potted
plant, a note block's note and instrument - so a ghost and the block it becomes
read alike.
Decided 2026-11-03 (maintainer): a ghost's card follows the same rules as the
real block's, from the file alone. This is the base for the file-only data and
the comparison still to come, so a difference between a ghost and its block
must come from the data, never from the presentation. The world never fills a
gap: a value the file does not decide reads "Not set by the schematic" rather
than the world's, which would pass for the file's. A note block's instrument
comes from the file's blocks below and above it (each block declares one
instrument for a note block under it and one for a note block on it, checked
in game); outside the file or at a void cell it is not set. Passed in game on
`a193244e`, and on the merged build `951fb651` (PR #68). Reviewed 2026-11-03: a block type that stores an engaged `none`
where others store nothing would have counted as a decision and swallowed the
instrument row, so the neighbour read normalises it (`declaresNoInstrument`).
Three things to keep in mind, not defects: a note block on the file's top or
bottom layer always reads "Not set by the schematic", correct but likely in a
small schematic, so the wording may want to name that reason; `fileCellAt`
guards only `structure`, not visibility, dimension or layers as `blockEntityAt`
does through `drawsCell`, which is safe while `ghostAlong` is its only caller;
and `GhostAt::blockEntity` points into the `structure` the same struct owns, a
lifetime the type does not enforce.
The row comes from the placement's `block_entities` entry for that
cell (`PlacementOverlap.h` `blockEntityAt`, turned and mirrored like the cells),
so it is complete even though the world's copy of a block entity is not.
Still open, and now the whole of what is left: the data only the file has
(container contents, a comparator's signal) and how to present it.
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
Kind: Design, direction decided 2026-11-06. From the maintainer's notes (2026-10-13).
Direction: no separate quick shapes, which would make the player check again after
every login. One action on an aimed beacon creates an ordinary shape, saved, listed,
shown on the maps and edited like any other; a Box should hold it with no file
format change. It is a copy of the range when made, so a changed pyramid needs a new
one, and on a server it goes at rejoin as every shape does. Check first in game: the
range's exact width and vertical extent, then the key or menu that creates it.
Header read 2026-11-12 (SDK 26.60.4; reading only): `BeaconBlockActor` has
`mNumLevels` and `mNumLevelsSet` as plain members and gets its state through
`_onUpdatePacket`, so the pyramid's level count is probably readable from the
client's block actor, also on a server (not confirmed in game). No range or
radius member exists in the header: the size per level has to come from the
in-game check and live in a pure, tested table in Lamium.
The proposal below is superseded where it differs.
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
Decided (maintainer 2026-11-12): the anvil highlight is wanted, as a
follow-up after the first version.
Decided (maintainer 2026-11-12): two goals, fewest levels and least prior
work on the result. The maintainer referred to the two choices the
enchant-order web tool offers (PROVENANCE.md); both narrow the orders in a
useful way. The planner has both (`Goal`, below).
Decided (maintainer 2026-11-12): merging two books of one enchantment into a
higher level stays out of the first version; more than 10 inputs is refused
and the screen says so.
Decided (maintainer 2026-11-12): a two-way switch above one step list,
fewest levels by default, with a line saying so when both goals give the
same plan.
Groundwork 2026-11-12 (pure logic and tests only; no screen, setting or
action): `src/features/inventory/EnchantPlan.h` plans the order for up to 10
inputs. The caller gives each input's cost as the sacrificed side and its
earlier anvil uses; the refusal threshold is a parameter. It returns the
steps with each cost, the total, the most expensive step and the result's
anvil uses. `tests/EnchantPlanTests.cpp` compares it with an exhaustive
search on 300 small sets. `Rules::goal` (2026-11-12) picks what is minimized
first: `FewestLevels` (total, then the most expensive step, then the result's
anvil uses) or `LeastPriorWork` (the result's anvil uses, then total, then
the most expensive step); the tests compare both with exhaustive searches.
Checked in game 2026-11-12 (VALIDATION-LOG "Anvil costs behind the enchantment
planner", vanilla anvil screen, local world):
- The prior-work surcharge is 2^uses - 1 for each side and the result has
  max(uses) + 1.
- A step's cost is the sacrificed side's enchantment levels times a
  per-enchantment multiplier, plus both surcharges, with nothing else added
  for undamaged, unrenamed inputs. The multiplier for a sacrificed item was
  twice the book's in both cases read (Knockback 1 and 2, Curse of Vanishing
  4 and 8); the screen should take multipliers from the game rather than a
  Lamium table, as proposed above.
- A step costing 40 or more is refused in Survival; Creative has no limit.
  `Rules::refuseAt = 40` matches; a Creative player needs 0.
Not checked: a damaged input, a rename, an enchantment already on the item or
in conflict with it, servers.
Data sources, read through `world_probe` 2026-11-12 (VALIDATION-LOG "World
probe: the enchantment registry"); all of these link on SDK 26.60.4:
- The list: `Enchant::mEnchants()`, 42 entries on the client, each with
  `mEnchantType`, `mStringId`, `mFrequency`, `mIsDisabled` and the virtual
  `getMaxLevel()` and `isCompatibleWith(type)`.
- The multiplier: no linkable function gives it (`ItemEnchants::getTotalValue`
  is `MCNAPI`). `mFrequency` 30, 10, 3, 1 matched 1, 2, 4, 8 per level for a
  sacrificed item and 1, 1, 2, 4 for a book in all nine costs read from the
  anvil. Use that mapping, a pure function with a test; a new enchantment then
  needs no Lamium update, a new frequency value does. It is a meaning a
  rebuild cannot check, so the screen asks `versionSensitiveAllowed()`.
- Whether an enchantment goes on the item: `EnchantUtils::canEnchant(stack,
  {type, level}, false)` answers `Enchant`, `Increment`, `Conflict` or `Fail`.
  It answered `Conflict` for one the item already had at that level.
- Display names: `EnchantUtils::getEnchantNameAndLevel(type, level)`.
- The item's earlier anvil uses: its repair cost, as recorded for L-319.
- Not tried: `EnchantUtils::combineEnchantedItems(first, second, out, book)`
  returns an int and links; it may give the game's own cost for a pair and
  would then replace the mapping. Try it before building on the mapping.
Mockup 2026-11-12: [demos/enchant-planner.html](demos/enchant-planner.html).
Decided after its first review (maintainer 2026-11-12):
- The item can be chosen; the screen opens on the held one. A `◀ ▶` stepper
  is turned down: it does not scale to the number of choices.
- The held item's enchantments are fixed rows and its anvil uses are charged.
- A step names an intermediate book by its contents, and items and books are
  drawn with their icons (`inspection::render::drawItemIcon`).
- Creative follows the current game mode and has no limit; no switch.
- Curses are listed last. The screen is called "エンチャント順序".
Decided after the second version (maintainer 2026-11-12):
- "Held" is the main hand, the selected hotbar slot; the hotbar and
  inventory are not searched.
- The item is chosen by kind (sword, pickaxe, ...) from an icon grid: what
  goes on an item follows its enchant slot, not its material.
- While the main-hand kind is chosen, a "use the held item's state" switch
  is shown; off plans for a fresh item of that kind.
- Sets: "everything at its highest" (enchantments with no conflict) and the
  last selection per kind. No Lamium-written or saved sets.
Decided (maintainer 2026-11-12, after the research below): the step list
beside the anvil screen is part of the first version.
The step list beside the anvil, researched 2026-11-12 (VALIDATION-LOG "World
probe: the anvil screen"); nothing was drawn yet:
- Drawing: the `AfterUIRenderEvent` of the container screen's `ScreenView`
  gives a render context; Lamium already draws there (container previews,
  locked trades).
- Which screen: `getScreenName()` is `anvil_screen`.
- Where: the window is the control `root_panel` (176x166 at 234,91 in a
  644x348 view), read from `mCachedPosition`/`mSize` once laid out, so the
  list can sit to its right; 234 units were free there.
- What is in the slots: `getItemStack("anvil_input_items", 0)` and
  `("anvil_material_items", 0)`, with `constructItemEnchantsFromUserData`
  for their enchantments and `getBaseRepairCost()` for their anvil uses.
- The game's own cost for the pair in the slots:
  `AnvilContainerManagerController::mCost` (a class-layout read behind
  `versionSensitiveAllowed()`), so the marked step can be checked against it
  and a difference shown rather than trusted.
- Input: the list takes no clicks or keys in the first version. A click
  outside the window drops the cursor's stack in vanilla, and keys go to the
  vanilla screen (the calculator dropped its container-screen step for that
  reason). The plan is changed from the planner screen.
- Still unknown: other GUI scales and narrow windows (the list needs a
  fallback when it does not fit: hide it), and whether Lamium's key opens the
  planner while the anvil is open.
Pure logic added 2026-11-12 in `EnchantPlan.h`, tested: `levelCost(frequency,
book)` (the mapping above; an unknown frequency gives nothing), `carried(...)`
(which planner inputs a stack in an anvil slot stands for) and `stepFor(...)`
(the step joining two sides).
The design is agreed and building was chosen (maintainer 2026-11-12).
Built 2026-11-12, not checked in game (VALIDATION.md): the screen inside the
settings (`src/ui/EnchantView.cpp`, layout in `EnchantLayout.h`), opened by an
unbound key or the Open button of "Enchantment order" under Inventory; the
game reads and the list beside the anvil in
`src/features/inventory/EnchantOrderGame.cpp`; the edits in `EnchantOrder.h`;
the capability `EnchantOrder` gates both. Differences from the mockup, for the
review in game:
- A kind is named by its example item in the game's words ("Diamond Sword"),
  not by a kind word: Lamium has no translated kind names and the game offers
  none.
- A row shows the enchantment with its level as the game writes it and has
  `-` `+` buttons, instead of a separate level between the buttons.
- The plan and the last choice per kind last for the session; nothing is
  saved to the settings file.
- The list beside the anvil also says when the pair in the slots is not a
  step, and warns when the game's own cost differs from the plan's.
- README and FEATURES.md wait for the review below.
First use by the maintainer 2026-11-12 (`0d2b5ecd`, VALIDATION-LOG): the four
checks passed. The maintainer's notes from using it, grouped; none is decided:
- Plain defects: the conflict reason carries a level ("ダメージ増加 I と競合")
  where the level means nothing; "All highest" turns curses on; it also
  enables "Last choice" when it changed nothing; the set buttons move down
  when the held-state row appears; on boots Feather Falling sits between the
  four protections that exclude each other.
- Reading: curses are not red as in the game; the kind icons are too small
  to choose from; a kind shows an item's name ("Diamond Sword") and reads as
  if the plan were for that item only, a kind word is wanted; where one step
  ends and the next begins is hard to see; the totals sit at the bottom, in
  the screen and beside the anvil, where they are not seen.
- Held state: fixed rows look dead (a press does nothing); the maintainer
  suggests weakening it, for example leaving the held state when such an
  enchantment is chosen.
- Sets: "All highest" could choose the usual one of a conflicting group
  (Protection on armor, Sharpness on a sword); the maintainer does not know
  the usual one for every kind (bow, mace).
- Items missing from the kinds: a book (enchanted books merged into one) and
  a carved pumpkin.
- Beside the anvil: nothing says what is wrong with a pair, which items the
  next step needs or how many levels the player has; the plan cannot be
  changed there; an item's work penalty is not shown anywhere (L-319).
- Outside this item: English Search does not find enchanted books by an
  enchantment's English name ("protection", "knockback") in the Creative
  search field.
Decided (maintainer 2026-11-12, after the grouped proposal) and built the
same day, not checked in game:
- The plain defects are fixed: the conflict reason is the name alone (what
  the game's texts for level I and V share); "All highest" leaves curses out
  and records nothing when it changes nothing; the held-state row is always
  there (grey without a main-hand item of the kind); enchantments that
  exclude each other are listed together (`grouped`).
- Reading: curses are drawn in the game's red (`palette::curse`); kind cells
  are 20 units with a full-size icon; a line separates the steps; the totals
  sit under the goal in the screen and on the first line beside the anvil.
- Kind names come from the game, never from Lamium: its creative group name
  where it has one (`itemGroup.name.sword` and the like: swords, pickaxes,
  axes, shovels, hoes, the four armor pieces, spears, skulls, enchanted
  books), else the item's own name (Bow, Trident, Mace...). The maintainer
  asked for vanilla wording in every language, Chinese included.
- Held state: pressing an enchantment the item carries leaves the held
  state; what it carries becomes wanted at the same level (`release`), and
  the footer says so.
- A level button below level 1 turns the enchantment off (the maintainer
  had to travel to the switch to drop Knockback after "All highest").
- "All highest" takes one of a group that excludes each other by the game's
  data alone: the highest frequency weight, then the one that goes on the
  fewest kinds of item, then the first (`allHighest`). No Lamium table.
- New kinds: enchanted books merged into one (a plan without an item) and
  the carved pumpkin; a skull stands in if the pumpkin's slot is missing.
- Beside the anvil: the total and the player's level on top (the level in
  the warning color when the next step costs more; not in Creative), done
  steps greyed (a stack at hand holds everything the step joins), the next
  step marked, a line saying which side of the pair is not in the plan or
  what the next step still lacks, and the prior-work surcharge of the two
  slot items.
Second use by the maintainer 2026-11-12 (`9a1bec30`, VALIDATION-LOG): the four
checks passed. Fixed the same day, not checked in game:
- The anvil list kept saying "finished" with no way out. A stack holding
  every input counts as finished, so another item with the same
  enchantments does too. Now a finished plan leaves the anvil's side once
  the anvil is closed (the planner brings it back), and a finished item is
  ignored while another item of the plan that is not finished is at hand
  (`progress`): holding a fresh sword means a second one is being made.
- The main-hand mark is a line under the kind's cell, clear of the icon.
- The enchantments keep one order whether the held state is used or not;
  what the item carries is fixed in its place.
- The held-state row sits under the list and shows only for the main hand's
  kind, so nothing above it moves.
- More space under the totals.
Third pass by the maintainer 2026-11-12 (`cfd22b5e`, VALIDATION-LOG): passed;
the lasting "finished" had been a stone pickaxe with the same enchantments.
Decided the same day and built, not checked in game:
- The held state is a cell, not a switch: the main-hand item itself (its own
  icon) stands left of the kinds, apart from them. Choosing it plans from
  its state; choosing a kind plans a fresh item, and choosing the held
  item's own kind keeps what it carries as wanted. The white mark and the
  switch row are gone. The cell appears only when the item's anvil uses
  could be read.
- An enchantment row reads name, switch, level buttons, as the settings
  table orders a row; "on the item" and the conflict reason stand where the
  switch would.
- The list beside the anvil has a switch, "List the steps beside the anvil"
  under Enchantment order (`inventory.enchantOrderAnvil`, on by default): it
  is the only part that shows without being asked. The planner itself has
  no switch.
Fourth pass by the maintainer 2026-11-12 (`b87a65eb`, VALIDATION-LOG): passed.
Decided the same day and built, not checked in game:
- A press on an enchantment the item carries does nothing but explain itself
  in the footer: fresh and main hand are chosen by their cells now.
- The main-hand cell is the last cell of the grid, after the enchanted
  book's, with a lighter frame; its place is kept free when nothing is held,
  so the grid never moves.
- An enchanted book in the main hand gets the cell too (it had been left
  out): the plan then merges further books into it, charging its anvil uses;
  it may end up on either side of a step.
Fifth pass by the maintainer 2026-11-12 (`06a753ef`, VALIDATION-LOG): passed,
"All highest" on a mace taking Density included. Decided the same day and
built, not checked in game: an enchantment row reads name, then the level
as a stepper (`-` level `+`, as Lamium's number rows), then the switch at
the right edge. The name no longer carries the level; a row that is off
shows the level it would turn on at, faint and without buttons; a row with
one level has the switch alone. This replaces "name, switch, level buttons"
of the fourth pass, which left the level far from its buttons and an empty
stretch right of the switch.
Sixth pass by the maintainer 2026-11-12 (`2af7b12e`, VALIDATION-LOG): passed.
A row on the item now writes its level in the level column like the others
(built, not checked in game).
Raising a carried enchantment, read and built 2026-11-12 (not checked in
game): the anvil charges the difference in levels (VALIDATION-LOG "Anvil
cost of raising a level"), so a book's value has a second form for "the
kept side holds the item" (`Input::onItem`). A row on the item has level
buttons from its level up; a raised level counts as one of the nine books.
Not for a held enchanted book: it may be the sacrificed side, which was not
read. "All highest" raises what the item carries too. The row's "on the
item" tag is gone (it pushed the level out of its column in Japanese and
English); the row has a dimmed switch that stays on, and the footer says
why. The list beside the anvil is drawn before the anvil screen instead of
after it, so the game's tooltips stay on top.
Eighth pass by the maintainer 2026-11-12 (`c884f267`, VALIDATION-LOG): passed,
but the list drawn before the screen is under the screen's dark layer.
Changed the same day (not checked in game): the list is drawn after the
screen again, at full brightness, and is left out for a frame in which the
game's item tooltip overlaps it (the tooltip's box is read where Lamium
already hooks its drawing, `HoverRenderer::mCursorPosition`, `mOffset` and
`mBoxDimensions`). The first three boxes are logged to `lamium.log` so the
meaning of those members can be checked against a screenshot.
Ninth pass by the maintainer 2026-11-12 (`4800022a`, VALIDATION-LOG): passed;
the logged boxes agree with the reading of the tooltip's members, and the
log lines were removed afterwards. README and FEATURES.md describe the
feature from here on.
Asked by the maintainer 2026-11-12: the list above the screen's dark layer
and under the tooltip, always shown, as the ideal. Built the same day as a
trial (not checked in game): Lamium already hooks the game's tooltip
drawing, which runs inside the screen's own drawing, so the list is painted
there right before the tooltip, from the state worked out the frame before;
a frame without a tooltip paints it after the screen as before. The
step-aside rule and the tooltip box read are gone. To see in game: the list
at full brightness with a tooltip over it, no flicker when the pointer
crosses items, and the list's item icons not showing through the tooltip.
Tenth pass by the maintainer 2026-11-12 (`e8d1d318`, VALIDATION-LOG): passed;
the maintainer agrees with trying choice (a) below for editing. Two things
seen and changed the same day (not checked in game):
- Closing: the list now moves with the window by however far the window is
  from where it rests, without being held at the screen's edge, so it
  slides off with it. It does not fade; Lamium's text has no opacity.
- An item dragged over the list was under it. The list is now painted
  before the first slot of the anvil screen draws in a frame (a new hook on
  `InventoryItemRenderer::$render`, `EnchantListBeforeSlots`), so slots, the
  dragged item and the tooltip all come after it. The tooltip hook and the
  after-screen paint remain as fallbacks for a frame in which no slot draws.
Eleventh pass by the maintainer 2026-11-12 (`768db9be`, VALIDATION-LOG):
failed. Painting from the slot renderer's hook left the list and every
enchanted item white: the game is mid-way through its item drawing there.
The hook is removed (do not paint from `InventoryItemRenderer::$render`
again). Instead, built the same day and not checked in game:
- While an item is held on the pointer and the pointer is over the list,
  the list is not painted, so the item is seen whole. Painting the held
  item again on top of the list was the alternative; it needs the game's
  exact place and size for it.
- Closing: the game's window fades as it slides. The list now takes an
  opacity from how far the window has moved from rest (gone at half the way
  to the screen's bottom) or the window control's own `mAlpha` when that is
  lower; `ui::DrawOpacity` applies it to fills, text and the icons.
Twelfth pass by the maintainer 2026-11-12 (`9ff54047`, VALIDATION-LOG): passed,
but the list giving way to a held item flickers and is not wanted. Decided:
the list is always drawn; the held item on top of it if that can be done,
under it otherwise. Built the same day (not checked in game): the give-way
rule is gone, so the held item is under the list again. To put it on top
Lamium would draw the held item once more over the list, which needs the
game's exact box for it; the build logs, three times per run, the small
controls under the pointer while an item is held at the anvil, to find that
control. Remove the log lines once it is known.
The held item's control was found from the log (VALIDATION-LOG "The item held
on the pointer at the anvil"): `selected_item_icon`, 21 units, following the
pointer. Built the same day (not checked in game): while that box overlaps
the list, Lamium draws the held stack once more in the same box after the
list, with its glint, so it is seen on top. Its count and durability bar
are not drawn again, so over the list a stack shows no number. The log
lines are removed.
Thirteenth pass by the maintainer 2026-11-12 (`8d62d240`, VALIDATION-LOG): the
held item is on top without doubling, but without its glint and with its
durability bar under the list. Built the same day (not checked in game):
the held item is drawn in the same call as the list's own icons, last, as a
glint does not show after another batch of icons; its durability bar is
drawn over it with the slot bar's geometry at the held size. The count of a
held stack is still not drawn over the list.
Fourteenth pass by the maintainer 2026-11-12 (`6fa5445b`, VALIDATION-LOG): the
bar is right; the held item's glint is lost over a long list and kept over
a short one. The maintainer asked whether the container previews' drawing
applies: it is the same icon call (`drawItemIcons`), and there every
glinting icon is followed by its own glint. The list's books were glinting
icons drawn without one. Built the same day (not checked in game): the
list's icons are drawn with their glint as in the previews, and a held
stack's count is drawn while the whole item is over the list (not while it
straddles the edge, where a second number slightly off the game's would
show double).
Fifteenth pass by the maintainer 2026-11-12 (`0855cb5b`, VALIDATION-LOG): glints
work with one step and fail with two or more, for the list's books and the
held item alike, while a preview of 30 enchanted books is fine. The
difference found in code: the list drew one book stack many times, the
preview draws a stack of its own per slot. Also seen: a held block or
glinting item went under the list's swords (those were drawn later, in a
batch), and a held stack's count went under the item at the list's edge.
Built the same day (not checked in game):
- Every icon gets a stack of its own (a pool per kind of icon), in the
  anvil list and in the planner screen.
- The anvil list draws all its icons one after another without batches
  (`IconAt::direct`), the held item last, so it is above every icon of the
  list whatever it is.
- The planner screen collects its icons and draws them in one call at the
  end, with glints (its books had none).
- A held stack's count is drawn whenever the held item is drawn again, on
  top of it.
Sixteenth pass by the maintainer 2026-11-12 (`8d5083f4`, VALIDATION-LOG): the
held item is above every icon of the list; the glints are unchanged; the
held stack's count is a little right of and below the game's.
- Count, built the same day (not checked in game): its place is taken from
  the game's own count control under `inventory_selected_icon_button` (the
  first shown descendant with "count" in its name), and it is drawn like the
  container previews' counts, with the engine's shadow; the control found is
  logged once per run. Without such a control the count is not drawn.
- Glints, open. What is known: one glinting icon followed by the held item
  works; with two or more steps no icon glints, not even the first, whether
  the list's swords go in a batch after the glinting icons (`0855cb5b`) or
  every icon is drawn in order (`8d5083f4`), and whether the books share a
  stack or not. The planner screen never glints. The container preview
  glints with 30 enchanted books through the same `drawItemIcons`. Not yet
  compared: the icon scale (the list draws at 8 units, the planner at 10 and
  16, the preview at 16), the text flushed right before the icons, real
  enchanted stacks against bare `enchanted_book` stacks. Three blind rounds
  have not found it; the next step is a build that draws a row of test
  icons in those variants, not another guess. Same family as L-91 (glints
  outside vanilla slots).
Seventeenth pass by the maintainer 2026-11-12 (`8396fafb`, VALIDATION-LOG): no
count over the list; the count control was not found by its name. The
maintainer agreed to a test build for the glints. Built the same day: the
xmake option `enchant_glint_test` (off by default). With it the anvil screen
shows, instead of the list, ten icon arrangements in turn, four seconds
each, numbered on screen (size, spacing, text before the icons, bare or
really enchanted stacks, one call or several, the list's own arrangement),
to see which keep their glint; and while a stack of two or more is held
outside the window it logs every control near the pointer that is not part
of a slot, shown or not, to find the count's control. Remove the option's
code once both are answered.
Glint test round 1 by the maintainer 2026-11-12 (VALIDATION-LOG): none of the
ten arrangements glints, the container preview still does. So the icons are
not the cause (count, size, kind of stack, calls); what the ten shared and
the preview does not is a translucent fill and flushed text before the
icons. The log named the count's control, `selected_stack_size_text`; the
list now looks for that name (not checked in game). Round 2 of the option,
built the same day, eight arrangements, five seconds each, the number
written after the icons: the preview's own code with 27 enchanted books at
the pointer; icons with nothing before them; after an opaque fill; after a
translucent fill; after an opaque fill and flushed text; at 8 units; bare
books; one stack four times.
Glint test round 2 by the maintainer 2026-11-12 (VALIDATION-LOG): the
preview's code glints at the anvil too; icons with nothing under them glint
weakly; icons on a fill of the panel colour (0.06 grey) do not. So the
screen, the hook and the text are not the cause: what is under the icon is.
The preview puts a 0.23 grey square under every icon. Round 3, built the
same day, varies that: a grey or white fill under all four, grey squares on
an opaque or translucent panel, the preview's frame colour alone, squares
alone, squares at 8 units, and the list's own arrangement with squares.
Glint test round 3 by the maintainer 2026-11-12 (VALIDATION-LOG): only the
white fill glints; the preview's own slot grey does not. So it is not what
lies under the icon but the colour of the last fill before it: the preview
draws its purple border last, the tests drew grey or the panel colour last,
and the list's last fill depends on its rows (which fits one step working
and two failing). Built the same day (not checked in game), in
`drawItemIcons` for every caller: the current shader colour is set to the
preview's border colour around each glint overlay and put back. If that is
right, the list, the planner screen and the HUD icons glint like the
preview, and the preview is unchanged. The test option stays until this is
confirmed.
Eighteenth pass by the maintainer 2026-11-12 (`d96f2cbe`, VALIDATION-LOG): the
glints (list, held item over it, planner screen, preview unchanged) and the
held stack's count are confirmed. The `enchant_glint_test` option and the
count's log line are removed. Open, the maintainer's question: the list's
and the planner's item icon is a plain stack, so it has no glint even at a
step where the item would be enchanted.
Decided by the maintainer the same day: the item's icon glints at the steps
where the item carries an enchantment (from the first step if the held item
has one, otherwise from the step after its first book). Built (not checked
in game), in the anvil list and the planner's steps: those icons are drawn
from a copy of the item with one of the plan's enchantments on it.
Confirmed by the maintainer the same day (`02a8f09f`, nineteenth pass).
Changing the plan at the anvil, researched in code the same day (nothing
built):
- A click on the list can be kept from the game: `CustomInput`'s
  `MouseInputEvent` listener cancels events before vanilla sees them, which
  Item Transfer already does on container screens
  (`TransferSession::mouseButton` answering true). The event arrives outside
  the client tick, so the hit test must use the list's box cached from the
  last frame and the action must be queued to the client thread.
- The pointer in GUI units is `ScreenView::mPointerLocationPrevious`
  (container previews, locked trades), enough for hover and for the hit test.
- The planner cannot open while the anvil is open today: `ui::open` asks for
  a gameplay screen (`gameplayScreen(getScreenName())`). Whether Lamium's
  scene can be pushed over a container screen, and whether the anvil and
  the items in its slots are still there after it closes, is unknown.
- Choices, with the agent's recommendation (not decided): (a) a "Change"
  button on the list that opens the planner over the anvil and returns to
  it on Esc, if the game allows a scene over a container screen
  (recommended first, as a trial build, because it reuses the whole
  planner); (b) the same button closing the anvil first, if (a) loses the
  anvil; (c) small controls on the list itself (the goal, dropping a
  step's enchantment), which duplicates the planner in a narrow list.
Earlier note (maintainer asked 2026-11-12): raising an enchantment the held item
already carries (Sharpness III on it, V wanted). The agent's view: this is
not the left-out "two equal books into a higher level"; that one changes
what the inputs are. Raising with one higher-level book keeps the inputs
and only changes what that book costs when it reaches the item, so it is a
small extension of the cost model (`Input` gets a second value for "the kept
side holds the item"). It needs one rule read from the anvil first: whether
the step is charged the book's full level or the difference from the level
on the item. Three readings settle it, each a fresh item with the lower
level from `/enchant` and a fresh higher book: Sharpness III + V (5 or 2),
Knockback I + II (2 or 1), Looting I + III (6 or 4).
Noted for later, outside L-170: the world map is experimental and has no way
to turn it off either; whether a screen that only opens on request needs a
switch is one question for both.
Trial of choice (a), built 2026-11-12 on the maintainer's go-ahead (not
checked in game): the anvil list has a "Change" button in a row of its own
at the bottom right. A left press on it, with nothing on the pointer, is
kept from the game (`CustomInput`'s mouse listener asks
`enchantOrder::game::listPress`, which answers from the button's box and
pointer of the last frame) and opens the planner over the anvil screen on
the client thread (`ui::openEnchantOrder(client, true)`, the one case
`ui::open` accepts `anvil_screen`). To find out in game: whether the scene
opens over the anvil at all, whether Esc returns to the anvil with the
items still in its slots, and what the planner chooses while the item is
in a slot and not in the main hand. If the anvil is lost, choice (b).
Twentieth pass by the maintainer 2026-11-12 (`998e845e`, VALIDATION-LOG): the
trial works. The scene opens over the anvil (whose window is not drawn
meanwhile), Esc returns to the anvil with the slots' items in place and the
new plan in the list; the anvil plays its opening animation on return.
Raised by the maintainer, not decided:
- Opening the planner takes the main hand's item and so drops the previous
  plan silently; from the anvil it took whatever the main hand held, not
  the item in the slot. Either the previous choice is kept while the
  main-hand cell is only shown, or a setting turns the main hand off.
- Without a plan the anvil shows no list and no button. The maintainer
  would rather do the planner's work on the anvil screen itself than open
  Lamium's screen each time, accepts that it is more work, and is willing
  to go through a mockup again.
Decided by the maintainer 2026-11-12: the previous plan wins. An unfinished
plan opens as it was left; the main hand's cell is shown and switches only
when pressed; without a plan the main hand is chosen as before; no new
setting. And the anvil-screen planner goes through a mockup first.
Built the same day (not checked in game): on opening, a plan that is still
active keeps its kind, its enchantments and whether it was planned from a
held item. If it was and that item is not in the main hand now (same kind,
same enchantments, same anvil uses), the main-hand cell stands for the
plan's item, rebuilt from the plan's record, so the plan is the same with
the item lying in the anvil's slot. Known gap: in that case another
enchantable item in the main hand is not offered until the plan is
finished or changed to a fresh one.
Mockup made the same day, not yet reviewed:
[demos/enchant-anvil.html](demos/enchant-anvil.html). It proposes a
one-row stub with "Plan" when there is no plan, "Edit" on the step list,
the item from the anvil's left slot (else the kind grid), and two
placements: A, the enchantments left of the window and the steps right;
B, one panel on the right with two tabs. Its five questions are at the
bottom of the page; nothing is to be built before they are answered.
Answered by the maintainer the same day; the mockup's second version shows
it:
- Placement A: the chooser left of the anvil window, the steps right.
- The chooser is always shown, with no "Plan" or "Edit" button; a press
  each time is a chore. Those who do not want the left side filled get a
  setting, not a button. With the setting off, or without room on the
  left, the step list keeps a "Change" button that opens Lamium's screen,
  shown without a plan too.
- The item is not taken from the left slot by itself: treating the main
  hand specially made the planner screen complicated before and ended as a
  cell after the kinds. The same here: the kind grid, with item cells
  added last.
- Lamium's screen stays (several interfaces for one feature are usual
  here: waypoints, shapes, schematics; and it plans without an anvil).
- Everything Lamium attaches to the anvil screen closes with it.
Decided by the maintainer the same day: both item cells. With two warnings
before building: a mark in a cell's corner sits on the item's picture and
was rejected for the planner screen, which sets its item cell apart by its
fill and frame; and the planner keeps the level in one place whatever the
row's state (on the item, on, off), which the mockup's "on the item" and
"highest" texts did not. The anvil's chooser is to share the planner
screen's look and handling wherever the anvil screen does not force a
difference.
Built the same day (not checked in game):
- One chooser for both places (`ui/EnchantChooser`, laid out by
  `EnchantLayout::layColumn`): the item's line, the kind cells, the item
  cells, the set buttons, the rows with the level stepper and the switch,
  drawn and pressed by the same code in Lamium's screen and left of the
  anvil. The anvil's panel (`EnchantLayout::fitPanel`) adds two status
  lines under the rows, where the screen has its footer.
- Item cells: the main hand's, then the anvil's left slot's (anvil only),
  both with the lighter fill and frame; their places are kept when empty.
  The plan's item is now a record taken when a cell is chosen (kind,
  enchantments, anvil uses, name), not the live item, so the plan stays
  the same when the item moves between hand, slot and pointer. A cell is
  marked chosen while the item in it matches that record. This replaces
  the earlier "keep the previous plan" patch (`f80ad88f`) and removes its
  known gap.
- At the anvil the chooser is always shown when the new setting "Choose
  enchantments beside the anvil" is on (default) and there is room on both
  sides; the steps go right. Without a plan the right panel says so. With
  the setting off or no room: steps only, with the "Change" button, shown
  without a plan too.
- With nothing chosen and no plan, an item put in the left slot is taken
  as the plan's item; otherwise the slot only offers its cell.
- Presses and wheel turns over the panels are kept from the game by the
  mouse listener (values of the last frame) and acted on with the next
  frame on the client thread. The goal is switched by pressing its name on
  the step list. Nothing is taken while an item is on the pointer.
- A finished plan is now cleared when the anvil closes, its enchantments
  going to "Last choice"; before, the planner brought it back.
- Not built: dragging the panel's scrollbar (a press on it jumps), text
  search.
Twenty-first pass by the maintainer 2026-11-12 (`47dfc78e`, VALIDATION-LOG):
the above works, at UI sizes 50% to 100%. Fixed the same day (not checked
in game):
- An item finished earlier, carrying some of the new plan's enchantments,
  was counted as the plan's item part-way and the list started in the
  middle. Only the plan's inputs and its steps' results count now
  (`progress`).
- The main-hand cell follows the main hand while the anvil is open, like
  the slot's cell.
- The anvil's panels get the thin outline Lamium's screen has, and the
  choosing panel is no taller than its rows need.
Twenty-second pass by the maintainer 2026-11-12 (`088f8120`, VALIDATION-LOG):
the three fixes work. Changed the same day (not checked in game): the
choosing panel keeps one height again (growing and shrinking with the rows
was restless); of two like items in the hand and the slot only the pressed
cell is shown chosen (the plan's record says which cell it came from, and
the other cell takes over when the item has moved there); a plain book is
no longer offered as an item (its enchant slot value is the enchanted
book's "everything").
Raised by the maintainer and not decided:
- The right panel (the steps) and the planner screen's right column differ
  and neither is clearly better: the anvil's is simple but says little,
  the planner's says more but its goal row is not obvious. They should be
  brought together, like the left side.
- What is missing for the whole plan cannot be seen at a glance; the list
  only names what the next step lacks, when it comes. The maintainer
  suggests doing this together with the related items set aside earlier
  (what the player has, the next step), as one feature.
The maintainer the same day: the related things are the item's work penalty
and the highlight of what the next step uses; rather than holding to what
the first version left out, things that belong together should be thought
through together, even if they are built later.
Mockup made the same day, not yet reviewed:
[demos/enchant-steps.html](demos/enchant-steps.html). One steps' side for
both screens: goal (two buttons or the stepper), total, a materials list
with what is not at hand marked, the steps with each side coloured (at
hand, made earlier, missing) and done / next / ready told apart, the
surcharge under a step's cost; the next step's two stacks outlined in the
anvil's inventory; the work penalty in an item's tooltip (L-319). Its six
questions are at the bottom of the page.
Twenty-third pass by the maintainer 2026-11-12 (`e85009f7`, VALIDATION-LOG):
one chosen cell and the fixed height are confirmed. The plain book is not:
the anvil does enchant a plain book from an enchanted book, in Survival and
Creative, so leaving it out is not "what the game does". Open: offer it or
not. Known if it is offered: the list cannot tell the plan's plain book
from the fresh enchanted books once it has been through the anvil (a held
book is told by what it carried, and a plain one carried nothing).
The steps' side, the maintainer's answers to the mockup the same day; no
building before the open points are settled:
- Decided: one component for both screens. The goal stays the stepper
  (arrows): it is what Lamium's settings use, and two buttons make the
  pointer travel. The next step's stacks are to be highlighted if it can
  be drawn, told apart by which slot each goes in (left or right). The
  work penalty in the tooltip is L-319's. Order: the shared steps' side
  first, then the highlight trial, then L-319.
- Open: the materials list. As drawn it repeats what the steps' colours
  say and takes room at the top. Either the steps carry the same
  information, or the list goes below the steps. Concerns with each: in
  the steps, a side made by an earlier step does not show which of its
  own materials is missing; below the steps, the list is far from the
  total, which is also "what is needed".
Agreed by the maintainer the same day and shown in the mockup's second
version: no materials list. The total's row carries a count ("materials
4/6", yellow when short); in the steps each name is yellow when that book
or item is neither at hand nor inside a merged stack, and a side is bright
only when that very stack is at hand (dim icon and grey text otherwise);
the last line names what the whole plan lacks when no step can be taken.
The highlight uses two colours, one for the stack that goes left and one
for the right, with the same outlines on the anvil's two slots.
What the maintainer had found hard to read was not the goal but the
planner screen's line "Dearest step / Anvil uses on the result": useful,
but not understood at first sight, and not shown at the anvil at all. The
mockup proposes one line for both screens in plainer words ("needed at
once: N levels", "the finished item's work penalty: +N"), each explained
in the status line on hover; the present words and no line can be compared
on the page.
Decided by the maintainer the same day (the mockup's third version):
- The plainer words. Your level goes beside the total, not beside "needed
  at once". The two items under it break onto a line each where one line
  does not hold them.
- The coloured names stay. The other rules of the second version (a side
  bright only when that very stack is at hand, a green row for every step
  that can be taken) were too much: the maintainer was reading the
  explanation instead of the list. Three ways to tell steps apart and no
  more: a yellow name is not at hand, a faint row is done, a white bar is
  the next step. Whether the next step can be taken is said by the last
  line and by the inventory outlines. The row of the pair lying in the
  slots stays highlighted as today.
- A plain book is not offered as an item, knowing that the anvil accepts
  it. That this is a choice and not ignorance of the game is recorded here
  and in FEATURES.md; the maintainer is not sure the game itself needs to
  say it.
Built 2026-11-12 on the maintainer's go-ahead (not checked in game), step
one of the agreed order:
- One steps' side for both places (`ui/EnchantSteps`, laid out by
  `EnchantStepsLayout`): the goal's stepper, the total with the player's
  level beside it and the materials count at the right, the two details
  (side by side, or a line each where they do not fit), the steps as two
  list rows each with a scrollbar, and the last line. The planner screen's
  right column and the anvil's right panel are this one drawing; the
  anvil's old compact list is gone.
- Where the player stands (`enchantOrder::follow`, pure): done steps, the
  next one and whether its two sides are at hand, and which inputs are
  held at all. The planner screen reads the player's inventory every
  frame; the anvil reads its slots, the pointer and the inventory as
  before.
- Three ways to tell steps apart: a name in the warning colour is not at
  hand, a done step is faint, a white bar marks the next. The step whose
  pair lies in the anvil's slots keeps its tint.
- The last line: the pair in the slots first (match, the game's cost
  differing, an item not of the plan), else the next step when it can be
  taken, else everything the plan still lacks.
- The other goal's outcome and the two details' explanations are said in
  the status line when the pointer is on them (the planner's footer; at
  the anvil the choosing panel's status lines). The planner's third
  summary line and the anvil's "your level" and "prior work: left, right"
  lines are gone; their texts are removed.
- At the anvil the goal's stepper takes presses while the choosing panel
  is open; the wheel and the scrollbar scroll the steps.
Twenty-fourth pass by the maintainer 2026-11-13 (`12092660`, VALIDATION-LOG):
the shared side works. Five things were not right; changed the same day
(not checked in game):
- A dividing line under the goal, as above the steps.
- The last line wraps onto up to three lines, and the panel grows by
  them, so what is missing is read whole. A side's names that are cut in
  a step show whole in a box by the row while the pointer is on it, the
  way the settings table shows a value that is cut.
- At the anvil the steps' panel starts level with the choosing panel's
  top (with the window's top when there is no choosing panel).
- The choosing panel's status takes three lines and stays inside it.
- The goal is the stepper box the lists use (`drawStepperBox`, taken out
  of `drawShapeStepper`); where it cannot be switched it is plain text.
Twenty-fifth pass by the maintainer 2026-11-13 (`582fb68f`, VALIDATION-LOG):
the five changes work; three rough spots, changed the same day (not
checked in game):
- The total, the player's level and the materials count run on as one
  line of text, as in the mockup.
- A step's prior work is short ("incl. 2") at the right of its second
  row, so the names have the room; what it means is said in the status
  line while the pointer is on the step.
- Without a plan the anvil's right panel is the same side as with one:
  the goal (switched there too), the dividers, the steps' heading and the
  lines saying what to do, in the room of two steps. The bare stub is
  gone.
Twenty-sixth pass by the maintainer 2026-11-13 (`6362a66a`, VALIDATION-LOG):
confirmed. The steps' side is closed here on the maintainer's word (more
polish is possible, none is asked for now).
Highlight trial, built 2026-11-13 (not checked in game). While the next
step's two stacks are both at hand and nothing is on the pointer, the slot
holding the stack that goes left gets a two-unit outline in the accent
colour and the one that goes right in white; the anvil's left and right
slot get the same outline while they are empty. A side already lying in
its anvil slot is not outlined. How it finds the places: the stacks come
from the `hotbar_items` and `inventory_items` collections apart (before:
the combined one), a slot's box from the `hotbar_grid` / `inventory_grid`
control's corner and the slot's index (nine columns of 18 units, pure
`gridSlot`), the anvil's two slots as the first two slot-sized square
controls under `recipe_grid`, left to right; their names are logged once
a run for the record. Drawn with the panels, after the screen or right
before the game's tooltip, never from the slot renderer. To find out in
game: whether the outlines sit on the right slots, in the hotbar and the
inventory, and on the anvil's two slots; whether they sit above the items
and under the tooltip; whether the hotbar's first slot is index 0. No
setting yet: it follows "List the steps beside the anvil".
Twenty-seventh pass by the maintainer 2026-11-13 (`8774eedf`, VALIDATION-LOG):
the trial draws in the right places, above the items and under the
tooltip, without breaking anything. The anvil's slots are named
`input_item_slot`, `material_item_slot`, `result_item_slot`; the code asks
for them by name now and the log line is gone. As a design it is not
there yet: the outline covers the item, white does not show on the light
inventory, and the anvil's own slots say nothing about whether what was
placed is right. Open: how the marks should look and what the anvil's
three slots should say.
Agreed in words with the maintainer 2026-11-13, to be compared in a trial
build rather than a mockup (it is about how marks look on the game's own
pictures and light inventory). Limits first: Lamium draws after the
screen or right before the tooltip, so always over the items; a colour
under an item would need the slot renderer, which broke the game's
drawing before. Built the same day (not checked in game):
- The anvil's three slots speak (pure `anvilSay`). Left and right: empty
  and their stack at hand, in a slot or on the pointer - the side's
  colour; holding the right stack - the same colour, stronger; holding
  anything else - the curses' red. The result slot: the pair makes a step
  at the plan's cost - green; at another cost - yellow; no step - red.
  The step they are held against is the one the placed pair makes, else
  the next step.
- In the inventory the stacks that step still wants are marked one by
  one (before: only when both were at hand), while nothing is on the
  pointer.
- Colours: left the accent green, right the "experimental" purple.
- The look of a mark, default: a one-unit frame on the slot's rim with a
  faint tint. The xmake option `enchant_mark_test` (off by default) shows
  five looks in turn, six seconds each, named on screen: thin frame;
  thin frame and a faint tint; tint alone; corners; thin frame and tint
  with the right side in yellow. Remove the option once one is chosen.
Round 1 of the looks by the maintainer 2026-11-13 (`c4c910dd` with the
option, VALIDATION-LOG): the frame with a tint and the tint alone show
best, to be tried stronger; yellow is out for the right side; what the
anvil's slots say works. Raised: green for "left" is also the colour of a
right pair - would the shapes' cyan and pink do, or are they hard to see;
could the game's own green slot background under the pointer be used;
and could a stack carried on the pointer be marked, as the panels redraw
a held item.
Round 2, built the same day (not checked in game): six looks, six
seconds each - frame with a 30% and a 45% tint, tint alone at 45% and
60%, in green and purple; frame with 30% and tint alone at 45% in the
shapes' cyan and pink. A stack on the pointer that the step wants is
marked on the box the game draws it in, in its side's colour.
Not built: the game's own hover tint. It belongs to the slot's own
drawing; turning it on for other slots would mean writing the game's
controls, and it has one colour. Only a look at how the slot draws it
(a probe) would say more.
Round 2 by the maintainer 2026-11-13 (`aa5c70a8` with the option,
VALIDATION-LOG). Decided: the tint alone, at 50% (45% looked a little
faint; the colour has to be seen, the item under it only recognised, and
if the system is right the item need not be clear), in the shapes' cyan
(left) and pink (right). That leaves green to "this pair is right", red
to "wrong" and yellow to "the cost differs". The mark on a carried stack
is left out of the first version: it flickered in front of and behind
the item and did not fit the size an item has while held. Built the same
day (not checked in game); the `enchant_mark_test` option and its looks
are removed. No setting of its own yet: the marks follow "List the steps
beside the anvil".
Thirtieth pass by the maintainer 2026-11-13 (`41179be1`, VALIDATION-LOG):
the tints and what the anvil's slots say are confirmed, except the result
slot's yellow, which could not be brought about. Changed the same day
(not checked in game):
- While one of the two stacks is carried, the other one keeps its mark
  in the inventory (before, every inventory mark went with an item on the
  pointer).
- A setting of their own, "Tint the slots to use next", on by default,
  beside the other two under Enchantment order (the maintainer: it is a
  change of looks, tastes differ). It is a sibling, not a setting under a
  setting; like the choosing panel it only works while the steps are
  listed beside the anvil, and its help says so.
Confirmed by the maintainer 2026-11-13 (`8a3f8cca`, VALIDATION-LOG): the
other stack keeps its tint while one is carried, and the tints' setting
turns them off. L-319's first part is built and confirmed; see there.
Left of L-170: the result slot's yellow (not brought about), Survival
(the 40-level refusal, the level's warning colour), the unbound key,
servers, the unverified-version fallback, and the mark on a carried stack
that the first version leaves out.
Reported by the maintainer 2026-11-13, built and confirmed the same day
(`26ebab3a`, VALIDATION-LOG): with something else than its own stack in an anvil slot, the
marks other than the red "wrong" went away - the inventory mark of the
stack that slot wants, and the empty other slot's tint when its stack was
the one lying in the wrong slot. The inventory search took any filled slot
as "its side is placed", and a stack lying in the other anvil slot did not
count as at hand. Now a slot counts as placed only when it holds its own
side, and the anvil's two slots count as at hand. The red stays as it was;
no new mark or colour.
- An item's work penalty in its tooltip is L-319's first line; not built
  here, it needs a setting and its place in the tooltip chosen.

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
More open choices found while preparing 2026-11-12 (not decided; the offer
matcher is not written until these are, because each changes it):
- Level: a wanted "Mending" or "Sharpness IV" matches that level exactly, or
  that level and above. Recommended: and above, since a higher level is
  never worse for the same price cap.
- Price: the cap compares the first input's count as shown (after
  discounts and demand) or the base price. Recommended: as shown, because
  that is what the player pays now; say so in the help text.
- A book with two enchantments: matches if any wanted enchantment is on it,
  or only when all are wanted. Recommended: any.
- Villager identity for L-166 favourites: `Actor::getOrCreateUniqueID()`
  links in SDK 26.60.4 (header read only). Whether the client's value equals
  the server's persistent id, and survives a rejoin, is the check to make
  before choosing per-villager favourites.

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
cause is known, the fix needs a different draw path (below). Known issue.
Update 2026-10-10: leather layers (and fence gates, L-119) fixed by the
shared-mesh icon path `inspection/render/ItemIcon` (checked). Update
2026-10-11 (L-142): glinting items went back to the direct path for their
glint, so an enchanted leather piece shows its undyeable parts
see-through again (maintainer: record it, not now). Left: the
enchanted shield's glint; one round failed and is parked; the research
record and the next idea are in BACKLOG-DONE under L-119.
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
Update 2026-11-07: two more items the direct icon path cannot draw — a
piglin head and a recovery compass show nothing in container previews
(user report 2026-11-05, Discord). The head is a model item like the
shield; the compass icon is data-driven. More weight for the slot-path
next step above; still parked.
Rechecked by the maintainer 2026-11-12 (`d96f2cbe`, 1.26.60.05): a piglin
head and the other heads are drawn in a container preview; a recovery
compass is still not, a compass is. The leather gap is unchanged and shows
in the enchantment order's icons as well.
Narrowed by the maintainer 2026-11-13 (seen while testing L-319, build
`6f70bdb2`): a head inside a shulker box that was opened once since the
world was entered is drawn in the preview; a head that has sat in a
shulker box as an item since before, never opened this session, is not.
Drawing the same kind of head correctly in another container or another
shulker box's preview does not change that. The same holds for the other
heads (dragon, creeper). Not looked into; where the two cases' item data
differ is the first thing to read (the stored data is now visible in the
tooltip with Item details on).

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
Header re-read 2026-11-12 (SDK 26.60.4; reading only): searching the client
headers for pumpkin, spyglass, scope, blur and overlay names finds nothing
beyond what VISUAL-EFFECTS.md already covers (`HudVignetteRenderer`,
`GuiData::mVignette`, the HUD `camera_renderer`). No new typed entry point
for step 2 came out of the headers, so step 1, the transparent-texture test
pack, is still the cheapest next step and needs no code.

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
Header read 2026-11-12 (SDK 26.60.4; reading only, nothing built or run):
- Confirmed in headers: gliding starts through entity systems, not through
  a `Player` call. `PlayerInputRequestComponent::mHasGlideIntent` sits next
  to `mHasFlyIntent`; `StartGlidingIntentSystem` and
  `StartGlidingActionSystem::createSystem(isClientSide)` exist (opaque), and
  `GlideInputSystem::doGlideInputSystem` takes abilities, move input, actor
  data flags, `FallFlyTicksComponent` and the state vector. The packet side
  is `PlayerAuthInputPacketPayload` action `StartGliding = 32` (and
  `PlayerActionType::StartGliding = 15`). `Player::startGliding`,
  `stopGliding` and `tryStartGliding` are `MCAPI`.
- Hypothesis (not confirmed): the jump key in mid-air sets the glide intent
  on the input request, the intent system checks the worn elytra and the
  fall state, and the action system sets the flag and queues the auth input
  action. `tryStartGliding` called from outside that order would then fail
  or have no packet behind it, which matches the L-70 traces.
- Next trace, read only: log `mHasGlideIntent`, `isGliding()`,
  `MobGliding::getFallFlyTicks` and the worn chest item on each client tick
  around a vanilla jump-to-glide, and around an L-70 swap followed by the
  player's own jump press. That shows on which tick after the swap vanilla
  itself accepts a glide.
- Open for the maintainer: whether setting `mHasGlideIntent` for one tick
  after the swap is confirmed counts as "a vanilla path" (it is what a jump
  press does, if the hypothesis holds) or as a faked flag. Not decided here.

### L-93 Schematic: load, place, project, verify and list materials (experimental)
Kind: Follow-up tracking; each L-item below owns its kind and open choices.
The initial scope shipped in 0.1.7; the screen review and 0.1.8 rendering/
compatibility work are complete. Their full record is in BACKLOG-DONE.md
(L-93); [SCHEMATIC.md](SCHEMATIC.md) owns the feature contract and technical
history. Runtime coverage remains in VALIDATION.md.

Open work, to choose with the maintainer rather than implement as one batch:
- Bugs: L-117 Japanese entity-name text and paused drawing work L-194
  (L-177, L-192 and L-193 closed 2026-11-07).
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

Header read 2026-11-12 (SDK 26.60.4; reading only, nothing built or run):
- Confirmed in headers: `EnderDragon` has eight `AABB*` members, `mHead`,
  `mNeck`, `mBody`, `mTail1`..`mTail3`, `mWing1`, `mWing2`, and
  `_hurt(AABB* part, source, damage)` takes the part that was hit. So Bedrock
  does keep per-part boxes on the actor class. Generic part data also exists:
  `SubBBsComponent::mSubBBs` (`vector<AABB>`) and
  `HitboxComponent::mHitboxes` (`Hitbox` = pivot + AABB).
- Not known: whether the client's dragon fills these (the pointers may be
  null or stale on the client, or point into `SubBBsComponent`), and in
  which space the boxes are (world or actor-relative). The overlay reads
  only `Actor::getAABB()` today (`WorldOverlay.cpp`).
- Suggested first check, a read-only trace on a real dragon in the End: per
  second, log whether each of the eight pointers is non-null, the box each
  points to, the dragon's position, and the sizes of the `SubBBsComponent`
  and `HitboxComponent` vectors on its entity. Reading the eight members
  depends on the class layout, so that path asks `versionSensitiveAllowed`;
  the two components are read through `tryGetComponent` and need a link
  check first (`Block::tryGetComponent<BlockInstrumentComponent>` did not
  link in L-305, so this may not either).
- If the trace shows live world-space boxes: draw them through the normal
  hitbox overlay; the head is `mHead`. Colors and labels are a choice for
  the maintainer (recommended: the existing hitbox color for the parts and
  the existing highlight color for the head, no new palette entry).
Probe 2026-11-12 (`world_probe` build, agent-run, VALIDATION-LOG): on a
dragon summoned in a local Overworld all eight pointers are non-null on the
client and point to world-space boxes that move with it (head 1x1x1, neck
3x3x3, body 5x3x5, tails 2x2x2, wings 4x3x4). So the parts are available to
the overlay without a symbol request. Still open before building: a real
dragon in the End, a server, and the color choice above.

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
Header read 2026-11-12 (SDK 26.60.4; reading only, nothing built or run):
- Confirmed in headers: `AgeableComponent::mAge`, and `BreedableComponent`
  with `mLoveTimer`, `mBreedCooldown`, `mBreedCooldownTime` and `mLoveCause`.
  `IClientInstance::getServerData()` (virtual) returns the in-process
  `Minecraft`, and `MinecraftGame::getServerInstance()` exists, so a local
  world's server side is reachable by name. `Actor::getOrCreateUniqueID()`
  links (`MCAPI`), which gives the key to find the same actor there.
- Not known: whether the client actor has either component (first bullet
  above), and whether `tryGetComponent` links for these two types.
- The second bullet above is a threading question, not a symbol question:
  the server level ticks on its own thread, so reading its actors from the
  client thread is a data race unless the read is queued onto the server
  thread and the plain values copied back. Lamium has no code that touches
  the local server today, so this would be its first; that is a maintainer
  decision (new kind of game dependency), not decided here.
- Cheapest first check, read only: on the client actor of a baby animal and
  of a just-bred adult, log whether each component is present and its
  values, once per second. If the client has them, nothing else is needed.
Probe 2026-11-12 (`world_probe` build, agent-run, VALIDATION-LOG): the
client actor has neither component (a baby cow and a baby squid in a local
world: `baby=true`, no `AgeableComponent`, no `BreedableComponent`). So a
timer needs the local server's actor (singleplayer only, the threading
decision above) or the feature stays at "baby" / "in love". Recommended:
the states only, said so in the help text; the server read is a large new
dependency for a number most players check rarely.

### L-114 3D previews in the schematic screen
Kind: Research **(strong model)**. Chosen 2026-10-08 (L-93 screen review).
Status: open for the Check tab. The Files preview and the Check tab's
colored preview are built, checked and shipped in 0.1.8; the previews have
drawn block entities and entities (`3889c2a`) and liquids (`6fbaa3a`) since
then. Not done: the Check tab preview's mistake look and see-through emphasis
(decided in the Check tab review below, then deferred to the 0.1.8 rendering
work, which shipped without them), and moving the zoomed view.
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
Groundwork 2026-11-12 (pure logic and tests only; not used by the Materials
tab): `src/features/schematic/RawMaterials.h` takes the wanted items and one
chosen recipe per craftable item and returns the raw materials, the crafts
per intermediate with the spare output, and any item left raw because its
recipes lead back to itself (iron block and ingot). Needs are added across
the list before rounding to whole crafts. `tests/RawMaterialsTests.cpp`.
Header read 2026-11-12 (SDK 26.60.4; nothing run): `Recipes` holds
`mRecipes` as tag -> id -> `Recipe`, and `mFurnaceResults`; `Recipe` has
`getResultItems()`, `getIngredient(x, y)`, `getCraftingSize()`,
`isShapeless()` (virtual) and `mMyIngredients`, `mTag`, `mPriority`. So the
client-side table can be walked by name; what it holds on a server, and how
tag ingredients ("any log") appear in `RecipeIngredient`, is still the
first thing to check in game.
Open choices, with the agent's recommendation (not decided):
- Several recipes for one item: recommended to prefer the crafting-table
  recipe with the highest `mPriority`, then the stonecutter, then the
  furnace, and never a recipe whose ingredient is the item's own product
  (block from ingots, ingots from block).
- An ingredient with several choices (any planks): recommended to show it
  as one named group ("Any planks") rather than pick a wood type.
- Furnace steps: recommended to list the smelted input as raw and not
  count fuel, since fuel depends on what the player burns.
- Spare output (2 stone bricks left from a batch of 4): recommended to show
  only the raw totals at first; the per-step list can come later.

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
Groundwork 2026-11-12 (no hook, no build option yet):
- Pure part: `src/features/research/FrameProfile.h` collects a window of
  frame times and per-stage times and reports the average, the worst frame,
  the 1 % low (mean of the slowest hundredth) and each stage's time and
  calls per frame; `tests/FrameProfileTests.cpp` covers it.
- Stage boundaries that link in SDK 26.60.4 (header read only; each would be
  a timing-only hook that calls the original unchanged):
  `GameRenderer::renderCurrentFrame` (the whole frame),
  `LevelRenderer::preRenderUpdate` (already hooked by FreeCamera terrain)
  and `LevelRenderer::renderLevel`, `LevelRendererPlayer::$queueRenderEntities`,
  `$renderBlockEntities` (already hooked by the ghost pass) and
  `$renderEntityEffects`, and `RenderChunkCoordinator::tick` (chunk rebuild
  bookkeeping). Chunk mesh building itself, particles and UI have no
  linkable entry found yet; `RenderChunkBuilder` and the particle engine are
  the next headers to read.
- Not done on purpose: the spike's hooks and its xmake option. They are new
  game dependencies, and the scenes that matter (village, farm, storage
  room, fast flight) need the maintainer at the controls.

---

## Later / parked

### L-329 Health a food would give back, beside its food values
Kind: idea, recorded and not pursued (maintainer 2026-11-13).
Beside the hunger and saturation a held food restores, show how much
health eating it would bring back. A food restores hunger and saturation
only; health follows from natural regeneration, so the number would be an
estimate (an upper bound that sprinting, jumping and damage lower), and
drawing an estimate is doubtful in itself. The maintainer's recollection:
Bedrock regenerates more slowly than Java, which widens the gap between
the estimate and what is seen. If taken up: first measure Bedrock's
regeneration rule in game, then decide whether an upper bound is worth
showing at all.

The first notes here were imported on 2026-10-12 for later consideration;
low-priority requests accepted since then are appended at the end.
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
Model 2026-11-12 (pure logic and tests only; no input hook, setting or
action): `src/input/OpposingKeys.h` resolves one axis from press, release
and clear events; `tests/OpposingKeysTests.cpp` covers auto-repeat, stray
releases and clearing on focus loss. Both release behaviors are modelled so
the choice below costs nothing either way.
Open choices, with the agent's recommendation (not decided):
- Release/resume: when the winning key is released while the other is
  still held, the held key takes over again (`Held`), or nothing moves
  until it is pressed again (`Repress`). Recommended: `Held`; it matches
  what a player expects when rocking between two keys, and `Repress` reads
  as a dropped input.
- Default: recommended off, since it changes vanilla movement (vanilla
  cancels opposing keys).
- Still to research before any wiring: where vanilla turns key state into
  the move vector, so the result replaces only the cancelled case, and
  what controllers and permanent movement feed into the same place.

### L-282 Toggle Vibrant Visuals with a key

Kind: Research, then Design. Requested on the Discord server
2026-10-28: is there a Vibrant Visuals toggle key; if not, can one
be added.
Maintainer note 2026-10-30: the motive, as reported, is weak PCs —
turning Vibrant Visuals off from a key instead of opening the vanilla
settings screen, which is heavy on such machines. The value is mostly
in the quick exit; turning it back on is the same key press.
Research first: whether the client can change the vanilla graphics
mode outside the settings screen (an options path the client owns),
what a switch costs mid-world (resource or shader reload, hitch,
screen flash), whether the vanilla setting itself persists the
change, and how version-sensitive the path is (Vibrant Visuals is
still changing; a new hook needs a GAME-UPDATES entry and the
capability gate). Priority is not high: this is a convenience proxy
for a vanilla setting, not information Lamium adds. Decide whether to
build it only once the research prices the switch.
No release target selected.

### L-283 Stronger block outline and a crosshair face highlight

Kind: Design. Requested on the Discord server 2026-10-29: a
customizable block outline (color, thickness, a see-through mode
showing all the cuboid's edges instead of only the visible ones) and
a highlight of the face the crosshair points at. The vanilla outline
is "way too inconspicuous"; the face highlight is for operations that
rely on precise angles.
Maintainer priority note 2026-10-30: tools outrank visual
enhancement, and Lamium's visual features mostly make more
information readable. Split this request the same way: the face
highlight and the outline's legibility are information and are the
point of the item; free color/thickness choice is aesthetics and
carries less weight, so decide the customization scope against the
existing discipline (palette colors; the four shape colors) rather
than assuming a free picker.
Open: the rendering path for the targeted block and its version
sensitivity (capability gate, GAME-UPDATES entry); the depth order
against the vanilla outline under the DESIGN world-overlay rule
(position markers win, guides yield); replace the vanilla outline or
draw alongside it; non-cuboid blocks (slabs, stairs, fences) whose
selection shape is not the full cube; what see-through edges may
draw over. Settings shape is one feature switch with only the child
options the design actually decides.
No release target selected.

### L-284 Custom crosshair

Kind: Design. Requested on the Discord server (maintainer's
recollection 2026-10-30; the requester and the date were not
recorded): let the player change the crosshair's appearance instead
of keeping the vanilla one.
Open: what is customizable within the UI discipline — style/shape,
size, color from the palette; the drawing is rectangles and text
only. A Lamium-drawn crosshair needs the vanilla one hidden and must
follow the vanilla crosshair's contexts (hidden with Hide HUD, the
states where vanilla changes or drops it). Related to L-283 (both
are about reading the aim point); decide whether they share a
settings home. Priority: low as pure appearance unless the design
ties it to visibility; nothing decided yet.
No release target selected.

### L-287 Make the minimap arrows legible at every map size
Kind: Design (parked 2026-10-31). Split from L-286 when its camera arrow
landed on main: the marker itself is done, but the player and camera arrows
are drawn into the 256 px map texture, so a small map minifies them into a
mangled shape, and at larger sizes the plain triangle does not read as an
arrow. The maintainer gave up on the current look for now.
Tried on branch `l286-minimap-camera-arrow` (kept locally, commits
`b2f563e4` to `ec66e53f`, not merged): screen-space arrow images (no
minification with the map), scaling with the Map Size with a 6 px floor, a
two-unit outline, a notch-less triangle for small arrows, a head-and-shaft
glyph, and hard black/white edges instead of anti-aliasing. Each was checked
in game; none was accepted: small sizes stayed hard to tell apart from the
white radar dots, and large ones showed a thin edge, an obvious triangle, or
a grey mosaic.
Open: a different marker rather than another glyph tweak - for example a
direction wedge on the map frame, the mockup's green player arrow, or drawing
the arrow in the HUD layer above the map. It must read at the smallest Map
Size without dominating it, keep the camera arrow distinguishable, and still
look like the map grew out of the game's own art. Do not re-tune the current
glyph without a new idea.
Re-check 2026-10-31 after L-286 merged to main (`b6ad8327`, DLL `c38a2ff2`):
the camera arrow draws well, and most Map Size and Range settings look clean.
Narrowed the same day: the distorted player and camera arrows come back
around Map Size 18 %, so the distortion is limited to some map sizes rather
than gone. That fits the in-texture drawing (the 256 px map texture, arrow
included, is minified to the map's on-screen size and those scales alias the
arrow's thin outline and notch). Watch item: re-open with that condition if
wanted.
No release target selected.

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
