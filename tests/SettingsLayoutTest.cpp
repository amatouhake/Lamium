#include "ui/SettingsLayout.h"
#include "settings/Options.h"
#include <stdexcept>
#include <limits>

void settingsLayoutTests() {
    using lamium::ui::SettingsLayout;
    auto check = [](bool ok) { if (!ok) throw std::runtime_error("settings layout invariant"); };
    for (int count : {1, 10, static_cast<int>(lamium::settings::options.size() + lamium::input::actions.size()) + 3, 100}) {
    for (float height : {100.f, 144.f, 180.f, 240.f, 300.f, 480.f}) {
        int first = 0;
        for (int row = 0; row < count; ++row) {
            auto layout = SettingsLayout::fit(320, height, count, row, first);
            first = layout.first;
            check(layout.visible > 0 && row >= first && row < first + layout.visible);
            check(layout.rowY(first + layout.visible - 1) + layout.rowHeight < layout.footer);
            check(layout.footer + (layout.secondHint ? 57 : 14) <= height);
            check(layout.hit(layout.left + 5, layout.rowY(row) + 5) == row);
            check(layout.hit(layout.left + 5, layout.rowY(row) + layout.rowHeight + 1) == -1);
            check(layout.hit(layout.left - 1, layout.rowY(row) + 5) == -1);
            check(layout.hit(layout.left + 5, layout.footer) == -1);
            check(layout.bottom + 6 <= height);
        }
        auto wrap = SettingsLayout::fit(320, height, count, 0, first);
        check(wrap.first == 0);
    }
    }
    check(SettingsLayout::fit(90, 60, 10, 0, 0).visible == 0);
    check(SettingsLayout::fit(640, 480, 10, 0, 0).visible == 10);
    auto full = SettingsLayout::fit(640, 360, 100, 0, 0);
    auto filtered = SettingsLayout::fit(640, 360, 5, 0, 0);
    check(full.visible >= 12);
    check(full.rowsTop == filtered.rowsTop && full.top == filtered.top);
    check(filtered.bottom < full.bottom);
    for (float scale : {1.f, 2.f, 3.f, 4.f}) {
        auto scrolled = SettingsLayout::fit(640, 360, 100, 40, 30);
        for (int row = scrolled.first; row < scrolled.first + scrolled.visible; ++row) {
            check(scrolled.hitPixels((scrolled.left + 8) * scale,
                (scrolled.rowY(row) + 5) * scale, 1 / scale) == row);
            check(scrolled.hitPixels((scrolled.left + 8) * scale,
                (scrolled.rowY(row) + SettingsLayout::rowHeight + 1) * scale, 1 / scale) == -1);
        }
    }
    check(full.hitPixels(300, 300, 0) == -1);
    check(full.hitPixels(300, 300, std::numeric_limits<float>::infinity()) == -1);
    check(full.hit(std::numeric_limits<float>::quiet_NaN(), full.rowsTop) == -1);
}
