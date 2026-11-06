#include "ui/RowSelection.h"
void check(bool, char const*);
void rowSelectionTests() {
    using Rows = lamium::ui::RowSelection<int>;
    std::vector<int> const order{10, 20, 30, 40, 50};
    Rows rows;
    rows.only(20);
    check(rows.keys == std::vector<int>{20} && rows.contains(20) && !rows.contains(30), "a plain click selects one row");
    rows.toggle(40);
    check(rows.keys == std::vector<int>{20, 40}, "Ctrl + click adds a row");
    rows.toggle(20);
    check(rows.keys == std::vector<int>{40}, "Ctrl + click on a selected row removes it");
    rows.only(20);
    rows.range(40, order);
    check(rows.keys == std::vector<int>{20, 30, 40}, "Shift + click takes the rows between");
    rows.range(10, order);
    check(rows.keys == std::vector<int>{10, 20}, "a second range starts again from the same anchor");
    Rows empty;
    empty.range(30, order);
    check(empty.keys == std::vector<int>{30}, "a range without an anchor selects its row");
    rows.only(99);
    rows.range(30, order);
    check(rows.keys == std::vector<int>{30}, "a range from a row not in the list selects its row");
    rows.only(20);
    rows.range(40, order);
    rows.retain([](int key) { return key != 20; });
    check(rows.keys == std::vector<int>{30, 40} && !rows.anchor, "a removed row leaves the selection and the anchor");
    rows.clear();
    check(rows.keys.empty() && !rows.anchor, "clearing forgets the rows and the anchor");
}
