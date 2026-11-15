# Enchantment order

Current contract, rewritten 2026-11-14; earlier records: `git show 10d58d01:docs/ENCHANT-ORDER.md`.

Plans the order of anvil steps that joins an item and the wanted books for
the fewest levels (or least prior work) and guides the player through it
beside the anvil screen (L-170, Experimental). Player's description:
[FEATURES.md](FEATURES.md); game members: [GAME-UPDATES.md](GAME-UPDATES.md).

Settings, under Inventory > "Enchantment order" (Experimental; the row has
an Open button and no switch of its own, the planner only opens on request):

| Row | Key | Default |
|---|---|---|
| List the steps beside the anvil | `inventory.enchantOrderAnvil` | off (a saved on stays on) |
| Choose enchantments beside the anvil | `inventory.enchantOrderAnvilChoose` | on |
| Tint the slots to use next | `inventory.enchantOrderAnvilMarks` | on |

The last two are siblings of the first, not settings under it, but only
work while it is on; their help says so. With it off the anvil screen is
the game's own and the planner still works. Action `openenchantorder`
("Lamium: Open the enchantment order planner"), no default key.

## Behavior

### Gate, data and lifetime

- Everything asks `versionSensitiveAllowed(EnchantOrder)`: the cost classes
  and the anvil's own cost are meanings a rebuild cannot check. On an
  unverified game version the planner does not open ("Not checked on this
  game version, so no order is planned.") and nothing is drawn beside the
  anvil.
- The planner needs a world and reads the game's enchantments each time it
  opens; if that fails it says so and stays closed.
- Nothing of the plan is saved. The chosen kind, the wanted enchantments,
  the goal, the plan and the last choice per kind are kept in memory. The
  stacks read from the game are dropped when the planner closes and on
  world exit; the plan's values are cleared only when the feature stops
  (not on world exit; open in L-170).

### What is planned

- All data comes from the game: the enchantment registry (type, highest
  level, frequency, disabled, incompatible pairs), what goes on an item
  (`EnchantUtils::canEnchant` on one example item per enchant slot), names
  from the game's texts. Lamium has no table of enchantments.
- A kind is an enchant slot, not a material: sword, pickaxe, axe, shovel,
  hoe, bow, crossbow, trident, mace, spear, the four armor pieces, fishing
  rod, elytra, shield, shears, flint and steel, brush, the two "on a stick"
  items, compass, carved pumpkin, skull, and enchanted books merged into one
  (a plan without an item). A candidate the game lacks, one that takes no
  enchantment, or one whose slot is already listed is left out. A kind is
  named by the game's creative group name (`itemGroup.name.*`) where it has
  one, else by the example item's name; never by a Lamium string.
- Inputs: the item (input 0, always the kept side) and one fresh book per
  wanted enchantment; at most 9 books, 10 inputs.
- A step costs the sacrificed side's enchantment levels times the book rate
  of each, plus the prior-work surcharge of both sides (2^uses - 1 each);
  the result has max(uses) + 1. Book rate per level from the frequency: 30
  and 10 give 1, 3 gives 2, 1 gives 4. Another frequency gives no plan ("An
  enchantment here has a cost class Lamium does not know").
- Books are always fresh and never merged into a higher level. Damage,
  repair and renaming are not part of the cost.
- Outside Creative an order with a step of 40 levels or more is left out;
  if every order has one, the screen says so. Creative has no limit. The
  game mode is read when the planner opens.
- Goals. Fewest levels (default): lowest total, then lowest dearest step,
  then fewest anvil uses on the result. Least prior work: fewest anvil uses
  on the result, then lowest total, then lowest dearest step. Both are
  always worked out so the other one's outcome can be told.
- Planning from an item as it is (an item cell): its enchantments are fixed
  and its earlier anvil uses (its repair cost) are charged. A level it
  carries can be raised with one higher book, charged the difference in
  levels when it reaches the item; it counts as one of the nine books.
- An enchanted book as that item is input 0 too, but may end up the
  sacrificed side, at the book rate of what it carries. Its own
  enchantments cannot be raised.
- A plain book is not offered as an item, although the anvil enchants one
  from an enchanted book: once through the anvil the plan could not tell it
  from the fresh books.

### The choosing column

One piece of code draws and handles it in the planner view and left of the
anvil.

- Top line: the chosen item's icon and name, with "(in your main hand)",
  "(in the anvil)" or "(the plan's item)" when planned from an item.
- Kind cells (20 units, full-size icon), then item cells with a lighter fill
  and frame: the main-hand item (the selected hotbar slot; the inventory is
  not searched) and, at the anvil, the item in the anvil's left slot. An
  item cell is offered only when the item can be enchanted and its anvil
  uses could be read. Choosing a kind plans a fresh item; choosing an item
  cell plans from that item's state.
- The plan's item is a record taken when its cell is pressed (kind,
  enchantments, anvil uses, name, which cell), not the live stack, so the
  plan stays the same while the item moves between hand, slot and pointer.
  A cell shows chosen while its item matches the record; of two like items
  in hand and slot only the pressed cell does.
- Pressing the kind of the plan's item turns the plan into "this item from
  fresh": what the item carries becomes wanted at the same levels.
- Set buttons "All highest", "Last choice", "Clear". All highest turns on
  every enchantment at its highest level, without curses, and raises what
  the item carries; of enchantments that exclude each other it takes the
  highest frequency weight, then the one that goes on the fewest kinds, then
  the first. It records nothing when it changes nothing. Last choice brings
  back what was chosen for that kind before (kept when the kind changes, a
  set button is pressed or the planner closes).
- Rows under "Enchantments n/9", in the game's order with enchantments that
  exclude each other together and curses last, in the game's red. A row
  reads name, level stepper (`-` level `+`), switch at the right edge; the
  level stays in one column whatever the row's state. Off: the level it
  would turn on at, faint, without buttons. One level only: the switch
  alone. A press turns a row on or off; `-` below level 1 turns it off.
- A row conflicting with something chosen or carried is faint and says
  "Conflicts with <name>" (no level) in place of stepper and switch. With
  nine books chosen the other rows cannot be turned on ("At most 9 books").
- A row for what the item carries has a dimmed switch that stays on, its
  level in the level column and level buttons from its level up (not for an
  enchanted book). Pressing it changes nothing and explains itself in the
  status line.

### The steps' side

One piece of code draws it in the planner view and right of the anvil.

- The goal as a stepper box (plain text where it cannot be switched). On
  hover the status line tells the other goal's total and anvil uses, or
  that both goals give the same order.
- One run of text: the total, the player's level ("(you have N)", warning
  colour when below the dearest step, not shown in Creative), and
  "Materials have/need" (warning colour when short). Materials are the
  plan's inputs, held alone or inside a merged stack.
