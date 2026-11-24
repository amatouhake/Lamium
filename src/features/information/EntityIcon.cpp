#include "features/information/EntityIcon.h"
#include "features/information/TargetCard.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/ActorPlacerItem.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include <mutex>
#include <optional>
#include <unordered_map>

namespace lamium::information {
namespace {
// Some eggs' actor id differs from the entity's own identifier, so the name
// convention cannot find them. Index every spawn-egg item by its actor id once
// per world (add-ons bring their own eggs); only strings are kept.
std::mutex eggMutex;
std::unordered_map<std::string, std::string> eggIndex;
bool eggIndexBuilt = false;
std::optional<std::string> spawnEggFor(ItemRegistryRef const& registry, std::string const& actorId) {
    std::scoped_lock lock(eggMutex);
    if (!eggIndexBuilt && !registry.mWeakRegistry.expired()) {
        auto const& items = registry.getNameToItemMap();
        for (auto const& [name, weak] : items) {
            auto* item = weak.get();
            if (!item || !item->isActorPlacerItem()) continue;
            auto const* placer = static_cast<ActorPlacerItem const*>(item);
            std::string const& actor = placer->mActorID->mFullName.get();
            if (!actor.empty()) eggIndex.try_emplace(actor, name.getString());
        }
        eggIndexBuilt = !items.empty();
    }
    if (auto found = eggIndex.find(actorId); found != eggIndex.end()) return found->second;
    return std::nullopt;
}
}
void clearSpawnEggIndex() {
    std::scoped_lock lock(eggMutex);
    eggIndex.clear();
    eggIndexBuilt = false;
}
// Spawn-egg icons come from the item registry: the exact name and the few
// renamed entities first, then the registry audit for anything else.
std::string entityIconItem(IClientInstance& client, std::string const& identifier) {
    if (identifier.empty()) return {};
    try {
        auto registry = client.getItemRegistry();
        for (auto const& candidate : spawnEggCandidates(identifier))
            if (registry.getItem(HashedString(candidate))) return candidate;
        if (auto egg = spawnEggFor(registry, identifier)) return std::move(*egg);
        // Projectiles and carried blocks have no egg, but an item of the same
        // subject often shares a usable id (snowball, arrow, painting).
        for (auto const& candidate : entityItemCandidates(identifier))
            if (registry.getItem(HashedString(candidate))) return candidate;
    } catch (...) {} // An icon must never fail what asked for it.
    return {};
}
}
