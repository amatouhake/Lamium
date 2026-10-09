# Visual effect visibility (L-42)

The maintainer revised the scope after testing on 2026-09-30: "Hide effects"
under Camera & view has a saved master switch with a toggle key, plus independent
child switches and optional keys. The master defaults off and children on;
older files preserve their existing selections. Only drawing changes;
weather, sound, equipment, status effects and boss state stay vanilla.

Current routes: rain/snow, particles, boss bars, nausea color and underwater,
lava and powder snow views are implemented, with local checks recorded in
[VALIDATION.md](VALIDATION.md). Pumpkin and spyglass frames are not supported;
their switches were removed and L-79 owns further research. The dated steps
below retain earlier defaults, candidates and unchecked-build statements as
implementation history, not current status.

## Distance fog (L-118, 2026-10-09)

A child "Distance fog" (selected by default like the others, unbound key) moves the air and weather
fog far away where no medium fog is shown: on land, in the Nether and the
End, and in a medium whose own fog is hidden. After vanilla
`LevelRendererPlayer::$setupFog`, `mCurrentDistanceFog` start/end become
16384/32768 (`farFog`, tested; values already farther or not finite stay
vanilla). Vanilla blends from that field next frame, so its own value is
written back before the next setup (the renderer address is kept only as an
identity). Render distance is unchanged. Checked in game on `34c3593`:
land (day, night, rain), Nether and End, and restoration. No effect under
Vibrant Visuals (its fog is volumetric); left for later.

## First implementation step, 2026-09-30

The first step exposed Rain and snow, and Particles, with the
Experimental badge. Both children carry their own toggle binding. Settings
persist independently; the toast names the individual effect being hidden.
Master off restores normal drawing without changing selections. Child switches
and keys still edit selections while paused, but never enable the master.
The UI dims child rows and labels them "Main switch off"; their toasts and
help explain that the selected effect is paused.

- Rain and snow: after `LevelRendererPlayer::createViewRenderObject`, change
  only the owned `WeatherRenderObject` snapshot for the local client's view.
  Zero rain/snow density and alpha. Keep vanilla `tickRain`/`doRainUpdate`
  intact, including their sound and splash work. Sky darkening stays vanilla.
  Rain splash drawing is also hidden: skip `Particle::$tessellate` only for
  `ParticleType::RainSplash` in the legacy pipeline. For data-driven particles,
  observe `_emitParticleNew` without changing it, and read the RainSplash
  entry in the engine's own `mNewParticleSystemJsonLookup`. Store at most one
  owned identifier of 192 characters, replacing it only when that entry
  changes; no game pointer is retained. Skip
  `ParticleEmitterActual::$extractForRendering` only when its effect name
  exactly matches that observed identifier. Missing/oversized mappings leave
  emitter drawing vanilla. Three constant-time lookups also reject identifiers
  shared with WaterSplash, WaterSplashManual or WaterWake, leaving such
  ambiguous pack mappings visible. No substring match, guessed identifier, scan of
  live particles, or alteration of emission/ticking is used. WaterSplash and
  other particles stay visible unless Particles is on. The follow-up playtest
  was positive, but no trace establishes individual native callback coverage.
- Particles: skip `ParticleEngine::render` (legacy layers) and
  `ParticleRenderer::renderParticles` (data-driven particles). Also zero the
  five ambient precipitation layers (plankton, spores and ash) in the weather
  snapshot; keep rain/snow controlled by their own switch. Particle creation
  and ticking stay vanilla, so this is a visibility feature, not a simulation
  or performance reduction. Split-screen per-view particle isolation is not
  established by these global render entry points.
- Hook availability gates each effect. Particles requires both particle
  render hooks and the weather hook, so an incomplete set leaves particles
  vanilla. Rain and snow additionally requires the three rain classification/
  extraction hooks. Validate all seven snapshot densities and alphas before modifying
  anything. Configuration is atomic; render callbacks never copy the full
  settings or retain game pointers. The only weather loop has seven entries.

