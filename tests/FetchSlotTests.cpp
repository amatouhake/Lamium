#include "features/inventory/FetchSlot.h"
#include "features/inventory/HandPick.h"
#include "ui/SettingsRows.h"
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
    using lamium::inventory::pickFor;
    std::array<std::string, 36> names{};
    names[2] = names[5] = "minecraft:stone";
    names[20] = "minecraft:glass";
    check(pickFor(names, 5, "minecraft:stone").kind == HandPick::Select && pickFor(names, 5, "minecraft:stone").slot == 5,
          "the selected slot wins when it holds the block");
    check(pickFor(names, 0, "minecraft:stone").slot == 2, "otherwise the first hotbar slot holding it is selected");
    auto swap = pickFor(names, 0, "minecraft:glass");
    check(swap.kind == HandPick::Swap && swap.slot == 20, "from the inventory it is swapped into the hand");
    check(pickFor(names, 0, "minecraft:dirt").kind == HandPick::None && pickFor(names, 0, "").kind == HandPick::None
              && pickFor(names, 9, "minecraft:stone").kind == HandPick::None,
          "nothing when it is not carried or no hotbar slot is selected");
}
