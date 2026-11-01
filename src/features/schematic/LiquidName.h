#pragma once
#include <string_view>

namespace lamium::schematic {
// The vanilla water and lava blocks, still and flowing, by block name.
inline bool isLiquidName(std::string_view name) {
    return name == "minecraft:water" || name == "minecraft:flowing_water" || name == "minecraft:lava"
        || name == "minecraft:flowing_lava";
}
inline bool isWaterName(std::string_view name) {
    return name == "minecraft:water" || name == "minecraft:flowing_water";
}
// The color water is drawn in where the game does not tint it by biome: the
// missing-water ghosts and Lamium's water icons (L-294). Alpha is the ghosts'.
struct LiquidColor { float r, g, b, a; };
inline constexpr LiquidColor waterColor{.25f, .45f, .95f, .55f};
}
