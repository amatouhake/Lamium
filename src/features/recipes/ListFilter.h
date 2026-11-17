#pragma once
#include "features/recipes/RecipeIndex.h"
#include <map>
#include <string>
#include <string_view>
#include <vector>

// L-359: narrowing the item list: by what is typed, and to what can be made
// from what is held. Pure.
namespace lamium::recipes {
// ASCII letters only: the rest of UTF-8 has no case here and compares as it is.
inline std::string lowered(std::string_view text) {
    std::string out(text);
    for (char& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}
// Every word of the query is somewhere in the item's name or its key. `query`
// and both texts are lowered already. An empty query matches everything.
inline bool searchMatches(std::string_view query, std::string_view name, std::string_view key) {
    size_t at = 0;
    while (at < query.size()) {
        while (at < query.size() && query[at] == ' ') ++at;
        size_t end = query.find(' ', at);
        if (end == std::string_view::npos) end = query.size();
        if (end > at) {
            auto const word = query.substr(at, end - at);
            if (name.find(word) == std::string_view::npos && key.find(word) == std::string_view::npos) return false;
        }
        at = end;
    }
    return true;
}
// Per item: whether a crafting recipe for it can be made once from `held`
// as it is, without making anything first. An ingredient any of several
// items satisfies takes them together.
inline std::vector<bool> canMakeNow(Index const& index, std::map<Item, Count> const& held) {
    std::vector<bool> out(index.items.size(), false);
    auto have = [&](Item item) {
        auto found = held.find(item);
        return found == held.end() ? Count{0} : found->second;
    };
    for (auto const& recipe : index.recipes) {
        if (recipe.station != Station::Crafting || out[static_cast<size_t>(recipe.makes)]) continue;
        bool enough = !recipe.uses.empty();
        for (auto const& part : recipe.uses) {
            Count got = 0;
            if (part.group) for (Item member : index.groups[static_cast<size_t>(part.id)].members) got += have(member);
            else got = have(part.id);
            if (got < part.count) { enough = false; break; }
        }
        if (enough) out[static_cast<size_t>(recipe.makes)] = true;
    }
    return out;
}
}
