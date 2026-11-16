#include "ui/ListColumns.h"
#include "ui/PaneScroll.h"
#include "ui/ShapesLayout.h"
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
#include "ui/ScrollDrag.h"
void scrollDragTests() {
    using namespace lamium::ui;
    int first = 0, other = 0;
    auto bar = rowScrollBar(100, 0, 100, 20, 10, 0);
    check(!scroll_drag::press(bar, 50, 10, scroll_drag::firstRow(first), &first) && !scroll_drag::holding(&first),
          "a press off the bar holds nothing");
    check(scroll_drag::press(bar, 100, 60, scroll_drag::firstRow(first), &first) && first == 7 && scroll_drag::holding(&first)
          && !scroll_drag::holding(&other) && !scroll_drag::holding(nullptr),
          "a press on the track centers the thumb there and holds that bar");
    scroll_drag::follow(500);
    check(first == 10, "while held the bar follows the pointer to its end");
    scroll_drag::release();
    scroll_drag::follow(0);
    check(first == 10 && !scroll_drag::holding(&first), "after release the pointer moves nothing");
    scroll_drag::PressOrder order;
    order.released();
    check(!order.takeReleaseAfterPress(), "a release alone ends the drag before anything else");
    order.pressed();
    order.released();
    check(order.takeReleaseAfterPress(), "a quick click's release ends what its press began");
    check(!order.takeReleaseAfterPress(), "the order is taken once");
    order.released();
    order.pressed();
    check(!order.takeReleaseAfterPress(), "a press after a drag's release stays held");
    order.pressed();
    order.released();
    order.pressed();
    check(!order.takeReleaseAfterPress(), "the last press of a frame stays held");
    order.pressed();
    order.clear();
    order.released();
    check(!order.takeReleaseAfterPress(), "a cleared press is forgotten");
}
void previewHeightTests() {
    using namespace lamium::ui;
    check(previewHeight(300, 0, 80, 12) == 165, "without item lines the preview keeps its share");
    check(previewHeight(250, 3, 80, 12) == 250 - 11 - 80 - 36, "item lines take room from the preview");
    check(previewHeight(300, 9, 80, 12) == previewHeight(300, 4, 80, 12), "only four lines are made room for; more scroll");
    check(previewHeight(150, 4, 80, 12) == 60, "the preview never goes under its minimum");
    check(previewHeight(600, 2, 80, 12) == 240, "with room to spare the preview keeps its size");
}
void barGutterTests() {
    using namespace lamium::ui;
    // The settings table: rows and the key column end short of the list's bar.
    auto fits = SettingsTable::fit(640, 360, 3, 0);
    check(fits.contentWidth() == fits.tableWidth && fits.keyX + fits.keyWidth == fits.rowsRight() - SettingsTable::pad,
          "a table that fits keeps its full width");
    auto t = SettingsTable::fit(640, 360, 200, 0);
    float barX = t.rowsRight() - 3, rowEnd = t.tableLeft + 1 + t.rowWidth();
    check(rowEnd < barX - 2 && t.keyX + t.keyWidth < rowEnd, "a scrolling table's rows and keys end before its bar");
    check(t.onListScrollbar(barX, t.rowsTop + 1) && t.hitZone(barX, t.rowsTop + 1, 13).zone != SettingsTable::Zone::Row,
          "pointing at the table's bar points at no row");
    check(t.hitZone(t.nameX, t.rowsTop + 1, 13).zone == SettingsTable::Zone::Row, "the row is still pointed at left of the bar");
    // A list: its shared columns end inside the shortened row.
    auto table = SettingsTable::fit(640, 360, 20, 0);
    auto l = ShapesLayout::fit(table, 640, 360, false, 80, 0, 16, 0, false, false);
    auto c = listColumns(l.listLeft, l.contentWidth(), 0, 18);
    float listRowEnd = l.listLeft + 1 + l.rowWidth();
    check(l.listCount > l.listVisible && c.shownX + c.shownWidth < listRowEnd - 2 && listRowEnd < l.scrollBar().x - 2,
          "a scrolling list's switch stays inside its row, the row short of the bar");
}
