#include "ui/HudContents.h"
#include "features/information/InfoLines.h"
#include "settings/Options.h"
void check(bool, char const*);
void hudContentsTests() {
    using namespace lamium::ui;
    using namespace lamium::ui::hud_editor;
    auto order = lamium::information::defaultLineOrder();
    auto info = contentsOf(HudElementId::Info, order);
    check(info.reorder && info.entries.size() == order.size(), "the Info HUD lists every line and can reorder them");
    check(info.entries.front().line == order.front() && info.entries.front().option == "information." + order.front(),
          "an Info entry is its line's settings option, in the saved order");
    for (auto id : {HudElementId::Target, HudElementId::Durability, HudElementId::Inventory, HudElementId::UsedSlots}) {
        auto contents = contentsOf(id, order);
        check(!contents.reorder && !contents.entries.empty(), "other elements get switches only");
        for (auto const& entry : contents.entries) check(entry.line.empty(), "only Info entries carry a line to reorder");
    }
    for (auto id : {HudElementId::Status, HudElementId::Minimap, HudElementId::Chat, HudElementId::Toast})
        check(contentsOf(id, order).entries.empty(), "an element without content switches has no Contents button");
    auto durability = contentsOf(HudElementId::Durability, order);
    check(durability.entries.size() == 5 && durability.entries[0].option == "information.durabilityMainHand"
          && durability.entries[1].option == "information.durabilityOffhand"
          && durability.entries[4].option == "information.durabilityAmmo", "Durability: main hand, offhand, armor, elytra, ammo");
    check(contentsOf(HudElementId::Inventory, order).entries[0].option == "information.inventoryHotbar"
          && contentsOf(HudElementId::UsedSlots, order).entries[0].option == "information.usedSlotsHotbar",
          "Inventory and Used slots: the hotbar switch each keeps");
    // Every switch is a settings row's own on/off option: no new saved keys,
    // and no look choice (Target's health, armor and growth meters) slips in.
    lamium::Settings value;
    for (int i = 0; i <= static_cast<int>(HudElementId::BossBars); ++i)
        for (auto const& entry : contentsOf(static_cast<HudElementId>(i), order).entries) {
            auto option = lamium::settings::find(entry.option);
            check(option != nullptr, "a Contents entry is an option of the settings table");
            if (!option) continue;
            auto current = option->read(value);
            check(std::holds_alternative<bool>(current), "a Contents entry is an on/off setting");
            option->adjust(value, 1);
            auto flipped = option->read(value);
            check(std::holds_alternative<bool>(flipped) && std::get<bool>(flipped) != std::get<bool>(current),
                  "the switch flips the settings row's own field");
        }

    // Info rows keep today's geometry: 136 wide, 12-unit rows, arrows at the end.
    Spot below{100, 100, true};
    auto layout = layoutContents(below, 15, 3, true, 0, 640, 360, 18, 9);
    check(layout.box.x == 100 && layout.box.y == 117 && layout.box.w == 136 && layout.box.h == 3 * 12 + 6,
          "the popover opens under its toolbar, sized to its rows");
    check(layout.rows.size() == 3 && !layout.track, "rows that fit need no scroll bar");
    auto const& top = layout.rows[0];
    check(top.toggle.x == 102 && top.toggle.w == 100 && top.up && top.up->x == 204 && top.down && top.down->x == 215,
          "a reorderable row: the switch then up and down cells");
    check(!top.canUp && top.canDown && layout.rows[1].canUp && layout.rows[1].canDown && !layout.rows[2].canDown,
          "the first row cannot move up and the last cannot move down");
    check(top.switchAt.x == 105 && top.switchAt.y == 120 + 1.5f && top.labelAt.x == 126 && top.labelWidth == 136 - 18 - 36,
          "switch and name sit where the Lines popover drew them");
    auto plain = layoutContents(below, 15, 4, false, 0, 640, 360, 18, 9);
    check(plain.rows.size() == 4 && !plain.rows[0].up && !plain.rows[0].down, "a switch-only row has no arrows");
    check(plain.rows[0].toggle.w == 136 - 9 && plain.rows[0].labelWidth == 136 - 18 - 14,
          "a switch-only row flips anywhere along it and its name takes the arrows' room");

    // Too little room: the list scrolls and keeps its first row in range.
    Spot low{100, 300, true};
    auto scrolled = layoutContents(low, 15, 40, true, 50, 640, 360, 18, 9);
    check(scrolled.visible > 0 && scrolled.visible < 40 && scrolled.track, "a long list scrolls with a bar");
    check(scrolled.first == 40 - scrolled.visible, "the first row is clamped so the list ends at its last row");
    check(scrolled.rows.back().index == 39 && !scrolled.rows.back().canDown, "the last row shown is the last line");
    check(scrolled.box.y + scrolled.box.h <= 300 - 2 + .01f, "opening upward, the popover hugs the toolbar's top");
    check(onContentsTrack(scrolled, scrolled.track->x, scrolled.track->top + 1), "the bar takes a press");
    check(scrolled.rows[0].toggle.x + scrolled.rows[0].toggle.w <= scrolled.track->x - 2,
          "a row's switch stops short of the scroll bar");
    auto dragged = layoutContents(low, 15, 40, true, 0, 640, 360, 18, 9, scrolled.track->top + scrolled.track->track);
    check(dragged.first == 40 - dragged.visible, "dragging the bar to its end shows the last rows");
    check(layoutContents(below, 15, 0, false, 0, 640, 360, 18, 9).rows.empty(), "an empty list has no rows");
}
