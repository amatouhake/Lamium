#include "ui/HudElement.h"
void check(bool, char const*);
void hudElementTests() {
    using namespace lamium::ui;
    check(defaultHudElement(HudElementId::Info).anchor == Anchor::TopLeft
          && defaultHudElement(HudElementId::Target).anchor == Anchor::TopCenter
          && defaultHudElement(HudElementId::Target).background == ElementBackground::Card
          && !defaultHudElement(HudElementId::Target).shadow
          && defaultHudElement(HudElementId::Status).anchor == Anchor::MiddleRight
          && defaultHudElement(HudElementId::Status).dy == -20
          && defaultHudElement(HudElementId::Toast).anchor == Anchor::BottomCenter
          && defaultHudElement(HudElementId::Toast).dy == -48
          && defaultHudElement(HudElementId::Toast).background == ElementBackground::Card,
          "element defaults match the demo");
    HudElement plain;
    auto at = [&](Anchor anchor) {
        HudElement element;
        element.anchor = anchor;
        return placeElement(800, 600, 100, 50, element);
    };
    check(at(Anchor::TopLeft).x == 4 && at(Anchor::TopLeft).y == 4, "top-left origin");
    check(at(Anchor::TopCenter).x == 350 && at(Anchor::TopCenter).y == 4, "top-center origin");
    check(at(Anchor::TopRight).x == 696 && at(Anchor::TopRight).y == 4, "top-right origin");
    check(at(Anchor::MiddleLeft).x == 4 && at(Anchor::MiddleLeft).y == 275, "middle-left origin");
    check(at(Anchor::Center).x == 350 && at(Anchor::Center).y == 275, "center origin");
    check(at(Anchor::MiddleRight).x == 696 && at(Anchor::MiddleRight).y == 275, "middle-right origin");
    check(at(Anchor::BottomLeft).x == 4 && at(Anchor::BottomLeft).y == 546, "bottom-left origin");
    check(at(Anchor::BottomCenter).x == 350 && at(Anchor::BottomCenter).y == 546, "bottom-center origin");
    check(at(Anchor::BottomRight).x == 696 && at(Anchor::BottomRight).y == 546, "bottom-right origin");
    for (auto anchor : {Anchor::TopLeft, Anchor::TopCenter, Anchor::TopRight, Anchor::MiddleLeft, Anchor::Center,
                        Anchor::MiddleRight, Anchor::BottomLeft, Anchor::BottomCenter, Anchor::BottomRight}) {
        HudElement small, large;
        small.anchor = large.anchor = anchor;
        auto a = placeElement(800, 600, 100, 50, small);
        auto b = placeElement(800, 600, 200, 110, large);
        auto factors = anchorFactors(anchor);
        check(a.x + factors.x * 100 == b.x + factors.x * 200
              && a.y + factors.y * 50 == b.y + factors.y * 110,
              "the anchor point stays put when the element grows");
    }
    HudElement pushed;
    pushed.dx = 10000;
    pushed.dy = -10000;
    auto clamped = placeElement(800, 600, 100, 50, pushed);
    check(clamped.x == 700 && clamped.y == 0, "offsets clamp to the screen");
    check(placeElement(800, 600, 900, 700, plain).x == 0
          && placeElement(800, 600, 900, 700, plain).y == 0, "oversized elements pin to the origin");
    check(placeElement(0, 600, 100, 50, plain).x == 0 && placeElement(800, 600, -1, 50, plain).x == 0,
          "invalid geometry falls back to the origin");
    auto drop = resolveDrag(353, 277, 100, 50, 800, 600, plain);
    check(drop.anchor == Anchor::Center && drop.dx == 0 && drop.dy == 0, "center drop picks the center anchor");
    HudElement pinned;
    pinned.pinned = true;
    pinned.anchor = Anchor::TopRight;
    auto kept = resolveDrag(100, 500, 100, 50, 800, 600, pinned);
    check(kept.anchor == Anchor::TopRight && kept.dx == 100 - 696 && kept.dy == 500 - 4,
          "pinned drags move only the offset");
    auto keptOffset = resolveDrag(360, 275, 100, 50, 800, 600, plain);
    check(keptOffset.anchor == Anchor::Center && keptOffset.dx == 10 && keptOffset.dy == 0,
          "large offsets survive while small ones snap to zero");
    auto rounded = resolveDrag(356.6f, 275.4f, 100, 50, 800, 600, plain);
    check(rounded.dx == 7 && rounded.dy == 0, "drag offsets round to whole units");
    check(nearestAnchor(0, 0, 800, 600) == Anchor::TopLeft
          && nearestAnchor(799, 599, 800, 600) == Anchor::BottomRight
          && nearestAnchor(400, 300, 800, 600) == Anchor::Center, "nearest anchor uses screen thirds");
    check(scaledExtent(100, 100) == 100 && scaledExtent(100, 50) == 75 && scaledExtent(100, 200) == 150,
          "scale clamps to its bounds");
}
