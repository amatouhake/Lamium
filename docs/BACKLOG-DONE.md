# Lamium backlog: done and closed

Items moved out of [BACKLOG.md](BACKLOG.md) once finished, closed or parked,
kept with their full history. Sections follow the kind each item had.
Runtime evidence is in [VALIDATION.md](VALIDATION.md).


---

## Bugs

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
