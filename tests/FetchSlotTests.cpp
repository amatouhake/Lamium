#include "features/inventory/FetchSlot.h"
#include "features/inventory/HandPick.h"
#include "ui/SettingsRows.h"
#include <cstddef>
void check(bool, char const*);
void fetchSlotTests() {
    using lamium::inventory::fetchSlot;
    check(fetchSlot(3, 0, std::nullopt) == 3, "by default a fetch goes to the selected slot");
    check(fetchSlot(3, 1, std::nullopt) == 0 && fetchSlot(3, 9, std::nullopt) == 8, "a fixed slot 1-9 is hotbar index 0-8");
    check(fetchSlot(3, 4, std::nullopt) == 3, "a fixed slot that is selected stays selected");
    check(fetchSlot(3, 9, 8) == 3, "Fake Offhand's slot is never a fetch destination; the selected slot is used");
    check(fetchSlot(3, 9, 7) == 8, "another Fake Offhand slot does not matter");
    check(fetchSlot(3, 12, std::nullopt) == 3 && fetchSlot(3, -1, std::nullopt) == 3, "an invalid setting means the selected slot");
    // The settings screen warns about a fetch slot that is Fake Offhand's slot.
    lamium::Settings value;
    value.inventory.toolSwitch = value.inventory.toolSwitchInventory = true;
    value.inventory.fakeOffhand = true;
    value.inventory.fakeOffhandSlot = 9;
    value.inventory.toolSwitchSlot = 9;
    using lamium::ui::optionWarning;
    check(optionWarning("inventory.toolSwitchSlot", value) && optionWarning("inventory.fakeOffhandSlot", value)
        && !optionWarning("inventory.weaponSwitchSlot", value), "both colliding rows warn, the other does not");
    value.inventory.fakeOffhand = false;
    check(!optionWarning("inventory.toolSwitchSlot", value), "no warning while Fake Offhand is off");
    value.inventory.fakeOffhand = true;
    value.inventory.toolSwitchInventory = false;
    check(!optionWarning("inventory.toolSwitchSlot", value), "no warning while the fetch is off");
    // L-263: where a ghost's block is picked from.
    using lamium::inventory::HandPick;
    using lamium::inventory::HandSlots;
    using lamium::inventory::pickFor;
    HandSlots slots{};
    auto hold = [&slots](int slot, char const* name, short aux = 0) {
        slots[static_cast<size_t>(slot)].name = name;
        slots[static_cast<size_t>(slot)].aux = aux;
    };
    hold(2, "minecraft:stone");
    hold(5, "minecraft:stone");
    hold(20, "minecraft:glass");
    check(pickFor(slots, 5, "minecraft:stone", 0).kind == HandPick::Select && pickFor(slots, 5, "minecraft:stone", 0).slot == 5,
          "the selected slot wins when it holds the block");
    check(pickFor(slots, 0, "minecraft:stone", 0).slot == 2, "otherwise the first hotbar slot holding it is selected");
    auto swap = pickFor(slots, 0, "minecraft:glass", 0);
    check(swap.kind == HandPick::Swap && swap.slot == 20, "from the inventory it is swapped into the hand");
    check(pickFor(slots, 0, "minecraft:dirt", 0).kind == HandPick::None && pickFor(slots, 0, "", 0).kind == HandPick::None
              && pickFor(slots, 9, "minecraft:stone", 0).kind == HandPick::None,
          "nothing when it is not carried or no hotbar slot is selected");
    // L-273: a variant shares the item name, so the data value has to match
    // as well or the pick selects or swaps a different variant.
    HandSlots variants{};
    variants[1] = {"minecraft:skull", 0};        // skeleton
    variants[4] = {"minecraft:monster_egg", 0};  // stone
    variants[13] = {"minecraft:skull", 1};       // wither skeleton
    struct AuxCase {
        int selected;
        char const* item;
        short aux;
        HandPick::Kind kind;
        int slot;
        char const* why;
    };
    AuxCase const auxCases[] = {
        {0, "minecraft:skull", 1, HandPick::Swap, 13, "another variant is swapped, never the one in the hotbar"},
        {1, "minecraft:skull", 0, HandPick::Select, 1, "the variant already in the selected slot is kept"},
        {0, "minecraft:skull", 2, HandPick::None, -1, "a variant that is not carried is nothing"},
        {0, "minecraft:monster_egg", 1, HandPick::None, -1, "a name carried only in another variant is nothing"},
        {0, "minecraft:monster_egg", 0, HandPick::Select, 4, "the carried variant is selected"},
    };
    for (auto const& item : auxCases) {
        auto pick = pickFor(variants, item.selected, item.item, item.aux);
        check(pick.kind == item.kind && pick.slot == item.slot, item.why);
    }
    using lamium::inventory::creativeSlotFor;
    check(creativeSlotFor(slots, 4) == 4, "creative: an empty selected slot takes the block");
    check(creativeSlotFor(slots, 2) == 0, "creative: else the first empty hotbar slot");
    HandSlots full{};
    for (int slot = 0; slot < 9; ++slot) full[static_cast<size_t>(slot)].name = "minecraft:dirt";
    check(creativeSlotFor(full, 3) == 3 && creativeSlotFor(full, 9) == -1,
          "creative: a full hotbar replaces the selected slot; nothing without a selected hotbar slot");
}
