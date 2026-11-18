#pragma once
#include "features/recipes/RecipeIndex.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// L-359 step 8: the places whose items count as held beside what is carried.
// The ender chest, once it was seen open, and the containers the player marked
// as stores, each with what it held when it was last seen open. Pure; the
// game side reads an open container and says what it saw.
namespace lamium::recipes {
struct Spot {
    int dimension = 0, x = 0, y = 0, z = 0;
    bool operator==(Spot const&) const = default;
};
// Items of one kind in a place, by name and aux as the game gives them (the
// index decides whether the aux tells items apart).
struct Stored {
    std::string name;
    int aux = 0;
    Count count = 0;
    bool operator==(Stored const&) const = default;
};
inline void addStored(std::vector<Stored>& items, std::string_view name, int aux, Count count) {
    if (name.empty() || !count) return;
    auto found = std::find_if(items.begin(), items.end(), [&](Stored const& item) { return item.name == name && item.aux == aux; });
    if (found == items.end()) items.push_back({std::string(name), aux, count}); else found->count += count;
}
struct Place {
    bool ender = false;       // the ender chest: one for the world, wherever it is opened
    Spot at;                  // a store: the block that was open when it was marked
    std::optional<Spot> pair; // a double chest's other half, which opens the same store
    std::string block;        // the block's name, to show it and to tell that another stands there now
    bool counted = true;      // the player's switch
    std::vector<Stored> items; // as last seen open, the contents of shulker boxes in it included
    std::int64_t seen = 0;    // seconds since 1970
    bool is(Spot const& spot) const { return !ender && (at == spot || (pair && *pair == spot)); }
    bool operator==(Place const&) const = default;
};
struct Places {
    static constexpr size_t most = 64, mostItems = 2048;
    // What is carried counts unless the player says no (a plan that is served from a store only).
    bool carried = true;
    std::vector<Place> list; // the ender chest first once it is known
    int store(Spot const& spot) const {
        for (size_t i = 0; i < list.size(); ++i) if (list[i].is(spot)) return static_cast<int>(i);
        return -1;
    }
    int ender() const { return !list.empty() && list.front().ender ? 0 : -1; }
    // What an open container holds. The ender chest is kept from its first
    // sight on; another container only if it is a store, and a store where
    // another block stands now is no store any more. True when anything changed.
    bool see(bool isEnder, Spot const& spot, std::optional<Spot> const& pair, std::string_view block, std::vector<Stored> items, std::int64_t now) {
        if (items.size() > mostItems) items.resize(mostItems);
        if (isEnder) {
            if (ender() < 0) list.insert(list.begin(), Place{true, {}, {}, std::string(block), true, {}, 0});
            auto& place = list.front();
            bool const changed = place.items != items;
            place.items = std::move(items);
            place.seen = now;
            return changed;
        }
        int const index = store(spot);
        if (index < 0) return false;
        auto& place = list[static_cast<size_t>(index)];
        if (place.block != block) { list.erase(list.begin() + index); return true; }
        // Opened by its other half, a store keeps the half it was marked by.
        bool const changed = place.items != items || (place.at == spot && place.pair != pair);
        if (place.at == spot) place.pair = pair;
        place.items = std::move(items);
        place.seen = now;
        return changed;
    }
    // The open container becomes a store. False when it is one, or there are too many.
    bool mark(Spot const& spot, std::optional<Spot> const& pair, std::string_view block, std::vector<Stored> items, std::int64_t now) {
        if (store(spot) >= 0 || (pair && store(*pair) >= 0) || list.size() >= most) return false;
        if (items.size() > mostItems) items.resize(mostItems);
        list.push_back({false, spot, pair, std::string(block), true, std::move(items), now});
        return true;
    }
    // A store is taken out; the ender chest stays and is switched off instead.
    bool unmark(int index) {
        if (index < 0 || index >= static_cast<int>(list.size()) || list[static_cast<size_t>(index)].ender) return false;
        list.erase(list.begin() + index);
        return true;
    }
};
// What the counted places hold, kinds of one name and aux summed.
inline std::vector<Stored> placeTotals(Places const& places) {
    std::vector<Stored> out;
    for (auto const& place : places.list)
        if (place.counted) for (auto const& item : place.items) addStored(out, item.name, item.aux, item.count);
    return out;
}
// The documents: tolerant like the notes' (RecipeNotes.h).
std::string encodePlaces(Places const& places);
Places decodePlaces(std::string_view text);
}
