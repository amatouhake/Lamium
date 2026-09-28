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

Ready tasks marked **(strong model)** are fully specified but visual or
cross-cutting enough that a strong model should implement them.

A large or open-ended feature (Map, placement and breaking features, later
Schematic) starts with a conversation with the maintainer about what it
should be - purpose, scope, what is left out - before any spec, spike or
mockup (decided 2026-09-28).

**Bugs** (something that ships behaves wrongly) are listed first in their own
section and are fixed before new features. Each bug still has a kind that
decides who picks it up.

Finished and closed items live in [BACKLOG-DONE.md](BACKLOG-DONE.md) with
their full history. An `L-` number referenced elsewhere that is not in this
file is there.

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
  `Lamium-<version>-client-windows-x64.zip` (matching `tooth.json`); the folder
  inside stays `Lamium/`, and release notes name the asset the same way.
  Bedrinth already lists Lamium (seen 2026-09-28 with versions 0.1.1-0.1.3):
  it indexes public repositories with a root `tooth.json`, so there is no
  separate registration step. Managed install/update is tracked by L-65 and
  [DISTRIBUTION.md](DISTRIBUTION.md); it is not described as verified until the
  install/update preservation matrix passes.
- Large features may start at any time. They land on main in steps, default
  off and with the Experimental badge, so main stays releasable while they
  grow. A step that is not usable yet stays out of the settings screen (or
  behind a build option) instead of being shown half-working.

## Current execution order

Keep this section short. It is only the ordering layer; task details and status
live in the L-items below. If this summary ever disagrees with an L-item, the
L-item wins. Every entry names what the task is, not only its number.

1. **Map — L-60:** the next large feature, an experimental, default-off
   minimap with radar and waypoints, then a world map. The minimap spec and
   its steps are written; building waits for the maintainer's go.
2. **Placement and breaking — L-15 restrictions and L-59 held placement
   style:** specs and steps written after the 2026-09-28 discussion;
   building waits for the maintainer's go.
3. **Small and medium work alongside Map**, picked by the maintainer:
   - L-62 Stop held mining before the tool breaks (bounded Research check,
     then Ready; on by default, under Interaction): stop at 1 durability left,
     toast, a new press mines on.
   - L-63 Saturation on the vanilla hunger bar (Research, then Design):
     hidden saturation drawn on the hunger bar, gain preview while holding
     food.
   - L-64 Food values in the inventory (Design, small): hunger/saturation
     gain on hover, next to the durability readout.
   - L-53 More Info HUD lines, wave 1 (implemented; in-game check pending):
     real time, Nether-scaled coordinates, yaw/pitch, horizontal/vertical
     speed, sprinting, difficulty and localized biome name with optional id.
   - L-42 Hide visual effects (Research; spec decided): boss bars, rain/snow,
     particles, pumpkin/spyglass overlays and the nausea tint, display only.
   - L-61 Held-item durability HUD (Ready, strong model): bar and number by
     default, bottom left, offhand/armor options, elytra row while gliding;
     its own row under HUD & overlays.
4. **Research when convenient:** L-57 client counters (entities, chunks,
   particles), L-30 Ender Dragon part hitboxes, L-33 mob growth and breeding
   timers.
5. **Distribution — L-65:** validate Bedrinth/LIP discovery and managed
   install/update with settings preserved, then tighten release/CI checks.
   This can proceed independently of gameplay feature work.

Ideas that are not yet chosen (for example swapping out almost broken tools,
more inventory transfer gestures, Schematic and Mass Craft) stay in the maintainer's notes and enter this file once chosen.
Schematic and Mass Craft rank below Map because existing standalone tooling
and resource packs already cover part of them.

Task-picking rule: bugs first; otherwise work on what the execution order
names. Cheap models skip strong-model, Design and Research work. Follow the
L-item's dependencies and model/validation requirements. When a task is done,
update its status and relevant feature doc, then move it to BACKLOG-DONE.md;
do not duplicate task details into this summary.

---

## Open decisions

HUD (docs/demos/hud.html), the settings key and the shape model are decided;
see DESIGN.md.

---

## Bugs

None open.

---

## Ready

