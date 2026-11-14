# Enchantment order

Feature contract and dated record for the enchantment order planner and its
panels beside the anvil screen (L-170, Experimental). Work status and open
choices are in
[BACKLOG.md](BACKLOG.md#l-170-enchantment-order-planner); runtime coverage
is in [VALIDATION.md](VALIDATION.md), with evidence in
[VALIDATION-LOG.md](VALIDATION-LOG.md). The player's description is in
[FEATURES.md](FEATURES.md); the game members it relies on are listed in
[GAME-UPDATES.md](GAME-UPDATES.md).

Read "Current behavior" first. The "Record" below it is the build diary in
the order it was written; a later round often replaces an earlier one, and
the record is not rewritten.

## Current behavior

Written 2026-11-14 from the whole record and checked against the code on
`main` (last change to the feature's sources: `47d84516`, 2026-11-13). It
says what the code does; what was seen in game is in VALIDATION.md, and
"Not checked in game" below lists what was not.

### Where the code is

| Part | File |
|---|---|
| The order search, cost rules, matching a stack to the plan (pure) | `src/features/inventory/EnchantPlan.h` |
| What the chooser edits, progress, slot marks (pure) | `src/features/inventory/EnchantOrder.h` |
| Game reads, the session's plan, the panels beside the anvil | `src/features/inventory/EnchantOrderGame.cpp` |
| The planner view inside the settings screen | `src/ui/EnchantView.cpp` |
| The choosing column, shared by both places | `src/ui/EnchantChooser.cpp`, `EnchantLayout.h` |
| The steps' side, shared by both places | `src/ui/EnchantSteps.cpp` |
| Presses and wheel turns kept from the anvil screen | `src/input/CustomInput.cpp` |

Tests: `tests/EnchantPlanTests.cpp` (the planner, the edits of
`EnchantOrder.h` and the layouts).

### Opening, settings and the version gate

- The feature is the row "Enchantment order" under Inventory, marked
  Experimental. The row has an Open button and no switch of its own. The
  action `openenchantorder` ("Lamium: Open the enchantment order planner")
  has no default key.
- Three settings under that row, saved in the settings file: "List the
  steps beside the anvil" (`inventory.enchantOrderAnvil`, off for a new
  install since 2026-11-14, L-338: an Experimental feature draws nothing
  until asked; a saved on stays on), "Choose enchantments beside the anvil"
  (`inventory.enchantOrderAnvilChoose`, on) and "Tint the slots to use next"
  (`inventory.enchantOrderAnvilMarks`, on). The last two are siblings of the
  first, not settings under it, but only work while the first is on. With
  the first off the anvil screen is the game's own; the planner still works.
- Everything asks `versionSensitiveAllowed(EnchantOrder)`: the cost classes
  and the anvil's own cost are meanings a rebuild cannot check. On a game
  version that is not verified the planner does not open (it says "Not
  checked on this game version, so no order is planned.") and nothing is
  drawn beside the anvil.
- The planner needs a world. It reads the game's enchantments each time it
  opens; if that fails it says so and stays closed.
- Nothing of the plan is saved. The chosen kind, the wanted enchantments,
  the goal, the plan and the last choice per kind are kept in memory. The
  stacks read from the game are dropped when the planner closes and on
  world exit; the plan's values themselves are cleared only when the feature
  stops (read from the code: they are not cleared on world exit).

### What is planned

- Data comes from the game: the enchantment registry (type, highest level,
  frequency, disabled, which pairs are incompatible), what goes on an item
  (`EnchantUtils::canEnchant` on one example item per enchant slot), names
  from the game's own texts. Lamium has no table of enchantments.
- A kind is an enchant slot, not a material: sword, pickaxe, axe, shovel,
  hoe, bow, crossbow, trident, mace, spear, the four armor pieces, fishing
  rod, elytra, shield, shears, flint and steel, brush, the two "on a stick"
  items, compass, carved pumpkin, skull, and enchanted books merged into
  one (a plan without an item). A candidate the game does not have, one
  that takes no enchantment, or one whose slot was already listed is left
  out. A kind is named by the game's creative group name where it has one
  (`itemGroup.name.*`), else by the example item's name.
- The plan's inputs are the item (input 0, always the kept side) and one
  fresh book per wanted enchantment: at most 9 books, 10 inputs.
- A step costs the sacrificed side's enchantment levels times the book rate
  of each, plus the prior-work surcharge of both sides (2^uses - 1 each);
  the result has max(uses) + 1. The book rate per level comes from the
  game's frequency value: 30 and 10 give 1, 3 gives 2, 1 gives 4. Another
  frequency value gives no plan and the message "An enchantment here has a
  cost class Lamium does not know".
- Books are always counted as fresh. Merging two books of one enchantment
  into a higher level is not planned. Damage, repair and renaming are not
  part of the cost.
- Outside Creative an order with a step of 40 levels or more is left out;
  if every order has one, the screen says so. In Creative there is no
  limit. The game mode is read when the planner opens.
- Two goals. Fewest levels: the lowest total, then the lowest dearest step,
  then the fewest anvil uses on the result. Least prior work: the fewest
  anvil uses on the result, then the lowest total, then the lowest dearest
  step. Fewest levels is the default; both are always worked out so the
  other one's outcome can be told.
- Planning from an item as it is (an item cell, below): its enchantments
  are fixed and its earlier anvil uses (from its repair cost) are charged.
  A level it carries can be raised with one higher book, which is charged
  the difference in levels when it reaches the item and counts as one of
  the nine books.
- An enchanted book as that item is input 0 too, but it may end up the
  sacrificed side, at the book rate of what it carries. Its own
  enchantments cannot be raised.
- A plain book is not offered as an item, although the anvil does enchant
  one from an enchanted book: once through the anvil the plan could not
  tell it from the fresh books.

### The choosing column

One piece of code draws and handles it in the planner view and left of the
anvil.

- Top line: the chosen item's icon and name, with "(in your main hand)",
  "(in the anvil)" or "(the plan's item)" when it is planned from an item.
- The kind cells (20 units, full-size icon), then the item cells with a
  lighter fill and frame: the main-hand item (the selected hotbar slot;
  the inventory is not searched), and at the anvil the item in the anvil's
  left slot. An item cell is offered only when the item can be enchanted
  and its anvil uses could be read. Choosing a kind plans a fresh item;
  choosing an item cell plans from that item's state.
- The plan's item is a record taken when its cell is pressed (kind,
  enchantments, anvil uses, name), not the live stack, so the plan stays
  the same while the item moves between hand, slot and pointer. A cell is
  shown chosen while the item in it matches the record; of two like items
  in the hand and the slot only the pressed cell is.
- Pressing the kind of the item the plan was made from turns the plan into
  "this item from fresh": what the item carries becomes wanted at the same
  levels.
- Set buttons: "All highest", "Last choice", "Clear". All highest turns on
  every enchantment at its highest level, without curses, and raises what
  the item carries; of enchantments that exclude each other it takes the
  one with the highest frequency weight, then the one that goes on the
  fewest kinds, then the first. Last choice brings back what was chosen for
  that kind before (kept when the kind changes, a set button is pressed or
  the planner closes).
- Rows, under "Enchantments n/9", in the game's order with enchantments
  that exclude each other together and curses last, curses in the game's
  red. A row reads name, level stepper (`-` level `+`), switch at the right
  edge. Off: the level it would turn on at (the highest), faint, without
  buttons. One level only: the switch alone. A press turns a row on or
  off; `-` below level 1 turns it off.
- A row that conflicts with something chosen or on the item is faint and
  says "Conflicts with <name>" in place of the stepper and switch. With
  nine books chosen the other rows cannot be turned on ("At most 9 books").
- A row for what the item carries has a dimmed switch that stays on, its
  level in the level column, and level buttons from its level up (not for
  an enchanted book). Pressing it changes nothing and explains itself in
  the status line.

### The steps' side

One piece of code draws it in the planner view and right of the anvil.

- The goal as a stepper box (plain text where it cannot be switched). With
  the pointer on it the status line tells the other goal's total and anvil
  uses, or that both goals give the same order.
- One run of text: the total, the player's level ("(you have N)", in the
  warning colour when below the dearest step, not shown in Creative), and
  "Materials have/need" (warning colour when short). Materials are the
  plan's inputs, held alone or inside a merged stack.
