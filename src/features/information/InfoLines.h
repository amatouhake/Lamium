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
    {"coordinates", "block", "targetBlock", "scaledCoordinates", "chunk", "slimeChunk", "dimension", "biome", "facing",
     "rotation", "yaw", "pitch", "speed", "horizontalSpeed", "verticalSpeed", "sprinting", "light",
     "weather", "time", "moon", "difficulty", "fps", "frameTime", "ping", "realTime"});
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
inline std::string formatAngle(float value) { return std::format("{:.1f}", value); }
inline std::string formatSpeed(double blocksPerSecond) {
    return std::format("{:.1f}", blocksPerSecond);
}
inline std::string formatRealTime(int hour, int minute) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return {};
    return std::format("{:02}:{:02}", hour, minute);
}
inline std::string formatRealDateTime(int year, int month, int day, int hour, int minute) {
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31) return {};
    auto time = formatRealTime(hour, minute);
    return time.empty() ? std::string{} : std::format("{:04}-{:02}-{:02} {}", year, month, day, time);
}
enum class ScaledDimension { Overworld, Nether };
struct ScaledPosition { double x, y, z; ScaledDimension destination; };
inline std::optional<ScaledPosition> scaledPosition(double x, double y, double z, int dimension) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return {};
    if (dimension == 0) return ScaledPosition{x / 8, y, z / 8, ScaledDimension::Nether};
    if (dimension == 1) return ScaledPosition{x * 8, y, z * 8, ScaledDimension::Overworld};
    return {};
}
inline std::string biomeTranslationKey(std::string_view identifier) {
    auto split = identifier.find(':');
    auto name = split == std::string_view::npos ? identifier : identifier.substr(split + 1);
    return name.empty() ? std::string{} : "biome." + std::string(name) + ".name";
}
enum class BiomeDisplay { Name, NameAndId, Id };
inline std::string formatBiomeValue(std::string_view localized, std::string_view identifier, BiomeDisplay display) {
    if (display == BiomeDisplay::Id || localized.empty()) return std::string(identifier);
    if (display == BiomeDisplay::NameAndId && localized != identifier)
        return std::format("{} ({})", localized, identifier);
    return std::string(localized);
}
struct PositionSample { double x, y, z, t; };
struct SpeedValues { double total, horizontal, vertical; };
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
    std::optional<SpeedValues> read() const {
        if (window.size() < 2) return {};
        double span = window.back().t - window.front().t;
        if (span < 0.5) return {};
        auto const& first = window.front();
        auto const& last = window.back();
        double dx = last.x - first.x, dy = last.y - first.y, dz = last.z - first.z;
        return SpeedValues{std::sqrt(dx * dx + dy * dy + dz * dz) / span,
                           std::sqrt(dx * dx + dz * dz) / span, dy / span};
    }
};
inline std::vector<std::string> defaultLineOrder() {
    return {infoLineIds.begin(), infoLineIds.end()};
}
// Stored order wins for known ids; a missing known id (a line added since the
// order was saved) goes right after the line before it in the default order
// (the first default line goes last); unknown ids drop out (typos must not pin a dead row).
inline std::vector<std::string> mergeLineOrder(std::vector<std::string> stored) {
    std::vector<std::string> result;
    for (auto const& id : stored)
        if (std::find(infoLineIds.begin(), infoLineIds.end(), id) != infoLineIds.end()
            && std::find(result.begin(), result.end(), id) == result.end())
            result.push_back(id);
    for (size_t i = 0; i < infoLineIds.size(); ++i) {
        if (std::find(result.begin(), result.end(), infoLineIds[i]) != result.end()) continue;
        auto at = i > 0 ? std::find(result.begin(), result.end(), infoLineIds[i - 1]) + 1 : result.end();
        result.emplace(at, infoLineIds[i]);
    }
    return result;
}
// Move one line up (direction < 0) or down within the order. Clamped at the
// ends; unknown ids leave the order unchanged.
inline std::vector<std::string> moveLineOrder(std::vector<std::string> order, std::string_view id, int direction) {
    auto at = std::find(order.begin(), order.end(), id);
    if (at == order.end()) return order;
    size_t index = static_cast<size_t>(at - order.begin());
    size_t other = direction < 0 ? (index == 0 ? 0 : index - 1)
                                 : (index + 1 >= order.size() ? order.size() - 1 : index + 1);
    if (other == index) return order;
    std::swap(order[index], order[other]);
    return order;
}
}
