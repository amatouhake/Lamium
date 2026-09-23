# Lightweight input automation

Periodic Attack, Periodic Use, and Permanent Sneak are planned consumers of
`interaction::AutomationInput`. This state machine is implemented and unit
tested, but is **not connected to Minecraft input yet**. These features are
not currently available in the settings UI.

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
