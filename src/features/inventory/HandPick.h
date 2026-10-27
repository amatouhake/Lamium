#pragma once
#include <array>
#include <string>
#include <string_view>

namespace lamium::inventory {
// L-263: where a picked item comes from, like the game's pick block. A
// hotbar slot holding it is selected (the selected one first); otherwise the
// first main-inventory slot holding it is swapped into the selected slot.
// Nothing when it is not carried.
struct HandPick {
    enum Kind { None, Select, Swap } kind = None;
    int slot = -1;
};
// One of the 36 slots as the pick decisions see it. The data value is part
// of the identity: skull kinds and monster eggs share a name, so a name-only
// match would pick or swap a different variant. Empty names are empty slots.
struct HandSlot {
    std::string name;
    short aux = 0;
    bool empty() const { return name.empty(); }
    bool holds(std::string_view item, short data) const {
        return !name.empty() && std::string_view(name) == item && aux == data;
    }
};
using HandSlots = std::array<HandSlot, 36>;
inline HandPick pickFor(HandSlots const& slots, int selected, std::string_view item, short aux) {
    if (item.empty() || selected < 0 || selected >= 9) return {};
    if (slots[static_cast<size_t>(selected)].holds(item, aux)) return {HandPick::Select, selected};
    for (int slot = 0; slot < 9; ++slot)
        if (slots[static_cast<size_t>(slot)].holds(item, aux)) return {HandPick::Select, slot};
    for (int slot = 9; slot < 36; ++slot)
        if (slots[static_cast<size_t>(slot)].holds(item, aux)) return {HandPick::Swap, slot};
    return {};
}
// L-263, creative mode: the hotbar slot a picked item that is not carried
// goes to, like the game's creative pick: the selected slot when empty,
// else the first empty hotbar slot, else the selected slot (its item is
// replaced). -1 when no hotbar slot is selected. Empty names are empty slots.
inline int creativeSlotFor(HandSlots const& slots, int selected) {
    if (selected < 0 || selected >= 9) return -1;
    if (slots[static_cast<size_t>(selected)].empty()) return selected;
    for (int slot = 0; slot < 9; ++slot)
        if (slots[static_cast<size_t>(slot)].empty()) return slot;
    return selected;
}
}
