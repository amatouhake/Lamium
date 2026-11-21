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

// L-359: what the player enters. Notes are kept for all worlds: favorites,
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
    constexpr std::string_view stations[]{"crafting", "stonecutter", "furnace", "blast_furnace", "smoker", "campfire", "soul_campfire", "smithing",
                                          "brewing_stand", "other"};
    return std::string(stations[static_cast<size_t>(recipe.station)]) + "|" + recipe.id;
}
// Items usually gathered, not made (L-370): their recipe turns something rarer into them (rabbit hide into
// leather, string into wool). They start as gathered in "How to make"; only the player's change is saved
// (`made`), so an item added here later reaches everyone and a choice to make one stays.
inline bool usuallyGathered(std::string_view key) {
    if (key.starts_with("minecraft:")) key.remove_prefix(10);
    return key == "leather" || key == "white_wool";
}
struct Notes {
    std::vector<std::string> favorites;          // item keys, oldest first
    std::map<std::string, std::string> choices;   // item key -> the recipe's choiceKey
    std::map<std::string, std::string> picks;     // group key -> member's item key
    std::set<std::string> stops;                  // item keys
    std::set<std::string> made;                   // usually gathered items the player set to be made
    bool operator==(Notes const&) const = default;
    // Gathered, not made: the player's choice, or an item usually gathered the player left so.
    bool gathered(std::string const& key) const { return stops.contains(key) || (usuallyGathered(key) && !made.contains(key)); }
    void setGathered(std::string const& key, bool gather) {
        if (gather) {
            made.erase(key);
            if (!usuallyGathered(key)) stops.insert(key); else stops.erase(key);
        } else {
            stops.erase(key);
            if (usuallyGathered(key)) made.insert(key);
        }
    }
    bool favorite(std::string_view key) const { return std::find(favorites.begin(), favorites.end(), key) != favorites.end(); }
    void toggleFavorite(std::string const& key) {
        auto found = std::find(favorites.begin(), favorites.end(), key);
        if (found != favorites.end()) favorites.erase(found);
        else if (!key.empty() && favorites.size() < 512) favorites.push_back(key);
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
    for (size_t item = 0; item < index.items.size(); ++item)
        if (usuallyGathered(index.items[item]) && !notes.made.contains(index.items[item])) request.stops.insert(static_cast<Item>(item));
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
    // In the list, also at 0: the wheel took it down to 0, and it stays shown until the pointer leaves it
    // (settle), so a turn too far can be taken back. The plan and the saved file leave such an item out.
    bool listed(std::string_view key) const {
        return std::any_of(targets.begin(), targets.end(), [&](auto const& target) { return target.first == key; });
    }
    void remove(std::string const& key) {
        auto found = std::find_if(targets.begin(), targets.end(), [&](auto const& target) { return target.first == key; });
        if (found == targets.end()) return;
        if (found->second > 0) remembered[key] = found->second;
        targets.erase(found);
    }
    void zero(std::string const& key) {
        for (auto& [name, held] : targets) if (name == key) { if (held > 0) remembered[key] = held; held = 0; return; }
    }
    // Items at 0 go out, but `keep` (the one under the pointer). True when one went.
    bool settle(std::string_view keep) {
        return std::erase_if(targets, [&](auto const& target) { return target.second == 0 && target.first != keep; }) > 0;
    }
    // In with the count it had before, or one; out when it is in; back to that count when it is at 0.
    void toggle(std::string const& key) {
        if (has(key)) { remove(key); return; }
        if (listed(key)) { auto before = remembered.find(key); set(key, before == remembered.end() ? Count{1} : before->second); return; }
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
// A wanted item's count after its stepper moved. Down from one is 0: the minus button takes the item out
// at once, the wheel leaves it at 0 until the pointer leaves it (Wanted::zero). A step of several (Shift:
// the item's stack) goes to the next multiple of it, so 1 becomes 64 and 128, not 65 and 129, and down
// stops at one before 0.
inline Count steppedWanted(Count now, bool up, Count step) {
    step = std::max<Count>(step, 1);
    if (up) return std::min<Count>((now / step + 1) * step, Wanted::most);
    if (now <= 1) return 0;
    return std::max<Count>((now - 1) / step * step, 1);
}
// A target a feature handed over (a schematic's materials still to place): what, how many and whose it
// is. It stands among the wanted items and is not the player's to edit.
struct Handed {
    std::string key;
    Count count = 0;
    std::string source; // the feature's name for it, a translation key's last part ("schematic")
    bool operator==(Handed const&) const = default;
};
// The wanted items and what features handed over as a request's targets for this table, an item wanted
// by both once with the sum; what the table lacks is left out.
inline void apply(Index const& index, Wanted const& wanted, std::vector<Handed> const& handed, Request& request) {
    auto add = [&](std::string const& key, Count amount) {
        Item const item = index.findKey(key);
        if (item < 0 || !amount) return;
        auto found = std::find_if(request.wanted.begin(), request.wanted.end(), [&](auto const& target) { return target.first == item; });
        if (found == request.wanted.end()) request.wanted.emplace_back(item, amount); else found->second += amount;
    };
    for (auto const& [key, amount] : wanted.targets) add(key, amount);
    for (auto const& target : handed) add(target.key, target.count);
}

// A wanted item the recipe table does not know, because no recipe makes or uses it (a grass block): it
// is gathered as it is, and stands in "to gather" beside what the recipes ask for. `heldByName`: what
// counts as held, by item name.
struct LooseLine {
    std::string key;
    Count need = 0, held = 0, left = 0;
    bool operator==(LooseLine const&) const = default;
};
inline std::vector<LooseLine> looseLines(Index const& index, Wanted const& wanted, std::vector<Handed> const& handed,
                                         std::map<std::string, Count> const& heldByName) {
    std::vector<LooseLine> out;
    auto add = [&](std::string const& key, Count amount) {
        if (key.empty() || !amount || index.findKey(key) >= 0) return;
        auto found = std::find_if(out.begin(), out.end(), [&](LooseLine const& line) { return line.key == key; });
        if (found == out.end()) out.push_back({key, amount, 0, 0}); else found->need += amount;
    };
    for (auto const& [key, amount] : wanted.targets) add(key, amount);
    for (auto const& target : handed) add(target.key, target.count);
    for (auto& line : out) {
        auto const have = heldByName.find(std::string(splitItemKey(line.key).name));
        line.held = std::min(line.need, have == heldByName.end() ? Count{0} : have->second);
        line.left = line.need - line.held;
    }
    return out;
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
