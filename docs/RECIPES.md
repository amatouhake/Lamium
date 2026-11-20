# Recipes

Looking recipes up in the game, inside Lamium: an item list beside the
inventory and the game's other item screens, a search field, a recipe view,
favorites, placing a recipe into the grid and Mass Craft. The design is
agreed with mockups and built in steps ([L-359](backlog/L-359.md)); the
plan of what to gather and the stores counted as held are in
[PLAN.md](PLAN.md), features of their own that work with Recipes off
(L-373). Built behind the Recipes switch (Inventory, experimental, off by
default): the item list with its search field and filter, the recipe view
and favorites. Keys, all rebindable, mouse buttons included: R (recipes of
the item under the pointer), U (its uses), A (favorite) and Backspace
(back, while the recipe view is open). What of this nobody has seen in game
is under "Limits".

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
- On the left: favorites at the top, the plan in the lower part (its
  lines carry item icons; what else it shows is decided with the plan).
- Both side lists lie on a dimmed band that runs to the screen's edge, with
  no frame and no margin: the game dims the world too little for bare icons.
- The favorite key puts the hovered item in or takes it out, wherever it
  is pressed.
- One search field at the bottom centre of the screen, low enough that it
  never has to move when the centre shows more and high enough for
  Japanese and letters that reach under the line: close to the inventory, and
  where later uses of the same field belong (highlighting the inventory
  slots that match, calculating).
- The list can be narrowed, at least to what can be made now. The filters
  have their place from the start: the list's bar, "All" and "Can be made"
  (one switch where the band is narrow).
- In the item list a favorite's cell is framed in the accent color; in
  both lists a wanted item carries how many are wanted where a stack has
  its count.
- An item held on the pointer is drawn over the bands and the windows, as
  beside the anvil.
- The game's tooltip for one of its slots is drawn over the bands: while
  the list shows, Inspection holds the game's tooltip call back and makes
  it again after the bands are painted. A slot under an open Lamium
  window has none, nor a container preview. The bundle's tooltip is a UI panel and still goes
  under the bands.
- The search (L-377): words must all be in the name as shown, the id or
  (with English search on) the English name; `-word` leaves out, `a|b`
  either (AND binding tighter), `"…"` a phrase, `#` a tag, `$` the
  tooltip text (read only once a search asks), `@` the id's namespace.
  Full-width signs read as ASCII, hiragana and katakana alike, case
  ignored; no regular expressions. Ctrl+F puts the cursor in the field
  (in it, selects all); Up and Down step through earlier searches, kept
  when the field gives up the focus; a tip over the field gives the
  syntax. The usual editing clears it (Ctrl+A, Backspace).
- The agent's choices while building, to confirm: "can be made" is a crafting
  recipe that what lies in the inventory itself (not what shulker boxes
  and bundles hold) makes once, without making anything first, and that
  fits the screen's grid (any size where there is none; L-367); the wanted mark being the count; the left band is
  halved between favorites and wanted; favorites are kept with the notes
  for all worlds; a chosen recipe is kept by station and id; deleting a
  world's or a server's saved data removes its wanted items; "any order"
  is a small mark of three squares, named under the pointer.
- The list moves aside when the game's own recipe book is switched on.
- Under "can be made" a press lays the item's recipe into the grid, as
  the game's recipe book does (Shift: as many as divide): the chosen
  recipe where it can be laid, else the first that can; pressed again, one
  more of each (nothing when short or full; with Shift a grid already as
  full as dividing allows is left alone). Without a grid or a fitting recipe it shows the recipes;
  a right press and the keys work as elsewhere (L-367).

### The recipe view
- Opens in the centre of the screen as a window in the game's grey, opaque
  (a dark one read badly over the game's window, and a translucent one let
  it show through), with dark text; its buttons, tabs and cells are
  Lamium's shared ones. The arrangement follows the game.
- The window keeps one size, two recipe columns wide and three rows high,
  in the middle of the room above the search field (all of it where it is
  lower; L-367); where one column is enough it sits in the middle. The
  centre keeps that room free, so the side lists never move.
- A page is two columns of three rows, six recipes, with room between
  them and inside the window's edge (L-367; ten without margins read as
  crowded, two were too few for an item with dozens of uses).
- Processes are tabs on the window's top edge (L-376), as other recipe
  viewers keep them: crafting, smelting, blasting, smoking, campfire
  cooking, stonecutting, smithing. The game files one cooking recipe under
  several stations' tags; it shows in the tab of each process that does
  it (steak in three). A tab is its usual station as an icon (campfire
  cooking: the campfire), named under the pointer by the process alone
  ("Smelting"); each the same size.
- A two-line heading: the item's icon and name across the width (under
  the pointer, the item's tooltip, so a name cut short can be read) with
  its own actions at the right end, a favorite button (the key works
  too) and "wanted"; under it the tab's stations as items, each once (the
  item's tooltip, a press its recipes, a right press its uses, as any
  item), the process's name and the pager (previous,
  "1 / 3", next, as wide as two-digit pages so the buttons stay put). The wheel turns pages too. Recipes carry no favorite
  button of their own: any item in a recipe can be opened, and the key is
  there.
