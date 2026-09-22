# Detached camera implementation notes

Freelook and FreeCamera are not implemented yet. This records SDK evidence and
the remaining integration questions; it is not a runtime validation report.

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