The maintainer confirmed independent rain/snow and particle hiding, restoration
and preserved rain sound on `7e72244`. That build left rain splashes controlled
only by Particles. On `41b1ff6` they checked the revised build in game and
reported no problems; the supplied checklist included the master and rain/
ordinary-water splash distinction, without individual case results. Pure
tests cover all eight master/weather/particle
combinations, exact bounded rain identifiers, independent toggles, the master
and child keys, translations and settings round trip. Additional runtime
checks include both particle pipelines, ordinary water splashes with only
Rain and snow enabled, Nether ambient layers, immediate restoration, resource
packs, graphics modes, focus/world/dimension transitions and sound preservation.

With the master on, Rain and snow hides falling precipitation and rain
splashes. Particles hides rain splashes and every other particle, but leaves
falling precipitation alone. Either child hides rain splashes; master off
shows everything regardless of saved selections.

## Later implementation and research record

### Boss bar step (implemented after the 43c4211 trace)

The maintainer completed the requested observations on 2026-09-30. Sprite and
text callbacks were recorded below hud_screen / boss_health_panel /
boss_hud_panel / boss_health_grid, with empty/filled progress bar textures.
The new Boss bars child hides only sprite/text drawing under both exact boss
panels and the hud_screen root. It walks at most 32 parents with names bounded
to 192 characters; substring matches, other screen roots and unknown/deeper
layouts retain vanilla drawing. Both hooks must install or this child stays
vanilla. No control visibility, layout, updates, boss state or texture data is
changed. Master off and child off restore drawing on the next callback. The
selection saves independently and its new unbound toggle action is appended
after the existing action ids. Pure tests cover ancestry classification,
master gating, independent keys, storage, translations and settings rows.
The maintainer confirmed boss hiding and expected switch behavior on `d20fdf8`.
They did not report the optional key or other HUD elements separately.

### Nausea color step (implemented after the d3f0293 trace)

With vanilla Screen Distortion set to zero, the maintainer confirmed the green
overlay. The log identifies stage 1, material ui_texture_and_color_blur_additive
and resource textures/misc/nausea, distinct from the status icon's stage-0
textures/ui/nausea_effect draw. The new saved Nausea color effect child defaults
off with an appended unbound toggle key. It suppresses only that exact material/
resource pair in the two declared reference-based Mesh render overloads.

The owning InGamePlayScreen render establishes a thread-local scalar mask only
when its client equals the current local client and has a player. An RAII scope
restores any enclosing mask; no client, player, mesh or texture pointer is kept
across callbacks/frames. Each candidate rechecks the active master/child mask;
material/resource names are bounded to 192 characters. Outside this scope,
unknown names/texture variants and incomplete hook sets retain vanilla drawing.
All three hooks must install before this child is available. No opaque renderer
fields, internal by-value texture lists or mesh/texture contents are modified.
The status effect, icon and vanilla Screen Distortion preference stay unchanged;
this does not remove the warp. Child/master off restores the next draw.
Pure tests cover exact routes, icons/frozen/unknown routes, owner/master gating,
independent keys, storage, translations and the four settings rows. On normal
build `b239eb9`, the maintainer confirmed green hiding, child/master/key
restoration and unchanged effect/icon/vanilla preference. Additional modes,
packs, restart persistence and lifecycle/owner cases remain unchecked.

### Frame and immersion step (2026-09-30 checkpoint)

New hypothesis, from static evidence rather than another passive trace:

- The vanilla pack's content list names `textures/misc/pumpkinblur` and
  `textures/ui/spyglass_scope` (distinct from the `spyglass_flat` UI icon),
  and the vanilla UI definitions reference neither, so both frames are drawn
  natively. The frost frame already observed as on_screen_effect +
  `textures/ui/frozen_effect` on the same gameplay-screen mesh path suggests
  the pumpkin and spyglass frames use that path too. The earlier traces
  sampled one mesh call in 32 and stopped after 300,000 inspections, so they
  could exhaust their budget before the frames were worn; their absence from
  the log does not rule the path out.
- The vanilla pack has no underwater or in-lava screen texture. The water and
  lava view effect is the medium fog (and its color), which setup selects
  through the typed `FogDefinition` Water/Lava/LavaResist/PowderSnow types from
  the renderer's camera-medium flags.

