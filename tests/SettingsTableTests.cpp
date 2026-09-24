#include "ui/SettingsTable.h"
#include <limits>
void check(bool, char const*);
void settingsTableTests() {
    using lamium::ui::SettingsTable;
    using Zone = SettingsTable::Zone;
    using Column = SettingsTable::Column;
    constexpr int navItems = 8;

    // Typical 16:9 GUI sizes keep the sidebar and full columns.
    for (auto [w, h] : {std::pair{640.f, 360.f}, std::pair{480.f, 270.f}, std::pair{960.f, 540.f}}) {
        auto t = SettingsTable::fit(w, h, 60, 0);
        check(t.usable() && !t.compact && !t.shortFooter, "16:9 keeps sidebar and full footer");
        check(t.left >= 0 && t.left + t.width <= w && t.top >= 0 && t.top + t.height <= h, "panel inside screen");
        check(t.nameX < t.stateX && t.stateX + SettingsTable::stateWidth < t.keyX
            && t.keyX + t.keyWidth <= t.rowsRight(), "columns are ordered inside the table");
        check(t.rowY(t.first + t.visible - 1) + SettingsTable::rowHeight <= t.footerTop, "rows end above the footer");
        check(t.footerButtonX(2) + SettingsTable::footerButtonWidth <= t.left + t.width, "footer buttons fit");
    }
    auto wide = SettingsTable::fit(640, 360, 60, 0);
    check(wide.keyWidth == 88 && SettingsTable::fit(500, 360, 60, 0).keyWidth == 70 && !SettingsTable::fit(500, 360, 60, 0).compact,
        "a narrower table narrows the key column first");
    auto narrow = SettingsTable::fit(400, 360, 60, 0);
    check(narrow.compact && narrow.tableLeft == narrow.left && narrow.rowsTop > wide.rowsTop - 1,
        "narrow windows move categories into tabs");
    auto shortWindow = SettingsTable::fit(640, 220, 60, 0);
    check(shortWindow.shortFooter && shortWindow.usable(), "short windows keep a one-line footer");
    check(!SettingsTable::fit(200, 300, 60, 0).usable() && !SettingsTable::fit(640, 120, 60, 0).usable(),
        "too-small windows are reported unusable");

    // Scrolling is owned by the caller and clamped.
    check(SettingsTable::fit(640, 360, 60, 1000).first == 60 - wide.visible, "scroll clamps to the last page");
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
    auto pinned = t.hit(t.left + 10, t.pinnedItemY(2) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 1, "HUD layout item is pinned at the sidebar bottom");
    pinned = t.hit(t.left + 10, t.pinnedItemY(1) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 2, "shapes item is pinned above it");
    pinned = t.hit(t.left + 10, t.pinnedItemY(0) + 3, navItems);
    check(pinned.zone == Zone::Nav && pinned.index == navItems - 3, "hotkeys item is pinned above shapes");
    check(t.hit(t.left + 10, t.footerTop + 5, navItems).zone == Zone::Footer, "footer spans the panel");
    check(t.footerButton(t.footerButtonX(1) + 3, t.footerButtonY() + 3) == 1, "footer button index");
    check(t.hit(t.left - 1, y, navItems).zone == Zone::None, "outside the panel");
    check(t.hit(std::numeric_limits<float>::quiet_NaN(), y, navItems).zone == Zone::None, "invalid pointer");
    auto tabs = SettingsTable::fit(400, 360, 60, 0);
    float tabWidth = (tabs.width - 4) / navItems;
    auto tab = tabs.hit(tabs.left + 2 + tabWidth * 3 + 2, tabs.navTop + 5, navItems, tabWidth);
    check(tab.zone == Zone::Nav && tab.index == 3, "tab hit");

    // Stepper parts are right-aligned in the value columns.
    check(t.stepperPart(t.stepperX() + 2) == -1 && t.stepperPart(t.stepperX() + t.stepperWidth() - 2) == 1
        && t.stepperPart(t.stepperX() + t.stepperWidth() / 2) == 0 && t.stepperPart(t.nameX) == 2, "stepper parts");
    check(t.stepperX() >= t.stateX, "stepper stays in the value columns");
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
}
