# Lightweight input automation

## Auto Attack / Auto Use (L-34, 2026-09-25)

Periodic Attack/Use became Auto Attack / Auto Use (DESIGN "Automatic attack
and use"). Feature ids stay `periodicAttack`/`periodicUse`; the
`periodicattack`/`periodicuse` actions are now the on/off switch, and
`cycleattackmode`/`cycleusemode` cycle the mode (all unbound).

- Settings: `interaction.autoAttack`/`autoUse` (switches, never saved),
  `attackMode`/`useMode` (saved by name, unknown names load as periodic),
  `attackTicks`/`useTicks` (1-1200, default 10; old seconds values migrate by
  rounding seconds x 20), `attackClicks`/`useClicks` (1-10, default 1) and
  `attackHeldOnly`/`useHeldOnly` (Fast click only while held, default
  off; a short-lived `attackTrigger: "held"` still loads; hotkeys keep the
  ids `cycleattacktrigger`/`cycleusetrigger`).
- `interaction::AutoClick` (pure, `tests/AutoClickTests.cpp`) is the state
  machine per button. The adapter calls `configure` with the settings and
  `tick` on every `ClientLevelTickEvent`; edges are delivered from the native
  `InputHandler::tick` through the captured vanilla callbacks.
- A physical press takes priority while held; Hold presses again after
  release and Periodic skips (not queues) clicks during the hold. Fast click
  gives N press/release pairs per tick (Always), or N release/press pairs
  while the button is held, ending pressed.
- Losing gameplay input (menus, focus, death, detached camera, another
  client) suspends and releases any synthetic press; it resumes when input
  returns. Focus loss, screen and dimension changes also forget the physical
  held state. World exit switches both off.

Unverified in game: whether several clicks within one input update all land
(attacks, scaffolding, snowballs), whether Hold keeps mining, and behavior
on servers. The rest of this file records the earlier Periodic adapter.

Periodic Attack, Periodic Use, and Permanent Sneak share the runtime intent
contract in `interaction::AutomationInput`. Permanent Sneak now has an
experimental native adapter and an unbound toggle action in Features/Hotkeys.
Its basic toggle, settings-screen cancellation, and app-switch cancellation have passed a local-world
runtime check; broader compatibility is still unverified. Periodic Attack and Use
now have an initial native adapter with unbound session toggles in Features and
Hotkeys. Periodic Attack has passed a basic local empty-hand check, including
toggle-off, settings cancellation, and a manual click disarming the action.
Periodic Use has passed a local egg-consumption check, toggle-off, manual-click
disarming, and cancellation on opening Settings. The broader interaction matrix
remains unverified.

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

## Periodic adapter checkpoint

The initial adapter captures the two observed vanilla action callbacks during
registration and scopes them to their owning `InputHandler`. It advances after
that handler's native input tick, only for the primary client whose
`ClientInputHandler` references the same owner. Native callbacks receive the
observed `DeactivateFocus` argument. No input hash, packet, or GameMode action
is constructed directly.

Each press lasts one input update. Attack and Use each expose an Interval in
their feature settings, in seconds (0.1–60, default 0.5). The adapter snapshots
the interval on activation; opening Settings cancels an active session, so
editing does not leave a previously scheduled click pending. Missing values
retain the old 500 ms cadence. Active automation appears at the upper right,
independent of Info HUD and gameplay key hints. The Automation status setting
can hide these lines without changing input intent. Activation
is never persisted. A manual down event disarms that action and transfers held
state to the physical input; cancellation does not release underneath a held
physical button. Common input invalidation, dimension changes, and runtime
disable cancel intent. Owner destruction discards callbacks without replaying
them against a disappearing input handler. Callback wrappers use weak owners
and continue forwarding original input after the adapter is stopped.

This initial registration-based adapter requires a fresh game process. It does
not recover callback registrations that happened before hook installation, or
rebuild captures when the mod is disabled and enabled within the same process.
Runtime checks must cover synthetic attack/use, physical takeover, menu/focus
cancellation, world exit, and continuous-use items before this feature can be
treated as usable. Build success alone does not establish those behaviors.

Initial adapter smoke on 2026-09-23, DLL
`C822AF983F6CA5BD767D15F31EBA1E72CDD8EC3B12333BAC530EFEE19B4B181D`:
startup and local-world entry succeeded, and Features accepted a custom Periodic
Attack binding. Activation did not produce sufficient visual or diagnostic
evidence to establish repeated attacks. Settings was reopened to cancel any
remaining intent. This is not a successful automation check. The next trace
adds activation rejection reasons, bounded edge counts, and update-owner
eligibility diagnostics to distinguish capture, activation, and dispatch failure.

Follow-up on 2026-09-23, commit `71883c7`, DLL SHA-256
`2A9A153A7CEC0A7C91B3B391F75BB3B2A084F50E9ABBDFC5586EEBAC45E3B9D8`,
in a local survival world on Minecraft 1.26.51.01 / LeviLamina Client 26.51.3 /
Deesse UI 1.3.9:

- The custom Periodic Attack key activated arm swings with an empty hand.
  The first four down edges were about 0.51 seconds apart; their corresponding
  up edges followed about 17–22 ms later. Toggling off recorded 39 presses and
  39 releases.
- Reactivating and opening Settings stopped after 23 matched presses/releases.
  The rejection diagnostic showed the primary client, armed client, and input
  owner matched; gameplay eligibility was false while Settings owned input.
  Closing Settings did not rearm: the next toggle logged a new activation.
- A manual left click during a subsequent activation delivered the ordinary
  attack down/up callbacks. The next toggle activated again rather than
  toggling off, establishing that the click had disarmed the session intent.
  This does not test a physically held button overlapping a synthetic press.

These observations supersede the earlier inconclusive attack smoke only.
They do not establish entity damage, block breaking, Periodic Use, continuous-use
items, world/dimension transitions, multiplayer, or hot re-enable support.
The configurable-interval and status HUD revisions pass build and settings
tests. A local runtime check of `99e1641`, DLL SHA-256
`B14199694DDE22CEC291E808BA3CA97F1AACD01F44EC0270F55BFBFED1034A07`,
confirmed the following in the same environment:

- Periodic Attack shows its active line; enabling Permanent Sneak adds a second
  line. Both disappear when Settings opens, and neither returns on closing it.
- The feature's Interval row displays 0.5 seconds and its 0.1–60 range. Right
  arrow changes it to 0.6 seconds without a save button.
- After closing Settings and explicitly reactivating attack, four traced down
  edges were separated by 615, 602, and 600 ms. Toggling off removes the status.

Alternate UI scales/locales and overlap with other configurable HUD positions
still need runtime coverage.

### Periodic Use local check

The same `99e1641` DLL and environment were used for a subsequent local survival
check on 2026-09-23:

- The in-game editor accepted a custom Periodic Use binding, with its independent
  interval left at 0.5 seconds.
- Activating with 14 eggs in the selected hotbar slot displayed the active status
  line and reduced the count to 13, then emptied the slot while no manual use
  input was supplied. Projectile flight was not separately captured.
- The first four traced down edges were separated by 516, 503, and 500 ms;
  corresponding releases followed about 16–17 ms later. Toggling off recorded
  33 matched presses/releases and removed the status line. Periodic input
  continued after the slot became empty until explicitly stopped.
- On a subsequent empty-hand activation, a manual right click removed the
  status. The next toggle logged a new activation, confirming that the manual
  click had disarmed the intent.
- Opening Settings during that activation stopped it after 23 matched
  presses/releases. The diagnostic reported matching client/owner identity
  with gameplay eligibility false. In a follow-up, closing Settings left the
  status absent; the next explicit toggle logged a new activation and restored
  the status, confirming that closing Settings had not rearmed use.

The follow-up also exercised the Automation status visibility option through
the in-game Features list. Turning it off saved `interface.automationStatus`
as false without a save button. After closing Settings and explicitly arming
Periodic Use with an empty hand, the status stayed hidden while diagnostics
confirmed repeated input edges. The next toggle stopped the action.
The stop diagnostic reported 23 presses and 22 releases: this diagnostic is
emitted before `cancelButton` sends its final synthetic release, so these counts
are not final totals and do not by themselves establish a stuck input.
Observing that final release directly remains a diagnostic limitation.

The subsequent diagnostic revision moves the stop summary after cancellation's
release callback. It includes `finalRelease` when that callback was dispatched
and `releaseSkipped` when a release was needed but the primary client no longer
matched. Counts remain bounded at 1000. This revision passes the DLL build and
existing test suite; its new diagnostic fields still need a fresh-process
runtime check and do not retroactively change the preceding observations.

This establishes repeated instant-use consumption in one local world. It does
not establish food consumption, bow charging, other continuous-use items,
placement, physical-button overlap, world transitions, or multiplayer behavior.

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