Implementation: five saved children (default off, appended unbound keys):
carved pumpkin view, spyglass frame, underwater fog, lava fog (with or without
Fire Resistance) and powder snow view. The existing gameplay-screen mesh
filter hides exact vanilla resources: pumpkinblur, spyglass_scope, and
on_screen_effect + frozen_effect for powder snow. Resource paths alone identify
pumpkin and spyglass because those names belong only to the frames; material
names are not assumed. For fog, `LevelRendererPlayer::$setupFog` runs with the
hidden medium's camera flags cleared (liquid kept only while another selected
liquid remains) and restores them in an RAII scope before returning, so vanilla
resolves its own air or weather fog and nothing outside fog setup sees the
change. Powder snow is available only with both the mesh and fog hooks.

Each switch logs `Hide effects: route <bit> reached (...)` once, the first time
it hides something, with the material/resource or "fog medium". A switch that
does nothing and never logs means its route is elsewhere (for example a
`ClientTexture` variant without a resource name); that is the next research
input. Unchecked: whether fog color and underwater vision clarity follow the
cleared flags, Fancy vs other modes, custom packs replacing the textures, and
restoring on switch-off while inside a medium.

Result on `87f11cd` (maintainer, custom packs removed): underwater, lava and
powder snow (fog and frost frame) hide and restore; lava with and without Fire
Resistance was not told apart. The pumpkin and spyglass frames stayed visible,
and the log has only the three fog-medium route lines, no mesh route for either
frame. So the frames are not TexturePtr meshes with those resource names on
the gameplay-screen mesh path, while the frost frame is (it hid).

### Gated frame trace (L-79 research record)

Hypothesis: the frames use a mesh with a texture variant that has no
resource name, a differently spelled resource, or another draw entry (UI,
blit, tessellator). The earlier traces could not tell, because they sampled
one call in 32 under a shared call budget and filtered by keyword.

The `effects_trace` build now sets an effect gate once per gameplay-screen
render from the player's own state: 1 while a carved pumpkin is on the head,
2 while scoping (`Player::isScoping`). With the gate closed it records every
mesh, blit, tessellator and UI route key (stage, material, resource or
texture kind/span size, UI path) as a baseline, unsampled, up to 4096 keys.
With the gate open it logs each key missing from the baseline as
`research L-42 gated gate=<n> <key>`, at most 128 lines. Nothing is filtered
by keyword and no texture or mesh content is read. The older sampled logs are
unchanged. Procedure: stand in the world for about ten seconds without either
frame, then wear a carved pumpkin for five seconds, take it off, then scope
with a spyglass for five seconds.

Result on the `f7d49cf` trace (packs removed; equipping went through the
inventory): with the pumpkin gate open, only inventory hover UI routes were
new; no mesh, blit or tessellator key appeared. No gate-2 line was logged at
all, so it is unknown whether `isScoping` ever opened the gate. Keys without a
texture name can coincide with baseline draws, so "no new key" does not rule
out an unnamed mesh. The HUD custom renderers `vignette_rend`
(`HudVignetteRenderer`) and `camera_renderer` draw every frame.

Next hypothesis: the frames are HUD custom-renderer draws through the UI
render context (`getTexture` by resource name, `drawImage`, `flushImages`
with a material name), which the mesh-level keys did not name. The next trace
(`33d0bef`) adds those three typed entry points to the gate comparison, logs
each gate change (`research L-42 gate now <n>`), and ignores gated routes
while a non-gameplay screen is open.

Result on the `33d0bef` trace: both gates opened (pumpkin 19:57:42-50,
scoping 19:57:52-58), and still no new key of any kind, including UI texture
fetches, images and flushes. The frames therefore add calls to keys that are
already drawn every frame (for example another large image in the HUD
vignette renderer, whose texture is fetched once and cached), which a
first-seen-key comparison cannot show.

Next trace (`f7d9b88`): per-frame call counts per key, delimited by the
gameplay-screen render and discarding non-gameplay frames; when a gate
closes, keys whose average calls per frame differ from the gate-0 average by
at least 0.5 are logged (`research L-42 count`). Inside the HUD vignette
renderer it also logs each image's position, size and UV and each flush's
material, color and alpha: up to 20 lines with no gate, then 60 with a gate
open. This identifies the exact extra draw a hide filter would need.

