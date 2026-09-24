#include "features/information/PlayerInfo.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Weather.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/block/BrightnessPair.h"
#include <limits>

namespace lamium::information {
PlayerInfo collectPlayerInfo(IClientInstance& client, PlayerInfoRequest request) {
    PlayerInfo result;
    auto* player = client.getLocalPlayer();
    if (!player) return result;
    result.present = true;
    // Actor state-vector position includes the player's vertical offset. Use
    // the SDK's feet position for both displayed XYZ and block sampling; do not
    // subtract a fixed standing eye height (poses can change that offset).
    auto const p = player->getFeetPos();
    bool finite = std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    if (request.coordinates && finite) result.position = PlayerInfo::Position{p.x,p.y,p.z};
    if (request.dimension) result.dimension = player->getDimension().mName.get();
    auto const rotation = player->getRotation();
    if ((request.facing || request.rotation) && std::isfinite(rotation.z)) result.yaw = rotation.z;
    if (request.rotation && std::isfinite(rotation.x)) result.pitch = rotation.x;
    if (request.time) {
        int ticks = player->getLevel().getTime();
        if (ticks >= 0) result.worldTime = ticks;
    }
    if ((request.biome || request.light) && finite) {
        auto safe = [](double value) {
            return value >= double(std::numeric_limits<int>::min())+1
                && value <= double(std::numeric_limits<int>::max())-1;
        };
        if (safe(p.x) && safe(p.y) && safe(p.z)) {
            BlockPos pos{static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y)),static_cast<int>(std::floor(p.z))};
            auto& region = player->getDimensionBlockSource();
            if (region.getChunkAt(pos)) {
                if (request.biome) result.biome = region.getBiome(pos).mHash->getString();
                if (request.weather)
                    result.raining = player->getDimension().mWeather->isRainingAt(region, pos);
                auto const& range = player->getDimension().mHeightRange;
                // Report stored sky/block light at the feet, not a night-adjusted
                // brightness or a prediction of server-side spawning rules.
                if (request.light && pos.y >= range->mMin && pos.y < range->mMax) {
                    auto brightness = region.getBrightnessPair(pos);
                    result.light = lightLevels(brightness.sky->mValue,brightness.block->mValue);
                }
            }
        }
    }
    return result;
}
}
