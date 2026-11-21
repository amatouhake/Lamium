#include "features/recipes/RecipeText.h"
#include "features/recipes/CountText.h"
#include "features/recipes/PlanTargets.h"
#include "features/schematic/SchematicItems.h"
#include "ui/Localization.h"
#include <format>

namespace lamium::recipes {
std::string stationName(Station station) {
    auto const item = stationItem(station);
    return item.empty() ? std::string("-") : schematic::items::shown(std::string(item)).name;
}
std::string kindName(Kind kind) {
    static constexpr char const* keys[]{"recipes.kind.crafting", "recipes.kind.smelting", "recipes.kind.blasting", "recipes.kind.smoking",
                                        "recipes.kind.campfire", "recipes.kind.stonecutting", "recipes.kind.smithing", "recipes.kind.brewing",
                                        "recipes.kind.other"};
    return ui::translated(keys[static_cast<size_t>(kind)]);
}
std::string groupName(Index const& index, int group) {
    auto const& entry = index.groups[static_cast<size_t>(group)];
    if (!entry.name.empty()) {
        std::string const key = "recipes.tag." + (entry.name.starts_with("minecraft:") ? entry.name.substr(10) : entry.name);
        if (std::string text = ui::translated(key); text != key) return text;
    }
    if (entry.members.empty()) return entry.name;
    auto const& first = schematic::items::shown(index.items[static_cast<size_t>(groupLead(index, group))]).name;
    return entry.members.size() == 1 ? first : ui::translated("recipes.andOthers", first);
}
std::string countsLine(char const* lead, Count held, Count need) {
    std::string line = ui::translated(lead) + "/" + ui::translated("recipes.col.need") + ": " + wholeCountText(static_cast<std::uint64_t>(held)) + "/"
                     + wholeCountText(static_cast<std::uint64_t>(need));
    if (held < need) line += "  " + ui::translated("recipes.stepShort", wholeCountText(static_cast<std::uint64_t>(need - held)));
    return line;
}
std::vector<std::string> stepInputLines(Index const& index, Request const& request, StepLine const& step) {
    std::vector<std::string> out;
    Item const items = static_cast<Item>(index.items.size());
    for (auto const& [id, count] : stepInputs(index, request, step))
        out.push_back(std::format("{}: {}", id >= items ? groupName(index, id - items) : schematic::items::shown(index.items[static_cast<size_t>(id)]).name,
                                  wholeCountText(static_cast<std::uint64_t>(count))));
    return out;
}
std::vector<std::string> stepInputLines(Index const& index, Request const& request, StepLine const& step, std::map<Item, Count> const& stock) {
    std::vector<std::string> out;
    Item const items = static_cast<Item>(index.items.size());
    auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
    for (size_t i = 0; i < recipe.uses.size(); ++i) {
        Item const id = partItem(index, request, recipe.uses[i]);
        Count const need = partNeed(recipe, i, step.crafts), have = stockOf(index, request, recipe.uses[i], stock);
        std::string const name = id >= items ? groupName(index, id - items) : schematic::items::shown(index.items[static_cast<size_t>(id)]).name;
        // Name first, then held/needed (L-370, 2026-11-22): "Stone: 146/44,844".
        std::string line = std::format("{}: {}/{}", name, wholeCountText(static_cast<std::uint64_t>(std::min(have, need))), wholeCountText(static_cast<std::uint64_t>(need)));
        if (have < need) line += "  " + ui::translated("recipes.stepShort", wholeCountText(static_cast<std::uint64_t>(need - have)));
        out.push_back(std::move(line));
    }
    return out;
}
}
