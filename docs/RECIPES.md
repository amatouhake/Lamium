# Recipes and the material plan

Looking recipes up in the game and planning what to gather, inside Lamium:
an item list beside the inventory, a recipe view, and a plan that traces
what features and the player want down to what must be gathered. The design
is agreed with mockups and built in steps ([L-359](backlog/L-359.md));
there are no settings rows or keys yet. Built: the schematic Materials
tab's raw view ([SCHEMATIC.md](SCHEMATIC.md)) and, only in builds made with
`xmake f --item_list=y`, the item list and the recipe view opened from it.

## Behavior

Decided by the maintainer on 2026-11-17 and 2026-11-18. What is still open
is in the L-item.

### Principles
- Lamium alone: no link to another mod's recipe data or screens. Recipes
  come from the game ([PROVENANCE.md](PROVENANCE.md) for what may be read).
- Each feature keeps working alone. The plan is a shared part that features
  register their needs into ([INTEGRATION.md](INTEGRATION.md)): a feature
  that is off registers nothing, and the plan works with whichever are on.
- The screen is laid out with the planned parts in mind, also those built
  later, so that adding them does not move what is there.

### The item list
- Beside the game's window on every screen that handles items (inventory,
  crafting table, furnace, chests), always shown: all items on the right.
  Not at the anvil for now, where Enchantment Order's panels are.
- A store can only be a storage block (chest, barrel, shulker box, ender
  chest): what a furnace or hopper holds changes by itself.
- On the left: favourites at the top, the plan in the lower part (its
  lines carry item icons; what else it shows is decided with the plan).
- Both side lists lie on a dimmed band that runs to the screen's edge, with
  no frame and no margin: the game dims the world too little for bare icons.
- The favourite key puts the hovered item in or takes it out, wherever it
  is pressed.
- One search field at the bottom centre of the screen, low enough that it
  never has to move when the centre shows more: close to the inventory, and
  where later uses of the same field belong (highlighting the inventory
  slots that match, calculating).
- The list can be narrowed, at least to what can be made now. The filters
  have their place from the start, whenever they are built.
- The list moves aside when the game's own recipe book is switched on.

### The recipe view
- Opens in the centre of the screen, in Lamium's own look: its panel,
  buttons and tabs, and the cells the container preview already draws. The
  arrangement follows the game; the game's look can come later if wanted.
- The window keeps one size, two recipe columns wide, so nothing changes
  between items; where one column is enough it sits in the middle. The
  centre keeps that room free, so the side lists never move.
- Shows as many recipes to a page as the window holds (two were too few
  for an item with dozens of uses).
- Stations are tabs: one station's recipes to a view, each the same size.
- The item's own actions sit together in the heading: a favourite button
  (the key works too) and "add to plan". Recipes carry no favourite button
  of their own: any item in a recipe can be opened, and the key is there.
