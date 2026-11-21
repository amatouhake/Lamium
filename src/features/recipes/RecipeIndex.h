#pragma once
#include "features/recipes/ItemKey.h"
#include "features/recipes/RawMaterials.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

// L-359 / L-116: the game's recipes as a snapshot without game types, the
// recipe used for each item unless the player chooses another, and what a
// list of wanted items costs. Items go by their key (ItemKey.h): the name,
// with the aux value where the table tells variants of one name apart.
namespace lamium::recipes {
using Item = int; // position in Index::items
using Count = std::uint64_t;
enum class Station { Crafting, Stonecutter, Furnace, BlastFurnace, Smoker, Campfire, SoulCampfire, Smithing, BrewingStand, Other };
inline bool smelts(Station station) { return station >= Station::Furnace && station <= Station::SoulCampfire; }
// Kinds of work (L-376): the recipe view's tabs, one per process as other viewers keep them. The game files one
// cooking recipe under several stations' tags; it shows in the tab of each process that does it.
enum class Kind { Crafting, Smelting, Blasting, Smoking, CampfireCooking, Stonecutting, Smithing, Brewing, Other };
inline Kind kindOf(Station station) {
    switch (station) {
    case Station::Furnace: return Kind::Smelting;
    case Station::BlastFurnace: return Kind::Blasting;
    case Station::Smoker: return Kind::Smoking;
    case Station::Campfire: case Station::SoulCampfire: return Kind::CampfireCooking;
    case Station::Crafting: return Kind::Crafting;
    case Station::Stonecutter: return Kind::Stonecutting;
    case Station::Smithing: return Kind::Smithing;
    case Station::BrewingStand: return Kind::Brewing;
    default: return Kind::Other;
    }
}
// The station a kind is known by: its tab's picture and its layout.
inline Station usualStation(Kind kind) {
    switch (kind) {
    case Kind::Crafting: return Station::Crafting;
    case Kind::Smelting: return Station::Furnace;
    case Kind::Blasting: return Station::BlastFurnace;
    case Kind::Smoking: return Station::Smoker;
    case Kind::CampfireCooking: return Station::Campfire;
    case Kind::Stonecutting: return Station::Stonecutter;
    case Kind::Smithing: return Station::Smithing;
    case Kind::Brewing: return Station::BrewingStand;
    case Kind::Other: break;
    }
    return Station::Other;
}
inline Station stationOf(std::string_view tag) {
    static constexpr std::pair<std::string_view, Station> known[]{
        {"crafting_table", Station::Crafting}, {"stonecutter", Station::Stonecutter}, {"furnace", Station::Furnace},
        {"blast_furnace", Station::BlastFurnace}, {"smoker", Station::Smoker}, {"campfire", Station::Campfire},
        {"soul_campfire", Station::SoulCampfire}, {"smithing_table", Station::Smithing}, {"brewing_stand", Station::BrewingStand}};
    for (auto [name, station] : known) if (tag == name) return station;
    return Station::Other;
}

// The item of a station's block, to name it as the game does; empty for a station without one.
inline std::string_view stationItem(Station station) {
    switch (station) {
    case Station::Crafting: return "minecraft:crafting_table";
    case Station::Stonecutter: return "minecraft:stonecutter_block";
    case Station::Furnace: return "minecraft:furnace";
    case Station::BlastFurnace: return "minecraft:blast_furnace";
    case Station::Smoker: return "minecraft:smoker";
    case Station::Campfire: return "minecraft:campfire";
    case Station::SoulCampfire: return "minecraft:soul_campfire";
    case Station::Smithing: return "minecraft:smithing_table";
    case Station::BrewingStand: return "minecraft:brewing_stand";
    case Station::Other: break;
    }
    return {};
}
// The table as the game holds it, names with their aux as ItemKey.h writes
// them ("name@*" for an ingredient of any aux). `uses` has one entry per grid
// cell or ingredient; an empty name is an empty cell. The first of `makes` is the
// product, the rest come with it (a cake's buckets).
struct SourcePart { std::string name; bool tag = false; };
struct SourceRecipe {
    std::string station, id;
    int priority = 0;
    std::vector<std::pair<std::string, Count>> makes;
    std::vector<SourcePart> uses;
    int width = 0, height = 0; // a shaped recipe's grid, `uses` row by row; 0 without one
    bool shapeless = false;    // the game says the order does not matter
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
    Station station = Station::Other; // the first of `stations`, which names the recipe in the notes
    std::vector<Station> stations;    // every station that does it, in order (cooking: often several)
    int priority = 0;
    Item makes = 0;
    Count count = 1;
    std::vector<std::pair<Item, Count>> extra; // by-products
    std::vector<Part> uses;                    // each item or group once, with how many
    // As it is laid out, for showing it: with a width, one per grid cell row
    // by row (id -1: an empty cell); without, the ingredients in the table's
    // order, which then does not matter.
    std::vector<Part> cells;
    int width = 0;
    bool anyOrder = false; // the game says so; not merely a shape that was not read
    // Uses its own product (copying a smithing template, dyeing a shulker box): shown, never the
    // recipe an item is made by, so a plan cannot go round it.
    bool own = false;
    // Brewing (L-361): its first use, the ingredient, serves up to `batch` crafts (one brews three bottles); 0 for
    // every other recipe.
    Count batch = 0;
};
// Any one member will do: an item tag, or (without a name) the inputs that smelt to the same item.
struct Group { std::string name; std::vector<Item> members; };
struct Index {
    std::vector<std::string> items;
    std::map<std::string, Item, std::less<>> ids;
    std::vector<Group> groups;
    std::vector<Recipe> recipes;
    std::vector<std::vector<int>> makers; // per item: the recipes whose product it is, those that use it left out
    std::vector<std::vector<int>> copies; // per item: the recipes that make it from itself
    std::set<std::string, std::less<>> variantNames; // names whose aux tells items apart
    Item find(std::string_view key) const {
        auto found = ids.find(key);
        return found == ids.end() ? -1 : found->second;
    }
    // An item of the game's: its aux counts only where the table tells variants apart (a worn tool's does not).
    Item find(std::string_view name, int aux) const {
        return variantNames.contains(name) ? find(itemKey(name, aux)) : find(name);
    }
    // An item by a key from anywhere (the item list keys every aux, the table only where it tells variants apart).
    Item findKey(std::string_view key) const {
        auto const parts = splitItemKey(key);
        return find(parts.name, parts.aux);
    }
    // The key of a game item as this table names it, known to it or not.
    std::string keyOf(std::string_view name, int aux) const {
        return variantNames.contains(name) ? itemKey(name, aux == anyAux ? 0 : aux) : std::string(name);
    }
    // A group nobody picked a member for stands in calculations as an item past the real ones.
    Item groupItem(int group) const { return static_cast<Item>(items.size()) + group; }
};

// Which of two recipes for one item comes first: by station, then the lowest
// priority, then the recipe named after its product, then the id.
// The member a group is known by (L-370): its name ("Sand etc.") and its picture where nothing held says
// otherwise. The plainest: one whose name the others' end with ("sand" in "red_sand"), the shortest of those;
// else an oak one (the wood families); else the shortest name, then the first by name. Not the first listed,
// which put red sand and cherry before sand and oak (maintainer 2026-11-21).
inline Item groupLead(Index const& index, int group) {
    auto const& members = index.groups[static_cast<size_t>(group)].members;
    if (members.empty()) return -1;
    auto plain = [&](Item item) {
        std::string_view name = splitItemKey(index.items[static_cast<size_t>(item)]).name;
        if (auto colon = name.find(':'); colon != std::string_view::npos) name = name.substr(colon + 1);
        return name;
    };
    Item best = -1;
    auto better = [&](Item a, Item b) {
        auto const x = plain(a), y = plain(b);
        return x.size() != y.size() ? x.size() < y.size() : x < y;
    };
    for (Item a : members)
        for (Item b : members) {
            auto const x = plain(a), y = plain(b);
            if (a != b && y.size() > x.size() && y.ends_with(x) && y[y.size() - x.size() - 1] == '_') {
                if (best < 0 || better(a, best)) best = a;
                break;
            }
        }
    if (best >= 0) return best;
    for (Item a : members) if (plain(a).starts_with("oak_") && (best < 0 || better(a, best))) best = a;
    if (best >= 0) return best;
    for (Item a : members) if (best < 0 || better(a, best)) best = a;
    return best;
}
inline bool before(Index const& index, Recipe const& a, Recipe const& b) {
    if (a.station != b.station) return a.station < b.station;
    if (a.priority != b.priority) return a.priority < b.priority;
    bool const aNamed = a.id == index.items[a.makes], bNamed = b.id == index.items[b.makes];
    if (aNamed != bNamed) return aNamed;
    return a.id < b.id;
}

// The table with every name settled into its key. An aux value tells items
// apart only where the table shows several for one name, as products or as
// named ingredients (bed colors; not a chest's facing, which is its one aux).
// There an ingredient of any aux becomes a group of the variants, named
// "name@*"; everywhere else the aux is dropped.
inline Source settled(Source const& given, std::set<std::string, std::less<>>* variantNames = nullptr) {
    std::map<std::string, std::set<int>, std::less<>> seen;
    auto note = [&](std::string const& key) {
        auto const parts = splitItemKey(key);
        if (!parts.name.empty() && parts.aux != anyAux) seen[std::string(parts.name)].insert(parts.aux);
    };
    for (auto const& recipe : given.recipes) {
        if (recipe.station == "deprecated") continue;
        for (auto const& made : recipe.makes) note(made.first);
        for (auto const& part : recipe.uses) if (!part.tag) note(part.name);
    }
    auto variant = [&](std::string_view name) {
        auto found = seen.find(name);
        return found != seen.end() && found->second.size() > 1;
    };
    auto key = [&](std::string const& from) {
        auto const parts = splitItemKey(from);
        return variant(parts.name) ? itemKey(parts.name, parts.aux) : std::string(parts.name);
    };
    Source out;
    out.tags = given.tags;
    out.recipes = given.recipes;
    for (auto& recipe : out.recipes) {
        for (auto& made : recipe.makes) made.first = key(made.first);
        for (auto& part : recipe.uses) {
            if (part.tag || part.name.empty()) continue;
            auto const parts = splitItemKey(part.name);
            if (parts.aux != anyAux || !variant(parts.name)) { part.name = key(part.name); continue; }
            part.name = itemKey(parts.name, anyAux);
            part.tag = true;
            auto& members = out.tags[part.name];
            if (members.empty()) for (int aux : seen.find(parts.name)->second) members.push_back(itemKey(parts.name, aux));
        }
    }
    if (variantNames) for (auto const& [name, auxes] : seen) if (auxes.size() > 1) variantNames->insert(name);
    return out;
}

// Left out: superseded recipes (the game's "deprecated" tag) and recipes
// without a product or ingredients. Recipes that use their own product (an
// ingredient that is the product, or any variant of its name: dyeing a
// shulker box, copying a banner) are kept apart as `copies`. Smelting recipes that differ only in their
// one input become one recipe with a nameless group (raw iron or either ore). A recipe another one already
// covers is left out: same station, product and layout, each cell the same or a member of the other's group
// there (a chest of crimson planks beside the chest of any planks).
inline Index build(Source const& given) {
    Index index;
    Source const source = settled(given, &index.variantNames);
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
        Recipe recipe{from.id, stationOf(from.station), {stationOf(from.station)}, from.priority, item(from.makes.front().first),
                      from.makes.front().second, {}, {}};
        if (recipe.station == Station::BrewingStand) recipe.batch = 3; // one ingredient brews three bottles (L-361)
        for (size_t i = 1; i < from.makes.size(); ++i)
            if (!from.makes[i].first.empty() && from.makes[i].second) recipe.extra.emplace_back(item(from.makes[i].first), from.makes[i].second);
        bool own = false;
        // A shape that does not fit its cells is not trusted: the recipe is laid out as one without.
        bool const shaped = from.width >= 1 && from.width <= 3 && from.height >= 1 && from.height <= 3
            && static_cast<size_t>(from.width * from.height) == from.uses.size();
        recipe.width = shaped ? from.width : 0;
        recipe.anyOrder = from.shapeless;
        for (auto const& part : from.uses) {
            if (part.name.empty()) {
                if (shaped) recipe.cells.push_back(Part{false, -1, 0});
                continue;
            }
            Part const next{part.tag, part.tag ? group(part.name) : item(part.name), 1};
            recipe.cells.push_back(next);
            own = own || (!next.group && next.id == recipe.makes)
                || (part.tag && splitItemKey(part.name).aux == anyAux
                    && splitItemKey(part.name).name == splitItemKey(index.items[static_cast<size_t>(recipe.makes)]).name);
            auto same = std::find_if(recipe.uses.begin(), recipe.uses.end(),
                                     [&](Part const& other) { return other.group == next.group && other.id == next.id; });
            if (same != recipe.uses.end()) ++same->count; else recipe.uses.push_back(next);
        }
        recipe.own = own;
        if (!recipe.uses.empty()) all.push_back(std::move(recipe));
    }
    // One cooking recipe filed under several stations (L-376): the same product, amount and cells become one
    // recipe that names every station, the first of them standing for it.
    std::vector<bool> dropped(all.size(), false);
    for (size_t i = 0; i < all.size(); ++i) {
        if (dropped[i] || !smelts(all[i].station)) continue;
        for (size_t j = i + 1; j < all.size(); ++j) {
            auto const& other = all[j];
            if (dropped[j] || !smelts(other.station) || other.makes != all[i].makes || other.count != all[i].count || other.cells != all[i].cells
                || other.extra != all[i].extra || other.own != all[i].own)
                continue;
            for (Station s : other.stations)
                if (std::find(all[i].stations.begin(), all[i].stations.end(), s) == all[i].stations.end()) all[i].stations.push_back(s);
            dropped[j] = true;
        }
        std::sort(all[i].stations.begin(), all[i].stations.end());
        all[i].station = all[i].stations.front();
    }
    // Smelting alternatives: the same stations, product and amount, one input each.
    std::map<std::tuple<std::vector<Station>, Item, Count>, std::vector<size_t>> alike;
    for (size_t i = 0; i < all.size(); ++i)
        if (!dropped[i] && !all[i].own && smelts(all[i].station) && all[i].uses.size() == 1 && all[i].uses[0].count == 1)
            alike[{all[i].stations, all[i].makes, all[i].count}].push_back(i);
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
        // What the player carries first: raw iron before the ores.
        std::stable_partition(inputs.members.begin(), inputs.members.end(), [&](Item id) { return !index.items[id].ends_with("_ore"); });
        index.groups.push_back(std::move(inputs));
        all[best].uses = {Part{true, static_cast<int>(index.groups.size()) - 1, 1}};
        all[best].cells = all[best].uses;
        all[best].width = 0;
        for (size_t i : list) dropped[i] = i != best;
    }
    auto covers = [&](Recipe const& wide, Recipe const& narrow) {
        if (wide.station != narrow.station || wide.makes != narrow.makes || wide.count != narrow.count || wide.width != narrow.width
            || wide.cells.size() != narrow.cells.size() || wide.extra != narrow.extra)
            return false;
        bool wider = false;
        for (size_t c = 0; c < wide.cells.size(); ++c) {
            auto const& w = wide.cells[c];
            auto const& n = narrow.cells[c];
            if (w == n) continue;
            if (!w.group || n.group || n.id < 0) return false;
            auto const& members = index.groups[static_cast<size_t>(w.id)].members;
            if (std::find(members.begin(), members.end(), n.id) == members.end()) return false;
            wider = true;
        }
        return wider;
    };
    std::map<Item, std::vector<size_t>> byProduct;
    for (size_t i = 0; i < all.size(); ++i) if (!dropped[i] && !all[i].own) byProduct[all[i].makes].push_back(i);
    for (auto const& [product, list] : byProduct)
        for (size_t narrow : list)
            for (size_t wide : list)
                if (wide != narrow && !dropped[wide] && covers(all[wide], all[narrow])) { dropped[narrow] = true; break; }
    for (size_t i = 0; i < all.size(); ++i) if (!dropped[i]) index.recipes.push_back(std::move(all[i]));
    index.makers.resize(index.items.size());
    index.copies.resize(index.items.size());
    for (size_t i = 0; i < index.recipes.size(); ++i)
        (index.recipes[i].own ? index.copies : index.makers)[index.recipes[i].makes].push_back(static_cast<int>(i));
    return index;
}

