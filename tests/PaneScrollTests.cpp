#include "ui/PaneScroll.h"
void check(bool, char const*);
void paneScrollTests() {
    using namespace lamium::ui;
    check(linesFitting(0, 10, 12, 10) == 1 && linesFitting(0, 34, 12, 10) == 3 && linesFitting(0, 33.9f, 12, 10) == 2,
          "a line fits while its text ends above the bottom");
    check(linesFitting(0, 9, 12, 10) == 0 && linesFitting(10, 0, 12, 10) == 0 && linesFitting(0, 50, 0, 10) == 0,
          "no room or no pitch fits no line");
    auto fitting = lineScroll(3, 2, 0, 100, 12, 10);
    check(!fitting.overflows() && fitting.visible == 3 && fitting.first == 0, "lines that fit do not scroll");
    auto many = lineScroll(10, 0, 0, 34, 12, 10);
    check(many.overflows() && many.visible == 3 && many.first == 0, "lines past the bottom scroll");
    check(lineScroll(10, 99, 0, 34, 12, 10).first == 7 && lineScroll(10, -3, 0, 34, 12, 10).first == 0,
          "the offset stops where the last line ends the view");
    check(lineScroll(10, 5, 0, 5, 12, 10).visible == 0,
          "with no room nothing is shown");
}
