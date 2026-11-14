# Item Transfer

## Current status

L-169's 0.2.2 part is complete. The held-stack gesture and the drop
gestures (next section) passed the maintainer's chest checklist on `9da5d9d`. The maintainer confirmed the
normal-build smoke checklist on c8c1f1b on 2026-10-15: all behaved as expected.
Detailed coverage beyond that report remains below. Drops, cursor-item
operations and W/S moves remain out of scope.

## Held stack and drops (2026-10-15, `9da5d9d`; chest checklist passed)

- Hovering keeps tracking slots while items are on the cursor (`Slot::cursor`);
  every other gesture still waits for an empty cursor. Shift + left on an
  empty slot then runs `placeHeld`: vanilla `_handlePlaceAll` puts the held
  stack there and the matching items of the other side follow as
  `CursorMatchingClick` moves in the same tracked batch; the placed slot is
  confirmed with them.
- Drop drags are chosen with the drop key held (`key.drop` from the game's
  keyboard layout, read once a second on the client thread; Q before the
  first read). One item and whole stacks use `_handleDropItem`; all but one
  takes the amount onto the empty cursor (`_handleTakeAmount`) and drops it
  with `_handleDropSelectedItem`. The prediction must only shrink the source
  (`droppedMove`) and leave the cursor empty, or the operation stops.
- `TransferDropSelected` (TransferRoutes.cpp) reads the cursor's items before
  vanilla drops them; a Shift + left press outside every slot with items
  held within the last second turns that into `DropMatching` for each
  matching player slot (both sides in the inventory screen). Vanilla ignores
  that press, so `pressAsPlainClick` hands it over as a plain left press: outside
  the window vanilla drops, on the panel it does nothing, and a button under the
  pointer takes it as a plain click.
- ItemTransferAmount is passed as its traced bytes (`dropAmountBytes`); see
  GAME-UPDATES.md for the re-trace after a game update. All of this is
  behind the `TransferRoutes` capability.

## Current implementation (2026-10-15)

- Ctrl + wheel transfers all matching stacks; Ctrl + Shift + wheel transfers
  everything in the wheel's direction. Alt + left click and Alt + Shift +
  left click do the corresponding operation from the clicked side.
  Everything-from-player moves the hovered section: the hotbar alone when a
  hotbar slot is hovered, otherwise the main inventory without the hotbar
  (changed 2026-10-15 after the batched build; in the inventory screen the
  hotbar is a whole side).
  Everything can start over an empty real slot; matching needs an item.
- Shift + right drag leaves one in every passed slot. Each new gesture has
  its own saved child switch, default on; an off gesture passes through to
  vanilla. The four old switches and saved action IDs are unchanged.
- The pure helpers in `TransferGesture.h` select gestures, sides, sweep
  order, hotbar exclusion and candidate count conservation. Matching uses
  the same native `matchesItem` predicate as the old wheel gestures.
- A sweep captures owned source stacks, high slots first and the hovered
  source last. Each queued source must still match its captured item/count.
  Slots are visited once per entry during a drag, preserving existing
  re-entry behavior. Releasing the button stops gathering; visited slots
  may finish.
- `TransferRoutes` discovers storage collections from the screen's own
  registered routes, excluding inventory aliases, cursor, armor/offhand,
  catalogs and crafting previews. This removes the ordinary-storage-only
  restriction without changing sorting's scope or the shared screen tracker.
- Vanilla `_handleAutoPlace(amount, collection, index)` owns request creation,
  prediction and feedback. During that one synchronous Lamium invocation,
  the manager's amount/stack auto-place calls and request builder receive only native targets
  on the other side, retaining order and `stopWhenPlaced`. Output collections
  are not destinations. The game's acceptance/capacity/disabled-slot rules
  remain authoritative; Lamium does not add a fuel or recipe filter. Native
  same-player-side fallbacks are removed. No game route map is mutated.
- The inventory screen still uses explicit placement between the hotbar and
  main inventory, avoiding auto-equip. Ordinary storage/inventory hover
  tracking stays as before; added screens use the native hovered slot under
  the version gate rather than assuming their overrides call the base hook.
  The requested chest and furnace/brewing/beacon smoke checks passed on the
  normal implementation build; the full per-screen matrix remains coverage.
