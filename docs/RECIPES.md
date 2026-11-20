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
- The game's tooltip for one of its slots is drawn over the bands: while
  the list shows, Inspection holds the game's tooltip call back and makes
  it again after the bands are painted. A slot under an open Lamium
  window has none, nor a container preview. The bundle's tooltip is a UI panel and still goes
  under the bands.
- The agent's choices while building, to confirm: the search takes the
  name as the game shows it or the item's id, every word typed must be in
  one of them, letters match in either case; "can be made" is a crafting
  recipe that what lies in the inventory itself (not what shulker boxes
  and bundles hold) makes once, without making anything first, and that
  fits the screen's grid (any size where there is none; L-367); the wanted mark being the count; the left band is
  halved between favourites and wanted; favourites are kept with the notes
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
- Stations are tabs on the window's top edge: one station's recipes to a
  view, each the same size. A tab is the station's block as an icon,
  named under the pointer.
- A two-line heading: the item's icon and name across the width (under
  the pointer, the item's tooltip, so a name cut short can be read) with
  its own actions at the right end, a favourite button (the key works
  too) and "wanted"; under it the station's name and the pager (previous,
  "1 / 3", next, as wide as two-digit pages so the buttons stay put). The wheel turns pages too. Recipes carry no favourite
  button of their own: any item in a recipe can be opened, and the key is
  there.
- Every tip over the view or the plan window is the game's tooltip box.
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
  member "How to make" names, else its members in turn; under the pointer
  it names the group and shows its members as small icons.
- Every station that can do a recipe is named, as icons.

### The plan
- Its words name what the player sees. The list the player fills is
  "wanted" (the panel's title, the window's title and its first tab; the
  recipe window's button is "add to wanted"); what comes out is "to gather"
  and "to make". On a row, "gather" moves an item that would be made to the
  gather list and "make" moves it back. "Back to any" undoes a picked
  group member.
  "Plan" stays the feature's name in settings only.
- There is one plan, built around the wanted items (L-370): it says how
  far each is, what is left to gather for all, and what to make. Several
  named plans are not built until one proves too few.
- Targets come from features (a schematic's materials still to place,
  while "Put what is left into the plan" on its Materials tab is on: a
  saved switch, off unless the player turns it on) and can be added by
  hand. Hand-added targets are saved per world, under the
  waypoints' key ([MAP.md](MAP.md)). A target a feature handed over cannot
  be edited; it looks like the others and says where it comes from.
- The left band's short list shows the wanted items, what to gather or
  what to make, named as the window's tabs; a press on a line or "Open"
  opens the window there. The window sits where the recipe view does; the
  recipe view opens over it (a press on an item) and gives it back.
- The window ("wanted", "to gather", "to make", "How to make",
  "stores") is the Recipes feature's (L-370), always as wide as the game's
  inventory with its book open (the bands narrow while it is open); with
  the plan off it holds only "How to make", from a recipe's in-use mark.
- "Wanted" is a table: fold, icon, name, held, wanted, a bar of held
  against wanted (what is gathered shows in "to gather"). The player's
  own rows turn "wanted" into "- N +" and show "x" only under the
  pointer; handed rows show their source. Held stock goes to the rows in
  the order added; a row that lost stock to one above says so under the
  pointer. A press opens its read-only breakdown: steps as slots, then
  what it needs gathered. A recipe's state and an any-of input's slot (framed green
  when chosen) go to "How to make"; "gather" on a step, "make" on a
  material set to gather.
- "To gather" uses the same columns, "All" as the first row; done rows
  faint below. "To make" is the work: steps that can be made now from
  what is held first, the others faint; a press opens the breakdown.
- "How to make" holds the choices for all plans and worlds: a group's
  member (a grid of slots, "any" first) and an item's recipe (rows,
  "automatic" first), by name; lines for the current plan, or with a
  switch all chosen before. Placing follows it, else what is held.
- A key (W by default, beside A for favourites; both rebindable) puts the
  hovered item into "wanted" or takes it out, wherever the favourite key
  works. While it is held the wheel changes how many are wanted, so the
  count is set where the item was added; an item that was wanted goes
  out when the key is released without the wheel having turned (a key
  of several inputs or a mouse button takes it out at once, and has no
  wheel); an item taken out keeps its count until the world is left.
