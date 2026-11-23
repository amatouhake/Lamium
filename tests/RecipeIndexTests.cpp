#include <cmath>
#include "features/recipes/CountText.h"
#include "features/recipes/BandEditor.h"
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/ItemSearch.h"
#include "features/recipes/MassCraftPlan.h"
#include "features/recipes/ListFilter.h"
#include "features/recipes/PlacePlan.h"
#include "features/recipes/Places.h"
#include "features/recipes/RawView.h"
#include "features/recipes/PlanTargets.h"
#include "features/recipes/PlanRows.h"
#include "features/recipes/HowTo.h"
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
        {"crafting_table", "chest_from_birch", 0, {{"chest", 1}}, both(cells("birch_planks", 4), both(cells("", 1), cells("birch_planks", 4)))},
        {"crafting_table", "chest_from_cherry", 0, {{"chest", 1}}, both(cells("cherry_planks", 4), both(cells("", 1), cells("cherry_planks", 4)))},
        {"crafting_table", "chest_from_cherry_again", 0, {{"chest", 1}}, both(cells("cherry_planks", 4), both(cells("", 1), cells("cherry_planks", 4)))},
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
        {"blast_furnace", "furnace_iron_ore", 100, {{"iron_ingot", 1}}, cells("iron_ore", 1)},
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
        // Stones a player mines, though Nether quartz makes them (L-391).
        {"crafting_table", "diorite", 0, {{"diorite", 2}}, both(cells("cobblestone", 2), cells("quartz", 2))},
        {"crafting_table", "polished_diorite", 0, {{"polished_diorite", 4}}, cells("diorite", 4)},
        {"crafting_table", "string_to_wool", 0, {{"white_wool", 1}}, cells("string", 4)},
        {"crafting_table", "wool_white_from_black", 50, {{"white_wool", 1}}, both(cells("bone_meal", 1), cells("black_wool", 1))},
        {"crafting_table", "wool_black_from_red", 50, {{"black_wool", 1}}, both(cells("ink_sac", 1), cells("red_wool", 1))},
        {"crafting_table", "wool_black_from_white", 50, {{"black_wool", 1}}, both(cells("ink_sac", 1), cells("white_wool", 1))},
        {"crafting_table", "wool_red_from_black", 50, {{"red_wool", 1}}, both(cells("red_dye", 1), cells("black_wool", 1))},
        {"crafting_table", "wool_red_from_white", 50, {{"red_wool", 1}}, both(cells("red_dye", 1), cells("white_wool", 1))},
        {"crafting_table", "white_dye_from_bone_meal", 0, {{"white_dye", 1}}, cells("bone_meal", 1)},
        {"crafting_table", "white_stained_glass", 0, {{"white_stained_glass", 8}}, both(cells("glass", 8), cells("white_dye", 1))},
        {"crafting_table", "white_stained_glass_from_bone_meal", 0, {{"white_stained_glass", 8}}, both(cells("glass", 8), cells("bone_meal", 1))},
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
        bool birch = false;
        for (int number : index.makers[id("chest")]) birch = birch || index.recipes[number].id == "chest_from_birch";
        check(!birch && index.makers[id("chest")].size() == 2,
              "a recipe of one member is left out beside the recipe of any member; one of an item outside the group stays");
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
        check(made("white_wool").empty() && choice.base[id("red_wool")] == id("white_wool"),
              "an item with a recipe from outside the loop is its base");
        check(made("diorite").empty() && made("polished_diorite") == "polished_diorite",
              "diorite is gathered by default, and what is made from it is still made (L-391)");
        check(made("black_wool") == "wool_black_from_white" && made("red_wool") == "wool_red_from_white",
              "among equal recipes the one closest to the base wins, not the first by name");
        check(made("white_stained_glass") == "white_stained_glass_from_bone_meal",
              "a recipe that uses bone meal itself wins over the one that needs it made into white dye first");
        check(made("honey_bottle").empty() && made("honey_block") == "honey_block", "a loop with a second ingredient is cut the same way");
        bool looped = false;
        for (Item item = 0; item < static_cast<Item>(index.items.size()); ++item)
            looped = looped || !plan(index, choice, {{{item, 1}}, {}, {}, {}}).looped.empty();
        check(!looped, "no item's plan runs into a loop with the default recipes");
    }
    {
        check(made("diamond").empty() && made("quartz").empty(), "smelting an ore that drops the item is not the default");
        check(made("diamond_block") == "diamond_block" && choice.base[id("diamond_block")] == id("diamond"), "the mined item is still its loop's base");
        // One rule (usuallyGathered): the named items, and what is smelted from an ore that drops it.
        check(usuallyGathered(index, id("diamond")) && usuallyGathered(index, id("quartz")) && !usuallyGathered(index, id("iron_ingot"))
                  && !usuallyGathered(index, id("diamond_block")) && !usuallyGathered(index, id("stick")),
              "an item smelted from an ore that drops it is usually gathered; iron (from raw iron) and what is made from it are not");
        check(made("diamond").empty() && made("white_wool").empty() && plan(index, choice, {{{id("diamond"), 2}}, {}, {}, {}}).raw == Raw{{id("diamond"), 2}},
              "the rule gives an item usually gathered no recipe: automatic gathers it");
        Notes smelt;
        smelt.choices["diamond"] = choiceKey(index.recipes[static_cast<size_t>(index.makers[id("diamond")].front())]);
        Choice const smelting = chosen(index, choice, smelt);
        check(smelting.recipe[id("diamond")] >= 0 && plan(index, smelting, {{{id("diamond"), 2}}, {}, {}, {}}).raw != Raw{{id("diamond"), 2}},
              "choosing a recipe for it makes it");
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
        check(planned(index, choice, request).covered == std::vector<Cover>{{planksGroup, id("oak_planks"), 3}, {planksGroup, id("birch_planks"), 4}},
              "the members that cover a group are named with their share");
        check(view.done == std::vector<GatherLine>{{id("oak_planks"), 2, 2, 0}},
              "what held stock covers whole, neither gathered nor made, is done");
        check(gatherTotal(view.gather).need == 48 && gatherTotal(view.gather).left == 41
                  && gatherTotal(view.gather).fraction() > .14f && gatherTotal(view.gather).fraction() < .15f && gatherTotal({}).fraction() == 1.f,
              "the whole of what to gather: all needed, all left, and the share gathered");
        check(recipeOptions(index, id("stone_bricks")) == index.makers[static_cast<size_t>(id("stone_bricks"))] && recipeOptions(index, -1).empty(),
              "an item's recipe options are its makers in order");
        {
            auto const& chests = index.makers[static_cast<size_t>(id("chest"))];
            auto named = [&](char const* recipe) {
                return std::count_if(chests.begin(), chests.end(), [&](int number) { return index.recipes[static_cast<size_t>(number)].id == recipe; });
            };
            check(named("chest_from_cherry") == 1 && named("chest_from_cherry_again") == 0, "a recipe the table repeats under another id is listed once");
        }
        {
            auto const& makers = index.makers[static_cast<size_t>(id("chest"))];
            Choice other = choice;
            other.recipe[static_cast<size_t>(id("chest"))] = makers.back();
            auto const order = recipesToTry(index, &other, id("chest"));
            check(order.size() == makers.size() && order.front() == makers.back() && recipesToTry(index, nullptr, id("chest")) == makers
                      && recipesToTry(index, &choice, -1).empty(),
                  "an item's recipes are tried with the chosen one first, then the others in order");
        }
        check(stepInputs(index, request, view.steps[1]) == std::vector<std::pair<Item, Count>>{{id("stone"), 44}}
                  && stepInputs(index, request, view.steps[2]) == std::vector<std::pair<Item, Count>>{{planks, 8}},
              "a step takes its recipe's parts times its crafts, a group as itself");
        // The plan by wanted item (L-370).
        Wanted wanted;
        wanted.targets = {{"a", 1}, {"b", 2}};
        check(prioritized(wanted, {{"c", 3, "schematic"}}) == std::vector<WantedEntry>{{"a", 1, ""}, {"b", 2, ""}, {"c", 3, "schematic"}},
              "wanted items stand in the order added, the player's own before what features hand over");
        auto const two = targetPlans(index, choice, {{id("chest"), 1}, {id("chest"), 1}}, {{id("oak_planks"), 8}}, {}, {});
        check(two.size() == 2 && two[0].left == 0 && two[0].finished == 0 && two[1].left > 0
                  && !two[0].sharedAbove() && two[1].sharedAbove() && two[1].leftAlone == 0,
              "held stock goes to the first wanted item; the one below gets what is left of it and knows it");
        {
            // Worked out an entry a step (L-390 1-12): the same shares as at once.
            TargetPlanning planning{{{id("chest"), 1}, {id("chest"), 1}}, {{id("oak_planks"), 8}}, {}, {}};
            planning.step(index, choice);
            bool const half = !planning.done() && planning.out.size() == 1;
            planning.step(index, choice);
            planning.step(index, choice);
            check(half && planning.done() && planning.out.size() == 2 && planning.out[0].left == two[0].left
                      && planning.out[1].left == two[1].left && planning.out[1].leftAlone == two[1].leftAlone,
                  "shares worked out a step at a time resume with the stock the entries above left");
        }
        std::map<Item, Count> plenty{{id("cobblestone"), 64}};
        check(stepNow(index, request, view.steps[0], plenty) >= view.steps[0].crafts && stepNow(index, request, view.steps[0], {}) == 0,
              "a step can be made in full when everything its crafts take is held, and not at all from nothing");
        // The lists' rows (PlanRows.h), the same for the window and the panel.
        std::vector<LooseLine> const loose{{"grass", 3, 3, 0}, {"dirt", 5, 1, 4}};
        auto const gathered = gatherRows(index, view, loose, {id("cobblestone")});
        check(gathered.left.size() == 3 && gathered.left[0].line.id == id("cobblestone") && gathered.left[0].stopped && gathered.left[1].line.id == planks
                  && gathered.left[2].key == "dirt" && gathered.left[2].line.left == 4,
              "what is left to gather: the recipes' lines first, then what the table does not know");
        check(gathered.finished.size() == 2 && gathered.finished[0].key == "grass" && gathered.finished[0].done
                  && gathered.finished[1].line.id == id("oak_planks") && gathered.finished[1].done,
              "what is gathered follows, faint: what the table does not know, then what held stock covers whole");
        std::map<Item, Count> const carried{{id("cobblestone"), 64}}, all{{id("cobblestone"), 64}};
        auto const made = makeRows(index, request, view, heldSteps(index, choice, view), carried, all);
        check(made.size() == 4 && made[0].step.item == id("stone") && made[0].state == StepState::Ready && made[0].now == view.steps[0].made
                  && made[1].state == StepState::Missing && made[1].now == 0 && made[3].state == StepState::Held && made[3].step.item == id("oak_planks"),
              "steps stand by what can be made now, the plan's order within, then what is held; each says how many can be made now");
        std::vector<WantedEntry> const entries{{"a", 2, ""}, {"b", 1, ""}, {"c", 3, "schematic"}};
        std::map<std::string, bool> kept;
        auto rows = wantedRows(entries, {0, 1, 3}, kept, false);
        check(rows.size() == 3 && rows[0].entry == 0 && !rows[0].done && rows[1].entry == 1 && rows[1].done && rows[2].entry == 2 && rows[2].done,
              "wanted rows not all held come first, then the done ones, each in the order added");
        rows = wantedRows(entries, {2, 1, 3}, kept, true);
        check(!rows[0].done && rows[0].entry == 0 && rows[0].held == 2, "while the pointer is over the list a row keeps its place");
        rows = wantedRows(entries, {2, 1, 3}, kept, false);
        check(rows.size() == 3 && rows[0].entry == 0 && rows[0].done && rows[2].entry == 2, "once the pointer leaves, the rows take their places");
        check(wantedHeld({{"chest", 1, ""}, {"grass", 2, ""}}, {two[0], TargetPlan{}}, loose) == std::vector<Count>{0, 3},
              "a wanted item holds its finished items, or what is held of it where the table does not know it");
    }
    {
        // L-386: an "any" ingredient traced past its kind, where most members are made from one input in one amount.
        Source wood;
        wood.tags = {{"planks", {"oak_planks", "spruce_planks", "bamboo_planks"}}, {"wooden_slabs", {"oak_slab", "spruce_slab"}},
                     {"coals", {"coal", "charcoal"}}, {"stone_crafting_materials", {"cobblestone", "cobbled_deepslate"}}};
        wood.recipes = {
            {"crafting_table", "oak_planks", 0, {{"oak_planks", 4}}, cells("oak_log", 1)},
            {"crafting_table", "oak_planks_from_stripped", 0, {{"oak_planks", 4}}, cells("stripped_oak_log", 1)},
            {"crafting_table", "spruce_planks", 0, {{"spruce_planks", 4}}, cells("spruce_log", 1)},
            {"crafting_table", "bamboo_planks", 0, {{"bamboo_planks", 2}}, cells("bamboo_block", 1)},
            {"crafting_table", "chest", 0, {{"chest", 1}}, cells("planks", 8, true)},
            {"crafting_table", "oak_slab", 0, {{"oak_slab", 6}}, cells("oak_planks", 3)},
            {"crafting_table", "spruce_slab", 0, {{"spruce_slab", 6}}, cells("spruce_planks", 3)},
            {"crafting_table", "shelf", 0, {{"shelf", 1}}, cells("wooden_slabs", 2, true)},
            {"crafting_table", "coal_from_block", 0, {{"coal", 9}}, cells("coal_block", 1)},
            {"furnace", "charcoal", 0, {{"charcoal", 1}}, cells("oak_log", 1)},
            {"crafting_table", "torch", 0, {{"torch", 4}}, cells("coals", 1, true)},
            {"furnace", "coal_from_ore", 0, {{"coal", 1}}, cells("coal_ore", 1)},
            {"stonecutter", "cobblestone_from_stone", 0, {{"cobblestone", 1}}, cells("stone", 1)},
            {"furnace", "stone", 0, {{"stone", 1}}, cells("cobblestone", 1)},
            {"stonecutter", "cobbled_deepslate_from_deepslate", 0, {{"cobbled_deepslate", 1}}, cells("deepslate", 1)},
            {"furnace", "deepslate", 0, {{"deepslate", 1}}, cells("cobbled_deepslate", 1)},
            {"crafting_table", "furnace", 0, {{"furnace", 1}}, cells("stone_crafting_materials", 8, true)},
            {"crafting_table", "stone_slab", 0, {{"stone_slab", 6}}, cells("stone", 3)},
            {"stonecutter", "stone_stairs", 0, {{"stone_stairs", 1}}, cells("stone", 1)},
            {"crafting_table", "deepslate_tiles", 0, {{"deepslate_tiles", 4}}, cells("deepslate", 4)},
        };
        Index const woods = build(wood);
        Choice const picked = choose(woods);
        auto at = [&](std::string const& name) { return woods.find(name); };
        auto groupNamed = [&](std::string const& name) {
            for (size_t g = 0; g < woods.groups.size(); ++g) if (woods.groups[g].name == name) return static_cast<int>(g);
            return -1;
        };
        int const planks = groupNamed("planks"), slabs = groupNamed("wooden_slabs"), coals = groupNamed("coals");
        auto const& trace = woods.traces[static_cast<size_t>(planks)];
        std::vector<Item> logs = trace.kind >= 0 ? woods.groups[static_cast<size_t>(trace.kind)].members : std::vector<Item>{};
        std::sort(logs.begin(), logs.end());
        std::vector<Item> expected{at("oak_log"), at("stripped_oak_log"), at("spruce_log")};
        std::sort(expected.begin(), expected.end());
        check(trace.uses == 1 && trace.makes == 4 && logs == expected && trace.recipes.size() == 3,
              "planks trace to every input of the members that line up, four from one; bamboo planks are left out");
        check(woods.traces[static_cast<size_t>(coals)].kind < 0, "smelting an ore that drops the item does not line a member up (coal is mined)");
        int const stones = groupNamed("stone_crafting_materials");
        check(stones >= 0 && woods.traces[static_cast<size_t>(stones)].kind < 0,
              "a member made from what is made from it (cobblestone from stone) is a loop's base, not traced");
        auto madeBy = [&](std::string const& name) { int const r = picked.recipe[static_cast<size_t>(at(name))]; return r < 0 ? std::string() : woods.recipes[static_cast<size_t>(r)].id; };
        check(madeBy("cobblestone").empty() && madeBy("stone") == "stone" && madeBy("cobbled_deepslate").empty() && madeBy("deepslate") == "deepslate",
              "in a loop the item smelted into another of it is the base, however many recipes use the other (cobblestone, not stone)");
        auto const& slabTrace = woods.traces[static_cast<size_t>(slabs)];
        check(slabTrace.kind >= 0 && slabTrace.uses == 3 && slabTrace.makes == 6 && woods.traces[static_cast<size_t>(slabTrace.kind)].kind == trace.kind,
              "a kind is traced in turn: slabs to their planks, those planks to the logs");
        Item const planksItem = woods.groupItem(planks), logsItem = woods.groupItem(trace.kind);
        // A chest takes 8 planks of any wood: 3 oak planks and 1 oak log held.
        Request chest{{{at("chest"), 1}}, {{at("oak_planks"), 3}, {at("oak_log"), 1}}, {}, {}};
        auto const made = planned(woods, picked, chest);
        check(made.result.raw == Raw{{logsItem, 1}}, "held planks cover the group first, held logs the kind, the rest is gathered as logs of any wood");
        check(std::find(made.result.steps.begin(), made.result.steps.end(), rawMaterials::Step{planksItem, 2, 3}) != made.result.steps.end()
                  && made.covered == std::vector<Cover>{{planks, at("oak_planks"), 3}, {trace.kind, at("oak_log"), 1}},
              "the group is made from its kind for what held planks leave, and the members that cover each are named");
        auto const view = rawView(woods, picked, chest);
        auto const step = std::find_if(view.steps.begin(), view.steps.end(), [&](StepLine const& s) { return s.item == planksItem; });
        check(step != view.steps.end() && groupStep(woods, *step) && step->made == 8 && woods.recipes[static_cast<size_t>(step->recipe)].makes == at("oak_planks"),
              "the group's step shows, laying in the member whose input is most held");
        check(step != view.steps.end() && stepInputs(woods, chest, *step) == std::vector<std::pair<Item, Count>>{{logsItem, 2}}
                  && stepNow(woods, chest, *step, {{at("oak_log"), 1}, {at("spruce_log"), 1}}) == 2,
              "the group's step takes its kind, any member counted together");
        std::map<Item, Count> const carried{{at("oak_log"), 1}, {at("spruce_log"), 3}};
        auto const runs = step != view.steps.end() ? stepRuns(woods, chest, *step, carried) : std::vector<std::pair<int, int>>{};
        auto const two = step != view.steps.end() ? stepRuns(woods, chest, *step, {{at("oak_log"), 1}, {at("spruce_log"), 1}}) : runs;
        check(runs.size() == 1 && woods.recipes[static_cast<size_t>(runs[0].first)].makes == at("spruce_planks") && runs[0].second == 2
                  && two.size() == 2 && two[0].second == 1 && two[1].second == 1
                  && woods.recipes[static_cast<size_t>(two[1].first)].makes == at("spruce_planks"),
              "making the group's step makes the member whose input is most carried first, then the next");
        auto const run = step != view.steps.end() ? makeRun(woods, chest, *step, carried) : MakeRun{};
        check(run.recipes.size() >= 2 && woods.recipes[static_cast<size_t>(run.recipes[0])].makes == at("spruce_planks") && run.rounds == 2,
              "making hands every member's recipe over, the most carried first, with one count for all (picked at its turn)");
        // The stores hold oak logs, spruce logs are carried: the step is laid in from what is carried.
        if (step != view.steps.end()) {
            Request stored{{{at("chest"), 1}}, {{at("oak_log"), 5}, {at("spruce_log"), 2}}, {}, {}};
            auto const planStored = rawView(woods, picked, stored);
            auto const s2 = std::find_if(planStored.steps.begin(), planStored.steps.end(), [&](StepLine const& s) { return s.item == planksItem; });
            check(s2 != planStored.steps.end() && woods.recipes[static_cast<size_t>(s2->recipe)].makes == at("oak_planks")
                      && woods.recipes[static_cast<size_t>(layRecipe(woods, stored, *s2, {{at("spruce_log"), 2}}))].makes == at("spruce_planks"),
                  "a group's step lays in what is carried, though the stores hold more of another member's input");
            Request oakOnly = chest;
            oakOnly.picks[trace.kind] = at("oak_log");
            auto const onlyOak = stepRuns(woods, oakOnly, *step, {{at("oak_log"), 1}, {at("spruce_log"), 5}});
            check(onlyOak.size() == 1 && woods.recipes[static_cast<size_t>(onlyOak[0].first)].makes == at("oak_planks"),
                  "a member of the kind picked in How to make keeps the run to it");
        }
        // Held bamboo planks still count as planks: nothing to trace.
        auto const bamboo = planned(woods, picked, {{{at("chest"), 1}}, {{at("bamboo_planks"), 8}}, {}, {}});
        check(bamboo.result.raw.empty() && bamboo.result.steps.size() == 1 && bamboo.result.steps[0].item == at("chest"), "a member that does not line up still counts toward its group when held");
        // Gathered as it is, or a member picked: as before.
        auto const gathered = planned(woods, picked, {{{at("chest"), 1}}, {}, {}, {planksItem}});
        check(gathered.result.raw == Raw{{planksItem, 8}}, "a group set to gather is not traced");
        auto const oak = planned(woods, picked, {{{at("chest"), 1}}, {}, {{planks, at("oak_planks")}}, {}});
        check(oak.result.raw == Raw{{at("oak_log"), 2}}, "a picked member is traced through its own recipe");
        Notes notes;
        notes.setGathered(groupKey(woods, planks), true);
        Request fromNotes;
        apply(woods, notes, fromNotes);
        check(fromNotes.stops.contains(planksItem), "a group's gather is saved by its key and reaches the plan");
    }
    {
        // L-389: How to make's recipe order and re-dyeing.
        Source dyes;
        dyes.recipes = {
            {"crafting_table", "harness_white_dye", 0, {{"white_harness", 1}}, both(cells("black_harness", 1), cells("white_dye", 1))},
            {"crafting_table", "harness_white_bone", 0, {{"white_harness", 1}}, both(cells("black_harness", 1), cells("bone_meal", 1))},
            {"crafting_table", "harness_white", 0, {{"white_harness", 1}}, both(cells("leather", 3), both(cells("glass", 2), cells("white_wool", 1)))},
            {"crafting_table", "terracotta_white", 0, {{"white_terracotta", 8}}, both(cells("terracotta", 8), cells("white_dye", 1))},
            {"crafting_table", "bed_red", 0, {{"bed@14", 1}}, cells("red_wool", 3)},
            {"crafting_table", "bed_white", 0, {{"bed@0", 1}}, cells("white_wool", 3)},
            {"crafting_table", "bed_white_dyed", 0, {{"bed@0", 1}}, both(cells("bed@14", 1), cells("white_dye", 1))},
        };
        Index const dyed = build(dyes);
        Choice const dyedChoice = choose(dyed);
        auto made = [&](std::string const& id) {
            for (auto const& recipe : dyed.recipes) if (recipe.id == id) return &recipe;
            return static_cast<Recipe const*>(nullptr);
        };
        check(colorName("minecraft:light_blue_harness").base == "harness" && colorName("minecraft:light_blue_harness").colored
                  && !colorName("minecraft:terracotta").colored && colorName("minecraft:bed@14").colored && !colorName("minecraft:bed").colored,
              "a color is taken off a name, the longer color first; an aux variant is a color of its own");
        check(redyes(dyed, *made("harness_white_dye")) && !redyes(dyed, *made("harness_white")) && !redyes(dyed, *made("terracotta_white"))
                  && redyes(dyed, *made("bed_white_dyed")) && !redyes(dyed, *made("bed_white")),
              "re-dyeing takes another color of the same thing; from a thing with no color it is the main recipe");
        Item const harness = dyed.find("white_harness");
        auto const listed = howToRecipes(dyed, harness, dyedChoice.recipe[static_cast<size_t>(harness)]);
        check(listed.main.size() == 1 && dyed.recipes[static_cast<size_t>(listed.main[0])].id == "harness_white" && listed.redye.size() == 2,
              "How to make lists the main recipe, the re-dyeing folded apart");
        std::vector<Item> const members{3, 1, 2, 9};
        check(rankedMembers(members, [](Item item) { return item == 9 ? 1000 : item; }) == std::vector<Item>{1, 2, 3, 9},
              "a group's members stand in the item list's order, the unknown after");
    }
    {
        // The item list in a panel from x 412 to the edge of a 640x360 screen.
        auto list = itemListIn({412, 0, 228, 360});
        check(list.left == 412 && list.width == 228 && list.columns == 12 && list.rows == 19, "the band runs from the window to the screen's edge in whole cells");
        check(list.perPage() == 228 && list.pages(229) == 2 && list.pages(0) == 1, "pages hold whole screens of cells, and there is always one");
        check(list.hit(list.cellX(13) + 1, list.cellY(13) + 1) == 13 && list.hit(416, 2) == -1 && list.hit(400, 100) == -1,
              "a point is in the cell drawn there, and in none on the bar or outside");
        check(!itemListIn({637, 0, 3, 360}).usable(), "no list where the panel leaves no room");
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
        source.recipes.push_back({"blast_furnace", "iron_d", 0, {{"iron", 1}}, {{"iron_ore", false}}});
        source.recipes.push_back({"furnace", "steak", 0, {{"steak", 1}}, {{"beef", false}}});
        source.recipes.push_back({"smoker", "steak", 0, {{"steak", 1}}, {{"beef", false}}});
        source.recipes.push_back({"campfire", "steak", 0, {{"steak", 1}}, {{"beef", false}}});
        source.recipes.push_back({"soul_campfire", "steak", 0, {{"steak", 1}}, {{"beef", false}}});
        source.recipes.push_back({"crafting_table", "campfire", 0, {{"minecraft:campfire", 1}}, {{"stick", false}, {"coal", false}}});
        source.recipes.push_back({"crafting_table", "furnace", 0, {{"minecraft:furnace", 1}}, {{"cobblestone", false}}});
        source.recipes.push_back({"crafting_table", "minecart_furnace", 0, {{"furnace_minecart", 1}}, {{"minecraft:furnace", false}, {"minecart", false}}});
        Index index = build(source);
        Choice choice = choose(index);
        auto const& pickaxe = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("pickaxe"))].front())];
        check(pickaxe.width == 3 && pickaxe.cells.size() == 9 && pickaxe.cells[3].id < 0 && pickaxe.cells[4].id == index.find("stick"),
              "a shaped recipe keeps its grid, empty cells and all");
        auto block = viewBlock(pickaxe);
        check(block.slots.size() == 9 && block.slots[4].cell == 4 && block.slots[3].cell == -1 && !anyOrder(pickaxe),
              "a 3x3 recipe fills the grid as it is shaped");
        auto const& stick = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("stick"))].front())];
        auto stickBlock = viewBlock(stick);
        check(stickBlock.slots[0].cell == 0 && stickBlock.slots[3].cell == 1 && stickBlock.slots[1].cell == -1,
              "a narrow shape sits at the grid's left");
        auto const& button = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("button"))].front())];
        auto const& odd = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("odd"))].front())];
        check(anyOrder(button) && !odd.width && odd.cells.size() == 2 && !anyOrder(odd),
              "a shape that does not fit its cells is laid out without it, and only the game's word marks any order");
        auto makes = viewTabs(index, choice, index.find("iron"), false);
        check(makes.size() == 2 && makes[0].kind == Kind::Smelting && makes[1].kind == Kind::Blasting && makes[0].recipes == makes[1].recipes
                  && makes[0].recipes.size() == 1 && makes[1].station == Station::BlastFurnace
                  && index.recipes[static_cast<size_t>(makes[0].recipes[0])].stations == std::vector<Station>{Station::Furnace, Station::BlastFurnace},
              "one recipe several stations do shows in the tab of each process, smelting alternatives being one recipe (L-376)");
        auto steak = viewTabs(index, choice, index.find("steak"), false);
        check(steak.size() == 3 && steak[2].kind == Kind::CampfireCooking && tabStations(index, steak[2]) == std::vector<Station>{Station::Campfire, Station::SoulCampfire}
                  && tabStations(index, steak[0]) == std::vector<Station>{Station::Furnace},
              "a tab names the stations of its own process");
        auto campfireUses = viewTabs(index, choice, index.find("minecraft:campfire"), true);
        check(!campfireUses.empty() && campfireUses[0].only == Station::Campfire && tabStations(index, campfireUses[0]) == tabStations(index, steak[2]),
              "a station's own tab names its process's stations as the process's tab does");
        auto furnaceUses = viewTabs(index, choice, index.find("minecraft:furnace"), true);
        check(furnaceUses.size() == 2 && furnaceUses[0].only == Station::Furnace && furnaceUses[0].recipes.size() == 2
                  && furnaceUses[1].kind == Kind::Crafting && furnaceUses[1].recipes.size() == 1,
              "a station's uses begin with what it does, then what it goes into");
        {
            // An item made at the furnace by the rule and at the crafting table too: crafting stays the first tab.
            Source both;
            both.recipes.push_back({"furnace", "ingot_smelted", 0, {{"ingot", 1}}, {{"ore", false}}});
            both.recipes.push_back({"crafting_table", "ingot_from_nuggets", 0, {{"ingot", 1}}, {{"nugget", false}, {"nugget", false}}});
            Index const two = build(both);
            Choice picked = choose(two);
            Item const ingot = two.find("ingot");
            picked.recipe[static_cast<size_t>(ingot)] = two.makers[static_cast<size_t>(ingot)][two.recipes[static_cast<size_t>(two.makers[static_cast<size_t>(ingot)][0])].station == Station::Furnace ? 0 : 1];
            auto const tabs = viewTabs(two, picked, ingot, false);
            check(tabs.size() == 2 && tabs[0].kind == Kind::Crafting && tabs[1].kind == Kind::Smelting,
                  "an item's tabs keep the kinds' order whatever recipe is in use (L-380)");
        }
        check(viewTabs(index, choice, index.find("minecraft:furnace"), false).front().kind == Kind::Crafting,
              "a station's recipes are how it is made");
        check(index.recipes[static_cast<size_t>(makes[0].recipes[0])].cells.size() == 1 && index.recipes[static_cast<size_t>(makes[0].recipes[0])].cells[0].group,
              "merged smelting shows its inputs as one slot");
        auto uses = viewTabs(index, choice, index.find("birch_planks"), true);
        check(uses.size() == 1 && uses[0].recipes.size() == 2, "an item is used where a group it belongs to is");
        check(viewTabs(index, choice, index.find("iron_ore"), true).size() == 2 && viewTabs(index, choice, -1, false).empty(),
              "a merged input counts as used, and an item the table lacks has no tabs");
        auto view = recipeViewAt(640, 360);
        check(block.productY + 9 == 27 && block.arrowY + 2 == 27 && block.noteY >= block.productY + 18,
              "the arrow and the product stand at the grid's middle, the small mark under them");
        check(block.placeX + viewButton == viewBlockWidth && block.placeY + viewButton == viewBlockHeight(Station::Crafting)
                  && block.chooseX + viewButton + 2 == block.placeX && block.chooseY == block.placeY && block.chooseY >= block.productY + 18
                  && block.noteX >= 54 && block.noteX + 9 <= block.chooseX, // the any-order mark is 9 wide (Widgets.h)
              "place stands at the block's bottom right, the check box beside it, clear of the product and the small mark (L-379)");
        bool cornersClear = true;
        for (Station s : {Station::Stonecutter, Station::Furnace, Station::Smithing}) {
            Recipe one;
            one.station = s, one.cells = {{false, 0, 1}, {false, 0, 1}, {false, 0, 1}};
            auto const b = viewBlock(one);
            cornersClear = cornersClear && b.placeY + viewButton == viewBlockHeight(s) && b.chooseX + viewButton <= b.placeX
                && (b.chooseX >= b.productX + 18 || b.chooseY >= b.productY + 18) && b.productY + 18 <= viewBlockHeight(s);
        }
        check(cornersClear, "every station's block keeps place and the check box at its corner, off the product");
        check(RecipeViewLayout::gap == 6 && RecipeViewLayout::block + 2 * RecipeViewLayout::pad == 102,
              "faces are 102 wide and 6 apart (L-379)");
        check(view.left == 202 && view.top == 51 && view.height == 257 && view.hintY() == 295
              && recipeViewAt(640, 200).height == 200 - 18 - 4 - 22 && recipeViewAt(640, 200).top == 22,
              "the view is centred, three rows high, under its tabs; on a low screen it fills the room");
        check(view.rows(Station::Crafting) == 3 && view.perPage(Station::Crafting) == 6 && view.columns(Station::Crafting, 3) == 1
              && view.columns(Station::Crafting, 4) == 2 && view.pages(Station::Crafting, 13) == 3
              && std::abs(view.blockY(Station::Crafting, 6, 2) - view.blockY(Station::Crafting, 6, 0) - view.rowStep(Station::Crafting)) < .01f
              && view.blockX(Station::Crafting, 6, 0) >= view.left + RecipeViewLayout::margin
              && view.blockX(Station::Crafting, 6, 1) + RecipeViewLayout::block <= view.left + RecipeViewLayout::width - RecipeViewLayout::margin,
              "a page is two columns of three rows inside the window's margins; one column when three fit");
        // Laying a recipe into the grid from what is carried.
        Item const iron = index.find("iron"), sticks = index.find("stick"), oak = index.find("oak_planks"), birch = index.find("birch_planks");
        using Moves = std::vector<PlaceMove>;
        auto one = placePlan(index, pickaxe, 3, {{0, iron, 5}, {1, sticks, 2}}, {}, false);
        check(one.ready() && one.crafts == 1 && one.moves == Moves{{0, 1, 0}, {0, 1, 1}, {0, 1, 2}, {1, 1, 4}, {1, 1, 7}},
              "a shaped recipe is laid cell by cell where its shape puts it, one of each");
        check(!placePlan(index, pickaxe, 2, {{0, iron, 5}, {1, sticks, 2}}, {}, false).fits, "a recipe wider than the grid does not fit");
        check(roomAfter({{0, sticks, 60, 64}}, {}, 1, sticks, 64) == 68 && roomAfter({{0, sticks, 64, 64}, {1, iron, 5, 64}}, {}, 0, sticks, 64) == 0
                  && roomAfter({}, {}, 2, sticks, 16) == 32,
              "making stops at the room the inventory has for the product: empty slots and what its stacks can still take");
        check(roomAfter({{0, iron, 64, 64}, {1, sticks, 64, 64}}, {{0, 64, 0}}, 0, sticks, 64) == 64
                  && roomAfter({{0, iron, 64, 64}, {1, sticks, 64, 64}}, {{0, 63, 0}}, 0, sticks, 64) == 0,
              "a slot the inputs laid in empty is room for the product, one left in it is not");
        check(alreadyLaid(index, pickaxe, 3, {iron, iron, iron, -1, sticks, -1, -1, sticks, -1})
              && !alreadyLaid(index, pickaxe, 3, {iron, iron, iron, -1, sticks, -1, -1, sticks, iron})
              && !alreadyLaid(index, pickaxe, 3, {iron, iron, -1, -1, sticks, -1, -1, sticks, -1}) && !alreadyLaid(index, pickaxe, 2, {iron, iron, iron, -1}),
              "a grid that holds the recipe where it would be laid, and nothing else, is already laid");
        check(fitsGrid(pickaxe, 3) && !fitsGrid(pickaxe, 2) && fitsGrid(pickaxe, 0), "a three-wide recipe fits only a three-wide grid");
        check(craftOnto(true, iron, 1, -1, 0, 64) == CraftOnto::Ready && craftOnto(false, iron, 1, -1, 0, 64) == CraftOnto::Missing
                  && craftOnto(true, iron, 4, iron, 60, 64) == CraftOnto::Ready && craftOnto(true, iron, 4, iron, 61, 64) == CraftOnto::PlaceOnly
                  && craftOnto(true, iron, 4, sticks, 3, 64) == CraftOnto::PlaceOnly,
              "a round goes onto an empty pointer or onto the product with room; another item or no room only places (L-381)");
        {
            std::vector<Item> const laid{iron, iron, iron, -1, sticks, -1, -1, sticks, -1};
            std::vector<int> const ones{1, 1, 1, 0, 1, 0, 0, 1, 0}, full{64, 1, 1, 0, 1, 0, 0, 1, 0};
            check(oneMore(index, pickaxe, 3, laid, ones, {{0, iron, 5, 64}, {1, sticks, 2, 64}}) == Moves{{0, 1, 0}, {0, 1, 1}, {0, 1, 2}, {1, 1, 4}, {1, 1, 7}}
                  && oneMore(index, pickaxe, 3, laid, ones, {{0, iron, 5, 64}, {1, sticks, 1, 64}}).empty()
                  && oneMore(index, pickaxe, 3, laid, full, {{0, iron, 5, 64}, {1, sticks, 2, 64}}).empty(),
                  "one more of each goes onto a laid grid; nothing when something is short or a cell is a full stack");
        }
        auto few = placePlan(index, pickaxe, 3, {{0, iron, 2}, {1, sticks, 2}}, {}, false);
        check(few.fits && !few.ready() && few.missing == std::vector<int>{2} && few.moves.empty(), "a cell nothing serves is named and nothing moves");
        auto split = placePlan(index, pickaxe, 3, {{0, iron, 3}, {1, sticks, 64}, {2, iron, 4}}, {}, true);
        check(split.crafts == 2 && split.moves == Moves{{0, 2, 0}, {0, 1, 1}, {2, 1, 1}, {2, 2, 2}, {1, 2, 4}, {1, 2, 7}},
              "as many as divide evenly, drawn from every stack of the item, a cell from two stacks where one runs out");
        auto capped = placePlan(index, pickaxe, 3, {{0, iron, 30}, {1, sticks, 64}}, {}, true, 4);
        check(capped.crafts == 4 && placePlan(index, pickaxe, 3, {{0, iron, 6}, {1, sticks, 64}}, {}, true, 4).crafts == 2,
              "laying what a step still needs stops there, or at what divides evenly if that is less (L-385)");
        check(placePlan(index, pickaxe, 3, {{0, iron, 64}, {1, sticks, 64, 16}, {2, iron, 64}, {3, iron, 64}}, {}, true).crafts == 16,
              "a cell holds at most its item's stack");
        int const planksGroup = stick.cells[0].id;
        check(placePlan(index, stick, 2, {{0, oak, 3}, {1, birch, 10}}, {}, false).moves == Moves{{1, 1, 0}, {1, 1, 2}},
              "any of several items: the one most carried, and a narrow shape keeps the grid's left");
        check(placePlan(index, stick, 2, {{0, oak, 3}, {1, birch, 10}}, {{planksGroup, oak}}, false).moves == Moves{{0, 1, 0}, {0, 1, 2}},
              "the member the player picked comes first");
        check(placePlan(index, stick, 2, {{0, oak, 9}, {1, birch, 10}}, {}, true).crafts == 9, "laying many, several kinds add up");
        // A mass craft (L-165): the recipe the grid lays out, what a take added, what is too much of an ingredient.
        auto at = [&](Recipe const& recipe) { return static_cast<int>(&recipe - index.recipes.data()); };
        check(recipeInGrid(index, 3, {iron, iron, iron, -1, sticks, -1, -1, sticks, -1}) == at(pickaxe)
              && recipeInGrid(index, 3, {iron, iron, -1, -1, sticks, -1, -1, sticks, -1}) < 0 && recipeInGrid(index, 3, std::vector<Item>(9, -1)) < 0,
              "the grid's items name the shaped recipe they lay out, and nothing else does");
        check(recipeInGrid(index, 3, {-1, -1, oak, -1, -1, birch, -1, -1, -1}) == at(stick) && recipeInGrid(index, 2, {-1, -1, -1, birch}) == at(button),
              "a shape is found anywhere in the grid with any member of its group, an any-order recipe by its items");
        check(excess({{oak, 64}, {oak, 9}, {iron, 3}, {-1, 0}}, oak, {5, 0, 0, 0}) == std::vector<Throw>{{1, 9, true}, {0, 59, false}}
              && excess({{oak, 5}, {iron, 3}}, oak, {5, 0}).empty() && excess({{oak, 5}}, oak, {0}) == std::vector<Throw>{{0, 5, true}},
              "the product beyond what was carried at the start is thrown out, whole stacks from the last slot back");
        check(excess({{pickaxe.makes, 1}, {pickaxe.makes, 1}, {-1, 0}}, pickaxe.makes, {0, 1, 0}) == std::vector<Throw>{{0, 1, true}}
              && excess({{oak, 4}, {oak, 1}}, oak, {5, 0}).empty() && excess({{oak, 3}, {oak, 64}}, oak, {}) == std::vector<Throw>{{1, 64, true}, {0, 3, true}},
              "a stack carried at the start stays in its slot though a later one is the same item, and no more than the whole is over");
        check(roomSlot(index, pickaxe, {{0, iron, 64}, {1, sticks, 5}, {2, iron, 9}, {3, oak, 2}, {4, oak, 3}}) == 2
              && roomSlot(index, pickaxe, {{0, iron, 64}, {1, sticks, 5}}) < 0 && roomSlot(index, pickaxe, {{0, iron, 1, 1}, {1, iron, 1, 1}}) < 0,
              "room is made by the smallest stack of an ingredient carried twice, never its only stack nor what does not stack");
        std::vector<PlaceSource> full;
        for (int slot = 0; slot < 10; ++slot) full.push_back({slot, iron, slot == 2 ? 7 : 64});
        check(surplusSlots(index, pickaxe, full) == std::vector<int>{2, 7, 8, 9},
              "an ingredient keeps its share of the slots the recipe's ingredients fill, the fullest stacks, though the other is not carried at all");
        check(surplusSlots(index, pickaxe, {{0, iron, 64}}).empty() && surplusSlots(index, stick, {{0, oak, 64}, {1, birch, 64}, {2, oak, 64}}).empty()
              && surplusSlots(index, pickaxe, {{0, iron, 64}, {1, sticks, 64}, {2, sticks, 64}, {3, sticks, 64}, {4, sticks, 3}}) == std::vector<int>{3, 4},
              "an ingredient is never thrown out altogether, and a recipe of one ingredient throws nothing");
        check(surplusSlots(index, pickaxe, {{0, iron, 1, 1}, {1, iron, 1, 1}, {2, iron, 1, 1}, {3, iron, 1, 1}}).empty()
              && recipeInGrid(index, 2, {-1, -1, -1, birch}, "none") == at(button) && recipeInGrid(index, 2, {-1, -1, -1, birch}, button.id) == at(button),
              "what does not stack is never thrown, and the recipe the game names is taken where it fits");
        {
            // Two recipes from one input: only the game's name tells them apart.
            Source two;
            two.recipes.push_back({"crafting_table", "first", 0, {{"a", 1}}, {{"x", false}}, 0, 0, true});
            two.recipes.push_back({"crafting_table", "second", 0, {{"b", 1}}, {{"x", false}}, 0, 0, true});
            Index const pair = build(two);
            auto named = [&](std::string const& id) {
                for (size_t i = 0; i < pair.recipes.size(); ++i) if (pair.recipes[i].id == id) return static_cast<int>(i);
                return -2;
            };
            Item const x = pair.find("x");
            check(named("first") != named("second") && recipeInGrid(pair, 2, {-1, -1, -1, x}, "second") == named("second")
                  && recipeInGrid(pair, 2, {-1, -1, -1, x}, "first") == named("first") && recipeInGrid(pair, 2, {-1, -1, -1, x}, "none") == named("first"),
                  "of two recipes that fit, the one the game names is taken, else the first");
        }
        check(view.blockX(Station::Crafting, 3, 1) == 274 && view.blockX(Station::Crafting, 8, 1) == 328
              && std::abs(view.blockY(Station::Crafting, 8, 3) - (51 + 35 + 70 + (70.f - 54) / 2)) < .01f,
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
        // L-363: data after the rest, split off before the aux.
        auto const book = splitItemKey("minecraft:enchanted_book{ench:[{id:9s,lvl:5s}]}");
        check(book.name == "minecraft:enchanted_book" && book.aux == 0 && book.data == "{ench:[{id:9s,lvl:5s}]}"
              && itemKey(book.name, book.aux, book.data) == "minecraft:enchanted_book{ench:[{id:9s,lvl:5s}]}",
              "a key's data follows the name and splits back off");
        auto const banner = splitItemKey("minecraft:banner@15{Type:1}");
        check(banner.name == "minecraft:banner" && banner.aux == 15 && banner.data == "{Type:1}" && baseKey("minecraft:banner@15{Type:1}") == "minecraft:banner@15",
              "the aux is read before the data");
        auto const named = splitItemKey("a:b@3{n:\"x@4\"}");
        check(named.name == "a:b" && named.aux == 3 && named.data == "{n:\"x@4\"}" && splitItemKey("a:b@*{t:1}").aux == anyAux,
              "an @ inside the data is not the aux");
        check(baseKey("a:b") == "a:b" && baseKey("") == "" && splitItemKey("a:b{").data.empty(), "a key without whole data has none");
        auto values = [](std::map<std::string, std::string> held) {
            return [held](std::string_view tag) -> std::optional<std::string> {
                auto found = held.find(std::string(tag));
                return found == held.end() ? std::nullopt : std::optional(found->second);
            };
        };
        check(identityData("minecraft:enchanted_book", values({{"ench", "[{id:9s,lvl:5s}]"}, {"RepairCost", "1"}, {"display", "{Name:\"x\"}"}}))
                  == "{ench:[{id:9s,lvl:5s}]}",
              "only the tags that make an item what it is count, not a repair cost or a name");
        check(identityData("minecraft:banner", values({{"Type", "0"}})).empty() && identityData("minecraft:banner", values({})).empty()
                  && identityData("minecraft:banner", values({{"Type", "1"}})) == "{Type:1}",
              "a plain banner keeps its key; the ominous one carries its type");
        check(identityData("minecraft:banner", values({{"Type", "0"}, {"Patterns", "[{Color:1,Pattern:\"bo\"}]"}})) == "{Patterns:[{Color:1,Pattern:\"bo\"}]}"
                  && identityData("minecraft:banner", values({{"Type", "0"}, {"Patterns", "[]"}})).empty(),
              "a banner's patterns are part of its key; no patterns are none");
        check(identityData("minecraft:firework_star", values({{"FireworksItem", "{}"}})).empty() && identityData("minecraft:stone", values({{"x", "1"}})).empty(),
              "an item outside the table carries no data");
        check(identityData("minecraft:enchanted_book", values({{"ench", std::string(300, 'x')}})).empty(), "data too long to save is left out");
    }
    {
        // Narrowing the list to what the held items make at once (the words: ItemSearch.h, L-377).
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
        {
            // A group is known by its plainest member, not the first listed (L-370).
            Source lead;
            lead.tags["minecraft:sands"] = {"minecraft:red_sand", "minecraft:sand"};
            lead.tags["minecraft:wooden_slabs"] = {"minecraft:cherry_slab", "minecraft:birch_slab", "minecraft:oak_slab"};
            lead.tags["minecraft:stones"] = {"minecraft:cobbled_deepslate", "minecraft:blackstone", "minecraft:cobblestone"};
            lead.recipes.push_back({"crafting_table", "a", 0, {{"a", 1}}, {{"minecraft:sands", true}}});
            lead.recipes.push_back({"crafting_table", "b", 0, {{"b", 1}}, {{"minecraft:wooden_slabs", true}}});
            lead.recipes.push_back({"crafting_table", "c", 0, {{"c", 1}}, {{"minecraft:stones", true}}});
            Index const led = build(lead);
            auto leadOf = [&](std::string_view product) {
                auto const& part = led.recipes[static_cast<size_t>(led.makers[static_cast<size_t>(led.find(product))].front())].uses.front();
                return led.items[static_cast<size_t>(groupLead(led, part.id))];
            };
            check(leadOf("a") == "minecraft:sand" && leadOf("b") == "minecraft:oak_slab" && leadOf("c") == "minecraft:cobblestone",
                  "a group's lead is the plainest member: sand, oak, cobblestone (L-370)");
        }
        {
            // The automatic choice (L-370): within one station the fewer steps win, whatever the names say; the
            // station's order still comes first (crafting over the stonecutter).
            Source steps;
            steps.recipes.push_back({"crafting_table", "a_long", 0, {{"thing", 1}}, {{"middle", false}}});
            steps.recipes.push_back({"crafting_table", "z_short", 0, {{"thing", 1}}, {{"stone", false}}});
            steps.recipes.push_back({"crafting_table", "middle", 0, {{"middle", 1}}, {{"part", false}}});
            steps.recipes.push_back({"crafting_table", "part", 0, {{"part", 1}}, {{"stone", false}}});
            steps.recipes.push_back({"crafting_table", "stairs_crafted", 0, {{"stairs", 4}}, {{"bricks", false}}});
            steps.recipes.push_back({"crafting_table", "bricks", 0, {{"bricks", 4}}, {{"stone", false}}});
            steps.recipes.push_back({"stonecutter", "stairs_cut", 0, {{"stairs", 1}}, {{"stone", false}}});
            Index const stepped = build(steps);
            Choice const picked = choose(stepped);
            auto idOf = [&](std::string_view name) { return stepped.recipes[static_cast<size_t>(picked.recipe[static_cast<size_t>(stepped.find(name))])].id; };
            check(idOf("thing") == "z_short" && idOf("stairs") == "stairs_crafted",
                  "automatic takes the recipe with fewer steps within a station, crafting still before the stonecutter (L-370)");
        }
        {
            // Brewing (L-361): a mix read from the game is a recipe of the stand, its ingredient first, counted once a
            // batch of three bottles; the stand takes it with its ingredient and the bottle before.
            Source brews;
            brews.recipes.push_back({"brewing_stand", "brew:minecraft:potion+minecraft:nether_wart", 0, {{"minecraft:potion@4", 1}},
                                     {{"minecraft:nether_wart", false}, {"minecraft:potion", false}}});
            Index const brewed = build(brews);
            Item const awkward = brewed.find("minecraft:potion", 4), water = brewed.find("minecraft:potion", 0), wart = brewed.find("minecraft:nether_wart", 0);
            auto const& mix = brewed.recipes.front();
            check(awkward >= 0 && water >= 0 && wart >= 0 && mix.station == Station::BrewingStand && mix.batch == 3 && mix.uses.front().id == wart
                      && kindOf(mix.station) == Kind::Brewing,
                  "a mix becomes a brewing recipe of the stand, its ingredient first, three bottles a batch");
            Choice const brewChoice = choose(brewed);
            auto const four = plan(brewed, brewChoice, {{{awkward, 4}}, {}, {}, {}});
            auto has = [&](Item item, Count count) {
                return std::find(four.raw.begin(), four.raw.end(), std::pair<Item, Count>{item, count}) != four.raw.end();
            };
            check(has(water, 4) && has(wart, 2), "four bottles brewed take four bottles before and two ingredients (L-361)");
            {
                // The plan's step states (L-370): all crafts now, some, none; a brewing ingredient serves three.
                StepLine const brew{awkward, 0, 4, 4, 0, 0};
                Request const none{};
                std::map<Item, Count> some{{wart, 1}, {water, 4}}, enough{{wart, 2}, {water, 4}}, short_{{water, 4}};
                check(stepNow(brewed, none, brew, some) == 3 && stepState(brewed, none, brew, some) == StepState::Partly
                          && stepState(brewed, none, brew, enough) == StepState::Ready && stepState(brewed, none, brew, short_) == StepState::Missing
                          && stepShort(brewed, none, brew, some) == std::vector<bool>{true, false},
                      "a step is ready, partly made or missing from what is held, its short inputs told (L-370)");
                check(stepState(brewed, none, brew, short_, enough) == StepState::Stored && stepState(brewed, none, brew, some, enough) == StepState::Partly
                          && stepState(brewed, none, brew, short_, short_) == StepState::Missing,
                      "what is carried decides what can be made now; the stores make a step one to take out for (L-385)");
                RawView done;
                done.done.push_back({awkward, 3, 3, 0});
                auto const held = heldSteps(brewed, brewChoice, done);
                check(held.size() == 1 && held[0].item == awkward && held[0].made == 3, "what held stock covers whole is a held step");
                check(slotCountText(999) == "999" && slotCountText(2880) == "2k" && slotCountText(23040) == "23k" && slotCountText(749966) == "749k"
                          && slotCountText(1000002) == "1.0M" && slotCountText(1290000) == "1.2M" && slotCountText(12345678) == "12M"
                          && slotCountText(999999999) == "999M" && slotCountText(1000000000) == "++",
                      "a slot's count is at most four characters, rounded down (L-370)");
                check(wholeCountText(999) == "999" && wholeCountText(1000) == "1,000" && wholeCountText(2400008) == "2,400,008",
                      "a cut count's tip shows it whole with its thousands apart");
            }
            auto const face = viewBlock(mix);
            check(face.brewing && face.slots.size() == 3 && face.slots[0].cell == 0 && face.slots[1].cell == 1 && face.slots[2].cell == -1
                      && face.productX == 68 && face.slots[0].x == 46 && face.placeY + viewButton == 54
                      && face.slots[2].x + 18 + 4 <= face.chooseX,
                  "a brewing face is laid out as the stand: ingredient on top, the bottle before at the left, the one after at the right");
            Bench const stand{Station::BrewingStand, 0};
            check(benchMakes(stand, mix) && benchKind(stand) == Kind::Brewing
                      && canMakeNow(brewed, {{wart, 1}, {water, 1}}, stand)[static_cast<size_t>(awkward)]
                      && placePlan(brewed, mix, 0, {{3, wart, 5}, {4, water, 1, 1}}, {}, false).ready(),
                  "the brewing stand takes a brew it can make from what is carried");
        }
        {
            // A station as the place a recipe goes (L-380).
            Bench const furnace{Station::Furnace, 0}, grid{Station::Crafting, 2};
            auto atFurnace = canMakeNow(index, {{index.find("oak_planks"), 2}, {index.find("sand"), 9}}, furnace);
            check(atFurnace[static_cast<size_t>(index.find("glass"))] && !atFurnace[static_cast<size_t>(index.find("stick"))],
                  "at a furnace what can be made is what it smelts from what is carried");
            auto const& glass = index.recipes[static_cast<size_t>(index.makers[static_cast<size_t>(index.find("glass"))].front())];
            check(benchMakes(furnace, glass) && !benchMakes(grid, glass) && !benchMakes({Station::Smoker, 0}, glass) && benchKind(furnace) == Kind::Smelting
                      && benchAny(furnace) && !benchAny({}) && !benchAny({Station::Crafting, 0}),
                  "a furnace takes what it smelts, in the smelting tab; a grid and a smoker do not");
            Item const sand = index.find("sand");
            auto const one = placePlan(index, glass, 0, {{4, sand, 9}}, {}, false), all = placePlan(index, glass, 0, {{4, sand, 9}, {7, sand, 70, 64}}, {}, true);
            check(one.ready() && one.moves == std::vector<PlaceMove>{{4, 1, 0}} && all.ready() && all.crafts == 64
                      && all.moves == std::vector<PlaceMove>{{4, 9, 0}, {7, 55, 0}} && !placePlan(index, glass, 2, {{4, sand, 9}}, {}, false).fits,
                  "a station's input takes one, or with Shift a stack from the stacks carried");
        }
        {
            auto const narrow = planWindowAt(640, 210, 430), open = planWindowAt(640, 158, 482), seen = planWindowAt(640, 210, 430, 324);
            check(narrow.width == planWindowWidth && narrow.left == 157 && open.left == 158 && open.width == 324 && seen.width == 324 && seen.left == 158,
                  "the Recipes window takes the game's book-open span exactly, and that width centred while the book is closed");
        }
        auto side = sidePanelIn({0, 0, 206, 180}, true), plan = sidePanelIn({0, 180, 206, 180}, false);
        check(side.columns == 11 && side.split() == 180 && side.favoriteRows() == 9 && side.usable() && side.wide() && plan.split() == 0,
              "a panel as the favorites has them alone, one as the plan its rows alone");
        check(side.hitFavorite(side.cellX(12) + 1, side.cellY(12) + 1) == 12 && side.hitFavorite(100, 178) == -1 && plan.rows() == 13
              && plan.hitRow(50, plan.rowY(3) + 1) == 3 && plan.hitRow(50, 359) == -1 && plan.hitRow(50, 100) == -1,
              "favorites are cells under their heading; the plan's rows reach the panel's end");
        check(!sidePanelIn({0, 0, 16, 360}, true).usable() && !sidePanelIn({0, 0, 60, 360}, false).wide(),
              "a narrow panel shows icons only, a narrower one nothing");
        auto field = searchFieldAt(640, 360);
        auto const row = bottomRowAt(640, 360);
        check(row.x == 232 && row.w == 176 && field.x == 270 && field.y == 342 && field.w == 120 && field.h == 16 && field.has(300, 350)
                  && !field.has(300, 340) && searchFieldHit(field).has(268, 340) && searchHighlightAt(field).x == 392
                  && searchHighlightAt(field).x + searchHighlightAt(field).w == row.x + row.w && windowButtonAt(row).x == row.x
                  && layoutButtonAt(row).x + layoutButtonAt(row).w + 4 == field.x,
              "the bottom row: the window's and the layout's buttons, then the field and its highlight switch, as wide as the inventory");
        check(rowWidth(640, 200, 440) == 212 && rowWidth(640, 225, 415) == 186 && rowWidth(640, 240, 400) == 176
                  && searchFieldAt(640, 360, 212).w == 156 && bottomRowAt(640, 360, 212).x == 214,
              "the row widens by up to a button each side where the panels leave room, never below the inventory's width");
        check(steppedWanted(1, false, 1) == 0 && steppedWanted(1, false, 16) == 0 && steppedWanted(5, false, 1) == 4
              && steppedWanted(9, false, 16) == 1 && steppedWanted(0, true, 1) == 1 && steppedWanted(0, false, 1) == 0
              && steppedWanted(Wanted::most, true, 16) == Wanted::most,
              "down from one is 0 and up from 0 is one; steps of several stop at one; steps stop at the most");
        {
            Wanted held;
            held.toggle("a"); held.set("a", 5); held.toggle("b");
            held.zero("a");
            bool const zeroShown = held.listed("a") && !held.has("a") && held.count("a") == 0;
            held.toggle("a");
            bool const back = held.count("a") == 5 && held.targets.size() == 2;
            held.zero("a"); held.zero("b");
            bool const went = held.settle("a");
            check(zeroShown && back && went && held.listed("a") && !held.listed("b") && held.targets.size() == 1,
                  "the wheel leaves an item at 0 in the list; toggling brings its count back; settling takes out all at 0 but the one kept");
            held.settle("");
            check(held.targets.empty() && held.remembered["a"] == 5, "an item settled at 0 remembers the count it had");
        }
        check(hoverLines("Shield\n\xC2\xA7" "7Durability: 336 / 336\n\n\xEE\x84\x80\xEE\x84\x80\n\xC2\xA7" "8minecraft:shield") == std::vector<HoverLine>{{"Durability: 336 / 336", '7'}, {"minecraft:shield", '8'}}
              && hoverLines("a\n\xC2\xA7" "9\xC2\xA7" "oStorage\n\xC2\xA7" "rplain").back() == HoverLine{"plain", 0}
              && hoverLines("a\n\xC2\xA7" "9\xC2\xA7" "oStorage").front() == HoverLine{"Storage", '9'}
              && hoverLines("Name only").empty() && hoverLines("a\nb\nc\nd", 2).size() == 2,
              "an item's hover text gives its lines under the name with the color they start in, without empty lines and icon glyphs");
        check(steppedWanted(1, true, 64) == 64 && steppedWanted(64, true, 64) == 128 && steppedWanted(70, false, 64) == 64
              && steppedWanted(64, false, 64) == 1 && steppedWanted(20, false, 16) == 16 && steppedWanted(3, true, 1) == 4,
              "a step of several lands on its multiples, so Shift gives whole stacks");
    }
    {
        // What the player enters: favorites, wanted items, and the notes a plan follows.
        Notes notes;
        notes.toggleFavorite("torch");
        notes.toggleFavorite("stick");
        notes.toggleFavorite("torch");
        check(notes.favorites == std::vector<std::string>{"stick"} && notes.favorite("stick") && !notes.favorite("torch"), "a favorite goes in and out");
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
        // Counted places (step 8): the ender chest from its first sight on, stores once marked.
        Places places;
        Spot const left{0, 17, 74, 151}, right{0, 18, 74, 151}, far{0, 40, 74, 151};
        check(!places.see(false, left, right, "minecraft:chest", {{"minecraft:stone", 0, 9}}, 100) && places.list.empty(),
              "an open container that is no store is not kept");
        check(places.mark(left, right, "minecraft:chest", {{"minecraft:stone", 0, 9}}, 100) && !places.mark(right, left, "minecraft:chest", {}, 101)
              && places.store(right) == 0 && places.store(far) < 0,
              "a double chest is one store by either half, and is marked once");
        check(places.see(true, {}, {}, "minecraft:ender_chest", {{"minecraft:stone", 0, 3}, {"minecraft:bed", 14, 1}}, 110) && places.ender() == 0
              && places.store(left) == 1 && !places.unmark(0),
              "the ender chest is kept once seen, stands first and is not taken out");
        check(places.see(false, right, left, "minecraft:chest", {{"minecraft:stone", 0, 20}}, 120) && places.list[1].at == left && places.list[1].seen == 120
              && !places.see(false, right, left, "minecraft:chest", {{"minecraft:stone", 0, 20}}, 130),
              "a store opened again holds what it holds now, opened by either half");
        check(storeBoxes(places) == std::vector<StoreBox>{{0, 17, 74, 151, 18, 74, 151}}, "a store is marked as the box of its blocks, a double chest as both; the ender chest is not");
        check(placeTotals(places) == std::vector<Stored>{{"minecraft:stone", 0, 23}, {"minecraft:bed", 14, 1}},
              "the counted places add up by name and aux");
        places.list[0].counted = false;
        check(placeTotals(places) == std::vector<Stored>{{"minecraft:stone", 0, 20}}, "a place switched off is not counted");
        {
            places.carried = false;
            Places const read = decodePlaces(encodePlaces(places));
            check(read.list == places.list && !read.carried && decodePlaces("{}").carried && decodePlaces("nonsense").list.empty()
                  && decodePlaces(R"({"places":[{"at":[0,1,2]},{"at":[0,1,2,3],"items":[{"item":"a","count":2},{"item":"a","count":3},{"item":"b","count":0}]},{"at":[0,1,2,3]}]})").list
                         == std::vector<Place>{{false, {0, 1, 2, 3}, {}, "", true, {{"a", 0, 5}}, 0}},
                  "places survive their document; what is not understood is left out and a position stands once");
        }
        check(places.see(false, left, right, "minecraft:barrel", {}, 140) && places.store(left) < 0 && places.unmark(0) == false && places.list.size() == 1,
              "a store where another block stands now is no store any more");
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
        {
            Notes preset;
            preset.setGathered("stick", true);
            check(preset.gathered("stick") && preset.stops == std::set<std::string>{"stick"} && decodeNotes(encodeNotes(preset)) == preset,
                  "the player's own gather is kept and saved");
            preset.setGathered("stick", false);
            check(!preset.gathered("stick") && preset.stops.empty(), "and taken back");
            Source woolSource;
            woolSource.recipes.push_back({"crafting_table", "string_to_wool", 0, {{"white_wool", 1}}, cells("string", 4)});
            Index woolIndex = build(woolSource);
            Choice const woolChoice = choose(woolIndex);
            check(woolChoice.recipe[static_cast<size_t>(woolIndex.find("white_wool"))] < 0,
                  "an item usually gathered has no recipe by the rule, even its only one, unless the player chooses it");
        }
        Wanted list;
        list.set("stick", 8);
        list.set("unknown_item", 3);
        Request both;
        apply(index, list, {}, both);
        apply(index, notes, both);
        Request handed;
        apply(index, list, {{"stick", 4, "schematic"}, {"oak_planks", 2, "schematic"}, {"unknown_item", 9, "schematic"}}, handed);
        check(handed.wanted == std::vector<std::pair<Item, Count>>{{index.find("stick"), 12}, {index.find("oak_planks"), 2}},
              "what a feature hands over joins the wanted items, an item of both once with the sum");
        auto view = rawView(index, base, both);
        check(both.wanted.size() == 1 && view.gather.size() == 1 && view.gather[0].id == index.find("oak_planks") && view.gather[0].left == 4 && view.steps.size() == 1,
              "the plan makes sticks from the picked planks and gathers those, as they are marked");
        check(looseLines(index, list, {{"grass", 100, "schematic"}, {"grass", 58, "schematic"}, {"stick", 4, "schematic"}}, {{"grass", 30}, {"unknown_item", 9}})
                  == std::vector<LooseLine>{{"unknown_item", 3, 3, 0}, {"grass", 158, 30, 128}},
              "a wanted item no recipe knows is gathered as it is: its need summed, what is held taken off");
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
        // L-374: the bands as regions of panels. A 176-wide window at 232..408 on a 640x360 screen, the
        // room the centre keeps from 210 to 430.
        using namespace band;
        auto const regions = regionsAround(640, 360, 210, 430, 97, 263, 330);
        check(regions.boxes[Left] == Box{0, 0, 206, 360} && regions.boxes[Right] == Box{434, 0, 206, 360}
              && regions.boxes[Top] == Box{210, 0, 220, 95} && regions.boxes[Bottom] == Box{210, 265, 220, 65},
              "regions lie beside the window to the screen's edges, and above and below it");
        std::array<bool, pageCount> const all{true, true, true};
        auto placed = place(defaultLayout(), regions, all);
        auto const* favorites = find(placed, Page::Favorites);
        auto const* plan = find(placed, Page::Plan);
        auto const* items = find(placed, Page::Items);
        check(placed.size() == 3 && favorites && plan && items && favorites->box == Box{0, 0, 206, 180} && plan->box == Box{0, 180, 206, 180}
              && items->box == Box{434, 0, 206, 360} && favorites->first && !plan->first,
              "the default is today's: favorites over the plan on the left, the item list on the right");
        auto const list = itemListIn(items->box);
        auto const side = sidePanelIn(favorites->box, true);
        check(list.columns == 11 && list.rows == 19 && side.favoriteRows() == 9 && side.columns == 11,
              "a panel is a page's layout of whole cells");
        placed = place(defaultLayout(), regions, {true, false, true});
        check(find(placed, Page::Plan) && find(placed, Page::Plan)->box == Box{0, 0, 206, 360} && !find(placed, Page::Favorites),
              "a page whose feature is off is left out and the others take its room");
        Layout three;
        three.regions[Left] = {{Page::Favorites, 90}, {Page::Plan, 5}, {Page::Items, 5}};
        placed = place(three, regions, all);
        bool const least = std::all_of(placed.begin(), placed.end(), [](Placed const& p) { return p.box.h >= minLength(Left); });
        check(placed.size() == 3 && least && find(placed, Page::Items)->box.y + find(placed, Page::Items)->box.h == 360,
              "every panel keeps one row, taken from the others; the last ends with the region");
        Layout above;
        above.regions[Top] = {{Page::Favorites, 100}};
        above.regions[Right] = {{Page::Items, 100}};
        placed = place(above, regions, all);
        check(find(placed, Page::Favorites)->shownIn == Top && find(placed, Page::Favorites)->box == Box{210, 0, 220, 95},
              "a panel above the window fills the room there");
        placed = place(above, regionsAround(427, 240, 120, 307, 20, 186, 210), all);
        check(find(placed, Page::Favorites) && find(placed, Page::Favorites)->shownIn == Left && find(placed, Page::Favorites)->region == Top,
              "a region too low for one row lends its panels to a side, on this screen only");
        check(windowTop(97, 232, 408, {{0, 0, 640, 360}, {240, 72, 26, 26}, {300, 70, 20, 20}, {450, 60, 20, 30}, {240, 20, 20, 20}}) == 72,
              "the window's top takes in the tabs on its edge, not backdrops, controls beside it or far above");
        Layout messy;
        messy.regions[Left] = {{Page::Plan, 33}, {Page::Plan, 50}, {Page::Favorites, 33}};
        auto const tidy = normalized(messy);
        check(tidy.regions[Left].size() == 2 && tidy.regions[Left][0].share + tidy.regions[Left][1].share == 100
              && tidy.regions[Left][0].share % 5 == 0 && normalized(defaultLayout()) == defaultLayout(),
              "a page shows once, and a region's shares are steps of 5 that add up to 100");
    }
    {
        // L-375: arranging the bands by dragging.
        using namespace band;
        using namespace band::editor;
        auto const regions = regionsAround(640, 360, 210, 430, 97, 263, 330);
        auto const layout = defaultLayout();
        std::array<bool, pageCount> const all{true, true, true};
        auto const placed = place(layout, regions, all);
        Box const tray{250, 120, 140, 26};
        Drag const plan{Page::Plan, Left, 1};
        check(hit(placed, regions, layout, plan, 50, 300, tray) == Target{editor::Kind::Self, Left, 1}, "over its own place a drop does nothing");
        check(hit(placed, regions, layout, plan, 50, 50, tray) == Target{editor::Kind::Swap, Left, 0}, "over another panel of its region it swaps");
        check(hit(placed, regions, layout, plan, 500, 50, tray) == Target{editor::Kind::Before, Right, 0}
                  && hit(placed, regions, layout, plan, 500, 300, tray) == Target{editor::Kind::After, Right, 0},
              "in another region it goes before or after the panel it is over");
        check(hit(placed, regions, layout, plan, 300, 40, tray) == Target{editor::Kind::Fill, Top}
                  && hit(placed, regions, layout, plan, 300, 130, tray).kind == editor::Kind::Tray
                  && hit(placed, regions, layout, plan, 300, 200, tray).kind == editor::Kind::None,
              "an empty region takes it whole, the tray takes it out, the game's window takes nothing");
        auto swapped = apply(layout, plan, {editor::Kind::Swap, Left, 0});
        check(swapped && swapped->regions[Left][0].page == Page::Plan && swapped->regions[Left][1].page == Page::Favorites,
              "a swap trades the two panels' places");
        auto moved = apply(layout, plan, {editor::Kind::After, Right, 0});
        check(moved && moved->regions[Left].size() == 1 && moved->regions[Left][0].share == 100 && moved->regions[Right].size() == 2
                  && moved->regions[Right][1].page == Page::Plan && moved->regions[Right][0].share == 50,
              "a panel moved to another region takes half of the one it goes beside");
        auto out = apply(layout, plan, {editor::Kind::Tray});
        check(out && unplaced(*out) == std::vector<Page>{Page::Plan} && out->regions[Left][0].share == 100
                  && apply(*out, Drag{Page::Plan}, {editor::Kind::Fill, Top})->regions[Top][0].page == Page::Plan
                  && !apply(*out, Drag{Page::Plan}, {editor::Kind::Tray}),
              "the tray takes a page out and gives it back; a page not placed cannot go into it again");
        auto sized = resize(layout, Left, 0, 50, 36, 360);
        check(sized.regions[Left][0].share == 60 && sized.regions[Left][1].share == 40
                  && resize(layout, Left, 0, 50, -400, 360).regions[Left][0].share == 10,
              "the line between two panels moves their shares in steps of 5, never under one row");
    }
    {
        // L-377: the item list's search.
        using namespace search;
        std::string const planksName = folded("Oak Planks"), planksTags = folded("minecraft:planks\nminecraft:wooden");
        std::string const tip = folded("Fuel: smelts 1.5 items");
        Fields const planks{planksName, "minecraft:oak_planks", "oak planks", planksTags, tip};
        Fields const log{"oak log", "minecraft:oak_log", "oak log", "minecraft:logs", ""};
        Fields const modded{"ruby", "gems:ruby_gem", "ruby", "", ""};
        check(matches(parse("oak planks"), planks) && !matches(parse("oak planks"), log) && matches(parse(""), log),
              "every word must match; an empty search matches all");
        check(matches(parse("oak -log"), planks) && !matches(parse("oak -log"), log), "a leading minus leaves out");
        check(matches(parse("ruby|log"), modded) && matches(parse("ruby|log"), log) && !matches(parse("ruby|log"), planks)
                  && parse("oak planks | ruby").size() == 2 && parse("oak planks | ruby")[0].size() == 2,
              "a bar is either, the words beside it binding tighter");
        check(matches(parse("#planks"), planks) && !matches(parse("#planks"), log) && matches(parse("-#logs"), planks),
              "a hash searches the tags, also left out with a minus");
        check(matches(parse("$fuel"), planks) && !matches(parse("$fuel"), log) && uses(parse("a $b"), Field::Tooltip) && !uses(parse("ab"), Field::Tooltip),
              "a dollar searches the tooltip, which is read only when asked for");
        check(matches(parse("@gems"), modded) && matches(parse("@minecraft"), log) && !matches(parse("@gems"), log),
              "an at sign searches the id's namespace");
        check(parse("\"#\"")[0][0] == Term{Field::Plain, "#", false} && matches(parse("\"oak planks\""), planks),
              "quotes keep a phrase together and a sign as text");
        check(folded("ＯＡＫ　＃ｐｌａｎｋｓ") == "oak #planks" && folded("ブロック") == folded("ぶろっく") && folded("ブロック") == "ぶろっく",
              "full-width signs and spaces read as ASCII, katakana as hiragana");
        check(matches(parse("ｏａｋ　－ｌｏｇ"), planks), "a search typed full-width works the same");
        History history;
        history.keep("oak"), history.keep("stone"), history.keep("oak");
        check(history.entries == std::vector<std::string>{"stone", "oak"} && history.step(true) == "oak" && history.step(true) == "stone"
                  && history.step(true) == "stone" && history.step(false) == "oak" && history.step(false).empty(),
              "Up steps back through earlier searches without repeats, Down forward to an empty field");
        History live;
        live.typed("st", 0), live.typed("stone", 300), live.typed("stone", 1400), live.typed("stone bri", 1500), live.typed("glass", 1700),
            live.typed("glass", 2800);
        check(live.entries == std::vector<std::string>{"stone", "glass"}, "a search that stood a moment while typed is kept, passing text is not");
        std::string const back = live.step(true);
        live.typed(back, 5000), live.typed(back, 9000);
        check(back == "glass" && live.entries.size() == 2 && live.at == 1, "text Up put in the field is not kept again while stepping");
    }
    {
        Index empty = build({});
        check(empty.items.empty() && choose(empty).recipe.empty() && plan(empty, choose(empty), {}).raw.empty(), "an empty table gives an empty index");
    }
}
