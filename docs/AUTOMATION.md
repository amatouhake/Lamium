# Lightweight input automation

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/AUTOMATION.md`.

**Auto Attack** and **Auto Use** (L-34) press the attack or use button for
the player: periodically, held down, or as fast clicks. **Permanent Sneak**
and **Permanent Sprint** (L-43) keep sneaking or sprinting without holding
the key. All four ship unbound and without the Experimental badge.

| Row | Switch | Keys (action ids) |
|---|---|---|
| Auto Attack | `interaction.autoAttack` (session, never saved) | on/off `periodicattack`; next mode `cycleattackmode`; Fast click only while held `cycleattacktrigger` |
| Auto Use | `interaction.autoUse` (session, never saved) | `periodicuse`, `cycleusemode`, `cycleusetrigger` |
| Permanent Sneak | none saved | `permanentsneak` |
| Permanent Sprint | none saved | `permanentsprint` |

Saved options per action: `attackMode`/`useMode` (by name; an unknown name
loads as Periodic), `attackTicks`/`useTicks` (1-1200, default 10),
`attackClicks`/`useClicks` (1-10, default 1), `attackHeldOnly`/`useHeldOnly`
(default off). Feature ids stay `periodicAttack`/`periodicUse` and the
action ids keep their old names; they are saved and never renamed.

## Behavior

### Automatic attack and use

Decided 2026-09-25 and revised the same day after the maintainer's review:
one feature per mouse action, each with exactly one switch, one mode and one
set of keys.

- **Switch**: the feature row's switch; its key toggles it. On means it is
  working in the chosen mode. Session state: never saved, off after leaving
  the world or restarting.
- **Mode** (saved): Periodic (clicks every N ticks), Hold (keeps the button
  pressed: mining, eating, shield, bow) or Fast click (N clicks per tick).
  One mode at a time; the "next mode" key cycles it. Changing mode while on
  releases the old mode's press first. Periodic clicks at once when started.
- **Settings**: Periodic interval shown as "10 tick (0.50s)"; Fast click
  shown as "1/tick (20/s)". Both rows are always listed; their labels name
  the mode.
- **Fast click only while the button is held** (saved switch, Fast click
  only): off by default, so Fast click acts while switched on like the other
  modes. Hold "while held" would be vanilla and Periodic did not need it
  (maintainer, 2026-09-25). Toasts and status say "Fast click (while held)"
  when it is on.
- **Keyed option rows**: a setting and the key that changes it share one
  row. The "Fast click only while held" row has its switch and key; the Mode
  row has its stepper (left of the key column) and the "next mode" key.
  Breaking Restriction's switch and mode rows carry their keys the same way.
  Those actions get no row of their own under the feature; Hotkeys still
  lists them. Clicking the key cell edits the binding.
- **Nothing stops it implicitly.** Only the switch (row or key) turns it
  off, plus leaving the world. A manual press takes priority while held and
  automation resumes after release: Hold presses again, Periodic continues
  on its schedule and skips (never queues) clicks during the hold. With Fast
  click, a held button gives N release/press pairs per tick, ending pressed;
  otherwise N press/release pairs.
- Menus, focus loss, death, dimension changes and detached cameras pause it,
  releasing any synthetic press; it resumes when gameplay input returns.
  Focus loss, screen and dimension changes also forget the physical held
  state (the release may never be reported).
- The status element shows "Auto Attack: Periodic", with "(paused)" while
  paused ([HUD.md](HUD.md), Automation status). Toasts name the mode:
  "Auto Attack: Hold  ON".
- Help text warns that servers may treat very fast input as cheating.
- Why this shape: the first version had separate Periodic/Hold/Fast toggles
  plus a parent row without a switch, so it was unclear what was on, and
  stopping on a manual click felt arbitrary.

### Permanent Sneak and Sprint

- No saved switch (CAMERA.md "Decided 2026-09-26", shared with Zoom,
  Freelook and FreeCamera): the switch shows whether they are wanted, the key
  or a click flips it, and the effect starts when gameplay allows.
- Menus and focus changes pause them; they resume afterwards. Death,
  dimension change and leaving the world turn them off. Nothing is saved
  across restarts.
- Vanilla still decides whether sprinting starts (forward input, food,
  blindness).
- A physically held sneak or sprint key is never cleared by Lamium.

## Code

- `src/features/interaction/AutoClick.h` (pure, `tests/AutoClickTests.cpp`):
  the state machine per button. `configure` follows the settings every tick
  (only changes act), `tick` counts client ticks and never queues catch-up
  clicks, `physical` hands the button to the user, `forgetHeld`, `suspend`,
  and `update` yields the edges for one native input update.
- `src/features/interaction/AutomationInput.h` (pure,
  `tests/AutomationInputTests.cpp`): the session intent shared with
  Permanent Sneak/Sprint. `AutoMode.h`: the mode enum and its names.
- `src/features/interaction/PeriodicInput.cpp`: the Auto Attack/Use adapter.
  Hooks `InputHandler::registerButtonDownHandler`/`registerButtonUpHandler`
  capture the vanilla callbacks for `button.destroy_or_attack` and
  `button.build_or_interact`, scoped to their owning `InputHandler` (weak
  owners; wrappers keep forwarding original input after stop). `Update`
  (after `InputHandler::tick`) emits edges only for the primary client whose
  input handler is that owner, passing `FocusImpact::DeactivateFocus`.
  `DestroyOwner` drops callbacks without replay. `ChangeDimension`
  (`LevelRendererPlayer::$onWillChangeDimension`) interrupts. Timing follows
  `ClientLevelTickEvent`; `endSession` switches both off at world exit.
  Eligibility (`eligible`) also excludes sleeping, riding and Lamium's own
  screens.
- `src/features/interaction/PermanentSneak.cpp` (both features):
  `ExtractSneakInput` hooks `ClientInputUpdateSystem::extractRawHIDInput`,
  checks that the input is the primary local client's
  (`ClientMoveInputHandler::getMoveInput`), and passes vanilla a transient
  copy with `SneakDown`/`SprintDown` set in `mRawInputState`. Synthetic bits
  are never stored in the user's HID state. `SneakDimensionChange` cancels
  both.
- Settings migration (`SettingsStore.cpp`): old seconds values
  (`attackInterval`/`useInterval`) load as seconds x 20 ticks; a short-lived
  `attackTrigger: "held"` loads as `attackHeldOnly`.
- `automation_trace` (xmake option, `AutomationTrace.cpp`): logs callback
  registrations (at most 512: button name, edge, suspendable flag) and
  dispatches (at most 64, only names containing `build` or `attack`, so menu
  navigation does not use up the budget; widen the filter before tracing
  other buttons), plus adapter edges and pause reasons. Read-only: it
  forwards callbacks with their original arguments, records no typed text,
  inventory, identity or world data. Needs a fresh process; callbacks
  registered before mod load are not covered.

## Known

- One left click invokes `button.destroy_or_attack` down and up, followed by
  `button.pointer_pressed`; one right click invokes `button.build_or_interact`.
  Both register down/up with `suspendable=false`, so the adapter needs its
  own eligibility guard.
- Both action callbacks receive focus value 2 (`DeactivateFocus`): an input
  focus-impact argument, not OS focus loss.
- The client input interfaces expose no direct attack/use dispatcher in the
  SDK.
- Sneak must be set in `mRawInputState` of the copied `MoveInputComponent`:
  setting `mInputState` gave zero output `SneakDown` samples while the hook
  and local match were reached.
- The game registers about 330 button callbacks; a 128-registration trace
  budget ends at hotbar selection, before attack and use.
- Callback registration happens once: the adapter needs a fresh game
  process and does not recover registrations made before its hooks, or after
  disabling and re-enabling the mod in one process.

## Tried

- Disarming on a manual click or on opening a menu (first Periodic adapter):
  felt arbitrary; replaced by priority-while-held and pause/resume.
- Intervals in seconds (0.1-60): replaced by client ticks.
- Setting `mInputState` instead of `mRawInputState` for sneak: vanilla
  ignored it.
- Not to be tried: guessing button hashes, invoking GameMode methods,
  building attack/use packets or setting the actor's sneaking flag directly
  instead of the vanilla input path.

## Limits

- Whether several clicks within one input update all land on servers is
  unchecked; see [VALIDATION.md](VALIDATION.md), rows "Auto Attack / Auto
  Use (L-34)" and "Permanent Sneak, Permanent Sprint (L-43)".
- Synthetic presses go only to the primary client; split-screen is not
  covered.
