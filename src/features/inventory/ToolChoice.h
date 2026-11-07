#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
namespace lamium::inventory {
struct ToolCandidate { float speed = 1; bool harvests = false; bool silk = false; };
// L-318: where a Silk Touch tool counts for a block. Missing: only where nothing
// drops without it (glass, ice). Changed: also where Silk Touch changes the drop
// (stone, ores, grass). Both come from the block's own drop list.
enum class SilkScope { None, Missing, Changed };
namespace detail {
inline bool effective(ToolCandidate tool) { return tool.harvests && std::isfinite(tool.speed) && tool.speed > 1; }
// The fastest Silk Touch tool in [from,to). Where nothing drops without it the
// tool wins even when no tool is effective; where only the drop changes, a tool
// that breaks the block faster is kept.
template <size_t N>
std::optional<int> silkTool(std::array<ToolCandidate,N> const& tools, int from, int to, SilkScope scope, float plainSpeed) {
    if (scope == SilkScope::None) return {};
    std::optional<int> best;
    for (int slot = from; slot < to; ++slot)
        if (tools[slot].silk && (!best || tools[slot].speed > tools[*best].speed)) best = slot;
    if (!best) return {};
    return scope == SilkScope::Missing || tools[*best].speed >= plainSpeed ? best : std::nullopt;
}
}
inline std::optional<int> chooseHotbarTool(std::array<ToolCandidate,9> const& tools, int selected, SilkScope scope = SilkScope::None) {
    if (selected < 0 || selected >= 9) return {};
    if (scope != SilkScope::None && tools[selected].silk) return {};
    std::optional<int> best;
    for (int slot=0; slot<9; ++slot)
        if (detail::effective(tools[slot]) && (!best || tools[slot].speed > tools[*best].speed)) best = slot;
    if (auto silk = detail::silkTool(tools,0,9,scope,best ? tools[*best].speed : 0.f)) return silk;
    if (detail::effective(tools[selected])) return {};
    return best;
}
// L-69: only when neither the selected slot nor the hotbar has an effective
// tool, the fastest one in the main inventory (ties: the higher slot). L-318: a
// Silk Touch tool there is taken for a block whose drop needs one.
inline std::optional<int> chooseInventoryTool(std::array<ToolCandidate,36> const& tools, int selected, SilkScope scope = SilkScope::None) {
    if (selected < 0 || selected >= 9) return {};
    std::array<ToolCandidate,9> hotbar;
    for (int slot=0; slot<9; ++slot) hotbar[slot] = tools[slot];
    if (scope != SilkScope::None) {
        if (tools[selected].silk) return {};
        float plain = 0.f;
        for (int slot=0; slot<36; ++slot)
            if (detail::effective(tools[slot])) plain = std::max(plain, tools[slot].speed);
        if (auto silk = detail::silkTool(tools,9,36,scope,plain)) return silk;
    }
    if (detail::effective(tools[selected]) || chooseHotbarTool(hotbar,selected)) return {};
    std::optional<int> best;
    for (int slot=9; slot<36; ++slot)
        if (detail::effective(tools[slot]) && (!best || tools[slot].speed >= tools[*best].speed)) best = slot;
    return best;
}
// L-318: what a block's drop depends on, from the names its drop list holds.
inline SilkScope scopeOfDrops(std::vector<std::string> plain, std::vector<std::string> touched) {
    std::sort(plain.begin(), plain.end());
    std::sort(touched.begin(), touched.end());
    if (touched.empty()) return SilkScope::None;
    if (plain.empty()) return SilkScope::Missing;
    return plain == touched ? SilkScope::None : SilkScope::Changed;
}
// The setting only adds the group where Silk Touch changes the drop.
inline SilkScope wantedScope(SilkScope dropped, bool includeChanged) {
    return dropped == SilkScope::Changed && !includeChanged ? SilkScope::None : dropped;
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
