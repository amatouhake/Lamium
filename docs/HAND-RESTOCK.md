# Hand Restock implementation work

The replenishment planner and initial native adapter are implemented. The
experimental feature is Off and Unbound by default, with controls in Features
and Hotkeys. Native build, settings round trips, translations, toggle behavior,
planner and response-ownership tests pass. The first native runtime check failed:
a final egg was consumed in local survival, but the 15 reserve eggs stayed in
the main inventory and the selected slot stayed empty. No restock result/error
was logged. Actual replenishment is not validated; do not treat the earlier
read-only HUD probe as proof.

The intended feature replenishes a consumed held block, food or firework from
the main inventory. It must run through vanilla inventory operations and wait
for their responses. It does not synthesize inventory contents or require a
server mod.

## Depletion and candidate selection

`RestockPlan.h` consumes snapshots surrounding a successful use operation. An
ordinary empty-slot observation is insufficient: dropping an item or manually
moving it must not trigger restock. The selected hotbar slot must change from
one item to empty, with the same player/world/input context and selection.
Containers left by consumption, such as bowls or buckets, are preserved.

Candidate sources are unchanged, unlocked stacks in main-inventory slots 9–35.
Other hotbar slots and equipment are preserved. Kind IDs must be assigned by the
native adapter using vanilla equivalence including item components, rather than
type names alone. The first compatible source wins. The plan stores the expected
source and rechecks it, the empty destination, selection and context immediately
before dispatch.

Tests cover depletion, unsuccessful use, pre-existing empty hands, replacement
items, locked slots, different kinds, source changes, changed selection/context,
invalid selected indices and exhaustion of eligible candidates.

## Native integration and remaining validation

`HandRestock.cpp` observes main-hand `GameMode::useItem`, `useItemOn` and
`Player::completeUsingItem`. Nested use callbacks do not start another operation.
The adapter retains a weak HUD controller and copied stack representatives;
player runtime ID, dimension, selected slot, input availability and model owner
are rechecked. All 36 HUD slots must agree with the player inventory before
planning or dispatch. Creative and spectator players are excluded.

A successful use callback closes request capture without deciding whether the
held stack has depleted. A subsequent tick first waits for the captured use
requests to be accepted, then takes the post-use snapshot and creates the
depletion plan. Only then does the adapter revalidate the plan and invoke the HUD controller's
`handleSwap` between the compatible source and empty selected slot. This second
operation has a separate ownership token and must also be acknowledged; source
and destination are checked afterward. Untracked, rejected or timed-out use
requests stop without replenishment. There is no timer-based assumption that
use succeeded, and no retry loop after failure.

World exit and focus loss cancel pending work immediately. A client tick cancels
on changed settings, screen input ownership, death, player/dimension/selection
change, or an unavailable/mismatching HUD model. Runtime checks must establish
whether each consumption path actually creates observable item-stack requests
and whether delayed updates require a different observation point. The current
adapter may deliberately stop as Untracked; that is not successful restock.

The opt-in `restock_trace` build also emits up to 128 fixed use-stage labels
and numeric values while Hand Restock is enabled. These distinguish hook entry,
eligibility/HUD checks, capture acquisition, depletion planning and context
cancellation. They do not log item contents or player/world identifiers, enable
the feature, bypass a guard, or add transfers. Egg diagnostics showed that both
the base and outer survival use callbacks returned before the held count fell.
Planning now occurs after tracked acceptance rather than at callback return.
Whether egg use produces a trackable request still requires native validation;
an Untracked result remains a cancellation, never permission to transfer.

The installed SDK exposes `HudScreenController::mHudScreenManagerController`
and the controller's vanilla `handleSwap` operation. Existing Sort uses
`ContainerManagerController` plus request-ID observation and authoritative
responses, but its screen tracking only follows container screens. Restock must
obtain and validate the HUD-owned controller without retaining it across world
or screen transitions. Collection names, slot mapping and source availability
must be verified against the live HUD model before attempting transfers.

A read-only `restock_trace` build now observes the first eight client HUD
controller creations. In a local creative-world check, the controller exposed
one collection, `hotbar_items`, with **36** slots. Every occupied slot 0–21
uniquely matched the same index in `Player::getInventory()`. Thus the HUD
collection name does not mean it contains only nine slots. Empty slots 22–35,
survival/multiplayer and actual transfers remain unverified. The adapter must
check collection existence, size and slot contents at use time; this observation
does not justify hardcoded unchecked access. The diagnostic is off by default,
never transfers items, and logs only bounded engine names/indices and match
counts, not item names/NBT or world/account identifiers.

Immediate use (`GameMode::useItem` / `useItemOn`) and delayed consumption
(`Player::completeUsingItem`) need separate runtime checks. A successful use
return value alone does not prove server acceptance. Avoid nested/double
observations and wait for the use operation's inventory state to settle before
starting the restock request.

The shared response tracker now grants a unique token to each operation. A
second acquisition fails without changing the first operation's pending IDs.
Reading, ending capture and releasing an operation require its token; stale
cancellation cannot erase a newer operation. Sort uses this interface and
releases its token after acknowledgement or cancellation. Restock must use the
same interface when connected. Tests cover contention, stale cancellation,
unrelated responses and shutdown/restart. A local creative-world regression
check completed 11 acknowledged sorting operations, followed by a separate
one-operation stack consolidation. Multiplayer contention, rejection and
timeouts remain unverified; see [VALIDATION.md](VALIDATION.md).

Cancel on settings/input capture, focus loss, death, world/dimension/player
change, selection change or inventory mismatch. A rejected, untracked or timed
out replenishment must stop without an automatic retry loop. Runtime validation
must cover blocks, food, fireworks, delayed/rejected responses, manual drops,
source mutations and leaving the world. The planner tests are not evidence that
these native paths work.
