# Detached camera implementation notes

Freelook now has an experimental integration, disabled and unbound by default.
FreeCamera is not implemented. This records SDK evidence and remaining
integration questions; it does not claim detached-camera runtime validation.

`DetachedCameraMotion` now supplies a game-independent displacement session for
the future FreeCamera adapter. It consumes camera-basis vectors, analog axes,
speed and elapsed time; it normalizes diagonals, limits a stalled update to
0.1 seconds, caps supplied speed at 100 blocks/second, and discards the session
on invalid input or owner replacement. These are initial internal bounds, not
user-facing settings. Cancellation removes the displacement without retaining
or restoring any player transform. This component is not connected to native
movement or rendering yet, and does not make FreeCamera available in-game.
The adapter still needs to anchor the activation eye, own/suppress local
movement, share the detached angular session, and validate render/culling
coordinates and all lifecycle exits.

`camera::consumeMovement` is the native extraction boundary prepared for that
adapter. After vanilla HID extraction it consumes `RawMoveInputComponent`'s
horizontal axes and momentary jump/sneak/ascend/descend flags, then clears only
the extracted movement axes/flags. It does not alter the stored physical input
or look/selection flags. It is not called by a runtime hook yet. Axis signs,
keyboard/controller behavior, subsequent movement consumers, and resumption of
held physical keys must be verified before enabling the adapter. Culling/world
overlays use a separate render camera origin; translating only the view matrix
is not sufficient evidence of a correct position override.

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
nonfinite/extreme input. The experimental `freelook` Hold action starts an
angular session from the player's current pitch/yaw; native turn input is
consumed for that local player. The camera API's actor-rotation query returns
the session angles for the local player, so vanilla derives render, culling and
third-person boom orientation from the detached pose.
Features and Hotkeys expose the action, with a separately persisted enable flag.
Release, settings entry, focus loss, world exit, dimension transition, camera
configuration changes, and non-gameplay screens discard the detached pose.
Cancellation retains a release latch: native key repeat cannot restart an
interrupted hold. The action's release callback clears that latch. Invalid input
and owner replacement follow the same rule. If the platform loses a key-up during
focus loss, one press/release may be needed before the next activation; silently
restarting a still-held input is not used to recover. Unit tests cover these
cancellation/repeat/release sequences; native focus recovery remains unverified.

User testing of the earlier view-matrix override (2026-09-23) showed that the
player and view both stayed fixed while terrain in the turned direction was
culled: culling consumed the modified view, but rendering did not. That override
was removed in favor of the actor-rotation substitution above. Turn scale and
sign are no longer assumed: ordinary (non-detached) turns record the ratio of
vanilla's actual rotation change to the native delta per axis, and Freelook
applies that ratio (1 until observed). Runtime validation of the replacement is
pending.
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
This narrows native hook effects to the current owner; it does not implement
independent simultaneous split-screen sessions or prove split-screen support.

The interaction guard now intercepts GameMode attack, start/continue/final block
destruction, start/continue/final placement, item use, use-as-attack, use-on-block
and entity interaction. While a valid detached session owns that local player,
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

## Boundaries

Freelook changes camera rotation while retaining the player's position and
rotation. FreeCamera additionally changes camera position while leaving the
player in place. Neither feature may simulate this by temporarily teleporting
or rotating the player, changing game mode, sending camera commands, or relying
on server support. Both share one detached-camera session; switching modes must
not leave two input owners or two camera overrides active.

## SDK surfaces inspected

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

## Next integration experiment

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

## Required runtime evidence

### Opt-in position override experiment

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

### Opt-in view override experiment

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

### Read-only observations

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
