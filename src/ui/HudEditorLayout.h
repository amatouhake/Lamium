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
using Boxes = std::array<std::optional<Box>, 13>; // Indexed by HudElementId.
// InfoHud draws the minimap, durability, magnification, status, target, toast, then info; later ones are on top.
inline constexpr std::array<HudElementId, 13> drawOrder{
    HudElementId::Chat, HudElementId::Minimap, HudElementId::Schematic, HudElementId::Inventory, HudElementId::UsedSlots, HudElementId::StatusEffects, HudElementId::Durability, HudElementId::Magnification, HudElementId::Status, HudElementId::Target, HudElementId::Toast,
    HudElementId::Info, HudElementId::PlayerList};

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
// Every element under the pointer, topmost first. A click on the selected
// element's place selects the next one below it, so covered elements can be
// reached (L-321).
inline std::vector<HudElementId> stackAt(Boxes const& boxes, float px, float py) {
    std::vector<HudElementId> hits;
    for (auto it = drawOrder.rbegin(); it != drawOrder.rend(); ++it) {
        auto const& box = boxes[static_cast<size_t>(*it)];
        if (box && box->contains(px, py)) hits.push_back(*it);
    }
    return hits;
}
inline std::optional<HudElementId> nextBelow(std::vector<HudElementId> const& stack, HudElementId current) {
    for (size_t i = 0; i < stack.size(); ++i)
        if (stack[i] == current) return stack[(i + 1) % stack.size()];
    return stack.empty() ? std::nullopt : std::optional(stack.front());
}

// Scale handles (L-321). An element grows away from its anchor point, which
// stays put while it is scaled, so only the corners away from that point get
// a handle: one for a corner anchor, two for an edge, four for the center.
inline constexpr float handleSize = 5;
struct Handle { Box box; Point corner; };
inline std::vector<Handle> scaleHandles(Anchor anchor, Box element, float screenW, float screenH) {
    std::vector<Handle> handles;
    auto f = anchorFactors(anchor);
    for (int c = 0; c < 4; ++c) {
        float cx = static_cast<float>(c % 2), cy = static_cast<float>(c / 2);
        if (cx == f.x || cy == f.y) continue;
        Point corner{element.x + element.w * cx, element.y + element.h * cy};
        float x = std::clamp(corner.x - handleSize / 2, 0.f, std::max(0.f, screenW - handleSize));
        float y = std::clamp(corner.y - handleSize / 2, 0.f, std::max(0.f, screenH - handleSize));
        handles.push_back({{x, y, handleSize, handleSize}, corner});
    }
    return handles;
}
// A handle is easier to grab than it looks.
inline bool grabs(Handle const& handle, float px, float py, float reach = 2) {
    return px >= handle.box.x - reach && px < handle.box.x + handle.box.w + reach
        && py >= handle.box.y - reach && py < handle.box.y + handle.box.h + reach;
}
// The scale a handle drag asks for: how far the pointer is from the anchor
// point compared with where the handle started, on the axis that moved more,
// in steps.
inline float dragScale(float startScale, Point anchor, Point handle, Point pointer) {
    float dx = std::abs(handle.x - anchor.x), dy = std::abs(handle.y - anchor.y);
    float ratio = std::max(dx > 0 ? std::abs(pointer.x - anchor.x) / dx : 0.f,
                           dy > 0 ? std::abs(pointer.y - anchor.y) / dy : 0.f);
    if (!std::isfinite(ratio) || !(ratio > 0)) return clampHudScale(startScale);
    return clampHudScale(std::round(clampHudScale(startScale) * ratio / hudScaleStep) * hudScaleStep);
}
// "Reset layout" and "Done" sit in a small titled panel above the crosshair,
// shown only while nothing is selected (L-321): the screen's middle is where
// elements rarely are.
inline Box idlePanel(float w, float h, float screenW, float screenH) {
    float x = std::max(0.f, (screenW - w) / 2);
    return {std::round(x), std::round(std::clamp(screenH / 2 - h - 12, 0.f, std::max(0.f, screenH - h))), w, h};
}
// A framed element's name sits on a plate in the middle of its frame, kept
// on the screen: a plate may be wider than a small frame.
inline Box namePlate(Box frame, float w, float h, float screenW, float screenH) {
    float x = frame.x + (frame.w - w) / 2, y = frame.y + (frame.h - h) / 2;
    return {std::round(std::clamp(x, 0.f, std::max(0.f, screenW - w))),
            std::round(std::clamp(y, 0.f, std::max(0.f, screenH - h))), w, h};
}
// A dashed outline for an element shown as a frame only: the top-left of
// each one-unit-thick dash and whether it runs horizontally.
struct Dash { float x, y, length; bool horizontal; };
inline std::vector<Dash> dashedFrame(Box box, float on = 3, float off = 2) {
    std::vector<Dash> out;
    if (!(box.w > 0) || !(box.h > 0) || !(on > 0) || !(off >= 0)) return out;
    for (float x = 0; x < box.w && out.size() < 600; x += on + off) {
        float length = std::min(on, box.w - x);
        out.push_back({box.x + x, box.y, length, true});
        out.push_back({box.x + x, box.y + box.h - 1, length, true});
    }
    for (float y = 0; y < box.h && out.size() < 1200; y += on + off) {
        float length = std::min(on, box.h - y);
        out.push_back({box.x, box.y + y, length, false});
        out.push_back({box.x + box.w - 1, box.y + y, length, false});
    }
    return out;
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
// A scrolled list's bar: `count` rows, `visible` of them from `first`, on a
// track of `track` units from `top`. The thumb, and the first row a pointer
// at `y` asks for (the thumb's middle follows the pointer).
struct ScrollThumb { float top = 0, h = 0; };
inline ScrollThumb scrollThumb(float top, float track, int count, int visible, int first) {
    if (count <= visible || visible <= 0) return {top, track};
    float h = std::max(6.f, track * visible / count);
    return {top + (track - h) * first / (count - visible), h};
}
inline int scrollFirstAt(float y, float top, float track, int count, int visible) {
    if (count <= visible || visible <= 0) return 0;
    float h = scrollThumb(top, track, count, visible, 0).h;
    float room = track - h;
    if (!(room > 0)) return 0;
    float t = std::clamp((y - top - h / 2) / room, 0.f, 1.f);
    return static_cast<int>(std::lround(t * (count - visible)));
}
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
