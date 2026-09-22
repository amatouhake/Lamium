#include "features/information/TargetInfo.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"

namespace lamium::information {
std::optional<TargetInfo> collectTargetInfo(IClientInstance& client) {
    auto* player = client.getLocalPlayer();
    if (!player) return {};
    auto const& hit = client.getLatestHitResult();
    if (hit.mType != HitResultType::Tile) return {};
    auto const& range = player->getDimension().mHeightRange;
    if (hit.mBlock.y < range->mMin || hit.mBlock.y >= range->mMax) return {};
    auto& source = player->getDimensionBlockSource();
    if (!source.getChunkAt(hit.mBlock)) return {};
    auto const& block = source.getBlock(hit.mBlock);
    if (block.isAir()) return {};
    TargetInfo result{block.buildDescriptionName(),block.getTypeName()};
    if (result.name.empty()) result.name = result.identifier;
    return result;
}
}
