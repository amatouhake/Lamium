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

1. **Schematic screen review — L-93 step 1** (decided 2026-10-08): placement
   frames, Placed/Files/Check/Materials list changes, chest/stack amounts,
   missing items and the ResourceCalculator button, entity ghosts as outline
   and faces. Then the research items L-114 3D previews, L-115 entity models,
   L-116 raw materials from the game's recipes. The L-111 integration
   proposal waits for the maintainer.
2. **Before the next release or 0.2.0 — L-111 integration between
   features:** the agent drafts a proposal in the current batch; the
   maintainer decides scope, risks and order.
3. **Small and medium features**, picked by the maintainer:
   - L-118 Hide distance fog (0.1.8).
   - L-121 Night Vision without dark corners (child option, default on).
   - L-90 Simplified Chinese localization: built and checked in game; waits
     for a native review of the wording.
4. **Placement and breaking — L-15 restrictions and L-59 held placement
   style:** L-15 breaking built and checked; the restriction plan is reopened
   for Design before placement. L-59 waits for the maintainer's go.
5. **Map — L-60 minimap, waypoints and world map:**
   core built and checked locally and on an external BDS. Runs in parallel
   with the small/medium features; neither ranks above the other. Open:
   waypoint server storage checks and L-86 radar-face follow-ups. Details
   are in the L-item and MAP.md.
6. **Schematic — L-93 load, place, project, verify and list materials:**
   included in 0.1.7 and checked locally. Choose the next accepted follow-up
   with the maintainer; the L-item lists them and SCHEMATIC.md retains the
   contract and build record. Server/broader coverage remains open.
7. **Research when convenient:** L-79 carved pumpkin and spyglass frame draw
   path (cheap-model friendly
   trace/test steps), L-71 starting a glide from the mod, L-57
   client counters, L-30 Ender Dragon part hitboxes, L-33 mob growth and
   breeding timers, L-96 Connected Textures (glass first; step 1 is the
   tessellator spike), L-105 performance profiling (measure before any
   optimization).
8. **Before a release:** the pre-release checks below. 0.1.7 was released
   on 2026-10-07; server checks of the 2026-10-06/07 work stay listed
   below as known gaps (Release policy does not require a full
   regression).

Ideas that are not yet chosen (for example more inventory transfer gestures,
an arrow-count HUD line, a fall-rescue elytra, Mass Craft) stay
in the maintainer's notes and enter this file once chosen.
Mass Craft ranks below Map because resource packs already cover part of it.

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

### Released builds and registry follow-up

The following are released-build checkpoints and listing follow-ups, not
unmet gates for versions already published:

- 0.1.7 was released on 2026-10-07 (`v0.1.7`, tag CI passed; asset SHA-256
  `2e0926e0...93b013b5`): L-93 Schematics (experimental), L-67 Weapon
  Switch, L-97 fixed fetch slot, L-94 offhand swap (`F`), L-95 Fake Offhand
  item use, L-102/L-103 Hand Restock threshold/order and inventory-screen
  transfer, L-104 map follow-ups, L-89 distant players, L-98 HUD density,
  L-99 Zoom below 2x, L-101 version display. The maintainer's smoke test
  passed on the release-ZIP DLL `2c3e7546...fd903806b` (`b3c6555`). No
  settings migration. After tagging: check that the registry PR picks up
  `v0.1.7` and that LeviLauncher/Bedrinth offer it. Schematic gaps (servers,
  other dimensions, large files, block entities from files, half-drawn beds)
  are in VALIDATION.md and the README's Known issues.
- 0.1.6 was released on 2026-10-02 (`v0.1.6`, tag CI passed; asset SHA-256
  `3d864ca1...3946554f`): L-90 Simplified Chinese (first AI-assisted
  translation, corrections welcome), L-88 target hearts, L-75 offhand slot,
  L-63 saturation, L-64/L-92 food values and durability inside the vanilla
  tooltip. The maintainer's smoke test passed on the release-ZIP DLL
  `86f5baf0...2587883f` (`b71c9f8`). No settings migration. After tagging:
  check that the registry PR picks up `v0.1.6` and that LeviLauncher/Bedrinth
  offer it.