- A prediction must move a positive amount no greater than requested,
  conserve source/target counts and change no unrelated stacks. It is only a
  candidate: all owned request IDs must succeed, then the changed slots must
  match the reacquired authoritative state before another request runs.
  Partial stacks resume only after that confirmation, leaving one where
  requested. A source with no requests and no predicted changes is skipped
  as a no-op; a return value alone never proves completion.
- Close, focus loss, world/dimension change, cursor item, text editing,
  disabled parent/gesture, rejection, timeout, unexpected prediction or
  changed queued source stops the run without automatically restarting.
  Input callbacks exchange owned values only. The scoped native references
  expire within the synchronous call; confirmations keep owned stacks.
- Budgets: at most 32 routes/targets/collections, 128 real slots per side,
  128 queued entries/input pulses and one request group per UI tick. A layout
  that cannot fit the complete side enumeration does not publish an actionable
  hover. No partial large sweep is silently substituted.
- `Item Transfer (native slot routes)` gates discovery, native hover and
  scoped dispatch. On an unverified game the new gestures/additional screens
  stay vanilla, the original four gestures remain available on old screens,
  and the settings/toast mark the feature partly paused. Failed native-hook
  installation also leaves extensions unavailable.

## Completed native checkpoint (2026-10-15)

Build e3f9276, `transfer_trace=y`, DLL SHA-256
`850CC7B7A8A053F24BE8D6C04A8B0BAB3497623765727A14C322EF54FEF3F103`.
The maintainer reports unchanged behavior; the log shows a local world and
LeviLamina 26.51.7. Exact game mode and resource-pack coverage were not stated.

- Alt key code 18 and Alt-modified left clicks reach the loader/game.
  Take-all moves 64 oak stairs to `cursor_items`; place-all returns them.
  They are cursor operations, not side/matching sweeps.
- Inventory, large chest, furnace, brewing stand and beacon all report
  `_isContainerScreen() == true`. The missing-screen restriction was in
  transfer type/collection selection, not that shared tracking predicate.
- Shift transfer reaches `_handleAutoPlace` with the whole-stack sentinel.
  Player routes list storage before the other player collection: furnace
  fuel, then ingredient; brewing fuel, ingredient, then three bottle slots;
  beacon payment. Role routes return to the combined 36-slot inventory.
- A furnace accepted an elytra into the ingredient slot while charcoal
  occupied fuel. This agrees with preserving native acceptance rather than
  assuming occupied inputs are smeltable. The earlier non-burning-fuel-slot
  observation remains a maintainer report, not this exact trace observation.
- Beacon payment takes one emerald; a coal transfer instead follows the
  player-side fallback. These observations require filtering same-side
  fallbacks for Lamium's other-side-only contract.

Handler snapshots are prediction. The maintainer's report confirms the
visible vanilla behavior, not exact authoritative counts for each packet.
This diagnostic checkpoint did not test other families, servers or the new
Lamium gestures; the normal implementation checkpoint follows.

## Completed normal-build checkpoint (2026-10-15)

Build c8c1f1bc0ccfcc4a57340c16d0c3e8dbb797269c, Windows x64 release client,
all traces/probes off. Built and installed DLL SHA-256 both:
`2A6B0A52B397511108815C2BE4315EFBA84403343AAA242B632D2B9FEF6BE515`.

The maintainer confirmed that the requested checks all behaved as expected:
the five new gestures and their switches, with chest and furnace/brewing/
beacon checks. The log shows a local world and LeviLamina 26.51.7 against
the verified Minecraft 1.26.51.01 target. Individual gesture/screen/edge-case
combinations, game mode, packs and server coverage were not itemized.
Source-count changes stopped some runs in storage and furnace/brewing
collections; their triggers were not specified. This is not a controlled
rejection/timeout test. No route-hook failure, request rejection or slot
confirmation mismatch appears in the captured log. Build and LamiumTests
passed; the game dependency inventory passed (60 files / 286 hooks).

## Detailed follow-up coverage

