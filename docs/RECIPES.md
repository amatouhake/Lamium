# Recipes and the material plan

Looking recipes up in the game and planning what to gather, inside Lamium:
an item list beside the inventory, a recipe view, and a plan that traces
what features and the player want down to what must be gathered. The design
is being agreed with mockups ([L-359](backlog/L-359.md)); there are no
settings rows or keys yet. The one built screen part is the schematic
Materials tab's raw view ([SCHEMATIC.md](SCHEMATIC.md)).

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
- Opens in the centre of the screen.
- Shows as many recipes to a page as the screen allows (two were too few
  for an item with dozens of uses), in a window no wider than the recipes
  shown need: one column where one is enough.
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
- Targets come from features (a schematic's materials still to place) and
  can be added by hand. Hand-added targets are saved per world, under the
  waypoints' key ([MAP.md](MAP.md)).
- What counts as held is a setting. Carried items and the shulker boxes
  among them are in by default; one count serves the plan and the schematic
  Materials tab. The setting gets a row once a second source (ender chest,
  placed chests) exists. Enchantment Order keeps counting the inventory.
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
- `src/ui/SchematicsView.cpp` (`refreshRaw`): the Materials tab's raw view.
- `recipe_trace` (`src/features/research/RecipeTrace.cpp`) writes the table
  to a file and logs sample plans.

## Known
What the client's table holds and how it reads: [L-116](backlog/L-116.md)
"Known" and VALIDATION-LOG 2026-11-17.

## Limits
- Items go by name: variants told apart by an aux value are one item (a bed
  plans as its first color).
- Potion mixes are not read (the SDK gives them no layout); trades and
  drops are not recipes.
- Checked on a local world and one server built on Bedrock Dedicated
  Server; other servers and Realms are not.
- Mockups: [demos/recipe-viewer.html](demos/recipe-viewer.html),
  [demos/materials-raw.html](demos/materials-raw.html).
