#include "ui/RowSelection.h"
void check(bool, char const*);
void rowSelectionTests() {
    using namespace lamium::ui;
    using Rows = RowSelection<int>;
    std::vector<int> const order{10, 20, 30, 40, 50};
    Rows rows;
    rows.only(20);
    check(rows.keys == std::vector<int>{20} && rows.contains(20) && !rows.contains(30) && rows.single() == 20, "a plain click selects one row");
    rows.toggle(40);
    check(rows.keys == std::vector<int>{20, 40} && rows.several() && !rows.single(), "Ctrl + click adds a row; several have no single row");
    rows.toggle(20);
    check(rows.keys == std::vector<int>{40} && rows.single() == 40, "Ctrl + click on a selected row removes it, leaving one");
    rows.toggle(40);
    check(rows.keys.empty() && !rows.single(), "removing the last row leaves nothing selected");
    rows.only(20);
    rows.range(40, order);
    check(rows.keys == std::vector<int>{20, 30, 40} && !rows.single(), "Shift + click takes the rows between");
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
    check(rows.keys == std::vector<int>{30} && rows.single() == 30, "a plain click returns to one row");
    rows.only(20);
    rows.range(40, order);
    rows.retain([](int key) { return key != 20; });
    check(rows.keys == std::vector<int>{30, 40} && !rows.anchor, "a removed row leaves the selection and the anchor");
    rows.retain([](int key) { return key != 40; });
    check(rows.single() == 30, "several rows that lose all but one leave that one alone");
    rows.only(50);
    rows.toggle(10);
    rows.toggle(30);
    check(rows.inOrder(order) == std::vector<int>{10, 30, 50}, "the pane lists the selected rows in the list's order");
    rows.only(30);
    rows.selectAll(order);
    check(rows.keys == order && rows.several(), "Ctrl + A selects every row");
    Rows none;
    none.selectAll({});
    check(none.keys.empty(), "Ctrl + A in an empty list selects nothing");
    none.selectAll({7});
    check(none.keys == std::vector<int>{7} && none.single() == 7, "Ctrl + A in a list of one selects it alone");
    check(rows.clearSeveral() && rows.keys.empty() && !rows.anchor, "Esc with several rows lets them all go");
    rows.only(30);
    check(!rows.clearSeveral() && rows.keys == std::vector<int>{30}, "Esc with one row leaves it to close the screen");
    rows.clear();
    check(!rows.clearSeveral() && rows.keys.empty(), "Esc with nothing selected is left to close the screen");

    check(bulkActions(SelectionList::Waypoints) == std::vector{BulkAction::Show, BulkAction::Hide, BulkAction::Delete}
        && bulkActions(SelectionList::Shapes) == bulkActions(SelectionList::Waypoints)
        && bulkActions(SelectionList::Placements) == bulkActions(SelectionList::Waypoints),
        "the screens' lists show, hide and delete together");
    check(bulkActions(SelectionList::MapPanel) == std::vector{BulkAction::Show, BulkAction::Hide},
        "the map's side panel shows and hides only");
    BulkArm arm;
    check(arm.press(BulkAction::Show) && arm.press(BulkAction::Hide), "show and hide act on the first press");
    check(!arm.press(BulkAction::Delete) && arm.armed == BulkAction::Delete, "delete arms on the first press");
    check(arm.press(BulkAction::Delete) && !arm.armed, "delete acts on the second press");
    arm.press(BulkAction::Delete);
    check(arm.press(BulkAction::Show) && !arm.press(BulkAction::Delete), "another press disarms delete");
    arm.disarm();
    check(!arm.press(BulkAction::Delete), "a selection change disarms delete");
    check(selectionKey(0x41, true, false) == SelectionKey::SelectAll && selectionKey(0x41, false, true) == SelectionKey::None,
        "Ctrl + A selects all, A alone does not");
    check(selectionKey(0x1b, false, true) == SelectionKey::Clear && selectionKey(0x1b, false, false) == SelectionKey::None,
        "Esc clears several rows first, then is left to the screen");

    auto all = namesFit(4, 50, 12);
    check(all.names == 4 && all.more == 0, "names that fit are all listed");
    auto exact = namesFit(4, 48, 12);
    check(exact.names == 4 && exact.more == 0, "names that just fit need no more line");
    auto cut = namesFit(10, 50, 12);
    check(cut.names == 3 && cut.more == 7, "names that do not fit leave the last line to N more");
    auto one = namesFit(5, 12, 12);
    check(one.names == 0 && one.more == 5, "one line holds only N more");
    auto noLine = namesFit(5, 11, 12);
    check(noLine.names == 0 && noLine.more == 0, "no line, no names and no more line");
    check(namesFit(0, 50, 12).names == 0 && namesFit(3, 50, 0).names == 0, "nothing selected or no line height lists nothing");

    SelectionPaneLayout wide;
    placeSelectionButtons(wide, bulkActions(SelectionList::Shapes), 106, 56, 4, 278, 96);
    check(wide.buttonX == std::vector<float>{106, 166, 278} && wide.buttonW == std::vector<float>{56, 56, 96},
        "on a wide pane Show and Hide sit where the first actions do, Delete where the pane's own Delete does");
    SelectionPaneLayout narrow;
    placeSelectionButtons(narrow, bulkActions(SelectionList::Shapes), 6, 56, 4, 88, 96);
    check(narrow.buttonX[2] == 88 && narrow.buttonW[2] == 96 && narrow.buttonX[1] + narrow.buttonW[1] + 4 <= 88
        && narrow.buttonW[0] == narrow.buttonW[1] && narrow.buttonX[1] == narrow.buttonX[0] + narrow.buttonW[0] + 4,
        "on a narrow pane Show and Hide narrow to stay clear of Delete, which keeps its place");
    narrow.buttonsY = 100;
    narrow.buttonH = 12;
    check(narrow.at(narrow.buttonX[1] + 1, 105) == 1 && narrow.at(90, 105) == 2 && narrow.at(90, 113) == -1
        && narrow.at(narrow.buttonX[0] + narrow.buttonW[0] + 2, 105) == -1, "a press finds its button and only its button");
    SelectionPaneLayout map;
    placeSelectionButtons(map, bulkActions(SelectionList::MapPanel), 0, 30, 3, 0, 0);
    check(map.buttonX == std::vector<float>{0, 33} && map.buttonW == std::vector<float>{30, 30}, "without Delete the buttons keep their width");
}
