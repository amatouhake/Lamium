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
enum class Meter { Hearts, Icons, Bar, Number };
struct CardOptions {
    bool details = false;   // Other block states and mob details
    bool coordinates = false;
    Meter health = Meter::Hearts;
    Meter armor = Meter::Icons;
    Meter growth = Meter::Bar;
};
struct CardRow {
    std::string label;      // Translation key, or plain text when labelIsKey is false
    std::string value;      // Display text, or a translation key when valueIsKey
    bool labelIsKey = true;
    bool valueIsKey = false;
    std::optional<float> progress;
    Meter meter = Meter::Number;
    int current = 0, maximum = 0; // Health points for Hearts
    std::string icon;             // Item icon before the value (binary NBT)
    Tone tone = Tone::Normal;
    std::string before;           // A change: this, an arrow, then the value
};
// One heart is 2 HP and the slots follow the maximum (L-88), ten per line.
// Past five lines the hearts can no longer be read, so the row becomes a bar.
inline constexpr int heartsPerLine = 10, maxHeartLines = 5;
inline int heartSlots(int maximum) { return maximum > 0 ? (maximum + 1) / 2 : 0; }
inline int heartLines(int maximum) { return (heartSlots(maximum) + heartsPerLine - 1) / heartsPerLine; }
// Schematic corrections, health and growth come first because they are what
// people look for; other details and raw states only when asked for. At most `limit` rows.
inline std::vector<CardRow> cardRows(TargetInfo const& target, CardOptions const& options, size_t limit = 8) {
    std::vector<CardRow> rows;
    auto add = [&](CardRow row) { if (rows.size() < limit) rows.push_back(std::move(row)); };
    // Schematic rows first, under the name they correct, so a short card keeps them.
    for (auto const& detail : target.details)
        if (detail.kind == DetailKind::Schematic) {
            CardRow row{detail.label, detail.value, detail.labelIsKey, detail.valueIsKey};
            row.icon = detail.icon;
            row.tone = detail.tone;
            row.before = detail.before;
            add(std::move(row));
        }
    for (auto const& detail : target.details) {
        if (detail.kind == DetailKind::Health) {
            auto meter = detail.progress ? options.health : Meter::Number;
            if (meter == Meter::Hearts && (detail.maximum <= 0 || heartLines(detail.maximum) > maxHeartLines))
                meter = Meter::Bar;
            add({detail.label, detail.value, true, detail.valueIsKey, detail.progress, meter,
                 detail.current, detail.maximum});
        }
    }
    for (auto const& detail : target.details) {
        if (detail.kind == DetailKind::Armor)
            add({detail.label, detail.value, true, detail.valueIsKey, detail.progress,
                 detail.progress ? options.armor : Meter::Number});
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
// Ten icons like the vanilla armor bar: each is full, half or empty.
enum class Heart { Empty, Half, Full };
// Health in absolute units: one slot per 2 maximum HP, odd HP as a half heart.
inline std::vector<Heart> healthHearts(int current, int maximum) {
    std::vector<Heart> result(static_cast<size_t>(heartSlots(maximum)), Heart::Empty);
    current = std::clamp(current, 0, std::max(maximum, 0));
    for (int i = 0; i < static_cast<int>(result.size()); ++i)
        result[i] = current >= 2 * (i + 1) ? Heart::Full : current == 2 * i + 1 ? Heart::Half : Heart::Empty;
    return result;
}
inline std::array<Heart, 10> hearts(float progress) {
    std::array<Heart, 10> result{};
    if (!std::isfinite(progress)) progress = 0;
    int halves = static_cast<int>(std::lround(std::clamp(progress, 0.f, 1.f) * 20));
    for (int i = 0; i < 10; ++i)
        result[i] = halves >= 2 * (i + 1) ? Heart::Full : halves == 2 * i + 1 ? Heart::Half : Heart::Empty;
    return result;
}
// Mobs have no item of their own; their spawn egg stands in as the icon.
// Candidate item ids in lookup order: the exact name first, then the few
// Bedrock entity ids whose egg dropped the old suffix or name. The caller
// keeps the first candidate the item registry confirms; the sprite registry
// scan catches mismatches these guesses miss.
inline std::string spawnEggAlias(std::string_view entityIdentifier) {
    std::string id(entityIdentifier);
    if (id.ends_with("_v2")) id.resize(id.size() - 3); // villager_v2, zombie_villager_v2
    else if (id == "minecraft:evocation_illager") id = "minecraft:evoker";
    else if (id == "minecraft:vindication_illager") id = "minecraft:vindicator";
    return id;
}
inline std::vector<std::string> spawnEggCandidates(std::string_view entityIdentifier) {
    std::vector<std::string> result;
    if (entityIdentifier.empty()) return result;
    std::string exact(entityIdentifier);
    result.push_back(exact + "_spawn_egg");
    auto alias = spawnEggAlias(entityIdentifier);
    if (alias != exact) result.push_back(alias + "_spawn_egg");
    return result;
}
// An actor is not always named after the item that stands in for it: the
// thrown trident is "thrown_trident". Every form is checked against the item
// registry, so a stripped id that is no item resolves to nothing.
inline std::string entityItemAlias(std::string_view entityIdentifier) {
    // Bedrock kept the entity on an id its item no longer shares.
    if (entityIdentifier == "minecraft:ender_crystal") return "minecraft:end_crystal";
    if (entityIdentifier == "minecraft:eye_of_ender_signal") return "minecraft:ender_eye";
    if (entityIdentifier == "minecraft:xp_bottle") return "minecraft:experience_bottle";
    return {};
}
inline std::vector<std::string> entityItemCandidates(std::string_view entityIdentifier) {
    std::vector<std::string> result;
    if (entityIdentifier.empty()) return result;
    std::string exact(entityIdentifier);
    result.push_back(exact);
    auto thrown = exact.find("thrown_", exact.find(':') + 1);
    if (thrown != std::string::npos) result.push_back(exact.substr(0, thrown) + exact.substr(thrown + 7));
    if (auto alias = entityItemAlias(entityIdentifier); !alias.empty()) result.push_back(std::move(alias));
    return result;
}
// A block with a pick item uses it; a block without one (portal, fire, ...)
// falls back to its own texture, never to an unrelated item.
inline TargetIcon chooseBlockIcon(std::string item, short aux, std::string texture) {
    if (!item.empty()) return TargetIcon{IconKind::Item, std::move(item), aux};
    if (!texture.empty()) return TargetIcon{IconKind::Texture, std::move(texture)};
    return {};
}
// The uv set addresses the stitched terrain atlas while the icon draws the
// source file its path names. Restate that tile - one animation frame - in
// the file's own coordinates, starting at the top-left frame; a set that
// already matches its file's size is left alone.
inline TargetIcon fileFrameUv(TargetIcon icon, float textureW, float textureH, float sourceW, float sourceH) {
    if (!(textureW > 0) || !(textureH > 0) || !(sourceW > 0) || !(sourceH > 0)
        || (textureW == sourceW && textureH == sourceH))
        return icon;
    float u = (icon.u1 - icon.u0) * textureW / sourceW;
    float v = (icon.v1 - icon.v0) * textureH / sourceH;
    icon.u0 = 0;
    icon.v0 = 0;
    icon.u1 = (u > 0) ? std::min(1.f, u) : 1.f;
    icon.v1 = (v > 0) ? std::min(1.f, v) : 1.f;
    return icon;
}
// The card eases between targets: 0.1 s, ease-out.
inline constexpr double morphSeconds = 0.1;
inline float morphProgress(double elapsed) {
    if (!(elapsed > 0)) return 0;
    double t = std::min(1.0, elapsed / morphSeconds);
    return static_cast<float>(1 - (1 - t) * (1 - t));
}
// The content is laid out at the final size, so it is drawn only while the
// easing background already covers that box (shrinking, or settled).
inline bool cardContentFits(float bgX, float bgY, float bgW, float bgH, float x, float y, float w, float h) {
    constexpr float slack = .5f;
    return bgX <= x + slack && bgY <= y + slack && bgX + bgW >= x + w - slack && bgY + bgH >= y + h - slack;
}
}
