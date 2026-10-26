#pragma once
#include <array>
#include <string>
#include <string_view>

namespace lamium::inventory {
// L-263: where a picked item comes from, like the game's pick block. A
// hotbar slot holding it is selected (the selected one first); otherwise the
// first main-inventory slot holding it is swapped into the selected slot.
// Nothing when it is not carried. `names` are the 36 slots' item names.
struct HandPick {
    enum Kind { None, Select, Swap } kind = None;
    int slot = -1;
};
inline HandPick pickFor(std::array<std::string, 36> const& names, int selected, std::string_view item) {
    if (item.empty() || selected < 0 || selected >= 9) return {};
    if (names[static_cast<size_t>(selected)] == item) return {HandPick::Select, selected};
    for (int slot = 0; slot < 9; ++slot)
        if (names[static_cast<size_t>(slot)] == item) return {HandPick::Select, slot};
    for (int slot = 9; slot < 36; ++slot)
        if (names[static_cast<size_t>(slot)] == item) return {HandPick::Swap, slot};
    return {};
}
}
