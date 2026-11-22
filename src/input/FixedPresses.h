#pragma once
#include "input/Binding.h"
#include <array>
#include <string_view>

// Presses that are not bindings (L-372): Lamium's own parts answer them
// (the recipe lists and windows) or they are a feature's fixed gestures
// (Item transfer). Hotkeys lists them as read-only rows under their feature
// so a key's every use can be found; their keys cannot be changed.
namespace lamium::input {
// One cap: a token Find by key matches, and the translation key drawn for it
// when the token's own name would not say it (a drag, the drop key).
struct FixedCap {
    Token token{Device::Key, 0};
    std::string_view text;
    bool matches() const { return token.code != 0; }
};
enum class FixedTarget { None, LamiumList, GameSlot };
struct FixedPress {
    std::string_view feature, name;
    std::string_view option; // its own switch in the settings, if any
    unsigned where;
    FixedTarget target;
    std::array<FixedCap, 3> caps;
};
namespace fixed_caps {
inline constexpr FixedCap shift{{Device::Key, 0x10}}, ctrl{{Device::Key, 0x11}}, alt{{Device::Key, 0x12}}, esc{{Device::Key, 0x1B}};
inline constexpr FixedCap wheel{{Device::Wheel, 1}, "fixed.cap.wheel"};
inline constexpr FixedCap leftClick{{Device::Mouse, 1}, "fixed.cap.leftClick"}, rightClick{{Device::Mouse, 2}, "fixed.cap.rightClick"};
inline constexpr FixedCap click{{Device::Mouse, 1}, "fixed.cap.click"};
inline constexpr FixedCap leftDrag{{Device::Mouse, 1}, "fixed.cap.leftDrag"}, rightDrag{{Device::Mouse, 2}, "fixed.cap.rightDrag"};
inline constexpr FixedCap dropKey{{Device::Key, 0}, "fixed.cap.dropKey"}; // the game's own binding, not known here
}
inline constexpr auto fixedPresses = [] {
    using namespace fixed_caps;
    constexpr unsigned screen = layer::itemScreen, both = layer::itemScreen | layer::window;
    constexpr auto slot = FixedTarget::GameSlot, list = FixedTarget::LamiumList;
    return std::to_array<FixedPress>({
        {"transfer", "fixed.transferWheelOne", "inventory.transferWheelOne", screen, slot, {wheel}},
        {"transfer", "fixed.transferWheelStack", "inventory.transferWheelStack", screen, slot, {shift, wheel}},
        {"transfer", "fixed.transferDragStack", "inventory.transferDragStack", screen, slot, {shift, leftDrag}},
        {"transfer", "fixed.transferDragOne", "inventory.transferDragOne", screen, slot, {ctrl, leftDrag}},
        {"transfer", "fixed.transferWheelMatching", "inventory.transferWheelMatching", screen, slot, {ctrl, wheel}},
        {"transfer", "fixed.transferWheelAll", "inventory.transferWheelAll", screen, slot, {ctrl, shift, wheel}},
        {"transfer", "fixed.transferClickMatching", "inventory.transferClickMatching", screen, slot, {alt, leftClick}},
        {"transfer", "fixed.transferClickAll", "inventory.transferClickAll", screen, slot, {alt, shift, leftClick}},
        {"transfer", "fixed.transferDragKeepOne", "inventory.transferDragKeepOne", screen, slot, {shift, rightDrag}},
        {"transfer", "fixed.transferCursorMatching", "inventory.transferCursorMatching", screen, slot, {shift, click}},
        {"transfer", "fixed.transferDropOne", "inventory.transferDropOne", screen, slot, {dropKey, ctrl, leftDrag}},
        {"transfer", "fixed.transferDropKeepOne", "inventory.transferDropKeepOne", screen, slot, {dropKey, shift, rightDrag}},
        {"transfer", "fixed.transferDropAll", "inventory.transferDropAll", screen, slot, {dropKey, shift, leftDrag}},
        {"transfer", "fixed.transferDropMatching", "inventory.transferDropMatching", screen, slot, {shift, click}},
        {"recipes", "fixed.recipesClick", "", both, list, {leftClick}},
        {"recipes", "fixed.recipesRightClick", "", both, list, {rightClick}},
        {"recipes", "fixed.recipesShiftClick", "", both, list, {shift, leftClick}},
        {"recipes", "fixed.recipesCtrlClick", "", both, list, {ctrl, leftClick}},
        {"recipes", "fixed.recipesWheel", "", both, list, {wheel}},
        {"recipes", "fixed.recipesClose", "", layer::window, FixedTarget::None, {esc}},
    });
}();
// The caps a press shows, in order; empty slots end the list.
inline int fixedCapCount(FixedPress const& press) {
    int count = 0;
    while (count < static_cast<int>(press.caps.size()) && (press.caps[count].matches() || !press.caps[count].text.empty())) ++count;
    return count;
}
// Find by key: every input pressed is one of the press's (a wheel in either direction).
inline bool fixedUses(FixedPress const& press, Chord const& pressed) {
    if (pressed.empty()) return false;
    for (auto token : pressed) {
        bool found = false;
        for (int i = 0; i < fixedCapCount(press); ++i) {
            auto const& cap = press.caps[i];
            if (!cap.matches()) continue;
            found = found || cap.token == token || (cap.token.device == Device::Wheel && token.device == Device::Wheel);
        }
        if (!found) return false;
    }
    return true;
}
}
