#pragma once
#include "features/information/TargetInfo.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace lamium::information {
// Target card content (BACKLOG L-08, DESIGN "HUD"). Pure: decides which rows
// the card shows and how ranged values are drawn; InfoHud draws them.
enum class Meter { Hearts, Bar, Number };
struct CardOptions {
    bool details = false;   // Other block states and mob details
    bool coordinates = false;
    Meter health = Meter::Hearts;
    Meter growth = Meter::Bar;
};
struct CardRow {
    std::string label;      // Translation key, or plain text when labelIsKey is false
    std::string value;      // Display text, or a translation key when valueIsKey
    bool labelIsKey = true;
    bool valueIsKey = false;
    std::optional<float> progress;
    Meter meter = Meter::Number;
};
// Health and growth come first because they are what people look for; other
// details and raw states only when asked for. At most `limit` rows.
inline std::vector<CardRow> cardRows(TargetInfo const& target, CardOptions const& options, size_t limit = 8) {
    std::vector<CardRow> rows;
    auto add = [&](CardRow row) { if (rows.size() < limit) rows.push_back(std::move(row)); };
    for (auto const& detail : target.details) {
        if (detail.kind == DetailKind::Health)
            add({detail.label, detail.value, true, detail.valueIsKey, detail.progress,
                 detail.progress ? options.health : Meter::Number});
    }
    for (auto const& detail : target.details) {
        if (detail.kind == DetailKind::Growth)
            add({detail.label, detail.value, true, detail.valueIsKey, detail.progress,
                 detail.progress ? options.growth : Meter::Number});
    }
    if (options.coordinates && target.blockPosition) {
        auto const& p = *target.blockPosition;
        add({"target.position", std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z)});
    }
    if (!options.details) return rows;
    for (auto const& detail : target.details)
        if (detail.kind == DetailKind::Other) add({detail.label, detail.value, true, detail.valueIsKey, detail.progress});
    for (auto const& state : target.states) {
        auto split = state.find(": ");
        if (split == std::string::npos) add({state, "", false});
        else add({state.substr(0, split), state.substr(split + 2), false});
    }
    return rows;
}
// Ten hearts like the vanilla health bar: each is full, half or empty.
enum class Heart { Empty, Half, Full };
inline std::array<Heart, 10> hearts(float progress) {
    std::array<Heart, 10> result{};
    if (!std::isfinite(progress)) progress = 0;
    int halves = static_cast<int>(std::lround(std::clamp(progress, 0.f, 1.f) * 20));
    for (int i = 0; i < 10; ++i)
        result[i] = halves >= 2 * (i + 1) ? Heart::Full : halves == 2 * i + 1 ? Heart::Half : Heart::Empty;
    return result;
}
// Mobs have no item of their own; their spawn egg stands in as the icon.
// A few Bedrock entity ids differ from their egg's name.
inline std::string spawnEggItem(std::string_view entityIdentifier) {
    if (entityIdentifier.empty()) return {};
    std::string id(entityIdentifier);
    if (id.ends_with("_v2")) id.resize(id.size() - 3); // villager_v2, zombie_villager_v2
    if (id == "minecraft:evocation_illager") id = "minecraft:evoker";
    return id + "_spawn_egg";
}
// "Target range" choices: 0 is the game's own reach (its hit result, or its
// pick range from a detached camera); the rest are fixed block distances.
inline constexpr std::array<float, 5> targetRanges{0, 8, 16, 32, 64};
inline std::optional<float> rangeBlocks(int choice) {
    if (choice <= 0 || choice >= static_cast<int>(targetRanges.size())) return std::nullopt;
    return targetRanges[static_cast<size_t>(choice)];
}
// The card eases between targets: 0.1 s, ease-out.
inline constexpr double morphSeconds = 0.1;
inline float morphProgress(double elapsed) {
    if (!(elapsed > 0)) return 0;
    double t = std::min(1.0, elapsed / morphSeconds);
    return static_cast<float>(1 - (1 - t) * (1 - t));
}
}
