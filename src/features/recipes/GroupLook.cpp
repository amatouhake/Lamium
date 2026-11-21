#include "features/recipes/GroupLook.h"
#include "features/recipes/CountText.h"
#include "features/recipes/RecipeNotes.h"
#include "features/recipes/RecipeText.h"
#include "features/recipes/RecipeViewDraw.h"
#include "features/schematic/SchematicItems.h"
#include "ui/Localization.h"
#include <algorithm>
#include <chrono>

namespace lamium::recipes::groupLook {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto restTime = std::chrono::milliseconds(300); // under that the wheel still scrolls or turns pages
std::uint64_t frame = 0;
struct Hover {
    int group = -1;
    float x = 0, y = 0;
    std::uint64_t frame = 0;
    Clock::time_point since{};
} hovered;
std::map<int, int> looked; // group -> members turned past what it would show
}
void beginFrame() { ++frame; }
void hover(int group, float x, float y) {
    if (hovered.group != group || hovered.x != x || hovered.y != y || hovered.frame + 1 < frame) hovered = {group, x, y, frame, Clock::now()};
    hovered.frame = frame;
}
int rested() {
    if (hovered.group < 0 || hovered.frame + 1 < frame) return -1;
    return Clock::now() - hovered.since >= restTime ? hovered.group : -1;
}
void turn(int group, int steps) {
    if (group >= 0) looked[group] += steps;
}
Item face(Index const& index, int group, std::map<int, Item> const& picks, std::map<Item, Count> const* held) {
    if (group < 0 || group >= static_cast<int>(index.groups.size())) return -1;
    auto const& members = index.groups[static_cast<size_t>(group)].members;
    if (members.empty()) return -1;
    Item base = groupLead(index, group);
    if (auto picked = picks.find(group); picked != picks.end() && std::ranges::find(members, picked->second) != members.end()) base = picked->second;
    else if (held) {
        Count most = 0;
        for (Item member : members)
            if (auto found = held->find(member); found != held->end() && found->second > most) most = found->second, base = member;
    }
    auto const at = looked.find(group);
    if (at == looked.end() || at->second == 0) return base;
    int const n = static_cast<int>(members.size());
    int const start = static_cast<int>(std::ranges::find(members, base) - members.begin());
    return members[static_cast<size_t>(((start + at->second) % n + n) % n)];
}
void tip(view::Late& late, Index const& index, int group, std::map<int, Item> const& picks, std::map<Item, Count> const* held) {
    using ui::translated;
    auto const& members = index.groups[static_cast<size_t>(group)].members;
    Item const shown = face(index, group, picks, held);
    auto heldOf = [&](Item item) -> Count {
        if (!held) return 0;
        auto found = held->find(item);
        return found == held->end() ? 0 : found->second;
    };
    late.tip = groupName(index, group);
    late.tipLines.clear();
    late.tipItems.clear();
    late.tipCounts.clear();
    late.tipCurrent = -1;
    late.hovered = shown >= 0 ? index.items[static_cast<size_t>(shown)] : std::string();
    if (shown >= 0) {
        std::string const name = schematic::items::shown(index.items[static_cast<size_t>(shown)]).name;
        late.tipLines.push_back(held ? translated("recipes.group.showingHeld", name, heldOf(shown)) : translated("recipes.group.showing", name));
    }
    auto picked = picks.find(group);
    late.tipLines.push_back(picked != picks.end()
                                ? translated("recipes.howTo.chosen") + ": " + schematic::items::shown(index.items[static_cast<size_t>(picked->second)]).name
                                : translated("recipes.howTo.any") + ": " + translated("recipes.howTo.anyTip"));
    for (size_t i = 0; i < members.size(); ++i) {
        late.tipItems.push_back(index.items[static_cast<size_t>(members[i])]);
        Count const have = heldOf(members[i]);
        late.tipCounts.push_back(have ? slotCountText(have) : std::string());
        if (members[i] == shown) late.tipCurrent = static_cast<int>(i);
    }
    if (members.size() > 1) late.tipLines.push_back(translated("recipes.group.wheel"));
}
}
