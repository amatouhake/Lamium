#include "features/recipes/ItemListLayout.h"
#include "features/recipes/ListFilter.h"
#include "features/recipes/PlacePlan.h"
#include "features/recipes/RawView.h"
#include "features/recipes/RecipeNotes.h"
#include "features/recipes/RecipeView.h"
void check(bool, char const*);
void recipeIndexTests() {
    using namespace lamium::recipes;
    using Raw = std::vector<std::pair<Item, Count>>;
    // The shapes the client's table has (L-116, read in game 2026-11-17), cut down.
    auto cells = [](std::string const& name, int times, bool tag = false) { return std::vector<SourcePart>(times, SourcePart{name, tag}); };
    auto both = [](std::vector<SourcePart> a, std::vector<SourcePart> const& b) { a.insert(a.end(), b.begin(), b.end()); return a; };
    Source source;
    source.tags = {{"planks", {"oak_planks", "birch_planks"}}, {"coals", {"coal", "charcoal"}}};
    source.recipes = {
        {"crafting_table", "stick", -1, {{"stick", 4}}, cells("planks", 2, true)},
        {"crafting_table", "stick_from_bamboo", 2, {{"stick", 1}}, cells("bamboo", 2)},
        {"crafting_table", "oak_planks_from_wood", 0, {{"oak_planks", 4}}, cells("oak_wood", 1)},
        {"crafting_table", "oak_planks", 0, {{"oak_planks", 4}}, cells("oak_log", 1)},
        {"crafting_table", "chest", 0, {{"chest", 1}}, both(cells("planks", 4, true), both(cells("", 1), cells("planks", 4, true)))},
        {"crafting_table", "torch", -1, {{"torch", 4}}, both(cells("coals", 1, true), cells("stick", 1))},
        {"crafting_table", "stonebrick", 0, {{"stone_bricks", 4}}, cells("stone", 4)},
        {"stonecutter", "stonecutter_stonebrick", 4, {{"stone_bricks", 1}}, cells("stone", 1)},
        {"furnace", "furnace_cobblestone", 360, {{"stone", 1}}, cells("cobblestone", 1)},
        {"crafting_table", "iron_block", 0, {{"iron_block", 1}}, cells("iron_ingot", 9)},
        {"crafting_table", "iron_ingot_from_block", 0, {{"iron_ingot", 9}}, cells("iron_block", 1)},
        {"crafting_table", "iron_ingot_from_nuggets", 0, {{"iron_ingot", 1}}, cells("iron_nugget", 9)},
        {"crafting_table", "iron_nugget", 0, {{"iron_nugget", 9}}, cells("iron_ingot", 1)},
        {"furnace", "furnace_iron", 100, {{"iron_ingot", 1}}, cells("raw_iron", 1)},
        {"furnace", "furnace_iron_ore", 100, {{"iron_ingot", 1}}, cells("iron_ore", 1)},
        {"blast_furnace", "furnace_iron", 100, {{"iron_ingot", 1}}, cells("raw_iron", 1)},
        {"furnace", "furnace_iron_axe", 110, {{"iron_nugget", 1}}, cells("iron_axe", 1)},
        {"crafting_table", "bucket", 0, {{"bucket", 1}}, cells("iron_ingot", 3)},
        {"crafting_table", "emerald_block", 0, {{"emerald_block", 1}}, cells("emerald", 9)},
        {"crafting_table", "emerald", 0, {{"emerald", 9}}, cells("emerald_block", 1)},
        {"crafting_table", "cake", 0, {{"cake", 1}, {"bucket", 3}}, both(cells("milk_bucket", 3), cells("wheat", 3))},
        {"crafting_table", "tipped_arrow", 0, {{"arrow", 8}}, both(cells("arrow", 8), cells("lingering_potion", 1))},
        {"crafting_table", "arrow", 0, {{"arrow", 4}}, both(cells("flint", 1), both(cells("stick", 1), cells("feather", 1)))},
        {"deprecated", "old_barrel", -1, {{"barrel", 1}}, cells("stick", 2)},
        // Loops longer than a pair, across stations, and with a second ingredient.
        {"crafting_table", "waxed_block", 0, {{"waxed_block", 1}}, both(cells("iron_block", 1), cells("honeycomb", 1))},
        {"crafting_table", "ingots_from_waxed", 0, {{"iron_ingot", 9}}, cells("waxed_block", 1)},
        {"stonecutter", "cobblestone_from_stone", 0, {{"cobblestone", 1}}, cells("stone", 1)},
        {"crafting_table", "furnace", -1, {{"furnace", 1}}, cells("cobblestone", 8)},
        {"crafting_table", "lever", 0, {{"lever", 1}}, both(cells("cobblestone", 1), cells("stick", 1))},
        {"crafting_table", "dropper", 0, {{"dropper", 1}}, both(cells("cobblestone", 7), cells("redstone", 1))},
        {"crafting_table", "string_to_wool", 0, {{"white_wool", 1}}, cells("string", 4)},
        {"crafting_table", "wool_white_from_black", 50, {{"white_wool", 1}}, both(cells("bone_meal", 1), cells("black_wool", 1))},
        {"crafting_table", "wool_black_from_red", 50, {{"black_wool", 1}}, both(cells("ink_sac", 1), cells("red_wool", 1))},
        {"crafting_table", "wool_black_from_white", 50, {{"black_wool", 1}}, both(cells("ink_sac", 1), cells("white_wool", 1))},
        {"crafting_table", "wool_red_from_black", 50, {{"red_wool", 1}}, both(cells("red_dye", 1), cells("black_wool", 1))},
        {"crafting_table", "wool_red_from_white", 50, {{"red_wool", 1}}, both(cells("red_dye", 1), cells("white_wool", 1))},
        {"crafting_table", "honey_block", 0, {{"honey_block", 1}}, cells("honey_bottle", 4)},
        {"crafting_table", "honey_bottle", 0, {{"honey_bottle", 4}, {"x", 0}}, both(cells("honey_block", 1), cells("glass_bottle", 4))},
        {"crafting_table", "sugar_from_honey", 0, {{"sugar", 3}}, cells("honey_bottle", 1)},
        // Ores that drop the item when mined.
        {"furnace", "furnace_diamond_ore", 100, {{"diamond", 1}}, cells("diamond_ore", 1)},
        {"furnace", "furnace_deepslate_diamond_ore", 100, {{"diamond", 1}}, cells("deepslate_diamond_ore", 1)},
        {"crafting_table", "diamond_block", 0, {{"diamond_block", 1}}, cells("diamond", 9)},
        {"crafting_table", "diamond", 0, {{"diamond", 9}}, cells("diamond_block", 1)},
        {"furnace", "furnace_quartz_ore", 100, {{"quartz", 1}}, cells("quartz_ore", 1)},
        {"crafting_table", "MapCloningRecipe", 0, {}, {}},
    };
    Index index = build(source);
    Choice choice = choose(index);
    auto id = [&](char const* name) { return index.find(name); };
    auto made = [&](char const* name) { Item item = id(name); return item < 0 || choice.recipe[item] < 0 ? std::string{} : index.recipes[choice.recipe[item]].id; };
    auto groupOf = [&](char const* name) {
        for (size_t i = 0; i < index.groups.size(); ++i) if (index.groups[i].name == name) return index.groupItem(static_cast<int>(i));
        return -1;
    };
    {
        check(id("barrel") < 0 || index.makers[id("barrel")].empty(), "a superseded recipe is not in the index");
        check(index.makers[id("arrow")].size() == 1 && made("arrow") == "arrow" && index.copies[id("arrow")].size() == 1
              && index.recipes[index.copies[id("arrow")].front()].own, "a recipe that uses its own product is kept apart and never chosen");
        check(viewTabs(index, choice, id("arrow"), false).front().recipes.size() == 2, "the view shows it after the others");
        auto const& chest = index.recipes[choice.recipe[id("chest")]];
        check(chest.uses.size() == 1 && chest.uses[0].group && chest.uses[0].count == 8, "grid cells of one tag add up, empty cells are skipped");
        auto const& cake = index.recipes[choice.recipe[id("cake")]];
        check(cake.count == 1 && cake.extra == Raw{{id("bucket"), 3}}, "what comes with the product is kept apart from it");
        check(index.groups[chest.uses[0].id].members == std::vector<Item>{id("oak_planks"), id("birch_planks")}, "a tag lists its members");
    }
    {
        check(made("stick") == "stick", "the lowest priority wins among crafting recipes");
        check(made("oak_planks") == "oak_planks", "on equal priority the recipe named after its product wins");
        check(made("stone_bricks") == "stonebrick", "the crafting table comes before the stonecutter whatever the priority");
        check(made("stone") == "furnace_cobblestone", "smelting is used when nothing else makes the item");
    }
    {
        check(choice.base[id("iron_block")] == id("iron_ingot") && choice.base[id("iron_nugget")] == id("iron_ingot"),
              "the form most other recipes use is the base of a group of convertible items");
        check(made("iron_block") == "iron_block" && made("iron_nugget") == "iron_nugget", "the other forms are made from the base");
        check(made("iron_ingot") == "furnace_iron", "the base is made only by recipes outside its group, the furnace before the blast furnace");
        auto const& smelt = index.recipes[choice.recipe[id("iron_ingot")]];
        check(smelt.uses.size() == 1 && smelt.uses[0].group && index.groups[smelt.uses[0].id].name.empty()
                  && index.groups[smelt.uses[0].id].members == std::vector<Item>{id("raw_iron"), id("iron_ore")},
              "smelting recipes that differ only in their input become one nameless group");
        check(choice.base[id("emerald_block")] == id("emerald") && made("emerald").empty() && made("emerald_block") == "emerald_block",
              "when nothing else uses either form, the one a conversion uses several of is the base");
        check(choice.base[id("stick")] == -1, "an item outside any loop has no base");
    }
    {
        check(made("iron_ingot") == "furnace_iron" && made("waxed_block") == "waxed_block" && choice.base[id("waxed_block")] == id("iron_ingot"),
              "a loop of three is not followed round: the base still comes from outside it");
        check(made("cobblestone").empty() && made("stone") == "furnace_cobblestone",
              "in a loop across stations the item more recipes use is gathered");
        check(made("white_wool") == "string_to_wool" && choice.base[id("red_wool")] == id("white_wool"),
              "an item with a recipe from outside the loop is its base");
        check(made("black_wool") == "wool_black_from_white" && made("red_wool") == "wool_red_from_white",
              "among equal recipes the one closest to the base wins, not the first by name");
        check(made("honey_bottle").empty() && made("honey_block") == "honey_block", "a loop with a second ingredient is cut the same way");
        bool looped = false;
        for (Item item = 0; item < static_cast<Item>(index.items.size()); ++item)
            looped = looped || !plan(index, choice, {{{item, 1}}, {}, {}, {}}).looped.empty();
        check(!looped, "no item's plan runs into a loop with the default recipes");
    }
    {
        check(made("diamond").empty() && made("quartz").empty(), "smelting an ore that drops the item is not the default");
        check(made("diamond_block") == "diamond_block" && choice.base[id("diamond_block")] == id("diamond"), "the mined item is still its loop's base");
        check(index.makers[id("diamond")].size() == 2 && made("iron_ingot") == "furnace_iron",
              "the ore recipes stay in the index, and smelting with a raw form among the inputs is still chosen");
    }
    {
        auto result = plan(index, choice, {{{id("stone_bricks"), 64}}, {{id("stone_bricks"), 20}, {id("stone"), 4}}, {}, {}});
        check(result.raw == Raw{{id("cobblestone"), 40}}, "a plan follows the chosen recipes down, held items first");
        auto blocks = plan(index, choice, {{{id("iron_block"), 2}, {id("iron_nugget"), 1}}, {}, {}, {}});
        Item const smelted = index.groupItem(index.recipes[choice.recipe[id("iron_ingot")]].uses[0].id);
        check(blocks.raw == Raw{{smelted, 19}} && blocks.looped.empty(), "a group of convertible items is traced through its base without a loop");
        auto stopped = plan(index, choice, {{{id("iron_block"), 1}}, {}, {}, {id("iron_ingot")}});
        check(stopped.raw == Raw{{id("iron_ingot"), 9}}, "an item the player stops at is gathered, not made");
    }
    {
        Item const planks = groupOf("planks");
        // 4 sticks need 2 of any planks, a chest 8; oak planks are also wanted as themselves.
        auto result = plan(index, choice, {{{id("stick"), 4}, {id("chest"), 1}, {id("oak_planks"), 3}}, {{id("oak_planks"), 5}, {id("birch_planks"), 4}}, {}, {}});
        check(result.raw == Raw{{planks, 4}}, "a group is covered by what its members have left after their own needs");
        check(std::find(result.fromHeld.begin(), result.fromHeld.end(), std::pair<Item, Count>{planks, 6}) != result.fromHeld.end()
                  && std::find(result.fromHeld.begin(), result.fromHeld.end(), std::pair<Item, Count>{id("oak_planks"), 3}) != result.fromHeld.end(),
              "held members are counted once, for the item first and the group after");
        auto picked = plan(index, choice, {{{id("chest"), 1}}, {}, {{planks - static_cast<Item>(index.items.size()), id("oak_planks")}}, {}});
        check(picked.raw == Raw{{id("oak_log"), 2}}, "a picked member stands for its group and is traced further");
        auto torches = plan(index, choice, {{{id("torch"), 4}}, {{id("charcoal"), 1}}, {}, {}});
        check(torches.raw == Raw{{planks, 2}}, "a held member covers a group that is not picked");
    }
    {
        // The Materials tab's lists (L-359): 64 stone bricks, a chest and 2 planks against some held stock.
        Request request{{{id("stone_bricks"), 64}, {id("chest"), 1}, {id("oak_planks"), 2}},
                        {{id("stone_bricks"), 20}, {id("stone"), 4}, {id("oak_planks"), 5}, {id("birch_planks"), 4}}, {}, {}};
        RawView view = rawView(index, choice, request);
        Item const planks = groupOf("planks");
        int const planksGroup = static_cast<int>(planks - static_cast<Item>(index.items.size()));
        check(view.gather == std::vector<GatherLine>{{id("cobblestone"), 40, 0, 40}, {planks, 8, 7, 1}},
              "gather lines carry need, what held stock covers and what is left, most left first");
        check(view.steps.size() == 3 && view.steps[0].item == id("stone") && view.steps[1].item == id("stone_bricks")
                  && view.steps[2].item == id("chest"), "steps come in making order: what is used before what uses it");
        check(view.steps[1] == StepLine{id("stone_bricks"), choice.recipe[id("stone_bricks")], 11, 44, 0, 20},
              "a step says how many crafts, how many they make, the spare and what held stock adds");
        check(view.covered == std::vector<Cover>{{planksGroup, id("oak_planks"), 3}, {planksGroup, id("birch_planks"), 4}},
              "the members that cover a group are named with their share");
        check(view.heldKinds == 4, "kinds that held stock covers a part of are counted once each");
        check(usersOf(index, request, view, id("stone")) == std::vector<std::pair<Item, Count>>{{id("stone_bricks"), 44}}
                  && usersOf(index, request, view, planks) == std::vector<std::pair<Item, Count>>{{id("chest"), 8}}
                  && usersOf(index, request, view, id("oak_planks")).empty(),
              "an item's users are the steps that take it; one only placed has none");
        check(stepInputs(index, request, view.steps[1]) == std::vector<std::pair<Item, Count>>{{id("stone"), 44}}
                  && stepInputs(index, request, view.steps[2]) == std::vector<std::pair<Item, Count>>{{planks, 8}},
              "a step takes its recipe's parts times its crafts, a group as itself");
    }
    {
        // The item list right of a 176-wide window at x 232 on a 640x360 screen.
        auto list = itemListRight(640, 360, 232, 176);
        check(list.left == 412 && list.width == 228 && list.columns == 12 && list.rows == 19, "the band runs from the window to the screen's edge in whole cells");
        check(list.perPage() == 228 && list.pages(229) == 2 && list.pages(0) == 1, "pages hold whole screens of cells, and there is always one");
        check(list.hit(list.cellX(13) + 1, list.cellY(13) + 1) == 13 && list.hit(416, 2) == -1 && list.hit(400, 100) == -1,
              "a point is in the cell drawn there, and in none on the bar or outside");
        check(!itemListRight(640, 360, 157, 480).usable(), "no list where the window leaves no room");
    }
    {
        // The recipe view: a shaped pickaxe, planks in any order, iron smelted two ways.
        Source source;
        source.tags["planks"] = {"oak_planks", "birch_planks"};
        source.recipes.push_back({"crafting_table", "pickaxe", 0, {{"pickaxe", 1}},
            {{"iron", false}, {"iron", false}, {"iron", false}, {"", false}, {"stick", false}, {"", false}, {"", false}, {"stick", false}, {"", false}}, 3, 3});
        source.recipes.push_back({"crafting_table", "stick", 0, {{"stick", 4}}, {{"planks", true}, {"planks", true}}, 1, 2});
        source.recipes.push_back({"crafting_table", "button", 0, {{"button", 1}}, {{"planks", true}}, 0, 0, true});
        source.recipes.push_back({"crafting_table", "odd", 0, {{"odd", 1}}, {{"stick", false}, {"iron", false}}, 3, 3});
        source.recipes.push_back({"furnace", "iron_a", 0, {{"iron", 1}}, {{"raw_iron", false}}});
        source.recipes.push_back({"furnace", "iron_b", 0, {{"iron", 1}}, {{"iron_ore", false}}});
        source.recipes.push_back({"blast_furnace", "iron_c", 0, {{"iron", 1}}, {{"raw_iron", false}}});
        Index index = build(source);
        Choice choice = choose(index);
        auto const& pickaxe = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("pickaxe"))].front())];
        check(pickaxe.width == 3 && pickaxe.cells.size() == 9 && pickaxe.cells[3].id < 0 && pickaxe.cells[4].id == index.find("stick"),
              "a shaped recipe keeps its grid, empty cells and all");
        auto block = viewBlock(pickaxe);
        check(block.slots.size() == 9 && block.slots[4].cell == 4 && block.slots[3].cell == -1 && !anyOrder(pickaxe) && !fitsSmallGrid(pickaxe),
              "a 3x3 recipe fills the grid as it is shaped");
        auto const& stick = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("stick"))].front())];
        auto stickBlock = viewBlock(stick);
        check(stickBlock.slots[0].cell == 0 && stickBlock.slots[3].cell == 1 && stickBlock.slots[1].cell == -1 && fitsSmallGrid(stick),
              "a narrow shape sits at the grid's left and fits the 2x2 grid");
        auto const& button = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("button"))].front())];
        auto const& odd = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("odd"))].front())];
        check(anyOrder(button) && !odd.width && odd.cells.size() == 2 && !anyOrder(odd),
              "a shape that does not fit its cells is laid out without it, and only the game's word marks any order");
        auto makes = viewTabs(index, choice, index.find("iron"), false);
        check(makes.size() == 2 && makes[0].station == Station::Furnace && makes[0].recipes.size() == 1 && makes[1].station == Station::BlastFurnace,
              "an item's recipes are split by station, smelting alternatives being one recipe");
        check(index.recipes[static_cast<size_t>(makes[0].recipes[0])].cells.size() == 1 && index.recipes[static_cast<size_t>(makes[0].recipes[0])].cells[0].group,
              "merged smelting shows its inputs as one slot");
        auto uses = viewTabs(index, choice, index.find("birch_planks"), true);
        check(uses.size() == 1 && uses[0].recipes.size() == 2, "an item is used where a group it belongs to is");
        check(viewTabs(index, choice, index.find("iron_ore"), true).size() == 1 && viewTabs(index, choice, -1, false).empty(),
              "a merged input counts as used, and an item the table lacks has no tabs");
        auto view = recipeViewAt(640, 360);
        check(block.productY + 9 == 27 && block.arrowY + 2 == 27 && block.noteY >= block.productY + 18,
              "the arrow and the product stand at the grid's middle, the small mark under them");
        check(view.left == 210 && view.top == 4 && view.height == 336 && view.hintY() == 329, "the view is centred, two columns wide, down to the search field's place");
        check(view.rows(Station::Crafting) == 5 && view.columns(Station::Crafting, 5) == 1 && view.columns(Station::Crafting, 6) == 2
              && view.pages(Station::Crafting, 11) == 2, "rows fill the height; a second column and page come when needed");
        // Laying a recipe into the grid from what is carried.
        Item const iron = index.find("iron"), sticks = index.find("stick"), oak = index.find("oak_planks"), birch = index.find("birch_planks");
        using Moves = std::vector<PlaceMove>;
        auto one = placePlan(index, pickaxe, 3, {{0, iron, 5}, {1, sticks, 2}}, {}, false);
        check(one.ready() && one.crafts == 1 && one.moves == Moves{{0, 1, 0}, {0, 1, 1}, {0, 1, 2}, {1, 1, 4}, {1, 1, 7}},
              "a shaped recipe is laid cell by cell where its shape puts it, one of each");
        check(!placePlan(index, pickaxe, 2, {{0, iron, 5}, {1, sticks, 2}}, {}, false).fits, "a recipe wider than the grid does not fit");
        auto few = placePlan(index, pickaxe, 3, {{0, iron, 2}, {1, sticks, 2}}, {}, false);
        check(few.fits && !few.ready() && few.missing == std::vector<int>{2} && few.moves.empty(), "a cell nothing serves is named and nothing moves");
        auto split = placePlan(index, pickaxe, 3, {{0, iron, 3}, {1, sticks, 64}, {2, iron, 4}}, {}, true);
        check(split.crafts == 2 && split.moves == Moves{{0, 2, 0}, {0, 1, 1}, {2, 1, 1}, {2, 2, 2}, {1, 2, 4}, {1, 2, 7}},
              "as many as divide evenly, drawn from every stack of the item, a cell from two stacks where one runs out");
        check(placePlan(index, pickaxe, 3, {{0, iron, 64}, {1, sticks, 64, 16}, {2, iron, 64}, {3, iron, 64}}, {}, true).crafts == 16,
              "a cell holds at most its item's stack");
        int const planksGroup = stick.cells[0].id;
        check(placePlan(index, stick, 2, {{0, oak, 3}, {1, birch, 10}}, {}, false).moves == Moves{{1, 1, 0}, {1, 1, 2}},
              "any of several items: the one most carried, and a narrow shape keeps the grid's left");
        check(placePlan(index, stick, 2, {{0, oak, 3}, {1, birch, 10}}, {{planksGroup, oak}}, false).moves == Moves{{0, 1, 0}, {0, 1, 2}},
              "the member the player picked comes first");
        check(placePlan(index, stick, 2, {{0, oak, 9}, {1, birch, 10}}, {}, true).crafts == 9, "laying many, several kinds add up");
        check(view.blockX(Station::Crafting, 3, 1) == 270 && view.blockX(Station::Crafting, 8, 1) == 325 && view.blockY(Station::Crafting, 8, 3) == 4 + 29 + 59,
              "one column sits in the middle; two fill left to right, then down");
    }
    {
        // Variants by aux: beds by color, made from wool or dyed from the white one; a chest's one aux is no variant.
        Source source;
        source.recipes.push_back({"crafting_table", "bed_white", 0, {{"bed", 1}}, {{"white_wool", false}, {"planks", false}}});
        source.recipes.push_back({"crafting_table", "bed_red", 0, {{"bed@14", 1}}, {{"red_wool", false}, {"planks", false}}});
        source.recipes.push_back({"crafting_table", "bed_red_dye", 1, {{"bed@14", 1}}, {{"bed", false}, {"red_dye", false}}});
        source.recipes.push_back({"crafting_table", "bed_any", 0, {{"bunk", 1}}, {{"bed@*", false}, {"bed@*", false}}});
        source.recipes.push_back({"crafting_table", "bed_copy", 0, {{"bed@14", 2}}, {{"bed@*", false}, {"paper", false}}});
        source.recipes.push_back({"crafting_table", "chest", 0, {{"chest@2", 1}}, {{"planks@*", false}}});
        source.recipes.push_back({"crafting_table", "hopper", 0, {{"hopper", 1}}, {{"chest@*", false}, {"iron", false}}});
        Index index = build(source);
        Choice choice = choose(index);
        check(index.find("bed") >= 0 && index.find("bed@14") >= 0 && index.find("bed", 14) == index.find("bed@14") && index.find("bed", 0) == index.find("bed"),
              "a name with several aux values in the table is several items");
        check(index.find("chest") >= 0 && index.find("chest@2") < 0 && index.find("chest", 2) == index.find("chest") && index.find("planks", 37) == index.find("planks"),
              "a name with one aux value is one item, whatever aux a stack of it has");
        check(index.keyOf("bed", 14) == "bed@14" && index.keyOf("chest", 2) == "chest" && index.keyOf("bed", 0) == "bed" && index.keyOf("sword", 120) == "sword",
              "a game item's key carries its aux only for a name with variants");
        auto const& red = index.recipes[static_cast<size_t>(choice.recipe[static_cast<size_t>(index.find("bed@14"))])];
        check(red.id == "bed_red" && index.makers[static_cast<size_t>(index.find("bed@14"))].size() == 2,
              "a variant has its own recipes; one that uses any variant of its own name is left out");
        auto const& bunk = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("bunk"))].front())];
        check(bunk.uses.size() == 1 && bunk.uses[0].group && bunk.uses[0].count == 2 && index.groups[static_cast<size_t>(bunk.uses[0].id)].members.size() == 2,
              "an ingredient of any aux is a group of the variants");
        auto const& hopper = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("hopper"))].front())];
        check(!hopper.uses[0].group && hopper.uses[0].id == index.find("chest"), "any aux of a name without variants is the item");
        check(splitItemKey("a:b@14").aux == 14 && splitItemKey("a:b@*").aux == anyAux && splitItemKey("a:b").aux == 0 && splitItemKey("a@b").name == "a@b"
              && itemKey("a:b", 0) == "a:b" && itemKey("a:b", 3) == "a:b@3", "a key is the name, with the aux after an @ when it is not 0");
    }
    {
        // Narrowing the list: by words, and to what the held items make at once.
        check(searchMatches("", "oak planks", "minecraft:oak_planks") && searchMatches("oak pl", "oak planks", "minecraft:oak_planks")
              && searchMatches("oak_", "オークの板材", "minecraft:oak_planks") && !searchMatches("oak iron", "oak planks", "minecraft:oak_planks")
              && searchMatches(lowered("OAK"), lowered("Oak Planks"), "") && searchMatches("板材", "オークの板材", "minecraft:oak_planks"),
              "every word of the query is in the name or the key; letters match whatever their case");
        Source source;
        source.tags["planks"] = {"oak_planks", "birch_planks"};
        source.recipes.push_back({"crafting_table", "stick", 0, {{"stick", 4}}, {{"planks", true}, {"planks", true}}, 1, 2});
        source.recipes.push_back({"crafting_table", "torch", 0, {{"torch", 4}}, {{"coal", false}, {"stick", false}}, 1, 2});
        source.recipes.push_back({"furnace", "glass", 0, {{"glass", 1}}, {{"sand", false}}});
        Index index = build(source);
        auto can = canMakeNow(index, {{index.find("oak_planks"), 1}, {index.find("birch_planks"), 1}, {index.find("coal"), 5}, {index.find("sand"), 9}});
        check(can[static_cast<size_t>(index.find("stick"))] && !can[static_cast<size_t>(index.find("torch"))] && !can[static_cast<size_t>(index.find("glass"))],
              "made now: the members of a group count together, nothing is made first, and only crafting counts");
        check(!canMakeNow(index, {{index.find("oak_planks"), 1}})[static_cast<size_t>(index.find("stick"))], "too few of an ingredient does not make it");
        auto side = itemListLeft(360, 210);
        check(side.width == 206 && side.columns == 11 && side.split() == 180 && side.favouriteRows() == 9 && side.usable() && side.wide(),
              "the left band runs from the screen's edge to the room the centre keeps, halved between favourites and wanted");
        check(side.hitFavourite(side.cellX(12) + 1, side.cellY(12) + 1) == 12 && side.hitFavourite(100, 178) == -1 && side.rows() == 12
              && side.hitRow(50, side.rowY(3) + 1) == 3 && side.hitRow(50, 350) == -1 && side.hitRow(50, 100) == -1,
              "favourites are cells above the middle; the wanted rows end a line short of the bottom");
        check(!itemListLeft(360, 20).usable() && !itemListLeft(360, 60).wide(), "a narrow band shows icons only, a narrower one nothing");
        auto field = searchFieldAt(640, 360);
        check(field.x == 232 && field.y == 344 && field.w == 176 && field.h == 14 && field.has(300, 350) && !field.has(300, 340), "the search field sits at the bottom centre");
        check(steppedWanted(1, false, 1, true) == 0 && steppedWanted(1, false, 16, false) == 1 && steppedWanted(20, false, 16, true) == 4
              && steppedWanted(9, false, 16, true) == 1 && steppedWanted(Wanted::most, true, 16, true) == Wanted::most,
              "only the minus button at one takes a wanted item out; steps stop at one and at the most");
    }
    {
        // What the player enters: favourites, wanted items, and the notes a plan follows.
        Notes notes;
        notes.toggleFavourite("torch");
        notes.toggleFavourite("stick");
        notes.toggleFavourite("torch");
        check(notes.favourites == std::vector<std::string>{"stick"} && notes.favourite("stick") && !notes.favourite("torch"), "a favourite goes in and out");
        Wanted wanted;
        wanted.toggle("lantern");
        wanted.set("lantern", 4);
        wanted.toggle("lantern");
        check(!wanted.has("lantern") && wanted.remembered["lantern"] == 4, "a wanted item taken out remembers its count");
        wanted.toggle("lantern");
        wanted.set("hopper", 0);
        wanted.set("bucket", 5000000);
        check(wanted.count("lantern") == 4 && wanted.count("hopper") == 1 && wanted.count("bucket") == Wanted::most && wanted.targets.front().first == "lantern",
              "put back it has that count again; a count stays between one and the most");
        Wanted read = decodeWanted(encodeWanted(wanted));
        check(read.targets == wanted.targets && read.remembered.empty(), "the wanted items are saved in order, the remembered counts are not");
        check(decodeWanted("not json").targets.empty() && decodeWanted("{\"wanted\":[{\"item\":\"a\",\"count\":-3},{\"item\":7,\"count\":1},{\"item\":\"b\",\"count\":2}]}").targets
              == std::vector<std::pair<std::string, Count>>{{"b", 2}}, "a document is read as far as it makes sense");
        Source source;
        source.tags["planks"] = {"oak_planks", "birch_planks"};
        source.recipes.push_back({"crafting_table", "stick", 0, {{"stick", 4}}, {{"planks", true}, {"planks", true}}, 1, 2});
        source.recipes.push_back({"crafting_table", "stick_bamboo", 0, {{"stick", 1}}, {{"bamboo", false}, {"bamboo", false}}, 1, 2});
        source.recipes.push_back({"crafting_table", "oak_planks", 0, {{"oak_planks", 4}}, {{"oak_log", false}}});
        Index index = build(source);
        Choice base = choose(index);
        notes.choices["stick"] = "crafting|stick_bamboo";
        notes.choices["oak_planks"] = "furnace|oak_planks";
        notes.choices["gone"] = "whatever";
        notes.picks["planks"] = "oak_planks";
        notes.picks["no_such_group"] = "oak_planks";
        notes.stops.insert("oak_planks");
        Notes same = decodeNotes(encodeNotes(notes));
        check(same == notes, "notes come back as they were written");
        Choice mine = chosen(index, base, notes);
        check(index.recipes[static_cast<size_t>(base.recipe[static_cast<size_t>(index.find("stick"))])].id == "stick"
              && index.recipes[static_cast<size_t>(mine.recipe[static_cast<size_t>(index.find("stick"))])].id == "stick_bamboo",
              "a recipe the player chose replaces the default; one the table lacks is ignored");
        check(mine.recipe[static_cast<size_t>(index.find("oak_planks"))] == base.recipe[static_cast<size_t>(index.find("oak_planks"))],
              "a choice names the station too: the same id at another station is another recipe");
        Request request;
        apply(index, notes, request);
        check(request.picks.size() == 1 && request.picks.begin()->second == index.find("oak_planks") && request.stops.contains(index.find("oak_planks")),
              "picks and stops reach the plan by name");
        Wanted list;
        list.set("stick", 8);
        list.set("unknown_item", 3);
        Request both;
        apply(index, list, both);
        apply(index, notes, both);
        auto view = rawView(index, base, both);
        check(both.wanted.size() == 1 && view.gather.size() == 1 && view.gather[0].id == index.find("oak_planks") && view.gather[0].left == 4 && view.steps.size() == 1,
              "the plan makes sticks from the picked planks and gathers those, as they are marked");
        check(groupKey(index, 0) == "planks", "a tag is noted by its name");
        check(index.findKey("stick@7") == index.find("stick") && index.findKey("stick") == index.find("stick") && index.findKey("none@2") < 0,
              "a key that carries an aux the table does not tell apart finds the item all the same");
        check(documentReadable(encodeNotes(notes)) && !documentReadable("{\"version\": 1,}") && !documentReadable("[]") && !documentReadable(""),
              "a document that does not parse is told apart from an empty one");
        KeptPresses presses;
        bool first = false;
        check(presses.decide(27, true, true, &first) && first && presses.decide(27, true, false, &first) && !first && presses.decide(27, false, false)
              && !presses.decide(27, false, false), "a kept press stays kept through its repeats and release, whatever closed meanwhile");
        check(!presses.decide(69, true, false) && !presses.decide(69, true, true, &first) && !first && !presses.decide(69, false, true)
              && presses.decide(69, true, true, &first) && first, "a press the game saw is the game's until its release; the next one is decided anew");
    }
    {
        Index empty = build({});
        check(empty.items.empty() && choose(empty).recipe.empty() && plan(empty, choose(empty), {}).raw.empty(), "an empty table gives an empty index");
    }
}