- One line of key hints sits just above the search field, in the same
  place whether a recipe is open (the window's last line) or not: click
  for recipes, right click for uses, the favourite key, and Backspace for
  back while a recipe is open.
- The game's inventory key closes the screen with the view open as it
  does without it; every other key is kept from the game meanwhile.
- Asked what an item is used in, an ingredient it is one of several for
  shows that item, not the others in turn.
- R over an item shows its recipes and U what it is used in, in the lists,
  the view and the game's own slots. Sorting therefore has no default key.
- Each recipe carries one control, "place" (with Shift: craft many, Mass
  Craft), and the marks "default" (press to make it the item's default),
  "any order" for a shapeless recipe and "2x2" for one the inventory's own
  grid can make.
- A recipe is shown as the game shows it, as closely as Lamium can draw it:
  a crafting recipe in the 3x3 grid, a furnace's or another station's in a
  fixed layout that follows that station's own screen. This is the one
  form: no short form beside it and no line of totals under it. A smaller
  form is thought about when a place needs one.
- Placing a recipe into the grid and crafting it many times (Mass Craft,
  [L-165](backlog/L-165.md)) start from the recipe view: their controls are
  part of its layout from the start, and they are researched together.
- An ingredient any one of several items satisfies (any planks) shows its
  members in turn, the icon changing every few seconds; members the player
  holds come first, then older items. All members appear as a grid of icons
  (a new shared widget), not a row each.
- Every station that can do a recipe is named, as icons.

### The plan
- Its words name what the player sees. The list the player fills is
  "wanted" (the panel's title, the window's title and its first tab; the
  recipe window's button is "add to wanted"); what comes out is "to gather"
  and "to make". On a row, "gather" moves an item that would be made to the
  gather list and "make" moves it back. A recipe is marked "in use", and
  another offers "use this". "Back to any" undoes a picked group member.
  "Plan" stays the feature's name in settings only.
- There is one plan: a list of targets in, what to gather and what to make
  out. Several named plans are not built until one proves too few.
- Targets come from features (a schematic's materials still to place) and
  can be added by hand. Hand-added targets are saved per world, under the
  waypoints' key ([MAP.md](MAP.md)). A target a feature handed over cannot
  be edited; it looks like the others and says where it comes from.
- The plan's short list on the left shows either what is left to gather or
  the targets, switched in place.
- A key (W by default, beside A for favourites; both rebindable) puts the
  hovered item into "wanted" or takes it out, wherever the favourite key
  works. An item taken out keeps its count until the world is left, so
  putting it back restores it.
- A wanted item's count is changed with the shared stepper (- N +), on the
  short list and in the window alike; nothing else changes it.
- Picking a member for a group and saying that an item is gathered, not
  made, are done inside the plan.
- The plan is lists; no tree view for now.
- What is held is the sum over the places that are counted: carried items
  always, the shulker boxes among them by default; later the ender chest
  and containers the player marks as stores, each counted as last seen
  open. One count serves the plan and the schematic Materials tab;
  Enchantment Order keeps counting the inventory. The later places are
  built after the first version, but their place on screen is fixed now:
  a "counted places" tab in the plan's window with a switch per place; a
  "have" column whose hover tells where the items are; a star on a number
  that includes a last-seen count, in a gutter of its own so the digits
  stay aligned; and, while a container is open, a "make this a store"
  button on the last line of the left band, which is kept free for it.
- Kept between sessions: only what the player enters (recipe choices, group
  picks, "stop here" marks), in one file for all worlds; an entry the
  current world's recipes lack is ignored, not deleted.
- The schematic Materials tab shows what is left as things to gather, no
  more. The steps to make belong to the plan.
- Which recipe makes an item by default, groups, fuel, spare output and
  held items: the rules in [L-116](backlog/L-116.md).

## Code
- `src/features/recipes/RecipeIndex.h`: the game's table by name, the
  default recipe per item, groups and `plan()`. `RawView.h`: a plan as
  gather and make lists. `RawMaterials.h`: the reduction. Pure;
  `tests/RecipeIndexTests.cpp`, `tests/RawMaterialsTests.cpp`.
- `RecipeSnapshot.cpp` reads the client's recipe table into the index, 600
  recipes a call, keeping no game pointer. `RecipeService.cpp` runs it each
  time a screen asks and publishes the index until the world is left, behind
  the RecipeTable capability ([GAME-UPDATES.md](GAME-UPDATES.md)).
- `ItemListLayout.h`, `RecipeView.h` (pure): where the list and the view
  sit, an item's recipes by station, a recipe's slots. `ItemList.cpp` draws
  the list after a game screen and keeps input from the game (the listeners
  in `input/CustomInput.cpp` ask it); `RecipeViewDraw.cpp` draws the view
  and does not know its host.
- `src/ui/SchematicsView.cpp` (`refreshRaw`): the Materials tab's raw view.
- `recipe_trace` (`src/features/research/RecipeTrace.cpp`) writes the table
  to a file and logs sample plans.

## Known
What the client's table holds and how it reads: [L-116](backlog/L-116.md)
"Known" and VALIDATION-LOG 2026-11-17.

## Limits
- The recipe view so far: R and U over an item are fixed keys, not
  actions; no "place", no
  changing the recipe in use, no favourite or "add to wanted" buttons; a
  group's members show in the table's order, not held ones first; what a
  recipe leaves besides its product is not shown; recipes that use their
  own product are left out with the index.
- Items go by name: variants told apart by an aux value are one item (a bed
  plans as its first color).
- Potion mixes are not read (the SDK gives them no layout); trades and
  drops are not recipes.
- Checked on a local world and one server built on Bedrock Dedicated
  Server; other servers and Realms are not.
- Mockups: [demos/recipe-viewer.html](demos/recipe-viewer.html),
  [demos/materials-raw.html](demos/materials-raw.html).