- Two details, side by side or a line each: "Needed at once: N levels" (the
  dearest step) and "Work penalty of the result: +N"; each is explained in
  the status line on hover.
- The steps, two rows each with a line between steps: the kept side with
  its icon and the step's cost, then "+" and the sacrificed side with
  "incl. N" when N of the cost is prior work (explained on hover). A side
  is named by its contents. The item's icon glints from the step at which
  the item carries an enchantment. A name cut short shows whole in a box
  by the row while the pointer is on it.
- Three ways to tell steps apart: a name in the warning colour is not at
  hand, a faint step is done, a white bar marks the next step. A step's
  cost is in the warning colour when the player's level is below it.
- The last line, up to three lines: the plan is finished; else "Next: step
  N" when its two stacks are at hand (or nothing is missing); else "Not in
  your inventory: ..." with everything the whole plan lacks.
- Where the player stands: a step is done when one stack at hand holds
  everything the step joins. Only the plan's inputs and its steps' results
  count as part of the plan. A finished item is ignored while another item
  of the plan that is not finished is at hand (a second one is being made).
  The plan's item is recognised by its enchant slot and enchantments, so
  another item of the same kind with the same enchantments counts as it.

### The planner view

- It is a view of the settings screen: a heading "Inventory > Enchantment
  order" with a button back to Inventory, the choosing column left, the
  steps' side right, and a three-line footer (status or hover help; how to
  use the rows; the 40-level limit or the Creative note, ending "Esc:
  back").
- Opening: an unfinished plan opens as it was left, whatever is in the main
  hand. Without one, the main-hand item is taken when it can be enchanted
  (from its state when its anvil uses could be read, else as a fresh item
  of its kind, with nothing said); else the kind left last, or the first.
- The steps' side follows the player's inventory (36 slots) every frame.
- The wheel scrolls the side the pointer is on; both scrollbars can be
  dragged. A press outside the view goes to the settings screen (sidebar,
  Close) and leaves the view. Esc leaves the view for the settings table.

### Beside the anvil screen

Drawn only while "List the steps beside the anvil" is on and the game
version is verified.

- The steps' side stands right of the anvil window, up to 200 units wide.
  With less than 110 units there it goes left of the window; with less
  than 110 on both sides nothing is drawn.
- The choosing column stands left of the window when its setting is on and
  there are at least 130 units left and 110 right of the window. It is
  always shown then, with one fixed height and three status lines under
  its rows (a message stays four seconds). Its main-hand and left-slot
  cells follow the hand and the slot while the anvil is open.
- With nothing chosen and no plan, an item put in the anvil's left slot is
  taken as the plan's item. Otherwise the slot only offers its cell.
- Without the choosing column the steps' side has a "Change" button at its
  bottom right, with or without a plan. A left press on it with nothing on
  the pointer opens the planner view over the anvil screen; the anvil and
  the items in its slots are still there afterwards.
- Without a plan the steps' side shows the goal, the heading and a line
  saying what to do, in the room of two steps.
- Input: left presses and wheel turns over the panels are kept from the
  game, judged from the boxes and pointer of the last frame and acted on
  with the next frame on the client thread. Nothing is taken while an item
  is on the pointer or while the panels are fading. The goal is switched
  there only while the choosing column is shown. The wheel scrolls the rows
  or the steps; a press on a scrollbar jumps (no dragging).
- What is at hand at the anvil: the two input slots, the pointer, the
  hotbar and the inventory.
- The step whose two stacks lie in the anvil's slots is tinted, and the
  last line then speaks of that pair first: "The slots match step N";
  "The game says N levels; the plan may be off" when the anvil's own cost
  (`AnvilContainerManagerController::mCost`) differs from the plan's; or
  that the left item, the right item or the pair is not part of the plan.
- Tints ("Tint the slots to use next"), a half-strength fill over the whole
  slot, drawn over the item:
  - In the hotbar and inventory, the first stack holding exactly the kept
    side in cyan and the first holding exactly the sacrificed side in pink.
    A side that lies in its own anvil slot or is carried on the pointer is
    not marked; the other one keeps its mark. A stack on the pointer is
    not marked.
  - The anvil's left and right slot: empty and its stack at hand, the
    side's colour; holding its own stack, the same colour with a frame;
    holding anything else, red with a frame.
  - The result slot, with both slots filled: green when the pair is a step
    at the plan's cost, yellow when the game's cost differs, red when the
    pair is no step.
  - The step these are held against is the one the placed pair makes, else
    the next step.
- Drawing: the panels are painted right before the game's item tooltip in
  a frame that has one (above the screen's dark layer, under the tooltip),
  else after the screen. They are always drawn. An item on the pointer
  that overlaps a panel is drawn once more on top of it, with its glint,
  durability bar and count. Lamium never paints from the slot renderer.
- Closing: the panels slide and fade with the anvil window. Everything
  attached to the anvil closes with it. A finished plan is cleared when the
  anvil has closed, its enchantments going to "Last choice"; an unfinished
  plan stays for the session.

### Not checked in game

From the feature's row in VALIDATION.md (all checks so far: local world,
Creative, UI sizes 50% to 100% for the panels):

- Survival: the 40-level refusal, the level beside the total and its
  warning colour.
- The key (unbound by default).
- Other GUI scales and narrow windows, where the panels move or hide.
- The warning for a cost that differs from the game's, and the result
  slot's yellow (could not be brought about).
- A plan of books alone at the anvil.
- Servers.
- The fallback on a game version that is not verified.

Not modelled and never read from the anvil: a damaged input, a rename, a
held enchanted book whose own enchantment is raised.

## Decisions

Maintainer decisions still in force, one line each; the wording and the
reasons are in the Record under the same date.

- 2026-10-12: books are planned as fresh; planning from the books carried
  can come later.
- 2026-10-12: an unbound key and the settings screen open the planner.
- 2026-11-12: two goals, fewest levels (default) and least prior work on
  the result.
- 2026-11-12: two books of one enchantment are not merged into a higher
  level; more than 10 inputs is refused.
- 2026-11-12: one step list with the goal above it; that both goals give
  the same plan is said.
- 2026-11-12: the item is chosen by kind from an icon grid (no `◀ ▶`
  stepper); what goes on an item follows its enchant slot.
- 2026-11-12: "held" is the main hand only; the hotbar and inventory are
  not searched.
- 2026-11-12: a held item's enchantments are fixed and its anvil uses are
  charged.
- 2026-11-12: a step names an intermediate book by its contents; items and
  books are drawn with their icons.
- 2026-11-12: Creative follows the game mode and has no limit; no switch.
- 2026-11-12: curses are listed last and drawn in the game's red; the
  screen is called "エンチャント順序".
- 2026-11-12: sets are "All highest" and the last selection per kind; no
  Lamium-written or saved sets.
- 2026-11-12: kind names come from the game in every language, never from
  Lamium.
- 2026-11-12: "All highest" leaves curses out and takes one of a group
  that excludes each other by the game's data alone; no Lamium table.
