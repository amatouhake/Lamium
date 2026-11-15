#include "ui/RowSelection.h"
void check(bool, char const*);
void rowSelectionTests() {
    using namespace lamium::ui;
    using Rows = RowSelection<int>;
    std::vector<int> const order{10, 20, 30, 40, 50};
    Rows rows;
    rows.only(20);
    check(rows.keys == std::vector<int>{20} && rows.contains(20) && !rows.contains(30) && rows.shown == 20, "a plain click selects one row");
    rows.toggle(40);
    check(rows.keys == std::vector<int>{20, 40} && rows.several() && rows.shown == 40, "Ctrl + click adds a row and shows it");
    rows.toggle(20);
    check(rows.keys == std::vector<int>{40} && rows.shown == 40, "Ctrl + click on a selected row removes it");
    rows.toggle(30);
    rows.toggle(30);
    check(rows.keys == std::vector<int>{40} && rows.shown == 40, "removing the shown row shows the last one still selected");
    rows.toggle(40);
    check(rows.keys.empty() && !rows.shown, "removing the last row leaves nothing shown");
    rows.only(20);
    rows.range(40, order);
    check(rows.keys == std::vector<int>{20, 30, 40} && rows.shown == 40, "Shift + click takes the rows between and shows the clicked one");
    rows.range(10, order);
    check(rows.keys == std::vector<int>{10, 20}, "a second range starts again from the same anchor");
    Rows empty;
    empty.range(30, order);
    check(empty.keys == std::vector<int>{30}, "a range without an anchor selects its row");
    rows.only(99);
    rows.range(30, order);
    check(rows.keys == std::vector<int>{30}, "a range from a row not in the list selects its row");
    rows.only(20);
    rows.click(50, true, true, order);
    check(rows.keys == std::vector<int>{20, 30, 40, 50}, "Shift wins over Ctrl");
    rows.click(30, false, false, order);
    check(rows.keys == std::vector<int>{30} && rows.shown == 30, "a plain click returns to one row");
    rows.only(20);
    rows.range(40, order);
    rows.retain([](int key) { return key != 20; });
    check(rows.keys == std::vector<int>{30, 40} && !rows.anchor && rows.shown == 40, "a removed row leaves the selection and the anchor");
    rows.retain([](int key) { return key != 40; });
    check(rows.keys == std::vector<int>{30} && rows.shown == 30, "a removed shown row shows another selected one");
    rows.only(30);
    rows.selectAll(order);
    check(rows.keys == order && rows.shown == 30, "Ctrl + A selects every row and keeps the shown one");
    Rows none;
    none.selectAll({});
    check(none.keys.empty() && !none.shown, "Ctrl + A in an empty list selects nothing");
    none.selectAll({7});
    check(none.keys == std::vector<int>{7} && none.shown == 7 && !none.several(), "Ctrl + A in a list of one selects it alone");
    check(rows.collapse() && rows.keys == std::vector<int>{30} && rows.shown == 30, "Esc with several rows keeps the shown one");
    check(!rows.collapse() && rows.keys == std::vector<int>{30}, "Esc with one row leaves it to close the screen");
    rows.clear();
    check(rows.keys.empty() && !rows.anchor && !rows.shown, "clearing forgets the rows, the anchor and the shown row");

    check(bulkActions(SelectionList::Waypoints) == std::vector{BulkAction::Show, BulkAction::Hide, BulkAction::Delete}
        && bulkActions(SelectionList::Shapes) == bulkActions(SelectionList::Waypoints)
        && bulkActions(SelectionList::Placements) == bulkActions(SelectionList::Waypoints),
        "the screens' lists show, hide and delete together");
    check(bulkActions(SelectionList::MapPanel) == std::vector{BulkAction::Show, BulkAction::Hide},
        "the map's side panel shows and hides only");
    BulkBar bar;
    check(bar.press(BulkAction::Show) && bar.press(BulkAction::Hide), "show and hide act on the first press");
    check(!bar.press(BulkAction::Delete) && bar.armed == BulkAction::Delete, "delete arms on the first press");
    check(bar.press(BulkAction::Delete) && !bar.armed, "delete acts on the second press");
    bar.press(BulkAction::Delete);
    check(bar.press(BulkAction::Show) && !bar.press(BulkAction::Delete), "another press disarms delete");
    bar.disarm();
    check(!bar.press(BulkAction::Delete), "a selection change disarms delete");
    check(selectionKey(0x41, true, false) == SelectionKey::SelectAll && selectionKey(0x41, false, true) == SelectionKey::None,
        "Ctrl + A selects all, A alone does not");
    check(selectionKey(0x1b, false, true) == SelectionKey::Collapse && selectionKey(0x1b, false, false) == SelectionKey::None,
        "Esc clears several rows first, then is left to the screen");

    auto l = bulkBarLayout(100, 50, 120, 12, {30, 30, 40});
    check(l.buttonX[2] + l.buttonW[2] == 220 && l.buttonX[1] + 30 + BulkBarLayout::gap == l.buttonX[2]
        && l.buttonX[0] == 220 - 100 - 2 * BulkBarLayout::gap, "the bar's buttons sit at its right edge");
    check(l.countWidth() == l.buttonX[0] - BulkBarLayout::gap - 100, "the count takes what is left");
    check(l.at(l.buttonX[1] + 1, 55) == 1 && l.at(l.buttonX[1] + 1, 62) == -1 && l.at(101, 55) == -1, "a press finds its button");
    auto tight = bulkBarLayout(0, 0, 50, 12, {30, 30});
    check(tight.countWidth() == 0, "a bar too narrow for the count leaves it no room");
}
