#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>

namespace lamium::ui {
// HUD element placement model (BACKLOG L-04a, DESIGN "HUD", docs/demos/hud.html).
// Elements are info lines, target, status and toast. Pure math; InfoHud draws.
enum class HudElementId { Info, Target, Status, Toast };
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
struct HudElement {
    Anchor anchor = Anchor::TopLeft;
    bool pinned = false; // Anchor chosen in settings; drags change only the offset.
    float dx = 0, dy = 0; // GUI-unit offset from the anchor point.
    float scale = 100; // Percent, 75-150.
    ElementBackground background = ElementBackground::None;
    bool shadow = true;
};
// Defaults match the demo (docs/demos/hud.html).
inline constexpr HudElement defaultHudElement(HudElementId id) {
    switch (id) {
    case HudElementId::Info: return {};
    case HudElementId::Target:
        return {Anchor::TopCenter, false, 0, 0, 100, ElementBackground::Card, false};
    case HudElementId::Status:
        return {Anchor::MiddleRight, false, 0, -20, 100, ElementBackground::None, true};
    default:
        return {Anchor::BottomCenter, false, 0, -48, 100, ElementBackground::Card, false};
    }
}
inline constexpr std::string_view hudElementKey(HudElementId id) {
    switch (id) {
    case HudElementId::Info: return "info";
    case HudElementId::Target: return "target";
    case HudElementId::Status: return "status";
    default: return "toast";
    }
}
inline float scaledExtent(float size, float scale) {
    if (!std::isfinite(size) || size < 0) return 0;
    return size * std::clamp(std::isfinite(scale) ? scale : 100.f, 75.f, 150.f) / 100;
}
struct ElementPlacement { float x, y; };
// The anchor point stays put when the element grows; clamp to the screen.
inline ElementPlacement placeElement(float screenW, float screenH, float elemW, float elemH,
                                     HudElement const& element) {
    if (!std::isfinite(screenW) || !std::isfinite(screenH) || screenW <= 0 || screenH <= 0
        || !std::isfinite(elemW) || !std::isfinite(elemH) || elemW < 0 || elemH < 0) return {};
    constexpr float margin = 4;
    auto factors = anchorFactors(element.anchor);
    float dx = std::isfinite(element.dx) ? element.dx : 0;
    float dy = std::isfinite(element.dy) ? element.dy : 0;
    float x = margin + (screenW - 2 * margin - elemW) * factors.x + dx;
    float y = margin + (screenH - 2 * margin - elemH) * factors.y + dy;
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
struct DragResult { Anchor anchor; float dx, dy; };
// Resolve a drag with the element's top-left at x,y: nearest anchor unless
// pinned (then only the offset moves). Small offsets snap to 0.
inline DragResult resolveDrag(float x, float y, float elemW, float elemH, float screenW, float screenH,
                              HudElement const& current) {
    Anchor anchor = current.pinned ? current.anchor
        : nearestAnchor(x + elemW / 2, y + elemH / 2, screenW, screenH);
    auto factors = anchorFactors(anchor);
    constexpr float margin = 4;
    float dx = std::round(x - (margin + (screenW - 2 * margin - elemW) * factors.x));
    float dy = std::round(y - (margin + (screenH - 2 * margin - elemH) * factors.y));
    if (!std::isfinite(dx)) dx = 0;
    if (!std::isfinite(dy)) dy = 0;
    if (std::abs(dx) < 6) dx = 0;
    if (std::abs(dy) < 6) dy = 0;
    return {anchor, dx, dy};
}
}
