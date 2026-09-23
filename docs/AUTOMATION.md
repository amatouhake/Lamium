# Lightweight input automation

Periodic Attack, Periodic Use, and Permanent Sneak share the runtime intent
contract in `interaction::AutomationInput`. Permanent Sneak now has an
experimental native adapter and an unbound toggle action in Features/Hotkeys.
Its basic toggle, settings-screen cancellation, and app-switch cancellation have passed a local-world
runtime check; broader compatibility is still unverified. Periodic Attack and Use
are not yet connected or exposed in the UI.

Initial local-world check on 2026-09-23: the settings editor accepted a custom
binding and the action logged activation, but the third-person player did not
visibly crouch. DLL SHA-256:
`8E45DEE8B16C7AAB1675C6FF7CC7AFF9181C21D3018FCEBE89C14C0F1973BD29`.
This is evidence against treating the first adapter as functional. Bounded
extraction/local-match/raw-sneak counters now report on cancellation to separate
an unused hook or identity mismatch from an ineffective input bit. No packet or
actor-state fallback has been added.

The diagnostic run (DLL `22C7BBC1B4B76D69525D07FBE6ACC1FC4D941A3537035DD3E1E0BF199D01E72E`)
reported 313 extraction calls, 313 local matches, and zero output SneakDown
samples while armed. This excludes an unused hook or local identity mismatch
for that run. The corrected adapter changes `mRawInputState` rather than
`mInputState` in the transient copy; the runtime results follow below.

The raw-input revision (commit `01604e3`, DLL
`F229ED391E7F245AAD1345081BD39D1D83C4C3A6F849F1702AF3DF9C9A175090`)
passed the following local survival checks with Minecraft 1.26.51.01,
LeviLamina Client 26.51.3, and Deesse UI 1.3.9:

- A custom hotkey activates crouching and maintains it after key release.
- The same hotkey restores the standing pose. Diagnostics reported 472
  extraction calls, local matches, and output SneakDown samples.
- Reactivating, opening Lamium Settings, and closing it restores standing
  without automatically resuming crouching.
- Switching to another app while crouching cancels the intent. Returning to
  Minecraft and resuming from its automatically opened pause screen leaves the
  player standing, without reactivation. Diagnostics reported 315 extraction
  calls, local matches, and output SneakDown samples before cancellation.
  This checks the combined focus-loss/pause transition; focus loss without a
  pause screen (including multiplayer) has not been isolated.

Physical-key overlap, world/dimension transitions, movement/ledge
behavior, and multiplayer remain unverified. The feature is still experimental.

## Runtime contract

- Arming is session state, never a persisted instruction to act on world load.
- Loss of gameplay input ownership cancels and disarms. Closing a menu or
  regaining focus must not restart automation without an explicit activation.
- The native adapter must cancel on focus loss, screen/world transitions,
  runtime disable, and any detached-camera interaction guard.
- Hold mode contributes one press until cancellation. Periodic mode releases
  on the following native input update and schedules from the actual press
  time. Delayed updates never produce catch-up bursts.
- Physical input takes priority. A release edge removes only Lamium's input
  contribution; the adapter must preserve the user's physically held input.
- Interval values must be positive. Interval configuration will be bounded in
  the settings layer before reaching this state machine.

## Integration still required

### Periodic input discovery

SDK 26.51.3 exposes named `InputHandler::registerButtonDownHandler` and
`registerButtonUpHandler` callbacks, but the inspected client input handler
interfaces do not expose a direct attack/use input dispatcher. Do not guess
button hashes or invoke GameMode methods as a substitute for vanilla input.

Configure `xmake f --automation_trace=y` for an opt-in discovery build. It
observes at most 512 registrations and 64 callback invocations per process,
logging the button name, down/up edge, suspendable flag (registration), and
focus impact (dispatch). It forwards each callback with its original arguments
and does not generate input or retain callbacks for replay. No typed text,
inventory contents, player identity, or world data is recorded by this trace.
Hooks start during mod load to catch subsequent client input registration.
Callbacks registered before mod load are not covered; absence of a logged
action is not evidence that the game lacks that action.

The discovery build requires a fresh process and is not intended for hot DLL
unloading: observed callbacks contain trace wrappers for their original
lifetime. Disable stops logging and removes the registration hooks. Rebuild
with `--automation_trace=n` for ordinary use.

Runtime discovery on 2026-09-23 (Minecraft 1.26.51.01, LeviLamina Client
26.51.3, Deesse UI 1.3.9):

- The initial 128-registration budget ended at hotbar selection, before attack
  and use. This was an instrumentation limit, not a missing vanilla path.
- The revised 512-registration budget captured 329 registrations without
  reaching the limit. DLL SHA-256:
  `C234AD031842E4F8A66BD54E38F7A47D47C5EC59ACB9BE1FE312E7B66BC308D8`.
- In a local survival world with an empty selected slot, one left click invoked
  `button.destroy_or_attack` down and up, followed respectively by
  `button.pointer_pressed` down and up. One right click invoked
  `button.build_or_interact` down and up.
- Both action callbacks used focus value 2, named `DeactivateFocus` by the SDK.
  This enum is an input focus-impact argument, not evidence of OS focus loss.
- Both action names have down/up registrations with `suspendable=false`.
  A synthetic adapter still needs its own eligibility guard.

Next establish the native update boundary, callback owner lifetime, and
physical-input ownership before connecting periodic intent. These observations
cover ordinary click dispatch, not successful item use, block breaking, or
synthetic callback replay. The trace itself does not implement Periodic Attack
or Periodic Use.

The first Permanent Sneak adapter hooks `extractRawHIDInput`, verifies that the
input belongs to the primary local client, and passes a transient copy with
`SneakDown` set to vanilla. It never stores synthetic flags in the user's HID
state. Common input invalidation cancels its session intent; dimension changes
and runtime disable also cancel. The checks above establish basic local-world
consumption of the copied bit, not the remaining movement or multiplayer cases.

Use the vanilla local input path rather than constructing attack/use packets
or modifying authoritative actor state directly. The SDK exposes separate
`MoveInputState`, `MoveInputComponent`, and local sneak state; selecting the
correct input update boundary requires runtime verification. Merely setting
the actor's sneaking flag does not establish that vanilla movement, input
release, and multiplayer semantics work.

Validate activation, deactivation, physical-key overlap, settings/chat screens,
focus loss, world exit/re-entry, and disable. For periodic actions also verify
item use/release and breaking behavior, with no delayed burst after a stall.
Unit tests prove the independent lifecycle contract only, not Minecraft input
integration or multiplayer compatibility.
