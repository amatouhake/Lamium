#pragma once
#include <string_view>

namespace lamium::schematic {
// The vanilla water and lava blocks, still and flowing, by block name.
inline bool isLiquidName(std::string_view name) {
    return name == "minecraft:water" || name == "minecraft:flowing_water" || name == "minecraft:lava"
        || name == "minecraft:flowing_lava";
}
}
