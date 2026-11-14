# Camera: Zoom, Freelook and FreeCamera

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/CAMERA.md`.

Zoom magnifies the view. Freelook turns the camera while the player's body,
head and movement keep their direction. FreeCamera also moves the camera
through the world while the player stays where it is. All three are
client-side camera changes: no teleport, no player rotation, no game-mode
change, no camera command, no server support. Freelook and FreeCamera are
off and unbound by default.

Settings (Camera): Zoom with Activation (Hold default), magnification (2x-50x),
Wide-angle view (off), magnification HUD element (on); Freelook with
Activation and starting view (first, rear third default, front third);
FreeCamera with Activation (Toggle default), flight speed (5-100 blocks/s,
default 20), Position reference (Player default, World), Hide while flying
(nothing, hand default, hand and hotbar, whole HUD), Leave on hit (off: any
hit, even without damage, ends FreeCamera; L-126), Underground hold (how
long the spectator culler stays after the eye leaves terrain: 0, 1, 3
default, 5, 10, 30 s or always; L-214). Keys: one per feature,
plus two unbound FreeCamera speed actions.

## Behavior

### Switches and lifecycle (decided 2026-09-26, L-47, L-27)

- Zoom, Freelook, FreeCamera, Permanent Sneak and Permanent Sprint have no
  saved switch. Their switch shows whether they are wanted; the key or a
  click on the switch flips it, and the effect starts when gameplay allows (a
  click in settings takes effect after closing them). Nothing is saved
  across restarts.
- Menus pause Zoom, Freelook, Sneak and Sprint; they resume afterwards.
  FreeCamera in Toggle mode keeps its position through menus, settings and
  focus loss. Hold ends when its input is released or input ownership is
  lost. Focus loss keeps toggle modes wanted; death, dimension change and
  leaving the world turn them all off.
- Lamium views (Settings, Shapes, Hotkeys, HUD layout) use the same input
  suspension as focus loss, not a camera reset (L-78). Toggle FreeCamera
  keeps its displacement, world reference and detached pose; movement input,
  timing and sprint are cleared and flight pauses. Zoom/Freelook pause while
  keeping toggle requests; Hold actions end through the release dispatch.
  Closing does not reactivate stale movement.
- Freelook and FreeCamera share one detached-camera session; they never run
  together, and switching must not leave two input owners or two camera
  overrides active.
- An interrupted hold keeps a release latch: native key repeat cannot
  restart it; one fresh press is needed. If the platform loses a key-up
  during focus loss, one press/release may be needed before the next
  activation; a still-held input is never silently restarted.

### Zoom (L-38, L-80, L-99, L-140, L-45)

- The magnification setting is 2x-50x (a saved value below 2x loads as 2x),
  so pressing Zoom always visibly zooms.
- While the Zoom key is held, the wheel goes from 1x to 50x; with
  "Wide-angle view" down to 0.5x, where the view widens but never past 160
  degrees (the wheel stops there). Turning the setting off brings a kept
  level below 1x up to 1x.
- A notch multiplies or divides the target by about 1.15, so every notch
  feels the same at 2x and 40x; a notch crossing 1x stops on exactly 1x once.
  The shown magnification eases toward the target; turn sensitivity follows
  the shown magnification above 1x and stays normal below it.
- The wheel level is kept between presses until the world is left, including
  levels below 1x and across dimension changes. A level left at exactly 1x
  is the exception: the next press starts from the setting.
- The wheel changes the magnification only while the Zoom key is held. In
  Toggle mode the wheel keeps its normal job (hotbar) while Zoom is on;
  Toggle switches on at the press and off at the release of a plain tap, and
  a hold that used the wheel leaves Zoom on.
- Pressing and releasing switch at once, whatever the Animations setting;
  only wheel changes ease (decided 2026-10-06).
- The magnification shows as "×12.5" in its own HUD element (small, dimmed,
  below the crosshair by default, movable in HUD layout).
- The toggle toast's default sits above the armor and absorption rows
  (bottom center, 72 up).
- The Zoom correction also slows the detached Freelook/FreeCamera turn,
  routed through the same `_applyTurnDelta` call.

### Freelook (L-39, L-48)

- Activation saves the current perspective, switches to the starting view
  and restores the saved perspective on release or cancellation. F5 works
  while Freelook is active. A wheel binding toggles in either Activation
  mode, since a wheel impulse cannot be held.
- Only the camera turns: the body, head pitch and head yaw keep their
  direction and movement (elytra flight included) keeps its course.
- Attack, use and building are blocked while detached (interaction guard).
- Starting while an item is being used (eating, charging) is refused; if the
  player starts using an item during the session, the override is
  discarded. Lamium never stops or completes item use itself.
- Release, settings entry, focus loss, world exit, dimension transition,
  camera configuration changes, non-gameplay screens, death, sleeping,
  riding, a runtime actor ID change, a missing runtime identity and an empty
  view stack cancel the session or prevent activation. Returning to vanilla
  removes the override; it never restores a stale player transform.

### FreeCamera (L-18, L-26, L-76, L-77, L-37, L-191)

- Rotation is Freelook's detachment. Movement keys move the camera; the
  player stays still. Attack and use are blocked as in Freelook.
- FreeCamera always flies in first person and shows the player's body.
  Starting in third person switches to first person, waits until the render
  eye stops moving, and restores the saved perspective on exit. F5 is
  ignored while active. Starting from third person begins at the head.
- Flight speed: 5-100 blocks/s in steps of 5, default 20. The two speed
  actions change it by 5 during active FreeCamera gameplay, save it and show
  a message toast; a held key does not repeat and the ends do not wrap.
  Labels: "Increase speed" / "Decrease speed"; Hotkeys names FreeCamera.
- Sprint (decided 2026-09-30): a fresh sprint-key press while moving forward
  doubles world-horizontal speed (max 200 blocks/s; vertical unchanged) and
  stays on after the key is released until forward input stops. Forward
  diagonals qualify; strafe-only, backward or cancelled forward input ends
  it. A press while stationary reserves nothing. Menus, focus loss and
  session end cancel it; an already-held key on resume is not a press.
  Opposing keyboard flags stay cancelled even if the analog vector is
  nonzero.
- Position reference: Player moves the camera with the body; World keeps the
  camera's world position independent of body movement while still taking
  flight input. Changing the choice during a session keeps the current
  position. A session starts at the player's eye; world/dimension exit
  clears the anchor.
- Hide while flying (decided 2026-10-15): the hand is the first-person item
  pass; the whole HUD skips the HUD screen's draw as F1 does, Lamium's HUD
  included; the hotbar hides its control for each draw only. Only while
  FreeCamera owns the view; no game option changes.
- Ends on its key, release (Hold), death, world exit and dimension change.

### Underground terrain visibility (L-37; implemented 2026-10-07)

Decided 2026-10-07. From inside solid ground FreeCamera shows the loaded
terrain and caves around it, as spectator rendering does, while keeping the
player's game mode, abilities, body and server movement. The spectator
culler is used while the camera eye is inside terrain (its block is not
air; an unloaded column or out-of-range height reads as no) and for the
Underground hold time after it leaves (`SpectatorHold`); open space keeps
the normal culler, which is cheaper. An unsupported path keeps vanilla
visibility; the rest of FreeCamera works without it.

## Code

All in `src/features/camera/` unless noted.

- `CameraSessions.cpp/.h`: session ownership, Zoom FOV, perspective
  save/restore (`setPlayerViewPerspective`), the
  `ClientInputUpdateSystem::extractRawHIDInput` hook, per-frame FreeCamera
  advance, item-use refusal (`ActorFlags::Usingitem`). Render, Zoom FOV and
  turn sensitivity check `LevelRendererPlayer::mClientInstance` / the
  player against the owning client; another renderer passes through.
- `DetachedCameraRig.cpp/.h`: removes and re-adds
  `VanillaCamera::UpdatePlayerFromCameraComponent`, saves and restores the
  camera's own angles, writes `CameraOffsetComponent::mEntityOffset`, adds
  `CameraRenderPlayerModelComponent` for the body.
- `DetachedLookState.h` (pure): angular session; bounded pitch, wrapped yaw,
  repeated activation ignored, discarded on cancel or invalid input.
  `tests/DetachedLookTests.cpp`.
- `DetachedCameraMotion.h` (pure): displacement session from camera basis,
  analog axes, speed and time; normalizes diagonals, limits a stalled update
  to 0.1 s, caps speed at 100 blocks/s before the sprint factor, discards on
  invalid input or owner replacement. `tests/DetachedCameraMotionTests.cpp`.
- `CameraMovementInput.cpp/.h`: `camera::consumeMovement` clears only the
  extracted horizontal axes and jump/sneak/ascend/descend flags of
  `RawMoveInputComponent` after vanilla extraction, for the session owner
  only; `camera::freecameraInputAxes` reads them just before;
  `SprintDown` is captured first. Stored physical input and look/selection
  flags stay. `tests-native/CameraMovementInputTests.cpp`
  (`LamiumNativeTests`: data manipulation only, not native ordering).
- `FreeCameraPosition.h` (pure): activation eye/reference and the chosen
  mode. `CameraAPI::$tryGetActorInterpolatedPosition` keeps its return value
  and calls the offset writer with that frame's interpolated body position;
  once reached, World skips the after-UI writer, which remains as fallback.
- `FreeCameraSpeed.h`, `FreeCameraSprint.h` (pure): speed normalization and
  the forward-only sprint latch.
- `FreeCameraCulling.cpp/.h`: intercepts the primary renderer's
  `updateLevelCullerType`; `terrainCuller` turns a requested 3 into 5 only
  for the owning session; other types and cameras pass through. Binds lazily
  from a real primary renderer in a `LevelRenderer::preRenderUpdate` hook;
  the slot comes from the SDK virtual-call thunk, never a hard-coded slot or
  address. An unmodified type-3 request must first leave `mLastCullerType`
  at 3, and every overridden request must retain 5; 30 mismatches in a row
  (`CullerCheck::limit`, L-213) disable the adapter. Capability `FreeCameraTerrain` under the shared version policy.
  `tests/FreeCameraCullingTests.cpp`.
- `FreeCameraHide.cpp/.h`: `ItemInHandRenderer::renderFirstPerson`; the
  `desktop_hotbar`, `pocket_hotbar`, `hotbar_panel` controls; capabilities
  `FreeCameraHand`, `FreeCameraHotbar`, `FreeCameraHud`.
- `CameraInteraction.cpp/.h`: interaction guard (below).
- `ZoomState.h` (pure): zoom levels, notch, easing, the 160-degree limit.
- `CameraTrace.cpp/.h`: development traces, xmake options `camera_trace`,
  `camera_probe` (fixed 20-degree yaw while Zoom is held) and
  `camera_position_probe` (2-block sideways shift); never in a release
  build, and never `camera_probe` together with the camera features.

Interaction guard: while a valid detached session owns the local player,
GameMode attack, start/continue/final block destruction, start/continue/final
placement, item use, use-as-attack, use-on-block and entity interaction, and
the SurvivalMode overrides of attack, interact, start/finish destruction,
start/final placement, use, use-as-attack and use-on-block return no success
(no swing for use-on-block) without calling the original. Other players and
inactive sessions pass through; stop/release operations stay untouched so
vanilla can clean up. Installed with the camera lifecycle, unwound if
startup fails.

Ordinary-build log lines (no trace option): `FreeCamera terrain: adapter
armed; awaiting FreeCamera renderer`, `... culler hook bound at virtual slot
...`, `... native request 3 -> 5 retained`. If the last is absent after
flight, the replacement was not observed: capture the log before trying
another path.

## Known

- Vanilla turns the active camera entity from look input, then copies its
  orientation to the local player for camera entities carrying
  `VanillaCamera::UpdatePlayerFromCameraComponent`. Removing it (after saving
  its look mode and the camera's `CameraDirectLookComponent` yaw/pitch or
  `CameraOrbitComponent` azimuth/polar angles) detaches the camera; restoring
  writes the angles back with orbit velocities and the direct-look yaw delta
  cleared, then re-adds the component. Each newly active rig is detached
  before look input and on the UI frame.
- `LocalPlayer::_applyTurnDelta` turns head yaw (`ActorHeadRotationComponent`)
  directly, without `Actor::setYHeadRot`; Freelook writes the activation head
  yaw back to current and previous head yaw after each look input and UI
  render.
- Vanilla builds the view from `CameraOffsetComponent::mEntityOffset`, so
  terrain, culling, shapes and chunk borders follow a displaced FreeCamera.
- `CameraRenderPlayerModelComponent` is an empty tag: test it with `all_of`,
  not `try_get`.
- The base GameMode interaction hooks never fire in survival mode;
  SurvivalMode has its own overrides. Creative uses the base GameMode.
- Culler types (observed on 1.26.51.01): normal and FreeCamera 3, spectator
  5. Type 3 clips caves at chunk borders when the camera is inside solid
  blocks; a camera in open cave space renders normally. The spectator type
  costs frame rate (65 -> 32 fps at a standstill in a local world).
- `CameraRegistry` owns the game/debug camera entities and has no
  standalone register/unregister API: do not rebuild it to add a Lamium
  camera.
- The camera entities belong to the level; without a local player the saved
  state is discarded.

## Tried

- Overriding the view matrix after `setupCamera`: changed culling, not the
  rendered view.
- Translating the view after `setupCamera` for FreeCamera: never reached the
  detached render; terrain vanished.
- Substituting the player's angles in `CameraAPI::tryGetActorRotation`: no
  effect on the rendered orientation.
- Moving third-person orbit rigs through the pivot: worked, but the rotation
  center felt wrong; FreeCamera locks first person instead.
- Continuing flight from the previous third-person eye: failed verification.
- Forcing the culler to 5 independently: fought native requests for 3 and
  blanked the view. Querying spectator/game type did not change the selection.
- A Zoom ease-in/out on press (0.1 s): felt wrong without a frame like the
  spyglass's.

## Limits

- Below 1x the held item drifts toward the screen center (the hand follows
  the wider projection); accepted for now.
- Terrain outside the client's loaded area stays unseen
  ([L-227 FreeCamera outside the loaded area shows only the sky](backlog/L-227.md)).
- Split-screen: hooks are limited to the owning client; independent
  simultaneous sessions are not implemented.
- Coverage (multiplayer view of the body, controllers, riding, item-use
  cases, other camera mods, world exit/death for the terrain adapter, Hold
  through Lamium views): [VALIDATION.md "Camera and view"](VALIDATION.md#camera-and-view).
- Open work:
  [L-181 FreeCamera draws the held trident the wrong way round](backlog/L-181.md),
  [L-123 FreeCamera stretches the worn elytra on the inventory player model](backlog/L-123.md),
  [L-244 FreeCamera keeps the inside-a-block view after starting in a block](backlog/L-244.md),
  [L-257 FreeCamera culler reads blocks on the render thread](backlog/L-257.md),
  [L-151 Show the player's body in first person](backlog/L-151.md),
  [L-325 Zoom: return to the set magnification on release](backlog/L-325.md).