- 0.1.5 was released on 2026-10-02 (`v0.1.5`, tag CI passed) at the maintainer's request,
  with the map (L-60, L-85, L-87) as its main change. The release build
  `6add9b9` (DLL `0fae1c58...dc657614c`, from the release ZIP) was deployed
  for a smoke test; no separate result was reported before tagging.
  After tagging: check that the registry PR picks up `v0.1.5` and that
  LeviLauncher/Bedrinth offer it.
- 0.1.4 was released on 2026-09-30 (`v0.1.4`, tag CI passed). Its smoke test
  passed on the release build `4d25424`: the 0.1.4 version, Hand Restock
  (L-66) and offhand totems (L-68), Tool Protection (L-62), Tool Switch fetch
  (L-69), Auto Elytra (L-70) and the L-02 dedicated openers. Still open:
  Hand Restock on BDS and with real latency. The coverage gaps below carried
  past 0.1.4; recheck the relevant ones before the next release.

### Pending feature checks

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
- If possible, a server with real latency for Hand Restock.
- Distant players on the map (L-89) and opaque player markers at any
  height: checked on phone/PC-hosted worlds with a trace build; check on the
  release build and a dedicated server.
- Weapon Switch (L-67, after 0.1.6): checked locally on `7b702da`; check on a
  server (the same-hit equipment packet) and on the release build.
- Offhand swap (L-94, F): checked locally on `856d79c`/`ed288b6`/`c7bb827`
  (every game mode with hands); check on a server that the screenless swap is
  not rolled back, and on the release build.
- Fake Offhand item use (L-95): checked locally up to `d93f04d`; check on a
  server (borrowed-slot reports, the firework hotbar echo correction with
  latency, no rollback), eggs, a non-mouse activation binding and the
  release build.
- Hand Restock threshold/order (L-102) and inventory-screen transfer
  (L-103): checked locally (`bd30648`, `97c44c6`, trace `163bb96`); check
  on a server with latency and on the release build.
- Map follow-ups (L-104): checked locally up to `ea70c4d`; check on a server
  (missing-section requests and their load, teleport through the command
  list) and on the release build.
- Fixed-slot fetch (L-97) and the stronger-weapon fetch: checked locally on
  `c7bb827`; check on a server (the same-hit selection report) and on the
  release build.

---

## Bugs

### L-121 Night Vision deepens the dark corners with Smooth Lighting on
Kind: Ready (small). Found by the maintainer 2026-10-09 as a bug; the same
day they showed it is the game's own behavior (VALIDATION-LOG).
Status: open. Decided 2026-10-09 (maintainer): a Night Vision child option
that removes the dark corners, **default on**; Lamium's Night Vision should
look fully bright by default. Off gives the vanilla look.
With Smooth Lighting on and Night Vision on, the corners and gaps that
smooth lighting darkens turn dark blue, in a wide ring around a hole in a
floor. The vanilla Night Vision effect does the same, also without
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

### L-119 Fence gates show no icon in the shulker box preview
Kind: Bug. Reported by the maintainer 2026-10-08.
Status: open, not investigated.
A fence gate inside a shulker box shows only its count in Shulker Box
Preview, without the item icon. Expected: icon and count. Find out whether
every fence gate kind does it and whether other special block items (doors,
signs, beds...) do too, then compare with how the inventory slot draws the
same item (see also L-91 for icons that differ from vanilla slots).