- 2026-11-12: the item to plan from is a cell after the kinds, set apart
  by fill and frame (no mark on the item's picture, no switch row).
- 2026-11-12: a press on an enchantment the item carries only explains
  itself; a carried level can be raised (the anvil charges the difference).
- 2026-11-12: a row reads name, level stepper, switch at the right edge,
  with the level in one place whatever the row's state.
- 2026-11-12: the steps beside the anvil are part of the first version and
  have a switch, on by default; the planner itself has no switch.
- 2026-11-12: the panels beside the anvil are always drawn; an item on the
  pointer is shown on top of them.
- 2026-11-12: do not paint from `InventoryItemRenderer::$render` (it left
  the list and enchanted items white).
- 2026-11-12: the item's icon glints at the steps where the item carries
  an enchantment.
- 2026-11-12: the previous plan wins: an unfinished plan opens as it was
  left; item cells switch only when pressed; no new setting.
- 2026-11-12: at the anvil the chooser stands left of the window and the
  steps right; the chooser is always shown, with a setting to turn it off
  and a "Change" button where it is not shown.
- 2026-11-12: the item is not taken from the anvil's left slot by itself;
  both item cells (main hand, left slot) come after the kinds. (As built
  the same day, a slot item is taken when nothing is chosen and there is
  no plan.)
- 2026-11-12: Lamium's planner screen stays beside the anvil panels;
  everything attached to the anvil screen closes with it.
- 2026-11-12: the anvil's chooser shares the planner's look and handling
  wherever the anvil screen does not force a difference.
- 2026-11-12: one steps' component for both screens; the goal is a
  stepper; no materials list, a count beside the total instead.
- 2026-11-12: the details read "Needed at once" and "Work penalty of the
  result"; the player's level goes beside the total.
- 2026-11-12: three ways to tell steps apart and no more: a yellow name is
  not at hand, a faint row is done, a white bar is the next step.
- 2026-11-12: a plain book is not offered as an item, knowing that the
  anvil accepts it.
- 2026-11-13: the steps' side is closed; more polish is possible, none is
  asked for.
- 2026-11-13: slot marks are a tint alone at 50%, cyan for the stack that
  goes left and pink for the right; green means the pair is right, red
  wrong, yellow a differing cost.
- 2026-11-13: a stack carried on the pointer is not marked in the first
  version.
- 2026-11-13: the tints have a setting of their own, on by default, a
  sibling of the other two.

## Record

Moved unchanged from the L-170 item of BACKLOG.md on 2026-11-14, oldest
first. The item's old status line read "Kind: Design, then Research (strong
model). Requested by the maintainer 2026-10-12; not chosen for building, no
release." and was replaced there. "Built ..., not checked in game" in a
round means at the time of that round; later rounds and VALIDATION.md say
what was checked since.

Agent's proposal (not decided):
- Data from the game, so new enchantments need no Lamium update: the
  registry `Enchant::mEnchants()` gives each enchantment's id, max level
  (`getMaxLevel`), rarity (`Frequency`, which sets the anvil multiplier),
  compatibility (`isCompatibleWith`) and whether it can go on the item.
- Anvil rules (prior-work penalty doubling per use, per-level multipliers,
  books against items, the refusal threshold) as a pure, tested table.
  Bedrock's rules are not assumed to equal another edition's: check every
  rule against the cost the anvil screen shows (`AnvilScreenController`)
  before trusting the planner.
- Solver: a pure, bounded search over combination trees (exhaustive up to
  about 10 inputs, which covers every real item), minimizing total levels
  with no step at or above the refusal threshold; ties prefer fewer steps.
- UI: a Lamium screen; pick the item (held item preselected), tick the
  enchantments and levels (incompatible ones greyed), optionally "use the
  books I carry" (read from the inventory, with their existing prior-work
  penalty). The result is a numbered list of steps with each step's cost
  and the total.
- Later, if wanted: highlight the next pair in the anvil screen.
Decided (maintainer 2026-10-12): the first version plans from fresh books
only (no prior-work penalty on the inputs); planning from the books in the
inventory can come later.
Decided (maintainer 2026-10-12): an unbound hotkey (and the settings
screen) opens it with the held item preselected; anything inside the anvil
screen comes later.
Decided (maintainer 2026-11-12): the anvil highlight is wanted, as a
follow-up after the first version.
Decided (maintainer 2026-11-12): two goals, fewest levels and least prior
work on the result. The maintainer referred to the two choices the
enchant-order web tool offers (PROVENANCE.md); both narrow the orders in a
useful way. The planner has both (`Goal`, below).
Decided (maintainer 2026-11-12): merging two books of one enchantment into a
higher level stays out of the first version; more than 10 inputs is refused
and the screen says so.
Decided (maintainer 2026-11-12): a two-way switch above one step list,
fewest levels by default, with a line saying so when both goals give the
same plan.
Groundwork 2026-11-12 (pure logic and tests only; no screen, setting or
action): `src/features/inventory/EnchantPlan.h` plans the order for up to 10
inputs. The caller gives each input's cost as the sacrificed side and its
earlier anvil uses; the refusal threshold is a parameter. It returns the
steps with each cost, the total, the most expensive step and the result's
anvil uses. `tests/EnchantPlanTests.cpp` compares it with an exhaustive
search on 300 small sets. `Rules::goal` (2026-11-12) picks what is minimized
first: `FewestLevels` (total, then the most expensive step, then the result's
anvil uses) or `LeastPriorWork` (the result's anvil uses, then total, then
the most expensive step); the tests compare both with exhaustive searches.
Checked in game 2026-11-12 (VALIDATION-LOG "Anvil costs behind the enchantment
planner", vanilla anvil screen, local world):
- The prior-work surcharge is 2^uses - 1 for each side and the result has
  max(uses) + 1.
- A step's cost is the sacrificed side's enchantment levels times a
  per-enchantment multiplier, plus both surcharges, with nothing else added
  for undamaged, unrenamed inputs. The multiplier for a sacrificed item was
  twice the book's in both cases read (Knockback 1 and 2, Curse of Vanishing
  4 and 8); the screen should take multipliers from the game rather than a
  Lamium table, as proposed above.
- A step costing 40 or more is refused in Survival; Creative has no limit.
  `Rules::refuseAt = 40` matches; a Creative player needs 0.
