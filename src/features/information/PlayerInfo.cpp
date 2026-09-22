#include "features/information/PlayerInfo.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/biome/Biome.h"
#include <limits>

namespace lamium::information {
PlayerInfo collectPlayerInfo(IClientInstance& client, PlayerInfoRequest request) {
    PlayerInfo result;
    auto* player = client.getLocalPlayer();
    if (!player) return result;
    result.present = true;
    auto const& p = player->getPosition();
    bool finite = std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    if (request.coordinates && finite) result.position = PlayerInfo::Position{p.x,p.y,p.z};
    if (request.dimension) result.dimension = player->getDimension().mName.get();
    if (request.facing && std::isfinite(player->getRotation().z)) result.yaw = player->getRotation().z;
    if (request.biome && finite) {
        auto safe = [](double value) {
            return value >= double(std::numeric_limits<int>::min())+1
                && value <= double(std::numeric_limits<int>::max())-1;
        };
        if (safe(p.x) && safe(p.y) && safe(p.z)) {
            BlockPos pos{static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y)),static_cast<int>(std::floor(p.z))};
            auto& region = player->getDimensionBlockSource();
            if (region.getChunkAt(pos)) result.biome = region.getBiome(pos).mHash->getString();
        }
    }
    return result;
}
}
