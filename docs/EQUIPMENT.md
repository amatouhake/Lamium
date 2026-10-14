# Equipment features

Tool Protection (L-62), Tool Switch fetching from the inventory (L-69) and Auto
Elytra (L-70), built on the screenless move from Hand Restock. Decisions are
in BACKLOG-DONE.md; results in [VALIDATION.md](VALIDATION.md), evidence in
[VALIDATION-LOG.md](VALIDATION-LOG.md). Starting a glide from the mod is open
Research (BACKLOG L-71).

## Shared move (`game/InventoryMove`)

`movePair` changes two player-owned slots at once: inventory slots, the
offhand or an armor slot. It refuses (changing nothing) while another
transaction, item-stack request or move is in progress. Inside the client
legacy request scope it calls the vanilla setters, then flushes the balanced
transaction. Inventory setters record their own actions; the offhand and armor
setters record none (traces at 04b594d and 5728561), so the move adds that
side's action itself. Which sides were recorded is learned by observing
`InventoryTransactionManager::addAction` during the move: reading the
transaction's action map after an armor setter crashed the game. A
transaction left unbalanced after the setters throws, and the caller stops.

Moves follow the Hand Restock ordering rule: never right behind a server-side
change they depend on. For durability that means 150 ms after the last break
or durability change.

## Tool Protection (L-62)

On by default, under Interaction.
- A held damageable item that wears down to 1 durability while held (or is
  used to mine at 1) is swapped with the same item: main inventory first, then
  other hotbar slots; closest enchantments, then most durability, then the
  higher slot. The worn item takes the replacement's slot; the selection never
  changes. One kept at 1 for Mending and merely selected is left alone.
- Mining with it waits (no progress) until the swap, then restarts breaking.
- Without a replacement, mining stops with a text-only toast "Stopped: the
  tool is about to break". Child "Never let it break" (on by default) keeps a
  new press from mining on; off, a new press mines on and breaks it.
- A worn elytra at 1 (it stops working rather than breaking) is swapped with
  another elytra the same way. Not yet checked in flight.
- Hooks: GameMode start/continueDestroyBlock (high priority) and a tick
  listener. Breaking Restriction and Tool Switch hook the same calls with
  their own pause/restart; consolidating them is noted in L-73.

## Tool Switch from the inventory (L-69)

Child of Tool Switch, "Fetch from inventory", off by default. When neither the
held item nor the hotbar has an effective tool for the block, the fastest one
in the main inventory (ties: the higher slot; never one about to break) is
moved into the selected slot and stays there, or into a fixed hotbar slot
("Fetch into", L-97; never Fake Offhand's slot) which is then selected. On a
new press it moves at once
if the last break is 150 ms behind; while a held attack moves to another block
breaking pauses until then, the tool moves and breaking restarts.

## Weapon Switch (L-67)

Own switch under Inventory, off by default; the design is retained under L-67
in BACKLOG-DONE.md.
When the local player attacks a living entity (anything with the Mob type
except armor stands), the hotbar item with the highest attack damage plus the
vanilla Sharpness/Smite/Bane bonus against that target is selected before the
attack runs (pure ranking in `WeaponChoice.h`). The held item stays on a tie;
otherwise a sword wins over an equal non-sword. No switch back.
Child "Fetch from inventory" (off): when the strongest inventory weapon hits
harder than every hotbar item (an equal one stays; before 2026-10-06 any
hotbar weapon, even a shovel, blocked the fetch), it (never one about to break) moves into the
selected slot, or into the "Fetch into" slot (L-97), which is then selected
and reported at once, and only when the last hit is 150 ms behind; otherwise the next
hit tries again. Hooks: GameMode and SurvivalMode `attack`.
The client reports its selected slot from its own tick, after the attack's
transaction, and the first build's switching hit did no damage (2026-10-02).
A switch therefore sends the equipment packet at once, as that tick would,
and marks it sent.

## Auto Elytra (L-70)

Experimental, off by default, under Interaction.
- Put on: the key "Put on / take off the elytra" (unbound), or any jump while
  holding firework rockets in the main hand (child "Jump with fireworks", on
  by default). The elytra with the most durability comes from the main
  inventory, then the hotbar; it takes the chest slot and the chest item takes
  its slot. No automatic glide: a second jump glides as in vanilla.
- Take off: the key again, or after a glide or firework jump, a set time after
  the first landing (child, 0-10 s, default 3 s); later hops do not restart it,
  only a new glide does, and the swap happens on a touch of the ground. The
  remembered chest item goes back on, else the chestplate with the highest
  protection (armor, toughness, enchantment levels, durability); the elytra
  takes its slot. With no chestplate the elytra stays on. An elytra worn by
  hand is followed once it glides. A key press on the ground waits for the key.
- Only the jump flag is read every tick; the held item is checked on a jump.

## Auto Elytra feedback (2026-10-14)

[L-180](BACKLOG.md) tracks a report that Auto Elytra does not respond to
Riptide trident launches. The shipped L-70 contract documents jumping
while holding fireworks, not a general Riptide activation. First decide
what the user expects (equip, enter glide or both), then reproduce the
Riptide sequence and verify client/server behavior. This is an open report,
not proof that the existing firework trigger regressed.

## Verification

Pure tests (`tests/EquipmentPlanTests.cpp`) cover durability limits, enchantment
distance, replacement and chestplate choice, the mining decision, inventory
tool choice and the elytra state machine (hand-worn elytra, key on the
ground, sprint-jump delay, empty chest).

In game (release build):
1. Tool Protection: wear a pickaxe down with a spare in the inventory, then
   only in the hotbar (swap, mining continues); without a spare (stop toast;
   strict on and off).
2. Tool Switch fetch: tools only in the inventory, hold across stone and dirt.
3. Auto Elytra: firework jump, glide, land (chestplate after 3 s, also while
   sprint jumping); key on the ground and in the air; hand-worn elytra; key
   only with the firework child off.
4. Re-join after each: no item gained or lost.
