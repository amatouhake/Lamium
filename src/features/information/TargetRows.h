#pragma once
#include "features/information/TargetInfo.h"
#include <algorithm>
#include <string_view>
namespace lamium::information {
struct TargetRows { std::vector<std::string> lines; size_t omittedStates{}; bool showOmitted{}; };
inline TargetRows targetRows(TargetInfo const& target, bool identifier, int capacity, std::string_view coordinates = {},
                             std::vector<std::string> const& details = {}) {
    TargetRows result;
    if (capacity <= 0) return result;
    result.lines.push_back(target.name);
    if (identifier && capacity > 1) result.lines.push_back(target.identifier);
    if (!coordinates.empty() && result.lines.size() < static_cast<size_t>(capacity))
        result.lines.emplace_back(coordinates);
    for (auto const& detail : details) {
        if (result.lines.size() >= static_cast<size_t>(capacity)) break;
        result.lines.push_back(detail);
    }
    size_t available = static_cast<size_t>(capacity) - result.lines.size();
    size_t shown = std::min({size_t{6},target.states.size(),available});
    if (shown < target.states.size() && available > 0) {
        shown = std::min(shown,available-1);
        result.showOmitted = true;
    }
    result.lines.insert(result.lines.end(),target.states.begin(),target.states.begin()+shown);
    result.omittedStates = target.states.size()-shown;
    return result;
}
}
