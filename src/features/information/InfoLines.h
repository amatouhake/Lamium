#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::information {
// Ordered info-line model (BACKLOG L-04b; providers extended by L-05).
// Switches stay the existing Information flags; only the order is saved.
inline constexpr auto infoLineIds = std::to_array<std::string_view>(
    {"coordinates", "dimension", "biome", "facing", "fps", "frameTime", "light", "ping", "rotation",
     "block", "chunk", "speed", "time", "weather", "moon"});
struct ChunkPosition { int chunkX, chunkZ, inX, inZ; };
inline ChunkPosition chunkPosition(double x, double z) {
    int chunkX = static_cast<int>(std::floor(x / 16));
    int chunkZ = static_cast<int>(std::floor(z / 16));
    return {chunkX, chunkZ, static_cast<int>(std::floor(x - chunkX * 16)),
            static_cast<int>(std::floor(z - chunkZ * 16))};
}
inline std::string formatChunk(ChunkPosition position) {
    return std::to_string(position.chunkX) + ", " + std::to_string(position.chunkZ) + " ("
        + std::to_string(position.inX) + ", " + std::to_string(position.inZ) + ")";
}
// Day count and clock derive from the total world time in ticks. The 0-based
// day and the (ticks + 6h) clock epoch match /time query output; confirm once
// in game. Moon phase cycles 8 in-game days like Java, 0 is the full moon;
// confirm the alignment against the visible moon.
inline int dayCount(int worldTime) { return worldTime / 24000; }
inline int dayTicks(int worldTime) {
    int ticks = worldTime % 24000;
    return ticks < 0 ? ticks + 24000 : ticks;
}
inline std::string formatClock(int worldTime) {
    int ticks = dayTicks(worldTime);
    int hours = (ticks / 1000 + 6) % 24;
    int minutes = (ticks % 1000) * 60 / 1000;
    std::string result = (hours < 10 ? "0" : "") + std::to_string(hours) + ":";
    return result + (minutes < 10 ? "0" : "") + std::to_string(minutes);
}
inline int moonPhase(int worldTime) { return dayCount(worldTime) % 8; }
inline constexpr std::string_view moonPhaseKey(int phase) {
    constexpr std::string_view keys[] = {"moon.full", "moon.waningGibbous", "moon.lastQuarter", "moon.waningCrescent",
                                         "moon.new", "moon.waxingCrescent", "moon.firstQuarter", "moon.waxingGibbous"};
    return phase >= 0 && phase < 8 ? keys[phase] : "moon.full";
}
inline std::string formatRotation(float yaw, float pitch) {
    return std::format("{:.1f} / {:.1f}", yaw, pitch);
}
inline std::string formatSpeed(double blocksPerSecond) {
    return std::format("{:.1f}", blocksPerSecond);
}
struct PositionSample { double x, y, z, t; };
// Blocks/s from position deltas over a >=0.5 s window. Teleports, stalls and
// non-finite input restart the window instead of spiking the average.
class SpeedSampler {
    std::deque<PositionSample> window;
    static double distance(PositionSample const& a, PositionSample const& b) {
        double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
public:
    void reset() { window.clear(); }
    void sample(double x, double y, double z, double now) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(now)) {
            reset();
            return;
        }
        if (!window.empty()) {
            auto const& last = window.back();
            double gap = now - last.t;
            if (!(gap >= 0) || gap > 2 || distance({x, y, z, now}, last) > 50) reset();
        }
        if (window.empty()) {
            window.push_back({x, y, z, now});
            return;
        }
        window.push_back({x, y, z, now});
        while (window.size() > 2 && window.back().t - window.front().t > 1) window.pop_front();
    }
    std::optional<double> read() const {
        if (window.size() < 2) return {};
        double span = window.back().t - window.front().t;
        if (span < 0.5) return {};
        return distance(window.back(), window.front()) / span;
    }
};
inline std::vector<std::string> defaultLineOrder() {
    return {infoLineIds.begin(), infoLineIds.end()};
}
// Stored order wins for known ids; missing known ids append in default order;
// unknown ids drop out (typos must not pin a dead row).
inline std::vector<std::string> mergeLineOrder(std::vector<std::string> stored) {
    std::vector<std::string> result;
    for (auto const& id : stored)
        if (std::find(infoLineIds.begin(), infoLineIds.end(), id) != infoLineIds.end()
            && std::find(result.begin(), result.end(), id) == result.end())
            result.push_back(id);
    for (auto id : infoLineIds)
        if (std::find(result.begin(), result.end(), id) == result.end())
            result.emplace_back(id);
    return result;
}
}
