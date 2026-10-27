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
// L-263, creative mode: the hotbar slot a picked item that is not carried
// goes to, like the game's creative pick: the selected slot when empty,
// else the first empty hotbar slot, else the selected slot (its item is
// replaced). -1 when no hotbar slot is selected. Empty names are empty slots.
inline int creativeSlotFor(std::array<std::string, 36> const& names, int selected) {
    if (selected < 0 || selected >= 9) return -1;
    if (names[static_cast<size_t>(selected)].empty()) return selected;
    for (int slot = 0; slot < 9; ++slot)
        if (names[static_cast<size_t>(slot)].empty()) return slot;
    return selected;
}
}