Not checked: a damaged input, a rename, an enchantment already on the item or
in conflict with it, servers.
Data sources, read through `world_probe` 2026-11-12 (VALIDATION-LOG "World
probe: the enchantment registry"); all of these link on SDK 26.60.4:
- The list: `Enchant::mEnchants()`, 42 entries on the client, each with
  `mEnchantType`, `mStringId`, `mFrequency`, `mIsDisabled` and the virtual
  `getMaxLevel()` and `isCompatibleWith(type)`.
- The multiplier: no linkable function gives it (`ItemEnchants::getTotalValue`
  is `MCNAPI`). `mFrequency` 30, 10, 3, 1 matched 1, 2, 4, 8 per level for a
  sacrificed item and 1, 1, 2, 4 for a book in all nine costs read from the
  anvil. Use that mapping, a pure function with a test; a new enchantment then
  needs no Lamium update, a new frequency value does. It is a meaning a
  rebuild cannot check, so the screen asks `versionSensitiveAllowed()`.
- Whether an enchantment goes on the item: `EnchantUtils::canEnchant(stack,
  {type, level}, false)` answers `Enchant`, `Increment`, `Conflict` or `Fail`.
  It answered `Conflict` for one the item already had at that level.
- Display names: `EnchantUtils::getEnchantNameAndLevel(type, level)`.
- The item's earlier anvil uses: its repair cost, as recorded for L-319.
- Not tried: `EnchantUtils::combineEnchantedItems(first, second, out, book)`
  returns an int and links; it may give the game's own cost for a pair and
  would then replace the mapping. Try it before building on the mapping.
Mockup 2026-11-12: [demos/enchant-planner.html](demos/enchant-planner.html).
Decided after its first review (maintainer 2026-11-12):
- The item can be chosen; the screen opens on the held one. A `◀ ▶` stepper
  is turned down: it does not scale to the number of choices.
- The held item's enchantments are fixed rows and its anvil uses are charged.
- A step names an intermediate book by its contents, and items and books are
  drawn with their icons (`inspection::render::drawItemIcon`).
- Creative follows the current game mode and has no limit; no switch.
- Curses are listed last. The screen is called "エンチャント順序".
Decided after the second version (maintainer 2026-11-12):
- "Held" is the main hand, the selected hotbar slot; the hotbar and
  inventory are not searched.
- The item is chosen by kind (sword, pickaxe, ...) from an icon grid: what
  goes on an item follows its enchant slot, not its material.
- While the main-hand kind is chosen, a "use the held item's state" switch
  is shown; off plans for a fresh item of that kind.
- Sets: "everything at its highest" (enchantments with no conflict) and the
  last selection per kind. No Lamium-written or saved sets.
Decided (maintainer 2026-11-12, after the research below): the step list
beside the anvil screen is part of the first version.
The step list beside the anvil, researched 2026-11-12 (VALIDATION-LOG "World
probe: the anvil screen"); nothing was drawn yet:
- Drawing: the `AfterUIRenderEvent` of the container screen's `ScreenView`
  gives a render context; Lamium already draws there (container previews,
  locked trades).
- Which screen: `getScreenName()` is `anvil_screen`.
- Where: the window is the control `root_panel` (176x166 at 234,91 in a
  644x348 view), read from `mCachedPosition`/`mSize` once laid out, so the
  list can sit to its right; 234 units were free there.
- What is in the slots: `getItemStack("anvil_input_items", 0)` and
  `("anvil_material_items", 0)`, with `constructItemEnchantsFromUserData`
  for their enchantments and `getBaseRepairCost()` for their anvil uses.
- The game's own cost for the pair in the slots:
  `AnvilContainerManagerController::mCost` (a class-layout read behind
  `versionSensitiveAllowed()`), so the marked step can be checked against it
  and a difference shown rather than trusted.
- Input: the list takes no clicks or keys in the first version. A click
  outside the window drops the cursor's stack in vanilla, and keys go to the
  vanilla screen (the calculator dropped its container-screen step for that
  reason). The plan is changed from the planner screen.
- Still unknown: other GUI scales and narrow windows (the list needs a
  fallback when it does not fit: hide it), and whether Lamium's key opens the
  planner while the anvil is open.
Pure logic added 2026-11-12 in `EnchantPlan.h`, tested: `levelCost(frequency,
book)` (the mapping above; an unknown frequency gives nothing), `carried(...)`
(which planner inputs a stack in an anvil slot stands for) and `stepFor(...)`
(the step joining two sides).
The design is agreed and building was chosen (maintainer 2026-11-12).
Built 2026-11-12, not checked in game (VALIDATION.md): the screen inside the
settings (`src/ui/EnchantView.cpp`, layout in `EnchantLayout.h`), opened by an
unbound key or the Open button of "Enchantment order" under Inventory; the
game reads and the list beside the anvil in
`src/features/inventory/EnchantOrderGame.cpp`; the edits in `EnchantOrder.h`;
the capability `EnchantOrder` gates both. Differences from the mockup, for the
review in game:
- A kind is named by its example item in the game's words ("Diamond Sword"),
  not by a kind word: Lamium has no translated kind names and the game offers
  none.
- A row shows the enchantment with its level as the game writes it and has
  `-` `+` buttons, instead of a separate level between the buttons.
- The plan and the last choice per kind last for the session; nothing is
  saved to the settings file.
- The list beside the anvil also says when the pair in the slots is not a
  step, and warns when the game's own cost differs from the plan's.
- README and FEATURES.md wait for the review below.
First use by the maintainer 2026-11-12 (`0d2b5ecd`, VALIDATION-LOG): the four
checks passed. The maintainer's notes from using it, grouped; none is decided:
- Plain defects: the conflict reason carries a level ("ダメージ増加 I と競合")
  where the level means nothing; "All highest" turns curses on; it also
  enables "Last choice" when it changed nothing; the set buttons move down
  when the held-state row appears; on boots Feather Falling sits between the
  four protections that exclude each other.
- Reading: curses are not red as in the game; the kind icons are too small
  to choose from; a kind shows an item's name ("Diamond Sword") and reads as
  if the plan were for that item only, a kind word is wanted; where one step
  ends and the next begins is hard to see; the totals sit at the bottom, in
  the screen and beside the anvil, where they are not seen.
- Held state: fixed rows look dead (a press does nothing); the maintainer
  suggests weakening it, for example leaving the held state when such an
  enchantment is chosen.
- Sets: "All highest" could choose the usual one of a conflicting group
  (Protection on armor, Sharpness on a sword); the maintainer does not know
  the usual one for every kind (bow, mace).
- Items missing from the kinds: a book (enchanted books merged into one) and
  a carved pumpkin.
- Beside the anvil: nothing says what is wrong with a pair, which items the
  next step needs or how many levels the player has; the plan cannot be
  changed there; an item's work penalty is not shown anywhere (L-319).
- Outside this item: English Search does not find enchanted books by an
  enchantment's English name ("protection", "knockback") in the Creative
  search field.
Decided (maintainer 2026-11-12, after the grouped proposal) and built the
same day, not checked in game:
- The plain defects are fixed: the conflict reason is the name alone (what
  the game's texts for level I and V share); "All highest" leaves curses out
  and records nothing when it changes nothing; the held-state row is always
  there (grey without a main-hand item of the kind); enchantments that
  exclude each other are listed together (`grouped`).
- Reading: curses are drawn in the game's red (`palette::curse`); kind cells
  are 20 units with a full-size icon; a line separates the steps; the totals
  sit under the goal in the screen and on the first line beside the anvil.
- Kind names come from the game, never from Lamium: its creative group name
  where it has one (`itemGroup.name.sword` and the like: swords, pickaxes,
  axes, shovels, hoes, the four armor pieces, spears, skulls, enchanted
  books), else the item's own name (Bow, Trident, Mace...). The maintainer
  asked for vanilla wording in every language, Chinese included.
- Held state: pressing an enchantment the item carries leaves the held
  state; what it carries becomes wanted at the same level (`release`), and
  the footer says so.
- A level button below level 1 turns the enchantment off (the maintainer
  had to travel to the switch to drop Knockback after "All highest").
- "All highest" takes one of a group that excludes each other by the game's
  data alone: the highest frequency weight, then the one that goes on the
  fewest kinds of item, then the first (`allHighest`). No Lamium table.
- New kinds: enchanted books merged into one (a plan without an item) and
  the carved pumpkin; a skull stands in if the pumpkin's slot is missing.
- Beside the anvil: the total and the player's level on top (the level in
  the warning color when the next step costs more; not in Creative), done
  steps greyed (a stack at hand holds everything the step joins), the next
  step marked, a line saying which side of the pair is not in the plan or
  what the next step still lacks, and the prior-work surcharge of the two
  slot items.
Second use by the maintainer 2026-11-12 (`9a1bec30`, VALIDATION-LOG): the four
checks passed. Fixed the same day, not checked in game:
- The anvil list kept saying "finished" with no way out. A stack holding
  every input counts as finished, so another item with the same
  enchantments does too. Now a finished plan leaves the anvil's side once
  the anvil is closed (the planner brings it back), and a finished item is
  ignored while another item of the plan that is not finished is at hand
  (`progress`): holding a fresh sword means a second one is being made.
- The main-hand mark is a line under the kind's cell, clear of the icon.
- The enchantments keep one order whether the held state is used or not;
  what the item carries is fixed in its place.
- The held-state row sits under the list and shows only for the main hand's
  kind, so nothing above it moves.
- More space under the totals.
Third pass by the maintainer 2026-11-12 (`cfd22b5e`, VALIDATION-LOG): passed;
the lasting "finished" had been a stone pickaxe with the same enchantments.
Decided the same day and built, not checked in game:
- The held state is a cell, not a switch: the main-hand item itself (its own
  icon) stands left of the kinds, apart from them. Choosing it plans from
  its state; choosing a kind plans a fresh item, and choosing the held
  item's own kind keeps what it carries as wanted. The white mark and the
  switch row are gone. The cell appears only when the item's anvil uses
  could be read.
- An enchantment row reads name, switch, level buttons, as the settings
  table orders a row; "on the item" and the conflict reason stand where the
  switch would.
- The list beside the anvil has a switch, "List the steps beside the anvil"
  under Enchantment order (`inventory.enchantOrderAnvil`, on by default): it
  is the only part that shows without being asked. The planner itself has
  no switch.
Fourth pass by the maintainer 2026-11-12 (`b87a65eb`, VALIDATION-LOG): passed.
Decided the same day and built, not checked in game:
- A press on an enchantment the item carries does nothing but explain itself
  in the footer: fresh and main hand are chosen by their cells now.
- The main-hand cell is the last cell of the grid, after the enchanted
  book's, with a lighter frame; its place is kept free when nothing is held,
  so the grid never moves.
- An enchanted book in the main hand gets the cell too (it had been left
  out): the plan then merges further books into it, charging its anvil uses;
  it may end up on either side of a step.
Fifth pass by the maintainer 2026-11-12 (`06a753ef`, VALIDATION-LOG): passed,
"All highest" on a mace taking Density included. Decided the same day and
built, not checked in game: an enchantment row reads name, then the level
as a stepper (`-` level `+`, as Lamium's number rows), then the switch at
the right edge. The name no longer carries the level; a row that is off
shows the level it would turn on at, faint and without buttons; a row with
one level has the switch alone. This replaces "name, switch, level buttons"
of the fourth pass, which left the level far from its buttons and an empty
stretch right of the switch.
Sixth pass by the maintainer 2026-11-12 (`2af7b12e`, VALIDATION-LOG): passed.
A row on the item now writes its level in the level column like the others
(built, not checked in game).
Raising a carried enchantment, read and built 2026-11-12 (not checked in
game): the anvil charges the difference in levels (VALIDATION-LOG "Anvil
cost of raising a level"), so a book's value has a second form for "the
kept side holds the item" (`Input::onItem`). A row on the item has level
buttons from its level up; a raised level counts as one of the nine books.
Not for a held enchanted book: it may be the sacrificed side, which was not
read. "All highest" raises what the item carries too. The row's "on the
item" tag is gone (it pushed the level out of its column in Japanese and
English); the row has a dimmed switch that stays on, and the footer says
why. The list beside the anvil is drawn before the anvil screen instead of
after it, so the game's tooltips stay on top.
Eighth pass by the maintainer 2026-11-12 (`c884f267`, VALIDATION-LOG): passed,
but the list drawn before the screen is under the screen's dark layer.
Changed the same day (not checked in game): the list is drawn after the
screen again, at full brightness, and is left out for a frame in which the
game's item tooltip overlaps it (the tooltip's box is read where Lamium
already hooks its drawing, `HoverRenderer::mCursorPosition`, `mOffset` and
`mBoxDimensions`). The first three boxes are logged to `lamium.log` so the
meaning of those members can be checked against a screenshot.
Ninth pass by the maintainer 2026-11-12 (`4800022a`, VALIDATION-LOG): passed;
the logged boxes agree with the reading of the tooltip's members, and the
log lines were removed afterwards. README and FEATURES.md describe the
feature from here on.
Asked by the maintainer 2026-11-12: the list above the screen's dark layer
and under the tooltip, always shown, as the ideal. Built the same day as a
trial (not checked in game): Lamium already hooks the game's tooltip
drawing, which runs inside the screen's own drawing, so the list is painted
there right before the tooltip, from the state worked out the frame before;
a frame without a tooltip paints it after the screen as before. The
step-aside rule and the tooltip box read are gone. To see in game: the list
at full brightness with a tooltip over it, no flicker when the pointer
crosses items, and the list's item icons not showing through the tooltip.
Tenth pass by the maintainer 2026-11-12 (`e8d1d318`, VALIDATION-LOG): passed;
the maintainer agrees with trying choice (a) below for editing. Two things
seen and changed the same day (not checked in game):
- Closing: the list now moves with the window by however far the window is
  from where it rests, without being held at the screen's edge, so it
  slides off with it. It does not fade; Lamium's text has no opacity.
- An item dragged over the list was under it. The list is now painted
  before the first slot of the anvil screen draws in a frame (a new hook on
  `InventoryItemRenderer::$render`, `EnchantListBeforeSlots`), so slots, the
  dragged item and the tooltip all come after it. The tooltip hook and the
  after-screen paint remain as fallbacks for a frame in which no slot draws.
Eleventh pass by the maintainer 2026-11-12 (`768db9be`, VALIDATION-LOG):
failed. Painting from the slot renderer's hook left the list and every
enchanted item white: the game is mid-way through its item drawing there.
The hook is removed (do not paint from `InventoryItemRenderer::$render`
again). Instead, built the same day and not checked in game:
- While an item is held on the pointer and the pointer is over the list,
  the list is not painted, so the item is seen whole. Painting the held
  item again on top of the list was the alternative; it needs the game's
  exact place and size for it.
- Closing: the game's window fades as it slides. The list now takes an
  opacity from how far the window has moved from rest (gone at half the way
  to the screen's bottom) or the window control's own `mAlpha` when that is
  lower; `ui::DrawOpacity` applies it to fills, text and the icons.
Twelfth pass by the maintainer 2026-11-12 (`9ff54047`, VALIDATION-LOG): passed,
but the list giving way to a held item flickers and is not wanted. Decided:
the list is always drawn; the held item on top of it if that can be done,
under it otherwise. Built the same day (not checked in game): the give-way
rule is gone, so the held item is under the list again. To put it on top
Lamium would draw the held item once more over the list, which needs the
game's exact box for it; the build logs, three times per run, the small
controls under the pointer while an item is held at the anvil, to find that
control. Remove the log lines once it is known.
The held item's control was found from the log (VALIDATION-LOG "The item held
on the pointer at the anvil"): `selected_item_icon`, 21 units, following the
pointer. Built the same day (not checked in game): while that box overlaps
the list, Lamium draws the held stack once more in the same box after the
list, with its glint, so it is seen on top. Its count and durability bar
are not drawn again, so over the list a stack shows no number. The log
lines are removed.
Thirteenth pass by the maintainer 2026-11-12 (`8d62d240`, VALIDATION-LOG): the
held item is on top without doubling, but without its glint and with its
durability bar under the list. Built the same day (not checked in game):
the held item is drawn in the same call as the list's own icons, last, as a
glint does not show after another batch of icons; its durability bar is
drawn over it with the slot bar's geometry at the held size. The count of a
held stack is still not drawn over the list.
Fourteenth pass by the maintainer 2026-11-12 (`6fa5445b`, VALIDATION-LOG): the
bar is right; the held item's glint is lost over a long list and kept over
a short one. The maintainer asked whether the container previews' drawing
applies: it is the same icon call (`drawItemIcons`), and there every
glinting icon is followed by its own glint. The list's books were glinting
icons drawn without one. Built the same day (not checked in game): the
list's icons are drawn with their glint as in the previews, and a held
stack's count is drawn while the whole item is over the list (not while it
straddles the edge, where a second number slightly off the game's would
show double).
Fifteenth pass by the maintainer 2026-11-12 (`0855cb5b`, VALIDATION-LOG): glints
work with one step and fail with two or more, for the list's books and the
held item alike, while a preview of 30 enchanted books is fine. The
difference found in code: the list drew one book stack many times, the
preview draws a stack of its own per slot. Also seen: a held block or
glinting item went under the list's swords (those were drawn later, in a
batch), and a held stack's count went under the item at the list's edge.
Built the same day (not checked in game):
- Every icon gets a stack of its own (a pool per kind of icon), in the
  anvil list and in the planner screen.
- The anvil list draws all its icons one after another without batches
  (`IconAt::direct`), the held item last, so it is above every icon of the
  list whatever it is.
- The planner screen collects its icons and draws them in one call at the
  end, with glints (its books had none).
- A held stack's count is drawn whenever the held item is drawn again, on
  top of it.
Sixteenth pass by the maintainer 2026-11-12 (`8d5083f4`, VALIDATION-LOG): the
held item is above every icon of the list; the glints are unchanged; the
held stack's count is a little right of and below the game's.
- Count, built the same day (not checked in game): its place is taken from
  the game's own count control under `inventory_selected_icon_button` (the
  first shown descendant with "count" in its name), and it is drawn like the
  container previews' counts, with the engine's shadow; the control found is
  logged once per run. Without such a control the count is not drawn.
