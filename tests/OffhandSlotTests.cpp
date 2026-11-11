#include "features/information/OffhandSlot.h"
#include "features/information/PaperDoll.h"
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
    auto drawn = editorHotbar(Box{149, 248, 182, 22}, 480, 270, 480, 270);
    check(drawn.x == 149 && drawn.y == 248, "the editor shows the hotbar where the game laid it out");
    auto fallback = editorHotbar(Box{200, 200, 300, 44}, 800, 300, 480, 270);
    check(fallback.x == 149 && fallback.y == 248 && fallback.w == 182,
          "on another screen size, or before any was seen, the desktop place");
    check(editorHotbar(std::nullopt, 0, 0, 480, 270).y == 248, "nothing seen yet: the desktop place");
    {
        namespace doll = lamium::information::paperDoll;
        doll::Box control{15, 15, 15, 15}; // the desktop paper doll's control
        auto area = doll::area(control);
        check(area && area->x == 0 && area->y == 0 && area->w == 45 && area->h == 50,
              "the paper doll's area reaches past its control as far as the chat's room for it");
        check(doll::covered(control, doll::Box{2, 2, 120, 60}), "the Info HUD at its default top-left place covers the doll");
        check(!doll::covered(control, doll::Box{300, 2, 120, 60}) && !doll::covered(control, doll::Box{2, 60, 120, 60}),
              "placed away from it, the doll stays");
        check(!doll::covered(std::nullopt, doll::Box{2, 2, 120, 60}) && !doll::covered(control, std::nullopt),
              "no doll control or no Info HUD: nothing hidden");
        doll::Box line{2, 66, 110, 10}; // the position line under the doll's room
        check(doll::meets(line, doll::Box{2, 2, 120, 70}) && !doll::meets(line, doll::Box{2, 2, 120, 60}),
              "the position line is hidden only where the Info HUD meets it");
        // L-309: the doll placed by the HUD layout.
        auto center = doll::controlCenter(0, 0);
        check(center.x == 22.5f && center.y == 22.5f, "the default element box centers the control at the game's desktop place");
        auto at = doll::controlPosition(center, 15, 15);
        check(at && at->x == 15 && at->y == 15, "a full-size control lands where the game draws it");
        auto shrunk = doll::controlPosition(center, 0, 0);
        check(shrunk && shrunk->x == 22.5f && shrunk->y == 22.5f, "a control shrunk by the entrance animation keeps its center");
        check(!doll::controlPosition(center, -1, 15) && !doll::controlPosition({NAN, 0}, 15, 15), "bad geometry places nothing");
        auto moved = doll::controlPosition(doll::controlCenter(200, 100), 15, 15);
        check(moved && moved->x == 215 && moved->y == 115, "a moved element moves the control with it");
        auto parts = doll::figure(doll::Box{100, 40, doll::placedWidth, doll::placedHeight});
        check(parts[0].x == 118.5f && parts[0].y == 49 && parts[0].w == 8 && parts[5].y + parts[5].h == 81,
              "the editor's stand-in figure is centered in the element box");
        using lamium::ui::HudElementId;
        check(lamium::ui::hudGameDrawn(HudElementId::PaperDoll) && lamium::ui::hudGameDrawn(HudElementId::Chat)
              && !lamium::ui::hudGameDrawn(HudElementId::Info), "the chat and the paper doll are the game's own elements");
        auto placed = lamium::ui::placeElement(480, 270, doll::placedWidth, doll::placedHeight,
                                               lamium::ui::defaultHudElement(HudElementId::PaperDoll));
        check(placed.x == 0 && placed.y == 0, "the paper doll's default place is the top left corner");
    }
    {
        lamium::Settings fresh;
        check(!fresh.information.paperDollPosition, "paper doll position starts off");
        auto saved = lamium::decodeSettings(R"({"information":{"paperDollPosition":true},"hud":{"paperDoll":{"anchor":8,"dx":-10,"dy":-20,"scale":150}}})");
        check(saved.information.paperDollPosition && saved.hud.paperDoll.dx == -10 && saved.hud.paperDoll.scale == 100,
              "the paper doll's switch and place load; it has no scale");
    }

    lamium::Settings defaults;
    check(!defaults.information.offhandSlot && !defaults.information.offhandSlotEmpty, "the offhand slot starts off");
    auto loaded = lamium::decodeSettings(R"({"information":{"offhandSlot":true,"offhandSlotEmpty":true}})");
    check(loaded.information.offhandSlot && loaded.information.offhandSlotEmpty, "offhand slot options load");
}
