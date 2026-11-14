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
L-item ([MAP.md](MAP.md), [SCHEMATIC.md](SCHEMATIC.md),
[ENCHANT-ORDER.md](ENCHANT-ORDER.md), [ITEM-DETAILS.md](ITEM-DETAILS.md));
current status and remaining work stay here. An item that has gathered more
than a few dated rounds moves them to such a doc (2026-11-14). Document roles: [README.md](README.md).

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

1. **Bugs first.** Open: L-343 (a server's map, waypoints and placements lost
   when its address changes; Research), L-335 (rows under a switched-off feature dim only in
   the middle; Ready), L-339 (smaller bugs from the 2026-11-14 code review;
   Ready), L-299 (a slot stuck after a screenless move when client and server
   desync), L-240 (death layout restore acts unexpectedly; Research), L-324
   (custom blocks brown on the map; Research), L-227 (FreeCamera shows only
   the sky outside loaded terrain; Research), L-243 terrain gaps, L-245 ghost
   face beside glass, L-257 FreeCamera culler thread, L-117 schematic name
   tags. What waits for more game symbols is collected in L-315.
   Parked: L-194 save corner marks, L-123 elytra pose, L-91 shield glint,
   L-132 inventory-screen movement, L-244 FreeCamera started inside a block,
   L-287 minimap arrow look; respect each item's stop/resume conditions. L-181
   trident pose is low priority.

2. **Small and medium work before large features/integration** (maintainer
   2026-10-10): the L-266 schematic follow-up batch closed with 0.3.1 except
   L-265 placement assist (Design). Decisions waiting for the maintainer:
   none. Built, with open checks
   or follow-ups in their items: L-338 death layout time limit, L-170
   enchantment order planner, L-319 item
   details, L-320 block highlight (released in 0.6.0). Remaining
   Design/Research: L-336 Durability HUD items, L-337 minimap size, L-340
   settings grouping, L-341 shared widgets (Refactor), L-325 zoom release,
   L-330 radar icons, L-166 trade helpers, L-167 resource packs, L-168 trade
   finder, L-169 transfer extensions, L-173 quick shapes, L-190 list
   selection, L-163 a Lamium inventory screen, L-249 simulation distance and
   L-317 (Nether minimap under forced Surface). Agree open choices before
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
- L-323 boss bar placement: Pocket UI (touch padding above the bars), other
  GUI scales and servers are not checked.
- L-334 showing and hiding HUD elements: switching a Lamium element from the
  editor's tag switch was not itemized; a settings file from before with Hide
  boss bars on is covered by unit tests only; Pocket UI, other GUI scales and
  servers are not checked.
- Fixes from the 2026-11-14 code review (`d4e36406`..`5359af0c`): four passed
  in game on `5359af0c` (VALIDATION-LOG 2026-11-14: category reset of the
  game parts' placement, faded icons and counts, Map size 50, the schematic
  menu click). Not looked at: an older file's death point with a full waypoint list; a hidden chat,
  scoreboard or boss bar under a clipping panel (needs a resource pack whose
  HUD clips them). The Status element's switch hiding the block restriction
  lines passed on `5fa24a6b` (VALIDATION-LOG 2026-11-14).
- Four decisions of 2026-11-14 (L-338) passed in game on `e92d2398`
  (VALIDATION-LOG 2026-11-14): Toggle toasts' switch, Esc returning to where
  a page was opened from, the anvil setting and the Placement mode row, Item
  details unchanged. Not looked at: Item details on an unverified game
  version (cannot be brought about on the verified one).
- The death layout's time limit (L-338): not checked in game.
- Durability HUD's glint on enchanted items passed on `fc83b806`
  (VALIDATION-LOG 2026-11-14), also faint on a hidden element.
- Durability HUD and Inventory HUD draw an item with the frame a slot would
  use: the crossbow's states passed on `df963770`, and a second crossbow
  in the inventory keeping its look while the held one is drawn on
  `761b612e` (VALIDATION-LOG 2026-11-14). Not looked at: a bow's pull, a
  compass or clock.
- Server data names (L-343): a server joined before the change still shows
  its map and waypoints, the log names the key and `config/servers.json`
  is written (passed, VALIDATION-LOG 2026-11-14). Not
  checked: a friend's world keeping its data over a rejoin (none was
  available).
