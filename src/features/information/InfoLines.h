#pragma once
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::information {
// Ordered info-line model (BACKLOG L-04b; providers extended by L-05).
// Switches stay the existing Information flags; only the order is saved.
inline constexpr auto infoLineIds = std::to_array<std::string_view>(
    {"coordinates", "dimension", "biome", "facing", "fps", "frameTime", "light", "ping"});
inline std::vector<std::string> defaultLineOrder() {
    return {infoLineIds.begin(), infoLineIds.end()};
}
// Stored order wins for known ids; missing known ids append in default order;
// unknown ids drop out (typos must not pin a dead row).
inline std::vector<std::string> mergeLineOrder(std::vector<std::string> stored) {
    std::vector<std::string> result;
    for (auto const& id : stored)
        if (std::find(infoLineIds.begin(), infoLineIds.end(), id) != infoLineIds.end()
            && std::find(result.begin(), result.end(), id) == result.end())
            result.push_back(id);
    for (auto id : infoLineIds)
        if (std::find(result.begin(), result.end(), id) == result.end())
            result.emplace_back(id);
    return result;
}
}
