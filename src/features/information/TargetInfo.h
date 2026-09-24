#pragma once
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
class IClientInstance;
namespace lamium::information {
// Owned snapshot shared by target HUD and future detailed debug providers.
struct TargetInfo {
    std::string name, identifier;
    std::vector<std::string> states;
    struct DetailRow {
        std::string label; // Translation key, resolved by the caller.
        std::string value; // Display text, or a translation key when valueIsKey.
        bool valueIsKey = false;
        std::optional<float> progress; // 0-1 for ranged values; empty otherwise.
    };
    std::vector<DetailRow> details;
    struct BlockPosition { int x, y, z; };
    std::optional<BlockPosition> blockPosition = std::nullopt;
};
enum class StateKind { Integer, Text };
// Max growth/age stage for common crops; unknown blocks show the value only.
inline std::optional<int> growthMax(std::string_view identifier, bool age) {
    if (!age) {
        if (identifier == "minecraft:wheat" || identifier == "minecraft:carrots"
            || identifier == "minecraft:potatoes")
            return 7;
        if (identifier == "minecraft:beetroot" || identifier == "minecraft:sweet_berry_bush") return 3;
        return {};
    }
    if (identifier == "minecraft:nether_wart") return 3;
    if (identifier == "minecraft:cocoa") return 2;
    return {};
}
// Interpret one raw block state into a readable detail row. Nullopt for states
// with no readable form; those stay in the raw states list.
inline std::optional<TargetInfo::DetailRow> interpretBlockState(std::string_view key, StateKind kind,
        long long number, std::string_view text, std::string_view identifier) {
    using DetailRow = TargetInfo::DetailRow;
    if ((key == "growth" || key == "age") && kind == StateKind::Integer && number >= 0) {
        auto max = growthMax(identifier, key == "age");
        std::string value = std::to_string(number);
        std::optional<float> progress;
        if (max && number <= *max) {
            value += " / " + std::to_string(*max);
            progress = *max > 0 ? static_cast<float>(number) / *max : 0.f;
        }
        return DetailRow{key == "age" ? "target.age" : "target.growth", std::move(value), false, progress};
    }
    if (key == "redstone_signal" && kind == StateKind::Integer && number >= 0 && number <= 15)
        return DetailRow{"target.power", std::to_string(number), false, static_cast<float>(number) / 15};
    if (key == "facing_direction" && kind == StateKind::Integer) {
        // Bedrock facing indices match Java's (down, up, north, south, west,
        // east); confirm against a door or stair in game.
        constexpr std::string_view directions[] = {"target.dirDown", "target.dirUp", "target.dirNorth",
                                                   "target.dirSouth", "target.dirWest", "target.dirEast"};
        if (number >= 0 && number < 6)
            return DetailRow{"target.facing", std::string(directions[number]), true, {}};
        return {};
    }
    if (key == "minecraft:cardinal_direction" && kind == StateKind::Text && !text.empty()) {
        std::string direction(text);
        direction[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(direction[0])));
        return DetailRow{"target.facing", std::move(direction), false, {}};
    }
    if (key == "open_bit" && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{"target.open", number ? "target.yes" : "target.no", true, {}};
    if (key == "upper_block_bit" && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{"target.half", number ? "target.upper" : "target.lower", true, {}};
    if (key == "door_hinge_bit" && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{"target.hinge", number ? "target.hingeRight" : "target.hingeLeft", true, {}};
    return {};
}
std::optional<TargetInfo> collectTargetInfo(IClientInstance&, bool includeStates = false);
}
