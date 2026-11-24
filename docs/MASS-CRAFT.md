# Mass Craft

Crafting one recipe over and over while a key is held, taking the product
and throwing it out, at the inventory and the crafting table. A setting of
its own (Inventory, off by default) with a hold key that has no default;
it works with Recipes off ([L-165](backlog/L-165.md), maintainer
2026-11-18). Recipes and the recipe view: [RECIPES.md](RECIPES.md).

## Behavior
A setting of its own, off by default, and it works with Recipes off
([L-165](backlog/L-165.md), maintainer 2026-11-18).
- It runs on the inventory and the crafting table while its key is held;
  the key has no default, and a key of several inputs matches in any
  order. It starts once the key was held for a time the
  player sets, 0 to 1 s, 0.3 s unless changed, so a brush of the key
  does nothing. Starting ends a plan's "make" and a recipe waiting to be
  laid in, and waits for a round being crafted onto the pointer.
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
  stays, kept by its slot (an enchanted tool or a named stack is not
  told apart by name, so it is never swapped for a new one); everything
  beyond it goes, also what was thrown out and picked up again. A round goes out in one frame where the game shows each
  part at once, the next 50 ms later; a take that found room for a part
  only is followed by the next in the same frame.
- A step makes at most 16 slot calls, all counted (a cell is one straight
  from its slot, three by the pointer, L-381; a round with more cells is a
  step of its own; what is past the limit waits; "make" lays and takes
  once a tick, the same bound), so a run
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
- `src/features/recipes/MassCraftPlan.h` (pure; `tests/RecipeIndexTests.cpp`): the recipe a
  grid lays out, what a take added, the stacks beyond an ingredient's
  share. The run is `massCraft` in `RecipePlace.cpp`, through the slot
  calls Item transfer uses.

## Limits
- Seen in a local world (VALIDATION-LOG 2026-11-18); the throw from the
  pointer and the surplus are not, nor a server.
