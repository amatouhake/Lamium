#pragma once
#include <array>
#include <string_view>
namespace lamium::overlay {
// Which stored light the overlay's numbers show (BACKLOG L-16).
enum class LightValue { Block, Sky, Both };
inline constexpr std::array<std::string_view,3> lightValueNames{"block","sky","both"};
inline constexpr std::array<std::string_view,3> lightValueLabels{"lightValue.block","lightValue.sky","lightValue.both"};
}
