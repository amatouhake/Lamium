# Villager trade helpers

Repeated trading (L-166 part 1) is complete, unreleased; its record is in
[BACKLOG-DONE](BACKLOG-DONE.md#l-166-villager-trade-helpers-part-1).
Runtime coverage stays in VALIDATION.md. The normal build passes local/BDS
trading, sold-out and material-limit stops, safe full-inventory refusal,
settings-off and two-input enchanted books. Controlled BDS response delays,
focus loss, closing and re-entry preserve the observed totals without an
automatic restart. The delayed partial run also stays stopped through a
Nether round trip. Coverage limits belong to BACKLOG's pre-release checks.

## Contract

The maintainer chose the repeated-trade part on 2026-10-13, then authorized
Computer Use for runtime checks and completing it. Shift + right-clicking an
unlocked trade row should trade it repeatedly, default on. Left click, plain
right click and Shift retain vanilla behavior. Favourites and remaining-use
presentation are still undecided.

Use the game's own trade requests. Start another round only after the
request succeeds and authoritative inventory and offer state confirm it.
Stop on sold-out offers, missing inputs, insufficient output space, a
rejected/untracked/timed-out request, screen close, focus loss, world or
dimension change, or disabling the feature. Never drop output to continue.
No game pointers are retained across frames or used from window input events.

## Implementation

`RepeatTradeState.h` owns the pure request phases and exact cost/output/use
checks. The native job holds entity, dimension, recipe, row and generation
IDs plus owned counts/layout values. Each UI tick reacquires the active
controller, actor offers and the 36 inventory slots plus both inputs.

The verified vanilla row operations are single entries in
`mButtonInteractedEventCallbackMap`: selection/refill and one automatic
trade. Their IDs and model layout are protected by the game-version gate;
missing or ambiguous handlers preserve vanilla input. Preparation waits for
all owned placement responses (or a genuinely unchanged no-op), then each
trade waits for its own request IDs and exact resulting counts. Client offer
uses are predicted before submission; a successful native trade response
confirms that round, and a uses mismatch stops. Never advance on counts or
predicted uses while a response is pending. The existing tracker supplies a
five-second timeout and never owns packet creation or retries.

The next request group starts on a later tick. Physical key/mouse presses,
wheel input, focus loss and settings changes invalidate the generation;
screen leave, world exit and offer refresh release observation ownership.
Dimension/entity/recipe changes stop when reacquired. Output capacity is
checked after native refill; one refill may free a consumed ingredient slot,
but no trade is sent if output still cannot fit. Before selection/refill,
both existing inputs must fit back into inventory without dropping: partial
matching stacks and shared empty slots are counted, with matching inputs
sharing one budget. Otherwise the run stops before calling vanilla refill.
Disabled or unsupported
paths leave ordinary trading intact. Settings include an unbound toggle key
and English, Japanese and Simplified Chinese labels.

## Current research (2026-10-13)

The 26.51.5 client SDK exposes:

- `Trade2ScreenController::_onContainerSlotPressed`, `tick`, `onLeave`,
  `TradeItemCollections` (tier and row indexes from a UI property bag),
  and `mVisualOffer` (`MerchantRecipe`, including uses, limits and prices).
- `Trade2ContainerManagerModel::getTradeIndex(tier, index)` and
  `mSelectedTrade`. `Trade2ContainerManagerController` is an empty class
  in the SDK; it exposes no callable trade method.
- `ItemStackRequestPacketPayload` construction from vanilla's request
  batch, craft actions' `mNumCrafts`, and the existing RequestTracker's
  ownership and ItemStackResponse barrier.
- The earlier L-129 packet captures contain `Recipes`, with `netId`,
  `tier`, `uses`, `maxUses`, `buyCountA` and `buyCountB`. Those captures
  establish opening data only, not how uses update after a trade.

Before implementing automatic requests, determine in game:

1. Which button/toggle event and property bag identify a trade row, how
   right click differs from selection, and which vanilla action executes it.
2. Whether a manual trade becomes a tracked item-stack request and whether
   Shift produces one trade or a batch (`crafts` count).
3. Whether the server sends updated `uses` after each trade, or some other
   authoritative offer update is needed. The visual offer may be predicted;
   changes to it alone are not completion evidence.
4. Output routing (inventory or cursor), full-inventory refusal and two-input
   trades, including an enchanted book. These decide the safe request path.

### First observed path (2026-10-13, local, Computer Use)

On `a1773c6` with `repeat_trade_trace`, the weapon smith's coal offer starts
at uses 3/16 and coal price 15. The ordinary Trade button sends CraftRecipe
(type 12, one craft) and puts its emerald in the inventory, acknowledged by
ItemStackResponse. Right click on its row already trades once through
CraftRecipeAuto (type 13, one craft); left click refills the ingredients
without another emerald. The row bag contains `#collections` with
`trade_tiers` and `trades`, and `#enabled`. The right-click down event is
ID 4096534505, source 710676172; its up event has no request. These IDs are
an observation to resolve against vanilla's names, not a portable API.

Three manual trades produced emerald counts 3 -> 4 -> 5 -> 6. Their request
IDs -7, -9 and -17 received success responses. No updated UpdateTradePacket
was received for those trades. `mVisualOffer` remains unset (show=false,
recipe=0), so it cannot supply the real current offer here. A follow-up
trace reads the trader's client offer list and observes `notifyTrade` and
ActorEventPacket, to distinguish prediction from a server notification.
No automatic-trade behavior, full-inventory test or server result yet.

### Auto-place spike result (2026-10-13, local, maintainer)

Resolved by runtime observation; the hypothesis is confirmed but limited.
Shift-clicking a trade row sends nothing. Shift-clicking the result slot
calls `_handleAutoPlace(2147483647, trade2_result_item, 0)` once, and one
request batch with CraftRecipe crafts=4 succeeds with matching client and
server `notifyTrade` (uses 5 -> 9). The 4 crafts equal the 64 coal in the
ingredient slot at price 15; inventory holdings are not restocked in this
path. So single-request multi-craft is real but slot-limited: it cannot
replace the refill loop for trading up to holdings. Possible future use is
bigger per-round steps (refill, then one multi-craft) instead of one craft
per round; unverified, not adopted. Full-inventory refusal on this path is
still unchecked.

## Trigger (resolved 2026-10-13)

Shift + right click is not consumed by the game: it arrives as its own
global button (1485818800, plain right click is 710676172) and reaches each
trade row's `InputComponent`, whose mappings simply do not list it.
`RepeatTradeRow` hooks `InputComponent::handleButtonEvent` and, only on a
component that maps 710676172 to the trade-row event, passes the press on
as 710676172. Rows queue their screen event, so a handled remapped press
marks only the next row press (250 ms); an unmodified row press clears it.
That press starts a run, or is swallowed when no run can start (vanilla
does nothing there). Other controls, such as inventory slots, keep vanilla
Shift + right click. Key auto-repeat of the held Shift is not new input and
does not cancel a run. Verified locally (VALIDATION-LOG).

The notes below are the handoff that led here.

### Implementation handoff before the normal BDS checks

- `src/features/inventory/RepeatTrades.cpp` (+ pure `RepeatTradeState.h`,
  `tests/RepeatTradeTests.cpp`): on its trigger it swallows the row
  right-click down event and returns Refresh, then each UI tick re-invokes
  vanilla's own row handlers (`2196830738` select/refill, `4096534505`
  single auto-trade) from `mButtonInteractedEventCallbackMap`. Lamium never
  builds trade transactions. Every round waits for owned `ItemStackResponse`
  success plus exact inventory delta plus predicted uses+1; any mismatch,
  timeout (5 s), sold-out, missing inputs, no output room (one refill
  allowed), screen leave, focus loss, world/dimension/entity/recipe change,
  `UpdateTradePacket`, physical input, settings-off, or unverified game
  stops the run. Trace build `606FA98F` deployed to the `1.26.51.01 LIP`
  instance.
