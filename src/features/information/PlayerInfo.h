#pragma once
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
class IClientInstance;
namespace lamium::information {
struct PlayerInfoRequest {
    bool coordinates{}, dimension{}, biome{}, facing{}, light{}, rotation{}, time{}, weather{}, difficulty{}, sprinting{};
};
struct LightLevels { int sky, block; };
// Where FreeCamera looks from (feet as if a player stood there) and its angles.
struct CameraPose { double x, y, z; float yaw, pitch; };
inline std::optional<LightLevels> lightLevels(int sky, int block) {
    if (sky < 0 || sky > 15 || block < 0 || block > 15) return {};
    return LightLevels{sky,block};
}
struct PlayerInfo {
    struct Position { float x, y, z; };
    bool present = false;
    // FreeCamera (L-124): position, angles and the place samples (biome,
    // light, weather) are the camera's; body holds the player's own.
    bool camera = false;
    std::optional<Position> position, bodyPosition;
    std::optional<float> bodyYaw, bodyPitch;
    std::optional<std::string> dimension, biome;
    std::optional<int> dimensionId, difficulty;
    std::optional<float> yaw, pitch;
    std::optional<bool> sprinting;
    std::optional<LightLevels> light;
    std::optional<int> worldTime; // Total world ticks; day count, clock and moon phase derive from it.
    std::optional<bool> raining; // Thunder is not separately exposed; true covers rain and storms.
};
// Values own their data, so consumers never retain Minecraft pointers.
PlayerInfo collectPlayerInfo(IClientInstance&, PlayerInfoRequest, std::optional<CameraPose> camera = {});
inline std::optional<std::string_view> facingKey(double yaw) {
    if (!std::isfinite(yaw)) return {};
    double normalized = std::fmod(yaw,360.);
    if (normalized < 0) normalized += 360;
    constexpr std::string_view keys[]{"facing.south","facing.west","facing.north","facing.east"};
    return keys[static_cast<int>(std::floor((normalized+45)/90))%4];
}
}
