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

An optional read-only probe is available with `xmake f --camera_trace=y`
followed by `xmake`. It observes the existing Zoom hook lifecycle and samples
`setupCamera` once per 120 calls, up to 32 samples per process. It records the
interpolation factor, availability of the prior view matrix, finite matrix
values, maximum pre/post view change, view/inverse-view identity error, and
camera basis lengths. It does not record positions, world identifiers, or
paths, and does not modify camera matrices or player state. Disable it with
`xmake f --camera_trace=n` and rebuild for ordinary use.

The diagnostic build compiles and links against SDK 26.51.3. Runtime observation
is still pending. A zero view change alone cannot establish that vanilla
reconstructs the camera on every call; it can also mean the camera is stationary.
These measurements do not establish culling, input ownership, or detached-camera
correctness.

Verify body position and rotation stay unchanged from another local observation
or suitable client diagnostics, and check remote behavior before claiming
multiplayer support. Exercise release/toggle, focus loss, menus, dimension/world
changes, death, riding, sleeping, perspective changes, Zoom, and another camera
mod. Check view/frustum alignment, overlay coordinates, hands, and unloaded
terrain. Instrumentation should remain local and optional; none of these checks
has been completed for detached cameras.