- Glints, open. What is known: one glinting icon followed by the held item
  works; with two or more steps no icon glints, not even the first, whether
  the list's swords go in a batch after the glinting icons (`0855cb5b`) or
  every icon is drawn in order (`8d5083f4`), and whether the books share a
  stack or not. The planner screen never glints. The container preview
  glints with 30 enchanted books through the same `drawItemIcons`. Not yet
  compared: the icon scale (the list draws at 8 units, the planner at 10 and
  16, the preview at 16), the text flushed right before the icons, real
  enchanted stacks against bare `enchanted_book` stacks. Three blind rounds
  have not found it; the next step is a build that draws a row of test
  icons in those variants, not another guess. Same family as L-91 (glints
  outside vanilla slots).
Seventeenth pass by the maintainer 2026-11-12 (`8396fafb`, VALIDATION-LOG): no
count over the list; the count control was not found by its name. The
maintainer agreed to a test build for the glints. Built the same day: the
xmake option `enchant_glint_test` (off by default). With it the anvil screen
shows, instead of the list, ten icon arrangements in turn, four seconds
each, numbered on screen (size, spacing, text before the icons, bare or
really enchanted stacks, one call or several, the list's own arrangement),
to see which keep their glint; and while a stack of two or more is held
outside the window it logs every control near the pointer that is not part
of a slot, shown or not, to find the count's control. Remove the option's
code once both are answered.
Glint test round 1 by the maintainer 2026-11-12 (VALIDATION-LOG): none of the
ten arrangements glints, the container preview still does. So the icons are
not the cause (count, size, kind of stack, calls); what the ten shared and
the preview does not is a translucent fill and flushed text before the
icons. The log named the count's control, `selected_stack_size_text`; the
list now looks for that name (not checked in game). Round 2 of the option,
built the same day, eight arrangements, five seconds each, the number
written after the icons: the preview's own code with 27 enchanted books at
the pointer; icons with nothing before them; after an opaque fill; after a
translucent fill; after an opaque fill and flushed text; at 8 units; bare
books; one stack four times.
Glint test round 2 by the maintainer 2026-11-12 (VALIDATION-LOG): the
preview's code glints at the anvil too; icons with nothing under them glint
weakly; icons on a fill of the panel colour (0.06 grey) do not. So the
screen, the hook and the text are not the cause: what is under the icon is.
The preview puts a 0.23 grey square under every icon. Round 3, built the
same day, varies that: a grey or white fill under all four, grey squares on
an opaque or translucent panel, the preview's frame colour alone, squares
alone, squares at 8 units, and the list's own arrangement with squares.
Glint test round 3 by the maintainer 2026-11-12 (VALIDATION-LOG): only the
white fill glints; the preview's own slot grey does not. So it is not what
lies under the icon but the colour of the last fill before it: the preview
draws its purple border last, the tests drew grey or the panel colour last,
and the list's last fill depends on its rows (which fits one step working
and two failing). Built the same day (not checked in game), in
`drawItemIcons` for every caller: the current shader colour is set to the
preview's border colour around each glint overlay and put back. If that is
right, the list, the planner screen and the HUD icons glint like the
preview, and the preview is unchanged. The test option stays until this is
confirmed.
Eighteenth pass by the maintainer 2026-11-12 (`d96f2cbe`, VALIDATION-LOG): the
glints (list, held item over it, planner screen, preview unchanged) and the
held stack's count are confirmed. The `enchant_glint_test` option and the
count's log line are removed. Open, the maintainer's question: the list's
and the planner's item icon is a plain stack, so it has no glint even at a
step where the item would be enchanted.
Decided by the maintainer the same day: the item's icon glints at the steps
where the item carries an enchantment (from the first step if the held item
has one, otherwise from the step after its first book). Built (not checked
in game), in the anvil list and the planner's steps: those icons are drawn
from a copy of the item with one of the plan's enchantments on it.
Confirmed by the maintainer the same day (`02a8f09f`, nineteenth pass).
Changing the plan at the anvil, researched in code the same day (nothing
built):
- A click on the list can be kept from the game: `CustomInput`'s
  `MouseInputEvent` listener cancels events before vanilla sees them, which
  Item Transfer already does on container screens
  (`TransferSession::mouseButton` answering true). The event arrives outside
  the client tick, so the hit test must use the list's box cached from the
  last frame and the action must be queued to the client thread.
