#include "features/research/RecipeTrace.h"
#ifdef LAMIUM_RECIPE_TRACE
#include "app/TraceLog.h"
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
#include "mc/world/item/NetworkItemInstanceDescriptor.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"
#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <map>
#include <string>

namespace lamium::researchTrace::recipeTable {
namespace {
// Reads only. Once the table has kept its size for five seconds it is written
// to `logs/recipe-table-<time>-<local|remote>.txt`, one line per recipe, and
// again when its size changes. Nothing is loaded or resolved: a result the
// client holds unloaded is written as the descriptor it came as. Sizes too
// large to be real mean a member is not where this build expects it; the
// walk stops there. Potion mixes are left out: the SDK gives `Mix<T>` no
// layout.
ll::event::ListenerPtr tickListener, joinListener, exitListener;
TraceBudget summaryBudget, failBudget;
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
void reset() { ticks = steady = 0; seen = dumped = -1; }
void tick() noexcept {
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) { reset(); return; }
        if (++ticks % 20 || written >= 6) return;
        auto& level = player->getLevel();
        auto const& recipes = level.getRecipes();
        long long const size = sizeOf(recipes);
        if (size != seen) { seen = size; steady = 0; return; }
        if (++steady < 5 || size == dumped) return;
        dumped = size;
        ++written;
        dump(level, recipes);
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
