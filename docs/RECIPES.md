# Recipes

Looking recipes up in the game, inside Lamium: an item list beside the
inventory and the game's other item screens, a search field, a recipe view,
favorites and placing a recipe into the grid. The design is
agreed with mockups and built in steps ([L-359](backlog/L-359.md)); the
plan of what to gather and the stores counted as held are in
[PLAN.md](PLAN.md), features of their own that work with Recipes off
(L-373). Built behind the Recipes switch (Inventory, experimental, off by
default): the item list with its search field and filter, the recipe view
and favorites. Keys, all rebindable, mouse buttons included: R (recipes of
the item under the pointer), U (its uses), A (favorite), Backspace (back,
while the recipe view is open) and, in creative, the middle button and
Shift + it (take one, a stack). What nobody has seen in game is in "Limits".

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
- Both side lists lie on a dimmed panel that runs to the screen's edge, with
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
  (one switch where the panel is narrow).
- A favorite carries no mark in the item list (L-369); in both lists a
  wanted item carries how many are wanted where a stack has its count.
- An item held on the pointer is drawn over the panels and the windows, as
  beside the anvil.
- The game's tooltip for one of its slots is drawn over the panels: while
  the list shows, Inspection holds the game's tooltip call back and makes
  it again after the panels are painted. A slot under an open Lamium
  window has none, nor a container preview. The bundle's tooltip is a UI panel and still goes
  under the panels.
- The search (L-377): words must all be in the name as shown, the id or
  (with English search on) the English name; `-word` leaves out, `a|b`
  either (AND binding tighter), `"…"` a phrase, `#` a tag, `$` the
  tooltip text (read only once a search asks), `@` the id's namespace.
  Full-width signs read as ASCII, hiragana and katakana alike, case
  ignored; no regular expressions. Ctrl+F puts the cursor in the field
  (in it, selects all); Up and Down step through earlier searches, a
  search kept once it has stood a second while typed or when the field
  gives up the focus. The field says "Search (name, ID)"; under the
  pointer over it, while it is not typed in, the syntax shows as a
  two-column table. The field is 16 units high, its text in the middle,
  18 above the screen's bottom, and takes presses a few units around it.
- Beside the field, a square switch of its height (picture: three by three
  cells, the middle one light; green while on) and a settings row,
  "Highlight search matches in the inventory", off by default (L-378):
  while it is on and the field holds text, the slots where items are kept
  (the inventory, the hotbar, a container's) whose items do not match are
  darkened, an empty one too; a crafting grid, its output, armor and a
  furnace's slots are left alone. The slots are the screen's small square
  controls the game places in a container's cell, found once the window
  rests (nothing is drawn while it slides in or out) and again when it
  moves; a slot is matched again only when its item or the search
  changes. Behind the SearchHighlight capability. The usual editing clears it (Ctrl+A, Backspace).
- The agent's choices while building, to confirm: "can be made" is a crafting
  recipe that what lies in the inventory itself (not what shulker boxes
  and bundles hold) makes once, without making anything first, and that
  fits the screen's grid (any size where there is none; L-367); the wanted mark being the count; the left panel is
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
  in the middle of the room above the key hints over the search field (all
  of it where it is lower; L-367); where one column is enough it sits in the middle. The
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
  ("Smelting"); each the same size. An item's tabs keep that order
  whatever recipe is in use (maintainer 2026-11-21); the first shown is
  the open station's own tab, else the tab of the recipe in use.
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
- The key hints stand right above the search row on every screen, a
  window open or not, in light text over the world (maintainer
  2026-11-24, L-360): click for recipes, right click for uses, the
  favorite and wanted keys, Backspace for back while a recipe is open,
  what Shift does, the take keys in creative. Where one line does not hold
  them they take two, the room for both kept free; closed, they show only
  where the game's window leaves them room.
- The game's inventory key closes the view like Esc and leaves the screen
  under it open; a second press closes that. Every key but the two recipe
  keys is kept from the game while the view is open.
- Asked what an item is used in, an ingredient it is one of several for
  shows that item, not the others in turn.
- R over an item shows its recipes and U what it is used in, in the lists,
  the view and the game's own slots. Sorting therefore has no default key.
- "Place" is on every recipe (maintainer 2026-11-21): pressable where the
  screen takes the tab's recipes and they can be laid, else flat and
  darker with the reason in its tip (the grid is too small, or the
  station it is placed at). Under its pointer, wherever it is shown, the
  slots of what is carried short turn red; elsewhere no slot is red.
- The heading's item answers presses as any item (a press its recipes, a
  right press its uses), so an item with neither is no dead end.
