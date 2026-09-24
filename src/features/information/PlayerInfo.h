#pragma once
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
class IClientInstance;
namespace lamium::information {
struct PlayerInfoRequest {
    bool coordinates{}, dimension{}, biome{}, facing{}, light{}, rotation{}, time{}, weather{};
};
struct LightLevels { int sky, block; };
inline std::optional<LightLevels> lightLevels(int sky, int block) {
    if (sky < 0 || sky > 15 || block < 0 || block > 15) return {};
    return LightLevels{sky,block};
}
struct PlayerInfo {
    struct Position { float x, y, z; };
    bool present = false;
    std::optional<Position> position;
    std::optional<std::string> dimension, biome;
    std::optional<float> yaw, pitch;
    std::optional<LightLevels> light;
    std::optional<int> worldTime; // Total world ticks; day count, clock and moon phase derive from it.
    std::optional<bool> raining; // Thunder is not separately exposed; true covers rain and storms.
};
// Values own their data, so consumers never retain Minecraft pointers.
PlayerInfo collectPlayerInfo(IClientInstance&, PlayerInfoRequest);
inline std::optional<std::string_view> facingKey(double yaw) {
    if (!std::isfinite(yaw)) return {};
    double normalized = std::fmod(yaw,360.);
    if (normalized < 0) normalized += 360;
    constexpr std::string_view keys[]{"facing.south","facing.west","facing.north","facing.east"};
    return keys[static_cast<int>(std::floor((normalized+45)/90))%4];
}
}