- The pointer in GUI units is `ScreenView::mPointerLocationPrevious`
  (container previews, locked trades), enough for hover and for the hit test.
- The planner cannot open while the anvil is open today: `ui::open` asks for
  a gameplay screen (`gameplayScreen(getScreenName())`). Whether Lamium's
  scene can be pushed over a container screen, and whether the anvil and
  the items in its slots are still there after it closes, is unknown.
- Choices, with the agent's recommendation (not decided): (a) a "Change"
  button on the list that opens the planner over the anvil and returns to
  it on Esc, if the game allows a scene over a container screen
  (recommended first, as a trial build, because it reuses the whole
  planner); (b) the same button closing the anvil first, if (a) loses the
  anvil; (c) small controls on the list itself (the goal, dropping a
  step's enchantment), which duplicates the planner in a narrow list.
Earlier note (maintainer asked 2026-11-12): raising an enchantment the held item
already carries (Sharpness III on it, V wanted). The agent's view: this is
not the left-out "two equal books into a higher level"; that one changes
what the inputs are. Raising with one higher-level book keeps the inputs
and only changes what that book costs when it reaches the item, so it is a
small extension of the cost model (`Input` gets a second value for "the kept
side holds the item"). It needs one rule read from the anvil first: whether
the step is charged the book's full level or the difference from the level
on the item. Three readings settle it, each a fresh item with the lower
level from `/enchant` and a fresh higher book: Sharpness III + V (5 or 2),
Knockback I + II (2 or 1), Looting I + III (6 or 4).
Noted for later, outside L-170: the world map is experimental and has no way
to turn it off either; whether a screen that only opens on request needs a
switch is one question for both.
Trial of choice (a), built 2026-11-12 on the maintainer's go-ahead (not
checked in game): the anvil list has a "Change" button in a row of its own
at the bottom right. A left press on it, with nothing on the pointer, is
kept from the game (`CustomInput`'s mouse listener asks
`enchantOrder::game::listPress`, which answers from the button's box and
pointer of the last frame) and opens the planner over the anvil screen on
the client thread (`ui::openEnchantOrder(client, true)`, the one case
`ui::open` accepts `anvil_screen`). To find out in game: whether the scene
opens over the anvil at all, whether Esc returns to the anvil with the
items still in its slots, and what the planner chooses while the item is
in a slot and not in the main hand. If the anvil is lost, choice (b).
Twentieth pass by the maintainer 2026-11-12 (`998e845e`, VALIDATION-LOG): the
trial works. The scene opens over the anvil (whose window is not drawn
meanwhile), Esc returns to the anvil with the slots' items in place and the
new plan in the list; the anvil plays its opening animation on return.
Raised by the maintainer, not decided:
- Opening the planner takes the main hand's item and so drops the previous
  plan silently; from the anvil it took whatever the main hand held, not
  the item in the slot. Either the previous choice is kept while the
  main-hand cell is only shown, or a setting turns the main hand off.
- Without a plan the anvil shows no list and no button. The maintainer
  would rather do the planner's work on the anvil screen itself than open
  Lamium's screen each time, accepts that it is more work, and is willing
  to go through a mockup again.
Decided by the maintainer 2026-11-12: the previous plan wins. An unfinished
plan opens as it was left; the main hand's cell is shown and switches only
when pressed; without a plan the main hand is chosen as before; no new
setting. And the anvil-screen planner goes through a mockup first.
Built the same day (not checked in game): on opening, a plan that is still
active keeps its kind, its enchantments and whether it was planned from a
held item. If it was and that item is not in the main hand now (same kind,
same enchantments, same anvil uses), the main-hand cell stands for the
plan's item, rebuilt from the plan's record, so the plan is the same with
the item lying in the anvil's slot. Known gap: in that case another
enchantable item in the main hand is not offered until the plan is
finished or changed to a fresh one.
Mockup made the same day, not yet reviewed:
[demos/enchant-anvil.html](demos/enchant-anvil.html). It proposes a
one-row stub with "Plan" when there is no plan, "Edit" on the step list,
the item from the anvil's left slot (else the kind grid), and two
placements: A, the enchantments left of the window and the steps right;
B, one panel on the right with two tabs. Its five questions are at the
bottom of the page; nothing is to be built before they are answered.
Answered by the maintainer the same day; the mockup's second version shows
it:
- Placement A: the chooser left of the anvil window, the steps right.
- The chooser is always shown, with no "Plan" or "Edit" button; a press
  each time is a chore. Those who do not want the left side filled get a
  setting, not a button. With the setting off, or without room on the
  left, the step list keeps a "Change" button that opens Lamium's screen,
  shown without a plan too.
- The item is not taken from the left slot by itself: treating the main
  hand specially made the planner screen complicated before and ended as a
  cell after the kinds. The same here: the kind grid, with item cells
  added last.
- Lamium's screen stays (several interfaces for one feature are usual
  here: waypoints, shapes, schematics; and it plans without an anvil).