- Two details, side by side or a line each: "Needed at once: N levels" (the
  dearest step) and "Work penalty of the result: +N", each explained in the
  status line on hover.
- Steps, two rows each with a line between steps: the kept side with its
  icon and the step's cost, then "+" and the sacrificed side with "incl. N"
  when N of the cost is prior work (explained on hover). A side is named by
  its contents. The item's icon glints from the step at which the item
  carries an enchantment. A cut name shows whole in a box by the row on
  hover.
- Exactly three ways to tell steps apart: a name in the warning colour is
  not at hand, a faint step is done, a white bar marks the next step. A
  step's cost is in the warning colour when the player's level is below it.
- Last line, up to three lines: the plan is finished; else "Next: step N"
  when its two stacks are at hand (or nothing is missing); else "Not in
  your inventory: ..." with everything the whole plan lacks.
- Progress: a step is done when one stack at hand holds everything the step
  joins. Only the plan's inputs and its steps' results count. A finished
  item is ignored while another unfinished item of the plan is at hand (a
  second one is being made). The plan's item is recognised by enchant slot
  and enchantments, so another item of that kind with the same enchantments
  counts as it.

### The planner view

- A page of the settings screen: heading "Inventory > Enchantment order"
  with a way back to Inventory, choosing column left, steps' side right,
  three-line footer (status or hover help; how to use the rows; the 40-level
  limit or the Creative note, ending "Esc: back").
- Opening: an unfinished plan opens as it was left, whatever is in the main
  hand. Without one, the main-hand item is taken when it can be enchanted
  (from its state when its anvil uses could be read, else as a fresh item
  of its kind, with nothing said); else the kind left last, or the first.
