# Recipes and the material plan

Looking recipes up in the game and planning what to gather, inside Lamium:
an item list beside the inventory, a recipe view, and a plan that traces
what features and the player want down to what must be gathered. The design
is agreed with mockups and built in steps ([L-359](backlog/L-359.md)).
Built: the schematic Materials tab's raw view ([SCHEMATIC.md](SCHEMATIC.md))
and, behind the Recipes switch (Inventory, experimental, off by default),
the item list with its search field and filter, the recipe view,
favourites and the wanted items with their window. Keys, all rebindable,
mouse buttons included: R (recipes of the item under the pointer), U (its
uses), A (favourite), W (wanted) and Backspace (back, while the recipe view
is open). What of this nobody has seen in game is under "Limits".

## Behavior

Decided by the maintainer on 2026-11-17 and 2026-11-18, the second day
after using the first build. What is still open is in the L-item.

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
  never has to move when the centre shows more and high enough for
  Japanese and letters that reach under the line: close to the inventory, and
  where later uses of the same field belong (highlighting the inventory
  slots that match, calculating).
- The list can be narrowed, at least to what can be made now. The filters
  have their place from the start: the list's bar, "All" and "Can be made"
  (one switch where the band is narrow).
- In the item list a favourite's cell is framed in the accent color; in
  both lists a wanted item carries how many are wanted where a stack has
  its count.
- An item held on the pointer is drawn over the bands and the windows, as
  beside the anvil.
- The agent's choices while building, to confirm: the search takes the
  name as the game shows it or the item's id, every word typed must be in
  one of them, letters match in either case; "can be made" is a crafting
  recipe that what is carried (shulker boxes included) makes once, without
  making anything first; the wanted mark being the count; the left band is
  halved between favourites and wanted; favourites are kept with the notes
  for all worlds; a chosen recipe is kept by station and id; deleting a
  world's or a server's saved data removes its wanted items; "any order"
  is a small mark of three squares, named under the pointer.
- The list moves aside when the game's own recipe book is switched on.

### The recipe view
- Opens in the centre of the screen as a window in the game's grey, opaque
  (a dark one read badly over the game's window, and a translucent one let
  it show through), with dark text; its buttons, tabs and cells are
  Lamium's shared ones. The arrangement follows the game.
- The window keeps one size, two recipe columns wide, so nothing changes
  between items; where one column is enough it sits in the middle. The
  centre keeps that room free, so the side lists never move.
- Shows as many recipes to a page as the window holds (two were too few
  for an item with dozens of uses).
- Stations are tabs: one station's recipes to a view, each the same size.
  A tab is the station's block as an icon, named under the pointer.
- The item's own actions sit together in the heading: a favourite button
  (the key works too) and "add to wanted", which keeps its width when it
  turns into "wanted". Recipes carry no favourite button
  of their own: any item in a recipe can be opened, and the key is there.
