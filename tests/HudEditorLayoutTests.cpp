#include "ui/HudEditorLayout.h"
void check(bool, char const*);
void hudEditorLayoutTests() {
    using namespace lamium::ui;
    using namespace lamium::ui::hud_editor;
    auto corner = anchorPoint(Anchor::BottomRight, 640, 360);
    check(corner.x == 640 && corner.y == 360, "anchor points sit on the screen edge");
    auto center = anchorPoint(Anchor::Center, 640, 360);
    check(center.x == 320 && center.y == 180, "the center anchor is the screen center");
    Box box{100, 50, 40, 20};
    auto point = elementPoint(Anchor::BottomRight, box);
    check(point.x == 140 && point.y == 70, "the guide ends at the element's matching corner");
    auto dots = dashes({0, 0}, {0, 20}, 4);
    check(dots.size() == 6 && dots.back().y == 20, "dashed guides reach both ends");
    check(dashes({0, 0}, {0, 0}).size() == 1, "a zero-length guide is one dot");

    auto dragged = dragBox(5, 5, 10, 10, 40, 20, 640, 360);
    check(dragged.box.x == 0 && dragged.box.y == 0 && dragged.lineX == 0.f, "dragging clamps and snaps flush");
    dragged = dragBox(300, 200, 10, 5, 40, 20, 640, 360);
    check(dragged.box.x == 290 && dragged.box.y == 195 && !dragged.lineX && !dragged.lineY,
          "the grab point follows the pointer in free space");
    dragged = dragBox(15, 100, 10, 5, 40, 20, 640, 360);
    check(dragged.box.x == hudInset && dragged.lineX == hudInset, "drags snap to the inset");

    Boxes boxes;
    boxes[static_cast<size_t>(HudElementId::Status)] = Box{0, 0, 100, 100};
    boxes[static_cast<size_t>(HudElementId::Info)] = Box{50, 50, 100, 100};
    check(topmost(boxes, 60, 60) == HudElementId::Info, "the element drawn last wins the hit");
    check(topmost(boxes, 10, 10) == HudElementId::Status, "uncovered elements stay hittable");
    check(!topmost(boxes, 300, 300), "empty space hits nothing");

    auto under = toolbarSpot(Box{10, 10, 80, 40}, 120, 14, 640, 360);
    check(under.below && under.x == 10 && under.y == 10 + 40 + toolbarGap, "the toolbar sits under the element");
    auto over = toolbarSpot(Box{10, 330, 80, 28}, 120, 14, 640, 360);
    check(!over.below && over.y == 330 - toolbarGap - 14, "no room below puts the toolbar above");
    auto edge = toolbarSpot(Box{600, 10, 40, 20}, 120, 14, 640, 360);
    check(edge.x == 640 - 120 - 2, "the toolbar stays on screen at the right edge");
    auto pop = popoverSpot(under, 14, 90, 60, 640, 360);
    check(pop.y == under.y + 14 + 2 && pop.h == 60, "popovers open below a toolbar that is below");
    auto popUp = popoverSpot(over, 14, 90, 60, 640, 360);
    check(popUp.y == over.y - 2 - 60 && popUp.h == 60, "popovers open above a toolbar that is above");
    Spot low{10, 300, true};
    auto flipped = popoverSpot(low, 14, 90, 120, 640, 360);
    check(flipped.y + flipped.h <= low.y && flipped.h == 120, "a popover that does not fit below opens above");
    Spot middle{10, 170, true};
    auto squeezed = popoverSpot(middle, 14, 90, 400, 640, 360);
    check(squeezed.y >= middle.y + 14 && squeezed.y + squeezed.h <= 360 && squeezed.h < 400,
          "a tall popover shrinks instead of covering the toolbar");
    check(Box{0, 0, 10, 10}.overlaps(Box{5, 5, 10, 10}) && !Box{0, 0, 10, 10}.overlaps(Box{10, 0, 5, 5}),
          "box overlap excludes touching edges");
}
