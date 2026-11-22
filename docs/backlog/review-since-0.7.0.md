# Review of the changes since 0.7.0: the findings in full

The detail behind [L-390](L-390.md). Reviewed on 2026-11-23 at `69271bd2`:
`git diff v0.7.0..HEAD -- src tests`, in three ranges. Nothing here is
triaged or agreed work. A finding leaves this file when it is fixed; what
stays is what is open.

"Read": confirmed by reading the code at that commit. "Likely": the code
allows it, not shown. "Game" / "Server": only a run can confirm it. Nothing
here was seen in game. Line numbers are that commit's.

## Range 1: recipes, plan, stores, Mass Craft

### 1-2 Medium: Mass Craft can throw the player's own stack of the product
`src/features/recipes/MassCraftPlan.h:75` (`excess`), `RecipePlace.cpp:666` (`keep`), `RecipeIndex.h:151`.
`keep` is a count, not the stacks. Items are told apart by name (aux only
where the table has variants), so wear, enchantments and names do not
count. Throws go from slot 35 back: carrying an enchanted iron pickaxe and
mass crafting iron pickaxes keeps only the lowest slot's, which may be a
new one. Same for firework rockets and named stacks.
Read (logic); Game: which slot a Shift take fills. RECIPES.md says what was
carried stays; its "Limits" says items differ by name and aux only.
`tests/RecipeIndexTests.cpp:557` checks counts only.

### 1-4 Low (rest): Mass Craft beside a round crafted onto the pointer
`src/features/recipes/RecipePlace.cpp` `massCraft::begin`. It now ends
"make" and a waiting place (`place::stop`), which leaves a round being
crafted onto the pointer going on; Mass Craft starts only with an empty
pointer, so the overlap is that round's Fetch step. Read.

### 1-5 Medium: a quick tap of the wanted key leaves it held
`src/features/recipes/ItemList.cpp:1091`, `:1166`, `:1064`.
The press goes through the action queue to the client thread; the release
is handled in the listener. A release that arrives first finds nothing to
clear, then the press sets `wantedHeld`. From then on every wheel notch on
item screens steps that item's wanted count and the lists do not page,
until the key is pressed and released again or the focus is lost.
Read (ordering); Game: how often a tap beats the queue.

### 1-6 Medium: the item list's search keeps the old names
`src/features/recipes/ItemList.cpp:251`, `:308`, `src/ui/GameLanguage.cpp:60`.
The folded names, English names and tooltip texts are rebuilt only after
the world is left. A language change in a world leaves the search in the
old language; English search switched on in a world finds nothing by
English name.
Read.

### 1-7 Low: "make" counts rounds as made once the grid reads empty
`src/features/recipes/RecipePlace.cpp:449`. A server that refuses and puts
the inputs back empties the grid too: the step is dropped as done and the
log's rounds are wrong. Nothing is lost. Likely; Server.

### 1-8 Low: the 16 slot calls a step are not kept on every path
`src/features/recipes/RecipePlace.cpp:889`, `:777`, `:786`, `:443`.
The surplus drops are neither charged nor counted in the log (some thirty
in a frame); after the budget is spent in a take, the room drop and the
take are still sent; `clearPointer` is uncounted; a cell laid by the
pointer is charged three, not two; "make" has no budget at all.
Read; Server (L-165 "Next" asks the same).

### 1-9 Low: a waiting place has no stale check
`src/features/recipes/RecipePlace.cpp:36`, `:360`. While it waits for the
grid to empty, a screen closed by its X button or by the server leaves
`going` set; the next screen of that name gets the old recipe laid in.
"make" (300 ms) and craft onto the pointer (500 ms) have one. Read; rare.

### 1-10 Low: a wanted change can wait for the next item screen to be saved
`src/features/recipes/RecipeState.cpp:86`, `:150`. Changed within 0.8 s of
closing the screen, the list is written only when an item screen is drawn
again or Lamium stops; a crash before that loses the change. Read.

### 1-11 Low: `places.json` is rewritten on every change of an open store
`src/features/recipes/BandPages.cpp:334`, `RecipeState.cpp:171`. Up to four
writes a second on the client thread; the wanted list waits 0.8 s for this
reason. Read (frequency); Game: the hitch.

### 1-12 Low: the plan's shares are worked out without a bound
`src/features/recipes/RecipeState.cpp:319`. VALIDATION-LOG has 8.2 ms for
29 entries; a schematic may hand over 2048. Read (path); the larger costs
are extrapolated.

### 1-13 Low: wanted marks look into a list edited in the same frame
`src/features/recipes/ItemList.cpp:658`, `BandPages.cpp:289`. The marks'
map is keyed by `string_view`s into `Wanted::targets`; "x" erases from it,
and panels drawn after that compare against moved strings (undefined
behavior, in practice a frame of wrong marks). Not with the default layout.
Read.

### 1-14 Low: the panel editor survives a screen the game closes
`src/features/recipes/ItemList.cpp:436`, `:504`, `BandEditorDraw.cpp:77`.
Closed by death or the server while arranging, the same screen reopens
with the editor on, and a drag in flight is saved. Read; Game: which
closes.

### 1-15 Low: the carried bundles listener looks at no setting
`src/features/recipes/RecipePlaces.cpp:124`. Four times a second on any
container screen it walks the inventory, shulker boxes and bundles, with
Recipes, Plan, Stores and Schematics off, with Lamium disabled, and
without a capability. `dropMissingStores` beside it checks both. Read.