- Saved data page (L-343 part 3): the list, switching at once and back,
  the choice over a rejoin, the first map unchanged after exploring under
  another data, Delete and none for the data in use, and the data of
  servers and local worlds joined before all passed (VALIDATION-LOG
  2026-11-14), and so did the Open button and child rows, the any-port
  switch releasing other ports with one port given other data afterwards,
  the marks naming the reason and same-named rows told apart (`16330f1f`).
  The notice of fuller data passed on `f5f0dc78` and `9b709e81`
  (VALIDATION-LOG 2026-11-14): the Waypoints screen, the Schematics
  screen's Placements, Verify and Materials tabs and the world map, whole
  words and all three buttons, "Do not show again" over a rejoin. Not
  looked at: docked panels. The map picture on the Saved data page passed
  on `5970a399` (VALIDATION-LOG 2026-11-14). Built only: the page staying
  open when the window loses focus.
- Changed since 0.6.0 with open cases listed in VALIDATION.md or their
  items: Enchantment order (L-170: Survival, the 40-level refusal, an unbound
  key, servers), Item details (L-319), death waypoints (L-328, L-332), shared
  toasts, Fake Offhand's own key (L-322), Connected Textures panes (L-331),
  and the features that lost the Experimental badge.

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

### L-343 A server's map, waypoints and placements are lost when its address changes
Kind: Bug. Research for the identity, then Design for moving existing data
(it touches saved data: agree before building).
Reports: Discord 2026-11 (after leaving a Realms server and reconnecting,
the explored map is gone and its file is back to 0 MB); the maintainer
(servers using a proxy plugin that makes even the vanilla client treat each
visit as a new server: schematic placements, waypoints and the map are
gone).
Cause in the code: a server's data lives in a folder and file named after
the address and port of the connection (`resolve` in
`src/features/map/WaypointSession.cpp`, `serverFileName` in `Waypoints.h`).
Seen in the maintainer's own config folders 2026-11-14 (host names, IPs,
ports and level ids in this item are stand-ins: the servers are private):
`play.example.net_19133` beside `http___play.example.net_19133_19133` and
`http___198.51.100.20_19133_19133`, and for a local server
`127.0.0.1_19132` beside `http___127.0.0.1_19132_19132`: the same port
under three forms of the address. Other ports of the same host are there
too (`_19134`, `_19135`), but those may be different servers: several
servers on one address differ only by port (maintainer 2026-11-14, their
own setup does this). **A different port is never taken as the same
server by rule.** Three things change the name:
1. The address sometimes arrives as a URL (`http://host:port`), which is
   turned into `http___host_port` and gets the port appended again. This
   part is deterministic: strip the scheme and the port before naming.
2. The port changes between visits of one server (Realms, networks that
   hand the player on to another port). Not told apart from two servers on
   one address by the address alone.
3. The name is sometimes the IP address instead of the host name.
Nothing is deleted: the earlier data is still under its old name, which is
also the workaround today (rename the folder and the `.json` files in
`mods/Lamium/config/map`, `waypoints`, `schematics`, `death-layouts`).
Open:
- What identifies a server when address and port do not (2, 3): something
  the server sends (the level's id or name), a Realm's id, or a name the
  player gives. Find what the client has at join time and whether it is
  stable across visits; two servers must never share data by accident, so
  where nothing reliable exists the player chooses and the default stays
  address and port.
- What happens to the folders that exist: merge by rule, or let the player
  pick ("use the data of ..." on the map or Waypoints screen), which also
  covers the cases no rule can decide.
Research 2026-11-14, from the SDK headers (nothing seen at runtime yet).
What the client holds at join that could identify a server:
- `Social::GameConnectionInfo` (already read for the address): `mType`
  (LAN, platform, Xbox Live), `mTransportType` (RakNet or NetherNet),
  `mHostIpAddress`, `mUnresolvedUrl`, `mPort`, `mServerRegion`,
  `mExternalServerName`, `mRakNetGUID` (the server process's id; expected
  to change when the server restarts), `mNetherNetId`.
- `mNetherNetId` is a variant with a **Realms** form holding a UUID
  (`NetherNet::Realms::NetworkID`): the likeliest stable id for a Realm,
  whose address and port change per visit. To be confirmed in game.
- From the server's start packet, through the level: the level id
  (`Level::getLevelId`), the level name (`LevelData::mLevelName`) and the
  seed. A server software or proxy may send the same default for every
  server, or a new value per visit, so none is trusted before it is seen.
- The start packet's telemetry ids (server, scenario, world, owner) and
  the featured-server info are untyped in the SDK; not read.
Trace: `xmake f --server_trace=y` logs one line per join, "Waypoints:
identity: ...", with all of the above (`traceIdentity` in
`WaypointSession.cpp`). The line names the server, so a trace log is not
for sharing as it is. Collect, each twice with a reconnect in between: a
Realm; a server behind the proxy plugin; an ordinary server by host name;
the same by IP; two servers on one address with different ports; a local
world.
Seen 2026-11-14 (a `server_trace` build, 1.26.60.05 Dev, the maintainer;
each joined twice; no Realm was available):

