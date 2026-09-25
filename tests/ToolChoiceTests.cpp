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
    ToolTarget target;
    check(target.enter({1,2,3}), "the first block is chosen for");
    check(!target.enter({1,2,3}) && !target.enter({1,2,3}), "continuing on the same block does not choose again");
    check(target.enter({1,2,4}) && target.enter({1,2,3}), "each move to another block chooses again, even back to an earlier one");
    target.clear();
    check(target.enter({1,2,3}), "a released attack starts fresh on the same block");
}
