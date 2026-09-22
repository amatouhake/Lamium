#pragma once
#include <array>
#include <string_view>
namespace lamium::interaction {
enum class RestrictionMode { Plane, Line, Column, Layer };
inline constexpr std::array<std::string_view,4> restrictionNames{"plane","line","column","layer"};
inline constexpr std::array<std::string_view,4> restrictionLabels{"mode.plane","mode.line","mode.column","mode.layer"};
}
