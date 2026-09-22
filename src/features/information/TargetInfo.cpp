#include "features/information/TargetInfo.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/locale/I18n.h"
#include "mc/deps/nbt/CompoundTagVariant.h"

namespace lamium::information {
std::optional<TargetInfo> collectTargetInfo(IClientInstance& client, bool includeStates) {
    auto* player = client.getLocalPlayer();
    if (!player) return {};
    auto const& hit = client.getLatestHitResult();
    if (hit.mType == HitResultType::Entity) {
        auto* entity = hit.getEntity();
        if (!entity || entity == player || entity->mRemoved
            || entity->getDimensionId() != player->getDimensionId()) return {};
        // Respect the game's filtered name, then use its localized entity type.
        // The weak hit reference is resolved only for this snapshot.
        TargetInfo result{entity->getFilteredNameTag(),entity->getTypeName()};
        if (result.name.empty()) {
            auto key = entity->getEntityLocNameString();
            result.name = getI18n().get(key,getI18n().getCurrentLanguage());
            if (result.name.empty() || result.name == key) result.name = result.identifier;
        }
        return result;
    }
    if (hit.mType != HitResultType::Tile) return {};
    auto const& range = player->getDimension().mHeightRange;
    if (hit.mBlock.y < range->mMin || hit.mBlock.y >= range->mMax) return {};
    auto& source = player->getDimensionBlockSource();
    if (!source.getChunkAt(hit.mBlock)) return {};
    auto const& block = source.getBlock(hit.mBlock);
    if (block.isAir()) return {};
    TargetInfo result{block.buildDescriptionName(),block.getTypeName()};
    if (result.name.empty()) result.name = result.identifier;
    if (includeStates) {
        auto const& tags = block.mSerializationId->mTags;
        auto found = tags.find("states");
        if (found != tags.end()) {
            if (auto* states = std::get_if<CompoundTag>(&found->second.mTagStorage)) {
                for (auto const& [key,value] : states->mTags) {
                    std::optional<std::string> text;
                    if (auto* v = std::get_if<ByteTag>(&value.mTagStorage)) text = std::to_string(v->data);
                    else if (auto* v = std::get_if<IntTag>(&value.mTagStorage)) text = std::to_string(v->data);
                    else if (auto* v = std::get_if<StringTag>(&value.mTagStorage)) text = *v;
                    if (text) result.states.push_back(key + ": " + *text);
                }
            }
        }
    }
    return result;
}
}