- Each recipe lies on a raised face of the window's grey, 5 units around
  its slots; faces are 102 wide and 6 apart in a window 236 wide (L-379).
  At its bottom right stands "place" (with Shift: as many as divide
  evenly), a small square button with a picture (an arrow down onto a
  line), named under the pointer, flat and darker with the reason when
  something is missing. Of an item made several ways, a check box of the
  same size stands left of it (DESIGN's exception for the game's grey):
  empty for a recipe not in use, a dark check for the one the rule
  picked, a green check for the player's choice. A press chooses that
  recipe; a press on the player's choice goes back to automatic. The
  order (the recipe in use first in its tab) is taken when the item
  is shown and kept while it stays shown, so a press moves nothing. "How to
  make" still lists them all. One small mark, "any order" for a
  shapeless recipe, stands under the arrow. That a recipe fits the
  inventory's own grid is seen from its shape.
- The arrow and the product stand at the middle of the ingredients.
- A recipe that uses its own product (copying a smithing template, dyeing
  a shulker box) is shown after the others and is never the recipe a plan
  makes the item by.
- A recipe is shown as the game shows it, as closely as Lamium can draw it:
  a crafting recipe in the 3x3 grid, a furnace's or another station's in a
  fixed layout that follows that station's own screen. This is the one
  form: no short form beside it and no line of totals under it. A smaller
  form is thought about when a place needs one.
- Crafting onto the pointer (L-379): Ctrl + a press on a listed item
  crafts one round of it onto the pointer, by the recipe in use where it
  fits the grid, else the first that fits; Ctrl + "place" by that recipe.
  What lay in the grid goes back first. The round goes onto an empty
  pointer or onto what it holds of the product; a pointer holding another
  item or with no room gets the round laid in only, and nothing it holds
  moves ([L-381](backlog/done/L-381.md)). `RecipePlace.cpp` takes the
  output by the slot call (seen to craft), else, with an empty pointer,
  Shift-takes it and picks the round up; one log line says which.