The smoke checkpoint above completes the development part; the following
plan retains detailed edge cases and broader coverage without treating
each as individually verified. Use a normal build with traces off. Enable Item Transfer
(it was off for the native checkpoint); check all nine child switches are
present and the five new ones default on. Record commit and DLL hash.

1. Chest with matching/different stacks on both sides: Ctrl + wheel moves
   all matching items up/down, from either hovered side; Ctrl + Shift +
   wheel moves everything while leaving the player's hotbar. Alt + left
   click and Alt + Shift + left click use the clicked side. Try an empty
   slot for everything; different names/enchantments and full destinations.
2. Shift + right drag with source counts 1, 2 and a full stack: one remains
   in each passed slot. Staying still does nothing more; leaving/re-entering
   the slot follows the established drag behavior. Turn each new switch off
   individually: its input stays vanilla, including whole-stack Shift +
   right drag. Restart to check saved choices. Recheck the original four
   gestures and inventory-screen auto-equip/drag and split-stack behavior.
3. Repeat every gesture in chest, barrel, shulker box, ender chest, dispenser,
   dropper, hopper, furnace, blast furnace, smoker, brewing stand, crafter
   and beacon. Check furnace fuel/input/output, brewing fuel/ingredient/
   bottles and beacon payment, including full, disabled and unsuitable slots.
   No transfer may fall back between hotbar and main inventory, put into an
   output or drop an item. Preserve each screen's native acceptance, even if
   an accepted furnace input cannot smelt/burn.
4. During a sweep: close, lose focus, hold a cursor item, edit text, disable
   the parent/gesture or change a queued source. It stops and does not restart.
   Check rejection/delay/timeouts and authoritative totals on a server, with
   a dimension/world change as well. A running furnace/brewing operation may
   change touched slot counts and conservatively stop the run.
5. Check an unverified-game build: advanced gestures/screens stay vanilla,
   Item Transfer is partly paused, the four original gestures still work in
   old screens, and unrelated vanilla input is unchanged.

Record checked screens in DESIGN and evidence in VALIDATION-LOG/VALIDATION.
The 0.2.2 part is in BACKLOG-DONE; release status and remaining coverage
stay in BACKLOG's Pre-release checks and VALIDATION.

## Earlier research checkpoint (2026-10-15, before the diagnostic run)

Maintainer observation, vanilla storage screen:

- Alt + left click selects an item like an ordinary click. The modified
  mouse operation reaches the game; the loader's Alt key notification is
  not yet observed.
- Shift + right drag transfers the whole hovered stack. Its L-169
  replacement deliberately leaves one item; switching that gesture off
  must preserve vanilla's whole-stack transfer.
- Follow-up: the chest's Alt + left click, Shift + right drag and ordinary
  take/place operations behave as before. Vanilla Shift + left click routes
  items to the expected roles in a furnace, brewing stand and beacon.
  The maintainer reports that a furnace's fuel slot also accepts an item
  that cannot burn; exact items and counts were not supplied. Brewing and
  beacon acceptance appeared restricted to suitable items, not exhaustively
  checked. Follow native routing and acceptance; occupying a fuel slot is
  not evidence that the item is fuel.
- The installed DLL found at follow-up differs from the prepared diagnostic
  DLL, and the current log has no `L-169` lines. These observations do not
  establish native handler names, collection names or auto-place orders.

Source evidence:

- `TransferSession.cpp` already invokes `_handleAutoPlace(amount,
  collection, index)` on the client/UI thread, one request at a time,
  through `RequestTracker`. Existing runtime coverage is in VALIDATION.md.
- The 26.51.5 SDK exposes `_handleTakeAll(collection, index)` and
  `_handlePlaceAll(collection, index)`, and manager `handlePlaceAll` takes
  `SelectedSlotInfo` plus a destination. Their names do not establish
  that they transfer a side or matching stacks. Do not call them as bulk
  operations without observing their cursor and slot effects.
- Native `tryGetAutoPlaceOrder(collection)` returns per-source target
  collections with `stopWhenPlaced`. This is the candidate route for
  role-specific destinations. The reported vanilla routing above does not
  yet identify the registered furnace/brewing/beacon collections and orders.