- Every tip over the view or the plan window is the game's tooltip box.
- One line of key hints sits just above the search field, in the same
  place whether a recipe is open (the window's last line) or not: click
  for recipes, right click for uses, the favorite key, and Backspace for
  back while a recipe is open.
- The game's inventory key closes the view like Esc and leaves the screen
  under it open; a second press closes that. Every key but the two recipe
  keys is kept from the game while the view is open.
- Asked what an item is used in, an ingredient it is one of several for
  shows that item, not the others in turn.
- R over an item shows its recipes and U what it is used in, in the lists,
  the view and the game's own slots. Sorting therefore has no default key.
- Each recipe carries one control, "place" (with Shift: as many as divide
  evenly), a small square button with a picture (an arrow down onto a
  line) at its upper right, named under the pointer, flat and darker with
  the reason when something is missing;
  and one small mark, "any order" for a shapeless recipe. That a recipe
  fits the inventory's own grid is seen from its shape. Each recipe lies
  on a raised face. An item with several recipes marks the one in use
  under its product, a check when chosen in "How to make", "automatic"
  when not; a press goes to its line there (L-370).
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
  has the button. A press lays one of each ingredient into the grid (one
  more of each onto the same recipe, 2026-11-20), with Shift as many as
  divide evenly from every stack carried, and closes the view. Crafting is the player's: laying in and crafting at one press was
  tried and is not what a recipe view is expected to do. What lay in the
  grid goes back to the inventory first. An ingredient that is missing disables the
  button and turns its slot's face red, as the game does. Any of several items:
  the member "How to make" names while it lasts, else the one most carried.
- An ingredient any one of several items satisfies (any planks) shows the
  member "How to make" names, else its members in turn with a star on the
  icon's corner (on every Recipes screen); it is named by its tag
  ("Planks") or "Raw Gold etc.", and under the pointer shows its members.
- A station's recipes (R, a left press) are how it is made; its uses (U, a
  right press) first what it does, only the recipes it can do, its
  heading naming all the process's stations as elsewhere, then the
  recipes it goes into, none twice.

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

### The look, reviewed with the maintainer (2026-11-19)
What bothered is in VALIDATION-LOG of that day; what was agreed is built:
- "One of several" as DESIGN "Controls" has it: the recipe view's
  stations, the plan's window, the band's lists (pictures where the band
  is narrow), the item list's "all / can be made".
- The recipe view's slots as the game draws its own, a missing
  ingredient's face on the game's red. "Favorite" and "wanted" are two
  joined buttons, green when on (chosen over a name and a switch and over
  icon toggles), at the heading's right end.
- A Lamium window's grey covers the whole span between the two bands, so
  the game's window with its recipe book out does not show beside it.
- The band's list and the plan window's lists scroll: the wheel, and the
  shared bar (`listScrollbar`). The item list on the right keeps pages.
- The open container's switch is the band's last line under a rule like
  the one above the plan, no stronger a division than that.
- An item Lamium draws says under its name what the game says of it,
  with Lamium's item details, in the game's tooltip box: the stack's own
  hover text, which Lamium's hook has added to, each line in the color it
  starts in, without the name and lines of icon glyphs (`hoverLines`). A
  row of the plan's window also says its count in chests and stacks.
- Settings (L-373): Recipes (experimental, off by default) with
  "Favorites" (on): without favorites, no favorites in the band, no
  marks, no button, and A does nothing. The Plan and the stores are
  features of their own ([PLAN.md](PLAN.md)); the band shows while any of
  the three has something in it.
- Lamium's parts stand still and fade with the game's window as it
  slides in and out, by how far it is from where it rests, as the anvil's
  panels do (`ui/WindowFade.h`; maintainer 2026-11-19).

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
- `BandLayout.h` (pure, L-374): the bands as regions beside, above and
  below the game's window, panels split along a region, one page to a
  panel; the layout is kept in the settings (`inventory.bands`, today's
  look by default). `BandEditor.h` (pure, L-375): where a dragged panel
  goes and the layout after the drop; `BandEditorDraw.cpp` draws the
  editor over the inventory, which the "Layout of the bands" link rows
  (Recipes, Plan) open after closing the settings. A page whose feature is off is
  left out; each panel keeps one row; a region too low for one row lends
  its panels to a side on that screen. `BandPages.cpp` draws the
  favorites, the plan's list and the open container's line, which lies
  in the left region's last panel.
- `ItemListLayout.h`, `RecipeView.h`, `ListFilter.h` (pure): where the
  lists, the search field and the view sit, an item's recipes by kind, a
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
- `RecipeNotes.h` (pure; documents in `RecipeNotesStore.cpp`): favorites,
  chosen recipes, picked members and items marked as gathered, for all
  worlds in the config folder's `recipes.json`; the wanted items per world
  in `wanted.json` beside its waypoints, the counted places (`Places.h`,
  pure) in `places.json` there; `RecipePlaces.cpp` reads the open
  container behind the CountedPlaces capability. `RecipeState.cpp` loads and saves
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
- The search field takes no pasted text.
- Mass Craft: seen in a local world (VALIDATION-LOG 2026-11-18); the
  throw from the pointer and the surplus are not, nor a server.
- "Place": a server is unchecked, and stations other than the two
  crafting grids have none. Shift + place with Item transfer on and an
  inventory slot under the pointer was not tried.
- Not built: held members first in a group's turn. Their own
  items: taking an item out of the list in creative
  ([L-360](backlog/L-360.md)), brewing ([L-361](backlog/L-361.md)), what
  a recipe gives back, a cake's buckets ([L-362](backlog/L-362.md)), and
  a listed item's own data ([L-363](backlog/L-363.md): no potion effect).
- Items are told apart by name and aux. Items that differ only in their
  data (enchanted books, a filled map) are one item. The schematic
  Materials tab still counts what is carried by name alone.
- Trades and drops are not recipes.
- Checked on a local world and one server built on Bedrock Dedicated
  Server; other servers and Realms are not.
- Mockups: [demos/recipe-viewer.html](demos/recipe-viewer.html),
  [demos/materials-raw.html](demos/materials-raw.html).
