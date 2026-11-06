#include "ui/LanguageSelection.h"
void check(bool, char const*);
void languageSelectionTests() {
    using namespace lamium::ui;
    LanguageSelection s;
    s.add("en_US", "English (US)");
    s.add("ja_JP", "");
    s.add("en_US", "again");
    s.add("", "empty");
    check(s.choices.size() == 2 && s.choices[1].name == "ja_JP", "duplicates and empty codes are dropped; a missing name shows the code");
    check(!s.request("en_US", "en_US"), "nothing is selected, nothing is requested");
    s.select(0);
    check(!s.request("en_US", "en_US"), "the current, saved language is not applied again");
    check(s.request("en_US", "ja_JP") == "en_US", "a current language that is not saved yet is saved");
    s.select(1);
    check(s.request("en_US", "en_US") == "ja_JP", "another language is requested");
    s.select(5);
    check(s.selected == 1, "out-of-range selection is ignored");
    s.move(-10, 1);
    check(s.selected == 0 && s.first == 0, "moving clamps to the list");
    s.move(10, 1);
    check(s.selected == 1 && s.first == 1, "moving reveals the selection");
    LanguageSelection many;
    for (size_t i = 0; i < LanguageSelection::limit + 5; ++i) many.add("c" + std::to_string(i), "n");
    check(many.choices.size() == LanguageSelection::limit, "the list is bounded");

    LanguagePace pace;
    check(pace.wait(0) == 0, "the first change starts at once");
    pace.applied(100);
    check(pace.wait(100) == 5 && pace.wait(102.5) == 3 && pace.wait(105) == 0, "later changes wait for the interval");

    LanguageContext asked{"ja_JP", 0, true};
    check(asked.permits({"ja_JP", 0, true}, false), "the same world, dimension and open screen applies");
    check(!asked.permits({"ja_JP", 0, true}, true), "a cancelled request does not apply");
    check(!asked.permits({"ja_JP", 1, true}, false), "a dimension change cancels");
    check(!asked.permits({"en_US", 0, true}, false), "a language changed elsewhere cancels");
    check(!asked.permits({"ja_JP", 0, false}, false), "a closed screen cancels");
    check(!LanguageContext{"ja_JP", 0, false}.permits({"ja_JP", 0, true}, false), "a request from a closed screen does not apply");

    auto t = SettingsTable::fit(640, 360, 60, 0, 13);
    auto l = languageLayout(t, 29, 0);
    check(l.width > 0 && l.listVisible > 0 && l.rowsTop + l.listVisible * SettingsTable::rowHeight <= l.footerTop,
        "language rows end above the footer");
    check(l.left == t.tableLeft && l.width == t.tableWidth && l.top == t.theadTop && l.footerTop == t.footerTop,
        "the list takes the settings rows' place; sidebar, header and footer stay");
    check(l.closeX + ShapesLayout::closeWidth <= t.rowsRight() && l.closeX > t.nameX, "Back sits at the end of the heading row");
    check(languageLayout(t, 29, 100).listFirst + l.listVisible == 29, "the first row is clamped");
    check(languageLayout(SettingsTable::fit(100, 100, 29, 0), 29, 0).width <= 0, "too-small windows draw nothing");
}