- Everything Lamium attaches to the anvil screen closes with it.
Decided by the maintainer the same day: both item cells. With two warnings
before building: a mark in a cell's corner sits on the item's picture and
was rejected for the planner screen, which sets its item cell apart by its
fill and frame; and the planner keeps the level in one place whatever the
row's state (on the item, on, off), which the mockup's "on the item" and
"highest" texts did not. The anvil's chooser is to share the planner
screen's look and handling wherever the anvil screen does not force a
difference.
Built the same day (not checked in game):
- One chooser for both places (`ui/EnchantChooser`, laid out by
  `EnchantLayout::layColumn`): the item's line, the kind cells, the item
  cells, the set buttons, the rows with the level stepper and the switch,
  drawn and pressed by the same code in Lamium's screen and left of the
  anvil. The anvil's panel (`EnchantLayout::fitPanel`) adds two status
  lines under the rows, where the screen has its footer.
- Item cells: the main hand's, then the anvil's left slot's (anvil only),
  both with the lighter fill and frame; their places are kept when empty.
  The plan's item is now a record taken when a cell is chosen (kind,
  enchantments, anvil uses, name), not the live item, so the plan stays
  the same when the item moves between hand, slot and pointer. A cell is
  marked chosen while the item in it matches that record. This replaces
  the earlier "keep the previous plan" patch (`f80ad88f`) and removes its
  known gap.
- At the anvil the chooser is always shown when the new setting "Choose
  enchantments beside the anvil" is on (default) and there is room on both
  sides; the steps go right. Without a plan the right panel says so. With
  the setting off or no room: steps only, with the "Change" button, shown
  without a plan too.
- With nothing chosen and no plan, an item put in the left slot is taken
  as the plan's item; otherwise the slot only offers its cell.
- Presses and wheel turns over the panels are kept from the game by the
  mouse listener (values of the last frame) and acted on with the next
  frame on the client thread. The goal is switched by pressing its name on
  the step list. Nothing is taken while an item is on the pointer.
- A finished plan is now cleared when the anvil closes, its enchantments
  going to "Last choice"; before, the planner brought it back.
- Not built: dragging the panel's scrollbar (a press on it jumps), text
  search.
Twenty-first pass by the maintainer 2026-11-12 (`47dfc78e`, VALIDATION-LOG):
the above works, at UI sizes 50% to 100%. Fixed the same day (not checked
in game):
- An item finished earlier, carrying some of the new plan's enchantments,
  was counted as the plan's item part-way and the list started in the
  middle. Only the plan's inputs and its steps' results count now
  (`progress`).
- The main-hand cell follows the main hand while the anvil is open, like
  the slot's cell.
- The anvil's panels get the thin outline Lamium's screen has, and the
  choosing panel is no taller than its rows need.
