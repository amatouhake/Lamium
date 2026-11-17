#include "features/recipes/ItemListLayout.h"
#include "features/recipes/RawView.h"
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
        check(index.makers[id("arrow")].size() == 1 && made("arrow") == "arrow", "a recipe that uses its own product is left out");
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
    }
    {
        // The item list right of a 176-wide window at x 232 on a 640x360 screen.
        auto list = itemListRight(640, 360, 232, 176);
        check(list.left == 416 && list.width == 224 && list.columns == 12 && list.rows == 19, "the band runs from the window to the screen's edge in whole cells");
        check(list.perPage() == 228 && list.pages(229) == 2 && list.pages(0) == 1, "pages hold whole screens of cells, and there is always one");
        check(list.hit(list.cellX(13) + 1, list.cellY(13) + 1) == 13 && list.hit(416, 2) == -1 && list.hit(400, 100) == -1,
              "a point is in the cell drawn there, and in none on the bar or outside");
        check(!itemListRight(640, 360, 157, 480).usable(), "no list where the window leaves no room");
    }
    {
        Index empty = build({});
        check(empty.items.empty() && choose(empty).recipe.empty() && plan(empty, choose(empty), {}).raw.empty(), "an empty table gives an empty index");
    }
}
