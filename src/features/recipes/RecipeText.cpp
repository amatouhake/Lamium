#include "features/recipes/RecipeText.h"
#include "features/schematic/SchematicItems.h"
#include "ui/Localization.h"
#include <format>

namespace lamium::recipes {
std::string stationName(Station station) {
    auto const item = stationItem(station);
    return item.empty() ? std::string("-") : schematic::items::shown(std::string(item)).name;
}
std::string kindName(Kind kind, std::vector<Station> const& stations) {
    static constexpr char const* keys[]{"recipes.kind.crafting", "recipes.kind.cooking", "recipes.kind.stonecutting", "recipes.kind.smithing",
                                        "recipes.kind.other"};
    std::string const name = ui::translated(keys[static_cast<size_t>(kind)]);
    std::string list;
    for (Station s : stations) {
        if (stationItem(s).empty()) continue;
        list += (list.empty() ? "" : ui::translated("recipes.kind.sep")) + stationName(s);
    }
    return list.empty() ? name : ui::translated("recipes.kind.at", name, list);
}
std::string groupName(Index const& index, int group) {
    auto const& entry = index.groups[static_cast<size_t>(group)];
    if (!entry.name.empty()) {
        std::string const key = "recipes.tag." + (entry.name.starts_with("minecraft:") ? entry.name.substr(10) : entry.name);
        if (std::string text = ui::translated(key); text != key) return text;
    }
    if (entry.members.empty()) return entry.name;
    auto const& first = schematic::items::shown(index.items[static_cast<size_t>(entry.members.front())]).name;
    return entry.members.size() == 1 ? first : ui::translated("recipes.andOthers", first);
}
std::string groupNameAny(Index const& index, int group) { return ui::translated("recipes.anyOf", groupName(index, group)); }
std::vector<std::string> stepInputLines(Index const& index, Request const& request, StepLine const& step) {
    std::vector<std::string> out;
    Item const items = static_cast<Item>(index.items.size());
    for (auto const& [id, count] : stepInputs(index, request, step))
        out.push_back(std::format("{}  {}", count, id >= items ? groupName(index, id - items)
                                                             : schematic::items::shown(index.items[static_cast<size_t>(id)]).name));
    return out;
}
}
