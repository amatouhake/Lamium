#include "ui/SettingsTable.h"
#include "ui/KeycapLines.h"
#include "app/Versions.h"
#include <array>
#include <limits>
void check(bool, char const*);
void settingsTableTests() {
    using lamium::ui::SettingsTable;
    using Zone = SettingsTable::Zone;
    using Column = SettingsTable::Column;
    constexpr int navItems = 13;
    // Every screen size, shrunk as drawn, gets the one full layout.
    for (auto [w,h] : {std::pair{800.f,600.f}, std::pair{240.f,150.f}, std::pair{380.f,240.f}, std::pair{640.f,360.f},
                       std::pair{1920.f,1080.f}, std::pair{426.f,240.f}, std::pair{300.f,900.f}}) {
        float scale = SettingsTable::scaleFor(w,h,20);
        auto normal = SettingsTable::fit(w / scale,h / scale,60,5,20);
        check(scale > 0 && scale <= 1 && normal.usable() && normal.tableLeft == normal.left + SettingsTable::sidebarWidth
            && normal.navStep >= SettingsTable::minNavStep
            && (normal.left + normal.width) * scale <= w + .01f && (normal.top + normal.height) * scale <= h + .01f,
            "a small screen shrinks the full layout to fit instead of switching to another one");
        check(normal.descriptionHintY() == normal.footerTop + 30
            && normal.footerTop + 3 + SettingsTable::descriptionLines * SettingsTable::descriptionLineHeight <= normal.descriptionHintY()
            && normal.hit(normal.left + 8,normal.footerTop + 4,20).zone == Zone::Footer,
            "two description lines sit above the operation hint in the footer");
    }

    // Typical 16:9 GUI sizes keep the sidebar and full columns.
    for (auto [w, h] : {std::pair{640.f, 360.f}, std::pair{480.f, 270.f}, std::pair{960.f, 540.f}}) {
        auto t = SettingsTable::fit(w, h, 60, 0);
        check(t.usable() && SettingsTable::scaleFor(w, h, 15) == 1, "16:9 GUI sizes fit unscaled");
        check(t.left >= 0 && t.left + t.width <= w && t.top >= 0 && t.top + t.height <= h, "panel inside screen");
        check(t.nameX < t.stateX && t.stateX + SettingsTable::stateWidth < t.keyX
            && t.keyX + t.keyWidth <= t.rowsRight(), "columns are ordered inside the table");
        check(t.rowY(t.first + t.visible - 1) + SettingsTable::rowHeight <= t.footerTop, "rows end above the footer");
        check(t.footerButtonX(2) + SettingsTable::footerButtonWidth <= t.left + t.width, "footer buttons fit");
    }
    auto wide = SettingsTable::fit(640, 360, 60, 0);
    check(wide.keyWidth == 88 && SettingsTable::fit(500, 360, 60, 0).keyWidth == 70,
        "a narrower table narrows the key column first");
    auto narrow = SettingsTable::fit(SettingsTable::minWidth + SettingsTable::margin, 360, 60, 0);
    check(narrow.usable() && narrow.width == SettingsTable::minWidth, "the narrowest layout still has every column");
    check(!SettingsTable::fit(400, 360, 60, 0).usable() && !SettingsTable::fit(640, 220, 60, 0).usable()
        && SettingsTable::scaleFor(400, 360, 13) < 1 && SettingsTable::scaleFor(640, 220, 13) < 1
        && SettingsTable::scaleFor(1920, 1080, 13) == 1,
        "below the minimum the layout is drawn shrunk, never enlarged");
    // L-101: the version after the title is its own zone and clears the search
    // field even in the narrowest usable panel ("Lamium" ~30 units, "0.1.6" ~22).
    for (float w : {456.f, 520.f, 640.f}) {
        auto t = SettingsTable::fit(w, 360, 60, 0);
        t.placeVersion(31, 22);
        float y = t.top + 6;
        check(t.versionWidth == 22 && t.versionX + t.versionWidth < t.searchX, "the version ends before the search field");
        check(t.hit(t.versionX + 1, y, navItems).zone == Zone::Version
            && t.hit(t.versionX + t.versionWidth + 1, y, navItems).zone != Zone::Version
            && t.hit(t.versionX + 1, t.rowsTop + 1, navItems).zone != Zone::Version,
            "only the version text in the header is the version zone");
    }
    check(SettingsTable::fit(640, 360, 60, 0).hit(40, 30, navItems).zone != Zone::Version,
        "without a placed version there is no version zone");
    auto tiny = SettingsTable::fit(456, 360, 60, 0);
    tiny.placeVersion(31, 200);
    check(tiny.versionWidth == 0 && tiny.hit(tiny.versionX + 1, tiny.top + 6, navItems).zone != Zone::Version,
        "a version that would reach the search field is hidden");
    // The versionLine format is what bug reports paste.
    check(lamium::versionLine("0.1.6", "1.26.51", "26.51.5") == "Lamium 0.1.6 · Minecraft 1.26.51 · LeviLamina 26.51.5",
        "the version line names Lamium, Minecraft and LeviLamina in English");
    for (auto const& t : {wide, narrow}) {
        for (bool hotkeys : {false, true}) {
            float x = t.headActionX(hotkeys), y = t.theadTop + 2;
            check(x > t.nameX + 40 && x + SettingsTable::headActionWidth <= (hotkeys ? t.keyX : t.stateX - 6),
                "the reset button sits between the name heading and the next column heading");
            check(t.headAction(x + 1, y, hotkeys) && !t.headAction(x + 1, t.rowsTop + 1, hotkeys)
                && t.hit(x + 1, y, 9).zone == SettingsTable::Zone::None,
                "the reset button is hit only on the heading line, which no other zone claims");
        }
        float kx = t.keyFindX(), ky = t.theadTop + 2;
        check(t.keyFindShown() && t.headActionX(true) + SettingsTable::headActionWidth < kx
              && kx + SettingsTable::keyFindWidth < t.keyX
              && t.keyFind(kx + 1, ky) && !t.headAction(kx + 1, ky, true) && !t.keyFind(kx + 1, t.rowsTop + 1),
              "Find by key sits next to the key heading, the Hotkeys reset button left of it");
        for (int count : {2, 3}) {
            float row = t.rowY(t.first + 2), last = t.captureButtonX(count - 1, count);
            check(t.captureButtonX(0, count) > t.nameX + 40 && last + SettingsTable::captureButtonWidth < t.keyX
                  && t.captureButton(t.captureButtonX(0, count) + 1, row + 4, row, count) == 0
                  && t.captureButton(last + 1, row + 4, row, count) == count - 1
                  && t.captureButton(last + 1, row + SettingsTable::rowHeight + 4, row, count) == -1,
                  "binding editor buttons sit on the edited row just before its key cell");
        }
    }

    // Scrolling is owned by the caller and clamped.
    check(SettingsTable::fit(640, 360, 60, 1000).first == 60 - wide.visible, "scroll clamps to the last page");
    // Fourteen sidebar items (L-93 added Schematics twice) in a short window.
    auto squeezed = SettingsTable::fit(640, 260, 60, 0, 14);
    check(squeezed.usable() && squeezed.navStep >= SettingsTable::minNavStep
          && squeezed.navItemY(14 - SettingsTable::pinnedItems - 1) + squeezed.navStep + 4 <= squeezed.pinnedItemY(0),
          "a short window narrows sidebar items so the top ones and the pinned ones do not meet");
    check(!SettingsTable::fit(640, 190, 60, 0, 14).usable() && SettingsTable::scaleFor(640, 190, 14) < 1
          && SettingsTable::fit(640, 360, 60, 0, 14).navStep == SettingsTable::navItemHeight,
          "a window too short for the sidebar is drawn shrunk; a tall one keeps full items");
    check(SettingsTable::fit(640, 360, 5, 3).first == 0, "short lists do not scroll");
    check(SettingsTable::reveal(10, 5, 8) == 5 && SettingsTable::reveal(10, 20, 8) == 13 && SettingsTable::reveal(10, 12, 8) == 10,
        "reveal scrolls minimally");

    // Hit testing: header controls, sidebar, rows by column, footer.
    auto t = SettingsTable::fit(640, 360, 60, 4);
    auto y = t.rowY(6) + 3;
    check(t.hit(t.nameX + 2, y, navItems).zone == Zone::Row && t.hit(t.nameX + 2, y, navItems).index == 6, "row hit follows scroll");
    check(t.hit(t.nameX + 2, y, navItems).column == Column::Name, "name column");
    check(t.hit(t.stateX + 2, y, navItems).column == Column::State, "state column");
    check(t.hit(t.keyX + 2, y, navItems).column == Column::Key, "key column");
    check(t.hit(t.closeX + 2, t.top + 8, navItems).zone == Zone::Close, "close button");
    check(t.hit(t.searchX + 2, t.top + 8, navItems).zone == Zone::Search, "search field");
    auto nav = t.hit(t.left + 10, t.navItemY(2) + 3, navItems);
    check(nav.zone == Zone::Nav && nav.index == 2, "sidebar item");
    auto pinned = t.hit(t.left + 10, t.pinnedItemY(SettingsTable::pinnedItems - 1) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 1, "HUD layout item is pinned at the sidebar bottom");
    pinned = t.hit(t.left + 10, t.pinnedItemY(SettingsTable::pinnedItems - 2) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 2, "world map item is pinned above the HUD layout");
    pinned = t.hit(t.left + 10, t.pinnedItemY(3) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 3, "schematics item is pinned above the world map");
    pinned = t.hit(t.left + 10, t.pinnedItemY(2) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 4, "waypoints item is pinned above schematics");
    pinned = t.hit(t.left + 10, t.pinnedItemY(1) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 5, "shapes item is pinned above waypoints");
    pinned = t.hit(t.left + 10, t.pinnedItemY(0) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 6, "hotkeys item is pinned above shapes");
    check(t.hit(t.left + 10, t.footerTop + 5, navItems).zone == Zone::Footer, "footer spans the panel");
    check(t.footerButton(t.footerButtonX(1) + 3, t.footerButtonY() + 3) == 1, "footer button index");
    check(t.hit(t.left - 1, y, navItems).zone == Zone::None, "outside the panel");
    check(t.hit(std::numeric_limits<float>::quiet_NaN(), y, navItems).zone == Zone::None, "invalid pointer");
    // Scrollbars: the list's in its right padding, the footer's beside its text.
    check(t.onListScrollbar(t.rowsRight() - 2, t.rowsTop + 5) && !t.onListScrollbar(t.keyX + t.keyWidth - 2, t.rowsTop + 5)
        && !SettingsTable::fit(640, 360, 5, 0).onListScrollbar(t.rowsRight() - 2, t.rowsTop + 5)
        && t.listFirstAt(t.rowsTop - 10) == 0 && t.listFirstAt(t.rowsTop + t.visible * SettingsTable::rowHeight) == 60 - t.visible,
        "the list scrollbar is pressed in the right padding and maps the track to every row");
    auto footer = SettingsTable::fit(640, 360, 60, 0, 20);
    float barX = footer.left + footer.width - SettingsTable::pad + 1;
    check(footer.onDescriptionScrollbar(barX, footer.footerTop + 5, 100) && !footer.onDescriptionScrollbar(barX, footer.footerTop + 5, 1)
        && !footer.onDescriptionScrollbar(footer.left + 20, footer.footerTop + 5, 100)
        && footer.descriptionFirstAt(footer.footerTop, 100) == 0
        && footer.descriptionFirstAt(footer.top + footer.height, 100) == 100 - SettingsTable::descriptionLines
        && !footer.onDescriptionScrollbar(barX, footer.footerTop + 5, SettingsTable::descriptionLines),
        "an overflowing description's scrollbar reaches its first and last lines");

    // Stepper parts: one state column on every row, ending before the key column (L-349).
    check(t.stepperPart(t.stepperX() + 2) == -1 && t.stepperPart(t.stepperX() + t.stepperWidth() - 2) == 1
        && t.stepperPart(t.stepperX() + t.stepperWidth() / 2) == 0 && t.stepperPart(t.nameX) == 2, "stepper parts");
    for (float screenWidth : {456.0f, 640.0f, 1280.0f}) {
        auto table = SettingsTable::fit(screenWidth, 360, 20, 0);
        check(table.usable() && table.stepperX() + table.stepperWidth() == table.keyX - SettingsTable::gap
            && table.stepperWidth() == SettingsTable::stepperColumnWidth && table.stepperX() > table.nameX
            && table.stepperPart(table.keyX + 2) == 2 && table.stepperPart(table.keyX - SettingsTable::gap - 1) == 1,
            "every stepper has the same width and ends before the key column, so a choice never reaches a key cell");
    }
    check(SettingsTable::fit(456, 360, 20, 0).keyWidth < SettingsTable::fit(640, 360, 20, 0).keyWidth,
          "the column test covers both key widths");
    {
        auto slider = SettingsTable::fit(640, 360, 20, 0);
        check(slider.sliderFraction(slider.sliderX() + 3) == 0 && slider.sliderFraction(slider.sliderX() + slider.sliderWidth() - 3) == 1,
              "the slider track maps its ends to 0 and 1");
        check(slider.sliderFraction(slider.sliderValueX() + 1) == -1, "the value text is not part of the track");
        check(SettingsTable::sliderValue(.5f, 2, 64, 1) == 33 && SettingsTable::sliderValue(.52f, 1, 10, .5f) == 5.5f,
              "slider values snap to the step");
        check(SettingsTable::sliderValue(2, 2, 64, 1) == 64 && SettingsTable::sliderPosition(6, 2, 64) > 0,
              "slider values and positions clamp to the range");
    }
    {
        // Key caps wrap onto another line only when the next cap plus its gap
        // would overflow (L-235); a lone cap wider than the line still advances.
        std::array<float, 3> caps{30, 30, 30};
        auto lines = lamium::ui::keycapLines(std::span<float const>{caps}, 100);
        check(lines.size() == 2 && lines[0] == std::pair<size_t, size_t>{0, 2} && lines[1] == std::pair<size_t, size_t>{2, 3},
              "three caps wrap into two lines at the measured width");
        std::array<float, 2> exact{30, 30};
        check(lamium::ui::keycapLines(std::span<float const>{exact}, 78).size() == 1,
              "caps that exactly fill the line do not wrap");
        std::array<float, 1> wide{200};
        auto lone = lamium::ui::keycapLines(std::span<float const>{wide}, 100);
        check(lone.size() == 1 && lone[0] == std::pair<size_t, size_t>{0, 1},
              "a lone too-wide cap still advances instead of looping");
        check(lamium::ui::keycapLines({}, 100).empty(), "no caps make no lines");
    }
}
