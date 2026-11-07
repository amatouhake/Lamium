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
    // L-318: a Silk Touch tool where the block's drop depends on it.
    std::array<ToolCandidate,9> silk;
    silk[1] = {1,true,true};
    check(chooseHotbarTool(silk,0,SilkScope::Missing) == 1, "where nothing drops without Silk Touch its tool is taken even at walking speed");
    check(chooseHotbarTool(silk,0,SilkScope::None) == std::nullopt, "with the behaviour off nothing is chosen");
    check(chooseHotbarTool(silk,1,SilkScope::Missing) == std::nullopt, "a held Silk Touch tool is not chosen again");
    silk[2] = {4,true,false};
    check(chooseHotbarTool(silk,2,SilkScope::Missing) == 1, "Silk Touch wins over an effective tool where nothing drops without it");
    check(chooseHotbarTool(silk,2,SilkScope::Changed) == std::nullopt, "a faster plain tool is kept where only the drop changes");
    silk[3] = {4,true,true};
    check(chooseHotbarTool(silk,2,SilkScope::Changed) == 3, "equal speed prefers the Silk Touch tool");
    std::array<ToolCandidate,36> bag;
    bag[20] = {1,true,true};
    check(chooseInventoryTool(bag,0,SilkScope::Missing) == 20, "a Silk Touch tool in the inventory is fetched where nothing drops without it");
    check(!chooseInventoryTool(bag,0,SilkScope::None), "no scope leaves the inventory alone");
    check(scopeOfDrops({}, {"glass x1"}) == SilkScope::Missing && scopeOfDrops({"cobblestone x1"}, {"stone x1"}) == SilkScope::Changed
              && scopeOfDrops({"dirt x1"}, {"dirt x1"}) == SilkScope::None && scopeOfDrops({"cobblestone x1"}, {}) == SilkScope::None,
          "the drop lists decide the scope");
    check(wantedScope(SilkScope::Changed,false) == SilkScope::None && wantedScope(SilkScope::Changed,true) == SilkScope::Changed
              && wantedScope(SilkScope::Missing,false) == SilkScope::Missing,
          "the setting only adds the group where the drop changes");
    ToolTarget target;
    check(target.enter({1,2,3}), "the first block is chosen for");
    check(!target.enter({1,2,3}) && !target.enter({1,2,3}), "continuing on the same block does not choose again");
    check(target.enter({1,2,4}) && target.enter({1,2,3}), "each move to another block chooses again, even back to an earlier one");
    target.clear();
    check(target.enter({1,2,3}), "a released attack starts fresh on the same block");
}
