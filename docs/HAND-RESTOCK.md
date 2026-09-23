# Hand Restock implementation work

The replenishment planner is implemented and tested, but is not connected to
Minecraft yet. No setting or automatic inventory action is enabled by this work.

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

## Native integration still required

The installed SDK exposes `HudScreenController::mHudScreenManagerController`
and the controller's vanilla `handleSwap` operation. Existing Sort uses
`ContainerManagerController` plus request-ID observation and authoritative
responses, but its screen tracking only follows container screens. Restock must
obtain and validate the HUD-owned controller without retaining it across world
or screen transitions. Collection names, slot mapping and source availability
must be verified against the live HUD model before attempting transfers.

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
