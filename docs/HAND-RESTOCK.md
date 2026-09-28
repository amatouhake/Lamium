# Hand Restock implementation work

Hand Restock remains experimental, Off and Unbound by default. Consumption
observation and hotbar reserve selection succeeded in local survival: after a
one-item stack is consumed, Lamium selects the first compatible reserve in
another hotbar slot. Main-inventory and offhand replenishment have not
succeeded; HUD swap/count-transfer experiments produced no captured inventory
request, and count transfer explicitly returned false. This does not establish
that main-inventory replenishment is impossible: a bounded follow-up may test
the game's ordinary server-authoritative inventory transaction path while
retaining Lamium's existing correlation and cancellation rules.
See [VALIDATION.md](VALIDATION.md) for build hashes and runtime observations.

## Intended behavior

When the selected main-hand stack is consumed, select a compatible reserve
from another hotbar slot through the proven `selectSlot` API (no stacks are
rewritten, no packets forged, no retry loop). Main-inventory replenishment is
an open issue: HUD-controller transfers through `ContainerManagerController`
(place and take both verified false 2026-09-27) have no supported path.
A future research spike may submit one ordinary inventory swap only after the
existing use/depletion correlation has identified a stable source and
destination, then wait for authoritative inventory state before declaring
success. Submission alone is not success, and a mismatch, correction, timeout
or unrelated mutation cancels without retrying. Hypothesis source: Stipuleroo
(GPL-3.0, reference only; PROVENANCE.md group 3b) restocks from the main
inventory on 26.51 with an ordinary inventory transaction. Source inspection,
if recorded under group 3b, is a separate Research pass and does not authorize
sending that transaction.
Bowls, buckets and other consumption replacements remain in the selected slot.

The maintainer also wants an offhand extension if the client exposes a safe
vanilla-backed path, especially automatically replacing a consumed Totem of
Undying from the main inventory. Treat this as a distinct observation/transfer
path until proven otherwise: offhand slot mapping, consumption timing and
controller permissions must be validated independently from the main-hand
adapter. Do not emulate success by writing the stack locally or forging an
inventory packet.

## Current consumption observation

The native adapter snapshots around main-hand GameMode use callbacks and
Player::completeUsingItem. It checks local player identity, dimension, selected
hotbar slot, gameplay input, HUD ownership and agreement between all 36 HUD
slots and player inventory. Creative and spectator players are excluded.

A successful callback closes request capture. Tracked use requests require an
Accepted response. The observed egg path instead sends a complex
ItemUseTransaction after the callback, with no item-stack request batch.
For that Untracked path, the adapter requires a matching main-hand Use/Place
transaction for the selected slot, then observes depletion for at most one
second. A submitted use is not server acknowledgement. The deadline cancels
observation; elapsed time never authorizes replenishment.

The held stack must change from one item to empty. All other inventory slots
must remain unchanged. A manual drop, unmatched transaction, selection/context
change, focus loss, world exit or inventory mismatch cancels observation.
The first unlocked compatible hotbar stack in slots 0–8 is selected using
vanilla item equivalence, including components. There is no fallback to another
reserve after unrelated inventory mutation. Main-inventory slots 9–35 are only
examined by the retained, unsupported transfer planner; they are never selected
by the shipped hotbar fallback.

## Replenishment: hotbar auto-select

After depletion the adapter selects a compatible hotbar reserve through
`PlayerInventory::selectSlot`, the same proven API Tool Switch uses, and
confirms the selection moved. No stacks are rewritten and no transfer token
is needed for the selection itself. Rejected, Untracked or TimedOut use
results still stop without retrying.

## Retired transfer approach and open issue

The HUD exposes hotbar_items with 36 slots, whose occupied entries match the
player inventory. Read access does not establish transfer capability: the
adapter reached plan-ready, but HUD `handlePlaceAmount` returned false and
generated no request (replacing the earlier unsuccessful `handleSwap`).
2026-09-27 trace: both controllers report `closed=false client=true
simulation=false`, so the simulation flag does not explain the failure;
`handleTakeAmount` also returns false under the same token. HUD-controller
transfers through `ContainerManagerController` have no supported path without
a screen, so that specific approach stays retired. Main-inventory
replenishment is BACKLOG L-66 (a bounded experiment, vanilla path first);
do not force-enable permissions, reuse a closed screen controller, rewrite
stacks locally, or treat a sent transaction as confirmation. Totem consumption
in the offhand fires no GameMode use/use-on/complete callback (passive damage
path), so offhand restock needs a separate consumption observer.

## L-66 negative spike result

The 2026-09-29 trace repeated two calls on the already-retired HUD
controller path: `handlePlaceAmount`, then `handleSwap`. Both returned false
synchronously and created no request. The second call was described during
the spike as a client-built transaction, but it was another controller verb;
no client-built transaction was sent. The redundant spike code has been
removed. Its build hash, setup and observed log lines remain in
[VALIDATION.md](VALIDATION.md).

This result rules out only those HUD-controller calls. It does not rule out a
different no-screen vanilla API or a separately constructed ordinary
inventory transaction. Absence of a linkable `ItemStackRequestScope` export
is an SDK observation, not proof that every client-backed path is impossible.
Automatically opening and closing the inventory screen is not an acceptable
substitute for seamless hand restock unless the maintainer explicitly chooses
that user-visible behavior.

## Diagnostics and validation

The opt-in restock_trace build records bounded fixed labels and numeric values:
use stages and complex sends, legacy slot/content updates after vanilla applies
them, capture boundaries, new-request counts and response counts. It logs no
item contents, request IDs, player identities or world identifiers. Legacy
updates and response counts are observations, not correlated use acceptance.
It also logs the HUD controller's transfer context at use time and the
container screen controller's context when a screen opens (closed, client-side
and simulation flags, `Restock transfer context`), to compare the failing HUD
path with the working screen path. Use callbacks now record the hand value,
so offhand (totem) consumption timing can be mapped separately.

Local egg tests establish callback-before-depletion ordering, complex send
ordering and successful depletion planning. Hotbar reserve selection was
verified in game on 2026-09-27 (DLL `1f1f7816`); an inventory-only reserve
correctly stopped with no transfer path. Block/food/firework behavior,
manual-drop cancellation, server rejection/correction, multiplayer and
disconnect handling remain unverified.

Planner tests cover unchanged compatible reserves, interference, replacement
items, locking and context/selection validity. Response ownership tests cover
contention, stale cancellation, unrelated responses and restart. Existing Sort
runtime checks establish its screen-based transfer path, not the HUD path.
Compilation and these tests cannot substitute for native validation.
