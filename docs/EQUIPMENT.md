# Equipment features

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/EQUIPMENT.md`.

Tools that move the player's own equipment without a screen: Tool
Protection, Tool Switch's and Weapon Switch's fetch from the inventory, and
Auto Elytra. All use the screenless move shared with
[HAND-RESTOCK.md](HAND-RESTOCK.md).

| Feature | Section, default | Settings | Toggle action |
|---|---|---|---|
| Tool Protection | Interaction, on | `interaction.toolGuard`; child "Never let it break" (`toolGuardStrict`, on) | `toolguard` |
| Tool Switch fetch | Inventory, child of Tool Switch | "Fetch from inventory" (`toolSwitchInventory`, off), "Fetch into" (`toolSwitchSlot`) | `toolswitch` (the feature) |
| Weapon Switch | Inventory, off | `weaponSwitch`; "Fetch from inventory" (`weaponSwitchInventory`, off), "Fetch into" (`weaponSwitchSlot`) | `weaponswitch` |
| Auto Elytra | Interaction, off | `elytraSwap`; "Jump with fireworks" (on), "Riptide launch" (on), "Chestplate back after landing" (0-10 s, default 3 s, half-second steps) | `elytraswap`; command "Put on / take off the elytra" (`elytraswapkey`, unbound) |

## Shared move

`movePair` (`src/features/inventory/game/InventoryMove.{h,cpp}`) changes two
player-owned slots at once: inventory slots, the offhand or an armor slot.
It refuses (changing nothing) while another transaction, item-stack request
or move is in progress. Inside the client legacy request scope it calls the
vanilla setters, then flushes the balanced transaction. A transaction left
unbalanced after the setters throws, and the caller stops.

Moves follow the Hand Restock ordering rule: never right behind a
server-side change they depend on. For durability, breaking and hits that
means 150 ms (`restockQuietMs`) after the last break, durability change or
hit.

## Tool Protection

- A held damageable item that wears down to 1 durability while held (or is
  used to mine at 1) is swapped with the same item: main inventory first,
  then other hotbar slots; closest enchantments, then most durability, then
  the higher slot. The worn item takes the replacement's slot; the selection
  never changes. One kept at 1 for Mending and merely selected is left alone.
- Mining with it waits (no progress) until the swap, then restarts breaking.
- Without a replacement, mining stops with a text-only toast "Stopped: the
  tool is about to break". "Never let it break" on keeps a new press from
  mining on; off, a new press mines on and breaks it.
- A worn elytra at 1 (it stops working rather than breaking) is swapped with
  another elytra the same way.

## Tool Switch: fetch from inventory

When neither the held item nor the hotbar has an effective tool for the
block, the fastest one in the main inventory (ties: the higher slot; never
one about to break) is moved into the selected slot and stays there, or
into the "Fetch into" hotbar slot, which is then selected. "Fetch into"
never uses Fake Offhand's slot: if it is set to that slot, fetched items go
to the selected slot and the row warns. On a new press it moves at once if
the last break is 150 ms behind; while a held attack moves to another block,
breaking pauses until then, the tool moves and breaking restarts.

## Weapon Switch

When the local player attacks a living entity (anything with the Mob type
except armor stands), the hotbar item with the highest attack damage plus
the vanilla Sharpness/Smite/Bane bonus against that target is selected
before the attack runs. The held item stays on a tie; otherwise a sword wins
over an equal non-sword. No switch back.

"Fetch from inventory": when the strongest inventory weapon hits harder
than every hotbar item (an equal one stays), it (never one about to break)
moves into the selected slot, or into the "Fetch into" slot, which is then
selected and reported at once, and only when the last hit is 150 ms behind;
otherwise the next hit tries again.

A switch sends the equipment packet at once, as the client's own tick
would, and marks it sent (see Known).

## Auto Elytra

- Put on: the command key, any jump while holding firework rockets in the
  main hand ("Jump with fireworks"), or a Riptide launch ("Riptide
  launch"). The elytra with the most durability comes from the main
  inventory, then the hotbar; it takes the chest slot and the chest item
  takes its slot. No automatic glide: a second jump glides as in vanilla
  (starting it from the mod: [L-71 Start an elytra glide from the mod](backlog/L-71.md)).
- Fake Offhand's slot: while Fake Offhand is on and its target slot holds
  firework rockets, a jump puts the elytra on whatever the main hand holds.
  It is an equipment trigger only: no firework is used and no glide starts.
  It follows the Fake Offhand setting, not the `FakeOffhandTargets`
  capability ([FAKE-OFFHAND.md](FAKE-OFFHAND.md)). (L-246)
- Riptide (maintainer 2026-10-14, 2026-10-30): the trigger is the launch,
  not the charge, since Riptide flies without an elytra. A launch is the
  rising edge of the spin (the player's `RiptideTridentSpinAttackComponent`,
  or the `Damagenearbymobs` actor flag). It arms a wait instead of swapping
  on the spot: the elytra goes on the first frame the player is airborne
  and out of the water. The wait is dropped once the launch has stopped
  carrying them (no spin and no upward speed) for half a second, which is
  what an underwater fight looks like, or two seconds after the spin ends,
  since swimming up also counts as upward speed. A launch in rain or from
  land is airborne a frame or two later. A Riptide spin with an elytra on
  restarts the return delay. Not in a vehicle or Creative.
- Take off: the key again, or after a glide, firework jump or launch, the
  set time after the first landing on ground or in water (maintainer
  2026-10-30); later hops do not restart it, only a new glide does, and the
  swap happens on a touch of the ground. A glide that restarts within the
  delay keeps the elytra. The remembered chest item goes back on, else the
  chestplate with the highest protection (armor, toughness, enchantment
  levels, durability); the elytra takes its slot. With no chestplate the
  elytra stays on. An elytra worn by hand is followed once it glides. A key
  press on the ground waits for the key.
- Only the jump flag is read every tick; the held item is checked on a jump.

## Code

- `src/features/interaction/ToolGuard.cpp`, `ElytraSwap.cpp`;
  `src/features/inventory/ToolSwitch.cpp`, `WeaponSwitch.cpp`,
  `FetchSlot.h`; pure: `EquipmentPlan.h` (durability limits, enchantment
  distance, replacement and chestplate choice, the mining decision, the
  elytra state machine and Riptide wait), `ToolChoice.h`, `WeaponChoice.h`.
- Mining: `src/features/interaction/MiningSession.cpp` owns GameMode
  start/continue/stopDestroyBlock (highest priority). Breaking Restriction,
  Tool Protection and Tool Switch each answer a gate (`MiningSession.h`:
  Proceed, Pause, End, Restart) in that order; the first answer other than
  Proceed applies and the later features are not asked.
- Weapon Switch hooks GameMode and SurvivalMode `attack`. Tool Protection
  also runs a client tick listener.
- Tests: `tests/EquipmentPlanTests.cpp` (incl. hand-worn elytra, key on the
  ground, sprint-jump delay, empty chest), `tests/WeaponChoiceTests.cpp`.

## Known

- The inventory setters record their own transaction actions; the offhand
  and armor setters record none, so `movePair` adds that side's action
  itself. Which sides were recorded is learned by observing
  `InventoryTransactionManager::addAction` during the move.
- The client reports its selected slot from its own tick, after an attack's
  transaction: a switch left to that tick made the switching hit do no
  damage. Hence the immediate equipment packet.
- Whether the local player carries the Riptide spin component or flag on
  the client during the spin on a server is not established.

## Tried

- Reading the transaction's action map after an armor setter: crashed the
  game. Observe `addAction` instead.
- Swapping the elytra at the Riptide launch and undoing it in water: never
  released; replaced by the wait until airborne.
- `Player::tryStartGliding` after a swap: always false
  ([L-71](backlog/L-71.md)).

## Limits

- Moves send with no response tracking; a rejected legacy transaction
  relies on vanilla's correction
  ([L-299 Screenless inventory moves can leave a slot stuck when client and server desync](backlog/L-299.md)).
- Tool Protection's elytra swap is not checked in flight.
- A Riptide launch underwater that reaches the surface later than the
  two-second coast does not equip.
- Unchecked in game: the "Tool Protection (L-62)", "Tool Switch, fetch from
  inventory (L-69)", "Weapon Switch (L-67)", "Fixed-slot fetch (L-97)" and
  "Auto Elytra (L-70)" rows in [VALIDATION.md](VALIDATION.md).
