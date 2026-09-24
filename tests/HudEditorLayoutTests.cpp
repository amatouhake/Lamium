#include "ui/HudEditorLayout.h"
void check(bool, char const*);
void hudEditorLayoutTests() {
    using namespace lamium::ui;
    using namespace lamium::ui::hud_editor;
    auto corner = anchorPoint(Anchor::BottomRight, 640, 360);
    check(corner.x == 636 && corner.y == 356, "anchor points share the element margin");
    auto center = anchorPoint(Anchor::Center, 640, 360);
    check(center.x == 320 && center.y == 180, "the center anchor is the screen center");
    Box box{100, 50, 40, 20};
    auto point = elementPoint(Anchor::BottomRight, box);
    check(point.x == 140 && point.y == 70, "the guide ends at the element's matching corner");
    auto dots = dashes({0, 0}, {0, 20}, 4);
    check(dots.size() == 6 && dots.back().y == 20, "dashed guides reach both ends");
    check(dashes({0, 0}, {0, 0}).size() == 1, "a zero-length guide is one dot");
    auto dragged = dragBox(5, 5, 10, 10, 40, 20, 640, 360);
    check(dragged.x == 0 && dragged.y == 0, "dragging clamps to the screen");
    dragged = dragBox(300, 200, 10, 5, 40, 20, 640, 360);
    check(dragged.x == 290 && dragged.y == 195, "the grab point follows the pointer");
    Boxes boxes;
    boxes[static_cast<size_t>(HudElementId::Status)] = Box{0, 0, 100, 100};
    boxes[static_cast<size_t>(HudElementId::Info)] = Box{50, 50, 100, 100};
    check(topmost(boxes, 60, 60) == HudElementId::Info, "the element drawn last wins the hit");
    check(topmost(boxes, 10, 10) == HudElementId::Status, "uncovered elements stay hittable");
    check(!topmost(boxes, 300, 300), "empty space hits nothing");

    auto right = fitPanel(640, 360, std::nullopt, 40, 0);
    check(right.x == 640 - Panel::width - 8, "the panel defaults to the right side");
    auto left = fitPanel(640, 360, Box{500, 100, 60, 20}, 5, 0);
    check(left.x == 8, "the panel moves away from a selected element on the right");
    check(right.visible < 40 && right.y + right.h <= 352, "long panels scroll inside the screen");
    auto scrolled = fitPanel(640, 360, std::nullopt, 40, 100);
    check(scrolled.first == 40 - scrolled.visible, "panel scrolling stops at the last row");
    auto small = fitPanel(640, 360, std::nullopt, 3, 0);
    auto row = hitPanel(small, small.x + 10, small.rowY(1) + 2);
    check(row.zone == PanelHit::Zone::Row && row.index == 1 && row.part == 2, "row labels are hit by index");
    auto dec = hitPanel(small, small.controlX() + 2, small.rowY(0) + 2);
    auto inc = hitPanel(small, small.controlX() + Panel::controlWidth - 2, small.rowY(0) + 2);
    check(dec.part == -1 && inc.part == 1, "row controls split into decrease and increase");
    auto button = hitPanel(small, small.buttonX(0) + 2, small.buttonY() + 2);
    check(button.zone == PanelHit::Zone::Button && button.index == 0, "footer buttons are hit by index");
    check(hitPanel(small, small.x - 1, small.y + 1).zone == PanelHit::Zone::None, "outside the panel hits nothing");
}
