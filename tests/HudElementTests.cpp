#include "ui/HudElement.h"
void check(bool, char const*);
void hudElementTests() {
    using namespace lamium::ui;
    check(defaultHudElement(HudElementId::Info).anchor == Anchor::TopLeft
          && defaultHudElement(HudElementId::Info).dx == hudInset && defaultHudElement(HudElementId::Info).dy == hudInset
          && defaultHudElement(HudElementId::Target).anchor == Anchor::TopCenter
          && defaultHudElement(HudElementId::Target).background == ElementBackground::Card
          && !defaultHudElement(HudElementId::Target).shadow
          && defaultHudElement(HudElementId::Status).anchor == Anchor::MiddleRight
          && defaultHudElement(HudElementId::Status).dy == -20
          && defaultHudElement(HudElementId::Toast).anchor == Anchor::BottomCenter
          && defaultHudElement(HudElementId::Toast).dy == -72
          && defaultHudElement(HudElementId::Toast).background == ElementBackground::Card,
          "element defaults match the demo");
    HudElement plain;
    auto at = [&](Anchor anchor) {
        HudElement element;
        element.anchor = anchor;
        return placeElement(800, 600, 100, 50, element);
    };
    check(at(Anchor::TopLeft).x == 0 && at(Anchor::TopLeft).y == 0, "top-left sits flush without an offset");
    check(at(Anchor::TopCenter).x == 350 && at(Anchor::TopCenter).y == 0, "top-center origin");
    check(at(Anchor::MiddleRight).x == 700 && at(Anchor::MiddleRight).y == 275, "middle-right origin");
    check(at(Anchor::BottomRight).x == 700 && at(Anchor::BottomRight).y == 550, "bottom-right sits flush");
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

    auto low = placeAt(plain, 10, 500, 100, 50, 800, 600);
    check(low.anchor == Anchor::BottomLeft, "a drop in the lower left third anchors bottom-left");
    auto back = placeElement(800, 600, 100, 50, low);
    check(back.x == 10 && back.y == 500, "a drop stays exactly where it was dropped");
    auto grown = placeElement(800, 600, 100, 80, low);
    check(grown.y + 80 == 550, "a bottom-anchored element grows upward");
    auto rounded = placeAt(plain, 356.6f, 275.4f, 100, 50, 800, 600);
    check(rounded.anchor == Anchor::Center && rounded.dx == 7 && rounded.dy == 0, "drop offsets round to whole units");
    check(nearestAnchor(0, 0, 800, 600) == Anchor::TopLeft
          && nearestAnchor(799, 599, 800, 600) == Anchor::BottomRight
          && nearestAnchor(400, 300, 800, 600) == Anchor::Center, "nearest anchor uses screen thirds");

    auto corner = snapTo(plain, Anchor::BottomRight);
    auto snapped = placeElement(800, 600, 100, 50, corner);
    check(snapped.x == 700 - hudInset && snapped.y == 550 - hudInset, "snap-to keeps the inset from both edges");
    auto middle = snapTo(plain, Anchor::TopCenter);
    check(middle.dx == 0 && middle.dy == hudInset, "centered snap positions keep no horizontal offset");

    check(magnet(1, 100, 800).position == 0 && magnet(1, 100, 800).line == 0.f, "near the edge snaps flush");
    check(magnet(5, 100, 800).position == hudInset && magnet(5, 100, 800).line == hudInset,
          "near the inset snaps to the inset");
    check(magnet(697, 100, 800).position == 700 - hudInset, "the far inset snaps too");
    check(magnet(351, 100, 800).position == 350 && magnet(351, 100, 800).line == 400.f, "the center line snaps");
    check(!magnet(200, 100, 800).line && magnet(200, 100, 800).position == 200, "free space does not snap");
    check(scaledExtent(100, 100) == 100 && scaledExtent(100, 50) == 75 && scaledExtent(100, 200) == 150,
          "scale clamps to its bounds");
}
