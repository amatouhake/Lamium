#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
namespace lamium::inventory {
// used: when this tool last broke a block (0 = no record), for ties (L-318).
struct ToolCandidate { float speed = 1; bool harvests = false; bool silk = false; int fortune = 0; unsigned used = 0; };
// L-318: where a Silk Touch tool counts for a block. Missing: nothing drops
// without it (glass, ice). Changed: it changes the drop (stone, ores, grass).
// Both come from the block's own drop list, as does whether Fortune changes it.
enum class SilkScope { None, Missing, Changed };
struct BlockDrops { SilkScope silk = SilkScope::None; bool fortune = false; };
// The player's drop priority (inventory.toolSwitchDrop), in setting order.
enum class DropPriority { Speed, Fortune, Silk };
// What a block asks of the tool. AnySilk is the guard every priority keeps:
// where nothing drops without Silk Touch, any Silk Touch tool will do.
enum class Want { Speed, AnySilk, Silk, Fortune };
inline Want wantFor(BlockDrops drops, DropPriority priority) {
    if (drops.silk == SilkScope::Missing) return Want::AnySilk;
    if (priority == DropPriority::Silk && drops.silk == SilkScope::Changed) return Want::Silk;
    if (priority == DropPriority::Fortune && drops.fortune) return Want::Fortune;
    return Want::Speed;
}
namespace detail {
inline bool effective(ToolCandidate tool) { return tool.harvests && std::isfinite(tool.speed) && tool.speed > 1; }
inline bool fits(ToolCandidate tool, Want want) {
    switch (want) {
    case Want::AnySilk: return tool.silk;
    case Want::Silk: return tool.silk && tool.harvests;
    case Want::Fortune: return tool.fortune > 0 && tool.harvests && !tool.silk;
    default: return effective(tool);
    }
}
inline float speedOf(ToolCandidate tool) { return std::isfinite(tool.speed) ? tool.speed : 0.f; }
// Positive when a is the better tool: Fortune level first where Fortune is
// wanted, then speed, then the tool used last.
inline int compare(ToolCandidate a, ToolCandidate b, Want want) {
    if (want == Want::Fortune && a.fortune != b.fortune) return a.fortune > b.fortune ? 1 : -1;
    if (speedOf(a) != speedOf(b)) return speedOf(a) > speedOf(b) ? 1 : -1;
    if (a.used != b.used) return a.used > b.used ? 1 : -1;
    return 0;
}
// The best fitting tool in [from,to); a full tie goes to the first slot, or to
// the last one with laterOnTie (the inventory's rule, L-69).
template <size_t N>
std::optional<int> best(std::array<ToolCandidate,N> const& tools, int from, int to, Want want, bool laterOnTie) {
    std::optional<int> found;
    for (int slot = from; slot < to; ++slot) {
        if (!fits(tools[slot], want)) continue;
        int order = found ? compare(tools[slot], tools[*found], want) : 1;
        if (order > 0 || (order == 0 && laterOnTie)) found = slot;
    }
    return found;
}
// A held tool that fits stays, unless a higher Fortune level is on hand.
template <size_t N>
bool heldFits(std::array<ToolCandidate,N> const& tools, int selected, Want want, std::optional<int> pick) {
    if (!fits(tools[selected], want)) return false;
    return want != Want::Fortune || !pick || tools[selected].fortune >= tools[*pick].fortune;
}
}
// L-31: an effective held tool stays; otherwise the fastest hotbar tool. L-318:
// where the block wants Silk Touch or Fortune, a fitting tool comes first, even
// when slower; with none on the hotbar the speed rule applies.
inline std::optional<int> chooseHotbarTool(std::array<ToolCandidate,9> const& tools, int selected, Want want = Want::Speed) {
    if (selected < 0 || selected >= 9) return {};
    if (want != Want::Speed) {
        auto pick = detail::best(tools,0,9,want,false);
        if (detail::heldFits(tools,selected,want,pick)) return {};
        if (pick) return pick;
    }
    if (detail::effective(tools[selected])) return {};
    return detail::best(tools,0,9,Want::Speed,false);
}
// L-69: only when the hotbar has nothing to offer, the best tool in the main
// inventory. L-318: a tool the block wants is fetched before the speed rule;
// under Fortune also over a hotbar Fortune tool of a lower level (maintainer
// 2026-11-08), but not over an equal level.
inline std::optional<int> chooseInventoryTool(std::array<ToolCandidate,36> const& tools, int selected, Want want = Want::Speed) {
    if (selected < 0 || selected >= 9) return {};
    std::array<ToolCandidate,9> hotbar;
    for (int slot=0; slot<9; ++slot) hotbar[slot] = tools[slot];
    if (want == Want::Fortune) {
        auto onHand = detail::best(hotbar,0,9,want,false);
        auto pick = detail::best(tools,9,36,want,true);
        if (pick && (!onHand || tools[*pick].fortune > hotbar[*onHand].fortune)) return pick;
        if (onHand) return {};
    } else if (want != Want::Speed) {
        if (detail::fits(tools[selected],want) || detail::best(hotbar,0,9,want,false)) return {};
        if (auto pick = detail::best(tools,9,36,want,true)) return pick;
    }
    if (detail::effective(tools[selected]) || chooseHotbarTool(hotbar,selected)) return {};
    return detail::best(tools,9,36,Want::Speed,true);
}
// L-318: what a block's drop depends on, from the names its drop list holds.
inline SilkScope scopeOfDrops(std::vector<std::string> plain, std::vector<std::string> touched) {
    std::sort(plain.begin(), plain.end());
    std::sort(touched.begin(), touched.end());
    if (touched.empty()) return SilkScope::None;
    if (plain.empty()) return SilkScope::Missing;
    return plain == touched ? SilkScope::None : SilkScope::Changed;
}
inline bool dropsDiffer(std::vector<std::string> plain, std::vector<std::string> other) {
    std::sort(plain.begin(), plain.end());
    std::sort(other.begin(), other.end());
    return plain != other;
}
// Which tool broke a block last, per inventory slot. A slot whose item changed
// since loses its record; id is any stable identity of the item kind.
class ToolUse {
    std::array<unsigned,36> stamps{};
    std::array<void const*,36> items{};
    unsigned clock = 0;
public:
    void mark(int slot, void const* item) {
        if (slot < 0 || slot >= 36 || !item) return;
        stamps[slot] = ++clock;
        items[slot] = item;
    }
    unsigned last(int slot, void const* item) const {
        return slot >= 0 && slot < 36 && item && items[slot] == item ? stamps[slot] : 0;
    }
    void clear() { *this = {}; }
};
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