| Joined | type | host | url | port | level id | level name | seed |
|---|---|---|---|---|---|---|---|
| Local world | 0 | 127.0.0.1 | empty | 0 | random-looking | world name | real |
| A friend's world | -1 | empty | empty | 0 | random-looking | world name | real |
| Server reached by transfer (three backends of one network) | 1 | the IP | `http://<IP>:<port>` | per backend | per backend (`lobby`, `local_world`, `farm`) | per backend | 0 |
| A featured server | 1 | its IP | `https://<name>:19132` | 19132 | the network's name | the same | 4294967295 |

Every value was the same on the second visit. `mRakNetGUID` was always
unset, `mNetherNetId` always empty, `mTransportType` "unknown" for every
remote join: none of them identifies anything here. What follows:
- **A friend's world has no address at all**, so `resolve` returns nothing
  and its waypoints, map, placements and death layout are never saved. Its
  level id was stable and looks unique, like a local world's.
- **The url carries a scheme and the port**, which is where the
  `http___..._<port>_<port>` names come from. The host part is the name the
  player typed for a direct join and the IP after a transfer, so one
  backend gets two names depending on how it was reached; `host` is the
  resolved IP in both cases.
- **The level id tells this network's backends apart** and stayed the same,
  but it is whatever the server's configuration says (`local_world` is a
  likely default elsewhere), so it cannot be the identity on its own.
