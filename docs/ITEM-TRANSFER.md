# Item Transfer

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/ITEM-TRANSFER.md`.

**Inventory transfer** (Experimental) moves items between the player
inventory and the other side of an item screen with the mouse wheel and
modified clicks and drags. One saved switch `inventory.transfer`, off for
new installs since 0.3.0 (a saved on stays on), with an unbound toggle
action (`transfer`, "Toggle Inventory Transfer") usable in gameplay or an
item screen. Each gesture has its own saved child switch, all default on;
an off gesture passes through to vanilla:

| Gesture | Setting (`inventory.`) |
|---|---|
| Wheel: one item | `transferWheelOne` |
| Shift + wheel: one stack | `transferWheelStack` |
| Shift + left drag: stacks | `transferDragStack` |
| Ctrl + left drag: one per slot | `transferDragOne` |
| Ctrl + wheel: matching items | `transferWheelMatching` |
| Ctrl + Shift + wheel: everything | `transferWheelAll` |
| Alt + left click: matching items | `transferClickMatching` |
| Alt + Shift + left click: everything | `transferClickAll` |
| Shift + right drag: leave one | `transferDragKeepOne` |
| Held item, Shift + click an empty slot: matching items | `transferCursorMatching` |
| Drop key + Ctrl + left drag: drop one per slot | `transferDropOne` |
| Drop key + Shift + right drag: drop all but one | `transferDropKeepOne` |
| Drop key + Shift + left drag: drop stacks | `transferDropAll` |
| Held item, Shift + click outside: drop matching items | `transferDropMatching` |

The gestures are fixed to these modifiers; rebinding is
[L-233 Make inventory transfer gestures rebindable](backlog/L-233.md).

## Behavior

Decided with the maintainer: the original four gestures 2026-09-27 (L-41),
bulk moves, keep-one and more screens 2026-10-15 (L-169, 0.2.2), held stack
and drops 2026-10-15 (L-169, 0.2.3).

### Where

- Transfers act in every screen that holds items beside the player
  inventory. Slots with roles (furnace fuel/input, brewing fuel/ingredient/
  bottles, beacon payment) get only what the game accepts; outputs are only
  taken from. Lamium adds no fuel or recipe filter: the game's acceptance
  rules are authoritative, even when an accepted furnace input cannot smelt.
- The inventory screen moves between the main inventory and the hotbar.
  There a Shift + left press on something worn (armor, elytra, heads,
  carved pumpkins) goes to vanilla, whose quick move equips it, and the held
  drag then transfers from the next slot on. A drag begun elsewhere transfers
  worn items like any other (maintainer, 2026-10-07).
- Unmodified clicks and drags stay vanilla, and so does Ctrl + right.

### Gestures

- **Wheel**: up sends one item into storage, down into the player
  inventory, whichever side is hovered. Hovering the source side sends from
  that slot through vanilla auto-place. Hovering the destination side brings
  one matching item from the highest source slot into the hovered stack; a
  full hovered stack accepts nothing.
- **Shift + wheel**: one stack of the hovered kind in the same direction,
  highest eligible source slot first, the hovered source stack last, placed
  by vanilla auto-place (also when the hovered item is already on that side).
- **Shift + left drag** moves the whole stack from every passed slot to the
  other side; **Ctrl + left drag** one item from each. Ctrl wins when both
  modifiers are down. A slot is visited once per entry: staying on it does
  nothing more, entering it again moves what is there now.
- **Ctrl + wheel** moves every stack of the hovered kind, **Ctrl + Shift +
  wheel** everything, in the plain wheel's direction. **Alt + left click**
  and **Alt + Shift + left click** do the same from the clicked slot's side
  to the other. "Everything" from the player inventory moves one section:
  the hotbar when a hotbar slot is hovered, otherwise the main inventory
  without the hotbar (maintainer 2026-10-15). Everything can start over an
  empty real slot; matching needs an item. Matching uses the game's
  `matchesItem` (names and enchantments count).
- **Shift + right drag** moves all but one item from each passed slot. With
  its switch off, vanilla's whole-stack Shift + right drag remains.
- **Held stack**: items on the cursor + Shift + left click on an empty slot
  puts the held stack there (vanilla placement), then every matching item
  from the other side moves to that side; the hand ends empty. A slot that
  does not take the whole stack stops after vanilla's own placement.
- **Drop drags**, with the game's drop key held: Ctrl + left drag drops one
  item from each passed slot, Shift + right drag all but one, Shift + left
  drag the whole stack. The drop key press itself stays vanilla (one item,
  or the stack with Ctrl, from the slot under the pointer).
- **Drop matching**: items on the cursor + Shift + left click outside the
  window: vanilla drops the held stack, then every matching item in the
  player inventory, hotbar included, is dropped too. On the panel the press
  does nothing; a button under the pointer takes it as a plain click.
- All five held-stack/drop switches default on (maintainer's choice; the
  agent had proposed matching drops off).

### Requests and confirmation

- Requests go out together: every queued move, up to 64 per UI tick, calls
  the screen's vanilla auto-place or explicit slot-transfer path in the same
  tick, so vanilla sends them as one batch. Lamium waits for all of their
  responses, then checks every touched slot against the authoritative state
  once. Moves gathered meanwhile (drag, wheel) form the next batch. Why: a
  resource pack's bulk move sent up to 44 auto-place requests in one frame
  and the game answered them in one response; waiting for each answer made
  moves visibly one by one. Drops use vanilla's drop request and are
  confirmed with the batch like moves.
- Each move's local prediction is only a candidate: it must move a positive
  amount no greater than requested, conserve source and target counts and
  change no unrelated stack (a drop must only shrink its source and leave the
  cursor empty). A move that predicts nothing (no room) is skipped. A return
  value alone never proves completion. Partial stacks resume only after
  confirmation.
- A sweep captures owned source stacks, high slots first and the hovered
  source last; each queued source must still match its captured item and
  count when its turn comes.
- Stops, dropping what is still queued and never restarting by itself:
  close, focus loss, world or dimension change, a cursor item (except the
  held-stack gestures), text editing, the feature or gesture switched off, a
  rejected or timed-out request, an unexpected prediction or a changed
  queued source. Vanilla rolls back its rejected predictions. Releasing the
  mouse stops gathering slots; visited ones finish. A running furnace or
  brewing stand may change touched counts and conservatively stop a run.
- On an unverified game the bulk, keep-one, held-stack and drop gestures and
  the added screens stay vanilla, the original four gestures remain on the
  ordinary storage screens, and the settings and toast mark the feature
  partly paused. A failed native hook leaves the extensions unavailable too.

## Code

- `src/features/inventory/transfer/TransferGesture.h` (pure, tested in
  `tests/TransferGestureTests.cpp`): gesture selection from button and
  modifiers (`mouseGesture`, `dragGesture`, `wheelGesture`), sides and
  collections (`collectionSide`, `playerCollection`, `excludedCollection`,
  `readOnlyCollection`, `routeTarget`, `hotbarSlot`), `sweepOrder`,
  `chooseSource`/`chooseDestination`, `conservedMove`, `droppedMove`,
  `amount`, `vanillaShift`/`wornItem`, `batchPerTick` (64) and
  `dropAmountBytes`.
- `src/features/inventory/game/TransferSession.cpp`: input callbacks (owned
  values only, under `inputLock`), hover tracking (`Slot`, `Slot::cursor`),
  the queue and batches, `placeHeld` (vanilla `_handlePlaceAll`, then
  `CursorMatchingClick` moves in the same batch), drop key reading, and stop
  conditions. Moves call vanilla `_handleAutoPlace(amount, collection,
  index)`; the inventory screen uses explicit placement between hotbar and
  main inventory to avoid auto-equip.
- `src/features/inventory/game/TransferRoutes.cpp`: discovers storage
  collections from the screen's own registered routes (excluding inventory
  aliases, cursor, armor/offhand, catalogs and crafting previews). During one
  synchronous Lamium `_handleAutoPlace` call, the manager's amount/stack
  auto-place calls and request builder see only native targets on the other
  side, keeping their order and `stopWhenPlaced`; same-player-side fallbacks
  and output collections are removed. No game route map is mutated; the
  scoped native references expire within the call. Drops:
  `_handleDropItem` (one item, whole stack), `_handleTakeAmount` then
  `_handleDropSelectedItem` (all but one). `TransferDropSelected` reads the
  cursor's items before vanilla drops them, so a Shift + left press outside
  every slot within a second becomes `DropMatching` for each matching player
  slot; `pressAsPlainClick` hands that press to vanilla as a plain left press.
- `RequestTracker` (`game/RequestTracker.*`) owns request IDs, the response
  barrier and timeouts; `ScreenTracker` is shared with sorting and trading
  and unchanged by transfer. Added screens use the native hovered slot under
  the version gate rather than assuming their overrides call the base hook.
- Capability `TransferRoutes` ("Item Transfer (native slot routes)") gates
  route discovery, native hover, scoped dispatch and the drop paths
  ([GAME-UPDATES.md](GAME-UPDATES.md)).
- Budgets: at most 32 routes, targets and collections; 128 real slots per
  side; 128 queued entries and input pulses; one batch per UI tick. A layout
  that cannot fit the complete side enumeration publishes no actionable
  hover; no partial large sweep is substituted.
- `transfer_trace` (xmake option, `TransferTrace.cpp`): read-only
  diagnostics of focused screen names, `_isContainerScreen`, collection sizes
  and items, native auto-place order, Alt key notifications, OS modifier
  state and the take-all/place-all/auto-place/take-amount/drop calls. It
  invokes nothing, changes no input cancellation, keeps no game pointer and
  is game-version gated. Budgets: 32 collections, 256 slots per snapshot, 32
  targets per route; restart Minecraft when exhausted. A handler's
  `predicted-after` snapshot is local prediction.

## Known

- The drop key is `key.drop` from the game's keyboard layout, read once a
  second on the client thread (up to four keys); Q until the first read.
- The drop calls take `ItemTransferAmount` by value (12 bytes, passed by
  address); Lamium passes and hooks its traced bytes (`dropAmountBytes`).
  Re-trace after a game update ([GAME-UPDATES.md](GAME-UPDATES.md)).
- Alt (key code 18) and Alt-modified left clicks reach the loader and game.
  Vanilla take-all/place-all (`_handleTakeAll`, `_handlePlaceAll`) are
  cursor operations (take-all moved 64 oak stairs to `cursor_items`), not
  side or matching sweeps.
- Inventory, large chest, furnace, brewing stand and beacon all report
  `_isContainerScreen() == true`; the old missing-screen restriction was in
  transfer type/collection selection, not that shared predicate.
- Vanilla Shift + left click reaches `_handleAutoPlace` with the whole-stack
  sentinel. Player routes list storage before the other player collection:
  furnace fuel then ingredient; brewing fuel, ingredient, then three bottle
  slots; beacon payment. Role routes return to the combined 36-slot
  inventory.
- `tryGetAutoPlaceOrder(collection)` returns per-source target collections
  with `stopWhenPlaced`.
- Native acceptance is looser than "suitable": a furnace accepted an elytra
  as ingredient with charcoal as fuel, and its fuel slot accepted an item
  that cannot burn. Occupying a slot is not evidence of fuel. Beacon payment
  takes one emerald; a coal transfer instead followed the player-side
  fallback (hence the fallback filter).
- Furnace and brewing controllers override hover callbacks in the SDK.
- Vanilla ignores a Shift + left press outside every slot with items held.

## Tried

- One request at a time, each confirmed before the next: correct but moves
  appeared one by one; replaced by batches (above).
- Calling `_handleTakeAll`/`_handlePlaceAll` as bulk operations: they act on
  the cursor, not a side.
- An ordinary-storage type/collection whitelist: kept slot-role screens out;
  replaced by the screen's own registered routes.

## Limits

- Not chosen: Shift + double click, Ctrl + Shift + click, a wheel "all but
  one", "fill existing stacks only", W/S-directed moves, more screen
  families. All wait in
  [L-169 Item Transfer: remaining gesture and screen extensions](backlog/L-169.md).
- Held-stack and drop gestures passed only in a chest; the full gesture and
  screen matrix, servers, a rebound drop key, controlled rejection and
  timeouts, world/dimension interruption, the unverified-game fallback and
  packs are unchecked: [VALIDATION.md](VALIDATION.md), row "Inventory
  transfer gestures", and [pre-release-checks.md](backlog/pre-release-checks.md).