- One line of key hints sits just above the search field, in the same
  place whether a recipe is open (the window's last line) or not: click
  for recipes, right click for uses, the favourite key, and Backspace for
  back while a recipe is open.
- The game's inventory key closes the view like Esc and leaves the screen
  under it open; a second press closes that. Every key but the two recipe
  keys is kept from the game while the view is open.
- Asked what an item is used in, an ingredient it is one of several for
  shows that item, not the others in turn.
- R over an item shows its recipes and U what it is used in, in the lists,
  the view and the game's own slots. Sorting therefore has no default key.
- Each recipe carries one control, "place" (with Shift: as many as divide
  evenly), and one small mark, "any order" for a shapeless recipe. That a
  recipe fits the inventory's own grid is seen from its shape. Which recipe
  makes an item is chosen only where it matters: opened from the list of
  what to make, an item with several recipes shows "in use" on one and
  "use" on the others.
- The arrow and the product stand at the middle of the ingredients.
- A recipe that uses its own product (copying a smithing template, dyeing
  a shulker box) is shown after the others and is never the recipe a plan
  makes the item by.
- A recipe is shown as the game shows it, as closely as Lamium can draw it:
  a crafting recipe in the 3x3 grid, a furnace's or another station's in a
  fixed layout that follows that station's own screen. This is the one
  form: no short form beside it and no line of totals under it. A smaller
  form is thought about when a place needs one.
- "Place" (maintainer 2026-11-18, [L-165](backlog/L-165.md)): on the
  inventory and the crafting table, a crafting recipe that fits the grid
  has the button. A press lays one of each ingredient into the grid, with
  Shift as many as divide evenly from every stack carried, and closes the
  view. Crafting is the player's: laying in and crafting at one press was
  tried and is not what a recipe view is expected to do. What lay in the
  grid goes back to the inventory first. An ingredient that is missing disables the
  button and frames its cell in the warning color. Any of several items:
  the member picked in the wanted window while it lasts, else the one most
  carried.
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
- Targets come from features (a schematic's materials still to place,
  while "Put what is left into the plan" on its Materials tab is on: a
  saved switch, off unless the player turns it on) and can be added by
  hand. Hand-added targets are saved per world, under the
  waypoints' key ([MAP.md](MAP.md)). A target a feature handed over cannot
  be edited; it looks like the others and says where it comes from.
- The plan's short list on the left shows the wanted items, what is left
  to gather or what to make, switched in place and named as the window's
  tabs (one button going round where the three do not fit); a press on a
  line or on "Open" opens the window on that list. The window has the tabs
  wanted, to gather and to make, and sits where the recipe view does, in
  its look; the recipe view opens over it (a press on an item's name) and
  gives it back when closed.
- A line of what to make says, under the pointer, what its crafts take and
  how many, on the short list and in the window.
- On both of the window's lists the row's button ("gather", "make") keeps
  the right end, the numbers left of it.
- A key (W by default, beside A for favourites; both rebindable) puts the
  hovered item into "wanted" or takes it out, wherever the favourite key
  works. An item taken out keeps its count until the world is left, so
  putting it back restores it. The short list then shows the wanted items,
  so what went in is seen.
- A wanted item's count is changed with the shared stepper (- N +), on the
  short list and in the window alike: minus at one takes the item out (the
  wheel and a Shift step stop at one), and a press on the number types it
  (Enter or a press elsewhere keeps it, Esc does not; 0 takes the item
  out).
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

### Mass Craft
A setting of its own, off by default, and it works with Recipes off
([L-165](backlog/L-165.md), maintainer 2026-11-18).
- It runs on the inventory and the crafting table while its key is held;
  the key has no default, and a key of several inputs matches in any
  order. It starts once the key was held for a time the
  player sets, 0 to 1 s, 0.3 s unless changed, so a brush of the key
  does nothing.
- The recipe is that of the item under the pointer, as for the recipe
  keys: in Lamium's lists or a slot of the game. Of its crafting recipes
  that fit the grid, the one the player's choices name, else the first;
  what lay in the grid goes back first, and the run goes on wherever the
  pointer moves. Ingredients need not be carried when it starts, so an
  inventory full of one ingredient can throw its surplus out.
- Over the output slot the recipe is the one lying in the grid, found in
  the table by the grid's items, and the run ends when the pointer leaves
  the slot. A grid the table has no recipe for does nothing.
- A round: all the grid makes is taken into the inventory, the product
  is thrown out and the recipe is laid in again as many times as divide
  evenly. What the inventory held of the product when the run began
  stays; everything beyond it goes, also what was thrown out and picked
  up again. A round goes out in one frame where the game shows each
  part at once, the next 50 ms later; a take that found room for a part
  only is followed by the next in the same frame.
- A step makes at most 16 slot calls (a move into a cell is two; a
  round with more cells is laid in by a step of its own), so a run
  stays under some 320 requests a second: a local world closed with a
  network error during a run without that limit.
- With no room in the inventory for the product, the smallest stack of
  an ingredient that is carried in another stack too is thrown out, so
  the take has a slot. Where there is none, the product is taken onto
  the pointer craft by craft and thrown from there: by presses on the
  output where the pointer is over it, else by the slot call that
  takes an amount.
- A round that cannot be laid in waits for more to arrive, and goes on
  throwing out the product that is picked up again meanwhile, every
  step while it keeps coming back, so the inventory keeps room for the
  ingredients. Where much of the product lies around, it still wins
  most of the slots, and ingredients on the ground come in slowly. With no slot
  free, of the slots the recipe's ingredients fill each ingredient keeps
  the share its cells are of the recipe, one slot at least and its
  fullest stacks, and the other slots are thrown out whole. A recipe of
  one ingredient throws nothing, and what does not stack (a filled
  shulker box to dye) is never thrown.
- Only what makes the run's product is taken: a round the server let
  through in part lays out another recipe (three planks of a chest's
  eight make slabs), and goes back to be laid in anew.
- It ends when the key is released. What a step leaves on the pointer
  (a cell the server did not let it place) goes back to a slot that
  takes it, out of the inventory if none does, and the product is
  thrown out. A step that shows nothing (the server refuses a
  request when items were picked up meanwhile and its inventory is not
  the client's) is tried again after 0.15 s, the grid's contents going
  back first where the run started from an item; twenty such steps in a
  row end the run. Each run writes
  its recipe and, at its end, rounds, takes, stacks thrown, requests and
  time to the log; a run that does nothing for a second writes once why
  and what it sees.

## Code
- `src/features/recipes/RecipeIndex.h`: the game's table by name, the
  default recipe per item, groups and `plan()`. `RawView.h`: a plan as
  gather and make lists. `RawMaterials.h`: the reduction. Pure;
  `tests/RecipeIndexTests.cpp`, `tests/RawMaterialsTests.cpp`.
- `RecipeSnapshot.cpp` reads the client's recipe table into the index, 600
  recipes a call, keeping no game pointer. `RecipeService.cpp` runs it each
  time a screen asks and publishes the index until the world is left, behind
  the RecipeTable capability ([GAME-UPDATES.md](GAME-UPDATES.md)).
- `ItemKey.h`: an item's key, the name plus the aux where the table tells
  variants apart; `settled()` in `RecipeIndex.h` decides where.
- `ItemListLayout.h`, `RecipeView.h`, `ListFilter.h` (pure): where the
  lists, the search field and the view sit, an item's recipes by station, a
  recipe's slots, the search and "can be made". `ItemList.cpp` is the host:
  it draws both bands and the field after a game screen and keeps input
  from the game (the listeners in `input/CustomInput.cpp` and the text hook
  in `ui/SettingsScreen.cpp` ask it; what they keep acts in the next
  frame). `RecipeViewDraw.cpp` and `PlanViewDraw.cpp` draw the two windows
  and do not know their host. `CountEdit.h`: the count being typed.
- `PlacePlan.h` (pure): which carried stacks go into which grid cell.
  `RecipePlace.cpp` does it with the slot calls Item transfer uses, behind
  its capability. `MassCraftPlan.h` (pure): the recipe a grid lays out,
  what a take added, the stacks beyond an ingredient's share; the run is
  `massCraft` in `RecipePlace.cpp`.
- `RecipeNotes.h` (pure; documents in `RecipeNotesStore.cpp`): favourites,
  chosen recipes, picked members and items marked as gathered, for all
  worlds in the config folder's `recipes.json`; the wanted items per world
  in `wanted.json` beside its waypoints. `RecipeState.cpp` loads and saves
  them and works the plan out. A file that cannot be read is left as it is
  and the changes are kept for the session only.
- `src/ui/SchematicsView.cpp` (`refreshRaw`): the Materials tab's raw view,
  what to gather only.
- Features hand targets over through `state::provide` (a function the
  plan asks twice a second); `schematic/PlanHandOver.cpp` is the
  schematic's, from the last check's material lines
  (`Verification.h` `planTargets`).
- `recipe_trace` (`src/features/research/RecipeTrace.cpp`) writes the table
  to a file and logs sample plans.

## Known
What the client's table holds and how it reads: [L-116](backlog/L-116.md)
"Known" and VALIDATION-LOG 2026-11-17.

## Limits
- Not seen in game: a server. The search field takes no pasted text.
- The look as a whole is reviewed once the feature is complete
  (maintainer 2026-11-18); until then only faults are fixed.
- Mass Craft: seen in a local world (VALIDATION-LOG 2026-11-18); the
  throw from the pointer and the surplus are not, nor a server.
- "Place": a server is unchecked, and stations other than the two
  crafting grids have none. Shift + place with Item transfer on and an
  inventory slot under the pointer was not tried.
- Not built: counted places
  beyond what is carried; held members first in a group's turn. Their own
  items: taking an item out of the list in creative
  ([L-360](backlog/L-360.md)), brewing ([L-361](backlog/L-361.md)), what
  a recipe gives back, a cake's buckets ([L-362](backlog/L-362.md)), and
  the game's tooltip and Lamium's item details for a listed item
  ([L-363](backlog/L-363.md)): today the lists name an item and no more.
- Items are told apart by name and aux. Items that differ only in their
  data (enchanted books, a filled map) are one item. The schematic
  Materials tab still counts what is carried by name alone.
- The wanted items' file goes when a world's or a server's saved data is
  deleted, but the Saved data screen does not count or show it, and moving
  a server's data to another key leaves it ([L-358](backlog/L-358.md)).
- Trades and drops are not recipes.
- Checked on a local world and one server built on Bedrock Dedicated
  Server; other servers and Realms are not.
- Mockups: [demos/recipe-viewer.html](demos/recipe-viewer.html),
  [demos/materials-raw.html](demos/materials-raw.html).