- Today's transfer is restricted by both `ScreenTracker`'s
  `_isContainerScreen()` check and `TransferSession`'s ordinary-storage
  type/collection whitelist. The claim that the first check excludes the
  missing screens has not been established; observe both before changing
  tracking shared with sorting and trading.
- Furnace and brewing controllers override hover callbacks in the SDK.
  The existing hover tracker observes only the base and crafting callbacks;
  check whether the overrides delegate before adding coverage.

`--transfer_trace=y` now adds bounded, read-only diagnostics. It observes
focused screen names and `_isContainerScreen`, collection sizes and items,
native auto-place order, Alt key notifications and OS modifier state,
and ordinary take-all/place-all/auto-place calls. No diagnostic invokes
these operations, changes input cancellation, or retains a game pointer.
The native observations are game-version gated. A handler's
`predicted-after` snapshot is local prediction, not server confirmation.
The budgets cover at most 32 collections, 256 slots per snapshot and
32 targets per route; restart Minecraft if the sample budget is exhausted.

## Initial native-handler checklist (new-gesture checks are above)

Use a diagnostic build without a release tag. Disable Item Transfer in
Lamium before observing vanilla; otherwise its existing gestures can
consume the input. Record build commit, DLL SHA-256, game/loader versions,
local or server, resource packs, and the actual final slot/cursor counts.
The log alone does not confirm authoritative completion.

1. Open a chest with two matching stacks and one different stack on each
   side. Try Alt + left click, then cancel/return the cursor item. Try
   Shift + right drag, ordinary take/place clicks, Shift + left click and
   the controller's take-all/place-all controls if available. Record which
   slots change, whether the cursor gets an item, and which native handler
   runs. An unavailable controller control is an unchecked case.
2. Open chest, barrel, shulker box, ender chest, dispenser, dropper, hopper,
   furnace, blast furnace, smoker, brewing stand, crafter and beacon once.
   Check the `L-169 focused view`, `layout` and `route` lines. Record missing
   layouts, collections and callbacks rather than guessing their names.
3. With vanilla Shift + left click, send fuel, a smeltable item and an
   invalid item to each furnace; bottles, an ingredient and blaze powder
   to the brewing stand; payment and an invalid item to the beacon. Check
   exact final counts and destinations, including full targets. Observe
   taking outputs and moving ordinary storage stacks back to inventory.
4. Repeat on a server. Keep the local and server evidence separate.

After this checkpoint, report the handler effects and selected routing
path before implementing L-169's new operations. Then test every new
gesture and child switch, matching versus differently named/enchanted
stacks, hotbar preservation for everything-from-player, all-but-one with
counts 1/2/full, full or invalid destinations, and interruption by closing,
focus loss, cursor item, text editing, rejection, timeout and changed source
counts. Record checked screens in DESIGN and runtime evidence in
VALIDATION-LOG/VALIDATION; build/tests alone do not close L-169.

## Inventory transfer (Decided 2026-09-27, BACKLOG L-41)

Moved from DESIGN.md on 2026-11-14 without changes; still the decided behavior.
- The feature has one saved switch, off for new installs since 0.3.0 (a saved
  on stays on), and an unbound toggle action usable in gameplay or an item
  screen. Transfers act in screens
  with a player inventory and another side; the inventory screen moves between
  main inventory and hotbar. Four original child
  switches enable ordinary wheel, Shift+wheel, Shift+left drag and Ctrl+left
  drag independently; all default on. An off gesture passes through to vanilla.
- Wheel up sends one item into storage; wheel down sends one item into the
  player inventory, regardless of which side is hovered. When hovering the
  source side, send from that slot through vanilla auto-place. When hovering
  the destination side, bring one matching item from the highest source slot
  into the hovered stack. A full hovered destination stack accepts nothing.
- Shift+wheel sends one stack of the hovered item kind in the same direction.
  Pick the highest eligible source slot first and leave the hovered source
  stack for last. Use vanilla auto-place for the destination, including when
  the hovered item is already on that side.