Twenty-second pass by the maintainer 2026-11-12 (`088f8120`, VALIDATION-LOG):
the three fixes work. Changed the same day (not checked in game): the
choosing panel keeps one height again (growing and shrinking with the rows
was restless); of two like items in the hand and the slot only the pressed
cell is shown chosen (the plan's record says which cell it came from, and
the other cell takes over when the item has moved there); a plain book is
no longer offered as an item (its enchant slot value is the enchanted
book's "everything").
Raised by the maintainer and not decided:
- The right panel (the steps) and the planner screen's right column differ
  and neither is clearly better: the anvil's is simple but says little,
  the planner's says more but its goal row is not obvious. They should be
  brought together, like the left side.
- What is missing for the whole plan cannot be seen at a glance; the list
  only names what the next step lacks, when it comes. The maintainer
  suggests doing this together with the related items set aside earlier
  (what the player has, the next step), as one feature.
The maintainer the same day: the related things are the item's work penalty
and the highlight of what the next step uses; rather than holding to what
the first version left out, things that belong together should be thought
through together, even if they are built later.
Mockup made the same day, not yet reviewed:
[demos/enchant-steps.html](demos/enchant-steps.html). One steps' side for
both screens: goal (two buttons or the stepper), total, a materials list
with what is not at hand marked, the steps with each side coloured (at
hand, made earlier, missing) and done / next / ready told apart, the
surcharge under a step's cost; the next step's two stacks outlined in the
anvil's inventory; the work penalty in an item's tooltip (L-319). Its six
questions are at the bottom of the page.
Twenty-third pass by the maintainer 2026-11-12 (`e85009f7`, VALIDATION-LOG):
one chosen cell and the fixed height are confirmed. The plain book is not:
the anvil does enchant a plain book from an enchanted book, in Survival and
Creative, so leaving it out is not "what the game does". Open: offer it or
not. Known if it is offered: the list cannot tell the plan's plain book
from the fresh enchanted books once it has been through the anvil (a held
book is told by what it carried, and a plain one carried nothing).
The steps' side, the maintainer's answers to the mockup the same day; no
building before the open points are settled:
- Decided: one component for both screens. The goal stays the stepper
  (arrows): it is what Lamium's settings use, and two buttons make the
  pointer travel. The next step's stacks are to be highlighted if it can
  be drawn, told apart by which slot each goes in (left or right). The
  work penalty in the tooltip is L-319's. Order: the shared steps' side
  first, then the highlight trial, then L-319.
- Open: the materials list. As drawn it repeats what the steps' colours
  say and takes room at the top. Either the steps carry the same
  information, or the list goes below the steps. Concerns with each: in
  the steps, a side made by an earlier step does not show which of its
  own materials is missing; below the steps, the list is far from the
  total, which is also "what is needed".
Agreed by the maintainer the same day and shown in the mockup's second
version: no materials list. The total's row carries a count ("materials
4/6", yellow when short); in the steps each name is yellow when that book
or item is neither at hand nor inside a merged stack, and a side is bright
only when that very stack is at hand (dim icon and grey text otherwise);
the last line names what the whole plan lacks when no step can be taken.
The highlight uses two colours, one for the stack that goes left and one
for the right, with the same outlines on the anvil's two slots.
What the maintainer had found hard to read was not the goal but the
planner screen's line "Dearest step / Anvil uses on the result": useful,
but not understood at first sight, and not shown at the anvil at all. The
mockup proposes one line for both screens in plainer words ("needed at
once: N levels", "the finished item's work penalty: +N"), each explained
in the status line on hover; the present words and no line can be compared
on the page.
Decided by the maintainer the same day (the mockup's third version):
- The plainer words. Your level goes beside the total, not beside "needed
  at once". The two items under it break onto a line each where one line
  does not hold them.
- The coloured names stay. The other rules of the second version (a side
  bright only when that very stack is at hand, a green row for every step
  that can be taken) were too much: the maintainer was reading the
  explanation instead of the list. Three ways to tell steps apart and no
  more: a yellow name is not at hand, a faint row is done, a white bar is
  the next step. Whether the next step can be taken is said by the last
  line and by the inventory outlines. The row of the pair lying in the
  slots stays highlighted as today.
- A plain book is not offered as an item, knowing that the anvil accepts
  it. That this is a choice and not ignorance of the game is recorded here
  and in FEATURES.md; the maintainer is not sure the game itself needs to
  say it.
Built 2026-11-12 on the maintainer's go-ahead (not checked in game), step
one of the agreed order:
- One steps' side for both places (`ui/EnchantSteps`, laid out by
  `EnchantStepsLayout`): the goal's stepper, the total with the player's
  level beside it and the materials count at the right, the two details
  (side by side, or a line each where they do not fit), the steps as two
  list rows each with a scrollbar, and the last line. The planner screen's
  right column and the anvil's right panel are this one drawing; the
  anvil's old compact list is gone.
- Where the player stands (`enchantOrder::follow`, pure): done steps, the
  next one and whether its two sides are at hand, and which inputs are
  held at all. The planner screen reads the player's inventory every
  frame; the anvil reads its slots, the pointer and the inventory as
  before.
- Three ways to tell steps apart: a name in the warning colour is not at
  hand, a done step is faint, a white bar marks the next. The step whose
  pair lies in the anvil's slots keeps its tint.
- The last line: the pair in the slots first (match, the game's cost
  differing, an item not of the plan), else the next step when it can be
  taken, else everything the plan still lacks.
- The other goal's outcome and the two details' explanations are said in
  the status line when the pointer is on them (the planner's footer; at
  the anvil the choosing panel's status lines). The planner's third
  summary line and the anvil's "your level" and "prior work: left, right"
  lines are gone; their texts are removed.
- At the anvil the goal's stepper takes presses while the choosing panel
  is open; the wheel and the scrollbar scroll the steps.
Twenty-fourth pass by the maintainer 2026-11-13 (`12092660`, VALIDATION-LOG):
the shared side works. Five things were not right; changed the same day
(not checked in game):
- A dividing line under the goal, as above the steps.
- The last line wraps onto up to three lines, and the panel grows by
  them, so what is missing is read whole. A side's names that are cut in
  a step show whole in a box by the row while the pointer is on it, the
  way the settings table shows a value that is cut.
- At the anvil the steps' panel starts level with the choosing panel's
  top (with the window's top when there is no choosing panel).
- The choosing panel's status takes three lines and stays inside it.
- The goal is the stepper box the lists use (`drawStepperBox`, taken out
  of `drawShapeStepper`); where it cannot be switched it is plain text.
Twenty-fifth pass by the maintainer 2026-11-13 (`582fb68f`, VALIDATION-LOG):
the five changes work; three rough spots, changed the same day (not
checked in game):
- The total, the player's level and the materials count run on as one
  line of text, as in the mockup.
- A step's prior work is short ("incl. 2") at the right of its second
  row, so the names have the room; what it means is said in the status
  line while the pointer is on the step.
- Without a plan the anvil's right panel is the same side as with one:
  the goal (switched there too), the dividers, the steps' heading and the
  lines saying what to do, in the room of two steps. The bare stub is
  gone.
Twenty-sixth pass by the maintainer 2026-11-13 (`6362a66a`, VALIDATION-LOG):
confirmed. The steps' side is closed here on the maintainer's word (more
polish is possible, none is asked for now).
Highlight trial, built 2026-11-13 (not checked in game). While the next
step's two stacks are both at hand and nothing is on the pointer, the slot
holding the stack that goes left gets a two-unit outline in the accent
colour and the one that goes right in white; the anvil's left and right
slot get the same outline while they are empty. A side already lying in
its anvil slot is not outlined. How it finds the places: the stacks come
from the `hotbar_items` and `inventory_items` collections apart (before:
the combined one), a slot's box from the `hotbar_grid` / `inventory_grid`
control's corner and the slot's index (nine columns of 18 units, pure
`gridSlot`), the anvil's two slots as the first two slot-sized square
controls under `recipe_grid`, left to right; their names are logged once
a run for the record. Drawn with the panels, after the screen or right
before the game's tooltip, never from the slot renderer. To find out in
game: whether the outlines sit on the right slots, in the hotbar and the
inventory, and on the anvil's two slots; whether they sit above the items
and under the tooltip; whether the hotbar's first slot is index 0. No
setting yet: it follows "List the steps beside the anvil".
Twenty-seventh pass by the maintainer 2026-11-13 (`8774eedf`, VALIDATION-LOG):
the trial draws in the right places, above the items and under the
tooltip, without breaking anything. The anvil's slots are named
`input_item_slot`, `material_item_slot`, `result_item_slot`; the code asks
for them by name now and the log line is gone. As a design it is not
there yet: the outline covers the item, white does not show on the light
inventory, and the anvil's own slots say nothing about whether what was
placed is right. Open: how the marks should look and what the anvil's
three slots should say.
Agreed in words with the maintainer 2026-11-13, to be compared in a trial
build rather than a mockup (it is about how marks look on the game's own
pictures and light inventory). Limits first: Lamium draws after the
screen or right before the tooltip, so always over the items; a colour
under an item would need the slot renderer, which broke the game's
drawing before. Built the same day (not checked in game):
- The anvil's three slots speak (pure `anvilSay`). Left and right: empty
  and their stack at hand, in a slot or on the pointer - the side's
  colour; holding the right stack - the same colour, stronger; holding
  anything else - the curses' red. The result slot: the pair makes a step
  at the plan's cost - green; at another cost - yellow; no step - red.
  The step they are held against is the one the placed pair makes, else
  the next step.
- In the inventory the stacks that step still wants are marked one by
  one (before: only when both were at hand), while nothing is on the
  pointer.
- Colours: left the accent green, right the "experimental" purple.
- The look of a mark, default: a one-unit frame on the slot's rim with a
  faint tint. The xmake option `enchant_mark_test` (off by default) shows
  five looks in turn, six seconds each, named on screen: thin frame;
  thin frame and a faint tint; tint alone; corners; thin frame and tint
  with the right side in yellow. Remove the option once one is chosen.
Round 1 of the looks by the maintainer 2026-11-13 (`c4c910dd` with the
option, VALIDATION-LOG): the frame with a tint and the tint alone show
best, to be tried stronger; yellow is out for the right side; what the
anvil's slots say works. Raised: green for "left" is also the colour of a
right pair - would the shapes' cyan and pink do, or are they hard to see;
could the game's own green slot background under the pointer be used;
and could a stack carried on the pointer be marked, as the panels redraw
a held item.
Round 2, built the same day (not checked in game): six looks, six
seconds each - frame with a 30% and a 45% tint, tint alone at 45% and
60%, in green and purple; frame with 30% and tint alone at 45% in the
shapes' cyan and pink. A stack on the pointer that the step wants is
marked on the box the game draws it in, in its side's colour.
Not built: the game's own hover tint. It belongs to the slot's own
drawing; turning it on for other slots would mean writing the game's
controls, and it has one colour. Only a look at how the slot draws it
(a probe) would say more.
Round 2 by the maintainer 2026-11-13 (`aa5c70a8` with the option,
VALIDATION-LOG). Decided: the tint alone, at 50% (45% looked a little
faint; the colour has to be seen, the item under it only recognised, and
if the system is right the item need not be clear), in the shapes' cyan
(left) and pink (right). That leaves green to "this pair is right", red
to "wrong" and yellow to "the cost differs". The mark on a carried stack
is left out of the first version: it flickered in front of and behind
the item and did not fit the size an item has while held. Built the same
day (not checked in game); the `enchant_mark_test` option and its looks
are removed. No setting of its own yet: the marks follow "List the steps
beside the anvil".
Thirtieth pass by the maintainer 2026-11-13 (`41179be1`, VALIDATION-LOG):
the tints and what the anvil's slots say are confirmed, except the result
slot's yellow, which could not be brought about. Changed the same day
(not checked in game):
- While one of the two stacks is carried, the other one keeps its mark
  in the inventory (before, every inventory mark went with an item on the
  pointer).
- A setting of their own, "Tint the slots to use next", on by default,
  beside the other two under Enchantment order (the maintainer: it is a
  change of looks, tastes differ). It is a sibling, not a setting under a
  setting; like the choosing panel it only works while the steps are
  listed beside the anvil, and its help says so.
Confirmed by the maintainer 2026-11-13 (`8a3f8cca`, VALIDATION-LOG): the
other stack keeps its tint while one is carried, and the tints' setting
turns them off. L-319's first part is built and confirmed; see there.
Left of L-170: the result slot's yellow (not brought about), Survival
(the 40-level refusal, the level's warning colour), the unbound key,
servers, the unverified-version fallback, and the mark on a carried stack
that the first version leaves out.
Reported by the maintainer 2026-11-13, built and confirmed the same day
(`26ebab3a`, VALIDATION-LOG): with something else than its own stack in an anvil slot, the
marks other than the red "wrong" went away - the inventory mark of the
stack that slot wants, and the empty other slot's tint when its stack was
the one lying in the wrong slot. The inventory search took any filled slot
as "its side is placed", and a stack lying in the other anvil slot did not
count as at hand. Now a slot counts as placed only when it holds its own
side, and the anvil's two slots count as at hand. The red stays as it was;
no new mark or colour.
- An item's work penalty in its tooltip is L-319's first line; not built
  here, it needs a setting and its place in the tooltip chosen.
