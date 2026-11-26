#include "features/schematic/SchematicsRows.h"
#include "ui/SettingsTable.h"
#include <cmath>
void check(bool, char const*);
void schematicsRowsTests() {
    using namespace lamium::ui;
    using namespace lamium::ui::schematics_rows;
    check(materialList({}).empty(), "no lines, no rows");
    check(materialList({false, false}) == std::vector<int>{0, 1} && materialList({true}) == std::vector<int>{0},
          "one kind alone is listed without a heading");
    check(materialList({false, false, true}) == std::vector<int>{blocksHeading, 0, 1, entitiesHeading, 2},
          "blocks and entities each stand under their heading");
    check(materialList({false, true, true}) == std::vector<int>{blocksHeading, 0, entitiesHeading, 1, 2},
          "a heading starts where the kind changes");

    auto table = SettingsTable::fit(640, 360, 20, 0);
    auto l = ShapesLayout::fit(table, 640, 360, false, 80, 0, 16, 0, false, false);
    constexpr int tabs = 4;
    float w = tabWidth(l, tabs), y = l.toolbarTop + 1;
    check(l.usable() && std::abs(w * tabs - (l.listWidth - 2 * ShapesLayout::pad)) < 1e-3f && tabX(l, 0, tabs) == l.listLeft + ShapesLayout::pad,
          "the tabs fill the list pane inside its padding, side by side");
    check(tabAt(l, tabX(l, 0, tabs) + 1, y, tabs) == 0 && tabAt(l, tabX(l, 2, tabs) + 1, y, tabs) == 2
              && tabAt(l, tabX(l, 3, tabs) + w - .5f, y, tabs) == 3, "a point on a tab names it");
    check(tabAt(l, tabX(l, 0, tabs) - 1, y, tabs) == -1 && tabAt(l, tabX(l, 3, tabs) + w + 1, y, tabs) == -1,
          "the padding beside the strip is no tab");
    check(tabAt(l, tabX(l, 1, tabs) + 1, l.toolbarTop - 1, tabs) == -1
              && tabAt(l, tabX(l, 1, tabs) + 1, l.toolbarTop + ShapesLayout::toolbarHeight, tabs) == -1, "above and below the toolbar is no tab");
    check(tabAt(ShapesLayout{}, 10, 10, tabs) == -1, "a layout too small to use has no tabs");

    // The lists' columns, as the view placed them before they moved here.
    auto wide = listPlaces(100, 500);
    check(wide.blocksX == 500 - ShapesLayout::pad - 40 && wide.sizeX == wide.blocksX - 4 - 54 && wide.showSize,
          "a wide Files list shows the size left of the block count");
    check(wide.carriedX == 500 - ShapesLayout::pad - 34 && wide.leftX == wide.carriedX - 36 && wide.showPlaced
              && wide.placedX == wide.leftX - 36 && wide.neededX == wide.placedX - 36 && wide.distW == 28,
          "a wide Materials list shows needed, placed, left and carried from the right");
    auto middle = listPlaces(100, 300);
    check(middle.showSize && !middle.showPlaced && middle.neededX == middle.leftX - 36,
          "the placed count goes first; needed then stands next to left");
    auto narrow = listPlaces(100, 260);
    check(!narrow.showSize && !narrow.showPlaced, "a narrow list drops the size too");
    auto edge = listPlaces(0, 80 + 4 + 54 + 40 + ShapesLayout::pad);
    check(edge.showSize && !listPlaces(1, 80 + 4 + 54 + 40 + ShapesLayout::pad).showSize, "the size shows from 80 units of name room");

    // The preview's own turn stays smooth however long the machine has been up.
    check(autoTurn(0) == 0 && std::abs(autoTurn(1) - 12) < 1e-4f && std::abs(autoTurn(30.5) - 6) < 1e-3f, "12 degrees a second, once round in 30 s");
    double const upNineDays = 9 * 86400.0 + .25;
    float const step = autoTurn(upNineDays + 1.0 / 60) - autoTurn(upNineDays);
    check(std::abs(step - .2f) < 1e-3f, "after nine days of uptime one 60 Hz frame still turns it a fifth of a degree");
    float const asFloat = std::fmod(static_cast<float>(upNineDays + 1.0 / 60) * 12.f, 360.f) - std::fmod(static_cast<float>(upNineDays) * 12.f, 360.f);
    check(asFloat == 0, "as a float second count the same frame does not turn it at all");

    // A click on the preview, as the view tested its last drawn box.
    Box const preview{100, 50, 120, 80};
    check(previewPress(preview, true, 100, 50) == PreviewPress::Slice && previewPress(preview, true, 219.5f, 65.5f) == PreviewPress::Slice,
          "while sliced the top 16 units across the preview show it whole");
    check(previewPress(preview, true, 150, 66) == PreviewPress::Drag && previewPress(preview, false, 150, 55) == PreviewPress::Drag,
          "below the slice line, or anywhere while whole, a press drags");
    check(previewPress(preview, true, 220, 60) == PreviewPress::None && previewPress(preview, false, 150, 130) == PreviewPress::None
              && previewPress(preview, false, 99.5f, 60) == PreviewPress::None && previewPress(preview, false, 150, 49.5f) == PreviewPress::None,
          "the right and bottom edges and what is above or left of it are outside");
    check(previewPress({100, 50, 120, 10}, true, 150, 62) == PreviewPress::Slice, "the slice line reaches 16 units down even past a shorter preview");
    check(previewPress({100, 50, 0, 80}, true, 100, 50) == PreviewPress::None, "a preview not drawn takes no press");

    // A click on the Check tab's own boxes: "Ignore settings", the chips, the controls.
    CheckBoxes c;
    c.shown = c.checked = true;
    c.ignoreSettings = {300, 20, 60, 12};
    c.headTop = 30;
    c.rowsTop = 42;
    c.chips = {{{10, 40}, {44, 80}, {0, 0}, {84, 120}}};
    c.controls = {{10, 200, 50, 12}, {30, 200, 50, 12}, {300, 25, 60, 12}};
    auto at = [&](float x, float y) { return checkPress(c, x, y); };
    check(at(300, 20).target == CheckTarget::IgnoreSettings && at(359.5f, 31.5f).target == CheckTarget::IgnoreSettings,
          "a point on Ignore settings presses it");
    check(at(310, 30).target == CheckTarget::IgnoreSettings, "Ignore settings wins over a control under it");
    check(at(10, 30).target == CheckTarget::Chip && at(10, 30).index == 0 && at(79.5f, 41.5f).index == 1 && at(84, 35).index == 3,
          "a point on a chip names it");
    check(at(42, 35).target == CheckTarget::None && at(0, 35).target == CheckTarget::None, "between chips and on a chip not drawn is nothing");
    check(at(10, 29.5f).target == CheckTarget::None && at(10, 42).target == CheckTarget::None, "a chip counts only inside the heading row");
    check(at(35, 205).target == CheckTarget::Control && at(35, 205).index == 0 && at(70, 205).index == 1,
          "overlapping controls go to the first drawn");
    check(at(80, 205).target == CheckTarget::None && at(10, 212).target == CheckTarget::None, "a control's right and bottom edges are outside");
    check(at(320, 36).target == CheckTarget::Control && at(320, 36).index == 2, "a control below Ignore settings is still pressed");

    auto layered = c;
    layered.controls.insert(layered.controls.begin(), Box{0, 30, 130, 12});
    layered.ignoreSettings = {100, 30, 40, 12};
    check(checkPress(layered, 20, 35).target == CheckTarget::Chip, "a chip wins over a control under it");
    check(checkPress(layered, 110, 35).target == CheckTarget::IgnoreSettings, "Ignore settings wins over a chip under it");

    auto view = c;
    view.ignoreView = true;
    view.controls.push_back({10, 30, 30, 10});
    check(checkPress(view, 10, 30).target == CheckTarget::Control && checkPress(view, 10, 30).index == 3,
          "in the ignore view the chips are gone and a control under them is pressed");
    auto unchecked = c;
    unchecked.checked = false;
    check(checkPress(unchecked, 310, 30).target == CheckTarget::Control && checkPress(unchecked, 310, 30).index == 2,
          "without a checked placement Ignore settings takes no press");
    auto undrawn = c;
    undrawn.ignoreSettings = {};
    check(checkPress(undrawn, 0, 0).target == CheckTarget::None, "Ignore settings not drawn takes no press");
    auto hidden = c;
    hidden.shown = false;
    check(checkPress(hidden, 300, 20).target == CheckTarget::None && checkPress(hidden, 10, 30).target == CheckTarget::None
              && checkPress(hidden, 35, 205).target == CheckTarget::None, "another tab shown takes none of the Check boxes");
}