- The steps' side follows the player's inventory (36 slots) every frame.
- The wheel scrolls the side under the pointer; both scrollbars drag. A
  press outside the view goes to the settings screen (sidebar, Close) and
  leaves the view.
- Esc: opened by the key or from the anvil's "Change" button, it closes the
  screen (back to the game or the anvil); opened from the settings, it goes
  to the settings table. The Back button always goes to the settings table.

### Beside the anvil screen

Drawn only while "List the steps beside the anvil" is on and the game
version is verified.

- The steps' side stands right of the anvil window, up to 200 units wide.
  With less than 110 units there it goes left; with less than 110 on both
  sides nothing is drawn. It starts level with the choosing column's top
  (the window's top without one).
- The choosing column stands left of the window when its setting is on and
  there are at least 130 units left and 110 right. It is then always shown,
  at one fixed height, with three status lines under its rows (a message
  stays four seconds). Its main-hand and left-slot cells follow hand and
  slot while the anvil is open.
- With nothing chosen and no plan, an item put in the left slot is taken as
  the plan's item. Otherwise the slot only offers its cell.
- Without the choosing column the steps' side has a "Change" button at its
  bottom right, with or without a plan. A left press with nothing on the
  pointer opens the planner view over the anvil screen; the anvil and its
  slots' items are still there afterwards.
- Without a plan the steps' side shows the goal, the heading and a line
  saying what to do, in the room of two steps.
- Input: left presses and wheel turns over the panels are kept from the
  game, judged from the boxes and pointer of the last frame and acted on
  with the next frame on the client thread. Nothing is taken while an item
  is on the pointer or while the panels fade. The goal is switched there
  only while the choosing column is shown. The wheel scrolls rows or steps;
  a press on a scrollbar jumps (no dragging).
- At hand at the anvil: the two input slots, the pointer, hotbar and
  inventory.
- The step whose pair lies in the anvil's slots is tinted, and the last
  line speaks of that pair first: "The slots match step N"; "The game
  says N levels; the plan may be off" when the anvil's own cost differs
  from the plan's; or that the left item, the right item or the pair is not
  part of the plan.
- Tints ("Tint the slots to use next"): a fill at 50% over the whole slot,
  drawn over the item. Cyan is the side that goes left, pink the right;
  green, red and yellow are kept for the anvil's verdicts.
  - Hotbar and inventory: the first stack holding exactly the kept side in
    cyan, the first holding exactly the sacrificed side in pink. A side
    lying in its own anvil slot or carried on the pointer is not marked;
    the other keeps its mark. A stack on the pointer is not marked.
  - Anvil left and right slot: empty with its stack at hand, the side's
    colour; holding its own stack, that colour with a frame; holding
    anything else, red with a frame. A slot counts as placed only when it
    holds its own side, and both anvil slots count as at hand.
  - Result slot, both slots filled: green when the pair is a step at the
    plan's cost, yellow when the game's cost differs, red when it is no
    step.
  - These are held against the step the placed pair makes, else the next.
- Drawing: the panels are painted right before the game's item tooltip in a
  frame that has one (above the screen's dark layer, under the tooltip),
  else after the screen. They are always drawn. An item on the pointer
  overlapping a panel is drawn once more on top of it, with glint,
  durability bar and count (at the game's own count control's place).
- Closing: the panels slide and fade with the anvil window (its distance
  from rest, or the window's own alpha when lower). Everything attached to
  the anvil closes with it. A finished plan is cleared once the anvil has
  closed, its enchantments going to "Last choice"; an unfinished plan stays
  for the session.

## Decided

By the maintainer, 2026-10-12 to 2026-11-14. Behavior above carries the
detail; these are not to be undone without asking.

- Fresh books only; no merging two books of one enchantment, no more than
  10 inputs, no plain book as the item (knowing the anvil takes one).
  Planning from the books carried may come later.
- The planner has no switch (key and Open button only); the anvil parts
  have one, off for a new install (L-338). Two goals, fewest levels first,
  under one goal stepper.
- Kinds from an icon grid by enchant slot (a `◀ ▶` stepper does not scale),
  named by the game in every language; "held" is the main hand only; an
  item to plan from is a cell after the kinds, set apart by fill and frame
  (no mark on the picture, no switch row); its enchantments are fixed, its
  anvil uses charged, its levels raisable.
- Sets: "All highest" (curses out, exclusive groups by the game's data
  alone) and the last choice per kind; no Lamium-written or saved sets.
  Rows: name, level stepper, switch at the right edge, level in one place;
  curses last in the game's red. Japanese screen name "エンチャント順序".
- The previous plan wins over the main hand; item cells switch only when
  pressed; no setting for it.
- At the anvil: chooser left, steps right, always shown (a press each time
  is a chore), a setting to hide it and "Change" where it is not shown; the
  planner page stays too; the anvil chooser shares the planner's look and
  handling. The item is not to be taken from the left slot by itself (as
  built it is when nothing is chosen; open in L-170).
- One steps' component for both screens: count beside the total, no
  materials list, the level beside the total, "Needed at once" / "Work
  penalty of the result", three ways to tell steps apart and no more. The
  steps' side is closed: no polish asked.
- Panels always drawn, a pointer's item on top; the item icon glints from
  its first enchanted step.
- Slot marks: tint alone at 50%, cyan left, pink right; green right pair,
  red wrong, yellow differing cost; no mark on a carried stack in the first
  version; their own setting, on (a matter of taste).
- An item's work penalty in its tooltip is L-319's.

## Code

| Part | File |
|---|---|
| Order search, cost rules, matching a stack to the plan (pure): `plan`, `Rules` (`goal`, `refuseAt = 40`), `levelCost`, `carried`, `stepFor`, `Input::onItem` | `src/features/inventory/EnchantPlan.h` |
| Chooser edits, progress, slot marks (pure): `allHighest`, `grouped`, `release`, `progress`, `follow`, `anvilSay`, `gridSlot` | `src/features/inventory/EnchantOrder.h` |
| Game reads, the session's plan, the panels beside the anvil, press/wheel handling (`anvilPress`, `anvilWheel`) | `src/features/inventory/EnchantOrderGame.cpp` |
| Planner page | `src/ui/EnchantView.cpp` |
| Choosing column (both places) and layouts (`layColumn`, `fitPanel`, `EnchantStepsLayout`) | `src/ui/EnchantChooser.cpp`, `src/ui/EnchantLayout.h` |
| Steps' side (both places) | `src/ui/EnchantSteps.cpp` |
| Mouse events kept from the anvil screen; opening over the anvil (`ui::openEnchantOrder(client, true)`) | `src/input/CustomInput.cpp`, `src/ui/SettingsScreen.cpp` |

Tests: `tests/EnchantPlanTests.cpp` (the planner against exhaustive search
for both goals, the edits of `EnchantOrder.h`, the layouts). No hooks of its
own: the anvil panels draw from `UIRenderEvent` on the anvil screen and
from Inspection's tooltip hook. Mockups: [planner](demos/enchant-planner.html),
[anvil](demos/enchant-anvil.html), [steps](demos/enchant-steps.html).

## Known

Anvil rules (VALIDATION-LOG "Anvil costs behind the enchantment planner",
"Anvil cost of raising a level"): the cost formula, the surcharge and the
40-level refusal in Behavior were read from the vanilla anvil screen, with
nothing else added for undamaged, unrenamed inputs. A sacrificed item's rate
is twice a book's. Raising a carried level charges the difference. The anvil
enchants a plain book from an enchanted book.

Game data (VALIDATION-LOG "World probe: the enchantment registry"; all link
on SDK 26.60.4):
- `Enchant::mEnchants()`: 42 entries on the client, each with
  `mEnchantType`, `mStringId`, `mFrequency`, `mIsDisabled`, virtual
  `getMaxLevel()` and `isCompatibleWith(type)`.
- No linkable function gives the rate (`ItemEnchants::getTotalValue` is
  `MCNAPI`). `mFrequency` 30, 10, 3, 1 matched 1, 2, 4, 8 per level for a
  sacrificed item and 1, 1, 2, 4 for a book in all nine costs read. A new
  enchantment needs no update; a new frequency value does.
- `EnchantUtils::canEnchant(stack, {type, level}, false)` answers
  `Enchant`, `Increment`, `Conflict` or `Fail`; `Conflict` for one the item
  already has at that level.
- Names: `EnchantUtils::getEnchantNameAndLevel(type, level)`; level I and V
  texts share the bare name. Anvil uses: `getBaseRepairCost()`.
- A plain book's enchant slot value is the enchanted book's "everything".

Anvil screen (VALIDATION-LOG "World probe: the anvil screen", "The item held
on the pointer at the anvil"):
- `getScreenName()` is `anvil_screen`; the window is `root_panel`, its box
  from `mCachedPosition` / `mSize` once laid out; its `mAlpha` falls as it
  slides away on closing.
- Slot contents: `getItemStack("anvil_input_items", 0)`,
  `("anvil_material_items", 0)`; hotbar and inventory from `hotbar_items`
  and `inventory_items`. Enchantments via
  `constructItemEnchantsFromUserData`.
- Slot boxes: `hotbar_grid` / `inventory_grid` corner plus index (nine
  columns of 18 units, hotbar index 0 first); the anvil's slots are the
  controls `input_item_slot`, `material_item_slot`, `result_item_slot`.
- The game's cost for the placed pair: `AnvilContainerManagerController::mCost`
  (a class-layout read).
- The item on the pointer: control `selected_item_icon` (21 units,
  following the pointer); its count `selected_stack_size_text`.
- Pointer in GUI units: `ScreenView::mPointerLocationPrevious`.
- Mouse events arrive outside the client tick; `CustomInput`'s
  `MouseInputEvent` listener can cancel them before vanilla. A click outside
  the window drops the pointer's stack in vanilla, keys go to the screen.
- Lamium's scene can be pushed over the anvil screen (`ui::open` accepts
  `anvil_screen` for the planner only); the anvil window is not drawn
  meanwhile, and Esc returns with the slots' items in place (the anvil
  replays its opening animation).
- The game's tooltip drawing runs inside the screen's drawing; painting
  there, right before the tooltip, is above the screen's dark layer.

Icons and glints (`inspection/render/ItemIcon.cpp`, shared):
- The glint overlay is scaled by the current shader colour, i.e. whatever
  was filled last; `drawItemIcons` sets it to white around each glint.
- A stack drawn several times in one frame loses its glint (and others'):
  every drawn icon has a stack of its own (pools in `EnchantOrderGame.cpp`).
- A glint drawn after another batch of icons does not show: the held item
  is drawn in the same call as the list's icons, last.

## Tried

- Painting from `InventoryItemRenderer::$render` (before the first slot):
  the list and every enchanted item turned white; the game is mid-way
  through its item drawing there. Do not paint from the slot renderer.
- Drawing the panels before the screen: they sit under its dark layer.
- Not painting the panels under an item on the pointer: flickered; the
  maintainer wants them always drawn.
- Marking a stack carried on the pointer: flickered in front of and behind
  the item and did not fit the held size.
- Two-unit outlines (accent left, white right) as slot marks: they covered
  the item and white did not show on the light inventory. Green for "left"
  clashed with green for a right pair.
- The game's own hover tint on other slots: it belongs to the slot's own
  drawing (writing the game's controls, one colour); not built.
- Finding the held count's control by "count" in its name: none matched.
- Growing the anvil's choosing panel with its rows: restless; it keeps one
  height.

## Limits

- Not modelled and never read from the anvil: a damaged input, a rename, a
  held enchanted book whose own enchantment is raised.
- `EnchantUtils::combineEnchantedItems(first, second, out, book)` links and
  returns an int; it may give the game's own cost for a pair and replace the
  frequency mapping. Not tried.
- A main-hand item whose anvil uses cannot be read is planned as fresh with
  no notice (`enchantOrder.heldUnknown` is translated but unused).
- Unchecked in game: see [VALIDATION.md](VALIDATION.md), row "Enchantment
  order, experimental".

### Remaining

- [L-170 Enchantment order planner](backlog/L-170.md): open choices (slot
  item taken by itself, the plan across a world exit, marking a carried
  stack, scrollbar dragging and search in the anvil's chooser) and the
  runtime checks.
- [L-342 Lower findings of the 2026-11-14 review, kept for later](backlog/L-342.md),
  "Enchantment order": reported rough spots, none agreed.
- [L-319 Item details: Work Penalty first, then the unshown rest](backlog/L-319.md):
  the work penalty in an item's tooltip.
- [L-91 Shield glint missing in Lamium's item icons](backlog/L-91.md): glints
  outside vanilla slots.
