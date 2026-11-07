#include "features/inventory/ToolChoice.h"
#include <limits>
void check(bool, char const*);
void toolChoiceTests() {
    using namespace lamium::inventory;
    std::array<ToolCandidate,9> tools;
    check(!chooseHotbarTool(tools,0), "empty hotbar keeps selected slot");
    tools[2] = {4,true}; tools[8] = {8,true};
    check(chooseHotbarTool(tools,0) == 8, "ineffective held item selects effective hotbar tool");
    check(!chooseHotbarTool(tools,2), "effective held tool stays selected even with a faster alternative");
    tools[8].harvests = false;
    check(chooseHotbarTool(tools,0) == 2, "fast tool unable to harvest is not selected");
    check(chooseHotbarTool(tools,8) == 2, "wrong tool tier does not count as effective");
    tools[4] = {4,true};
    check(chooseHotbarTool(tools,0) == 2, "equal candidates resolve to first hotbar slot");
    tools[2].speed = std::numeric_limits<float>::quiet_NaN();
    tools[4].speed = std::numeric_limits<float>::infinity();
    check(!chooseHotbarTool(tools,0), "non-finite tool speeds cannot drive selection");
    check(!chooseHotbarTool(tools,-1) && !chooseHotbarTool(tools,9), "selection outside hotbar is never changed");
    // L-318: what the block asks of the tool, from its drops and the priority.
    BlockDrops glass{SilkScope::Missing,false}, stone{SilkScope::Changed,false}, ore{SilkScope::Changed,true}, dirt{};
    check(wantFor(glass,DropPriority::Speed) == Want::AnySilk && wantFor(glass,DropPriority::Fortune) == Want::AnySilk,
          "where nothing drops without Silk Touch every priority takes it");
    check(wantFor(stone,DropPriority::Speed) == Want::Speed && wantFor(stone,DropPriority::Silk) == Want::Silk
              && wantFor(stone,DropPriority::Fortune) == Want::Speed,
          "a changed drop only counts under the Silk Touch priority; Fortune falls back to speed where it does nothing");
    check(wantFor(ore,DropPriority::Fortune) == Want::Fortune && wantFor(ore,DropPriority::Silk) == Want::Silk
              && wantFor(dirt,DropPriority::Silk) == Want::Speed,
          "Fortune and Silk Touch apply where the drop list says they matter");
    std::array<ToolCandidate,9> silk;
    silk[1] = {1,true,true};
    check(chooseHotbarTool(silk,0,Want::AnySilk) == 1, "where nothing drops without Silk Touch its tool is taken even at walking speed");
    check(chooseHotbarTool(silk,0,Want::Speed) == std::nullopt, "the speed rule ignores an ineffective Silk Touch tool");
    check(chooseHotbarTool(silk,1,Want::AnySilk) == std::nullopt, "a held Silk Touch tool is not chosen again");
    silk[2] = {4,true,false};
    check(chooseHotbarTool(silk,2,Want::AnySilk) == 1, "Silk Touch wins over an effective tool where nothing drops without it");
    check(chooseHotbarTool(silk,2,Want::Silk) == 1, "the Silk Touch priority takes a slower Silk Touch tool");
    silk[1].harvests = false;
    check(chooseHotbarTool(silk,2,Want::Silk) == std::nullopt, "a Silk Touch tool that cannot harvest the block never counts");
    silk[3] = {4,true,true};
    check(chooseHotbarTool(silk,2,Want::Silk) == 3, "a harvesting Silk Touch tool is taken");
    std::array<ToolCandidate,9> luck;
    luck[0] = {8,true}; luck[1] = {6,true,false,1}; luck[2] = {4,true,false,3}; luck[3] = {8,true,true};
    check(chooseHotbarTool(luck,0,Want::Fortune) == 2, "the highest Fortune level wins over speed");
    check(chooseHotbarTool(luck,2,Want::Fortune) == std::nullopt, "a held tool with the highest Fortune level stays");
    check(chooseHotbarTool(luck,1,Want::Fortune) == 2, "a held lower Fortune level gives way to a higher one");
    luck[2].harvests = false;
    check(chooseHotbarTool(luck,0,Want::Fortune) == 1, "a Fortune tool that cannot harvest the block never counts");
    luck[1] = {}; luck[2] = {};
    check(chooseHotbarTool(luck,5,Want::Fortune) == 0, "without a Fortune tool the speed rule applies");
    // A tie goes to the tool used last, then to the first slot.
    std::array<ToolCandidate,9> tie;
    tie[2] = {8,true}; tie[6] = {8,true};
    check(chooseHotbarTool(tie,0) == 2, "with no record the first slot wins a tie");
    tie[6].used = 5;
    check(chooseHotbarTool(tie,0) == 6, "the tool used last wins a tie");
    tie[2].speed = 9;
    check(chooseHotbarTool(tie,0) == 2, "a faster tool still wins over the one used last");
    std::array<ToolCandidate,36> bag;
    bag[20] = {1,true,true};
    check(chooseInventoryTool(bag,0,Want::AnySilk) == 20, "a Silk Touch tool in the inventory is fetched where nothing drops without it");
    check(!chooseInventoryTool(bag,0,Want::Speed), "the speed rule leaves an ineffective inventory tool alone");
    bag[4] = {1,true,true};
    check(!chooseInventoryTool(bag,0,Want::AnySilk), "a Silk Touch tool on the hotbar is not fetched over");
    bag[4] = {}; bag[0] = {8,true}; bag[30] = {4,true,false,2};
    check(chooseInventoryTool(bag,0,Want::Fortune) == 30, "a Fortune tool is fetched over an effective held tool under the Fortune priority");
    bag[12] = {4,true}; bag[14] = {4,true};
    bag[0] = {}; bag[30] = {};
    check(chooseInventoryTool(bag,0,Want::Speed) == 14, "inventory ties still go to the higher slot");
    bag[12].used = 3;
    check(chooseInventoryTool(bag,0,Want::Speed) == 12, "the inventory tool used last wins a tie");
    check(scopeOfDrops({}, {"glass x1"}) == SilkScope::Missing && scopeOfDrops({"cobblestone x1"}, {"stone x1"}) == SilkScope::Changed
              && scopeOfDrops({"dirt x1"}, {"dirt x1"}) == SilkScope::None && scopeOfDrops({"cobblestone x1"}, {}) == SilkScope::None,
          "the drop lists decide the scope");
    check(dropsDiffer({"raw_iron x1"}, {"raw_iron x4"}) && !dropsDiffer({"a x1","b x1"}, {"b x1","a x1"}),
          "Fortune counts where the drop list changes, in any order");
    ToolUse use;
    int pickaxe = 0, shovel = 0;
    check(use.last(3,&pickaxe) == 0, "no record before a break");
    use.mark(3,&pickaxe); use.mark(5,&pickaxe);
    check(use.last(5,&pickaxe) > use.last(3,&pickaxe), "a later break has the higher record");
    check(use.last(3,&shovel) == 0, "a slot whose item changed loses its record");
    use.clear();
    check(use.last(5,&pickaxe) == 0, "clearing forgets every record");
    ToolTarget target;
    check(target.enter({1,2,3}), "the first block is chosen for");
    check(!target.enter({1,2,3}) && !target.enter({1,2,3}), "continuing on the same block does not choose again");
    check(target.enter({1,2,4}) && target.enter({1,2,3}), "each move to another block chooses again, even back to an earlier one");
    target.clear();
    check(target.enter({1,2,3}), "a released attack starts fresh on the same block");
}
