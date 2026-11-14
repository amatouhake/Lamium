# Visual effect visibility (L-42)

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/VISUAL-EFFECTS.md`.

**Hide effects** (Camera & view) stops drawing selected visual effects. It
has a saved master switch, default off, with an unbound toggle key ("Toggle
hiding effects"), and one saved child switch per effect, each default on and
each with its own unbound toggle key:

| Child | Setting | Action id |
|---|---|---|
| Rain and snow | `visuals.hideWeather` | `hideweather` |
| Particles | `visuals.hideParticles` | `hideparticles` |
| Nausea color effect | `visuals.hideNausea` | `hidenausea` |
| Underwater fog | `visuals.hideWater` | `hidewater` |
| Lava fog | `visuals.hideLava` | `hidelava` |
| Powder snow view | `visuals.hidePowderSnow` | `hidepowdersnow` |
| Distance fog | `visuals.hideDistanceFog` | `hidedistancefog` |
| Nether portal screen | `visuals.hidePortal` | `hideportal` |

Hide boss bars is no longer part of Hide effects: since L-334 it is the boss
bars' show value under Game HUD placement ([HUD.md](HUD.md)), and the old
`hidebossbars` action is the boss bars' key there. The carved pumpkin and spyglass
frames are not supported (switches removed before release; research in
[L-79 Carved pumpkin and spyglass frame draw path](backlog/L-79.md)).

## Behavior

Decided by the maintainer on 2026-09-30, revised before release; Nether
portal screen added 2026-10-12 (L-148).

- Master off, every child on by default: turning on the one master switch
  hides everything; nothing is hidden until then.
- Master off restores drawing at once and keeps the selections. Child
  switches and keys still edit selections while the master is off but never
  turn the master on. The UI and key toasts make that paused state explicit
  (DESIGN "Rows"); a child toast names the individual effect.
- Only drawing changes. Sound, weather state, particle emission and ticking,
  equipment, status effects, the portal transfer and its timing stay vanilla.
  Particles is a visibility feature, not a performance reduction.
- **Rain and snow** hides falling precipitation and rain-derived splashes.
  Sky darkening and rain sound stay.
- **Particles** hides every particle, rain splashes included, and the five
  ambient precipitation layers (plankton, red/blue spores, ash, white ash),
  but not falling rain or snow. Ordinary water splashes stay visible with only
  Rain and snow selected.
- **Nausea color effect** removes the green overlay only. The warp is the
  vanilla Screen Distortion preference, which Lamium does not change; the
  status effect and its icon stay.
- **Underwater fog**, **Lava fog** (with or without Fire Resistance) and
  **Powder snow view** (its fog and the frost frame) let vanilla resolve the
  ordinary air or weather fog instead of the medium's fog.
- **Distance fog** moves air and weather fog out to the render distance where
  no medium fog is shown: on land, in the Nether and the End, and in a medium
  whose own fog is hidden. Render distance is unchanged. Under Vibrant Visuals
  (graphics mode Advanced and up) distance fog stays vanilla.
- **Nether portal screen** removes the purple screen in a portal.

## Code

- `src/features/visuals/HideEffects.cpp`: hooks and `configure`. Each
  effect is gated by a capability (`app/Capabilities.h`: `HideWeather`,
  `HideParticles`, `HideNausea`, `HideFog`, `HidePortal`; `HideBossBars` is
  driven by Game HUD's `information.hidesBossBars()`). An incomplete hook set
  leaves that effect vanilla and logs one warning.
  - Rain and snow (`HideWeather`, needs all four): `WeatherVisibility`
    (after `LevelRendererPlayer::createViewRenderObject`), `RainParticleVisibility`
    (`Particle::$tessellate`, legacy `ParticleType::RainSplash` only),
    `RainEffectMapping` (observes `ParticleEngine::_emitParticleNew`, reads the
    RainSplash entry of `mNewParticleSystemJsonLookup`), `RainEmitterVisibility`
    (`ParticleEmitterActual::$extractForRendering`, exact name match).
  - Particles (`HideParticles`, needs all three): `LegacyParticleVisibility`
    (`ParticleEngine::render`), `DataParticleVisibility`
    (`ParticleRenderer::renderParticles`), plus `WeatherVisibility` for the
    ambient layers.
  - Nausea color and the frost frame (`HideNausea`, needs all three):
    `EffectLocalScreen` (`InGamePlayScreen::$render` sets a thread-local mask
    in an RAII scope, only when its client is the current local client with a
    player), `NauseaMeshVisibility` and `NauseaMetadataVisibility` (the two
    reference-based `mce::Mesh::renderMesh` overloads).
  - Medium and distance fog (`HideFog`): `MediumFogVisibility`
    (`LevelRendererPlayer::$setupFog`). Powder snow needs both `HideFog` and
    `HideNausea`.
  - Portal (`HidePortal`): `PortalScreenVisibility` (after
    `LocalPlayer::$normalTick`).
  - Boss bars: `BossBarSpriteVisibility`, `BossBarTextVisibility`
    (`SpriteComponent::render`, `TextComponent::$render`).
- `src/features/visuals/EffectVisibility.h` (pure): effect bits and
  `effectMask`, `overlayMeshBit` (exact material/resource pairs),
  `visibleMedium`, `hidesDistanceFog`, `farFog`, `BossBarRoute`,
  `hiddenWeatherLayers`, `rainEffectMatches`. Tested in
  `tests/SettingsStoreTests.cpp` with the settings round trip, keys and rows.
- Each route logs `Hide effects: route <bit> reached (<detail>)` once, the
  first time it hides something: evidence of which route a switch reached.
- Settings: `Settings::Visuals` (`hideEffects` and the children); row
  `hideEffects` in `SettingsRows.h`; game dependencies in
  [GAME-UPDATES.md](GAME-UPDATES.md) under `HideEffects.cpp`.
- `effects_trace` (xmake option, `EffectTrace.cpp`): opt-in, read-only
  research trace for undiscovered routes (below). Ordinary builds contain no
  trace hooks.

### Route rules the code keeps

- Weather: change only the owned `WeatherRenderObject` snapshot of the local
  client's view. Validate all seven densities and alphas (finite,
  non-negative) before changing any; zero density and alpha of hidden
  layers. Never alter `tickRain`/`doRainUpdate`: they feed sound and splash
  emission.
- Rain splashes in the data-driven pipeline: store at most one owned
  identifier (192 characters), replaced only when the engine's RainSplash
  entry changes; no game pointer kept. An identifier shared with WaterSplash,
  WaterSplashManual or WaterWake (three constant-time lookups) is ambiguous
  and stays visible; missing or oversized mappings stay vanilla. No substring
  match, guessed identifier or scan of live particles.
- Overlay meshes: hide only `ui_texture_and_color_blur_additive` +
  `textures/misc/nausea` (nausea) and `on_screen_effect` +
  `textures/ui/frozen_effect` (powder snow), names bounded to 192 characters,
  only inside the local gameplay-screen scope. Unknown names, texture
  variants without a resource name and calls outside the scope draw.
- Medium fog: run `$setupFog` with the hidden medium's camera flags cleared
  (the liquid flag kept while another, unhidden liquid remains) and restore
  them in an RAII scope before returning, so nothing outside fog setup sees
  the change.
- Distance fog: after vanilla setup, `farFog` sets `mCurrentDistanceFog` to
  start at the render distance (`getViewDistanceChunks() * 16`, or vanilla's
  end if farther) and end 5% past it; values not finite or already farther
  stay vanilla. Vanilla blends from that field next frame, so its own value
  is written back before the next setup (the renderer address is kept only as
  an identity, never dereferenced).
- Portal: set `mPortalEffectTime` and `mOPortalEffectTime` to 0 after every
  tick while hidden.
- Boss bars: hide sprite/text drawing only under the exact boss panels below
  the `hud_screen` root, walking at most 32 parents with names up to 192
  characters; other roots and unknown or deeper layouts draw.
- Render callbacks never copy the full settings or keep game pointers;
  configuration is one atomic mask.

## Known

- `WeatherRenderObject::PrecipitationType` has exactly seven layers (Rain,
  Snow, Plankton, RedSpores, BlueSpores, Ash, WhiteAsh; static_assert).
- The portal's purple screen follows the local player's portal effect time
  (0 to 1 over about 4 s in a portal). It is client-only; the transfer is
  decided elsewhere (checked in game 2026-10-12).
- The nausea color is stage 1, material `ui_texture_and_color_blur_additive`,
  resource `textures/misc/nausea`; the status icon is a different draw
  (stage 0, `textures/ui/nausea_effect`). Observing it needs vanilla Screen
  Distortion at zero, otherwise only the warp shows.
- The frost frame is `on_screen_effect` + `textures/ui/frozen_effect` on the
  gameplay-screen mesh path.
- The vanilla pack has no underwater or in-lava screen texture: the water and
  lava view effect is the medium fog and its color, selected in setup through
  the typed `FogDefinition` types from the renderer's camera-medium flags.
  `$_getFogDistanceSettingType` exposes Air, Weather, Water, Lava,
  LavaResist and PowderSnow.
- Settled Fancy distance fog samples (trace): water grows from about 0.01 to
  14.7 after one second and 29.5 after five; lava stays 0.64, powder snow 2.
- Vibrant Visuals fog is volumetric; the distance fog change has no effect
  there, and an earlier build turned the world magenta and flickering, so it
  stays vanilla.
- Opaque in the SDK: `WeatherRenderer`, `PlayerRenderView`,
  `FullScreenEffectRenderer`, `OnCameraEffectRenderer` (members of
  `InGamePlayScreen`) and the full-screen framebuilder objects. Do not invent
  private render methods, offsets or layouts.
- `Mesh::_renderMesh` takes a by-value `brstd::static_vector` that is an empty
  template in the SDK: do not hook it. The multi-texture `renderMesh`
  overload's by-value GSL span is complete; read only material and span
  length, never the textures or the internal TextureList.
- Client and server texture variants may have no resource name.
- The vanilla UI definitions (and the public HUD sample) do not establish a
  per-effect native path; no resource-pack source or asset is copied.

## Tried

- Distance fog at fixed 16384/32768 blocks: at 96 chunks the game stopped
  building chunks away from the view direction. Replaced by the render
  distance plus 5%.
- Substituting the Air fog type for a hidden medium: replaced by clearing the
  camera-medium flags so vanilla resolves its own non-medium fog.
- Pumpkin and spyglass frames matched by resource path (`pumpkinblur`,
  `spyglass_scope`) on the gameplay-screen mesh path: nothing hid and no
  route line appeared; switches removed. See the frame trace below.
- Treating the nausea texture with a generic UI material as the color
  overlay: it is the status icon.
- Repeating an identical sampled trace: a sampled, keyword-filtered trace
  under a shared budget could not show the frames. Change the method or the
  backend before asking for more frame observations.

## Gated frame trace

What three trace rounds established about the carved pumpkin and spyglass
frames (continued in L-79):

- With an effect gate (1 while a carved pumpkin is on the head, 2 while
  `Player::isScoping`), first-seen route keys showed no new mesh, blit,
  tessellator or UI render-context key (`getTexture`, `drawImage`,
  `flushImages`) while either frame was visible.
- Per-frame call counts per key, compared against gate 0, changed only for
  the XP bar custom renderer (held item changed). `HudVignetteRenderer`
  made no image or flush call at all.
- So neither frame passes through the three `Mesh::renderMesh` overloads,
  the three `ScreenRenderer::blit` overloads, `Tessellator::triggerIntercept`
  or the UI render context. The frost frame and nausea color do pass through
  `Mesh::renderMesh`.
- The HUD custom renderers `vignette_rend` (`HudVignetteRenderer`) and
  `camera_renderer` draw every frame.
- Next step is a texture-substitution test pack (L-79), not more hooks.

The `effects_trace` build (`xmake f --effects_trace=y`, then build; reset
with `--effects_trace=n` before an ordinary build; deploy only when the
maintainer asks for a trace build) keeps that tooling:

- Gate comparison: with the gate closed it records every mesh, blit,
  tessellator and UI route key (stage, material, resource or texture
  kind/span size, UI path) as a baseline, unsampled, up to 4096 keys; with a
  gate open it logs new keys as `research L-42 gated gate=<n> <key>` (at most
  128 lines) and each gate change as `research L-42 gate now <n>`. Gated
  routes are ignored while a non-gameplay screen is open.
- Counts: per-frame call counts per key, delimited by the gameplay-screen
  render; when a gate closes, keys whose average differs from gate 0 by at
  least 0.5 are logged as `research L-42 count`. Inside the HUD vignette
  renderer each image (position, size, UV) and flush (material, color,
  alpha) is logged, 20 lines without a gate and 60 with one.
- Older sampled observation: UI control routes and sprite paths, mesh
  material/texture identifiers, blit candidates with destination sizes,
  render-stage tags (0 unknown, 1 in-game, 2 post-level, 3 HUD vignette), fog
  distance/density types, camera-medium bits and resolved fog samples (after
  entering a medium, then after one and five seconds). Bounded: 300,000
  sampled callbacks per kind (about one in 32 with a varying slot), small
  fixed route and sample counts, identifiers up to 192 characters, up to
  eight one-time entry records showing which hooks were reached.
- It never logs displayed text, player identity or coordinates, never
  suppresses drawing or changes fog, reads no texture or vertex contents and
  keeps only owned strings and scalars. The first line reports hook
  availability; absence from the log does not prove an effect is absent.

## Limits

- The carved pumpkin and spyglass frames stay visible (L-79).
- The rain and particle entry points are global; per-view isolation in split
  screen is not established.
- Distance fog has no effect under Vibrant Visuals.
- A resource pack that maps rain splashes to a water-splash identifier keeps
  its rain splashes visible.
- What is and is not checked in game: [VALIDATION.md](VALIDATION.md), row
  "Hide effects (L-42)"; release checks in
  [pre-release-checks.md](backlog/pre-release-checks.md).
