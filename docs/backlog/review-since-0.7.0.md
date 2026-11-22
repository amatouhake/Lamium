# Review of the changes since 0.7.0: the findings in full

The detail behind [L-390](L-390.md). Reviewed on 2026-11-23 at `69271bd2`:
`git diff v0.7.0..HEAD -- src tests`, in three ranges, and every finding
read again at `72776881` (2026-11-23). A finding leaves this file when it
is fixed; what stays is deferred, each with why it waits.

"Read": confirmed by reading the code. "Likely": the code allows it, not
shown. "Game" / "Server": only a run can confirm it. Nothing here was
seen in game. Line numbers are `69271bd2`'s unless a function is named.

## Range 1: recipes, plan, stores, Mass Craft

### 1-7 Low: "make" counts rounds as made once the grid reads empty
`RecipePlace.cpp` `tickMaking`, the Take phase. It tests `gridEmpty`
before any take is sent, so a lay the server refuses and rolls back counts
as made with no take at all: `step.rounds` drops, the step may go, later
steps lack their inputs and the run ends "done". Nothing is lost.
Likely; Server. Deferred: to the server check (L-165 "Next"); a fix counts
only after a take and confirms by the product in the inventory.

### 1-8 Low: the 16 slot calls a step are not kept on every path
`RecipePlace.cpp` `massCraft::advance`. The surplus drops (about thirty in
a frame with no slot free, at most once a second) are not charged; after
the budget is spent in a take, the room drop and the take still go (about
two over); `clearPointer`'s few calls are uncounted. "make" has no budget
but one apply a tick, the same envelope. (A cell laid by the pointer is
charged three: safe, only the comment is off.) Read; Server. Deferred:
with L-165, before Mass Craft is offered for servers.

### 1-12 Low: the plan's shares are worked out without a bound
`RecipeState.cpp` `shares()`. VALIDATION-LOG has 8.2 ms for 29 entries; a
schematic hands over its distinct materials (capped at 2048, in practice
tens to a few hundred), at most twice a second while held counts change.
Read (path); costs extrapolated. Deferred: measure first (the "shares of
N took" log line with a large schematic); a fix is a resumable pure
`targetPlans` and a choice of what the rows show meanwhile.

### 1-19 Low: UI rules (Design)
`PlanViewDraw.cpp` `stateTag` and the window's count field are built in
the screen's file. `drawFieldBox` has no window look, so the field needs
a window variant in `ListViewWidgets`, and moving the tag to the framed
`drawTag` changes its look. Read. Deferred: a Design question (AGENTS
rule 7). The editor's refusal chip and lit tray are `warning` now.

## Range 2: settings screen, hotkeys, input

## Range 3: everything else

### 3-2 Low: an unreadable `servers.json` still names a data set
`WaypointSession.cpp` `withList`. The join goes on with an empty list, so
a world on `<base>-w2`, or a server given other data, records into the
base, with a log line only; the list is never rewritten while
unreadable, so this lasts until the file is fixed by hand. The encoder
has no size cap while the reader refuses past 1 MB. Read (logic);
trigger not measured. Deferred: what a join does with an unreadable list
is a maintainer choice; capping the encoder (dropping the oldest prints)
is the cheap half.

## Questions for the maintainer (decided designs, not findings)

1. "make" takes the output unchecked because the output slot cannot be
   read (L-385). Mass Craft's `makesProduct` reads the grid and the game's
   recipe id, not the output slot: can "make" use it?
2. RECIPES.md reads as if releasing the Mass Craft key puts the pointer
   back and throws the product; `finish()` makes no game call, the tidying
   happens at the next step's start only.
3. A stack the player picks up by mouse during Mass Craft is treated as
   the run's: put away, or thrown if it is the product.
4. Ingredients go by name too: an enchanted bow is laid into a dispenser.
5. A mouse button binding acts while the search field or a count is typed
   (`CustomInput.cpp:215`); SETTINGS-KEYMAP.md says none acts while typing.
6. A binding that takes a fixed press (Sort on Shift + left click replaces
   the transfer gesture) gets no warning; L-372's Open names the game's
   keys only.
7. A category's reset leaves the panel layout (`inventory.bands` has no
   option); "Reset all" resets it.
8. A schematic's remainder leaves the plan while the player is in another
   dimension.
9. "Do not split here" moves a Realm from lent `world_` data to the empty
   `realm_<id>`, as L-365 decided; "Use this data" now leads back.
10. The tray's frame is `dangerEdge` while a panel is dropped on it
    (`BandEditorDraw.cpp:163`): destructive, or a refusal's color?
11. `recipes.json`, `wanted.json` and `places.json` write `"version": 1`
    and never read it.
12. `TransferRoutes` in `PausedFeatures.h:61` does not ask for the Plan;
    whether "make" works with Recipes off was not checked.

## Checked, nothing found

- Action ids: the 158 of 0.7.0 unchanged, 11 appended.
- New hooks are in trace-only files and named in GAME-UPDATES.md;
  `src/features/research/` is empty in a normal build.
- Translations: 2183 rows in four languages, same order and placeholders;
  none of the 108 removed keys is still used.
- New settings have field, option, tolerant load and save, row and four
  languages; `map.textSize` converts once.
- `RealmCookie.cpp` is behind its capability and falls back to the level
  id; `DataMerge.h` overwrites no unreadable region and writes atomically.
- The tooltip hold-back replays without a second deferral; the recipe
  table drops a build for a world that was left; the input listeners touch
  only queued or locked state (beyond what L-353 lists).
- LamiumTests and the three check scripts passed at `69271bd2`.

## Not read, or by diff only

By diff: `InfoHud.cpp`, `EnchantOrderGame.cpp`, `HudEditor.cpp`,
`SchematicsView.cpp`, `WorldMap.cpp`, `Minimap.cpp`, `WorldOverlay.cpp`,
`Options.h`, `Settings.h`, `Widgets.*`, `ListViewWidgets.*`. In part:
`ItemList.cpp` 715-1030, `SettingsScreen.cpp`, `tests/RecipeIndexTests.cpp`.
The four trace-only files by their `#ifdef` structure.
