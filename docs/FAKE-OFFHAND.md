# Fake Offhand

L-49 implements block placement; L-95 adds the validated instant-use subset
and is closed. Both decision records are in [BACKLOG-DONE.md](BACKLOG-DONE.md).
Timed secondary use was excluded on 2026-10-07; the broader extension scope
below is the earlier research record, not a promise of support. Confirmed
runtime coverage and remaining checks are in [VALIDATION.md](VALIDATION.md).

## Extension scope (chosen 2026-10-06)

The maintainer chose broad secondary-hand capability, rather than one item
category. Keep one switch, one target slot and the existing activation binding.
Implement support in validated steps without narrowing the overall goal.

| Capability | Examples | Research boundary |
|---|---|---|
| Placement | Blocks, torches, seeds | Existing placement plus target-sensitive use |
| Instant use | Buckets, bottles, fire starters, tools, throwables, fireworks, fishing rod | Air/block use, replacement stacks and cooldowns |
| Timed consumption | Food, potions, milk, stews | Start/progress/finish/cancel; correct source slot and returned container |
| Charged/continuous use | Bow, crossbow, trident, spyglass, brush | Charge/release, retained use slot, durability and cancellation |
| Entity use | Feed, tame, shear, milk, lead, name tag, saddle | Target interaction vs selected-hand use; item source and result |
| Holding effects | Shield, map, arrows, totem, Mending and equipment modifiers | Real equipped-hand state; cannot infer support from a use callback |

Target priority is ordinary target interaction and applicable primary-hand
use, then secondary use when the primary hand passes. Distinguish a pass
from failure and mutation; never retry a second use simply because a bool
result is false. Bedrock result types and target-sensitive ordering need
observation before implementing the fallback. L-49 remains unchanged during
this diagnostic step.

Instant uses restore selection in the call. Timed uses must complete or cancel
against their actual source slot and restore afterward, without overwriting
a manual selection. Investigate retaining the primary selection while using
the secondary slot before adopting a visible selection for the whole hold.
An ordinary attack must never accidentally use the borrowed item. Simultaneous
primary-hand attack and secondary use is a separate lifecycle check.

Passive effects remain in scope for feasibility research, not promised parity:
an item in a hotbar slot is not server-authoritatively held in an offhand.
Existing real-offhand support (L-68, L-75, L-94) remains available. Do not
implement client-only health, protection or enchantment effects as if the
server accepted them. Record unsupported cases and the reason per capability.

