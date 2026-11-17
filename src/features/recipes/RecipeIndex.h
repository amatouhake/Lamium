#pragma once
#include "features/schematic/RawMaterials.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

// L-359 / L-116: the game's recipes as a snapshot without game types, the
// recipe used for each item unless the player chooses another, and what a
// list of wanted items costs. Items go by name alone: an aux value is not
// kept, so variants of one name (bed colors, tipped arrows) are one item.
namespace lamium::recipes {
using Item = int; // position in Index::items
using Count = std::uint64_t;
enum class Station { Crafting, Stonecutter, Furnace, BlastFurnace, Smoker, Campfire, SoulCampfire, Smithing, Other };
inline bool smelts(Station station) { return station >= Station::Furnace && station <= Station::SoulCampfire; }
inline Station stationOf(std::string_view tag) {
    static constexpr std::pair<std::string_view, Station> known[]{
        {"crafting_table", Station::Crafting}, {"stonecutter", Station::Stonecutter}, {"furnace", Station::Furnace},
        {"blast_furnace", Station::BlastFurnace}, {"smoker", Station::Smoker}, {"campfire", Station::Campfire},
        {"soul_campfire", Station::SoulCampfire}, {"smithing_table", Station::Smithing}};
    for (auto [name, station] : known) if (tag == name) return station;
    return Station::Other;
}

// The table as the game holds it, by name. `uses` has one entry per grid cell
// or ingredient; an empty name is an empty cell. The first of `makes` is the
// product, the rest come with it (a cake's buckets).
struct SourcePart { std::string name; bool tag = false; };
struct SourceRecipe {
    std::string station, id;
    int priority = 0;
    std::vector<std::pair<std::string, Count>> makes;
    std::vector<SourcePart> uses;
};
struct Source {
    std::vector<SourceRecipe> recipes;
    std::map<std::string, std::vector<std::string>> tags; // tag -> member item names
};

struct Part {
    bool group = false; // id is a group, else an item
    int id = 0;
    Count count = 0;
    bool operator==(Part const&) const = default;
};
struct Recipe {
    std::string id;
    Station station = Station::Other;
    int priority = 0;
    Item makes = 0;
    Count count = 1;
    std::vector<std::pair<Item, Count>> extra; // by-products
    std::vector<Part> uses;                    // each item or group once, with how many
};
// Any one member will do: an item tag, or (without a name) the inputs that smelt to the same item.
struct Group { std::string name; std::vector<Item> members; };
struct Index {
    std::vector<std::string> items;
    std::map<std::string, Item, std::less<>> ids;
    std::vector<Group> groups;
    std::vector<Recipe> recipes;
    std::vector<std::vector<int>> makers; // per item: the recipes whose product it is
    Item find(std::string_view name) const {
        auto found = ids.find(name);
        return found == ids.end() ? -1 : found->second;
    }
    // A group nobody picked a member for stands in calculations as an item past the real ones.
    Item groupItem(int group) const { return static_cast<Item>(items.size()) + group; }
};

// Which of two recipes for one item comes first: by station, then the lowest
// priority, then the recipe named after its product, then the id.
inline bool before(Index const& index, Recipe const& a, Recipe const& b) {
    if (a.station != b.station) return a.station < b.station;
    if (a.priority != b.priority) return a.priority < b.priority;
    bool const aNamed = a.id == index.items[a.makes], bNamed = b.id == index.items[b.makes];
    if (aNamed != bNamed) return aNamed;
    return a.id < b.id;
}

// Left out: superseded recipes (the game's "deprecated" tag), recipes without
// a product or ingredients, and recipes that use their own product (dyeing a
// bed, tipping arrows). Smelting recipes that differ only in their one input
// become one recipe with a nameless group (raw iron or either ore).
inline Index build(Source const& source) {
    Index index;
    auto item = [&](std::string const& name) {
        auto [slot, fresh] = index.ids.emplace(name, static_cast<Item>(index.items.size()));
        if (fresh) index.items.push_back(name);
        return slot->second;
    };
    std::map<std::string, int> tagGroups;
    auto group = [&](std::string const& name) {
        if (auto found = tagGroups.find(name); found != tagGroups.end()) return found->second;
        Group made{name, {}};
        if (auto found = source.tags.find(name); found != source.tags.end())
            for (auto const& member : found->second) {
                Item const id = item(member);
                if (std::find(made.members.begin(), made.members.end(), id) == made.members.end()) made.members.push_back(id);
            }
        index.groups.push_back(std::move(made));
        return tagGroups[name] = static_cast<int>(index.groups.size()) - 1;
    };
    std::vector<Recipe> all;
    for (auto const& from : source.recipes) {
        if (from.station == "deprecated" || from.makes.empty() || from.makes.front().first.empty() || !from.makes.front().second)
            continue;
        Recipe recipe{from.id, stationOf(from.station), from.priority, item(from.makes.front().first), from.makes.front().second, {}, {}};
        for (size_t i = 1; i < from.makes.size(); ++i)
            if (!from.makes[i].first.empty() && from.makes[i].second) recipe.extra.emplace_back(item(from.makes[i].first), from.makes[i].second);
        bool own = false;
        for (auto const& part : from.uses) {
            if (part.name.empty()) continue;
            Part const next{part.tag, part.tag ? group(part.name) : item(part.name), 1};
            own = own || (!next.group && next.id == recipe.makes);
            auto same = std::find_if(recipe.uses.begin(), recipe.uses.end(),
                                     [&](Part const& other) { return other.group == next.group && other.id == next.id; });
            if (same != recipe.uses.end()) ++same->count; else recipe.uses.push_back(next);
        }
        if (!own && !recipe.uses.empty()) all.push_back(std::move(recipe));
    }
    // Smelting alternatives: same station, product and amount, one input each.
    std::map<std::tuple<Station, Item, Count>, std::vector<size_t>> alike;
    for (size_t i = 0; i < all.size(); ++i)
        if (smelts(all[i].station) && all[i].uses.size() == 1 && all[i].uses[0].count == 1)
            alike[{all[i].station, all[i].makes, all[i].count}].push_back(i);
    std::vector<bool> dropped(all.size(), false);
    for (auto const& [key, list] : alike) {
        if (list.size() < 2) continue;
        size_t best = list.front();
        Group inputs;
        for (size_t i : list) {
            if (before(index, all[i], all[best])) best = i;
            auto const& part = all[i].uses[0];
            auto add = [&](Item id) {
                if (std::find(inputs.members.begin(), inputs.members.end(), id) == inputs.members.end()) inputs.members.push_back(id);
            };
            if (part.group) for (Item id : index.groups[part.id].members) add(id); else add(part.id);
        }
        index.groups.push_back(std::move(inputs));
        all[best].uses = {Part{true, static_cast<int>(index.groups.size()) - 1, 1}};
        for (size_t i : list) dropped[i] = i != best;
    }
    for (size_t i = 0; i < all.size(); ++i) if (!dropped[i]) index.recipes.push_back(std::move(all[i]));
    index.makers.resize(index.items.size());
    for (size_t i = 0; i < index.recipes.size(); ++i) index.makers[index.recipes[i].makes].push_back(static_cast<int>(i));
    return index;
}

// The recipe each item is made by, unless the player says otherwise.
struct Choice {
    std::vector<int> recipe; // per item: position in Index::recipes, -1 for none
    std::vector<Item> base;  // per item: the base form of its group of convertible items, -1 outside one
};
// Items that convert into each other (nugget, ingot, block) form a group: the
// base is the form most recipes outside the group use, on a tie the form a
// conversion uses several of, then the name. The others are made from the
// base, step by step; the base only by recipes that do not come from the group.
inline Choice choose(Index const& index) {
    size_t const count = index.items.size();
    Choice out{std::vector<int>(count, -1), std::vector<Item>(count, -1)};
    // (made, from): the crafting recipe that makes one item from nothing but another.
    std::map<std::pair<Item, Item>, int> conversion;
    for (size_t i = 0; i < index.recipes.size(); ++i) {
        auto const& recipe = index.recipes[i];
        if (recipe.station != Station::Crafting || recipe.uses.size() != 1 || recipe.uses[0].group) continue;
        auto [slot, fresh] = conversion.emplace(std::pair{recipe.makes, recipe.uses[0].id}, static_cast<int>(i));
        if (!fresh && before(index, recipe, index.recipes[slot->second])) slot->second = static_cast<int>(i);
    }
    std::vector<Item> root(count);
    std::iota(root.begin(), root.end(), 0);
    auto find = [&](Item item) {
        while (root[item] != item) item = root[item] = root[root[item]];
        return item;
    };
    std::vector<bool> grouped(count, false);
    for (auto const& [key, recipe] : conversion)
        if (conversion.contains({key.second, key.first})) {
            root[find(key.first)] = find(key.second);
            grouped[key.first] = grouped[key.second] = true;
        }
    std::vector<int> used(count, 0);       // recipes outside the item's group that use it
    std::vector<bool> several(count, false); // a conversion uses more than one of it
    for (auto const& recipe : index.recipes)
        for (auto const& part : recipe.uses)
            if (!part.group && grouped[part.id] && find(part.id) != find(recipe.makes)) ++used[part.id];
    for (auto const& [key, recipe] : conversion)
        if (grouped[key.first] && grouped[key.second] && index.recipes[recipe].uses[0].count > 1) several[key.second] = true;
    std::map<Item, std::vector<Item>> families;
    for (Item item = 0; item < static_cast<Item>(count); ++item) if (grouped[item]) families[find(item)].push_back(item);
    for (auto const& [key, members] : families) {
        Item const base = *std::min_element(members.begin(), members.end(), [&](Item a, Item b) {
            if (used[a] != used[b]) return used[a] > used[b];
            if (several[a] != several[b]) return several[a] > several[b];
            return index.items[a] < index.items[b];
        });
        for (Item member : members) out.base[member] = base;
        std::vector<Item> open{base};
        std::set<Item> reached{base};
        for (size_t at = 0; at < open.size(); ++at)
            for (Item member : members) {
                auto made = conversion.find({member, open[at]});
                if (made == conversion.end() || !reached.insert(member).second) continue;
                out.recipe[member] = made->second;
                open.push_back(member);
            }
    }
    for (Item item = 0; item < static_cast<Item>(count); ++item) {
        if (out.base[item] >= 0 && out.base[item] != item) continue;
        for (int candidate : index.makers[item]) {
            auto const& recipe = index.recipes[candidate];
            bool const fromGroup = out.base[item] == item
                && std::all_of(recipe.uses.begin(), recipe.uses.end(), [&](Part const& part) { return !part.group && out.base[part.id] == item; });
            if (fromGroup) continue;
            if (out.recipe[item] < 0 || before(index, recipe, index.recipes[out.recipe[item]])) out.recipe[item] = candidate;
        }
    }
    return out;
}

// What the player wants, holds and has chosen. A picked member stands for its
// group everywhere; an item in `stops` is gathered, not made.
struct Request {
    std::vector<std::pair<Item, Count>> wanted, held;
    std::map<int, Item> picks; // group -> member
    std::set<Item> stops;
};
using Result = schematic::rawMaterials::Result;
// Item ids in the result past Index::items are groups (Index::groupItem). A
// group is covered by what is left of its members after their own needs, so
// held planks are not counted for planks and for "any planks" both.
inline Result plan(Index const& index, Choice const& choice, Request const& request) {
    namespace raw = schematic::rawMaterials;
    Item const count = static_cast<Item>(index.items.size());
    raw::Recipes recipes;
    for (Item item = 0; item < count; ++item) {
        int const chosen = choice.recipe[item];
        if (chosen < 0 || request.stops.contains(item)) continue;
        auto const& recipe = index.recipes[chosen];
        raw::Recipe to{recipe.count, {}};
        for (auto const& part : recipe.uses) {
            Item id = part.id;
            if (part.group) {
                auto picked = request.picks.find(part.id);
                id = picked != request.picks.end() ? picked->second : index.groupItem(part.id);
            }
            auto same = std::find_if(to.uses.begin(), to.uses.end(), [&](auto const& use) { return use.first == id; });
            if (same != to.uses.end()) same->second += part.count; else to.uses.emplace_back(id, part.count);
        }
        recipes.emplace(item, std::move(to));
    }
    auto first = raw::reduce(request.wanted, recipes, request.held);
    std::map<Item, Count> left;
    for (auto [item, held] : request.held) left[item] += held;
    for (auto [item, used] : first.fromHeld) left[item] -= std::min(left[item], used);
    auto held = request.held;
    for (auto [item, need] : first.raw) {
        if (item < count || item - count >= static_cast<Item>(index.groups.size())) continue;
        Count take = 0;
        for (Item member : index.groups[item - count].members) {
            Count const use = std::min(left[member], need - take);
            left[member] -= use;
            take += use;
        }
        if (take) held.emplace_back(item, take);
    }
    return held.size() == request.held.size() ? first : raw::reduce(request.wanted, recipes, held);
}
}
