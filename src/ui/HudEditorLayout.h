#pragma once
#include "ui/HudElement.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace lamium::ui::hud_editor {
// Geometry of the HUD layout editor (BACKLOG L-04c, docs/demos/hud-editor.html).
// Pure: the editor glue draws and feeds pointer positions in GUI units.
struct Point { float x = 0, y = 0; };
struct Box {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    bool overlaps(Box const& o) const { return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h; }
};
using Boxes = std::array<std::optional<Box>, 4>; // Indexed by HudElementId.
// InfoHud draws status, target, toast, then info; later ones are on top.
inline constexpr std::array<HudElementId, 4> drawOrder{
    HudElementId::Status, HudElementId::Target, HudElementId::Toast, HudElementId::Info};

// The anchor point on the screen (anchors sit on the screen edge).
inline Point anchorPoint(Anchor anchor, float screenW, float screenH) {
    auto f = anchorFactors(anchor);
    return {screenW * f.x, screenH * f.y};
}
// The point of the element that stays on its anchor when the element grows.
inline Point elementPoint(Anchor anchor, Box box) {
    auto f = anchorFactors(anchor);
    return {box.x + box.w * f.x, box.y + box.h * f.y};
}
// Dots for the dashed anchor guide, from the anchor to the element point.
inline std::vector<Point> dashes(Point from, Point to, float step = 4) {
    std::vector<Point> points;
    float dx = to.x - from.x, dy = to.y - from.y;
    float length = std::hypot(dx, dy);
    if (!std::isfinite(length) || !(step > 0)) return points;
    int count = static_cast<int>(length / step);
    for (int i = 0; i <= std::min(count, 400); ++i) {
        float t = count ? static_cast<float>(i) / count : 0;
        points.push_back({from.x + dx * t, from.y + dy * t});
    }
    return points;
}
// Element box while dragging: the grab point follows the pointer, clamped,
// then pulled onto the edge, the inset or the center lines.
struct Drop { Box box; std::optional<float> lineX, lineY; };
inline Drop dragBox(float px, float py, float grabX, float grabY, float w, float h, float screenW, float screenH) {
    float x = std::clamp(px - grabX, 0.f, std::max(0.f, screenW - w));
    float y = std::clamp(py - grabY, 0.f, std::max(0.f, screenH - h));
    auto mx = magnet(x, w, screenW), my = magnet(y, h, screenH);
    return {{mx.position, my.position, w, h}, mx.line, my.line};
}
// The element under the pointer; the one drawn last wins.
inline std::optional<HudElementId> topmost(Boxes const& boxes, float px, float py) {
    for (auto it = drawOrder.rbegin(); it != drawOrder.rend(); ++it) {
        auto const& box = boxes[static_cast<size_t>(*it)];
        if (box && box->contains(px, py)) return *it;
    }
    return std::nullopt;
}

// The element toolbar sits under the element, or above it when there is no
// room below. The caller keeps the spot while only the look changes and asks
// again after a move or when the element grows over it.
struct Spot { float x = 0, y = 0; bool below = true; };
inline constexpr float toolbarGap = 5;
inline Spot toolbarSpot(Box element, float w, float h, float screenW, float screenH) {
    bool below = element.y + element.h + toolbarGap + h <= screenH;
    float x = std::clamp(element.x, 2.f, std::max(2.f, screenW - w - 2));
    float y = below ? element.y + element.h + toolbarGap : element.y - toolbarGap - h;
    return {x, std::clamp(y, 2.f, std::max(2.f, screenH - h - 2)), below};
}
// Popovers never cover their toolbar: they open on the far side from the
// element when the content fits there, otherwise on the roomier side, and
// shrink to the room they get (the caller scrolls what does not fit).
struct PopoverFit { float x = 0, y = 0, h = 0; };
inline PopoverFit popoverSpot(Spot toolbar, float toolbarH, float w, float h, float screenW, float screenH) {
    float x = std::clamp(toolbar.x, 2.f, std::max(2.f, screenW - w - 2));
    float roomBelow = std::max(0.f, screenH - (toolbar.y + toolbarH + 2) - 2);
    float roomAbove = std::max(0.f, toolbar.y - 2 - 2);
    bool preferBelow = toolbar.below ? roomBelow >= h || roomBelow >= roomAbove : !(roomAbove >= h || roomAbove >= roomBelow);
    float height = std::min(h, preferBelow ? roomBelow : roomAbove);
    float y = preferBelow ? toolbar.y + toolbarH + 2 : toolbar.y - 2 - height;
    return {x, y, height};
}
}
