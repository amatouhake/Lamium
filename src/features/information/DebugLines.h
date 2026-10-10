#pragma once
#include "features/information/FrameRateMeter.h"
#include "features/information/InfoLines.h"
#include "features/information/PlayerInfo.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lamium::information {
// Debug View line model (BACKLOG L-54, DESIGN "HUD"). Pure: the caller collects
// owned values and supplies the game-standard text (translations); this decides
// the two columns, the Java-F3 literals, omission and the look-at truncation.
enum class DebugLabel { GameStandard, JavaF3 };
// Entity counts by kind (L-120, docs/demos/entity-counts.html): each entity
// lands in the first kind that fits, so the parts add up to the total.
enum class EntityKind { Player, Item, Hostile, Passive, Other };
using EntityKinds = std::array<int, 5>;
inline EntityKind entityKind(bool player, bool item, bool monster, bool mob) {
    if (player) return EntityKind::Player;
    if (item) return EntityKind::Item;
    if (monster) return EntityKind::Hostile;
    if (mob) return EntityKind::Passive;
    return EntityKind::Other;
}
struct TypeCount {
    std::string id, name; // name: the localized type name, or empty
    int count = 0;
};
// The most common types, most first (ties by identifier), and the rest summed.
struct TypeSummary {
    std::vector<TypeCount> top;
    int moreTypes = 0, moreCount = 0;
};
inline TypeSummary summarizeTypes(std::vector<TypeCount> all, size_t limit) {
    std::sort(all.begin(), all.end(), [](TypeCount const& a, TypeCount const& b) {
        return a.count != b.count ? a.count > b.count : a.id < b.id;
    });
    TypeSummary summary;
    for (size_t i = 0; i < all.size(); ++i) {
        if (i < limit) {
            summary.top.push_back(std::move(all[i]));
        } else {
            ++summary.moreTypes;
            summary.moreCount += all[i].count;
        }
    }
    return summary;
}
// Identifiers without the vanilla namespace; add-on namespaces stay.
inline std::string_view shortIdentifier(std::string_view id) {
    constexpr std::string_view vanilla = "minecraft:";
    return id.starts_with(vanilla) ? id.substr(vanilla.size()) : id;
}
struct DebugTarget {
    std::string identifier;
    std::vector<std::string> javaLines; // "Health: 20 / 20 | Armor: 2" or the raw block states.
    std::vector<std::string> gameLines; // Localized detail lines; at most two are used.
};
struct DebugValues {
    std::string header; // "Minecraft 1.26.51 · LeviLamina 26.51.5 · Lamium 0.1.6"
    std::optional<FrameStatistics> timing;
    std::optional<std::int64_t> ping;
    std::optional<int> renderDistance, maxRenderDistance;
    std::optional<int> entities, chunks, particles; // Client counts (L-57)
    std::optional<EntityKinds> entityKinds;          // The entity total by kind (L-120)
    std::optional<TypeSummary> entityTypes;          // The most common entity types (L-120)
    std::optional<double> x, y, z;
    std::optional<float> yaw, pitch;
    // FreeCamera (L-124): x..pitch, block, chunk, light and biome are the
    // camera's; body is the player's own position and angles.
    struct Body { double x, y, z; float yaw, pitch; };
    std::optional<Body> body;
    std::string dimension, biome;
    std::optional<int> skyLight, blockLight;
    std::optional<int> difficulty; // 0 peaceful, 1 easy, 2 normal, 3 hard
    std::optional<int> worldTime;
    std::optional<bool> raining;
    std::optional<DebugTarget> target;
    std::optional<bool> rayTracing, vibrantVisuals, clouds, fancySkies, fullscreen;
    std::optional<int> maxFps;
    std::optional<std::string> memory, cpu, gpu, display, os;
};
struct DebugLine { std::string text; };
struct DebugColumns { std::vector<DebugLine> left, right; };
// Game-standard lines: the caller formats with ui::translated and passes only
// the lines it can build; an empty string omits that line.
struct GameText {
    std::string perf, counts, coordinates, bodyCoordinates, blockChunk, facing, bodyFacing, light, biome, time, lookAt, dimension,
        renderDistance, visuals, screen, client, system, memory, cpu, gpu, display, os;
    std::string entityKinds;              // "内訳: プレイヤー 2 | ..." (L-120)
    std::vector<std::string> entityTypes; // Heading, "ゾンビ 11" lines and the rest (L-120)
};
// Same quarter mapping as facingKey: yaw 0 faces south (+Z), 90 west (-X).
inline std::string_view javaFacingWord(double yaw) {
    static constexpr std::string_view words[]{"south", "west", "north", "east"};
    if (!std::isfinite(yaw)) return words[0];
    double normalized = std::fmod(yaw, 360.);
    if (normalized < 0) normalized += 360;
    return words[static_cast<int>(std::floor((normalized + 45) / 90)) % 4];
}
inline std::string_view javaFacingAxis(double yaw) {
    static constexpr std::string_view axes[]{"+Z", "-X", "-Z", "+X"};
    if (!std::isfinite(yaw)) return axes[0];
    double normalized = std::fmod(yaw, 360.);
    if (normalized < 0) normalized += 360;
    return axes[static_cast<int>(std::floor((normalized + 45) / 90)) % 4];
}
inline std::string_view javaDifficulty(int difficulty) {
    static constexpr std::string_view words[]{"Peaceful", "Easy", "Normal", "Hard"};
    return difficulty >= 0 && difficulty < 4 ? words[difficulty] : words[2];
}
inline std::string_view javaMoonName(int phase) {
    static constexpr std::string_view words[]{"Full moon", "Waning gibbous", "Last quarter", "Waning crescent",
                                              "New moon", "Waxing crescent", "First quarter", "Waxing gibbous"};
    return phase >= 0 && phase < 8 ? words[phase] : words[0];
}
// Appends a separator and a part only when the part is not empty.
inline void joinPart(std::string& line, std::string_view part, std::string_view separator = " | ") {
    if (part.empty()) return;
    if (!line.empty()) line += separator;
    line += part;
}
inline void addLookAt(std::vector<DebugLine>& lines, DebugValues const& value, DebugLabel style,
                      GameText const& game) {
    if (!value.target) return;
    if (style == DebugLabel::GameStandard && game.lookAt.empty()) return;
    auto const& target = *value.target;
    lines.push_back({" "});
    lines.push_back({style == DebugLabel::JavaF3 ? std::string("Look at") : game.lookAt});
    lines.push_back({target.identifier});
    auto const& parts = style == DebugLabel::JavaF3 ? target.javaLines : target.gameLines;
    for (size_t i = 0; i < std::min<size_t>(parts.size(), 2); ++i) lines.push_back({parts[i]});
}
// Rows the right column moves down so none of its lines, kept at the screen's
// right edge, runs into the left line beside it. A narrow GUI (large UI
// scale) would otherwise push the column off screen.
inline size_t rightColumnOffset(std::vector<float> const& left, std::vector<float> const& right, float available,
                                float gap) {
    for (size_t offset = 0; offset < left.size(); ++offset) {
        bool clear = true;
        for (size_t i = 0; i < right.size() && clear; ++i) {
            size_t row = i + offset;
            clear = row >= left.size() || left[row] + gap + right[i] <= available;
        }
        if (clear) return offset;
    }
    return left.size();
}
inline DebugColumns buildDebugColumns(DebugValues const& value, DebugLabel style, GameText const& game) {
    DebugColumns columns;
    auto& left = columns.left;
    auto& right = columns.right;
    if (!value.header.empty()) left.push_back({value.header});
    std::string perf;
    if (value.timing) perf = std::format("{} fps ({:.1f} ms)", std::lround(value.timing->fps), value.timing->milliseconds);
    if (style == DebugLabel::JavaF3) {
        if (value.ping) joinPart(perf, std::format("Ping {} ms", *value.ping));
        if (value.renderDistance) joinPart(perf, std::format("Render distance {}", *value.renderDistance));
    } else {
        joinPart(perf, game.perf);
    }
    if (!perf.empty()) left.push_back({std::move(perf)});
    if (style == DebugLabel::JavaF3) {
        std::string counts;
        if (value.entities) joinPart(counts, std::format("E: {}", *value.entities));
        if (value.chunks) joinPart(counts, std::format("C: {}", *value.chunks));
        if (value.particles) joinPart(counts, std::format("P: {}", *value.particles));
        if (!counts.empty()) left.push_back({std::move(counts)});
        if (auto const& k = value.entityKinds)
            left.push_back({std::format("E: Players {}, Items {}, Hostile {}, Passive {}, Other {}", (*k)[0], (*k)[1], (*k)[2],
                                        (*k)[3], (*k)[4])});
    } else {
        if (!game.counts.empty()) left.push_back({game.counts});
        if (!game.entityKinds.empty()) left.push_back({game.entityKinds});
    }
    if (style == DebugLabel::JavaF3) {
        std::string_view camera = value.body ? "Camera " : "";
        if (value.x && value.y && value.z)
            left.push_back({std::format("{}XYZ: {:.1f} / {:.1f} / {:.1f}", camera, *value.x, *value.y, *value.z)});
        if (auto const& b = value.body)
            left.push_back({std::format("Player XYZ: {:.1f} / {:.1f} / {:.1f}", b->x, b->y, b->z)});
        if (value.x && value.z) {
            auto chunk = chunkPosition(*value.x, *value.z);
            left.push_back({std::format("Block: {} {} {} | Chunk: {}, {}",
                static_cast<int>(std::floor(*value.x)), value.y ? static_cast<int>(std::floor(*value.y)) : 0,
                static_cast<int>(std::floor(*value.z)), chunk.chunkX, chunk.chunkZ)});
        }
        if (value.yaw && value.pitch)
            left.push_back({std::format("{}Facing: {} (towards {}) | Yaw/Pitch: {:.1f} / {:.1f}", camera,
                javaFacingWord(*value.yaw), javaFacingAxis(*value.yaw), *value.yaw, *value.pitch)});
        if (auto const& b = value.body)
            left.push_back({std::format("Player Facing: {} (towards {}) | Yaw/Pitch: {:.1f} / {:.1f}",
                javaFacingWord(b->yaw), javaFacingAxis(b->yaw), b->yaw, b->pitch)});
        if (value.skyLight && value.blockLight)
            left.push_back({std::format("Client Light: {} (sky {}, block {})",
                std::max(*value.skyLight, *value.blockLight), *value.skyLight, *value.blockLight)});
        if (!value.biome.empty()) {
            std::string line = std::format("Biome: {}", value.biome);
            if (value.difficulty) joinPart(line, std::format("Difficulty: {}", javaDifficulty(*value.difficulty)));
            left.push_back({std::move(line)});
        }
        if (value.worldTime) {
            std::string line = std::format("Day {} · {}", dayCount(*value.worldTime), formatClock(*value.worldTime));
            if (value.raining) joinPart(line, *value.raining ? "Rain" : "Clear");
            joinPart(line, javaMoonName(moonPhase(*value.worldTime)));
            left.push_back({std::move(line)});
        }
    } else {
        if (!game.coordinates.empty()) left.push_back({game.coordinates});
        if (!game.bodyCoordinates.empty()) left.push_back({game.bodyCoordinates});
        if (!game.blockChunk.empty()) left.push_back({game.blockChunk});
        if (!game.facing.empty()) left.push_back({game.facing});
        if (!game.bodyFacing.empty()) left.push_back({game.bodyFacing});
        if (!game.light.empty()) left.push_back({game.light});
        if (!game.biome.empty()) left.push_back({game.biome});
        if (!game.time.empty()) left.push_back({game.time});
    }
    addLookAt(left, value, style, game);
    // The type list closes the left column (maintainer 2026-10-11).
    if (style == DebugLabel::JavaF3) {
        if (auto const& types = value.entityTypes; types && !types->top.empty()) {
            left.push_back({" "});
            left.push_back({"Entities by type"});
            for (auto const& type : types->top) left.push_back({std::format("{}: {}", shortIdentifier(type.id), type.count)});
            if (types->moreTypes) left.push_back({std::format("{} more types: {}", types->moreTypes, types->moreCount)});
        }
    } else if (!game.entityTypes.empty()) {
        left.push_back({" "});
        for (auto const& line : game.entityTypes) left.push_back({line});
    }
    std::string clientHead = style == DebugLabel::JavaF3 ? "Client" : game.client;
    std::string systemHead = style == DebugLabel::JavaF3 ? "System" : game.system;
    std::string dimension, distance, visuals, screen;
    if (style == DebugLabel::JavaF3) {
        if (!value.dimension.empty()) dimension = std::format("Dimension: {}", value.dimension);
        if (value.renderDistance) {
            distance = std::format("Render distance: {}", *value.renderDistance);
            if (value.maxRenderDistance) distance += std::format(" / {}", *value.maxRenderDistance);
        }
        auto onOff = [](bool on) { return on ? "on" : "off"; };
        if (value.rayTracing) visuals = std::format("Ray Tracing: {}", onOff(*value.rayTracing));
        if (value.vibrantVisuals) joinPart(visuals, std::format("Vibrant Visuals: {}", onOff(*value.vibrantVisuals)), " · ");
        if (value.fullscreen) screen = std::format("Fullscreen: {}", onOff(*value.fullscreen));
        if (value.maxFps) joinPart(screen, std::format("Max FPS: {}", *value.maxFps), " · ");
        if (value.clouds) {
            std::string clouds = std::format("Clouds: {}", onOff(*value.clouds));
            if (value.fancySkies) joinPart(clouds, std::format("Skies: {}", *value.fancySkies ? "fancy" : "plain"), " · ");
            if (!screen.empty()) screen += " · ";
            screen += clouds;
        } else if (value.fancySkies) {
            joinPart(screen, std::format("Skies: {}", *value.fancySkies ? "fancy" : "plain"), " · ");
        }
    } else {
        dimension = game.dimension;
        distance = game.renderDistance;
        visuals = game.visuals;
        screen = game.screen;
    }
    bool hasClient = !clientHead.empty() && (!dimension.empty() || !distance.empty() || !visuals.empty() || !screen.empty());
    if (hasClient) {
        right.push_back({clientHead});
        for (auto const* line : {&dimension, &distance, &visuals, &screen})
            if (!line->empty()) right.push_back({*line});
    }
    std::string memory = style == DebugLabel::JavaF3 ? (value.memory ? "Memory: " + *value.memory : "") : game.memory;
    std::string cpu = style == DebugLabel::JavaF3 ? (value.cpu ? "CPU: " + *value.cpu : "") : game.cpu;
    std::string gpu = style == DebugLabel::JavaF3 ? (value.gpu ? "GPU: " + *value.gpu : "") : game.gpu;
    std::string display = style == DebugLabel::JavaF3 ? (value.display ? "Display: " + *value.display : "") : game.display;
    std::string os = style == DebugLabel::JavaF3 ? (value.os ? "OS: " + *value.os : "") : game.os;
    bool hasSystem = !systemHead.empty() && (!memory.empty() || !cpu.empty() || !gpu.empty() || !display.empty() || !os.empty());
    if (hasSystem) {
        if (!right.empty()) right.push_back({" "});
        right.push_back({systemHead});
        for (auto const* line : {&memory, &cpu, &gpu, &display, &os})
            if (!line->empty()) right.push_back({*line});
    }
    return columns;
}
}
