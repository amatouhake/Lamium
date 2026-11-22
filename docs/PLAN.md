# The plan and the stores

Two features that work alone or with Recipes ([RECIPES.md](RECIPES.md))
since L-373: the **Plan** traces what features and the player want down to
what must be gathered and what to make; the **Stores** are the containers
counted as held, for the plan and for Schematics. Both are in the Inventory
section, experimental and off by default. The plan's design is
[L-359](backlog/L-359.md) and [L-370](backlog/L-370.md); the rules of a
plan's recipes are in [L-116](backlog/L-116.md).

## Behavior

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
- The left panel's short list shows the wanted items, what to gather or
  what to make, named as the window's tabs; a press on a line opens the
  window there. The window sits where the recipe view does; the
  recipe view opens over it (a press on an item, while Recipes is on) and
  gives it back.
- The window ("Notes", L-382) is one for the three features, its tabs by feature, each
  shown while its feature is on (L-373): "wanted", "to gather" and "to
  make" (the Plan), "How to make" (Recipes), "stores" (the stores). It is
  always as wide as the game's inventory with its book open (the panels
  narrow while it is open). The bottom row reads "open the window,
  arrange the panels | search field, highlight", as wide as the
  inventory, or up to a button more each side where the panels leave the
  room (the field takes it), never over them: the first button (a tabbed window)
  opens it on its last tab or closes it, green while open, its tip the
  name and the tabs; the second (a divided square) arranges the panels, green while it
  does, and a press on it again ends that as Done does.
  Buttons with pictures are white and greys, green when on. The settings
  screen holds how the features behave, the window the player's data.
  A switch line has its switch right after its words. Count columns
  take six digits. Only column headings stay put; a tab's switch line and "All" scroll
  with the rows; every tab's scrollbar spans the same height, 3 units
  from the rows and from the window's shadow, drawn in whole units.
- "Wanted" is a table: fold, icon, name, held, needed, a bar of held
  against needed (what is gathered shows in "to gather"). On every tab
  what comes before the table takes the left 45% at most; in the rest,
  ending 8 units short of the scrollbar, held and needed stand close, the controls' column is as wide as
  the tab's widest control and the bar takes the rest; every control is
  always shown. Items all held follow under "done", faint. The
  player's own count sits on a faint sunk box: a press types it in place
  with the search field's keys (all selected at first; Enter keeps it), the wheel steps it; "x" stands in the
  controls' column; handed rows show their source there; a hand-added and a handed row of one
  item stay two rows. Held stock goes to the rows in
  the order added; a row that lost stock to one above says so under the
  pointer. A press opens its read-only breakdown: steps as slots one indent
  in, the products in one column after the most inputs' room, then
  what it needs gathered on the table's held, needed and bar columns,
  "gather" and "make" in the controls' column. A recipe's state (on every step made, a recipe or
  not to choose from) and an any-of input's slot (framed green when
  chosen) go to "How to make"; "gather" on a step, "make" on a
  material set to gather.
