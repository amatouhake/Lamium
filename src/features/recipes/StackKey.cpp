#include "features/recipes/StackKey.h"
#include "app/Capabilities.h"
#include "app/Versions.h"
#include "features/recipes/ItemKey.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include "mc/world/item/ItemStackBase.h"

namespace lamium::recipes {
std::string stackKey(ItemStackBase const& stack) {
    if (stack.isNull()) return {};
    std::string const name = stack.getTypeName();
    if (name.empty() || name.size() > 200) return {};
    int const aux = stack.getAuxValue();
    std::string data;
    // The name first: no other item's data is read.
    if (!identityTags(name).empty() && versionSensitiveAllowed(capabilities::Id::ItemDetails))
        if (auto const* user = stack.mUserData.get())
            data = identityData(name, [&](std::string_view tag) -> std::optional<std::string> {
                auto found = user->mTags.find(tag);
                if (found == user->mTags.end()) return std::nullopt;
                return found->second.toSnbt(SnbtFormat::Minimize);
            });
    if (name.size() + data.size() > 390) data.clear(); // a saved key holds 400 bytes
    return itemKey(name, aux == anyAux ? 0 : aux, data);
}
}
