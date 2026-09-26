# Hand Restock implementation work

Hand Restock remains experimental, Off and Unbound by default. Consumption
observation reaches a valid plan in local survival, but actual replenishment
has **not** succeeded. Both HUD swap and count-transfer experiments produced
no captured inventory request; count transfer explicitly returned false.
See [VALIDATION.md](VALIDATION.md) for build hashes and runtime observations.

## Intended behavior

When the selected main-hand stack is consumed, select a compatible reserve
from another hotbar slot through the proven `selectSlot` API (no stacks are
rewritten, no packets forged, no retry loop). Main-inventory replenishment is
an open issue: HUD-controller transfers through `ContainerManagerController`
(place and take both verified false 2026-09-27) have no supported path.
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
The first unlocked compatible main-inventory stack in slots 9–35 is selected
using vanilla item equivalence, including components. There is no fallback to
another reserve after unrelated inventory mutation.

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
a screen, so main-inventory replenishment stays an open issue. Do not
force-enable permissions, reuse a closed screen controller, or alternate
transfer methods without new evidence. Totem consumption in the offhand fires
no GameMode use/use-on/complete callback (passive damage path), so offhand
restock needs a separate consumption observer.

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
ordering and successful depletion planning. They do not validate actual
replenishment, block/food/firework behavior, manual-drop cancellation,
server rejection/correction, multiplayer or disconnect handling.

Planner tests cover unchanged compatible reserves, interference, replacement
items, locking and context/selection validity. Response ownership tests cover
contention, stale cancellation, unrelated responses and restart. Existing Sort
runtime checks establish its screen-based transfer path, not the HUD path.
Compilation and these tests cannot substitute for native validation.