### L-117 Schematic entity name tags render badly in Japanese
Kind: Bug. Reported by the maintainer 2026-10-08 (L-93 checks); present
before the step 1 changes.
Status: mostly fixed 2026-10-08 (`47534d7`, checked in game); the rest is
accepted for now. World name tags stay only over entities drawn as dashed
frames (no model, L-115). Japanese glyphs come from glyph sheet 48 (type 3,
scale 1.333): the plate is now measured per glyph at its sheet's scale, and
non-ASCII sheets keep the font's own material with text constants scaled to
the on-screen size of a font pixel, so the colored fringes are gone. Left:
Japanese text draws dark gray instead of white (a white dark color brought
the fringes back). Maintainer: acceptable for now, looks less like a bug.
The world name tags over missing schematic entities show colored fringes and
look broken with the Japanese locale (screenshot in the conversation). They
are drawn in `GhostRenderer.cpp` `drawNameTags` with the "default" font and
the game's name tag materials. Starting points: compare with how vanilla
draws a named entity's tag in Japanese (font type, glyph texture filtering,
the text material) and with Lamium's waypoint labels in the world.

### L-113 Numbers sit higher than Japanese text in the settings screen
Kind: Bug. Reported by the maintainer 2026-10-08 while checking the change
arrow.
Status: open; waiting for a screenshot of where it shows.
With the Japanese locale, Lamium raises Latin runs (letters and digits) by
1.5 units (`latinRaise()` in `Widgets.cpp`, DESIGN.md) so they share the line
with kana and kanji. In the settings screen the numbers now read as higher
than the Japanese text beside them, which suggests the raise is too large
there (the change arrow, placed for Latin text, sat about 0.8 units above
the kanji). Changing the raise moves every Japanese label, so measure it on a
screenshot of the settings screen (and the HUD) before choosing a new value;
check stepper values, sliders, the key cells and the Info HUD lines.

---

## Feature work and research

### L-118 Hide distance fog
Kind: Ready. Chosen for 0.1.8 (maintainer, 2026-10-08), from a user
request ("no fog").
Status: open.
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