// Smelting that only undoes mining: every input is an ore block, and mining
// the ore gives the item itself (diamond, redstone, coal, quartz). The table
// does not know drops, so the name has to tell. Not chosen unless the player
// asks; iron and gold have a raw form among their inputs and are not meant.
inline bool smeltsOreOnly(Index const& index, Recipe const& recipe) {
    if (!smelts(recipe.station) || recipe.uses.empty()) return false;
    auto ore = [&](Item item) { return index.items[item].ends_with("_ore"); };
    for (auto const& part : recipe.uses) {
        if (!part.group) { if (!ore(part.id)) return false; continue; }
        auto const& members = index.groups[part.id].members;
        if (members.empty() || !std::all_of(members.begin(), members.end(), ore)) return false;
    }
    return true;
}

// The recipe each item is made by, unless the player says otherwise.
struct Choice {
    std::vector<int> recipe; // per item: position in Index::recipes, -1 for none (gathered)
    std::vector<Item> base;  // per item: the base of its loop of items, -1 outside one
};
// Items whose recipes lead back to each other form a loop: nugget, ingot and
// block; stone and cobblestone; the wool colors. The choice never follows a
// loop round. Inside one, items are settled one at a time, an item only by a
// recipe whose parts from the loop are settled already. The first, the base,
// therefore has a recipe from outside the loop (smelting ore) or is gathered.
// Next comes the item most recipes outside the loop use, then one a recipe in
// the loop uses several of, then by name. Among an item's recipes the station
// and priority decide, then the one closest to the base (white wool and dye,
// not another color and dye), then `before`. Outside a loop, among recipes of one station and priority, the one
// with fewer steps to make from what is gathered comes before `before` (maintainer 2026-11-21: the station's order
// stays, so crafting keeps coming first). Smelting an ore that drops the
// item is never the default (`smeltsOreOnly`).
inline Choice choose(Index const& index) {
    Item const count = static_cast<Item>(index.items.size());
    Choice out{std::vector<int>(count, -1), std::vector<Item>(count, -1)};
    std::vector<std::vector<Item>> uses(count);
    for (auto const& recipe : index.recipes)
        if (!recipe.own) for (auto const& part : recipe.uses) if (!part.group) uses[recipe.makes].push_back(part.id);
    // Strongly connected components (Tarjan, without recursion).
    std::vector<int> order(count, -1), low(count, 0), loop(count, -1);
    std::vector<bool> stacked(count, false);
    std::vector<Item> stack;
    std::vector<std::pair<Item, size_t>> frames;
    int next = 0, loops = 0;
    for (Item start = 0; start < count; ++start) {
        if (order[start] >= 0) continue;
        auto enter = [&](Item item) {
            order[item] = low[item] = next++;
            stack.push_back(item);
            stacked[item] = true;
            frames.emplace_back(item, 0);
        };
        enter(start);
        while (!frames.empty()) {
            Item const at = frames.back().first;
            if (frames.back().second < uses[at].size()) {
                Item const to = uses[at][frames.back().second++];
                if (order[to] < 0) enter(to);
                else if (stacked[to]) low[at] = std::min(low[at], order[to]);
                continue;
            }
            frames.pop_back();
            if (!frames.empty()) low[frames.back().first] = std::min(low[frames.back().first], low[at]);
            if (low[at] != order[at]) continue;
            for (;;) {
                Item const member = stack.back();
                stack.pop_back();
                stacked[member] = false;
                loop[member] = loops;
                if (member == at) break;
            }
            ++loops;
        }
    }
    std::vector<std::vector<Item>> members(loops);
    for (Item item = 0; item < count; ++item) members[loop[item]].push_back(item);
    std::vector<int> used(count, 0);
    std::vector<bool> several(count, false);
    for (auto const& recipe : index.recipes)
        for (auto const& part : recipe.uses) {
            if (part.group || recipe.own) continue;
            if (loop[part.id] != loop[recipe.makes]) ++used[part.id];
            else if (part.count > 1) several[part.id] = true;
        }
    std::vector<int> depth(count, -1); // settled when >= 0
    // The steps an item takes to make with the recipes chosen so far: none when it is gathered, else one and
    // those of each item its recipe uses (a group as its member with fewest). Ingredients come before the items
    // made from them here, so theirs are settled; a member not yet settled, or a loop gone round, counts none.
    std::vector<int> steps(count, -1);
    std::vector<bool> counting(count, false);
    auto recipeSteps = [&](auto const& self, Recipe const& recipe) -> int {
        auto stepsOf = [&](Item item) -> int {
            if (steps[item] >= 0) return steps[item];
            if (counting[item] || out.recipe[item] < 0) return 0;
            counting[item] = true;
            int const value = self(self, index.recipes[out.recipe[item]]);
            counting[item] = false;
            return steps[item] = value;
        };
        int total = 1;
        for (auto const& part : recipe.uses) {
            if (!part.group) { total += stepsOf(part.id); continue; }
            int fewest = -1;
            for (Item member : index.groups[part.id].members) {
                int const n = stepsOf(member);
                if (fewest < 0 || n < fewest) fewest = n;
            }
            total += std::max(0, fewest);
        }
        return total;
    };
    for (auto const& group : members) {
        if (group.size() == 1) {
            Item const item = group.front();
            for (int candidate : index.makers[item]) {
                if (smeltsOreOnly(index, index.recipes[candidate])) continue;
                if (out.recipe[item] < 0) { out.recipe[item] = candidate; continue; }
                // Station and priority first; then fewer steps; then `before`.
                auto const& recipe = index.recipes[candidate];
                auto const& held = index.recipes[out.recipe[item]];
                bool better;
                if (recipe.station != held.station || recipe.priority != held.priority) better = before(index, recipe, held);
                else if (int const mine = recipeSteps(recipeSteps, recipe), theirs = recipeSteps(recipeSteps, held); mine != theirs) better = mine < theirs;
                else better = before(index, recipe, held);
                if (better) out.recipe[item] = candidate;
            }
            continue;
        }
        auto earlier = [&](Item a, Item b) {
            if (used[a] != used[b]) return used[a] > used[b];
            if (several[a] != several[b]) return several[a] > several[b];
            return index.items[a] < index.items[b];
        };
        Item base = -1;
        for (size_t settled = 0; settled < group.size(); ++settled) {
            Item pick = -1;
            int pickRecipe = -1, pickDepth = 0;
            bool pickReady = false;
            for (Item item : group) {
                if (depth[item] >= 0) continue;
                int best = -1, bestDepth = 0;
                for (int candidate : index.makers[item]) {
                    auto const& recipe = index.recipes[candidate];
                    if (smeltsOreOnly(index, recipe)) continue;
                    int far = 0;
                    bool ready = true;
                    for (auto const& part : recipe.uses) {
                        if (part.group || loop[part.id] != loop[item]) continue;
                        ready = ready && depth[part.id] >= 0;
                        if (ready) far = std::max(far, depth[part.id] + 1);
                    }
                    if (!ready) continue;
                    bool better = best < 0;
                    if (!better) {
                        auto const& held = index.recipes[best];
                        if (recipe.station != held.station) better = recipe.station < held.station;
                        else if (recipe.priority != held.priority) better = recipe.priority < held.priority;
                        else if (far != bestDepth) better = far < bestDepth;
                        else better = before(index, recipe, held);
                    }
                    if (better) { best = candidate; bestDepth = far; }
                }
                bool const ready = best >= 0;
                if (pick < 0 || (ready != pickReady ? ready : earlier(item, pick))) {
                    pick = item;
                    pickRecipe = best;
                    pickDepth = bestDepth;
                    pickReady = ready;
                }
            }
            out.recipe[pick] = pickRecipe;
            depth[pick] = pickRecipe < 0 ? 0 : pickDepth;
            if (base < 0) base = pick;
        }
        for (Item item : group) out.base[item] = base;
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
using Result = rawMaterials::Result;
// A held member that covers part of a group's need.
struct Cover {
    int group = 0;
    Item member = 0;
    Count count = 0;
    bool operator==(Cover const&) const = default;
};
struct Planned {
    Result result;
    std::vector<Cover> covered;
};
// Item ids in the result past Index::items are groups (Index::groupItem). A
// group is covered by what is left of its members after their own needs, so
// held planks are not counted for planks and for "any planks" both.
inline Planned planned(Index const& index, Choice const& choice, Request const& request) {
    namespace raw = rawMaterials;
    Item const count = static_cast<Item>(index.items.size());
    raw::Recipes recipes;
    for (Item item = 0; item < count; ++item) {
        int const chosen = choice.recipe[item];
        if (chosen < 0 || request.stops.contains(item)) continue;
        auto const& recipe = index.recipes[chosen];
        raw::Recipe to{recipe.count, {}, recipe.batch};
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
    Planned out;
    for (auto [item, need] : first.raw) {
        if (item < count || item - count >= static_cast<Item>(index.groups.size())) continue;
        Count take = 0;
        for (Item member : index.groups[item - count].members) {
            Count const use = std::min(left[member], need - take);
            if (!use) continue;
            left[member] -= use;
            take += use;
            out.covered.push_back({item - count, member, use});
        }
        if (take) held.emplace_back(item, take);
    }
    out.result = held.size() == request.held.size() ? std::move(first) : raw::reduce(request.wanted, recipes, held);
    return out;
}
inline Result plan(Index const& index, Choice const& choice, Request const& request) { return planned(index, choice, request).result; }
}
