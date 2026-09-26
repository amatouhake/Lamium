#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>

namespace lamium::ui {
// HUD element placement model (BACKLOG L-04a, DESIGN "HUD", docs/demos/hud.html).
// Elements are info lines, target, status, toast and the zoom magnification.
// Pure math; InfoHud draws. Append new ids: they index saved boxes.
enum class HudElementId { Info, Target, Status, Toast, Magnification };
enum class Anchor {
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight,
};
struct AnchorFactors { float x, y; };
inline constexpr AnchorFactors anchorFactors(Anchor anchor) {
    switch (anchor) {
    case Anchor::TopLeft: return {0, 0};
    case Anchor::TopCenter: return {.5f, 0};
    case Anchor::TopRight: return {1, 0};
    case Anchor::MiddleLeft: return {0, .5f};
    case Anchor::Center: return {.5f, .5f};
    case Anchor::MiddleRight: return {1, .5f};
    case Anchor::BottomLeft: return {0, 1};
    case Anchor::BottomCenter: return {.5f, 1};
    default: return {1, 1};
    }
}
enum class ElementBackground { None, Card };
// Placement is an anchor plus an offset, but the anchor is never chosen
// directly: where an element is dropped decides it (DESIGN "HUD").
struct HudElement {
    Anchor anchor = Anchor::TopLeft;
    float dx = 0, dy = 0; // GUI-unit offset from the anchor point.
    float scale = 100; // Percent, 75-150.
    ElementBackground background = ElementBackground::None;
    bool shadow = true;
};
// Gap from the screen edge used by defaults and snap-to; flush is allowed.
inline constexpr float hudInset = 4;
// Defaults match docs/demos/hud-editor.html.
inline constexpr HudElement defaultHudElement(HudElementId id) {
    switch (id) {
    case HudElementId::Info: return {Anchor::TopLeft, hudInset, hudInset, 100, ElementBackground::None, true};
    case HudElementId::Target: return {Anchor::TopCenter, 0, hudInset, 100, ElementBackground::Card, false};
    case HudElementId::Status: return {Anchor::MiddleRight, -hudInset, -20, 100, ElementBackground::None, true};
    // Small and away from the crosshair so it does not compete with the view.
    case HudElementId::Magnification: return {Anchor::Center, 0, 36, 75, ElementBackground::None, true};
    // Above the armor and absorption rows over the hotbar.
    default: return {Anchor::BottomCenter, 0, -72, 100, ElementBackground::Card, false};
    }
}
inline constexpr std::string_view hudElementKey(HudElementId id) {
    switch (id) {
    case HudElementId::Info: return "info";
    case HudElementId::Target: return "target";
    case HudElementId::Status: return "status";
    case HudElementId::Magnification: return "magnification";
    default: return "toast";
    }
}
inline float scaledExtent(float size, float scale) {
    if (!std::isfinite(size) || size < 0) return 0;
    return size * std::clamp(std::isfinite(scale) ? scale : 100.f, 75.f, 150.f) / 100;
}
struct ElementPlacement { float x, y; };
// Top-left of an element whose anchor point sits `factor` across the screen.
inline float anchorOrigin(float screen, float size, float factor) { return (screen - size) * factor; }
// The anchor point stays put when the element grows; clamp to the screen.
inline ElementPlacement placeElement(float screenW, float screenH, float elemW, float elemH,
                                     HudElement const& element) {
    if (!std::isfinite(screenW) || !std::isfinite(screenH) || screenW <= 0 || screenH <= 0
        || !std::isfinite(elemW) || !std::isfinite(elemH) || elemW < 0 || elemH < 0) return {};
    auto factors = anchorFactors(element.anchor);
    float dx = std::isfinite(element.dx) ? element.dx : 0;
    float dy = std::isfinite(element.dy) ? element.dy : 0;
    float x = anchorOrigin(screenW, elemW, factors.x) + dx;
    float y = anchorOrigin(screenH, elemH, factors.y) + dy;
    return {std::clamp(x, 0.f, std::max(0.f, screenW - elemW)),
            std::clamp(y, 0.f, std::max(0.f, screenH - elemH))};
}
inline Anchor nearestAnchor(float cx, float cy, float screenW, float screenH) {
    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(screenW) || !std::isfinite(screenH)
        || screenW <= 0 || screenH <= 0) return Anchor::TopLeft;
    int column = cx < screenW / 3 ? 0 : cx < 2 * screenW / 3 ? 1 : 2;
    int row = cy < screenH / 3 ? 0 : cy < 2 * screenH / 3 ? 1 : 2;
    constexpr Anchor grid[3][3] = {{Anchor::TopLeft, Anchor::TopCenter, Anchor::TopRight},
                                   {Anchor::MiddleLeft, Anchor::Center, Anchor::MiddleRight},
                                   {Anchor::BottomLeft, Anchor::BottomCenter, Anchor::BottomRight}};
    return grid[row][column];
}
// Store a drop with its top-left at x,y: the screen third of its center
// picks the anchor (so it grows away from the nearest edge); the offset keeps
// it exactly where it was dropped.
inline HudElement placeAt(HudElement element, float x, float y, float elemW, float elemH, float screenW, float screenH) {
    element.anchor = nearestAnchor(x + elemW / 2, y + elemH / 2, screenW, screenH);
    auto factors = anchorFactors(element.anchor);
    element.dx = std::round(x - anchorOrigin(screenW, elemW, factors.x));
    element.dy = std::round(y - anchorOrigin(screenH, elemH, factors.y));
    if (!std::isfinite(element.dx)) element.dx = 0;
    if (!std::isfinite(element.dy)) element.dy = 0;
    return element;
}
// "Snap to" one of the nine positions, keeping the inset from the edges.
inline HudElement snapTo(HudElement element, Anchor anchor) {
    auto factors = anchorFactors(anchor);
    element.anchor = anchor;
    element.dx = factors.x == 0 ? hudInset : factors.x == 1 ? -hudInset : 0;
    element.dy = factors.y == 0 ? hudInset : factors.y == 1 ? -hudInset : 0;
    return element;
}
// Drag magnet: pull a coordinate onto the edge, the inset or the center when
// within `reach`; returns the guide line position that caught it.
struct Magnet { float position; std::optional<float> line; };
inline Magnet magnet(float position, float size, float screen, float reach = 3) {
    struct Candidate { float at, line; };
    Candidate candidates[] = {{0, 0}, {hudInset, hudInset}, {screen - size, screen},
                              {screen - size - hudInset, screen - hudInset}, {(screen - size) / 2, screen / 2}};
    Magnet best{position, std::nullopt};
    float distance = reach;
    for (auto const& c : candidates) {
        float d = std::abs(position - c.at);
        if (d < distance || (d == distance && !best.line)) { distance = d; best = {c.at, c.line}; }
    }
    return best;
}
}
