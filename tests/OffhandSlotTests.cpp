#include "features/information/OffhandSlot.h"
#include "settings/SettingsStore.h"
#include <limits>
void check(bool, char const*);
void offhandSlotTests() {
    using namespace lamium::information::offhand;
    // Classic hotbar: 182x22 centered at the bottom of a 480x270 GUI screen.
    auto slot = slotBox({149, 248, 182, 22}, 480, 270);
    check(slot && slot->x == 149 - 28 && slot->y == 248 && slot->w == 22 && slot->h == 22,
          "the slot sits left of the hotbar with a six-unit gap, on its line");
    auto icon = iconBox(*slot);
    check(icon.x == slot->x + 3 && icon.y == 251 && icon.w == 16, "the icon sits where the hotbar puts one");
    auto tall = slotBox({200, 200, 300, 44}, 800, 300);
    check(tall && tall->w == 44 && tall->x == 200 - 56, "a taller hotbar scales the slot and the gap");
    check(!slotBox({10, 248, 182, 22}, 480, 270), "no slot when it would leave the screen");
    check(!slotBox({149, 248, 0, 22}, 480, 270) && !slotBox({149, 260, 182, 22}, 480, 270)
          && !slotBox({std::numeric_limits<float>::quiet_NaN(), 248, 182, 22}, 480, 270),
          "an unusable hotbar box draws nothing");
    check(shown(true, true, false) && !shown(true, false, false) && shown(true, false, true)
          && !shown(false, true, true), "hidden while empty unless the empty frame is kept");

    lamium::Settings defaults;
    check(!defaults.information.offhandSlot && !defaults.information.offhandSlotEmpty, "the offhand slot starts off");
    auto loaded = lamium::decodeSettings(R"({"information":{"offhandSlot":true,"offhandSlotEmpty":true}})");
    check(loaded.information.offhandSlot && loaded.information.offhandSlotEmpty, "offhand slot options load");
}