- Verified locally: one trigger runs coal 9 -> 16/16 and axes 16 -> 1
  emerald with 5 outputs; locked rows and input-less rows send nothing;
  cursor stays empty. (The Shift trigger did not work then; see above.)

### Verified vanilla behavior (1.26.51.01, local)

- Row right-click: one `CraftRecipeAuto` (type 13), crafts=1, acknowledged.
  Row left-click: refill only. Trade button: one `CraftRecipe` (type 12).
- `notifyTrade` fires client-side before the request (prediction) and
  server-side after (authoritative); both observed with matching counts.
  No `UpdateTradePacket` follows ordinary trades. `mVisualOffer` stays
  unset (show=false), unusable as evidence.
- Result-slot Shift+click: `_handleAutoPlace(2147483647,
  trade2_result_item, 0)` then one batch, `CraftRecipe` crafts=N (N=4 seen:
  64 coal at price 15 -> 4 emeralds, 4 coal left). Slot-limited; inventory
  is NOT restocked (maintainer-held bulk coal untouched).
- Every modified click on a trade ROW never reaches
  `ScreenController::$handleEvent` (later explained: no row mapping, above): 5 right-click samples with Shift held
  (mouse downs logged, zero button events), Shift+left, and Ctrl+right
  (mouse logged, zero button events). Unmodified row clicks and result-slot
  Shift+click DO arrive (buttons `4096534505`, `2033374506`). Shift+Trade
  button arrival is untested; redo needs unconditional mouse-click logging
  (current trace gates mouse logs on a 10 s activity window).
  Log: `<instance>/mods/Lamium/logs/lamium.log` on `1.26.51.01 LIP`.