### L-61 Held-item durability HUD
Kind: Ready **(strong model)** except the elytra flight time, which needs a
short Research step first. Chosen by the maintainer 2026-09-28 from user
feedback (a UI pack's compact held-tool readout such as `1188/1561`; behavior
reference only).
Status: decided 2026-09-28 (look in
[demos/durability-hud.html](demos/durability-hud.html)); ready to build
except the flight-time research.
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
- Options, both default off: offhand (shield and other damageable offhand
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

### L-53 More Info HUD lines (wave 1)
Kind: Implemented; display refinements await an in-game recheck. Agreed with
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

---

## Design

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

### L-42 Hide visual effects without changing game state
Kind: Design done (2026-09-28); Research next, one render entry at a time.
Status: planning; the user-visible choices are closed and nothing is built.
One group of render-only toggles: boss bars, rain/snow, all particles,
carved-pumpkin overlay, spyglass overlay (zoom kept) and the nausea green
vignette (vanilla Screen Distortion already removes the warp). Weather,
effects, boss state and equipment are never changed. Research each effect in
its own backend category rather than looking for one universal hook: HUD
overlays, weather, particles, camera/media overlays and post-processing may
have separate paths. Version-sensitive renderer paths are capability-gated and
leave vanilla behavior unchanged when the expected contract is unavailable.
Ship effects one by one. Status-effect-only particle filtering stays an idea
until its source can be identified.
Decided 2026-09-28: a keyless group heading "Hide effects" under Camera &
view (beside Hide offhand, which is the same kind of feature) with one switch
per effect, each bindable without a default key. Particles start as a
single hide-all switch; per-kind choices come only once their sources are
identified.

### L-64 Food values in the inventory
Kind: Ready once L-63 settles the saturation marking. Chosen by the
maintainer 2026-09-28 alongside L-63.
Status: open.
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

### L-15 Breaking and placement restrictions
Kind: Design done (discussion with the maintainer, 2026-09-28); breaking is
then Ready **(strong model)**, placement needs Research first. Replaces the
current Breaking Restriction (capture/reset keys) and the unimplemented
placement mode.
Status: planning. Nothing is built, and no step starts until the maintainer
says so.
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

---

## Research

Diagnostics: `xmake f ... --research_trace=y` logs lines prefixed
"research L-3x/L-4x" for L-36, L-37, L-40 and L-44 (see
`src/features/research/` and the L-36 block in `BreakingRestriction.cpp`),
`research L-14` render call-site lines for the Hide Offhand shield path
(`HideOffhand.cpp`), the L-49 build-session trace (`FakeOffhandTrace.cpp`) and
the L-58 Target icon lines. Those items are in BACKLOG-DONE.md.

### L-65 Managed distribution and update-safe packaging
Kind: Research, then Ready. Chosen by the maintainer 2026-09-28.
Status: open.
Make Bedrinth/LIP a supported discovery and managed install/update path without
sacrificing user configuration. The package contract is
[DISTRIBUTION.md](DISTRIBUTION.md).

Current state:
- A LIP v3 `tooth.json` exists at the repository root and declares a Windows
  x64 client-only variant with the current LeviLamina Client range.
- Bedrinth already lists Lamium (0.1.1-0.1.3, install command
  `lip install github.com/amatouhake/Lamium#client@0.1.3`), so LeviLauncher
  users may already be installing and updating it through a path that has not
  been checked. Settings loss on those updates is the main risk.
- GitHub Releases remain the documented install path until managed
  install/update has been validated.
- Release archives intentionally exclude runtime-created `config/` and
  `logs/`; `scripts/Check-Package.ps1` enforces that boundary.

Steps:
1. Research how LeviLauncher currently installs and updates Lamium from
   Bedrinth (LIP or a plain ZIP overwrite of `mods/Lamium/`), then run a real
   clean install -> settings change -> managed update using two Lamium package
   versions (0.1.3 -> 0.1.4 is the natural pair). Confirm
   `config/settings.json` and explicit key bindings survive, while the
   DLL/manifest/notices update, and that LeviLauncher accepts the
   `LeviLamina#client` range for the instance. Repeat through LIP CLI when it is an
   intended supported path. Record actual uninstall behavior rather than
   assuming whether user data is kept.
2. If runtime-owned files survive naturally because they are not package
   assets, keep `preserve_files` empty. Add preservation metadata only if the
   real managed-update test proves it is required.
3. Ready (may be done before steps 1-2): add CI/package checks for version
   agreement across `xmake.lua`, `tooth.json`, the expected `v<version>`
   tag/asset convention and package layout. Keep the manual ZIP path usable.
4. After the managed path passes, update the README install section so
   LeviLauncher/Bedrinth is recommended, LIP CLI is the advanced path and
   GitHub Releases is the manual fallback.

### L-62 Stop held mining before the tool breaks
Kind: Research, then Ready. The control point exists; the bounded runtime
ordering check below decides where the stop belongs before implementation.
Chosen by the maintainer 2026-09-28 from user feedback.
Status: open.
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

### L-60 Map: minimap, waypoints and world map (experimental)
Kind: Design done for the minimap (step 0, 2026-09-28); the steps below are
Research then Ready **(strong model)**. The world map still needs its own
design discussion. Chosen by the maintainer 2026-09-28 as the next large
feature.
Status: planning. Nothing is built, and no step starts until the maintainer
says so. The requested placement/breaking discussion is complete (L-59/L-15).
A client-side map built from the chunks the client has loaded: a minimap HUD
element with a radar and waypoints first, then a full-screen world map backed
by an on-disk cache. It ships default off with the Experimental badge and
grows on main in steps (Release policy above). Look agreed in
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

#### Minimap spec (decided with the maintainer, 2026-09-28)
Map
- Square, north up; the player is a white arrow with a black edge, modeled
  on the vanilla map's player marker (check whether the game's own map icon
  can be drawn at runtime). Rotating (heading up) and round are settings.