- "To gather" uses the same columns, "All" as the first row (a row as
  the others, no icon, a rule under it), then under headings what is still to gather (most left first) and what is
  gathered (faint). "To make" is the work (L-370): under headings the
  steps that can be made now, made in part (both from what is carried),
  made after taking out (the stores would cover them; an input's tip
  names the stores, L-385), short (faint,
  inputs nobody holds on the game's red), and held (faint, a check); "can
  make now" and "needed" columns in items with a bar. A press opens the
  breakdown. Under the pointer a step offers "lay in" (the recipe view's
  picture and presses: one round, Shift what is still needed, Ctrl one
  round onto the pointer) and "make" (what is still needed: rounds laid in
  and taken as a Shift click takes them, again, the product kept; at a
  station only laid in), both only where the open screen takes it, else
  greyed with where it is made; the first heading's "Make all" makes
  every step of the open crafting grid in the plan's order, each as far
  as what is carried allows, in part too, so one step's product feeds
  the next; on the panel it is the make picture over the rows' make
  buttons, on the column headings' line. The panel's lists keep the
  window's order and its columns (demos/panel-table.html A): under a line
  of headings, held (or can make now) and needed in six-digit columns, no
  bar, controls always shown: "wanted" the count in a box typed or
  stepped as in the window and "x"; "to make" the two buttons, a stores'
  chest before the counts of a step to take out for (the panel has no
  headings). Done rows at the end, checked, kept in place while the
  pointer is over the list; the panel lists the same rows as the window
  (gathered ones too, faint).
- Tips say only what their row does not show, in the window and on the
  panel alike (L-387); a count cut to fit shows whole under the pointer.
  A "to gather" row: its name, a group's members and the stores holding
  some. A wanted row: its name, whose a handed row is, a share taken
  above. A step's product: its name and the inputs short of what is
  carried, "Logs: 2 more". An input: its name, "Held: 0 (2 more)" (its
  slot shows what is needed) and the stores holding some. "To make"'s
  columns read "Can make" and "To make" (作れる / 作る). A cut count's
  tip is the number alone, never the row's item lines.
- Marks: a favorite carries none (it stands in the favorites' panel); a
  wanted item its count; a chosen candidate in "How to make" a green
  frame in the unit outside its slot; a group's slot always its "*",
  chosen or not; the member a group's tip shows a white frame.
- A group (an ingredient any of its members serves) holds still on
  every recipe screen: the item whose uses are shown, else the member
  chosen, else the one most held, else the one it is known by; its tip
  frames the same member the slot shows. Resting the pointer on its picture (slot or icon, not the rest of
  its row) a moment, the
  wheel looks through the members (it chooses nothing); its tip names
  the one shown, says whether one is chosen ("Any" explains the kinds
  held count together) and shows the members full size, the shown one
  framed white and the one chosen green over it, with no counts held;
  no item's own lines (fuel, id) follow. The name needs no "*": the
  lines say it is a kind. In "How to make" a line's left shows the
  group's or item's tip; on its right each picture names itself. A group's
  members stand in the item list's creative order on every screen.
- Tips describe the item alone; what a press or the wheel does there
  ("Wheel: look through the kinds", "Change it in How to make") is
  added to the window's hint line while the pointer is on it.
- A group "any" is traced past its kind where most of its members are,
  by their usual recipe, made from one input in one amount (L-386):
  planks from logs (every log, wood, stripped one, stem and hyphae: the
  game's "logs" tag), four from one; wooden slabs from planks. Held members count
  first; "to make" has the group's step, which takes the kind and makes
  the member whose input is most carried, then the next. Ore smelting
  and loops (cobblestone, nuggets) do not line a member up. A group can
  be gathered as it is instead ("gather" above its members in "How to
  make", or on its breakdown step).
- "How to make" holds the choices for all plans and worlds, in the item
  list's creative order (a group by the member it is known by),
  each line's state on one line (an item's: the way in use as its row
  below shows it, in full ink, with "automatic" while the rule
  decides); lines for the current plan, or with a switch all chosen
  before. Placing follows it, else what is held (L-389,
  demos/howto-choices-3.html, demos/howto-lines.html): words one line;
  an item's station and words each keep one column, a group's words
  follow its pictures. The arrow stands as far right as the longest
  words allow, so the longest recipe (five inputs) shows whole and the
  name gives way. A group opens "<kind>から作る" where traced, "<group>を集める",
  then its members as slots (a press decides one; its own ways are then
  its line under "Recipes"). An item opens "automatic" (the rule's way,
  faint, the word alone), "gather" (the breakdown's "gather" and "make"
  set the same), then its recipes as slots and station without words,
  the rule's first, re-dyeing folded as "染め直し（N）" (its rows in line
  with the others). Rows have no tip of their own; each picture names
  itself. Items usually gathered
  (leather, white wool, anything smelted from an ore that drops it:
  `usuallyGathered`) have no automatic recipe, as a loop's base: they
  read "Automatic: gather" and are made by choosing a recipe; the notes
  keep only the player's choices. In a loop of items made from each
  other the one a furnace turns into another of it is the base, as mined:
  cobblestone is gathered and stone smelted from it. An item has a line
  where the plan makes it ("gather" is always a choice), where it is
  gathered with a recipe that could make it, or where the player chose.
  No search (maintainer 2026-11-22): a line is found from a breakdown's
  state tag. Once one is needed, the window gets a field of its own with
  the item list's search language, not the item list's bar, so the
  window does not hang on the inventory and one field does not narrow
  both.
- A key (W by default, beside A for favorites; both rebindable) puts the
  hovered item into "wanted" or takes it out, on item screens and in the
  recipe view, with Recipes on or off. While it is held the wheel changes how many are wanted, so the
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
- Kept between sessions: only what the player enters (recipe choices, group
  picks, "stop here" marks), in one file for all worlds; an entry the
  current world's recipes lack is ignored, not deleted.
- The schematic Materials tab shows what is left as things to gather, no
  more. The steps to make belong to the plan.
- Which recipe makes an item by default, groups, fuel, spare output and
  held items: the rules in [L-116](backlog/L-116.md).
- Its key is W; its pages are the panel's lower part on the left. With
  Recipes off the panel holds the plan alone, there is no item list or
  search field, and a press on an item opens no recipe view.
- The wanted tab has "Include what the schematic still needs", the
  switch of the schematic's Materials tab, while Schematics is on; the
  settings screen has it on the Plan's row (the same saved value).

### The stores
- With the stores off, what is held is what is carried. With them on it
  is the sum over the places counted, each with its switch in the stores
  list: what is carried (on unless the player turns it off, so a plan can
  be served from a store alone; one place among the others, kept with the
  world's places, L-373); the ender chest from the first time it is seen
  open; and the containers the player makes stores (a chest, barrel or
  placed shulker box; a double chest is one store by either half; a
  minecart's chest is none). A store counts what it held when it was
  last seen open, and is noted while it is open and the stores are on. One
  count serves the plan, the schematic Materials tab and the Schematic
  HUD; Enchantment Order and "can be made" keep counting what is carried.
- What a shulker box or a bundle holds counts with it, carried or in a
  store; there is no switch for it. A bundle keeps only an id, and what
  it holds is read through an open item screen: it is read whenever one
  is open and remembered by the id, so the plan, which only shows on
  item screens, has it as it is, and the HUD and the Materials tab have
  what the last item screen saw (nothing before the world's first).
- While a container is open, a button on its window's title line, left
  of the close button and drawn as that is (15 units, a chest in the
  close button's mark color, a green check while on, the close button's
  face under the pointer; its tip
  in the game's box, "Count as a store: on/off"), counts it as a store: on makes it a
  store, off takes it out; at the ender chest it switches the chest in
  or out of the count. The window has a "Stores" tab: what is carried, the ender chest and the
  stores, each a name and a switch in one column, a store also with
  "Remove". A store where another block stands,
  or none, is dropped: looked at once a second within 48 blocks, in
  loaded chunks, and when the container is opened. The word is "store" throughout.
- In "to gather", a "have" number that includes a place's last-seen count
  is starred and says under the pointer where the items are.
- The stores are shown in the world within 48 blocks as faint faces in
  the accent color with their outline, as a shape is (L-369), by the world
  overlay (`WorldOverlay.cpp`, from `storeMarks`), while
  "Outline stores in the world" (on) is on. They show once the world's
  places were read, which the first item screen does.

## Code
- The plan: `RecipeIndex.h` `plan()`, `RawView.h`, `RawMaterials.h` (pure;
  `tests/RecipeIndexTests.cpp`, `tests/RawMaterialsTests.cpp`);
  `RecipeState.cpp` works it out; `PlanViewDraw.cpp` draws the window;
  `ItemList.cpp` hosts the panel ([RECIPES.md](RECIPES.md) "Code").
- The wanted items per world in `wanted.json` beside its waypoints, the
  counted places (`Places.h`, pure) in `places.json` there;
  `RecipePlaces.cpp` reads the open container behind the CountedPlaces
  capability.
- Features hand targets over through `state::provide` (a function the
  plan asks twice a second); `schematic/PlanHandOver.cpp` is the
  schematic's, from the last check's material lines
  (`Verification.h` `planTargets`).

## Limits
- With the stores on and the plan off, the stores list (to remove a store,
  or switch what is carried) is not reachable; the open container's line
  still marks and unmarks a store.
- The wanted items' and the counted places' files go when a world's or a
  server's saved data is deleted, but the Saved data screen does not count
  or show it, and moving a server's data to another key leaves it
  ([L-358](backlog/L-358.md)).
- The split (L-373) is not checked in game.
- Mockups: [demos/recipe-plan.html](demos/recipe-plan.html) to
  `recipe-plan-4.html`, [demos/plan-window.html](demos/plan-window.html).
