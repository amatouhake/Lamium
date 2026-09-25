#pragma once
#include <array>
#include <cmath>
#include <optional>
namespace lamium::inventory {
struct ToolCandidate { float speed = 1; bool harvests = false; };
inline std::optional<int> chooseHotbarTool(std::array<ToolCandidate,9> const& tools, int selected) {
    if (selected < 0 || selected >= 9) return {};
    auto effective = [](ToolCandidate tool) { return tool.harvests && std::isfinite(tool.speed) && tool.speed > 1; };
    if (effective(tools[selected])) return {};
    std::optional<int> best;
    for (int slot=0; slot<9; ++slot)
        if (effective(tools[slot]) && (!best || tools[slot].speed > tools[*best].speed)) best = slot;
    return best;
}
// The block a held attack is working on. Tool Switch chooses once per new
// block: when breaking starts, and again whenever continued breaking moves to
// another block without the button being released (BACKLOG L-31).
class ToolTarget {
    std::optional<std::array<int,3>> current;
public:
    bool enter(std::array<int,3> pos) {
        if (current == pos) return false;
        current = pos;
        return true;
    }
    void clear() { current.reset(); }
};
}