- About 128 x 128 blocks by default; zoom steps from about 32 to 512 blocks
  across. Chunks the client has not loaded stay blank. Nothing is requested
  from a server.
- A thin 1-unit frame only; nothing is drawn outside the map.
Terrain
- A representative color per block, biome tints for grass, foliage and
  water, and height shading against the north and west neighbors, at the
  strength shown in the mockup. No day/night darkening.
- Under a ceiling the map switches to a cave view on its own: floors near
  the player's height bright, walls dark. The Nether always uses it. A key
  can force the cave or surface view.
Radar
- Simple dots by kind, each kind a setting: other players (light blue, with
  their name), hostile mobs (red) and passive/neutral mobs (white) on by
  default; dropped items (yellow) off. Every dot has a thick black ring.
  Dots 8 or more blocks above or below the player are drawn fainter.
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
Text, placement and settings
- Optional lines below the map with a shadow, all default off: coordinates,
  biome, compass letters (N E S W). No clock.
- Its own HUD element, default top right at a medium size (about a fifth of
  the screen height), movable and scalable in the layout editor. Hidden while
  Debug View is shown (a "hide while Debug View is open" switch like the Info
  HUD's and Target's); not hidden while zooming or in FreeCamera.
- A new settings category "Map" holds the minimap, radar and waypoint
  options (and later the world map), with the Waypoints screen pinned at the
  bottom of the sidebar.
- Bindable actions without default keys: minimap zoom in/out, minimap
  show/hide, add a waypoint, force the cave/surface view. New actions are
  appended to `enum Action`.

#### Steps
Each step lands on main behind the default-off switch and ends with an
in-game check by the maintainer. Pure logic goes in headers with tests.

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

#### World map (later; design discussion first)
Follows the minimap. Open for its own step 0: how it opens and is
controlled, the on-disk region cache (location, size, clearing), how
waypoints are edited from it, and what else it shows. Discussed after the
minimap spec above is built or when the maintainer asks.

### L-63 Saturation on the vanilla hunger bar
Kind: Research, then Design. Chosen by the maintainer 2026-09-28.
Status: open.
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
Open (after research, with a mockup): the exact gold, the outline width, and
the default.

### L-57 Client info counters
Kind: Research. Split from L-53 on 2026-09-27 (wave 2).
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
- L-26 FreeCamera flight speed (parked, after L-18). Currently fixed at
  20 blocks/s. Add a user-facing speed setting with sane
  bounds; decide on a fast-flight modifier, if any, at design time.
- L-28 Third-person underground camera (parked, after L-18). While detached
  underground, vanilla collision avoidance fights the pivot offset and the
  view judders block by block. Decide whether to soften avoidance while
  detached or document it as a limit; above-ground flight is unaffected.
  Moot while FreeCamera locks first person.
- L-29 Hide the hotbar while detached (parked, after L-18). Requested
  2026-09-24: an option to hide the hotbar while FreeCamera is active
  (looking-only flight needs no hotbar). Find the vanilla hotbar render entry
  first; Freelook is out of scope unless trivially shared.
- L-21 Shape color picker or more colors: only if the four colors prove
  insufficient.
- L-37 FreeCamera cannot see caves from underground: parked as a known
  limitation (README known issues; traces in BACKLOG-DONE.md).
- Not started, not yet triaged: Schematic subsystem (browser, placement,
  projection, verifier, material list), Mass Craft. These
  need a Design pass before they become tasks. (Fast Attack/Use became L-34;
  Scroll Transfer became L-41.)
