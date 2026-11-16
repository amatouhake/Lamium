#include "features/schematic/RawMaterials.h"
void check(bool, char const*);
void rawMaterialsTests() {
    using namespace lamium::schematic::rawMaterials;
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
}
