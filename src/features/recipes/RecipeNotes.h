#pragma once
#include "features/recipes/RecipeIndex.h"
#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// L-359: what the player enters. Notes are kept for all worlds: favourites,
// the recipe an item is made by where it is not the default, the member
// picked for an ingredient any of several items satisfies, and the items
// marked as gathered, not made. Wanted is the hand-filled list of one world.
// All by item key and names, never by position in a table: an entry the
// current world's table lacks is ignored there and kept. Pure but for the
// documents in RecipeNotesStore.cpp.
namespace lamium::recipes {
// A group as notes name it: a tag or a variant group by its name, a group of
// smelting inputs by its members.
inline std::string groupKey(Index const& index, int group) {
    auto const& entry = index.groups[static_cast<size_t>(group)];
    if (!entry.name.empty()) return entry.name;
    std::vector<std::string> members;
    for (Item member : entry.members) members.push_back(index.items[static_cast<size_t>(member)]);
    std::sort(members.begin(), members.end());
    std::string out = "items:";
    for (auto const& member : members) out += member + ",";
    return out;
}
// A recipe as notes name it: where it is made and its id. The game gives one id to the same smelting at
// the furnace, the smoker and the campfires, so the id alone does not say which.
inline std::string choiceKey(Recipe const& recipe) {
    constexpr std::string_view stations[]{"crafting", "stonecutter", "furnace", "blast_furnace", "smoker", "campfire", "soul_campfire", "smithing", "other"};
    return std::string(stations[static_cast<size_t>(recipe.station)]) + "|" + recipe.id;
}
struct Notes {
    std::vector<std::string> favourites;          // item keys, oldest first
    std::map<std::string, std::string> choices;   // item key -> the recipe's choiceKey
    std::map<std::string, std::string> picks;     // group key -> member's item key
    std::set<std::string> stops;                  // item keys
    bool operator==(Notes const&) const = default;
    bool favourite(std::string_view key) const { return std::find(favourites.begin(), favourites.end(), key) != favourites.end(); }
    void toggleFavourite(std::string const& key) {
        auto found = std::find(favourites.begin(), favourites.end(), key);
        if (found != favourites.end()) favourites.erase(found);
        else if (!key.empty() && favourites.size() < 512) favourites.push_back(key);
    }
};
// The default choice with the player's recipes put in, where the table still has them.
inline Choice chosen(Index const& index, Choice choice, Notes const& notes) {
    for (auto const& [key, recipe] : notes.choices) {
        Item const item = index.findKey(key);
        if (item < 0) continue;
        for (int maker : index.makers[static_cast<size_t>(item)])
            if (choiceKey(index.recipes[static_cast<size_t>(maker)]) == recipe) choice.recipe[static_cast<size_t>(item)] = maker;
    }
    return choice;
}
// The notes as a request's picks and stops for this table.
inline void apply(Index const& index, Notes const& notes, Request& request) {
    for (size_t group = 0; group < index.groups.size(); ++group) {
        auto picked = notes.picks.find(groupKey(index, static_cast<int>(group)));
        if (picked == notes.picks.end()) continue;
        Item const member = index.findKey(picked->second);
        auto const& members = index.groups[group].members;
        if (member >= 0 && std::find(members.begin(), members.end(), member) != members.end()) request.picks[static_cast<int>(group)] = member;
    }
    for (auto const& key : notes.stops) if (Item const item = index.findKey(key); item >= 0) request.stops.insert(item);
}

struct Wanted {
    static constexpr Count most = 999999;
    std::vector<std::pair<std::string, Count>> targets; // item key and how many, in the order added
    // What a target taken out had, so putting it back restores it; for the session, not saved.
    std::map<std::string, Count> remembered;
    Count count(std::string_view key) const {
        for (auto const& [name, amount] : targets) if (name == key) return amount;
        return 0;
    }
    bool has(std::string_view key) const { return count(key) > 0; }
    void remove(std::string const& key) {
        auto found = std::find_if(targets.begin(), targets.end(), [&](auto const& target) { return target.first == key; });
        if (found == targets.end()) return;
        remembered[key] = found->second;
        targets.erase(found);
    }
    // In with the count it had before, or one; out when it is in.
    void toggle(std::string const& key) {
        if (has(key)) { remove(key); return; }
        if (key.empty() || targets.size() >= 512) return;
        auto before = remembered.find(key);
        targets.emplace_back(key, before == remembered.end() ? Count{1} : before->second);
    }
    void set(std::string const& key, Count amount) {
        amount = std::clamp<Count>(amount, 1, most);
        for (auto& [name, held] : targets) if (name == key) { held = amount; return; }
        if (!key.empty() && targets.size() < 512) targets.emplace_back(key, amount);
    }
};
// A wanted item's count after its stepper moved: 0 takes the item out. Only the minus button at one
// does that; the wheel and a step of several stop at one.
inline Count steppedWanted(Count now, bool up, Count step, bool button) {
    if (up) return std::min<Count>(now + step, Wanted::most);
    if (now <= 1) return button ? 0 : 1;
    return now > step ? now - step : 1;
}
// The wanted items as a request's targets for this table; what it lacks is left out.
inline void apply(Index const& index, Wanted const& wanted, Request& request) {
    for (auto const& [key, amount] : wanted.targets) if (Item const item = index.findKey(key); item >= 0) request.wanted.emplace_back(item, amount);
}

// The documents. Reading tolerates anything: what is not understood is left out.
std::string encodeNotes(Notes const& notes);
Notes decodeNotes(std::string_view text);
std::string encodeWanted(Wanted const& wanted);
Wanted decodeWanted(std::string_view text);
// Whether a document is one of these at all; one that is not is left alone, never replaced.
bool documentReadable(std::string_view text);
// Empty when the file is not there. `unreadable`: it is there and could not be read.
std::optional<std::string> readDocument(std::filesystem::path const& path, bool& unreadable);
}