### L-120 Entity counts by kind in Debug View
Kind: Design. From the maintainer's notes (2026-10-09); extends L-57.
Status: open.
Keep the `E:` total and optionally break it down without double counting,
in this order: players, dropped items, hostile, passive, other, so the parts
add up to `E:`; optionally per identifier (`getTypeName()`, add-on entities
included). Same source and cadence as the L-57 count (the client's actor
list for the player's dimension, once a second). Dropped items count
entities, not stack sizes; block entities are not actors. Open: a separate
small panel or lines inside Debug View, sorting and how many identifiers.

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
Kind: Design done (discussion with the maintainer, 2026-09-28); breaking is
then Ready **(strong model)**, placement needs Research first. Replaces the
current Breaking Restriction (capture/reset keys) and the unimplemented
placement mode.
Status: step 1 (breaking) built 2026-10-07 with L-73 step 13 and checked in
game 2026-10-08 (VALIDATION-LOG). Implementation notes: RESTRICTIONS.md.
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

### L-90 Simplified Chinese localization
Kind: Design decided, then implementation. Chosen by the maintainer
2026-10-02.
Status: built 2026-10-02 (agent-drafted text for all keys, `TranslationsZhCN.h`
with a build-time order check, docs/TRANSLATING.md). Checked in game on
`ca25c2c` (fit, baseline and behavior fine; no Latin raise needed). Open: a
native review of the wording, invited from FeixiangTMC as a PR. It shipped
in 0.1.6 as a first AI-assisted translation with corrections welcome; the
native review remains open and is not a release gate.
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

### L-91 Icons Lamium draws differ from vanilla slots (shield glint, leather)
Kind: Research. Found by the maintainer 2026-10-02 while checking L-75;
leather added 2026-10-06 (found under L-61).
Status: parked 2026-10-06 after a trace and three in-game experiments; the
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

### L-96 Connected textures, starting with glass
Kind: Research **(strong model)** first, then Design for the settings.
Taken up 2026-10-05 after a public request.
Status: open; default off and Experimental when it ships.
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
Research output: the function(s) that can be intercepted on this game
version, whether the crop works for glass and panes, the cost on a large
view distance, and how it behaves with Vibrant Visuals / Deferred rendering.

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

### L-60 Map: minimap, waypoints and world map (experimental)
Kind: Design completed; implementation built. Remaining work is validation
and the separately listed radar follow-ups.
Status: minimap, radar, waypoints and world map built and checked locally
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

### L-93 Schematic: load, place, project, verify and list materials (experimental)
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
- Neighbor-dependent ghost shapes using schematic neighbors.
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
   Built 2026-10-08, not yet checked in game: placement frames (`273fb39`),
   the Placed progress column and selection bar, Files folders and columns
   (`e183d46`), Check filter counts and differing states (`926c9f6`),
   Materials sections, HUD switch, chest amounts, missing slots and the
   ResourceCalculator link (`8dd8457`), entity boxes with faces and every
   name shown (`98d9fe1`).
2. Research, after step 1 or alongside it: L-114 3D previews in the screen,
   L-115 entity models as ghosts, L-116 raw materials from the game's recipes.

0.1.8 rendering and compatibility work (plan 2026-10-09 in SCHEMATIC.md
"Rendering and compatibility plan for 0.1.8"): A saved-file version and a
vanilla-loader check, **done and checked in game 2026-10-09**; then B schematic
neighbors, render layers, liquids (previews too) and block entity data, C
event-driven rebuilds, D waterlogging in the check (the check stays
strict). B-D: Research (strong model), approach left to the agent.

Pick the next follow-up with the maintainer; the list is not an implementation
order. Chosen 2026-10-07: placement markers on the minimap/world map,
built the same day (look in SCHEMATIC.md) and checked in game 2026-10-08.
Deeper map integration (toggling and editing placements from the map) is
part of L-111. Runtime
gaps stay under Pre-release checks and VALIDATION.

### L-57 Client info counters
Kind: Research. Split from L-53 on 2026-09-27 (wave 2).
Status: first version built 2026-10-08 and checked in game the same day
(local; servers unchecked). Debug View
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

### L-112 Readable block state names in the target card
Kind: Ready. Chosen 2026-10-08 with the L-93 target-card redesign.
Status: built 2026-10-08 (`interpretBlockState`, tested) and checked in game
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

### L-114 3D previews in the schematic screen
Kind: Research **(strong model)**. Chosen 2026-10-08 (L-93 screen review).
Status: Files preview built and checked in game 2026-10-08 (`f2fbf49`);
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
Status: built and checked in game 2026-10-08 (`EntityModels.cpp`, rounds on
trace builds up to `ef9bc54`); move to BACKLOG-DONE when the known limits
below are accepted or split off.
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

### L-111 Integration between features (before the next release or 0.2.0)
Kind: Design. Raised by the maintainer 2026-10-08 after checking the
2026-10-07 batch; not chosen for building yet.
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
Status: agent's proposal written 2026-10-08 in [INTEGRATION.md](INTEGRATION.md)
(shared parts features register into: face drawing, map layers, a Lamium
radial menu, looked-at selection, settings cross-links; suggested order and
open questions). Waiting for the maintainer.

### L-109 Restore the death-time hotbar and inventory layout on pickup
Kind: Design decided 2026-10-08, then Ready **(strong model)**. Idea from the
maintainer 2026-10-07; chosen for building 2026-10-08.
Status: built 2026-10-08 (default off, Experimental); checked in game the
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

### L-105 Performance: find the real bottleneck before optimizing
Kind: Research **(strong model)**. Chosen by the maintainer 2026-10-07 from
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
- Not started, not yet triaged: Mass Craft. It needs a Design pass before
  it becomes a task (Schematic became L-93). (Fast Attack/Use became L-34;
  Scroll Transfer became L-41.)
