#include "features/research/RecipeTrace.h"
#ifdef LAMIUM_RECIPE_TRACE
#include "app/TraceLog.h"
#include "features/recipes/ItemKey.h"
#include "features/recipes/RecipeSnapshot.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/NetworkItemInstanceDescriptor.h"
#include "mc/world/item/alchemy/PotionBrewing.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <map>
#include <set>
#include <string>

namespace lamium::researchTrace::recipeTable {
namespace {
// Reads only. Once the table has kept its size for five seconds it is written
// to `logs/recipe-table-<time>-<local|remote>.txt`, one line per recipe, and
// again when its size changes. Nothing is loaded or resolved: a result the
// client holds unloaded is written as the descriptor it came as. Sizes too
// large to be real mean a member is not where this build expects it; the
// walk stops there. Potion mixes (L-361): the SDK gives `Mix<T>` no layout,
// so they are found by asking `PotionBrewing::mix` for every ingredient on
// every potion, splash and lingering potion by aux, written to
// `logs/brewing-<time>-<local|remote>.txt`.
ll::event::ListenerPtr tickListener, joinListener, exitListener;
TraceBudget summaryBudget, failBudget, indexBudget;
bool local = false;
unsigned ticks = 0, steady = 0, written = 0;
long long seen = -1, dumped = -1;
struct Unreadable { std::string what; };
void guard(size_t size, size_t limit, char const* what) {
    if (size > limit) throw Unreadable{std::format("{} has {} entries", what, size)};
}
char const* kindName(ItemDescriptor::InternalType type) {
    switch (type) {
    case ItemDescriptor::InternalType::Invalid: return "invalid";
    case ItemDescriptor::InternalType::Default: return "item";
    case ItemDescriptor::InternalType::Molang: return "molang";
    case ItemDescriptor::InternalType::ItemTag: return "tag";
    case ItemDescriptor::InternalType::Deferred: return "deferred";
    case ItemDescriptor::InternalType::ComplexAlias: return "alias";
    }
    return "unknown";
}
struct Walk {
    std::map<int, std::string> names;     // item id -> name, for the furnace keys
    std::map<std::string, int> kinds;     // descriptor kind -> count
    std::string itemOf(Item const* item, int aux) {
        if (!item) return "null";
        int const id = item->mId;
        auto const& name = item->mFullName->getString();
        // Never throws: the tag walk below calls this from inside the game's own loop.
        if (name.size() > 200) return std::format("unreadable#{}:{}", id, aux);
        names.emplace(id, name);
        return std::format("{}#{}:{}", name, id, aux);
    }
    // An item's members under a tag: counted twice, because whether the
    // callback's true or false ends the walk is not known.
    std::string members(ItemDescriptor::BaseDescriptor const& impl) {
        std::string out;
        int counts[2]{};
        for (int answer = 0; answer < 2; ++answer) {
            std::string list;
            int count = 0;
            auto each = [&](Item const& item, short aux) noexcept {
                try {
                    if (++count <= 80) list += (count > 1 ? "," : "") + itemOf(&item, aux);
                } catch (...) { return true; }
                return answer == 1 || count >= 2000;
            };
            impl.forEachItemUntil(each);
            counts[answer] = count;
            if (list.size() > out.size()) out = std::move(list);
        }
        return std::format("{{n={}/{} {}}}", counts[0], counts[1], out);
    }
    std::string describe(ItemDescriptor const& descriptor) {
        auto const* impl = descriptor.mImpl.get();
        if (!impl) { ++kinds["empty"]; return "-"; }
        auto const type = impl->getType();
        char const* kind = kindName(type);
        ++kinds[kind];
        std::string name = impl->getFullName();
        guard(name.size(), 400, "a descriptor name");
        if (type == ItemDescriptor::InternalType::Default) {
            auto const entry = impl->getItem();
            return std::format("item:{}", itemOf(entry.mItem, entry.mAuxValue));
        }
        if (type == ItemDescriptor::InternalType::ItemTag) return std::format("tag:{}{}", name, members(*impl));
        return std::format("{}:{}", kind, name);
    }
    std::string describe(ItemDescriptorCount const& descriptor) {
        auto out = describe(static_cast<ItemDescriptor const&>(descriptor));
        if (descriptor.mStackSize != 1 && out != "-") out += std::format("*{}", static_cast<int>(descriptor.mStackSize));
        return out;
    }
    std::string describe(ItemInstance const& item) {
        if (item.isNull()) return "-";
        auto name = item.getTypeName();
        guard(name.size(), 200, "a result name");
        names.emplace(item.getId(), name);
        return std::format("{}#{}:{}*{}", name, item.getId(), item.getAuxValue(), static_cast<int>(item.mCount));
    }
};
std::string timeStamp() {
    auto const now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm parts{};
    localtime_s(&parts, &now);
    return std::format("{:04}{:02}{:02}-{:02}{:02}{:02}", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour,
        parts.tm_min, parts.tm_sec);
}
long long sizeOf(Recipes const& recipes) {
    auto const& table = *recipes.mRecipes;
    guard(table.size(), 200, "the recipe table");
    long long total = static_cast<long long>(recipes.mFurnaceResults->size());
    for (auto const& [tag, byId] : table) total += static_cast<long long>(byId.size());
    return total;
}
void dump(Level& level, Recipes const& recipes) {
    Walk walk;
    std::string body, counts;
    int shaped = 0, shapeless = 0, multi = 0, loaded = 0, unloaded = 0, noResult = 0;
    for (auto const& [tag, byId] : *recipes.mRecipes) {
        guard(byId.size(), 50000, "a recipe tag");
        counts += std::format(" {}={}", tag.getString(), byId.size());
        for (auto const& [id, owned] : byId) {
            guard(id.size(), 400, "a recipe id");
            auto const* recipe = owned.get();
            if (!recipe) { body += std::format("R\t{}\t{}\tnull\n", tag.getString(), id); continue; }
            bool const isMulti = recipe->isMultiRecipe(), isShapeless = recipe->isShapeless();
            ++(isMulti ? multi : isShapeless ? shapeless : shaped);
            auto const& results = *recipe->mResults;
            auto const& ready = *results.mResults;
            auto const& waiting = *results.mUnloadedResults;
            auto const& parts = *recipe->mMyIngredients;
            guard(ready.size(), 64, "a result list");
            guard(waiting.size(), 64, "an unloaded result list");
            guard(parts.size(), 200, "an ingredient list");
            (results.mResultsAreLoaded ? loaded : unloaded) += 1;
            if (ready.empty() && waiting.empty()) ++noResult;
            std::string made, pending, uses;
            for (auto const& item : ready) made += (made.empty() ? "" : " | ") + walk.describe(item);
            for (auto const& item : waiting)
                pending += (pending.empty() ? "" : " | ") + walk.describe(static_cast<ItemDescriptorCount const&>(item));
            for (auto const& part : parts) uses += (uses.empty() ? "" : " | ") + walk.describe(part);
            body += std::format("R\t{}\t{}\t{}\tw={} h={} size={} grid={}\tprio={}\tloaded={}\tresults=[{}]\tunloaded=[{}]\tuses=[{}]\n",
                tag.getString(), id, isMulti ? "multi" : isShapeless ? "shapeless" : "shaped", static_cast<int>(recipe->mWidth), static_cast<int>(recipe->mHeight),
                isMulti ? -1 : recipe->size(), isMulti ? -1 : recipe->getCraftingSize(), static_cast<int>(recipe->mPriority),
                results.mResultsAreLoaded ? 1 : 0, made, pending, uses);
        }
    }
    int furnace = 0;
    std::string smelts;
    guard(recipes.mFurnaceResults->size(), 20000, "the furnace table");
    for (auto const& [outer, inner] : *recipes.mFurnaceResults) {
        guard(inner.size(), 20000, "a furnace entry");
        for (auto const& [key, result] : inner) {
            ++furnace;
            smelts += std::format("F\t{}\t{}\t{}\n", outer, key, walk.describe(result));
        }
    }
    std::string head = std::format("# Lamium recipe table, {} world, written {}\n# tags:{}\n# {} shaped, {} shapeless, {} multi; results "
                                   "loaded on {} recipes, not loaded on {}, none on {}; {} furnace results; {} unlockable\n# descriptors:",
        local ? "local" : "remote", timeStamp(), counts, shaped, shapeless, multi, loaded, unloaded, noResult, furnace,
        recipes.mUnlockableRecipes->size());
    for (auto const& [kind, count] : walk.kinds) head += std::format(" {}={}", kind, count);
    head += "\n# tag hashes (64 bit, then as int):";
    for (char const* tag : {"crafting_table", "stonecutter", "smithing_table", "cartography_table", "furnace", "blast_furnace", "smoker",
             "campfire", "soul_campfire"}) {
        auto const hash = HashedString{tag}.getHash();
        head += std::format(" {}={}/{}", tag, hash, static_cast<int>(hash));
    }
    head += "\n# R: tag, id, shape, sizes, priority, results loaded, results, results not loaded, ingredients (item:name#id:aux*count, "
            "tag:name{n=members counted two ways, members})\n# F: outer key, inner key, result\n# I: item id, name\n";
    // Every registered item, so the furnace keys can be named: smelted inputs are often in no recipe.
    size_t registered = 0;
    if (auto registry = level.getItemRegistry(); !registry.mWeakRegistry.expired()) {
        auto const& all = registry.getNameToItemMap();
        guard(all.size(), 100000, "the item registry");
        registered = all.size();
        for (auto const& [name, weak] : all)
            if (auto const* item = weak.get(); item && item->mFullName->getString().size() <= 200)
                walk.names.emplace(static_cast<int>(item->mId), item->mFullName->getString());
    }
    head += std::format("# {} names in the item registry\n", registered);
    std::string items;
    for (auto const& [id, name] : walk.names) items += std::format("I\t{}\t{}\n", id, name);
    auto const path = Runtime::instance().self().getModDir() / "logs"
                    / std::format("recipe-table-{}-{}.txt", timeStamp(), local ? "local" : "remote");
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << head << body << smelts << items;
    file.close();
    traceLog(summaryBudget, 12, "L-116 recipe table ({}): tags{}; {} shaped, {} shapeless, {} multi; results loaded {}, not loaded {}, "
                                "none {}; {} furnace results; written to {}{}",
        local ? "local" : "remote", counts, shaped, shapeless, multi, loaded, unloaded, noResult, furnace, path.filename().string(),
        file.fail() ? " (write failed)" : "");
}
// L-359: after the dump the same table is read into the recipe index, a few
// hundred recipes a tick, and a handful of plans are logged to compare with
// what the game's own screens ask for.
recipes::game::Snapshot snapshot;
bool indexing = false;
int indexSteps = 0;
double longestStep = 0;
double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
std::string shortName(std::string const& name) { return name.starts_with("minecraft:") ? name.substr(10) : name; }
std::string nameOf(recipes::Index const& index, recipes::Item item) {
    auto const count = static_cast<recipes::Item>(index.items.size());
    if (item < 0) return "?";
    if (item < count) return shortName(index.items[item]);
    if (item - count >= static_cast<recipes::Item>(index.groups.size())) return "?";
    auto const& group = index.groups[item - count];
    if (!group.name.empty()) return "#" + shortName(group.name);
    std::string out = "{";
    for (size_t i = 0; i < group.members.size() && i < 4; ++i) out += (i ? "|" : "") + shortName(index.items[group.members[i]]);
    return out + (group.members.size() > 4 ? "|...}" : "}");
}
void report() {
    using namespace recipes;
    auto const started = std::chrono::steady_clock::now();
    Index const index = build(snapshot.source());
    Choice const choice = choose(index);
    traceLog(indexBudget, 60, "L-359 recipe index ({}): {} recipes and {} tags read in {} steps (longest {:.2f} ms); {} items, {} groups, {} "
                              "recipes kept; built and chosen in {:.2f} ms{}",
        local ? "local" : "remote", snapshot.source().recipes.size(), snapshot.source().tags.size(), indexSteps, longestStep,
        index.items.size(), index.groups.size(), index.recipes.size(), since(started), snapshot.failed() ? " (table unreadable)" : "");
    std::string chosen;
    for (char const* name : {"stick", "torch", "oak_planks", "chest", "iron_ingot", "gold_ingot", "copper_ingot", "iron_nugget", "bone_meal",
             "redstone", "slime_ball", "emerald", "dried_kelp", "resin_clump", "white_wool", "glass", "charcoal", "bed", "arrow", "paper",
             "stone", "smooth_stone", "stone_bricks", "netherite_ingot", "netherite_pickaxe", "honey_block"}) {
        Item const item = index.find(std::string{"minecraft:"} + name);
        chosen += std::format(" {}={}", name, item < 0 ? "absent" : choice.recipe[item] < 0 ? "none" : index.recipes[choice.recipe[item]].id);
    }
    traceLog(indexBudget, 60, "L-359 chosen recipes:{}", chosen);
    std::map<Item, std::string> bases;
    for (Item item = 0; item < static_cast<Item>(index.items.size()); ++item)
        if (choice.base[item] >= 0 && choice.base[item] != item) bases[choice.base[item]] += " " + nameOf(index, item);
    std::string families;
    for (auto const& [base, members] : bases) families += std::format(" [{}:{}]", nameOf(index, base), members);
    traceLog(indexBudget, 60, "L-359 convertible groups ({}):{}", bases.size(), families);
    struct Sample { std::vector<std::pair<char const*, Count>> wanted, held; };
    Sample const samples[]{
        {{{"stone_bricks", 64}}, {}},
        {{{"stone_bricks", 64}}, {{"stone_bricks", 20}, {"stone", 10}}},
        {{{"iron_block", 1}, {"iron_nugget", 5}}, {}},
        {{{"torch", 16}}, {}},
        {{{"chest", 1}, {"stick", 4}, {"oak_planks", 3}}, {{"oak_planks", 5}, {"birch_planks", 4}}},
        {{{"piston", 1}}, {}},
        {{{"netherite_pickaxe", 1}}, {}},
        {{{"emerald_block", 8}}, {}},
        {{{"cake", 1}}, {}},
        {{{"beacon", 1}}, {}},
        {{{"bed", 1}}, {}},
        {{{"smooth_stone", 16}, {"oak_stairs", 64}}, {}},
        {{{"observer", 1}, {"dispenser", 1}}, {}},
    };
    for (auto const& sample : samples) {
        Request request;
        std::string text = "want";
        auto named = [&](std::vector<std::pair<char const*, Count>> const& from, std::vector<std::pair<Item, Count>>& to) {
            for (auto [name, count] : from) {
                Item const item = index.find(std::string{"minecraft:"} + name);
                text += std::format(" {}x{}{}", count, name, item < 0 ? "(absent)" : "");
                if (item >= 0) to.emplace_back(item, count);
            }
        };
        named(sample.wanted, request.wanted);
        if (!sample.held.empty()) { text += "; held"; named(sample.held, request.held); }
        auto const planned = std::chrono::steady_clock::now();
        Result const result = plan(index, choice, request);
        double const took = since(planned);
        text += " -> gather";
        for (auto [item, count] : result.raw) text += std::format(" {}x{}", count, nameOf(index, item));
        text += "; make";
        for (auto const& step : result.steps)
            text += std::format(" {}:{}{}", nameOf(index, step.item), step.crafts, step.spare ? std::format("(+{})", step.spare) : "");
        if (!result.fromHeld.empty()) {
            text += "; from held";
            for (auto [item, count] : result.fromHeld) text += std::format(" {}x{}", count, nameOf(index, item));
        }
        if (!result.looped.empty()) {
            text += "; looped";
            for (Item item : result.looped) text += " " + nameOf(index, item);
        }
        traceLog(indexBudget, 60, "L-359 plan: {} ({:.2f} ms)", text.size() > 1500 ? text.substr(0, 1500) + " ..." : text, took);
    }
}
// Every mix the client knows, by asking for each: an item `isPotionIngredient` says is one, put on each potion
// kind by aux (0 to 63). A result that is empty or the same as what it was put on is no mix.
void dumpBrewing(Level& level) {
    auto registry = level.getItemRegistry();
    if (registry.mWeakRegistry.expired()) return;
    auto const& all = registry.getNameToItemMap();
    guard(all.size(), 100000, "the item registry");
    std::vector<Item const*> ingredients, bases;
    for (auto const& [name, weak] : all) {
        auto const* item = weak.get();
        if (!item) continue;
        std::string const full = item->mFullName->getString();
        if (full == "minecraft:potion" || full == "minecraft:splash_potion" || full == "minecraft:lingering_potion") bases.push_back(item);
        if (PotionBrewing::isPotionIngredient(ItemDescriptor(*item, 0))) ingredients.push_back(item);
    }
    std::string text = std::format("# L-361 brewing, {} ingredients, {} potion kinds\n# B: from:aux, ingredient, to:aux\n", ingredients.size(), bases.size());
    int mixes = 0;
    for (auto const* base : bases)
        for (int aux = 0; aux < 64; ++aux) {
            ItemStack from;
            from.reinit(*base, 1, aux);
            ItemInstance const source(from);
            for (auto const* ingredient : ingredients) {
                ItemInstance const to = PotionBrewing::mix(ItemDescriptor(*ingredient, 0), source);
                if (to.isNull()) continue;
                if (to.getTypeName() == from.getTypeName() && to.getAuxValue() == aux) continue;
                ++mixes;
                text += std::format("B\t{}:{}\t{}\t{}:{}\n", from.getTypeName(), aux, ingredient->mFullName->getString(), to.getTypeName(), to.getAuxValue());
            }
        }
    auto const path = Runtime::instance().self().getModDir() / "logs" / std::format("brewing-{}-{}.txt", timeStamp(), local ? "local" : "remote");
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    traceLog(summaryBudget, 12, "L-361 brewing: {} mixes from {} ingredients on {} potion kinds, written to {}", mixes, ingredients.size(), bases.size(),
             path.filename().string());
}
// L-363: every creative entry, to see which ones the item list folds (same name and aux, other data). Written
// to `logs/creative-<time>-<local|remote>.txt`: one E line per entry (place, group, key, user data and block
// states as SNBT, "-" for none), then one K line per key more than one entry holds (entries, distinct data,
// the user data's top-level tag names).
void dumpCreative(Level& level) {
    auto const* registry = level.getItemRegistry().getCreativeItemRegistry().get();
    if (!registry) return;
    auto const& entries = registry->mCreativeItems.get();
    guard(entries.size(), 20000, "the creative list");
    struct Key {
        int entries = 0;
        std::set<std::string> data, tags;
    };
    std::map<std::string, Key> keys;
    std::string lines;
    int nulls = 0, withData = 0;
    size_t place = 0;
    for (auto const& entry : entries) {
        auto const& item = entry.mItemInstance.get();
        size_t const at = place++;
        if (item.isNull()) { ++nulls; continue; }
        int const aux = item.getAuxValue();
        std::string const key = recipes::itemKey(item.getTypeName(), aux == recipes::anyAux ? 0 : aux);
        auto& known = keys[key];
        std::string data;
        if (auto const* user = item.mUserData.get()) {
            data = user->toSnbt(SnbtFormat::Minimize);
            for (auto const& [name, tag] : user->mTags) known.tags.insert(name);
        }
        if (auto const* block = item.mBlock) {
            auto const& id = block->mSerializationId.get();
            if (auto found = id.mTags.find("states"); found != id.mTags.end()) {
                std::string states = found->second.toSnbt(SnbtFormat::Minimize);
                if (states != "{}") data += (data.empty() ? "" : " ") + std::string("states=") + states;
            }
        }
        if (!data.empty()) ++withData;
        ++known.entries;
        known.data.insert(data);
        lines += std::format("E\t{}\t{}\t{}\t{}\n", at, static_cast<unsigned>(entry.mGroupIndex), key, data.empty() ? "-" : data);
    }
    int folded = 0, sameData = 0, shared = 0;
    std::string summary;
    for (auto const& [key, known] : keys) {
        if (known.entries < 2) continue;
        ++shared;
        folded += known.entries - 1;
        sameData += known.entries - static_cast<int>(known.data.size());
        std::string tags;
        for (auto const& tag : known.tags) tags += (tags.empty() ? "" : ",") + tag;
        summary += std::format("K\t{}\t{}\t{}\t{}\n", key, known.entries, known.data.size(), tags.empty() ? "-" : tags);
    }
    std::string const head = std::format(
        "# L-363 creative list: {} entries ({} null), {} keys, {} with data; {} keys held by more than one entry, "
        "{} entries folded today, {} of them identical to another\n# E: place, group, key, data\n# K: key, entries, "
        "distinct data, user data tags\n",
        entries.size(), nulls, keys.size(), withData, shared, folded, sameData);
    auto const path = Runtime::instance().self().getModDir() / "logs" / std::format("creative-{}-{}.txt", timeStamp(), local ? "local" : "remote");
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << head << summary << lines;
    traceLog(summaryBudget, 12, "L-363 creative list: {} entries, {} keys, {} folded ({} identical), written to {}", entries.size(), keys.size(),
             folded, sameData, path.filename().string());
}
void reset() {
    ticks = steady = 0;
    seen = dumped = -1;
    indexing = false;
    snapshot.reset();
}
void tick() noexcept {
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) { reset(); return; }
        auto& level = player->getLevel();
        if (indexing) {
            auto const started = std::chrono::steady_clock::now();
            bool const finished = snapshot.step(level, 600);
            ++indexSteps;
            longestStep = std::max(longestStep, since(started));
            if (finished) { indexing = false; report(); }
            return;
        }
        if (++ticks % 20 || written >= 6) return;
        auto const& recipes = level.getRecipes();
        long long const size = sizeOf(recipes);
        if (size != seen) { seen = size; steady = 0; return; }
        if (++steady < 5 || size == dumped) return;
        dumped = size;
        ++written;
        dump(level, recipes);
        try { dumpBrewing(level); } catch (...) { traceLog(failBudget, 4, "L-361 brewing unreadable"); }
        try { dumpCreative(level); } catch (...) { traceLog(failBudget, 4, "L-363 creative list unreadable"); }
        snapshot.reset();
        indexSteps = 0;
        longestStep = 0;
        indexing = true;
    } catch (Unreadable const& error) {
        written = 6;
        traceLog(failBudget, 4, "L-116 recipe table unreadable: {}", error.what);
    } catch (std::exception const& error) {
        written = 6;
        traceLog(failBudget, 4, "L-116 recipe table failed: {}", error.what());
    } catch (...) { written = 6; }
}
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    joinListener = bus.emplaceListener<ll::event::ClientStartJoinLevelEvent>([](auto& event) {
        local = event.isJoiningLocalServer();
        reset();
    });
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { reset(); });
    tickListener = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { tick(); });
    Runtime::instance().self().getLogger().warn("L-116 recipe table diagnostics enabled");
}
void stop() {
    for (auto* listener : {&tickListener, &joinListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    reset();
}
}
#else
namespace lamium::researchTrace::recipeTable { void start() {} void stop() {} }
#endif
