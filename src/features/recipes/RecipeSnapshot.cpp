#include "features/recipes/RecipeSnapshot.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/level/Level.h"
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
        out = {name, false};
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
    if (!recipe.isShapeless()) to.width = recipe.mWidth, to.height = recipe.mHeight;
    for (auto const& item : results) {
        if (item.isNull()) continue;
        auto name = item.getTypeName();
        guard(name.size(), 200);
        to.makes.emplace_back(std::move(name), static_cast<Count>(item.mCount));
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
    size = -1;
    done = 0;
    broken = false;
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
        return true;
    } catch (...) {
        read = {};
        broken = true;
        return true;
    }
}
}
