# Pre-release checks

The [Release policy](../BACKLOG.md#release-policy-decided-2026-09-28) requires a smoke
check of the actual release build: load, settings and changed features.
Choose relevant checks from these known gaps; a full regression and every
server case are not release gates. [VALIDATION.md](../VALIDATION.md) owns
current per-feature coverage; VALIDATION-LOG.md owns build/hash evidence.
Do not infer game success from a build or unit-test result.

## Since 0.6.0

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
  on `5970a399`, and on `03c2248d` the page staying open when the window
  loses focus, the "Used by" lines, the any-port switch as reworked, rows
  named by their own data and the data in use listed (VALIDATION-LOG
  2026-11-14). The notice in docked panels passed too. Left for when one
  is available (maintainer 2026-11-14): a friend's world keeping its data
  over a rejoin; a Realm.
- Changed since 0.6.0 with open cases listed in VALIDATION.md or their
  items: Enchantment order (L-170: Survival, the 40-level refusal, an unbound
  key, servers), Item details (L-319), death waypoints (L-328, L-332), shared
  toasts, Fake Offhand's own key (L-322), Connected Textures panes (L-331),
  and the features that lost the Experimental badge.

## Left from 0.6.0

The tag (VALIDATION-LOG 2026-11-12; release published the same day): the published ZIP, copied by hand,
loads and passed a short smoke; Bedrinth and LeviLauncher list 0.6.0
(VALIDATION-LOG 2026-11-12). An install through LeviLauncher or LIP is not checked.

Passed on `bb1cc4be` (VALIDATION-LOG): load, settings, Block highlight (L-320),
the HUD layout editor (L-321), Tool Switch's drop priority (L-318), a clean
exit on LeviLamina 26.60.4. Not checked: a settings file from 0.5.1 with only
Chat position on, which the release notes describe from the code; a label
with a zero-wide character aligned right or centered. Other open gaps are in
VALIDATION.md and the table below.

## Left from 0.5.1

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

## Gaps carried from released builds

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
| In-world language/server packs, L-167 | English Search and schematic ghosts after a change, broader servers/packs, rapid switching/hang risk and the 5 s interval (L-310). Cosmetic BDS evidence does not establish all pack types. See LANGUAGE.md. |
| Camera and visual effects | FreeCamera world-exit/death/controller/body-view and hold-input cases, speed restart persistence; Hide effects restart/keys/master, ambient layers, packs, graphics modes and lifecycle. Detailed coverage is in VALIDATION.md, CAMERA.md and VISUAL-EFFECTS.md. |
| Schematic ghost pick in creative, L-263 | A block not carried is given into the hotbar (empty selected slot, another empty slot, full hotbar), unlisted items (water, lava, structure block) do nothing, carried and survival picks unchanged; also on a server. |
| Distribution listings | 0.1.4–0.1.8 registry pickup is recorded; LeviLauncher 0.2.0 was seen; 0.5.1 is offered by the LIP registry, Bedrinth and LeviLauncher (VALIDATION-LOG 2026-11-07). Verify the version being released using DISTRIBUTION.md rather than reopening old release gates. |
