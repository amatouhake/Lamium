# Fake Offhand

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/FAKE-OFFHAND.md`.

Uses a configured hotbar slot as a second hand while the activation binding
is held: blocks placed and instant items used from that slot without
selecting it. Under Inventory, off by default (`inventory.fakeOffhand`).
Rows: "Target slot" (`fakeOffhandSlot`, 1-9, default 9), the toggle action
`fakeoffhand` (unbound) and the hold action "Fake Offhand activation"
(`fakeoffhanduse`, right click by default). It performs no inventory
transfer and does not use the real offhand (that is Offhand swap, L-94).
Decision records: [L-49](backlog/done/L-49.md) (placement),
[L-95](backlog/done/L-95.md) (instant use),
[L-189](backlog/done/L-189.md) (target-specific primary passes),
[L-198](backlog/done/L-198.md) (primary-use regressions).

## Scope (maintainer)

- 2026-10-06: the goal is broad secondary-hand capability, not one item
  category, built in validated steps. One switch, one target slot, the
  existing activation binding.
- 2026-10-07: timed secondary use (food, potions, bows, crossbows,
  tridents, spyglass) is out of scope: the per-call borrow cannot keep it,
  and keeping the target selected for the whole hold was not chosen.
- Passive holding effects (shield, totem, map, Mending) are not faked: a
  hotbar item is not server-authoritatively held in an offhand. Never
  implement client-only health, protection or enchantment effects.
- 2026-10-15: target-specific primary uses: include as much as possible,
  and prefer the game's own checks to hand-kept lists.

## Behavior

### Who acts

1. Entities, an active primary timed use and interactive blocks (unless
   sneaking) are decided first and stay vanilla.
2. Then the primary (selected) item keeps priority unless it passes: its
   ordinary use cannot act on this target now.
3. Then the slot's item: placement for a block item aimed at a block face,
   an instant use for an eligible item.

A pass is distinguished from failure and mutation; a second use is never
tried because a callback returned false.

### Secondary items

- Placement: a block item and a block hit. Air, entities and non-block
  items keep the selected hand.
- Instant use: projectiles (`isThrowable` or a known projectile identity)
  without a use duration, and buckets except milk. Fireworks qualify while
  gliding or on a block target, so they are not wasted in plain air.
  Fishing rods and on-a-stick items are excluded: their line follows the
  selected item.
- Eligibility is classified from item properties (`isFood`,
  `getMaxUseDuration`, `isThrowable`, `isBucket`, `isLiquidClipItem`,
  `isHumanoidArmor`/Armor tag, `isBlockPlanterItem`, `isFertilizer`,
  `isDye`, `isDamageable`, sword and pickaxe tags, block item); a use
  callback is never called to find a pass.

### Primary passes

| Aim | Primary passes | Keeps vanilla priority |
|---|---|---|
| Any | Empty hand; vanilla sword/pickaxe tags | Non-`minecraft:` or unreadable items; wearables; known air-use identities (fishing rod, firework, empty map, books, ender eye, shield, elytra, bundles, carved pumpkin, heads) |
| Air (no hit) | Every other block item except liquid-placed ones; any item without a use duration, projectile, bucket or liquid-clip property; food at confirmed full hunger | Timed, projectile, bucket and liquid-clip items; hungry, always-edible, creative or unreadable food |
| Ordinary block | Non-block items with none of the properties above and no planter, fertilizer, dye or durability property, and not a known block-use identity (seeds, doors, signs, redstone, string, spawn eggs, minecarts, boats, honeycomb, books, discs, lodestone compass, resin clump, ...), aimed at a solid block without a block entity | Block items (placement), food (some plant crops), damageable tools, the listed identities, and every such material aimed at a block with a block entity (decorated pot, lectern, sign, chiseled bookshelf, vault, ...) or a non-solid block |

L-198: the primary-pass check applies to placement as well as instant use;
placement is refused while a primary timed use is active. A normal primary
block passes in air even if its native item reports a use duration;
liquid-placed and wearable/known air-use items keep their checks. Aimed at a
block, the primary block keeps placement.

Target-specific passes (L-189), on top of the table: a primary item whose
use only acts on certain blocks or states passes when the game says it
cannot act now; anything unread keeps vanilla priority. Block targets must
be plain (no block entity, and solid or a collision that fills the cell,
such as leaves).
- Planters (seeds, carrots, potatoes, nether wart, berries, cocoa beans):
  the planted block's `mayPlace` and `canSurvive` on the aimed face, with
  air there; when the planted block is unknown, crops need the top of
  farmland and nether wart the top of soul sand (`plantsOn`); else the
  component's `canUseOn`. Vanilla planters also count by name (`plants`).
- Fertilizers (bone meal): the block's `canBeFertilized` with the block
  above. Minecarts: `isRailBlock`.
- Honeycomb: copper that is not waxed yet. Axes: an unstripped log, wood,
  stem, hyphae or bamboo block (by name), or copper that is waxed or
  oxidized.
- Shovels (tag), hoes (tag) and shears: short vanilla block lists
  (`shovelChanges`, `hoeChanges`, `shearsChange`).
- Empty buckets and glass bottles: water or lava along the aim within reach.
  Food at confirmed full hunger passes on plain blocks unless it plants and
  its planter could be used there. Dyes, ink sacs and books pass on plain
  blocks (their uses need block entities).
- Anywhere: a Riptide trident out of water and rain, a bow without arrows,
  an uncharged crossbow without arrows or fireworks (survival; inventory
  and offhand searched). Aimed at nothing: a firework while not gliding, an
  empty bucket or bottle with no liquid along the aim, a filled bucket with
  no block face to pour on (L-290).

These block rules are gated by the `FakeOffhandTargets` capability ("Fake
Offhand (item use targets)", [GAME-UPDATES.md](GAME-UPDATES.md)); on an
unverified game they fall back to the table, and the slot's own instant
uses stay available.

### Bindings

- Right click: the native use-button handler intercepts an eligible instant
  use before vanilla tries the primary item. It selects the target, replays
  the captured down-handler list once and restores selection. It also
  borrows the placement slot around the first press, since vanilla acts
  there before any build tick (an empty hand would otherwise open a
  container while sneaking). A queued activation and later physical
  handlers do not deliver a second down edge for an armed hold; if the
  queued activation arrives first, it starts the same hold and the later
  physical down is suppressed.
- Any other binding (L-288, L-292): replays the captured vanilla use edges
  with the borrowed slot selected, only while a build tick has a slot. Such
  a binding asks for the Fake Offhand, so the primary item does not keep
  priority, and an interactive block aimed without sneaking is opened with
  the borrowed slot selected. With the target slot itself selected, a press
  borrows nothing and sends the vanilla use edge, held until the key is
  released, so the selected item acts as a right click would (L-322).
- With Sneak Placement on (L-293), the slot's item is what acts, so the
  interactive-block choice takes Sneak Placement's sneak state instead of
  the player's, for every binding: the slot's block is placed on a chest
  with an empty main hand; with "Open while sneaking", sneaking opens it.
  Without that option such a chest opens only with Fake Offhand off or its
  slot empty. When Sneak Placement holds a use back and replays it, the
  replay selects the slot the use came from for that call only.
- Auto Elytra reads the target slot's fireworks for its jump trigger
  ([EQUIPMENT.md](EQUIPMENT.md) "Auto Elytra").

### Selection and holds

- Selection is restored inside each build call, including unwinding, and
  only while it is still the borrowed slot; a later manual selection is not
  overwritten. Only primary/target slot identities and the owed native
  release survive a call; no game pointer does.
- The adapter never replaces a callback's ItemStack argument, retries a
  false result or calls an extra use. Base GameMode use/on hooks report the
  borrowed selection once before the first native use callback;
  restoration reports the prior slot afterward. No callback, no report.
- Held instant use: while the same primary/target slots remain owned and
  eligible, build calls borrow the item and restore within the call. The
  native hold decides the repeat cadence (bucket replacements included); no
  repeat edges, extra uses or timer constants are added. Holding use while
  gliding launches one firework, as with a main-hand firework.
- A manual selection, target change, depleted or unsupported target, menu,
  death, focus loss, world/dimension exit (a dimension hook releases the
  hold first) or disable cancels the hold. Interactive or entity targeting
  cancels an instant hold. A held secondary session cancels when the
  primary becomes applicable (e.g. dirt aimed from the sky onto a block); a
  new press then uses the primary.
- For one second after a reported instant restore, a MobEquipment or
  PlayerHotbar update that changes the selection from exactly the restored
  slot to exactly the borrowed slot is undone and reported again. Any other
  selection change is kept.
- An ordinary attack never uses the borrowed item.

## Code

- `src/features/inventory/FakeOffhand.cpp`: hooks
  `ClientInstance::_tickBuildAction` (build ticks), `GameMode::useItem` /
  `useItemOn` (selection reports), `LegacyClientNetworkHandler::handle` for
  MobEquipment and PlayerHotbar (echo undo),
  `LevelRendererPlayer::onWillChangeDimension`; `targetUse()` (L-189 game
  checks), `primaryPasses`, `nativeDown` (called from
  `src/features/interaction/PeriodicInput.cpp`'s use-button wrapper).
- Pure: `FakeOffhandPlan.h` (`primaryPass`, `secondaryInstant`,
  `targetUseOf`, `plants`, `plantsOn`, the tool lists, `undoesRestore`,
  queued-press and hold-edge decisions, slot choices), tested in
  `tests/FakeOffhandTests.cpp`.
- `xmake f --offhand_trace=y`: read-only hooks for build ticks, base and
  survival air/block/entity use, and player start/complete/stop/release;
  logs selected and last reported slot, item identities and counts, use
  slot, callback results, durations, build-action intentions, each item's
  properties as letters, and an adapter budget for press choices and replay
  outcomes. Slots are zero-based; each stage group has a fixed line budget.
  Never deploy or commit with it on.

## Known

SDK headers (read on 26.51.5, not re-read for 26.60.4):
- `GameMode::useItem` / `useItemOn` cover air and block use; `interact` is
  the entity boundary. `useItem` and `interact` return bool;
  `InteractionResult` has only success and swing bits: no declared pass/fail
  distinction.
- `Player::startUsingItem`, `completeUsingItem`, `releaseUsingItem`,
  `stopUsingItem` are the timed-use lifecycle; `Player::mItemInUse` holds an
  owned item and its slot record. `LocalPlayer::mSentSelectedSlot` tracks
  the reported selection.

Observed in game (trace builds):
- A selection change stops vanilla food and bow use; left click does not.
  A bow's use callback returns false with a populated use item.
- A water bucket can succeed on a block and then call air use with an empty
  bucket: a fallback second use would act twice.
- Water and empty buckets both report duration 32 and the Drink animation;
  swords report 72000 though they have no use. Duration cannot classify.
- Wheat seeds and carrots report neither the planter property nor its
  component. The hit's liquid flag stays unset with an empty bucket aimed
  at water. Leaves are not solid.
- After a firework use the server reselects the borrowed slot about 50 ms
  after restoration.
- Vanilla repeats a held throw inside build processing, about 200-250 ms
  apart; the first air use happens during handleBuildAction.
- Vanilla acts on the first right-click press inside the native handler,
  before any build tick.

## Tried

- Keeping the target selected for the whole hold: interfered with main-hand
  use; reverted (L-49).
- Per-item allowlists for primary passes: replaced by property
  classification (a totem primary blocked the secondary).
- A maximum-use-duration heuristic for eligibility: rejected every
  empty/sword/pickaxe case; durations do not classify (above).
- Borrowing only in the held build route: no air use for snowballs, and
  water placed and collected in immediate succession.
- Borrowing after the queued activation: the primary tool's physical use
  callback ran first and suppressed the later borrowed air use. Hence the
  native wrapper, which avoids that attempt; undocumented flags are not
  reset and false results are not retried.

## Limits

- Not supported: timed uses (food, potions, bows, crossbows, tridents,
  spyglass), entity uses (feed, tame, shear, lead, name tag, saddle) and
  passive holding effects.
- The block-target identity list is the uncertain part: an unlisted vanilla
  item whose block use the properties do not reveal is replaced by the
  secondary use on plain blocks. Report such items so a list or property
  covers them.
- Server acceptance of the borrowed selection is not established beyond
  everyday use.
- Unchecked in game: the "Fake Offhand (L-49, L-95)" row in
  [VALIDATION.md](VALIDATION.md); the own key with eating, a bow, a shield
  or a trident ([L-342 Lower findings of the 2026-11-14 review, kept for later](backlog/L-342.md)).
