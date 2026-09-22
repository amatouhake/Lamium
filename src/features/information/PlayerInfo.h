#pragma once
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
class IClientInstance;
namespace lamium::information {
struct PlayerInfoRequest { bool coordinates{}, dimension{}, biome{}, facing{}; };
struct PlayerInfo {
    struct Position { float x, y, z; };
    bool present = false;
    std::optional<Position> position;
    std::optional<std::string> dimension, biome;
    std::optional<float> yaw;
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