- Shift+left drag transfers the entire stack from every passed slot to the
  other side. Ctrl+left drag transfers one from each passed slot. Shift+right
  drag leaves one when its new child switch is on; Ctrl+right stays vanilla.
  A slot is visited once per entry during a drag. Ctrl takes precedence when both modifiers
  are down. Unmodified clicks and drags stay vanilla.
- Requests go out together (revised 2026-10-15, L-169; before: one at a
  time, each confirmed before the next): every queued move, up to 64 per UI
  tick, calls the screen's vanilla auto-place or explicit slot-transfer path
  in the same tick, so vanilla sends them as one batch. Lamium then waits for
  all of their responses and checks every touched slot against the
  authoritative state once. Moves gathered meanwhile (drag, wheel) form the
  next batch. Why: a resource pack's bulk move, traced on 2026-10-15, sent up
  to 44 auto-place requests in one frame and the game answered them in one
  response; waiting for each answer made Lamium's moves visibly one by one.
- Each move's local prediction must still be a conserved move of its own
  source; a move that predicts nothing (no room) is skipped. Close/focus
  changes, a cursor item, text editing, feature disable, a rejected or
  timed-out request, an unexpected prediction or a changed source stop the
  operation and drop what is still queued; vanilla rolls back its rejected
  predictions. Mouse release stops gathering slots but lets visited ones
  finish.

### Bulk moves and more screens (Decided 2026-10-15 for 0.2.2, L-169; smoke-tested, unreleased)

- Ctrl + wheel moves every stack of the hovered kind, Ctrl + Shift + wheel
  everything, in the plain wheel's direction. Alt + left click and Alt +
  Shift + left click do the same from the clicked slot's side to the other.
  "Everything" from the player inventory moves one section: the hotbar
  when a hotbar slot is hovered, otherwise the main inventory without the
  hotbar (maintainer 2026-10-15, after the batched build's check).
- Shift + right drag moves all but one item from each passed slot.
- Transfers work in every screen that holds items beside the player
  inventory. Slots with roles get only what they accept; outputs are only
  taken from.
- Each new gesture has its own switch, on by default. Not chosen: Shift +
  click on an empty slot with a cursor item, a wheel "all but one", drop
  gestures, W/S moves.

### Held stack and drops (Decided 2026-10-15, L-169; passed in a chest on `9da5d9d`)

- Items on the cursor + Shift + left click on an empty slot: the held stack
  goes into that slot, then every matching item from the other side moves
  to that side; the hand ends empty. A slot that does not take the whole
  stack stops after vanilla's own placement.
- With the game's drop key held: Ctrl + left drag drops one item from each
  passed slot, Shift + right drag all but one, Shift + left drag the whole
  stack. The press of the drop key itself stays vanilla (one item, or the
  stack with Ctrl, from the slot under the pointer).
- Items on the cursor + Shift + left click outside the window: vanilla
  drops the held stack, then every matching item in the player inventory,
  hotbar included, is dropped too.
- Each has its own switch, all on by default (maintainer's choice; the
  agent had proposed matching drops off). Drops use vanilla's drop request
  and are confirmed with the batch like moves. Not chosen for now: Shift +
  double click, Ctrl + Shift + click, "fill existing stacks only".

Vanilla baseline reported 2026-10-15: chest inputs behave as before;
Shift + left click routes items by role in furnace, brewing stand and
beacon screens. A furnace fuel slot also accepted an item that cannot
burn. Native slot acceptance and routing, rather than a new Lamium fuel
filter, remain the baseline. Brewing/beacon restrictions were not checked
exhaustively. The e3f9276 diagnostic checkpoint
confirmed unchanged vanilla behavior in a large chest, furnace, brewing
stand and beacon and captured Alt and native handlers/orders; it is not
validation of the new Lamium gestures. Native discovery follows those orders
with same-player-side fallbacks removed. On c8c1f1b the maintainer confirmed
the normal implementation checklist, including the five new gestures and
switches and chest/furnace/brewing/beacon checks, all behaved as expected.
Checked screens in that smoke checklist: chest, furnace, brewing stand and
beacon. Individual combinations and the remaining screen families, servers
and controlled failure cases were not separately reported. Details:
[ITEM-TRANSFER.md](ITEM-TRANSFER.md).
