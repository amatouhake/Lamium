# Villager trade helpers

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/TRADE-HELPERS.md`.

**Repeated trading** (L-166 part 1, shipped in 0.2.1): Shift + right click
on an unlocked trade row trades it as often as allowed. One saved switch
`inventory.repeatTrades`, default on, with an unbound toggle key
(`repeattrades`). Further trade helpers (favourites, trading all favourites,
batching, display) are candidates in
[L-166 Villager trade helpers: favourites and assisted trading](backlog/L-166.md);
rerolling a villager for an offer is the separate
[L-168 Trade finder](backlog/L-168.md).

## Behavior

Decided 2026-10-12, trigger 2026-10-13 (L-166); the maintainer chose this
part on 2026-10-13.

- Shift + right click on an unlocked trade row starts a run. Left click,
  plain right click and Shift alone keep vanilla behavior. Shift + right
  click on a row where no run can start does nothing (as in vanilla); other
  controls, such as inventory slots, keep vanilla Shift + right click.
- Use the game's own trade requests, one round at a time. Start another
  round only after the request succeeds and authoritative inventory and
  offer state confirm it.
- Stop on: sold-out offer, missing inputs, output that cannot fit, a
  rejected, untracked or timed-out request, screen close, focus loss, world,
  dimension, entity or recipe change, an offer refresh, any physical key,
  click or wheel input, the setting switched off, or an unverified game. Key
  auto-repeat of the held Shift is not new input and does not stop a run.
  A stopped run never restarts by itself.
- Never drop output to continue. Before selection/refill, both existing
  inputs must fit back into the inventory without dropping (partial matching
  stacks and shared empty slots counted, matching inputs sharing one
  budget); otherwise the run stops before vanilla refill. Output capacity is
  checked after refill: one refill may free a consumed ingredient slot, but
  no trade is sent if output still cannot fit.
- Disabled or unsupported paths leave ordinary trading intact.
- Favourites and remaining-use presentation are undecided (L-166).

## Code

- `src/features/inventory/RepeatTradeState.h` (pure,
  `tests/RepeatTradeTests.cpp`): request phases and exact cost, output and
  use checks.
- `src/features/inventory/RepeatTrades.cpp`, hooks:
  - `RepeatTradeRow` (`InputComponent::handleButtonEvent`): only on a
    component that maps plain right click (710676172) to the trade-row event,
    a Shift + right press (1485818800, Shift alone) is passed on as 710676172.
    Rows queue their screen event, so a handled remapped press marks only the
    next row press (within 250 ms); an unmodified row press clears the mark.
  - `RepeatTradeInput` (`ScreenController::$handleEvent`): on that marked
    trade-row press (button 4096534505 from source 710676172) with
    `#collections` (`trade_tiers`, `trades`) and `#enabled` true in the row's
    property bag, on the focused active trade screen, it creates the job and
    swallows the press. Any other button-down clears the job.
  - `RepeatTradeTick` (`Trade2ScreenController::$tick`): advances the job.
    Each UI tick reacquires the active controller, the trader's offers, the 36
    inventory slots and both inputs, then re-invokes vanilla's own row
    handlers from `mButtonInteractedEventCallbackMap`: 2196830738
    (select/refill) and 4096534505 (one automatic trade). Lamium never builds
    trade transactions. The next request group starts on a later tick.
  - `RepeatTradeLeave` (`Trade2ScreenController::$onLeave`) and
    `RepeatTradeRefresh` (`UpdateTradePacket`) clear the job.
- The job holds only entity, dimension, recipe, tier, row and generation IDs
  plus owned counts and layout values; no game pointer crosses frames or is
  used from window input events. Physical input (`input/CustomInput.cpp`),
  focus loss and settings changes (`Runtime.cpp`) bump the generation; world
  exit cancels.
- Confirmation: preparation waits for all owned placement responses (or a
  genuinely unchanged no-op); each trade waits for its own request IDs and
  exact resulting counts. Client offer uses are predicted (+1) before
  submission; a successful native trade response confirms the round, and a
  uses mismatch stops. Never advance on counts or predicted uses while a
  response is pending. `RequestTracker` supplies the five-second timeout and
  never creates packets or retries.
- Capability `RepeatedTrades` ("Repeated trades (trade row handlers)") is
  asked at every invocation; the handler IDs and model layout are behind it,
  and missing or ambiguous handlers keep vanilla input
  ([GAME-UPDATES.md](GAME-UPDATES.md)).
- `repeat_trade_trace` (xmake option, `src/features/research/RepeatTradeTrace.cpp`):
  read-only `L-166` log lines with a sequence number and steady-clock
  milliseconds: mouse/modifier input, button and toggle event IDs and
  property bags, slot presses, visual offer and small container contents on
  change, vanilla request batches (IDs, action types, craft counts), applied
  ItemStackResponse slots, inventory packets and received offer uses. Fixed
  line budget per stage, owned strings and scalars only; network
  observation continues ten seconds after closing the screen. Restart for a
  fresh budget. It sends nothing and changes no button. The older
  `trade_trace` is separate; leave it off.

## Known

Seen on 1.26.51.01 in a local world:

- Row right click trades once through `CraftRecipeAuto` (type 13,
  crafts=1), acknowledged; its up event has no request. Row left click only
  selects and refills the inputs. The Trade button sends `CraftRecipe` (type
  12, one craft) and puts the output in the inventory.
- `notifyTrade` fires client-side before the request (prediction) and
  server-side after (authoritative), with matching counts. No
  `UpdateTradePacket` follows ordinary trades. `mVisualOffer` stays unset
  (show=false, recipe 0) and is no evidence; the trader's client offer list
  is read instead.
- Shift + right click is not consumed by the game: it arrives as its own
  global button (1485818800) and reaches each row's `InputComponent`, whose
  mappings do not list it. That is why modified row clicks never reached
  `ScreenController::$handleEvent` before remapping.
- Result-slot Shift + click calls `_handleAutoPlace(2147483647,
  trade2_result_item, 0)` once and sends one batch, `CraftRecipe` crafts=N,
  limited to what is already in the input slots (64 coal at price 15 gave 4
  emeralds, 4 coal left); the inventory is not restocked. Not adopted;
  bigger per-round steps are L-166 candidate 4. Its full-inventory refusal is
  unchecked.
- SDK: `Trade2ScreenController::_onContainerSlotPressed`, `tick`, `onLeave`,
  `TradeItemCollections` (tier and row from a property bag), `mVisualOffer`
  (`MerchantRecipe`); `Trade2ContainerManagerModel::getTradeIndex(tier,
  index)` and `mSelectedTrade`. `Trade2ContainerManagerController` is empty in
  the SDK and exposes no trade method.
- The button IDs above are observations checked by the version gate, not a
  portable API.

## Tried

- Building `ItemStackRequest` trade payloads: never; vanilla's own handlers
  only (L-166).
- Treating `mVisualOffer` changes or predicted uses as completion: they are
  prediction.
- Result-slot Shift + click as the repeat path: slot-limited, cannot trade
  up to inventory holdings.

## Limits

- Real latency and loss, other servers, price changes and a forced exit
  with a response pending are unchecked: [VALIDATION.md](VALIDATION.md),
  row "Repeated trading", and
  [pre-release-checks.md](backlog/pre-release-checks.md).
