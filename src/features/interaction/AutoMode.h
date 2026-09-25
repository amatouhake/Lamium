#pragma once
#include <array>
#include <string_view>
namespace lamium::interaction {
// How Auto Attack / Auto Use clicks while switched on (DESIGN "Automatic
// attack and use"). Saved by name; the on/off switch itself is never saved.
enum class AutoMode { Periodic, Hold, Fast };
inline constexpr std::array<std::string_view,3> autoModeNames{"periodic","hold","fast"};
inline constexpr std::array<std::string_view,3> autoModeLabels{"autoMode.periodic","autoMode.hold","autoMode.fast"};
// When Fast click clicks. Only Fast click has this choice: Periodic and Hold
// always act while switched on, and Hold "while held" would be vanilla.
enum class FastTrigger { Always, WhileHeld };
inline constexpr std::array<std::string_view,2> fastTriggerNames{"always","held"};
inline constexpr std::array<std::string_view,2> fastTriggerLabels{"fastTrigger.always","fastTrigger.held"};
}