### 1-16 Low: a `$` search reads tooltips unguarded, and the failure is silent
`src/features/recipes/ItemList.cpp:259`, `:1203`. An item whose hover text
throws is asked again every frame; the list, panels and windows stop
drawing and nothing is logged. Likely; Game.

### 1-17 Low: a group's wheel position is kept across recipe tables
`src/features/recipes/GroupLook.cpp:24`. Kept by group index and never
cleared: in a world whose table numbers groups otherwise, another group
shows a turned member. Read.

### 1-18 Low: the schematic's hand-over lacks the "ready" test
`src/features/schematic/PlanHandOver.cpp:16` (the HUD has it,
`InfoHud.cpp:360`). After another placement is selected, or an earlier one
removed, the plan shows the previous remainder or a wrong name until the
next pass ends. Read.

### 1-19 Low: UI rules
`src/features/recipes/PlanViewDraw.cpp:358`, `:609`, `BandEditorDraw.cpp:191`.
A pressable state tag and the window's count field are put together in the
screen's file (the panel's field is the shared `drawFieldBox`). The
"cannot go here" chip is `palette::danger`, which DESIGN keeps for
destructive actions; a refusal is `warning`. Read.

### 1-20 Low: tests
`tests/RecipeIndexTests.cpp:571`: "the recipe the game names is taken"
passes with the name ignored, since one recipe fits the fixture.
`PlanViewDraw.cpp:109` (`makeAllRuns`) is logic in a .cpp without a test.
Read.

### 1-21 Low: documents
`docs/PLAN.md:174` describes the shared stepper for a wanted count; the
code has the count box and "x" (as the same document says earlier).
`docs/RECIPES.md:382` says stations have no "place" (L-380 built it). Read.

## Range 2: settings screen, hotkeys, input

### 2-1 Low: a click on read-only key caps toggles the row's switch
`src/ui/SettingsTableView.cpp:470`, `:1291`. On the settings screen a click
in the key column of an Item transfer gesture row switches the gesture;
SETTINGS-KEYMAP.md says a press does nothing. Read. The click rule is from
before 0.7.0, the caps are new.

### 2-2 Low: the "fires and conflicts agree" test cannot fail
`tests/BindingTests.cpp:83`. Both sides come from `whereActs`. Not
covered: two layers at once, and the second gate in
`src/input/Actions.cpp:219`. No wrong behavior today. Read.

### 2-3 Low: Windows' key name is asked without the extended-key bit
`src/app/Desktop.cpp:61`. An extended key the game only calls "ButtonNNN"
would get its non-extended twin's name or none. Likely; Game.

From before 0.7.0, outside this review: an armed reset stays armed over
closing and reopening the settings (`src/ui/SettingsTableView.cpp:387`).

## Range 3: everything else

### 3-1 Medium: "Use this data" does not last where the identity is not split
`src/features/map/WorldPrints.h:75`, `WaypointSession.cpp:660`. `pick`
returns the base for an unsplit identity before it looks at the rows
`use` records. On a Realm or friend's world answered "Do not split here",
choosing the old `world_<hex>` data works until the next join, then goes
back to `realm_<id>` without a word. Nothing is deleted. This is the
migration L-358 describes. Read (pure; a unit test reproduces it). A server
by address is not affected.

### 3-2 Low: an unreadable `servers.json` still names a data set
`src/features/map/WaypointSession.cpp:124`, `:190`. The join goes on with
an empty list, so a world that uses `<base>-w2` records into the base,
with a log line only. Read (logic). How the file becomes unreadable (a
world name long enough to pass the 1 MB read limit) was reported, not
checked.

### 3-3 Low: a new world can be given a `-w<n>` key that still holds data
`src/features/map/WaypointSession.cpp:64`. `held()` counts waypoints,
placements, the death layout and the map, not shapes or wanted items, so
such a key reads as unused. Data holding only wanted items is not listed
on the Saved data page. Read.

### 3-4 Low: a merge that fails part way leaves the placements stale
`src/features/map/WaypointSession.cpp:844`. A failed shapes write returns
after the merged placements were written and before the world generation
moves; the next placement commit overwrites them. Merging again restores
them. Read; needs a write failure.

### 3-5 Low: the minimap keeps two `static ItemStack`s
`src/features/map/Minimap.cpp:1018`. The only ones in `src`; other icons
are copies for the frame (rule 2). Likely; Game: across a rejoin.

### 3-6 Low: the paused notice names Recipes for Schematics alone
`src/app/PausedFeatures.h:77`. `RecipeTable` still asks
`schematic.enabled`; the Materials tab no longer uses the table. Read; the
toast needs an unverified game build.

### 3-7 Low: icon kind caches go by numeric id across worlds
`src/features/inspection/render/ItemIcon.cpp:89`, `:112`. Never cleared;
two worlds giving one id to different add-on items mix them up. Likely;
Game.

### 3-8 Low: `shown()` empties its map while references are out
`src/features/schematic/SchematicItems.cpp:57`. Past 16384 keys in a world
it clears; callers hold `Shown const&` across calls. `stacks` in the same
file was changed never to do this. Read; needs a very large add-on set.

### 3-9 Low: leftovers and documents
`src/overlay/WorldOverlay.cpp:83`: `identityFailed` is never set, and
`joiningLocal` is read by the shape trace only. `docs/SCHEMATIC.md:177`:
"carried" now includes bundles and, with Stores on, the stores.
`docs/OVERLAYS.md:220` lists `LocalShapePath.h`, used by
`WaypointSession.cpp` only. Read.

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
   `realm_<id>`, as L-365 decided; with 3-1 no way back is left.
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
