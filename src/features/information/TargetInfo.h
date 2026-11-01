#pragma once
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
class IClientInstance;
namespace lamium::information {
// Schematic: the selected placement's block here when it differs (L-93),
// shown whether or not other details are.
enum class DetailKind { Other, Health, Armor, Growth, Schematic };
// Schematic rows take the verifier's colors: red for a wrong or extra block,
// yellow for a wrong state, light blue for a missing one.
enum class Tone { Normal, Wrong, State, Missing };
// What the card draws beside the name. Item is a real item stack (block items
// and spawn eggs); Texture is the target's own texture for blocks that have no
// item, with the region of the source image to draw (one animation frame).
enum class IconKind { None, Item, Texture };
struct TargetIcon {
    IconKind kind = IconKind::None;
    std::string name; // Item: item identifier. Texture: path of the source image.
    short aux = 0;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    bool tinted = false; // Texture drawn in the water color (L-294).
};
// Owned snapshot shared by target HUD and future detailed debug providers.
struct TargetInfo {
    std::string name, identifier;
    TargetIcon icon;
    std::vector<std::string> states;
    struct DetailRow {
        std::string label; // Translation key, resolved by the caller.
        std::string value; // Display text, or a translation key when valueIsKey.
        bool valueIsKey = false;
        std::optional<float> progress; // 0-1 for ranged values; empty otherwise.
        DetailKind kind = DetailKind::Other;
        int current = 0, maximum = 0; // Health points, for hearts in absolute units.
        std::string icon;             // An item icon before the value (binary NBT), if any.
        bool labelIsKey = true;
        Tone tone = Tone::Normal;
        std::string before; // A change: drawn before an arrow, then `value`.
    };
    std::vector<DetailRow> details;
    struct BlockPosition { int x, y, z; };
    std::optional<BlockPosition> blockPosition = std::nullopt;
    bool ghost = false; // A schematic ghost, not a real block: the name takes the ghost's color.
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
// `nether`: lava there flows seven blocks one depth step at a time; elsewhere
// three, two depth steps at a time (0, 2, 4, 6).
inline std::optional<TargetInfo::DetailRow> interpretBlockState(std::string_view key, StateKind kind,
        long long number, std::string_view text, std::string_view identifier, bool nether = false) {
    using DetailRow = TargetInfo::DetailRow;
    if ((key == "growth" || key == "age") && kind == StateKind::Integer && number >= 0) {
        auto max = growthMax(identifier, key == "age");
        std::string value = std::to_string(number);
        std::optional<float> progress;
        if (max && number <= *max) {
            value += " / " + std::to_string(*max);
            progress = *max > 0 ? static_cast<float>(number) / *max : 0.f;
        }
        return DetailRow{key == "age" ? "target.age" : "target.growth", std::move(value), false, progress,
                         DetailKind::Growth};
    }
    // Water and lava (L-264): 0 a source, 1-7 steps away from one, 8 and up falling.
    if (key == "liquid_depth" && kind == StateKind::Integer && number >= 0) {
        if (number == 0) return DetailRow{"target.flow", "target.flowSource", true, {}};
        if (number >= 8) return DetailRow{"target.flow", "target.flowFalling", true, {}};
        bool slowLava = identifier.find("lava") != std::string_view::npos && !nether;
        long long most = slowLava ? 3 : 7;
        // A depth past the liquid's own spread keeps the last step: lava reads
        // 7 in the Overworld too, never "4 / 3" (L-277).
        long long steps = std::min(slowLava ? (number + 1) / 2 : number, most);
        return DetailRow{"target.flowSteps", std::to_string(steps) + " / " + std::to_string(most), false,
                         1.f - static_cast<float>(steps) / (most + 1)};
    }
    // A candle's or copper bulb's state, and the on/off forms of L-262
    // under their Java Edition state names.
    if ((key == "lit" || key == "powered") && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{key == "lit" ? "target.lit" : "target.powered", number ? "target.yes" : "target.no", true, {}};
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
    // L-112: named directions as text.
    if ((key == "minecraft:cardinal_direction" || key == "minecraft:facing_direction" || key == "minecraft:block_face")
        && kind == StateKind::Text && !text.empty()) {
        for (auto [name, label] : {std::pair{"down", "target.dirDown"}, {"up", "target.dirUp"}, {"north", "target.dirNorth"},
                                   {"south", "target.dirSouth"}, {"west", "target.dirWest"}, {"east", "target.dirEast"}})
            if (text == name) return DetailRow{"target.facing", label, true, {}};
        return {};
    }
    // Stairs, and trapdoors' "direction": 0 east, 1 west, 2 south, 3 north.
    if ((key == "weirdo_direction" || (key == "direction" && identifier.find("trapdoor") != std::string_view::npos))
        && kind == StateKind::Integer) {
        constexpr std::string_view directions[] = {"target.dirEast", "target.dirWest", "target.dirSouth", "target.dirNorth"};
        if (number >= 0 && number < 4) return DetailRow{"target.facing", std::string(directions[number]), true, {}};
        return {};
    }
    if (key == "upside_down_bit" && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{"target.upsideDown", number ? "target.yes" : "target.no", true, {}};
    if (key == "top_slot_bit" && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{"target.half", number ? "target.upper" : "target.lower", true, {}};
    if (key == "minecraft:vertical_half" && kind == StateKind::Text && (text == "top" || text == "bottom"))
        return DetailRow{"target.half", text == "top" ? "target.upper" : "target.lower", true, {}};
    if (key == "pillar_axis" && kind == StateKind::Text && (text == "x" || text == "y" || text == "z"))
        return DetailRow{"target.axis", text == "x" ? "X" : text == "y" ? "Y" : "Z", false, {}};
    if (key == "open_bit" && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{"target.open", number ? "target.yes" : "target.no", true, {}};
    if (key == "upper_block_bit" && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{"target.half", number ? "target.upper" : "target.lower", true, {}};
    if (key == "door_hinge_bit" && kind == StateKind::Integer && (number == 0 || number == 1))
        return DetailRow{"target.hinge", number ? "target.hingeRight" : "target.hingeLeft", true, {}};
    return {};
}
// The ray the card looks along. From the body (not detached), the game's own
// hit wins within the reach: it is the block the outline shows, and a ray
// from the eye position can differ from it at a block's edge (L-217).
struct ViewRay { double x, y, z, dx, dy, dz; double reach; bool detached = false; };
// `includeLiquids`: real water and lava the ray meets are the target (L-264).
std::optional<TargetInfo> collectTargetInfo(IClientInstance&, bool includeStates = false,
                                            std::optional<ViewRay> ray = std::nullopt, bool includeSchematic = true,
                                            bool includeLiquids = false);
// Water's own texture as a tinted icon, for screens that would otherwise draw
// the water item's untinted gray (L-294). Empty when it cannot be read.
TargetIcon waterIcon();
// Drops per-world icon lookups on world exit.
void startTargetIcons();
void stopTargetIcons();
}