Result on the `f7d9b88` trace (pumpkin 341 frames, scoping 284, against 713
and 876 baseline frames): the only key whose per-frame count changed was the
XP bar custom renderer (held item changes). The HUD vignette renderer made no
image or flush call at all. So neither frame passes through any hooked entry:
the three `Mesh::renderMesh` overloads, the three `ScreenRenderer::blit`
overloads, tessellator interception, or the UI render context. The frost
frame and nausea color, which do pass through `Mesh::renderMesh`, use a
different path. Three trace rounds have not found the frames; this is the
stop point for the hook-by-hook approach. A texture-substitution check (a
test pack with transparent pumpkinblur/spyglass_scope textures) would show
whether those textures are drawn at all before any further native work.

Decided 2026-09-30 (maintainer): the carved pumpkin and spyglass frames are
parked as BACKLOG L-79; their switches, keys and resource matches were removed
before release. The effects trace keeps the gate/count tooling for L-79.

- `WeatherRenderer` and `PlayerRenderView` are opaque in SDK 26.51.5. Do not
  invent private render methods or offsets.
- The SDK has generic SpriteComponent, TextComponent and custom UI renderer
  entry points, but no confirmed per-effect native entry for the four HUD/view
  effects. Read-only runtime routes must establish what draws each effect
  before filtering it. The public [official HUD sample](https://github.com/Mojang/bedrock-samples/blob/main/resource_pack/ui/hud_screen.json)
  was inspected as platform documentation; it does not establish the current
  game's per-effect native path. No resource-pack source or assets are copied.
- `LevelRendererPlayer::$_getFogDistanceSettingType` exposes Air, Weather,
  Water, Lava, LavaResist and PowderSnow; the renderer also has density and
  volumetric coefficient paths and camera-medium flags. A declaration alone
  does not prove which graphics mode consumes which path, or remove the
  corresponding full-screen view overlay. The 2026-09-30 step lets fog setup
  resolve the non-medium type itself (cleared camera-medium flags) instead of
  substituting Air, and relies on the vanilla pack having no water/lava screen
  texture; the in-game check decides whether that is the whole view effect.

## Bounded read-only observation record

Expanded 2026-09-30 after the request to implement the remaining seven effects.
`FullScreenEffectRenderer` and `OnCameraEffectRenderer` are empty declarations
in SDK 26.51.5; the full-screen framebuilder objects are also opaque. Generic
mesh rendering exposes material identifiers and sometimes texture resources,
but no documented per-effect ownership. `Mesh::_renderMesh` takes a by-value
`brstd::static_vector` that is an empty template in this SDK: do not hook that
entry or invent its layout. The trace uses the two `renderMesh` overloads
whose texture variant and optional metadata are passed by const reference.
Client/server texture variants may have no resource name; record their variant
kind rather than guessing a texture. The latest trace also covers the typed
multi-texture span; render-graph paths remain uncovered, so absence from this
log does not prove absence of an effect.
Boss and nausea children use the confirmed routes; the other five effects have
no production suppression yet.

`effects_trace` is an opt-in xmake option. An ordinary build contains no UI
or fog trace hooks. Both trace-enabled and trace-disabled DLL builds were
checked. The first runtime trace was collected on `43c4211`; see the log and
the boss implementation above. To collect observations, build with
`xmake f --effects_trace=y` then `xmake build Lamium`; deployment still
requires the maintainer to request a trace build and close Minecraft.

The trace logs UI control routes, sprite resource paths, mesh material/texture
identifiers, vertex/draw counts, fog distance/density enum types, camera-medium
bits and resolved fog distance/density/control values after vanilla setup.
It never logs displayed text, player identity or world coordinates, and does
not suppress drawing or change fog. In gameplay it inspects at most 300,000 UI
callbacks and 300,000 mesh callbacks per process, sampling approximately one
in 32 with a varying slot so a stable draw order does not exhaust the budget
or starve a control. It stores at most 80 unique UI routes (64 candidates plus
16 others), and 56 mesh routes (48 candidates plus 8 others). Each individual
route/path/material identifier is bounded to 192 characters. At most 48
distance-type combinations, 40 density-type combinations and 24 resolved fog
samples are recorded. Owned strings and scalar samples only; no retained game
pointers, texture changes, vertex-buffer reads or scans of live entities.
These observations identify candidate paths; they do not prove an effect works.

In a local test world, encounter a boss bar, equip a carved pumpkin, use a
spyglass, trigger nausea, and enter water, lava and powder snow, keeping each
visible for several seconds. For lava, include fire resistance; for powder
snow, include the freezing view effect. Search the log for `research L-42`;
report the graphics mode and repeat relevant cases in each mode. The first
line reports hook availability. If no candidate appears or the callback
budget was exhausted, restart and enter the missing effect directly. Before
ordinary builds, reset with `xmake f --effects_trace=n` and rebuild.

Follow-up trace: the first log established the three distance/density medium
selections and the frozen mesh, but did not capture pumpkin/spyglass drawing
or a verified nausea color overlay. The nausea texture used with a generic UI
material may be a status icon. No production suppression uses that identifier.
UI routes now include a separately bounded 192-character tail so deeply
nested custom controls are distinguishable. Three reference-based
ScreenRenderer blit overloads record candidate material/resource identifiers
and destination sizes, distinguishing large surfaces from small icons. At
most 300,000 sampled screen callbacks and 64 unique screen candidates are
recorded. Render-stage tags are thread-local scalars only: 0 unknown, 1 the
in-game render, 2 post-level render, 3 the dedicated HUD vignette renderer.
The vignette hook observes rather than suppresses all vignettes. Resolved fog
now records at most 24 samples, after entering each medium, then after one
and five continuous seconds, to distinguish transition values from settled
values. This replaces the earlier limit of one sample per medium. No opaque
renderer fields or by-value texture-list layouts are read or invented.

The maintainer reported Fancy graphics for the first trace. Active global-pack
metadata matches the custom HUD routes seen there. Follow-up observations
should use the same configuration first; pack-free comparisons may be needed
if a remaining effect does not produce an identifiable route.

The `d20fdf8` follow-up records settled Fancy distance fog: water grows from
about 0.0099 to 14.6841 after one second and 29.5375 after five; lava remains
0.64 and powder snow 2. Density stays zero in these samples. Lava/fire-resistant
lava select types 3/4, but their shared medium-bit phase budget does not record
fire resistance separately. All six new screen hooks installed, without a
screen candidate or vignette route. Frozen drawing has stage 1 and material
on_screen_effect; the nausea texture still has a generic UI material at stage
0. This is not a confirmed nausea color effect. Pumpkin/spyglass have no draw
candidate yet. The sampled hooks do not cover the declared multi-texture mesh
overload or tessellator interception; inspect those typed paths rather than
inventing the opaque full-screen renderer's layout. No immersion switch is
exposed until its fog and view-overlay scope can be implemented together.

The maintainer clarified that pumpkin and spyglass frames were actually visible.
For nausea, Screen Distortion had not been reduced to zero: they saw the warp,
not the green color effect. A future nausea-color observation must set vanilla
Screen Distortion to zero; Lamium does not change that preference itself.

The next trace covers the declared `Mesh::renderMesh` multi-texture overload,
whose by-value GSL span has a complete implementation in the SDK dependencies.
It observes only the material and span length, never dereferencing textures
or reading the opaque internal TextureList. It shares the existing bounded
mesh budget and marks the texture as span[N], without inventing resource names.
The tessellator's documented `triggerIntercept` entry observes its reference-
based material/TexturePtr pair using the existing screen budget. It does not
force interception or replace callbacks. Candidate filters now include the
observed on_screen_effect material and frozen texture spelling. Up to eight
one-time entry records distinguish callback reach from successful hook
installation for screen/stage, span and tessellator paths. These are additional
read-only research hooks; five hide switches remain unimplemented after the
new nausea step.

On `d3f0293`, all sixteen observation hooks installed. Entry records show the
gameplay render, span mesh, rectangular blit and variant blit being reached;
there is no texture-blit, post-level, vignette or tessellator-intercept entry.
The green nausea material/resource pair is newly recorded. Pumpkin/spyglass
remain unidentified even though the maintainer confirmed their frames visible.
No additional immersion observation was requested this step. The span records
observed debug and shape materials with zero textures; they do not identify
either missing frame. A repeated identical trace is not the next research step:
inspect a different documented backend before asking for more frame observations.