- A wanted item's count is changed with the shared stepper (- N +), on the
  short list and in the window alike: minus at one takes the item out at
  once; the wheel (also with the wanted key held over the list) takes it
  to 0, shown faint, and it goes out once the pointer leaves its row or
  the key is up, so a turn too far can be turned back (maintainer
  2026-11-19, L-367). A Shift step stops at one; Shift steps to the next
  multiple of the item's stack, 1 to 64 to 128. A press on the number types it
  (Enter or a press elsewhere keeps it, Esc does not; 0 takes the item
  out).
- "To gather" lists everything to gather: what the recipes ask for and,
  after it, wanted items the recipe table does not know because no
  recipe makes or uses them (a grass block), gathered as they are.
- What is held is the sum over what is counted: what is carried (a
  switch, on unless the player turns it off, so a plan can be served
  from a store alone); the ender chest from the first time it is seen
  open; and the containers the player makes stores (a chest, barrel or
  placed shulker box; a double chest is one store by either half; a
  minecart's chest is none). A store counts what it held when it was
  last seen open, and is noted while it is open and Recipes is on. One
  count serves the plan, the schematic Materials tab and the Schematic
  HUD; Enchantment Order and "can be made" keep counting what is carried.
- What a shulker box or a bundle holds counts with it, carried or in a
  store; there is no switch for it. A bundle keeps only an id, and what
  it holds is read through an open item screen: it is read whenever one
  is open and remembered by the id, so the plan, which only shows on
  item screens, has it as it is, and the HUD and the Materials tab have
  what the last item screen saw (nothing before the world's first).
- While a container is open, the last line of the left band, under a
  rule and clear of the screen's edge, is "Count as a store" with a
  switch: on makes it a store, off takes it out; at the
  ender chest it switches the chest in or out of the count. The plan's
  window has a "Stores" tab: what is carried, the ender chest and the
  stores, each a name and a switch in one column, a store also with
  "Remove". A store where another block stands,
  or none, is dropped: looked at once a second within 48 blocks, in
  loaded chunks, and when the container is opened. The word is "store" throughout.
- In "to gather", a "have" number that includes a place's last-seen count
  is starred and says under the pointer where the items are.
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

### The look, reviewed with the maintainer (2026-11-19)
What bothered is in VALIDATION-LOG of that day; what was agreed is built:
- "One of several" as DESIGN "Controls" has it: the recipe view's
  stations, the plan's window, the band's lists (pictures where the band
  is narrow), the item list's "all / can be made".
- The recipe view's slots as the game draws its own, a missing
  ingredient's face on the game's red. "Favourite" and "wanted" are two
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
- Lamium's parts stand still and fade with the game's window as it
  slides in and out, by how far it is from where it rests, as the anvil's
  panels do (`ui/WindowFade.h`; maintainer 2026-11-19).
- The wanted tab has "Include what the schematic still needs", the
  switch of the schematic's Materials tab, while Schematics is on.
- The stores are outlined in the world within 48 blocks, in the accent
  color, by the world overlay (`WorldOverlay.cpp`, from `storeMarks`).
  They show once the world's places were read, which the first item
  screen does.
- Settings, all on by default: "Favourites" and "Plan" (maintainer
  2026-11-19, in place of a switch for the band, which overlapped the
  plan's) and "Outline stores in the world". The band shows while
  either of the first two is on, and one alone has all of it. Without
  favourites: no favourites in the band, no marks, no button, and A does
  nothing. Without the plan: no wanted items, lists, stores or W.

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
- The wanted items' and the counted places' files go when a world's or a
  server's saved data is deleted, but the Saved data screen does not count or show it, and moving
  a server's data to another key leaves it ([L-358](backlog/L-358.md)).
- Trades and drops are not recipes.
- Checked on a local world and one server built on Bedrock Dedicated
  Server; other servers and Realms are not.
- Mockups: [demos/recipe-viewer.html](demos/recipe-viewer.html),
  [demos/materials-raw.html](demos/materials-raw.html).
