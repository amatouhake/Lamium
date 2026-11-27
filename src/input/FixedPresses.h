#pragma once
#include "input/Binding.h"
#include <algorithm>
#include <array>
#include <span>
#include <string_view>

// Presses that are not bindings (L-372): Lamium's own parts answer them
// (the recipe lists and windows) or they are a feature's fixed gestures
// (Item transfer). Hotkeys lists them as read-only rows under their feature
// so a key's every use can be found; their keys cannot be changed.
namespace lamium::input {
// One cap: a token Find by key matches, named as a binding names it. `text`
// (a translation key) names it instead where no binding has the input: the
// wheel in either direction, a drag. `drop` and `inventory` are the game's
// own drop and inventory keys, named and matched by the keys the game has for
// them, `text` where those are unknown.
struct FixedCap {
    Token token{Device::Key, 0};
    std::string_view text;
    bool drop = false, inventory = false;
    bool matches() const { return token.code != 0; }
    bool game() const { return drop || inventory; }
};
enum class FixedTarget { None, LamiumList, GameSlot };
struct FixedPress {
    std::string_view feature, name; // name: empty where its switch's label names it
    std::string_view option; // its own switch in the settings, if any
    unsigned where;
    FixedTarget target;
    std::array<FixedCap, 3> caps;
};
namespace fixed_caps {
inline constexpr FixedCap shift{{Device::Key, 0x10}}, ctrl{{Device::Key, 0x11}}, alt{{Device::Key, 0x12}}, esc{{Device::Key, 0x1B}};
inline constexpr FixedCap wheel{{Device::Wheel, 1}, "fixed.cap.wheel"};
inline constexpr FixedCap leftClick{{Device::Mouse, 1}}, rightClick{{Device::Mouse, 2}};
inline constexpr FixedCap click = leftClick;
inline constexpr FixedCap leftDrag{{Device::Mouse, 1}, "mouse.leftDrag"}, rightDrag{{Device::Mouse, 2}, "mouse.rightDrag"};
inline constexpr FixedCap dropKey{{Device::Key, 0}, "fixed.cap.dropKey", true};
inline constexpr FixedCap inventoryKey{{Device::Key, 0}, "fixed.cap.inventoryKey", false, true};
}
inline constexpr auto fixedPresses = [] {
    using namespace fixed_caps;
    constexpr unsigned screen = layer::itemScreen, both = layer::itemScreen | layer::window;
    constexpr auto slot = FixedTarget::GameSlot, list = FixedTarget::LamiumList;
    // Item transfer's gestures by input (wheel, clicks, drags, the drop key),
    // moves before drops within one, the settings rows' order; caps read Ctrl,
    // Shift, Alt, the drop key, then mouse (maintainer 2026-11-24).
    return std::to_array<FixedPress>({
        {"transfer", "", "inventory.transferWheelOne", screen, slot, {wheel}},
        {"transfer", "", "inventory.transferWheelStack", screen, slot, {shift, wheel}},
        {"transfer", "", "inventory.transferWheelMatching", screen, slot, {ctrl, wheel}},
        {"transfer", "", "inventory.transferWheelAll", screen, slot, {ctrl, shift, wheel}},
        {"transfer", "", "inventory.transferClickMatching", screen, slot, {alt, leftClick}},
        {"transfer", "", "inventory.transferClickAll", screen, slot, {shift, alt, leftClick}},
        {"transfer", "", "inventory.transferCursorMatching", screen, slot, {shift, click}},
        {"transfer", "", "inventory.transferDropMatching", screen, slot, {shift, click}},
        {"transfer", "", "inventory.transferDragStack", screen, slot, {shift, leftDrag}},
        {"transfer", "", "inventory.transferDragOne", screen, slot, {ctrl, leftDrag}},
        {"transfer", "", "inventory.transferDragKeepOne", screen, slot, {shift, rightDrag}},
        {"transfer", "", "inventory.transferDropAll", screen, slot, {shift, dropKey, leftDrag}},
        {"transfer", "", "inventory.transferDropOne", screen, slot, {ctrl, dropKey, leftDrag}},
        {"transfer", "", "inventory.transferDropKeepOne", screen, slot, {shift, dropKey, rightDrag}},
        {"transfer", "", "inventory.transferDropHovered", screen, slot, {ctrl, shift, dropKey}},
        {"recipes", "fixed.recipesClick", "", both, list, {leftClick}},
        {"recipes", "fixed.recipesRightClick", "", both, list, {rightClick}},
        {"recipes", "fixed.recipesShiftClick", "", both, list, {shift, leftClick}},
        {"recipes", "fixed.recipesCtrlClick", "", both, list, {ctrl, leftClick}},
        {"recipes", "fixed.recipesWheel", "", both, list, {wheel}},
        {"recipes", "fixed.recipesClose", "", layer::window, FixedTarget::None, {esc}},
        {"recipes", "fixed.recipesCloseInventory", "", layer::window, FixedTarget::None, {inventoryKey}},
    });
}();
// The caps a press shows, in order; empty slots end the list.
inline int fixedCapCount(FixedPress const& press) {
    int count = 0;
    while (count < static_cast<int>(press.caps.size()) && (press.caps[count].matches() || !press.caps[count].text.empty())) ++count;
    return count;
}
// The game's drop and inventory keys, as its keymap has them.
struct FixedGameKeys { std::span<int const> drop, inventory; };
// Find by key: every input pressed is one of the press's (a wheel in either
// direction; the drop and inventory keys by the keys the game has for them).
inline bool fixedUses(FixedPress const& press, Chord const& pressed, FixedGameKeys game = {}) {
    if (pressed.empty()) return false;
    for (auto token : pressed) {
        bool found = false;
        for (int i = 0; i < fixedCapCount(press); ++i) {
            auto const& cap = press.caps[i];
            if (cap.game()) {
                auto const codes = cap.drop ? game.drop : game.inventory;
                found = found || (token.device == Device::Key && std::ranges::find(codes, token.code) != codes.end());
                continue;
            }
            if (!cap.matches()) continue;
            found = found || cap.token == token || (cap.token.device == Device::Wheel && token.device == Device::Wheel);
        }
        if (!found) return false;
    }
    return true;
}
}

namespace lamium::input {
// The fixed press a settings switch turns on and off, if any.
inline FixedPress const* fixedPressOf(std::string_view option) {
    if (option.empty()) return nullptr;
    for (auto const& press : fixedPresses)
        if (press.option == option) return &press;
    return nullptr;
}
}
