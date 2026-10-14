# Detached camera implementation notes

Freelook and FreeCamera are disabled and unbound by default. FreeCamera remains
experimental. Both passed local runtime checks. Current product behavior is in
[DESIGN.md](DESIGN.md#camera-decided-2026-09-26-unless-noted); current coverage
and remaining checks are in [VALIDATION.md](VALIDATION.md). This file records
the native integration and its evolution. Dated checkpoints retain earlier
cancellation rules and limitations; later L-26/L-76/L-77/L-78 entries and
DESIGN define the current behavior. The SDK/probe sections at the end are
earlier research, not instructions to replace the working camera path.

## Freelook camera detachment

L-39 (verified by the maintainer in game 2026-09-27, commit b585411, DLL
7af6b60a): activation saves the current perspective and requests rear third
person; release and cancellation restore the saved perspective. F5 remains
available during Freelook. Each newly active camera rig is detached before look
input and on the UI frame, so switching rigs does not deliberately reattach
camera-to-player rotation.

L-48 (maintainer-confirmed in game 2026-09-27, commit a7ce3a6, DLL d262d42e)
adds a saved starting-view choice for Freelook (first, rear third by default,
front third) and a saved Activation choice for FreeCamera (Hold or Toggle by
default). A wheel binding remains a toggle in either mode. The maintainer
confirmed the follow-up as complete; individual edge cases were not reported.

Vanilla (Minecraft 1.26.51.01) turns the active camera entity from look input and
then copies the camera orientation to the local player for camera entities that
carry `VanillaCamera::UpdatePlayerFromCameraComponent`. `LocalPlayer::_applyTurnDelta`
does not change the player's rotation synchronously; it also turns the head yaw
(`ActorHeadRotationComponent`) directly, without `Actor::setYHeadRot`.

While the Freelook action is held:

- The component is removed from every entity with `ActiveCameraComponent` and
  `UpdatePlayerFromCameraComponent`, after saving its look mode and the camera's
  own angles: `CameraDirectLookComponent` yaw/pitch (first person, radians) and
  `CameraOrbitComponent` current/ideal azimuth and polar angle (third person).
- Look input still reaches vanilla, so only the camera turns; the player's body
  and pitch stay unchanged and movement (including elytra flight) keeps its
  original direction.
- The head yaw captured at activation is written back to both current and
  previous head yaw after each look input and after each UI render.

On release or any cancellation, the saved camera angles are written back (with
orbit velocities and the direct-look yaw delta cleared) before the component is
re-added with its original look mode. Without a local player the saved state is
discarded, since the camera entities belong to that level. Attack, use and
building remain blocked by the existing interaction guard while detached.

Earlier approaches did not work at runtime and were removed: overriding the view
matrix after `setupCamera` changed culling but not the rendered view, and
substituting the local player's angles in `CameraAPI::tryGetActorRotation` did not
affect the rendered orientation.

Local runtime check (2026-09-23, Minecraft 1.26.51.01 / LeviLamina Client 26.51.3):
in first and third person, the view turned freely while held and returned to the
original direction on release; body, head pitch and head yaw stayed fixed without
jitter, and nothing snapped on release. During elytra flight the original flight
direction was kept while looking around; turning the view did not steer. Trace
logs confirmed one detached camera per activation (direct look in first person,
orbit in third person) and head yaw changes originating in `_applyTurnDelta`.
Multiplayer visibility of the head, riding, spectator/creative flight, dimension
changes during a hold, controllers and custom camera presets are unverified.

`DetachedCameraMotion` now supplies a game-independent displacement session for
the future FreeCamera adapter. It consumes camera-basis vectors, analog axes,
speed and elapsed time; it normalizes diagonals, limits a stalled update to
0.1 seconds, caps supplied speed at 100 blocks/second, and discards the session
on invalid input or owner replacement. These are initial internal bounds, not
user-facing settings. Cancellation removes the displacement without retaining
or restoring any player transform. FreeCamera drives it (see FreeCamera below).

`camera::consumeMovement` is the native extraction boundary prepared for that
adapter. After vanilla HID extraction it consumes `RawMoveInputComponent`'s
horizontal axes and momentary jump/sneak/ascend/descend flags, then clears only
the extracted movement axes/flags. It does not alter the stored physical input
or look/selection flags. A FreeCamera extraction hook calls it after vanilla HID
extraction for the session owner only; FreeCamera reads its axes with
`camera::freecameraInputAxes` just before. Freelook sessions pass through
untouched. Keyboard axes were verified in game; controllers were not tested.

The standalone `LamiumNativeTests` target builds this adapter against the SDK
types without launching Minecraft or calling engine functions. Run
`xmake build LamiumNativeTests` and `xmake run LamiumNativeTests`. It verifies
that consumption clears both horizontal vectors and movement flags, preserves
look/selection state, does not modify a copied source snapshot, and distinguishes
held jump/sneak from a persistent sneak toggle. The local build/run passes;
CI now includes the target. These tests establish data manipulation only, not
native input ordering, axis signs, or actual player movement suppression.

`DetachedLookState` now provides the game-independent angular session: begin from
a fresh orientation, ignore repeated activation, accumulate degree deltas with
bounded pitch and wrapped yaw, and discard the pose on cancellation or invalid
input. Snapshot and input updates are synchronized. Unit tests cover boundary
crossing, pitch limits, repeated activation, cancellation/reactivation, and
nonfinite/extreme input. The `freelook` Hold action uses this
session for activation, ownership and cancellation; the camera itself is
detached as described above.
Features and Hotkeys expose the action, with a separately persisted enable flag.
Release, settings entry, focus loss, world exit, dimension transition, camera
configuration changes, and non-gameplay screens discard the detached pose.
Cancellation retains a release latch: native key repeat cannot restart an
interrupted hold. The action's release callback clears that latch. Invalid input
and owner replacement follow the same rule. If the platform loses a key-up during
focus loss, one press/release may be needed before the next activation; silently
restarting a still-held input is not used to recover. Unit tests cover these
cancellation/repeat/release sequences; native focus recovery remains unverified.

Pure tests cover pitched starts, both poles and yaw boundary angles. Matching
this model to native interpolation, front third-person view and camera effects
still needs runtime validation; it is not proof of the final rendered world pitch.
Runtime actor ID
changes now cancel the session without retaining an actor pointer; death,
sleeping, riding, missing runtime identity, and an empty view stack also cancel
or prevent activation. Owner replacement is unit-tested, while these native
lifecycle checks still require Minecraft validation. Interaction aim,
split-screen rendering, culling and perspective transitions remain incomplete;
this is not a stable Freelook feature.

Render application now checks `LevelRendererPlayer::mClientInstance` against the
client that owns the hold. A different renderer passes through without cancelling
that hold. Zoom FOV and turn sensitivity also check their client/player owner.
`TurnHook` also routes the Zoom correction through the one `_applyTurnDelta`
call, so the detached Freelook/FreeCamera turn slows like the attached look
instead of using raw input.
This narrows native hook effects to the current owner; it does not implement
independent simultaneous split-screen sessions or prove split-screen support.

The interaction guard now intercepts GameMode attack, start/continue/final block
destruction, start/continue/final placement, item use, use-as-attack, use-on-block
and entity interaction, plus the SurvivalMode overrides of attack, interact,
finish/start block destruction, start/final placement, use, use-as-attack and
use-on-block (the base hooks never fire in survival mode; creative mode uses
the base GameMode). Stage 1 runtime testing showed mob attacks passing while
detached in survival mode, which these mirrors fixed (verified in game 2026-09-24). While a valid detached session owns that local player,
these paths return no success (and no swing for use-on-block), without invoking
the original operation. Other players and inactive sessions pass through.
Stop/release operations remain untouched so vanilla can clean up existing use.
The guard installs with the camera lifecycle and is unwound if startup fails.

This is build-verified only. Runtime checks must cover keyboard remapping,
offhand use, continued mining/placement, and another interaction mod. Starting
Freelook while an item is already charging/eating is now rejected using the SDK's
`ActorFlags::Usingitem` status flag. If that flag becomes set during a detached
session, the override is discarded. Lamium does not call stop/release/complete
item use or mutate the flag; the existing action remains vanilla-owned. This
policy still requires validation with food, bows, crossbows and offhand use,
including the initial-use tick and release-triggered effects. No claim
of complete interaction isolation is made from the list of hooks alone.
The next work should validate and correct this integration, not merely expand its
settings. Do not enable the old fixed-angle `camera_probe` simultaneously.

## FreeCamera

FreeCamera (experimental, unbound Toggle) reuses Freelook's detached session
and adds position. Verified in game 2026-09-24 on build 31323b8 (trace build):

- Rotation: the same detachment as Freelook; the two never run together.
- Movement: after `ClientInputUpdateSystem::extractRawHIDInput`, axes are read
  from the direction flags (analog vector as controller fallback) and then
  consumed, so the player stays still. Attack/use are blocked by the
  interaction guard, including the SurvivalMode mirrors.
- Position: `DetachedCameraMotion` advances once per rendered frame (fixed 20
  blocks/s, basis from the fresh vanilla view) and the displacement is written
  into the detached camera entity's `CameraOffsetComponent::mEntityOffset`
  after UI render. Vanilla builds the view from it, so terrain, culling,
  shapes and chunk borders follow. Chunks outside the player's render
  distance stay ungenerated.
- Perspective: FreeCamera always flies in first person and adds
  `CameraRenderPlayerModelComponent` (an empty tag: test it with `all_of`, not
  `try_get`) so the body is visible. Starting in third person sets first
  person with `setPlayerViewPerspective`, waits until the render eye stops
  moving, and restores the saved perspective on exit. F5 is ignored while
  active.
- Exits: toggle, settings, death, focus loss and world re-entry verified.
  Dimension change could not be exercised with the camera alone.

Failed approaches: translating the view after `setupCamera` (924dd12,
8c81b36) was applied but never reached the detached render; terrain vanished
instead. Moving third-person orbit rigs through the pivot worked but the
rotation center felt wrong, so FreeCamera locks first person instead.
Continuing flight from the previous third-person eye (31323b8) did not pass
verification and was removed: starting from third person begins at the head.

## Trident interaction report (2026-10-14)

[L-181](BACKLOG.md) tracks an unverified report of a trident striking the
player when used with FreeCamera active. Establish whether the trident was
thrown, used for Riptide or merely held, and whether the behavior occurs
on a server. Keep interaction blocking, camera/body position and
server-authoritative projectile behavior separate during diagnosis.
No fix or change to the camera contract is selected.

## Underground terrain visibility (L-37; implemented 2026-10-07)

Runtime status: **underground terrain checked in a local world and on BDS**
by the maintainer on `d56b81e`, 2026-10-07, Minecraft 1.26.51.01 / LeviLamina
Client 26.51.6 (DLL `822285b2...5f8bfe7c`, trace options off). The runtime log
confirms binding at SDK-derived virtual slot 26 and the native 3 -> 5 request
being retained. The maintainer also confirmed normal-view and player-control
restoration after turning FreeCamera off. Other players' view of the body is
explicitly unchecked. A further report confirms intended Hold/Toggle behavior,
camera retention through menus and focus loss, and FreeCamera turning OFF on
dimension travel. Per-case environments were not listed separately. World
exit/death, controllers and broader graphics/resource coverage remain open
for the terrain adapter. See VALIDATION.md and VALIDATION-LOG.md.

The first candidate (`aa5efa9`)
was tested on 2026-10-07 but its 26.51.5-only loader gate disabled it on the
installed 26.51.6. Screenshots showed unchanged partial cave rendering; no
native request was replaced. `d56b81e` admits that installed patch and
logs arming separately from the actual substitution. See VALIDATION-LOG.md.
Earlier game checks found normal/FreeCamera
terrain culler type 3 and spectator type 5; caves were clipped at straight
chunk boundaries from inside solid blocks, while a camera inside open cave
space rendered normally. A separate forced update to 5 fought native requests
for 3 and blanked the view. Querying spectator/game type did not change the
selection. The old probes and failed hook are retained in BACKLOG-DONE.md and
VALIDATION-LOG.md; they are not evidence that this candidate works.

`FreeCameraCulling.cpp` intercepts the primary renderer's native
`updateLevelCullerType` request. It changes only a requested type 3 to 5 during
the owning FreeCamera session, then calls the original once. Other requested
types and other cameras pass through. Ending FreeCamera lets the next native
request choose the normal culler. The replacement avoids the prior independent
3/5 requests; cave drawing and restoration are maintainer-confirmed, while
native call frequency was not measured. Pure tests cover all
byte-sized requests and stable selection across
repeated activation/exit frames; they do not exercise the game renderer.

The candidate requires Minecraft executable file version 1.26.51.1
(launcher version 1.26.51.01) and LeviLamina Client 26.51.5 or 26.51.6. These
have different coverage: cave drawing was checked on 26.51.6; 26.51.5 has not
been re-tested with this adapter. A pre-render hook
binds lazily from an actual primary renderer while FreeCamera is active. The
SDK virtual-call thunk provides the slot; the implementation must be executable
code in the game module. There is no hard-coded vtable slot or game address.
Only requests on that render thread for that primary renderer and the current
client/player session can change. No game object pointer is retained between
callbacks. An unmodified native type-3 request must first leave the typed
`mLastCullerType` field at 3. Every later overridden request must retain 5;
a mismatch disables the adapter. These checks establish a narrow call/field
contract, not correctness of cave rendering. A warning explains a disabled
path; the rest of FreeCamera retains its previous rendering behavior.

Ordinary-build startup messages report the detected loader and
`FreeCamera terrain: adapter armed; awaiting FreeCamera renderer` after the
version gates pass. One-time messages identify the binding and first substitution:
`FreeCamera terrain: culler hook bound at virtual slot ...` and
`FreeCamera terrain: native request 3 -> 5 retained`. No trace option is needed.
Absence of the latter after flight means the replacement was not observed;
capture the log before pursuing a different path. Player game type, abilities,
position, camera input policy and network packets are not modified here.
Terrain beyond the client's loaded area remains outside this scope.

Further coverage to check when needed (basic cave rendering passed locally
and on BDS; the following individual cases were not all reported):

- Start FreeCamera above ground, enter solid ground near a known cave, rotate
  and cross chunk boundaries. Compare the surrounding cave/terrain with a
  separate spectator view; watch for blank frames, disappearing chunks or
  repeated rebuilding after standing still.
- Fly through open cave space and back to the surface; inspect terrain,
  shadows and overlays. Toggle off/on repeatedly and confirm the normal view
  and player controls return without flicker or missing terrain.
- Hold/Toggle operation, camera retention through menus/focus loss and OFF on
  dimension travel were confirmed in the follow-up. Check world exit/re-entry
  and death separately for the terrain adapter.
- Check ordinary first/third person, Freelook and Zoom with FreeCamera off.
  Check the player's mode and body remain unchanged from another player's
  view. BDS cave drawing passed; remote body appearance remains unverified.

## Boundaries

### Flight speed controls (L-26, implemented 2026-09-30)

FreeCamera saves a base flight speed of 5-100 blocks/s, in steps of 5 and
defaulting to 20. Two unbound Press actions change it by 5 during active
FreeCamera gameplay, persist the change and show a message toast. Holding a
speed key does not repeat; the endpoints do not wrap. The settings child and
the actions use the same normalization.

Native extraction captures `MoveInputState::Flag::SprintDown` before consuming
movement. Revised 2026-09-30 after maintainer testing: a fresh sprint-key press
while the combined forward axis is positive starts a session-owned sprint.
Releasing the sprint key keeps it active until forward input stops, including
strafe-only, backward or opposing directions. A press while stationary does
not reserve a later sprint. Forward diagonals receive the same boost: double
only world-horizontal motion after ordinary diagonal normalization, leaving
vertical displacement unchanged. Maximum horizontal speed is 200 blocks/s.
Menus, focus loss and ending the session cancel sprint; resuming with an
already-held key does not count as another press. The speed setting updates
an atomic configuration without restarting the detached session. No player
sprint state, position, game mode or key mapping is changed.

Detail action labels read "Increase speed" / "Decrease speed"; Hotkeys names
FreeCamera explicitly. Opposing keyboard flags remain cancelled even if the
native analog vector is nonzero.

The maintainer confirmed five-step speed adjustment, bound speed keys, the
previous held horizontal-only boost on `7e72244`. On `41b1ff6` they reported
checking the revised build in game and finding no problems. The supplied
checklist included the revised labels and sprint behavior; individual input/
menu/focus cases and restart persistence were not reported separately.

Freelook changes camera rotation while retaining the player's position and
rotation. FreeCamera additionally changes camera position while leaving the
player in place. Neither feature may simulate this by temporarily teleporting
or rotating the player, changing game mode, sending camera commands, or relying
on server support. Both share one detached-camera session; switching modes must
not leave two input owners or two camera overrides active.

### Position reference (L-76; rapid-motion fix L-77)

FreeCamera exposes a saved Position reference choice, Player or World. Missing
keys default to Player, preserving the existing behavior. `FreeCameraPosition`
owns only an activation eye/reference and the selected mode. The original
UI-render writer combined it with displacement and the current player's eye.
Player uses body-relative displacement; World writes the difference between
the fixed world target and the current eye into the existing camera offset.
Compensation runs even at zero flight displacement, including while a menu
pauses flight. No player transform, movement state or gameplay medium is changed.

Changing the choice rebases the reference at the current target, preserving
the position without restarting movement or rotation. Detached target readouts
use the same position calculation. Ending a session clears the reference;
reactivation captures the new eye. Speed and forward-only sprint are shared by
both modes. Pure tests cover body movement without input, continued flight,
switching in both directions, reset/reactivation and non-finite values.

The maintainer confirmed position retention, live switching and release on
`43c4211`, but rapid body movement/elytra caused visible corrections (L-77).
First fix: `CameraAPI::$tryGetActorInterpolatedPosition` keeps its vanilla
return value and calls the offset writer with that frame's interpolated body
position plus the current eye/body difference. Only the owning client and its
local actor can write. Once reached, World skips after-UI tick-position
compensation; Player retains the existing constant-offset writer. A one-time
session log records callback reach. If the native route is not reached, the
previous writer remains as a fallback. No actor transform or API return value
is changed. Pure tests cover rapid motion over five interpolation alphas.

On `d20fdf8` the maintainer confirmed improved elytra motion, normal reference
switching and release. The log confirms native interpolation-writer reach;
it does not establish every caller's consumption order. Check different frame
rates, menus/focus, targeting and exit/death/dimension cleanup on the normal
release build. The adapter remains experimental.

### Lamium view entry (L-78)

The common opener for Settings, Shapes, Hotkeys and HUD layout now calls the
same input suspension as focus loss instead of resetting all camera sessions.
Toggle FreeCamera keeps its displacement, world reference and detached pose;
movement input, timing and sprint are cleared. While a view owns input, the
existing frame logic pauses flight and retains the camera. Zoom/Freelook pause
while keeping toggle requests; Hold actions end through input invalidation's
release dispatch. Closing does not reactivate stale movement. Explicit off
and world/death/dimension cleanup still use the existing reset paths.
On `d3f0293` the maintainer confirmed Toggle Player/World position/orientation
retention and paused flight through Lamium views, plus normal inventory,
window movement and explicit release. Hold and broader lifecycle behavior
remain unverified; no individual Escape/Close results were supplied.

## Earlier SDK and probe record (2026-09-23)

The sections below preceded the camera-entity implementation described above.
The view-matrix approaches did not produce the detached view and were abandoned.
Their diagnostic options remain development tools; their "next" instructions
and "not completed" statements describe that earlier checkpoint. The L-37
implementation above and completed follow-up in BACKLOG-DONE record the
current cave-visibility path.

### SDK surfaces inspected

In the 26.51.3 client SDK:

- `LocalPlayer::_applyTurnDelta(Vec2 const&)` is already intercepted by Zoom.
  Detached look input should be routed through that same integration point,
  avoiding competing hooks with different sensitivity or cancellation rules.
- `LevelRendererPlayer::setupCamera(mce::Camera&, float)` is exported. It is a
  candidate for applying a final render-camera override after vanilla setup.
- `mce::Camera` contains view/world/projection matrix stacks, inverse view,
  right/up/forward vectors, position, and frustum. It exports
  `updateViewMatrixDependencies()`. Changing position or a single matrix alone
  is insufficient evidence that all rendering/culling consumers agree.
- `MinecraftCamera::CameraComponent` separately stores orientation, position,
  projection parameters, post-view transform, and saved matrices. An ECS
  override is another candidate, but its ordering relative to rendering and
  player input must be established first.
- `VanillaCamera::UpdatePlayerFromCameraComponent` contains a look mode. Its
  existence makes it necessary to inspect camera-to-player propagation before
  changing a game camera entity. The declaration does not prove when it runs.
- `IClientInstance` exposes the camera, camera registry/systems, and weak camera
  entity references. `CameraRegistry` owns game/debug camera entities and
  exports preset/entity setup; its declaration provides no simple standalone
  register/unregister-camera API. Do not rebuild the global registry to add a
  Lamium camera.
- `ICameraAPI` exposes actor positions/rotations, movement input, clipping,
  timing, and viewport information. `IVanillaCameraAPI` exposes bobbing, vehicle,
  portal, sleeping, and perspective information. These declarations do not
  establish a supported isolated-camera lifecycle.

### Initial integration experiment

For the next hold/drag experiment, `camera_trace` also records three independently
bounded Freelook stages (32 records each per process): successful session begin,
native turn deltas accepted by that session, and relative degree angles actually
written to the render view. Startup camera samples cannot consume these budgets.
These records contain no player/world identifiers or positions. They distinguish
an unobserved hold from missing turn input or missing render application; they do
not by themselves prove body isolation, correct sensitivity, or visible rotation.
The ordinary build has no Freelook trace code.

First observe `setupCamera` during first/third-person rendering and establish
the view-matrix convention, whether vanilla reconstructs it every frame, and
which camera position drives culling, world overlays, and hand rendering.
Prefer a per-frame render override if these consumers stay consistent; otherwise
investigate the ECS camera update sequence. Do not choose an approach solely
because a hook links successfully.

Capture only Lamium-owned pose/input state. Avoid retaining actor or camera
component pointers between callbacks. Discard the detached pose on world exit,
dimension change, player replacement, focus loss, menu entry, disable, or a
missing camera. Returning to vanilla should remove the override, not restore a
stale player transform. Repeated held-key events after cancellation must not
reactivate the session until a fresh press, matching the existing input layer.

FreeCamera needs movement input ownership and suppression of player movement,
attack, and use while detached. Freelook needs explicit handling of interaction
aim versus displayed aim before it is considered ready. Settings opening must
cancel the detached session, and Zoom must use the same camera/input policy.

### Probe procedures and observations

#### Opt-in position override experiment

`xmake f --camera_position_probe=y --camera_probe=n` followed by
`xmake build Lamium` enables a two-block camera-local rightward displacement
while Zoom is held. The probe applies a translation to the fresh vanilla view
after setup, retains vanilla's world origin, and lets the subsequent camera
dependency update run normally. It does not alter player position, movement,
game mode, or packets. Releasing Zoom removes the per-frame override. The
rotation probe takes precedence if both options are enabled; use one at a time.

This is a local development experiment, not FreeCamera. Zoom still affects FOV,
movement still belongs to vanilla, and the existing camera trace is enabled.
Check visible parallax, near/far geometry, Shape alignment, culling at the view
edges, and return to vanilla before extending the translation to a moving
session. In particular, the separate render origin may have downstream users
that do not consume the adjusted view. Disable with
`xmake f --camera_position_probe=n` and rebuild before ordinary use.

#### Opt-in view override experiment

`xmake f --camera_probe=y` enables a development-only fixed 20-degree
camera-local yaw while the existing Zoom action is held in gameplay. It also
enables the trace hooks. The probe composes with the fresh vanilla view after
each setup call, marks the view stack dirty through its mutable accessor, and
leaves dependency updates to the normal render path. It does not write player
position/rotation or cached camera dependencies. Zoom release, cancellation,
disable, and non-gameplay screens stop applying the override; it does not restore
a saved matrix. Disable with `xmake f --camera_probe=n` and rebuild.

This is not Freelook: mouse input still follows vanilla player rotation,
Zoom still changes FOV, and interaction aim has not been separated. Use only in
a local validation world to check whether the modified view remains stable
across frames, aligns with world overlays, and returns on release. Its runtime
behavior is not yet verified. A complete feature still needs its own input
action, detached pose, cancellation policy, and interaction handling.

#### Read-only observations

An optional read-only probe is available with `xmake f --camera_trace=y`
followed by `xmake`. It observes the existing Zoom hook lifecycle and samples
`setupCamera` once per 120 calls, up to 32 samples per process. It records the
interpolation factor, availability of the prior view matrix, finite matrix
values, maximum pre/post view change, view/inverse-view identity error, and
camera basis lengths. It does not record positions, world identifiers, or
paths, and does not modify camera matrices or player state. Disable it with
`xmake f --camera_trace=n` and rebuild for ordinary use.

The probe also observes the first 64 dependency-update calls after setup has
been seen on the calling thread. Each records a thread-local setup serial,
whether setup is still on the stack, and whether the dependency update is for
that active setup camera, followed by post-update inverse error and basis
lengths. Camera identity is retained only within the synchronous setup scope;
an update outside that scope is deliberately not identified with a prior camera.
The two log streams have separate limits (32 setup samples, 64 updates).

On 2026-09-23, ordering-probe build `dc02bbc` was installed with matching DLL
hashes and exercised in the same local creative world, starting in rear
third-person view. All 64 dependency samples were outside setup and finite.
The first setup sample (serial 1) still had zero basis lengths; the immediately
following dependency samples at serial 1 had unit basis lengths and inverse
errors of zero and approximately `1.53e-5`. The sample budget ended at setup
serial 43 during startup. World and Shape rendering remained visible and the
game exited normally. This does not cover subsequent perspective transitions
or deliberate rotation.

The observed order supports trying an opt-in render-only rotation after vanilla
setup, allowing the existing later dependency update to process the modified
view. It does not prove camera identity outside setup: `sameCamera=false` there
means no identity comparison was possible, not that it was a different camera.
The next experiment should apply a reversible rotation to the fresh view matrix
on each call, with no player transform writes, then inspect world/overlay
alignment and dependency consistency. Do not extend passive tracing indefinitely
instead of testing that integration hypothesis.

The diagnostic build compiles and links against SDK 26.51.3. On 2026-09-23,
build `6bfaeb4` was installed with matching source/destination DLL hashes and
observed in a local creative world on Minecraft 1.26.51.01. First-person world
rendering and an F5 switch to rear third-person rendering remained visible,
including the existing Shape overlay. Minecraft then exited normally.

The flushed log contained exactly 32 samples (0 through 31). All reported finite
view/product matrices and an available pre-call view. Sample 0 had basis lengths
`0/0/0` and inverse error `0.74165905`; subsequent samples had lengths `1/1/1`
and inverse error `0`. Pre/post view differences stayed approximately `0.741659`.
This demonstrates that the hook runs and changes the view in this scene, but
cached camera dependencies are not valid at every observed initialization stage.
The perspective transition was not tagged in the trace, so these values must not
be assigned to a specific perspective or used to claim transition coverage.

Next, observe `updateViewMatrixDependencies()` ordering relative to setup and
compare fresh inverse/basis values during deliberate camera rotation. The
stationary samples cannot establish whether cached dependencies belong to the
current or previous frame. A zero view change alone would likewise not establish
that vanilla reconstructs the camera on every call. These measurements do not
establish culling, input ownership, or detached-camera correctness.

Verify body position and rotation stay unchanged from another local observation
or suitable client diagnostics, and check remote behavior before claiming
multiplayer support. Exercise release/toggle, focus loss, menus, dimension/world
changes, death, riding, sleeping, perspective changes, Zoom, and another camera
mod. Check view/frustum alignment, overlay coordinates, hands, and unloaded
terrain. Instrumentation should remain local and optional; none of these checks
has been completed for detached cameras.
