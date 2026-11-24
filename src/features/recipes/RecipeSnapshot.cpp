#include "features/recipes/RecipeSnapshot.h"
#include "app/Capabilities.h"
#include "app/Versions.h"
#include "features/recipes/StackKey.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/alchemy/PotionBrewing.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/level/Level.h"
#include <algorithm>
#include <iterator>

namespace lamium::recipes::game {
namespace {
// Sizes too large to be real mean a member is not where this build expects it.
struct Unreadable {};
void guard(size_t size, size_t limit) { if (size > limit) throw Unreadable{}; }
// False when the recipe has a part the index cannot name (no such kind was in the 1.26.60 table).
bool partOf(ItemDescriptor const& descriptor, Source& source, SourcePart& out) {
    auto const* impl = descriptor.mImpl.get();
    if (!impl) { out = {}; return true; }
    auto const type = impl->getType();
    if (type == ItemDescriptor::InternalType::Default) {
        auto const entry = impl->getItem();
        if (!entry.mItem) return false;
        auto const& name = entry.mItem->mFullName->getString();
        guard(name.size(), 200);
        out = {itemKey(name, entry.mAuxValue), false};
        return true;
    }
    if (type != ItemDescriptor::InternalType::ItemTag) return false;
    std::string name = impl->getFullName();
    guard(name.size(), 200);
    if (!source.tags.contains(name)) {
        auto& members = source.tags[name];
        // The callback's true ends the walk; it must not throw through the game's loop.
        auto each = [&](::Item const& item, short) noexcept {
            try {
                auto const& member = item.mFullName->getString();
                if (member.size() <= 200 && members.size() < 4096) members.push_back(member);
            } catch (...) { return true; }
            return false;
        };
        impl->forEachItemUntil(each);
    }
    out = {std::move(name), true};
    return true;
}
void add(HashedString const& tag, std::string const& id, ::Recipe const& recipe, Source& source) {
    guard(id.size(), 400);
    if (recipe.isMultiRecipe()) return;
    auto const& results = *(*recipe.mResults).mResults;
    auto const& parts = *recipe.mMyIngredients;
    guard(results.size(), 64);
    guard(parts.size(), 200);
    SourceRecipe to{tag.getString(), id, recipe.mPriority, {}, {}};
    to.shapeless = recipe.isShapeless();
    if (!to.shapeless) to.width = recipe.mWidth, to.height = recipe.mHeight;
    for (auto const& item : results) {
        if (item.isNull()) continue;
        auto name = item.getTypeName();
        guard(name.size(), 200);
        int const aux = item.getAuxValue();
        to.makes.emplace_back(itemKey(name, aux == anyAux ? 0 : aux), static_cast<Count>(item.mCount));
    }
    if (to.makes.empty()) return;
    to.uses.reserve(parts.size());
    for (auto const& part : parts) {
        SourcePart next;
        if (!partOf(part, source, next)) return;
        to.uses.push_back(std::move(next));
    }
    source.recipes.push_back(std::move(to));
}
}
void Snapshot::reset() {
    read = {};
    starsRead = false;
    size = -1;
    done = 0;
    broken = false;
    ingredients.clear();
    listed = false;
    kind = aux = 0;
    ingredient = 0;
}
// The three potion kinds a mix is put on, and the auxes asked of each: the game's potions end below 64.
constexpr std::string_view potionKinds[]{"minecraft:potion", "minecraft:splash_potion", "minecraft:lingering_potion"};
constexpr int mostAux = 64;
bool Snapshot::brew(Level& level, int budget) {
    auto registry = level.getItemRegistry();
    if (registry.mWeakRegistry.expired()) return true;
    if (!listed) {
        // The ingredients by name, never as pointers kept for later: every item the game calls a potion
        // ingredient, and gunpowder and dragon's breath, which turn a potion into a splash and a lingering one.
        auto const& all = registry.getNameToItemMap();
        guard(all.size(), 100000);
        for (auto const& [name, weak] : all) {
            auto const* item = weak.get();
            if (!item) continue;
            std::string const full = item->mFullName->getString();
            guard(full.size(), 200);
            if (full == "minecraft:gunpowder" || full == "minecraft:dragon_breath" || PotionBrewing::isPotionIngredient(ItemDescriptor(*item, 0)))
                if (std::find(ingredients.begin(), ingredients.end(), full) == ingredients.end()) ingredients.push_back(full);
        }
        std::sort(ingredients.begin(), ingredients.end());
        guard(ingredients.size(), 200);
        listed = true;
    }
    while (kind < static_cast<int>(std::size(potionKinds))) {
        auto const base = registry.lookupByName(HashedString(potionKinds[static_cast<size_t>(kind)]));
        auto const* potion = base.get();
        if (!potion) { ++kind; continue; }
        ItemStack from;
        from.reinit(*potion, 1, aux);
        ItemInstance const source(from);
        std::string const fromKey = itemKey(from.getTypeName(), aux);
        for (; ingredient < ingredients.size(); ++ingredient) {
            if (budget-- <= 0) return false;
            auto const found = registry.lookupByName(HashedString(ingredients[ingredient]));
            auto const* item = found.get();
            if (!item) continue;
            ItemInstance const to = PotionBrewing::mix(ItemDescriptor(*item, 0), source);
            if (to.isNull() || (to.getTypeName() == from.getTypeName() && to.getAuxValue() == aux)) continue;
            std::string const toKey = itemKey(to.getTypeName(), to.getAuxValue());
            // The ingredient first: a brewing recipe counts it once a batch of three (RecipeIndex.h).
            read.recipes.push_back({"brewing_stand", "brew:" + fromKey + "+" + ingredients[ingredient], 0, {{toKey, 1}},
                                    {{itemKey(ingredients[ingredient], 0), false}, {fromKey, false}}});
        }
        ingredient = 0;
        if (++aux >= mostAux) aux = 0, ++kind;
    }
    return true;
}
// Where reading item data is confirmed for this version; without it the rockets stay one item, as before.
void Snapshot::readStars(Level& level) {
    if (!versionSensitiveAllowed(capabilities::Id::ItemDetails)) return;
    auto const* registry = level.getItemRegistry().getCreativeItemRegistry().get();
    if (!registry) return;
    auto const& entries = registry->mCreativeItems.get();
    guard(entries.size(), 20000);
    for (auto const& entry : entries) {
        auto const& item = entry.mItemInstance.get();
        if (item.isNull() || item.getTypeName() != "minecraft:firework_star") continue;
        auto const* user = item.mUserData.get();
        if (!user) continue;
        auto found = user->mTags.find("FireworksItem");
        if (found == user->mTags.end()) continue;
        if (std::string key = stackKey(item); !key.empty()) read.fireworkStars.emplace(std::move(key), found->second.toSnbt(SnbtFormat::Minimize));
    }
}
bool Snapshot::step(Level& level, int budget) noexcept {
    if (broken) return true;
    try {
        auto const& table = *level.getRecipes().mRecipes;
        guard(table.size(), 200);
        long long total = 0;
        for (auto const& [tag, byId] : table) {
            guard(byId.size(), 50000);
            total += static_cast<long long>(byId.size());
        }
        if (total != size) {
            reset();
            size = total;
        }
        size_t skip = done;
        for (auto const& [tag, byId] : table) {
            if (skip >= byId.size()) { skip -= byId.size(); continue; }
            for (auto at = std::next(byId.begin(), static_cast<std::ptrdiff_t>(skip)); at != byId.end(); ++at) {
                if (budget-- <= 0) return false;
                if (auto const* recipe = at->second.get()) add(tag, at->first, *recipe, read);
                ++done;
            }
            skip = 0;
        }
        // The firework stars' explosions, which the rockets made of them carry (L-363), once the recipes are read.
        if (!starsRead) {
            try { readStars(level); } catch (...) { read.fireworkStars.clear(); } // the recipes stand without them
            starsRead = true;
        }
        // The recipes are read; then the potion mixes, a twelfth of the budget a call (each asks the game to brew:
        // a few thousand in all, so a few seconds of ticks).
        return brew(level, std::max(1, budget / 12));
    } catch (...) {
        read = {};
        broken = true;
        return true;
    }
}
}
