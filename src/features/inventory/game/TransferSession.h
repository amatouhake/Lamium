#pragma once

#include "features/inventory/transfer/TransferGesture.h"
#include <string>

class ContainerScreenController;
class ItemStack;

namespace lamium::inventory::game {
// Input callbacks only exchange owned values with this session. Container
// state and vanilla transfers are accessed from the UI/client thread in tick.
class TransferSession {
public:
    // `drop`: the game's drop key is held (dropKey).
    static bool mouseButton(int button, bool down, bool shift, bool control, bool alt, bool drop, bool cancelled);
    static bool dropKey(int key);
    // A key press: Ctrl + Shift + the drop key over an item drops its kind; true takes it from vanilla.
    static bool dropKeyPress(int key, bool repeat, bool shift, bool control, bool alt, bool cancelled);
    // Vanilla dropped the cursor's items (a click outside the window).
    static void cursorDropped(ContainerScreenController& controller, ItemStack const& held) noexcept;
    static bool wheel(int direction, bool shift, bool control, bool cancelled);
    static void modifierReleased();
    static void cancel();
    static void slotHovered(ContainerScreenController& controller, std::string const& collection, int index);
    static void slotUnhovered(ContainerScreenController& controller, std::string const& collection, int index);
    static void tick(ContainerScreenController& controller);
};
}