Native Bedrock differences require their own checks; for example, shielding
uses sneak rather than ordinary use (official platform behavior reference:
[Taking Inventory: Shield](https://www.minecraft.net/en-us/article/taking-inventory--shield)).
No external mod implementation is used.

## Right-click fallback feedback (2026-10-14)

[L-189](BACKLOG.md) records a request to invoke the configured Fake Offhand
item when a primary-hand right-click has no **meaningful applicable effect**.
Reported examples include fireworks while full or when holding a shovel/axe.
This is **not** a firework-specific binding and is not implemented here.

The existing target-priority rule already distinguishes a primary-hand
pass from a failure, but the newly requested no-op cases may be reported
as handled or otherwise consume input without a useful action. Research
the actual game result and state-change boundary, including air/block/entity
targets, before deciding when to fall through. A negative bool alone is
not permission to double-use. Preserve a genuine primary use, prevent
accidental activation, and validate server-authoritative outcomes. No
choice of dispatch strategy, default or release is made yet.

## First instant-use adapter (2026-10-06, single clicks checked)

Eligibility is classified from item properties (2026-10-07), replacing the
earlier per-item allowlists. `FakeOffhandPlan.h` holds the rules and tests;
`FakeOffhand.cpp` reads the properties (`isFood`, `getMaxUseDuration`,
`isThrowable`, `isBucket`, `isLiquidClipItem`, `isHumanoidArmor`/Armor tag,
`isBlockPlanterItem`, `isFertilizer`, `isDye`, `isDamageable`, sword and
pickaxe tags, block item) and never calls a use callback to find a pass.

Secondary (target slot) items that use the instant borrow: projectiles
(`isThrowable` or a known projectile identity) without a use duration, and
buckets except milk. Fireworks qualify while gliding or on a block target, so
they are not wasted in plain air. Fishing rods and on-a-stick items are
excluded because their line follows the selected item; tridents, bows, food
and other timed uses are out of scope (below).

Primary (selected slot) passes when its ordinary use cannot act:

| Aim | Primary passes | Keeps vanilla priority |
|---|---|---|
| Any | Empty hand; vanilla sword/pickaxe tags (checked in game, though swords report a 72000 use duration) | Non-`minecraft:` or unreadable items; wearables; known air-use identities (fishing rod, firework, empty map, books, ender eye, shield, elytra, bundles, carved pumpkin, heads) |
| Air (NoHit) | Every other block item except liquid-placed ones; any item without a use duration, projectile, bucket or liquid-clip property; food at confirmed full hunger | Timed, projectile, bucket and liquid-clip items; hungry, always-edible, creative or unreadable food |
| Ordinary block | Non-block items with none of the properties above and no planter, fertilizer, dye or durability property, and not a known block-use identity (seeds, doors, signs, redstone, string, spawn eggs, minecarts, boats, honeycomb, books, discs, lodestone compass, resin clump, ...), aimed at a solid block without a block entity (2026-10-07) | Block items (placement), food (some plant crops), damageable tools, the listed identities, and every such material aimed at a block with a block entity (decorated pot, lectern, sign, chiseled bookshelf, vault, ...) or a non-solid block |

Entities, active primary timed use and interactive blocks (unless sneaking)
are decided before this and stay vanilla. The block-target identity list is
the uncertain part: an unlisted vanilla item whose block use the properties do
not reveal would be replaced by the secondary use. Requiring a plain target
(solid, no block entity) limits that to uses on ordinary blocks. Report such items so the
list (or a property) can cover them. The trace build logs each item's
properties as letters next to its name to confirm what the game reports.

After a firework use the server reselects the borrowed slot about 50 ms
after restoration (trace, 2026-10-07). For one second after a reported
instant restore, a MobEquipment or PlayerHotbar update that changes the
selection from exactly the restored slot to exactly the borrowed slot is
undone and reported again. Any other selection change is kept.

Holding use while gliding launches one firework, matching vanilla with a
main-hand firework (maintainer, 2026-10-07); no repeat is added.

A held secondary session cancels when the primary becomes applicable (for
example dirt aimed from the sky onto a block); a new press then uses the
primary.

Timed secondary use (food, potions, bows, crossbows, tridents, spyglass) is
out of scope by maintainer decision (2026-10-07): the per-call borrow cannot
keep it, and keeping the target selected for the whole hold was not chosen.

Selection precedes a captured ordinary use-button down edge, so vanilla
acquires the item reference and runs its ordinary air/block paths; the adapter never
replaces a callback's ItemStack argument, retries a false result or calls an
extra use. Base GameMode use/on hooks report the borrowed equipment selection
once before the first native use callback, and restoration reports the prior
slot afterward. No callback means no equipment report. Server acceptance
remains unverified. Selection is restored only while it is still owned; a
later selection is not overwritten. Only primary/target slot identities and
the owed native release survive the call; no game pointer does. Eligibility
and repeat ownership predicates are covered by `FakeOffhandTests.cpp`.

For right-click activation, the native use-button wrapper intercepts eligible
instant use before vanilla attempts the primary item. It selects the target,
replays the complete captured down-handler list once and restores selection.
Raw replay cannot reenter the wrapper. Additional physical handlers and the
queued activation do not deliver a second down edge while this instant hold
is already armed. If the queued activation arrives first, it starts the same
hold and the later physical down is suppressed. Other bindings keep their
queued client-thread activation. Unsupported or unavailable borrowing leaves
the native handler intact. Release is owed during exception
unwinding and until the activation ends. While the same primary/target slots
remain owned and eligible, build calls borrow the instant item and restore
selection within the call. Native hold state decides repeat cadence, including
bucket replacements; no repeat edges, extra uses or timer constants are
inserted. A manual selection, target change, depleted/unsupported target,
menu, death, focus loss, world/dimension exit or disable cancels the hold.
Interactive/entity targeting cancels this limited instant adapter. Existing
input invalidation handles focus, menus and world exit; a dimension hook
releases this hold before the transition. Block targets keep the existing
held-placement path. No action-intention flags are invented.
Test buckets (including collecting water when targeting liquid) and
snowballs/eggs; compare individual clicks and held input with normal cadence.
Observe source count, returned container, effect, restored selection and
rejoin state. Food/bows, entity interactions and passive holding effects are
still unsupported by this adapter. Existing block placement is retained.

The first candidate (`621a7b8`, DLL
`566d683ef87b856ba78167f98cfa2ba38d5b3325072c82e3fc8950a3e3d39f4e`)
failed in every tested empty-hand/sword/pickaxe combination: no water
placement, collection or snowball throwing. Placement still worked. Samples
with a water bucket in slot 8 retained the sword in slot 0 inside build/use
callbacks, establishing a rejected selection rather than server rollback.
The old trace did not log eligibility values; the exact rejecting condition
is not established. The revision removes the maximum-duration heuristic and
admits known target identities and vanilla primary sword/pickaxe tags.
Its diagnostics record both items' maximum use durations and animations,
plus handleBuildAction intention values, to distinguish rejection from a
missing air-use route after borrowing. The revision is not yet validated.

On `c4d6258`, DLL
`9449ca57ae7d6835dfe3d059b4350bd6ca175adad90f317e008290ee1b33ca2a`,
the maintainer reported no snowball action; water placement/collection worked
but could happen in immediate succession and appear to do nothing except
sound. Logs show the snowball selected during build calls but only useItemOn
callbacks, with no air use. Water-to-empty and empty-to-water transitions
occurred about 50 ms apart. Water and empty buckets both report duration 32
and Drink animation, so those properties cannot distinguish milk from water.
This establishes the limitations of the held-build route for instant items.

On `c522b22`, DLL
`6c9c0d4c038f561307e0eb2678e3a9be3ec6aea1918bda9cf9a095b8a009f156`,
the maintainer confirmed snowball throwing, water placement/collection,
block placement and chest interaction. Once-per-hold is a temporary adapter
limitation, not the target contract. Held activation must repeat according
to ordinary item-use cadence and cooldowns; release and ownership loss stop
repetition. Do not replay every frame or invent a universal item interval.
The selected-bucket comparison in that session shows replacement transitions
roughly 200-250 ms apart. In a fresh session on that same diagnostic DLL,
with Fake Offhand, Auto Use and Hand Restock off, the maintainer confirmed
ordinary main-hand snowballs repeated in both air and on block faces. The
trace shows the first air use during handleBuildAction and repeated air use
inside build processing roughly 200-250 ms apart, with source count changes.
This supports retaining the initialized native hold while borrowing selection
only within build calls; the observed timing is not encoded as a universal
item interval.

On `e7ce3f1`, DLL
`5830155266b2baf0858de91f3fd796f3e32b8b0a03115a462d8353f47aaa4d8c`,
the maintainer confirmed empty-primary snowball/bucket repetition and release,
manual-selection cancellation, block placement and chest interaction. Buckets
also worked with swords/pickaxes, but snowballs failed with those primaries.
Failed samples show native primary-item use while the target snowball count
stays unchanged. The rejecting stage is unknown: prior traces did not record
adapter eligibility or replay results. The next diagnostic adds an independent
128-line adapter budget for press choices, block/tag/idle classification,
target/hit/selection state, existing ownership, cancellation and native edge
replay outcomes, without changing gameplay. Compare empty/sword/pickaxe
snowballs in air and on ordinary blocks in a fresh session, then one bucket
control with a failing primary. Do not guess a retry from native bool results.

On `2f7878c`, DLL
`9451ad565dc084ac2622528d794e1c8898614cb72711e224c0581f549e39a019`,
the maintainer ran that long-hold sequence, omitting short clicks. Both tested
tools have the expected tags, no block pointer, and idle classification;
the target is chosen, selection changes and raw down replay returns true.
However, each tool's initial physical use callback runs before queued
activation. Subsequent borrowed snowball air use is absent and target counts
do not change. Empty-primary samples reach and repeat borrowed snowball use;
the sword/bucket control changes containers after replay. This rejects an
eligibility or slot-selection explanation. A prior unsuccessful native item
attempt suppressing the later air-use path is the supported hypothesis;
the exact internal gate is not established. The revised native input wrapper
avoids that prior attempt rather than resetting undocumented flags or
retrying a false callback result.

On `6e41014` (2026-10-07), DLL
`7807d4c8ea445cbf871a2b7632cefddf036bbb6398383414d9a68ee9ee8643c4`,
the maintainer found no problems with empty/sword/pickaxe snowballs in air
and on blocks: long holds repeat and stop on release, and short clicks throw
one. Sword/bucket placement and collection, block placement and chest
interaction also passed. Logs show native borrowing before queued activation,
with the queued duplicate suppressed and the primary selection restored.
The tested instant subset is confirmed. Eggs, other bindings, overlap,
broader cancellation, rejoin, servers and a trace-disabled smoke remain open;
the earlier manual-selection result belongs to `e7ce3f1`.

The sword/snowball single/held/release and bucket smoke also passed on
trace-disabled `58d121d`, DLL
`ce2604e8e8966dc33bbd46f1d240e56a6ff4487eda8fb10c9716c304add16a89`.
The maintainer found the secondary snowball unavailable with a totem and
other, unspecified primary use items. The totem is excluded by the explicit
primary allowlist, not by a use failure. A known-passive allowlist followed and was
replaced by property classification before any runtime check. Food, bows,
buckets, wearables and target-sensitive items retain primary priority rather
than triggering a speculative second use. Determining their applicable pass
remains open and requires the item and target context.

## Placement contract

The feature borrows a configured hotbar slot, default slot 9, while its
activation binding is held. The default binding is right click. It performs
no inventory transfer and does not use the real offhand.

`FakeOffhand.cpp` hooks `ClientInstance::_tickBuildAction`. Its pure predicate
in `FakeOffhandPlan.h` requires a block item and a block hit. Interactive
blocks preserve ordinary interaction unless sneaking. Air, entities and
non-block target items keep the selected hand. Selection is restored inside
each build call, including unwinding, only if the selected slot is still the
borrowed slot. The native right-click handler also borrows the placement
slot around its first press (2026-10-07): vanilla acts there before any build
tick, and an empty hand would otherwise open a container while sneaking.
Other bindings replay the captured vanilla use edges.

## L-95 static review (2026-10-06)

Inspected the installed LeviLamina Client SDK 26.51.5 headers and Lamium's
own adapters; no reference-only mod source was used. These declarations
identify research boundaries, not confirmed runtime ordering:

- `GameMode::useItem` and `useItemOn` cover item use in air and on blocks;
  `interact` is a separate entity boundary. `releaseUsingItem` is available.
- `useItem` and `interact` return bool; `InteractionResult` exposes only
  success and swing bits. There is no declared pass/fail distinction there.
  A safe fallback needs additional evidence about the vanilla route and
  whether an earlier callback changed state.
- `Player::startUsingItem`, `completeUsingItem`, `releaseUsingItem` and
  `stopUsingItem` expose the timed-use lifecycle.
- `Player::mItemInUse` contains an owned item and `PlayerInventorySlotData`
  slot record. Observe those values to establish which slot vanilla follows
  after temporary selection is restored; never retain a player pointer.
- `LocalPlayer::mSentSelectedSlot` tracks reported equipment selection.
  Weapon Switch already reports a new selection before attacking. Additional
  use must establish its own reporting order; copying the attack solution
  does not prove food or charged-item synchronization.
- Hand Restock already observes ordinary and timed uses. Its timed-completion
  guard documents a completion callback immediately after a new use begins.
  Trace both features together; do not interpret every completion callback as
  consumption or attempt to restock from an unconfirmed result.

Removing only the block-item and block-hit guards would allow additional
calls but would leave their timed-use, release and server-selection contracts
unverified. L-49 also records that retaining selection throughout every hold
interfered with main-hand use and was reverted. An extension must distinguish
instant, placement and timed-use ownership.

## Baseline result (2026-10-06)

Maintainer report on `c673fad`, DLL
`49059b210fc94b7c0b831f360680eabe8dd5c9de3911d1294028507fd09bf554`,
with only `offhand_trace` enabled: ordinary food completion/interruption,
bow firing, water placement/collection and feeding were exercised. Both
food and bow use stopped on manual slot changes; left click did not stop
either. Existing Fake Offhand placement and chest interaction were retained.
No additional Fake Offhand category was tested in this diagnostic build.

Log evidence: food starts with duration 32 and records inventory slot 2;
completion/stop clears its use item, with the count change observed later.
Bow starts with duration 72000 and records slot 1; its use callback returns
false despite a populated use item. Release calls stop; changing selection
from 1 to 2 calls stop while the old use item still names slot 1. Water bucket
use can succeed on a block and then call air use with an empty bucket; a
second explicit fallback would risk another action. Feeding enters both the
survival and base interact boundaries. These observations motivate one
whole-call selection scope for instant use and a separate timed-use design.

## Research and verification

`offhand_trace` adds read-only hooks for build ticks, base and survival
air/block/entity use, and player start/complete/stop/release. It logs selected
slot, last reported slot, selected/use/target item identities and counts,
the use slot/container, callback results, duration arguments and timestamps.
It also records build-action intentions and both items' maximum durations
and use animations. Maximum duration alone is not used to admit items.
Slot numbers in these diagnostics are zero-based. Results are observations,
not authoritative confirmation. Build ticks log only state changes; each
stage group has a 256-line lifetime budget. Restart for another category if
the budget is exhausted. The older L-49 and L-66 diagnostics are unchanged.

First baseline session, with Hand Restock and Auto Use off:
1. Fake Offhand off: select food, eat once, then begin eating and release
   early. Repeat with a bow, charging and firing once; select a bucket and
   place/collect water; feed an animal with an applicable item.
2. During food/bow use, manually select another slot and try an attack.
   Observe whether vanilla stops use and which item actually acts.
3. Fake Offhand on, target slot containing blocks: place on an ordinary
   block, open a chest normally, sneak-place against it. Then put food in
   the target slot while holding a tool: additional use is still unsupported
   in this read-only build, so this records the current adapter's baseline.

Build diagnostics with `xmake f --offhand_trace=y -y`, then build Lamium and
LamiumTests. Deploy only for a requested trace test; name the commit, DLL
hash and enabled options. After compile verification, reset with
`xmake f --offhand_trace=n -y` and rebuild the ordinary DLL before committing.
Include physical right click and a non-mouse activation in later adapter
checks. Keep every trace option off for ordinary builds.

For each supported category, the maintainer checks:
1. Actual effect, duration and source stack with the target slot different
   from the selected slot; release before completion and after completion.
2. Interactive blocks, air and entities according to the agreed priority;
   ordinary block placement still follows L-49.
3. Manual slot changes, intervening attacks, an empty/depleted target,
   Hand Restock and Auto Use overlap.
4. Disable, menus, focus loss, death, dimension change and world exit:
   no stale hold or restoration over a later manual selection.
5. Rejoin and server testing: counts, replacement containers and durability
   persist, with no rollback or duplicated effect.

The baseline confirms ordinary use and the existing placement regression.
The native first-input adapter additionally has confirmed single and held
snowball/bucket actions for the tested primary hands. Remaining validation
cases and timed/entity/passive extension categories are still open.
