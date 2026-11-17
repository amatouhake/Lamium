#include "features/recipes/RawMaterials.h"
void check(bool, char const*);
void rawMaterialsTests() {
    using namespace lamium::recipes::rawMaterials;
    using Raw = std::vector<std::pair<Item, Count>>;
    enum : Item { bricks = 1, stone, cobble, planks, log, sticks, block, ingot, torch, coal };
    Recipes recipes{
        {bricks, {4, {{stone, 4}}}}, {stone, {1, {{cobble, 1}}}},
        {planks, {4, {{log, 1}}}},   {sticks, {4, {{planks, 2}}}},
        {torch, {4, {{sticks, 1}, {coal, 1}}}},
    };
    {
        auto result = reduce({{bricks, 10}}, recipes);
        check(result.raw == Raw{{cobble, 12}}, "a chain is followed down to what has no recipe");
        check(result.steps == std::vector<Step>{{bricks, 3, 2}, {stone, 12, 0}},
              "crafts round up to whole batches and report what is left over");
        check(result.looped.empty(), "a plain chain has no loop");
    }
    {
        auto result = reduce({{sticks, 4}, {planks, 2}}, recipes);
        check(result.raw == Raw{{log, 1}}, "needs for a shared intermediate are added before rounding");
        check(result.steps == std::vector<Step>{{sticks, 1, 0}, {planks, 1, 0}}, "an item comes before what it is made from");
    }
    {
        auto result = reduce({{torch, 5}, {cobble, 5}, {log, 0}}, recipes);
        check(result.raw == Raw{{cobble, 5}, {log, 1}, {coal, 2}}, "raw items asked for directly are kept, zero counts dropped");
        check(result.steps == std::vector<Step>{{torch, 2, 3}, {sticks, 1, 2}, {planks, 1, 2}},
              "several ingredients each get their own chain");
    }
    {
        Recipes loop{{block, {1, {{ingot, 9}}}}, {ingot, {9, {{block, 1}}}}};
        auto result = reduce({{block, 2}}, loop);
        check(result.raw == Raw{{ingot, 18}} && result.looped == std::vector<Item>{ingot},
              "a recipe pair that leads back to itself stops at the item that was not asked for");
        check(result.steps == std::vector<Step>{{block, 2, 0}}, "the asked item is still crafted once");
        auto both = reduce({{block, 1}, {ingot, 1}}, loop);
        check(both.looped.size() == 1 && !both.raw.empty(), "a loop where everything was asked for still ends");
        // A loop (10 <-> 20) beside a chain (5 from 6) that sorts before it: the chain is crafted, not kept raw.
        Recipes beside{{10, {1, {{20, 1}, {5, 1}}}}, {20, {1, {{10, 1}}}}, {5, {1, {{6, 1}}}}};
        auto chain = reduce({{10, 1}}, beside);
        check(chain.looped == std::vector<Item>{20} && chain.raw == Raw{{6, 1}, {20, 1}}
                  && std::find(chain.steps.begin(), chain.steps.end(), Step{5, 1, 0}) != chain.steps.end(),
              "only an item on the loop is kept raw; one made beside it is still crafted (L-342)");
    }
    {
        Recipes odd{{stone, {0, {{cobble, 1}}}}, {planks, {4, {}}}, {sticks, {4, {{planks, 0}}}}};
        auto result = reduce({{stone, 3}, {planks, 3}, {sticks, 1}}, odd);
        check(result.raw == Raw{{stone, 3}, {planks, 3}} && result.steps == std::vector<Step>{{sticks, 1, 3}},
              "a recipe that makes nothing or uses nothing leaves its item raw");
        check(reduce({}, recipes).raw.empty() && reduce({}, recipes).steps.empty(), "nothing wanted, nothing needed");
    }
    {
        auto result = reduce({{bricks, 64}}, recipes, {{bricks, 20}, {log, 9}});
        check(result.steps == std::vector<Step>{{bricks, 11, 0}, {stone, 44, 0}} && result.raw == Raw{{cobble, 44}},
              "held items of what is wanted are used before any is made");
        check(result.fromHeld == Raw{{bricks, 20}}, "what held stock covers is reported, unused stock is not");
        auto enough = reduce({{bricks, 8}}, recipes, {{bricks, 8}});
        check(enough.steps.empty() && enough.raw.empty() && enough.fromHeld == Raw{{bricks, 8}}, "enough held, nothing to do");
    }
    {
        // Stone is wanted directly (3) and through bricks (4): 7 in all, 5 held, 2 to smelt.
        auto result = reduce({{stone, 3}, {bricks, 4}}, recipes, {{stone, 5}});
        check(result.steps == std::vector<Step>{{bricks, 1, 0}, {stone, 2, 0}} && result.raw == Raw{{cobble, 2}},
              "held stock of an item needed two ways comes off once, from the whole need");
        check(result.fromHeld == Raw{{stone, 5}}, "all of it is used when the need is larger");
    }
    {
        auto result = reduce({{torch, 8}}, recipes, {{planks, 1}, {coal, 5}, {sticks, 1}});
        check(result.steps == std::vector<Step>{{torch, 2, 0}, {sticks, 1, 3}, {planks, 1, 3}},
              "a held intermediate shortens the chain below it; a held part of a batch does not");
        check(result.raw == Raw{{log, 1}} && result.fromHeld == Raw{{planks, 1}, {sticks, 1}, {coal, 2}},
              "held raw materials come off the raw total");
        Recipes loop{{block, {1, {{ingot, 9}}}}, {ingot, {9, {{block, 1}}}}};
        auto looped = reduce({{block, 2}}, loop, {{ingot, 10}});
        check(looped.raw == Raw{{ingot, 8}} && looped.fromHeld == Raw{{ingot, 10}}, "an item kept raw by a loop also uses held stock");
    }
}
