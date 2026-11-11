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
    // The lines list's scroll bar (L-221): 24 rows, 10 shown, a 120-unit track.
    auto thumb = scrollThumb(100, 120, 24, 10, 0);
    check(thumb.top == 100 && std::abs(thumb.h - 50) < .01f, "the thumb is as tall as the shown share of the list");
    auto last = scrollThumb(100, 120, 24, 10, 14);
    check(std::abs(last.top + last.h - 220) < .01f, "at the last page the thumb reaches the track's end");
    check(scrollFirstAt(90, 100, 120, 24, 10) == 0 && scrollFirstAt(400, 100, 120, 24, 10) == 14
          && scrollFirstAt(160, 100, 120, 24, 10) == 7, "the pointer maps the track to every first row, clamped");
    check(scrollFirstAt(160, 100, 120, 8, 10) == 0, "a list that fits does not scroll");

    // L-321: covered elements, scale handles and the idle panel.
    Boxes stacked;
    stacked[static_cast<size_t>(HudElementId::Target)] = Box{100, 0, 80, 30};
    stacked[static_cast<size_t>(HudElementId::PlayerList)] = Box{110, 0, 60, 60};
    stacked[static_cast<size_t>(HudElementId::Info)] = Box{0, 0, 50, 40};
    auto hits = stackAt(stacked, 120, 10);
    check(hits.size() == 2 && hits[0] == HudElementId::PlayerList && hits[1] == HudElementId::Target,
          "the stack under the pointer lists the topmost element first");
    check(nextBelow(hits, HudElementId::PlayerList) == HudElementId::Target
          && nextBelow(hits, HudElementId::Target) == HudElementId::PlayerList,
          "a second click reaches the element below and wraps back to the top");
    check(nextBelow(hits, HudElementId::Info) == HudElementId::PlayerList && !nextBelow({}, HudElementId::Info),
          "an element not under the pointer starts from the top");

    Box scaled{10, 10, 100, 40};
    auto cornerHandles = scaleHandles(Anchor::TopLeft, scaled, 640, 360);
    check(cornerHandles.size() == 1 && cornerHandles[0].corner.x == 110 && cornerHandles[0].corner.y == 50,
          "a corner anchor gets one handle, on the opposite corner");
    check(scaleHandles(Anchor::TopCenter, scaled, 640, 360).size() == 2
          && scaleHandles(Anchor::Center, scaled, 640, 360).size() == 4,
          "an edge anchor gets two handles and the center four");
    auto atEdge = scaleHandles(Anchor::TopLeft, Box{600, 330, 40, 30}, 640, 360);
    check(atEdge.size() == 1 && atEdge[0].box.x == 640 - handleSize && atEdge[0].box.y == 360 - handleSize,
          "a handle at the screen edge stays on the screen");
    check(grabs(cornerHandles[0], 106, 46) && !grabs(cornerHandles[0], 100, 40), "a handle is grabbed a little outside its square");
    Point anchorAt{10, 10}, handleAt{110, 50};
    check(dragScale(100, anchorAt, handleAt, {160, 50}) == 150, "dragging a handle away from the anchor enlarges");
    check(dragScale(100, anchorAt, handleAt, {62, 30}) == 50, "dragging toward the anchor shrinks, in steps");
    check(dragScale(100, anchorAt, handleAt, {1000, 50}) == hudScaleMax && dragScale(100, anchorAt, handleAt, {10, 10}) == 100,
          "a handle drag stays within the scale bounds and ignores a pointer on the anchor");
    check(dragScale(100, anchorAt, handleAt, {113, 51}) == 105, "the axis that moved more decides, rounded to the step");

    auto idle = idlePanel(100, 40, 640, 360);
    check(idle.x == 270 && idle.y == 128 && idle.w == 100, "the idle panel is centered above the crosshair");
    check(idlePanel(100, 40, 80, 30).x == 0 && idlePanel(100, 40, 80, 30).y == 0, "a tiny screen keeps the panel on it");
    auto dashed = dashedFrame(Box{0, 0, 10, 5});
    check(dashed.size() == 6 && dashed[0].horizontal && dashed[0].length == 3 && !dashed[4].horizontal,
          "a dashed frame runs along all four edges");
    check(dashedFrame(Box{0, 0, 0, 5}).empty(), "an empty box has no frame");
}
