#pragma once
#include <array>
#include <string_view>
namespace lamium::overlay {
// Which stored light the overlay's numbers show (BACKLOG L-16).
enum class LightValue { Block, Sky, Both };
inline constexpr std::array<std::string_view,3> lightValueNames{"block","sky","both"};
inline constexpr std::array<std::string_view,3> lightValueLabels{"lightValue.block","lightValue.sky","lightValue.both"};
// Which way the numbers read: turned toward the view, or fixed so turning the
// camera never rebuilds them (cheaper with large ranges).
enum class LightFacing { View, North, East, South, West };
inline constexpr std::array<std::string_view,5> lightFacingNames{"view","north","east","south","west"};
inline constexpr std::array<std::string_view,5> lightFacingLabels{"lightFacing.view","lightFacing.north","lightFacing.east","lightFacing.south","lightFacing.west"};
}
