#include "features/recipes/RecipeText.h"
#include "features/schematic/SchematicItems.h"
#include "ui/Localization.h"

namespace lamium::recipes {
std::string stationName(Station station) {
    auto const item = stationItem(station);
    return item.empty() ? std::string("-") : schematic::items::shown(std::string(item)).name;
}
std::string groupName(Index const& index, int group) {
    auto const& entry = index.groups[static_cast<size_t>(group)];
    if (!entry.name.empty()) {
        std::string const key = "recipes.tag." + (entry.name.starts_with("minecraft:") ? entry.name.substr(10) : entry.name);
        if (std::string text = ui::translated(key); text != key) return ui::translated("recipes.anyOf", text);
    }
    if (entry.members.empty()) return entry.name;
    auto const& first = schematic::items::shown(index.items[static_cast<size_t>(entry.members.front())]).name;
    return entry.members.size() == 1 ? first : ui::translated("recipes.andOthers", first, entry.members.size() - 1);
}
}