### Constraints for any proposal

- One round at a time through vanilla's own handlers; no self-built
  `ItemStackRequest` payloads (BACKLOG L-166). Post-hoc verification from
  authoritative state (response + inventory + uses); fail open to vanilla.
- `versionSensitiveAllowed("Repeated trades (trade row handlers)")` at every
  invocation; a new hook/API gets a `GAME-UPDATES.md` entry.
- No game pointers across frames or from input events; cancel on physical
  input; default-on setting with EN/JA/ZH help text (`Translations*.h`,
  order-sensitive); `SettingsStoreTests`/`TranslationsTest` must pass.

## Historical first runtime checkpoint
Build with `xmake f --repeat_trade_trace=y -y`, then `xmake build Lamium`.
This is an independent, read-only trace; leave the old `trade_trace` off.
It never sends requests, modifies button behavior or automates trades.
The normal build compiles this diagnostic out; repeated trading remains enabled.

`L-166` log lines carry a sequence and steady-clock milliseconds:
mouse/modifier input; button and toggle event IDs and property bags;
slot presses; visual offer and small container contents when state changes;
vanilla request batches with IDs/action types/craft counts; applied
ItemStackResponse slots; inventory packets; and received offer uses.
Each stage has its own fixed line budget. Enumerations are bounded and only
owned strings/scalars are remembered. Network observations extend for ten
seconds after closing the trade screen, so a late response remains visible.
Restart the client for a fresh budget when moving from local to BDS checks.
No SNBT files are written and earlier L-129 captures remain intact.

Check locally first, then on BDS 1.26.51.1:

1. Open an unlocked villager offer with enough inputs. Select it with left
   click, trade once through vanilla, wait two seconds, and trade again.
2. Right-click its row once, then try vanilla Shift trading separately.
   Record what each does (selection, one trade, many trades or nothing).
3. Try a locked row, a sold-out offer and a full inventory. Record whether
   output appears on the cursor or in inventory; keep the ordinary flow.
4. Try a two-input enchanted-book offer. Close the screen just after one
   manual trade, wait two seconds, reopen, and record the visible uses.

Expected: vanilla behavior is preserved and the log distinguishes requests
and applied responses from visual updates. No repeated trading is expected
in this checkpoint. Save the log with the build/hash and observed outcomes.
If diagnostics cannot install, preserve the error before proceeding.

Keep the tested DLL/PDB/manifest as a separate local artifact, then reset
`xmake f --repeat_trade_trace=n -y` and rebuild the normal DLL before commit
or release. Deploy a trace artifact only for an explicitly requested test,
with Minecraft closed, and compare source/deployed SHA-256 values.

## Repeated villager trading (Decided 2026-10-12, trigger 2026-10-13, L-166)

Moved from DESIGN.md on 2026-11-14 without changes; still the decided behavior.
- Shift + right click on an unlocked trade row trades it as often as
  allowed, default on. Left click and plain right click keep vanilla
  behavior; any key or click stops a run.
- Use vanilla's trade request and confirm each round from authoritative
  inventory and offer state. Stop when sold out, inputs run out or output
  cannot fit; never drop output to continue.
- The maintainer chose this part on 2026-10-13 through its first required
  runtime checkpoint. Favourites and remaining-use presentation remain
  undecided. Research notes: [TRADE-HELPERS.md](TRADE-HELPERS.md).