- A port that changes between visits was not seen in this session.
Agreed by the maintainer 2026-11-14, with the remark that this network
also announced a changed server address lately, so not every second name
comes from a transfer.
Built 2026-11-14, not checked in game (parts 1 and 2 of four):
1. No address (a friend's world): the data is saved under the level id
   (`world_<hex of the id>` in the config folders). Nothing was saved
   there before.
2. With an address: `config/servers.json` records, per server, the key its
   data is under, the names it was joined by, the IP it was last found at,
   the port and the last level id (`ServerIdentity.h`, tested). A join
   belongs to a recorded server when the port is the same and it was joined
   by this name before, or it is at this IP now, the level id does not say
   otherwise, and no name says otherwise (the join has no name, as after a
   transfer, or the record has none yet). A recorded server keeps its key
   whatever its folders hold later. Only when a join brings something new
   (a server not recorded, a new name or IP of a recorded one) are the
   names its data may be under compared (the forms seen above), and the one
   holding the most is used: its documents first, then the size of its map.
   Names passed over are remembered and not compared again. A new server is
   named `<host>_<port>`, without scheme. Nothing is merged, moved or
   deleted; the log says "this server's data is under '<key>'" at each join.
What this does not do yet:
- A server reached only by IP does not find the data it has under its host
  name, because nothing has told Lamium that the name and the IP are one
  server; a join by the name does, once. Part 3 is for this.
- One key carries all four kinds of data. When the name with the waypoints
  and placements wins over the name the map was explored under, that map
  is no longer shown (it is still on disk). Decided 2026-11-14: one key
  stays for all four kinds; the map is not chosen on its own, and part 3
  is how the player picks another name.
- A world joined without an address that sends a plain name as its level id
  would share data with any other sending the same. Not seen; a Realm is
  not seen at all.
Open, to agree before building:
3. A different port, or a name and an IP nothing has linked: the player
   says "use the data of ..." on the Waypoints or map screen, listing the
   names passed over and those with the same level id first. Where it
   sits and how it reads. Mockup, second version 2026-11-14, awaiting
   review: [demos/server-data.html](demos/server-data.html). Decided by
   the maintainer on the first version (2026-11-14): the entry sits under
   General, not World map, with hints on the screens it affects while
   their data is empty; tags tell rows apart, no group headings; the
   sidebar stays; a choice takes effect at once; local worlds and friends'
   worlds are listed too, to see what they hold and how large it is.
   Asked for: "use this data for any port of this address" (for a Realm,
   if its port changes per visit), and a preview (map thumbnail, waypoint
   names) so a person can tell whose data it is.
   Decided on the second version (2026-11-14): the features that keep data
   get a small link row in the settings table, always shown; the notice on
   the tool screens shows while a candidate holds more than the data in
   use, not only while empty (the map is recorded from the first moment),
   and its dismissal reads "do not show again"; the port option is the
   shared switch, not a check box (DESIGN "Components"), shown for any
   server's data so it can be switched off later, above "use this data";
   delete is wanted, with a thorough confirmation; the thumbnail is the
   Overworld surface around where the player last was. The third version
   draws the tool screens in their real layouts.
   Built 2026-11-14, not checked in game: General > "Saved data" (a row
   that names the joined server's data and opens the page; the page keeps
   the sidebar, lists servers', friends' and local worlds' data with tags,
   and shows the selected one's waypoints by name and color, placements,
   map size, last use and level id); "Use this data" re-attaches the
   joined server at once (`useData`: the waypoints reload and the map,
   placements and death layout follow the place as at a join; the map
   store first saves what it holds to the folder it was using); the
   any-port switch (`Entry::anyPort`, a record of the exact port still
   wins); Delete, pressed twice, naming the waypoints and the map size,
   never for the data in use, on a row of its own so that arming it moves
   nothing.
   After the first game check (2026-11-14, maintainer): the row has the
   shared Open button (an unbound "Open the saved data" action) and a
   child row naming the data in use, as the mockup had it; the same child
   row sits under Waypoints, World map and Schematics ("Saved data ...
   Choose >"), which is the link row of the second version. Switching the
   any-port switch on now releases the address's other recorded ports, so
   they follow it at once (it did nothing for a port that had its own
   record); the data they used stays a candidate, and a port given other
   data afterwards keeps it. The row's mark says why it is a candidate
   ("Same level ID", "Same address", "Used before") instead of one word,
   and the note no longer repeats it. The switch is at the right of its
   words, as in the settings rows. Rows that read the same carry what
   tells them apart (a server's last use, a local world's folder name).
   Built after the second game check (2026-11-14), not looked at: the
   notice (`ui/DataNotice`). While another data that may be the joined
   server's holds more waypoints, placements or map than the data in use,
   the Waypoints screen, the Schematics screen's Placements tab and the
   world map say whose and how much, with "Use this data", "Saved data"
   and "Do not show again" (kept per server, `Entry::quiet`). It is worked
   out at a join and after a change, not per frame. On the two list
   screens it is a band between the toolbar and the list
   (`ShapesLayout::fit`'s `notice`), docked panels included; on the world
   map it lies under the top bar at the left and takes only its own
   presses. After its game check (2026-11-14): in the list screens' narrow
   column the words were cut and the third button did not fit, so the
   notice now takes the height its words and buttons need (the buttons
   wrap to another row; the screen leaves the room measured at the last
   draw); and it shows on the Schematics screen's Verify and Materials
   tabs too, which show what is placed.
   The map picture, built 2026-11-14 and not looked at: the details show
   the Overworld surface, three regions across at eight blocks to a pixel
   (`MapThumbnail.h`, tested), with the data's Overworld waypoints on it
   and their names beside it. It is centered on the region written last:
   where the player last was is not recorded for data that exists, so the
   newest file stands in for it. A region is read per frame
   (`ui/SavedDataThumb`), so a large map does not stall the page; data
   without an Overworld map says "No map".
4. Whether folders that turn out to be one server are ever merged (waypoint
   lists could be; map tiles less obviously), or only chosen between.
Still to see: a Realm; a server whose port changes between visits.

### L-335 Rows under a switched-off feature are dimmed only in the middle
Kind: Ready. Maintainer 2026-11-14 (`5359af0c`, Durability HUD off): the
dark band over the child rows starts right of the tree guide and ends before
the value column, so the left edge and the values on the right ("Bar and
num...", "Edit in HUD layout >") stay at full strength. DESIGN "Settings
screen": rows of a switched-off feature are drawn dimmed. Fix: the dimming
covers the whole row, guide and value column included, in every feature.
DESIGN says command rows are not dimmed; whether the HUD layout link counts
as one is the only open point. Files: `src/ui/SettingsTableView.cpp`. Check
in game on one feature with a choice row and a layout link.

### L-339 Smaller bugs found by the 2026-11-14 code review
Kind: Ready (each is narrow; verified against the code, none seen in game).
- A toast raised while the settings screen is open is never drawn (version
  copied, saved map deleted, a waypoint added from the map's prompt); the HUD
  draws only while the screen is closed (`SettingsScreen.cpp`, the HUD call).
- Help text of Hide effects, Connected Textures and Skip server packs still
  says "Experimental." in all four languages after the badge came off.
- A death's waypoint does not advance the next-color counter (`Waypoints.h`
  `recordDeath`).
- The mode-cycle toast reads "Auto Attack > Mode" (`Actions.cpp`).
- The retired boss bar placement action is still matched and consumes its
  key.
- The waypoint prompt's primary button has no hover state; the Schematics
  Verify tab uses a text ">"; a right click acts as a left click on the
  header and the sidebar; a distance of 0 reads "0 blocks".
- Transfer labels with two colons lose the action name in English
  (`splitLabel` splits at the first ": ").
- Inventory HUD: a compass and a clock do not show their needle and time
  (maintainer 2026-11-14 on `b709804b`; the crossbow, a bow's pull and a
  cast fishing rod do). The frame is asked the same way as for the offhand
  slot, so first see whether the offhand slot shows them, then how the two
  draw paths differ (`ItemIcon.cpp`: batch or direct).
- The suspicious stew's effect line is left out when the tooltip already
  contains the effect's name anywhere, for example a stew named "Speed
  stew" (`Inspection.cpp`).
- The toggle toast's switch does not follow the element's scale (18x9 fixed
  in a `14*zoom` row), and HUD cards pad `5`/`3` fixed in some elements and
  `5*z`/`3*z` in others (`InfoHud.cpp`).
- The death place is a 3D distance: after a death high up the items fall
  farther than 32 blocks and the layout never starts (`DeathLayout.h`).
- With the default scope, picking up only main-inventory items does not
  start a layout, though the help says the first pickup there does.
- The world map draws a hidden death waypoint at full strength; the
  Waypoints screen dims it.
- Six screen-opening actions are missing from `opensMenu`
  (`CustomInput.cpp`).
- Enchantment order is not in `paused::requested`, so the join notice does
  not name it on an unverified game.

### L-342 Lower findings of the 2026-11-14 review, kept for later
Kind: mixed; nothing here is agreed work. Listed so the review's result is
in the repository (its working notes were not committed). "Seen" means read
in the code; the rest was reported by a reviewer and not checked. Pick one
up through its own item when it is to be worked on.
Settings table:
- Seen: the sidebar's "Schematics" is both a section and a pinned opener;
  the Map page has two switches named "Waypoints"; Debug View's "hide
  while shown" rules sit under two headings in four wordings; Debug View
  alone still lists text shadow and background; in-world aids are split
  between Camera & view and HUD & overlays; `featureSection` is an if-chain
  whose miss falls silently into HUD & overlays; a distance of 0 means
  unlimited; sidebar counts skip features whose switch is a child row;
  session features (Zoom, Freelook, FreeCamera) never dim their child rows;
  Game HUD's parts are ordered differently in the rows and in the editor.
- Reported: sibling rows are named unevenly ("Show X" beside a bare noun,
  six ways to say a distance); small integer ranges are a stepper in one
  place and a slider in another; README and FEATURES place Toggle toasts
  and Book paste in other sections than the screen does.
Screens (beyond L-341):
- Reported: about nine small-button variants, two Close buttons in separate
  code; tooltip and popup panels at opacities from .92 to 1.0 and panels
  from .8 to 1.0 (DESIGN says .8); the three docked screens and the three
  list-replacing screens are copies of each other, one footer line off by
  one unit; the world map builds its hit list while drawing and has no
  layout header; its side list has no scroll bar; five ways to show a
  passing notice with no rule for which; waypoint marks drawn by three
  copies of the same code; chat, scoreboard and boss bar placement are
  three near-identical files.
- Reported, behavior: a waypoint's name is written only when editing ends
  while a shape's is written at once; Tab stops on world-only pages outside
  a world; a key of a switched-off feature is silent for some features and
  toasts for others; the block highlight list confirms a removal on the
  left and removes at one click on the right; out-of-range numbers are
  reported in some fields and dropped silently in others; HUD layout does
  not report a failed save; Esc in a search field does three things;
  Shift for 10x and right click for less exist on some steppers only; the
  header says "Close (Esc)" on pages where Esc goes back; reset and "find
  by key" have no keyboard path; a right click acts as a left click on tool
  buttons, arming and running a removal; the tool screens' top-right switch
  is the feature's switch but reads "Show all"; focus loss closes the
  language list while a drag or another sub-screen goes on; HUD layout's
  idle panel lets clicks through to the element behind it.
Enchantment order (also ENCHANT-ORDER.md "Remaining" in L-170):
- Reported: the hover split of "needed at once" and "work penalty" does not
  match the drawn text; an off row has a dead area where - and + would be;
  the level's warning color compares with the plan's most expensive step,
  not the next one; the planner takes no key but Esc; an item past 10 anvil
  uses reads "every order is refused" even in Creative; the plan follows
  stacks without looking at their anvil uses; a plain or single-enchantment
  book in the hand cannot be told from a fresh one; the planner runs twice
  per change inside the draw event; clicks keep hitting the cached frame
  for 300 ms after the anvil closes; a book-limit warning stays in the
  settings footer after leaving; its tooltip layer depends on Inspection's
  hook having started.
Other:
- Reported: a hidden game HUD part comes back differently per part on focus
  loss or an unverified game; toast grouping is per queue run, not per key
  press; the older death point's conversion runs in the HUD frame even with
  Waypoints off; GAME-UPDATES.md lacks some newer reads (the hover cursor
  position, `bundle_cursor_tooltip`, control names the anvil code reads)
  and still calls boss bar placement unchecked; the glint draw changes the
  shader color without a gate; `IconAt::direct` draws before the batches
  though its comment says on top; the literal 16 for the number of HUD
  elements is repeated instead of taken from the enum; `RawMaterials.h`
  treats an item after a loop as part of it (unused feature); pure geometry
  left in `.cpp` files without tests (step row lookup, hint placement, the
  editor's tag and switch split); `zh_TW` mixes two spellings of "plan";
  comments in `EnchantPlan.h` and `EnchantOrderGame.cpp` describe earlier
  states; the debug world probe keeps a `ScreenView` pointer across frames
  (trace build only); Fake Offhand's own key with eating, a bow, a shield
  or a trident is not checked in game.
- AGENTS.md says not to open other mods' source without exception, while
  PROVENANCE.md records a permission decided 2026-11-14 for one project:
  AGENTS.md should point at that exception.

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

### L-338 When a death layout ends
Kind: Feature, built 2026-11-14 on the maintainer's go-ahead; not checked in
game. The last of five choices from the 2026-11-14 review (the other four:
DESIGN.md "Components", ENCHANT-ORDER.md, ITEM-DETAILS.md).
Rule (DESIGN.md, L-338): a layout also ends once the place of the death has
been loaded around the player for 5 minutes in total, because the dropped
items are gone by then. "Loaded" is the death's dimension and 48 blocks
across (inside the smallest simulation distance, so the count never runs
ahead of the items); away from it the count stands still. The count is kept
in the layout file (`loadedTicks`, saved every 10 s), runs with the feature
switched off too, and waits while a restore is moving items. Ending shows
"Death layout restore ended: the dropped items are gone by now"; the help
text explains the limit. Pure part: `age`, `loadedAround` in
`DeathLayout.h`, tested.
Remaining:
- In game: stay near a death for 5 minutes without picking up (the toast,
  the file removed); go far away and return later (still restores); rejoin
  in between (the count continues).
- Accepted for now: on a server other players can keep the chunk loaded
  while this player is away, so the items can be gone with the count
  unfinished; the layout then ends at the next death, as before. A second,
  long limit is not built.
- Not changed: a started layout is still dropped on the first tick after
  joining more than 32 blocks away (the L-332 rule). L-240 stays open until
  the maintainer sees whether the unexpected restores stop.

### L-336 Durability HUD: choose its items in HUD layout
Kind: Design. Maintainer 2026-11-14: Info HUD's lines are chosen in the HUD
layout editor, while Durability HUD's items (offhand, armor, elytra, ammo)
are only switches in the settings table. Make the two work the same way.
Open: whether the editor's lines popover lists them (and their order), and
whether the settings rows stay.

### L-337 Minimap: one size setting instead of two
Kind: Design. Maintainer 2026-11-14: the minimap's side is "Map size"
(10-50 % of the screen height) times the element's HUD scale, so two
settings change its size, unlike every other element. Open: which one
stays (scale only, with the map size folded into it and saved values
converted; or Map size for the map and the scale for its text only, as
the comment on `Settings::Map::size` already says).

### L-340 Settings grouping
Kind: Design. Maintainer 2026-11-14 (noticed before the review, confirmed by
it): General is a mixed bag; HUD-wide settings (Toggle toasts, HUD text) sit
away from the HUD section; hiding the paper doll is reachable from three
places and offhand settings from five. Moving a feature between sections is
one table (`featureSection` in `SettingsRows.h`) and does not touch saved
settings. Output: the section list and each feature's place.

### L-341 Shared widgets for repeated controls
Kind: Refactor (strong model; no behavior change except where copies
differ). The 2026-11-14 review counted separate implementations of: the
press-twice confirm button (8, the danger red written as a literal in 7
files, HUD layout arming yellow instead), the text field frame (8), the
stepper (7 variants) and the scroll bar (5). Also differing by screen: Esc
while typing commits in the tool views and discards in the world map; "add
waypoint here" behaves three ways (prompt or not, camera or body position
under FreeCamera, the temporary setting). Move each control into
`src/ui/Widgets.h` one at a time, with the differences listed and agreed
first.
Raised by the maintainer 2026-11-14, for the same pass (each is a choice
to agree, not a fix to make on sight):
- A row that opens a page does it two ways: "Game language" shows its value
  as a button and the whole row opens the list; the calculator, the world
  map and Saved data have the shared Open button. Saved data was first
  built the first way and changed to the second after its game check.
- The primary button has two looks: HUD layout's "Done (Esc)" is a green
  border on the plain fill and is used nowhere else; "+ Add here", "Use
  this data" and the notices' first button are a green fill.
- Yellow means two things: a warning elsewhere (warning text, the saved
  data notice's frame, a candidate's mark), and in HUD layout the frame
  and legend of the game's own HUD parts, which is not a warning.
- Losing the window's focus closes the Game language list; the Saved data
  page stays open since 2026-11-14 (the maintainer: going to another
  window and back should not lose the page); the other pages were not
  looked at.

### L-330 Radar: mob icons that draw wrongly, then icons by default
Kind: Research, then Design. Maintainer 2026-11-13: some mobs' icons on
the radar do not draw correctly, so the radar keeps its Experimental badge.
First find which mobs draw wrongly and why (`features/map/RadarFaces.cpp`;
the earlier note "L-86 Radar faces follow-up" under Later lists mobs
without a face). If the icons can be made right, the maintainer wants the
radar's default changed from dots to icons (`map.radarFaces`, off today);
that default is decided once the icons are confirmed in game.

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
Kind: Feature, built 2026-11-13 after 0.6.0 (not in a release yet) and
checked in game by the maintainer; what remains is Design. Maintainer's
note 2026-11-07 (multiplayer-session feedback).
Goal: an item's tooltip tells what the client knows and the game does not
print: the anvil work penalty with its prior-use count first, then a
suspicious stew's effect, property values, the ID, tags and NBT.
Current behavior, the decisions in force and the dated build record:
[ITEM-DETAILS.md](ITEM-DETAILS.md). Runtime coverage: VALIDATION.md.
Remaining:
- Open choice: the enchantability wording ("Enchantability: 15"), if the
  setting's help text is not enough.
- Not checked in game: the Item details capability added 2026-11-14 (tags,
  NBT, property values, the ID and the stew's effect left out on an
  unverified game version).
- Open choice: on an unverified game version the work penalty line goes
  missing under the `EnchantOrder` capability, which belongs to
  Enchantment order, so nothing marks Item details as paused.
- Open choice: the main switch's help says off leaves the game's own
  tooltip, but "Hide shulker box contents text" (Container previews)
  still applies. Reword the help or make the hiding follow the switch.
- Not built, each needs a source or a decision first: the anvil repair
  material, firework detail the game does not print, grindstone
  experience return. The maintainer's direction 2026-11-13 is "as much
  about an item as can be shown"; no list is decided.
- Small fix: "Show NBT" serialises each of the first 24 entries whole
  before cutting it to 60 characters; the locked-trades tip asks for the
  text every frame. Cap the serialisation or remember the text.
- GAME-UPDATES.md's Inspection entry does not list the
  `minecraft:use_modifiers` component or `getCooldownDuration`.
- Not checked in game: a settings file from before the split, a cost
  that is not 2^n - 1, the Work penalty toggle key, servers, an
  unverified game version.

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
Kind: Feature, Experimental; built in steps on `main` 2026-11-12..13 after
0.6.0 (not in a release yet) and checked in game round by round by the
maintainer; what remains is checks and Design. Requested 2026-10-12.
Goal: given an item and the enchantments wanted, show the order of anvil
combinations that costs the least experience and avoids steps the anvil
refuses as too expensive.
Current behavior, the decisions in force and the dated record of every
round: [ENCHANT-ORDER.md](ENCHANT-ORDER.md). Read its "Current behavior"
first; the record is not needed to pick up work.
Remaining, runtime checks (VALIDATION.md's row has the same list):
- Survival: the 40-level refusal, the level beside the total and its
  warning colour.
- The unbound key; other GUI scales and narrow windows; servers; a plan of
  books alone at the anvil; the fallback on an unverified game version.
- The cost-differs warning and the result slot's yellow: could not be
  brought about.
Remaining, open choices (not decided; the maintainer chooses):
- Decided and built 2026-11-14, not checked in game: "List the steps
  beside the anvil" is off for a new install and is the switch for all
  three beside-the-anvil parts (no separate main switch was added: the
  planner only opens on request); Esc in the planner opened from the
  anvil's "Change" button or by the key closes the screen (back to the
  anvil or the game), and its Back button goes to the settings table.
- A slot item is taken as the plan's item when nothing is chosen and there
  is no plan, beside the decision "not taken from the left slot by itself".
- The plan is kept in memory across a world exit (read from the code, not
  checked in game); whether it should be cleared there.
- A stack carried on the pointer is not marked (left out of the first
  version: it flickered and did not fit the held size).
- Left out by decision, may come later: planning from the books carried,
  merging two books of one enchantment, a plain book as the item.
- Not built: dragging a scrollbar and text search in the anvil's choosing
  panel. `enchantOrder.heldUnknown` is translated but unused: a main-hand
  item whose anvil uses cannot be read is planned as fresh with no notice.

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
Trial 2026-11-14 (`8e00a4b1`, removed in the next commit): BEI's
item-icon path has the same structure as Lamium's and the same gaps (the
maintainer saw the enchanted shield, enchanted leather, recovery compass
and unloaded heads fail there too). Its three draw-context differences
(off-screen white quad flushed with `ui_flush`, zOrder 0, foil at the
icon's alpha without Lamium's shader-colour tint), alone and together,
drew no shield glint; flat items' glint looked the same in all of them.
BEI offers no lead for L-91.

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
