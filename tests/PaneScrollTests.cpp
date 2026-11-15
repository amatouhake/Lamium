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
#include "ui/ScrollBar.h"
void scrollBarTests() {
    using namespace lamium::ui;
    auto fits = rowScrollBar(100, 0, 50, 4, 5, 0);
    check(!fits.scrolls() && !fits.hit(100, 10), "a bar whose rows fit neither scrolls nor takes a press");
    auto bar = rowScrollBar(100, 0, 100, 20, 10, 0);
    check(bar.scrolls() && bar.thumb == 50 && bar.thumbTop() == 0, "the thumb is the shown share, at the top for the first row");
    check(bar.hit(97, 10) && bar.hit(102, 10) && !bar.hit(90, 10) && !bar.hit(100, 100), "a press counts on the bar's column only");
    check(bar.grabAt(20) == 20 && bar.grabAt(80) == 25, "the thumb keeps its grip, the track centers it");
    check(bar.posAt(20, 20) == 0 && bar.posAt(70, 20) == 10 && bar.posAt(500, 20) == 10 && bar.posAt(-50, 20) == 0,
          "dragging maps the thumb's travel onto the range and stops at both ends");
    auto lower = rowScrollBar(100, 0, 100, 20, 10, 5);
    check(lower.thumbTop() == 25, "the thumb follows the first row");
    check(rowScrollBar(100, 0, 100, 1000, 10, 0, 8).thumb == 8, "a long list keeps a thumb you can take");
}
