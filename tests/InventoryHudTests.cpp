#include "features/information/InventoryHud.h"
void check(bool, char const*);
void inventoryHudTests() {
    using namespace lamium::information::inventoryHud;
    auto main = gridRows(false);
    check(main.size() == 3 && main[0].front() == 9 && main[0].back() == 17 && main[2].back() == 35,
          "the main rows follow the inventory screen: 9-17, 18-26, 27-35");
    auto all = gridRows(true);
    check(all.size() == 4 && all[3].front() == 0 && all[3].back() == 8, "the hotbar is the fourth row");
    std::array<bool, slotCount> occupied{};
    for (int slot : {0, 1, 9, 20, 35}) occupied[slot] = true;
    check(freeSlots(occupied, false) == 24, "the main range counts 27 slots");
    check(freeSlots(occupied, true) == 31, "with the hotbar, 36 slots");
    occupied.fill(true);
    check(freeSlots(occupied, true) == 0 && freeSlots(occupied, false) == 0, "a full inventory has nothing free");
    check(rowTop(2, 18) == 36 && rowTop(3, 18) == 58 && gridHeight(false, 18) == 54 && gridHeight(true, 18) == 76,
          "the hotbar row sits below a gap");
    check(rowTop(3, 9) == 29, "the gap scales with the slot");
}