- Presses on a listed item: a press shows its recipes (under "can be
  made" it places one round), Ctrl + press crafts onto the pointer, a
  right press shows its uses. Shift + press (L-360, `TakePress.h`): under
  "can be made" as many as divide evenly; under "All" in creative a stack
  into the inventory, elsewhere the recipes; the favorites and the plan's
  rows by the game mode, in creative as "All", else as "can be made" (a
  plan row then opens the list, as a press does: placing is the "to make"
  rows', maintainer 2026-11-24). One that cannot act
  shows the recipes, whose red slots tell why. Whatever the pointer holds
  stays on it. The hints say what Shift does in this mode; the place
  button's tip names Shift and Ctrl.
- Taking in creative (L-360): the take keys, over Lamium's lists and the
  recipe view (not the game's slots, maintainer 2026-11-24), put one
  or a stack onto an empty pointer, else into the inventory. At the
  inventory and the crafting table only: the item is taken from the
  creative list's search cells, matched by its key and filled by a
  refresh where it has none (`CreativeTake.cpp`, its own capability).
  Where nothing is taken a yellow notice over the hints says why: not at
  this screen, hidden by the game's own search, or not a creative entry.
  Outside creative the keys do nothing and are not in the hints.
- "Place" (maintainer 2026-11-18, [L-165](backlog/L-165.md)): at the
  inventory and the crafting table a crafting recipe that fits the grid is
  laid in, one of each (one more onto the same recipe), with Shift as many
  as divide evenly, and the view closes; crafting stays the player's.
  What lay in the grid goes back first. Each cell moves straight from its
  slot (the manager's place-amount), so the pointer keeps what it holds
  (L-381); a move that shows nothing goes through the empty pointer. Any
  of several items: the member "How to make" names, else the most carried.
- Brewing ([L-361](backlog/done/L-361.md)): the game's table holds no brewing,
  so after the recipes the game is asked, a few dozen a tick, what each
  potion ingredient (and gunpowder and dragon's breath) makes of each
  potion, splash and lingering potion by aux; each answer is a recipe of
  the brewing stand in its own tab. Its face follows the stand
  ([demos/brewing-view.html](demos/brewing-view.html), F): blaze powder in
  the fuel slot at the top left, the fuel's tube through a short coil
  into the gauge, the ingredient at the top with three tubes to the
  bottles, the bottle before at the left, the middle one empty, the one
  after at the right. The empty bottle is the game's own image at its
  size; the rest is drawn on whole units after the game's. A plan counts the ingredient once a batch of three
  bottles and never asks for blaze powder. At the stand a press lays the
  ingredient and the bottle before into the left bottle, Shift into all
  three; the fuel is untouched.
- At a station ([L-380](backlog/done/L-380.md)): the furnace, blast furnace,
  smoker, stonecutter and smithing table take the recipes of their own
  tab (smelting, blasting, smoking, stonecutting, smithing), whose faces
  then have "place". A press lays one in, Shift as much as carried up to
  a stack (the smithing table: as many as divide evenly, one of each
  slot a round), each cell straight into its slot. A slot holding another
  item is emptied into the inventory first, one holding the same item
  gets more; fuel is never touched. The stonecutter's result is the
  player's to pick (Lamium cannot choose it): after Lamium lays its input
  in, a tip in the game's look above the result list's panel says
  "Choose a result" until the list is pressed, the input changes or the
  screen goes. Shown at a station, an item's recipes open on that
  station's tab where it has one (beef at the smoker on smoking). "Can be made" there lists what that station makes
  from what is carried, and a press places it.
- An ingredient any one of several items satisfies (any planks) shows the
  member "How to make" names, else its members in turn with a star on the
  icon's corner (on every Recipes screen); it is named by its tag
  ("Planks") or "Raw Gold etc.", and under the pointer shows its members.
- A station's recipes (R, a left press) are how it is made; its uses (U, a
  right press) first what it does, only the recipes it can do, its
  heading naming all the process's stations as elsewhere, then the
  recipes it goes into, none twice.

### Mass Craft
A feature of its own that works with Recipes off: [MASS-CRAFT.md](MASS-CRAFT.md).

### The look, reviewed with the maintainer (2026-11-19)
What bothered is in VALIDATION-LOG of that day; what was agreed is built:
- "One of several" as DESIGN "Controls" has it: the recipe view's
  stations, the plan's window, the panel's lists (pictures where the panel
  is narrow), the item list's "all / can be made".
- The recipe view's slots as the game draws its own, a missing
  ingredient's face on the game's red. "Favorite" and "wanted" are two
  joined buttons, green when on (chosen over a name and a switch and over
  icon toggles), at the heading's right end.
- A Lamium window's grey covers the whole span between the two panels, so
  the game's window with its recipe book out does not show beside it.
- The panel's list and the plan window's lists scroll: the wheel, and the
  shared bar (`listScrollbar`). The item list on the right keeps pages.
- The open container's store button is on the game window's title line,
  left of its close button (L-382; [PLAN.md](PLAN.md)).
- An item Lamium draws says under its name what the game says of it,
  with Lamium's item details, in the game's tooltip box: the stack's own
  hover text, which Lamium's hook has added to, each line in the color it
  starts in, without the name and lines of icon glyphs (`hoverLines`). A
  row of the plan's window also says its count in chests and stacks.
- Settings (L-373): Recipes (experimental, off by default) with
  "Favorites" (on): without favorites, no favorites in the panel, no
  marks, no button, and A does nothing. The Plan and the stores are
  features of their own ([PLAN.md](PLAN.md)); the panel shows while any of
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
- `ItemKey.h`: an item's key: name, the aux where the table tells variants
  apart (`settled()`), and what makes a book, rocket or banner what it is
  (`identityTags`, L-363), from a stack by `StackKey.cpp`, kept from a slot
  the list lacks; `findKey` drops it. A listed key draws as its creative
  entry (`listEntries`: a star's color), with its glint.
- `BandLayout.h` (pure, L-374): the panels as regions beside, above and
  below the game's window, panels split along a region, one page to a
  panel; the layout is kept in the settings (`inventory.bands`, today's
  look by default). `BandEditor.h` (pure, L-375): where a dragged panel
  goes and the layout after the drop; `BandEditorDraw.cpp` draws the
  editor over the inventory, which the "Panel layout" link rows
  (Recipes, Plan) open after closing the settings. A page whose feature is off is
  left out; each panel keeps one row; a region too low for one row lends
  its panels to a side on that screen. `BandPages.cpp` draws the
  favorites, the plan's list and the open container's line, which lies
  in the left region's last panel.
- `ItemListLayout.h`, `RecipeView.h`, `ListFilter.h` (pure): where the
  lists, the search field and the view sit, an item's recipes by kind, a
  recipe's slots, the search and "can be made". `ItemList.cpp` is the host:
  it draws both panels and the field after a game screen and keeps input
  from the game (the listeners in `input/CustomInput.cpp` and the text hook
  in `ui/SettingsScreen.cpp` ask it; what they keep acts in the next
  frame). `RecipeViewDraw.cpp` and `PlanViewDraw.cpp` draw the two windows
  and do not know their host. `CountEdit.h`: the count being typed.
- `TakePress.h` (pure; `tests/TakePressTests.cpp`): what Shift + press does
  per list and mode, where a taken item goes, why nothing was.
  `CreativeTake.cpp`: the take through the creative list's cells.
- `PlacePlan.h` (pure): which carried stacks go into which grid cell.
  `RecipePlace.cpp` does it with the slot calls Item transfer uses, behind
  its capability; Mass Craft's run too ([MASS-CRAFT.md](MASS-CRAFT.md)).
- `RecipeNotes.h` (pure; documents in `RecipeNotesStore.cpp`): favorites,
  chosen recipes, picked members and items marked as gathered, for all
  worlds in the config folder's `recipes.json`; the wanted items per world
  in `wanted.json` beside its waypoints, the counted places (`Places.h`,
  pure) in `places.json` (for a server `config/places/<key>.json`); `RecipePlaces.cpp` reads the open
  container behind the CountedPlaces capability. `RecipeState.cpp` loads and saves
  them and works the plan out. A file that cannot be read is left as it is
  and the changes are kept for the session only.
- Features hand targets over through `state::provide` (a function the
  plan asks twice a second); `schematic/PlanHandOver.cpp` is the
  schematic's, from the last check's material lines of the selected
  placement (none while another placement's pass is the last)
  (`Verification.h` `planTargets`).
- `recipe_trace` (`src/features/research/RecipeTrace.cpp`) writes the table
  to a file and logs sample plans.

## Known
What the client's table holds and how it reads: [L-116](backlog/L-116.md)
"Known" and VALIDATION-LOG 2026-11-17.

## Limits
- The search field takes no pasted text.
- Crafting onto the pointer: a server is unchecked.
- "Place": a server is unchecked. Shift + place with Item transfer on and
  an inventory slot under the pointer was not tried.
- Taking in creative: a server and a held item are unchecked.
- Not built: held members first in a group's turn. Their own items: what a
  recipe gives back, a cake's buckets ([L-362](backlog/L-362.md)), a
  listed item's own data ([L-363](backlog/L-363.md): no potion effect).
- A key with data has no recipes but a rocket's (`rocketData`: its stars);
  a bare one stands for its least variant. Materials counts without data.
- Trades and drops are not recipes.
- Checked on a local world and one server built on Bedrock Dedicated
  Server; other servers and Realms are not.
- Mockups: [demos/recipe-viewer.html](demos/recipe-viewer.html),
  [demos/materials-raw.html](demos/materials-raw.html).
