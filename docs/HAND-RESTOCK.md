# Hand Restock

Current behavior and how it works. Scope decisions are in BACKLOG-DONE.md
(L-66, L-68); test results in [VALIDATION.md](VALIDATION.md) and the evidence
behind them in [VALIDATION-LOG.md](VALIDATION-LOG.md). The screenless move it
uses is shared with the equipment features ([EQUIPMENT.md](EQUIPMENT.md)).
Off by default, toggle action unbound.

## Behavior

- Refill the selected main-hand slot after an observed consumption. The
  selection never changes.
- Top up when the same item falls to the "Refill at or below" setting or
  fewer (default 6, 0 only on depletion; capped below the item's maximum,
  L-102); depletion uses the same planner.
  One source per refill, moving only what fits; each item's own stack size.
- Source: an unlocked compatible main-inventory stack (slots 9-35), the
  smallest (default) or largest by the "Take from" setting (L-102); ties from
  the higher slot (lower rows). Compatibility is vanilla matching,
  components included.
- Child "Restock from hotbar" (on by default): other hotbar slots come after
  the main inventory, in the same size order, ties nearest the selection, then the
  higher slot.
- Remainders: a last water/lava/milk/powder-snow bucket, stew or soup, potion
  or honey bottle that turns into its empty container is exchanged with a
  reserve; the container takes the reserve's slot. Unknown transformations
  fail open.
- Child "Shulker boxes" and "Buckets" (on by default, L-251): off keeps a
  placed or used shulker box out of Hand Restock, and off keeps an emptied
  bucket instead of exchanging it with a reserve. The other remainders and
  filling a bucket from a source are unchanged.
- Totems (L-68): a totem that saved the player is refilled in its slot, the
  selected hand always, the offhand with the child "Restock offhand totems"
  (on by default).
- Never: without a reserve; after drops, manual moves, unrelated changes,
  selection/context changes or failed uses; for damageable items (Tool
  Protection handles those); with a toast or extra key.

## How consumption is recognized

Pure planning is in `RestockPlan.h` (two snapshots, all 36 slots plus the
offhand revalidated before a move) and `RestockUse.h` (send correlation, timed
completion, the quiet period). `HandRestock.cpp` is the adapter: owned item
copies, a weak HUD controller, player runtime id, dimension and a context
generation, never a player pointer across frames. HUD and player inventory
must agree; a disagreement before a move only gives up that operation.

- Uses: GameMode useItem/useItemOn start an observation; the matching
  ItemUse send must follow in the same tick. A placement also sends one
  secondary Use, accepted once. Request-backed uses need an Accepted response.
  Callback success or a send alone never proves consumption: the snapshot must
  show exactly the tracked decrease (or the recognized remainder) with every
  other slot unchanged.
- Food and drink: starting GameMode::useItem returns false and is still
  tracked once startUsingItem runs; holding use re-sends Use for the same slot.
  Completion is the evidence (no release is sent while use is held); a release
  before completion is an interrupted use. completeUsingItem repeats right after
  the next start, so a completion earlier than half the duration is ignored.
- Held use: a new use of the same kind while the inventory shows exactly the
  tracked uses continues the operation and the refill accounts for all of
  them. Other uses while a refill waits (the next bite, the leftover bowl a
  held use retries) stay untracked and do not cancel it; if they change the
  inventory, the snapshot check does.
- Totems: the actor event TalismanActivate plus a tick-to-tick watch that sees
  the totem's slot empty with nothing else changed.

## Ordering the move

The server runs a legacy use in its next tick but applies an inventory
transaction on receipt, in send order. A move sent right after the use reaches
the server first and is rejected (BDS, a41f0f2). So the move waits for a
server inventory update that covers the slot and already shows the
consumption (20-40 ms after placement or eating, local and BDS). Throwables
get no such update; for them the move waits 150 ms after the last tracked use,
which orders it after the server tick regardless of latency and is shorter
than the ~200 ms repeat of a held throw. A stalled server can still reject a
move; vanilla restores its state and nothing is retried. A totem is removed by
the server, so its refill is already ordered after it. Neither the update nor
the delay acknowledges the move itself.

## Moving

`game/InventoryMove` predicts both slots with the client's own setters under
the SDK's client legacy request scope and flushes the balanced transaction
once; the actual ItemStacks are copied so metadata is kept. No manual packet,
hand-edited legacy slot list or auto-opened screen. The resulting snapshot is
checked immediately and once more on the next tick; that is a prediction
check, not server confirmation. Server updates during prediction cancel
observation and no old snapshot is written back.

Deadlines only cancel: one second after an ordinary or completed use; a timed
use gets its duration (at most 60 s) plus one second. Screens, input or focus
loss, disabling, selection, dimension or world changes and drops invalidate
pending work, as does changing the hotbar-source setting. An exception after a
move stops Hand Restock until the next context reset.

## Verification

Pure tests cover thresholds, depletion, remainders, the shulker and bucket
switches, 1/16/64 stacks, component kinds, locks, source order, hotbar
sources, the offhand target, full inventories, revalidation, context
changes, placement dedup, held-use counting, timed completion and the quiet
period; settings tests cover defaults, round trips and translations.

In game (release build, local world then a server):
1. Place from 7 blocks with a 32 and a 64 reserve: 38, then 64 with 6 left at
   the source. Hold to place continuously and hold to throw eggs: refills while
   holding.
2. Eat food once and held; interrupt eating (no refill). Eat stew held and pour
   water: the reserve replaces the container.
3. Largest stack and lower-row ties; hotbar reserves on and off.
4. Totem in the offhand and in the hand after `/damage`.
5. Drop, rearrange, switch slots, open screens, lose focus, change dimension
   during observation: no delayed or unrelated move. Re-join: no duplication,
   loss or ghost item.

`--restock_trace=y` logs fixed stage labels with numbers ("Restock:",
"Move:"); it changes no behavior.
