#include "ui/SettingsLayout.h"
#include "settings/Options.h"
#include <stdexcept>

void settingsLayoutTests() {
    using lamium::ui::SettingsLayout;
    auto check = [](bool ok) { if (!ok) throw std::runtime_error("settings layout invariant"); };
    for (int count : {1, 10, static_cast<int>(lamium::settings::options.size()) + 1, 100}) {
    for (float height : {100.f, 144.f, 180.f, 240.f, 300.f, 480.f}) {
        int first = 0;
        for (int row = 0; row < count; ++row) {
            auto layout = SettingsLayout::fit(320, height, count, row, first);
            first = layout.first;
            check(layout.visible > 0 && row >= first && row < first + layout.visible);
            check(layout.rowY(first + layout.visible - 1) + layout.rowHeight < layout.footer);
            check(layout.footer + (layout.secondHint ? 29 : 14) <= height);
            check(layout.hit(layout.left + 5, layout.rowY(row) + 5) == row);
            check(layout.hit(layout.left + 5, layout.rowY(row) + 21) == -1);
            check(layout.hit(layout.left - 1, layout.rowY(row) + 5) == -1);
            check(layout.hit(layout.left + 5, layout.footer) == -1);
        }
        auto wrap = SettingsLayout::fit(320, height, count, 0, first);
        check(wrap.first == 0);
    }
    }
    check(SettingsLayout::fit(90, 60, 10, 0, 0).visible == 0);
    check(SettingsLayout::fit(640, 480, 10, 0, 0).visible == 10);
}
