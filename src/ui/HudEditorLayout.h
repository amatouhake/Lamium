#pragma once
#include "ui/HudElement.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace lamium::ui::hud_editor {
// Geometry of the HUD layout editor (BACKLOG L-04c, docs/demos/hud.html).
// Pure: the editor glue draws and feeds pointer positions in GUI units.
struct Point { float x = 0, y = 0; };
struct Box {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
using Boxes = std::array<std::optional<Box>, 4>; // Indexed by HudElementId.
// InfoHud draws status, target, toast, then info; later ones are on top.
inline constexpr std::array<HudElementId, 4> drawOrder{
    HudElementId::Status, HudElementId::Target, HudElementId::Toast, HudElementId::Info};
inline constexpr float margin = 4; // Same inset as placeElement.

inline Point anchorPoint(Anchor anchor, float screenW, float screenH) {
    auto f = anchorFactors(anchor);
    return {margin + (screenW - 2 * margin) * f.x, margin + (screenH - 2 * margin) * f.y};
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
// Element box while dragging: the grab point follows the pointer, clamped.
inline Box dragBox(float px, float py, float grabX, float grabY, float w, float h, float screenW, float screenH) {
    float x = std::clamp(px - grabX, 0.f, std::max(0.f, screenW - w));
    float y = std::clamp(py - grabY, 0.f, std::max(0.f, screenH - h));
    return {x, y, w, h};
}
// The element under the pointer; the one drawn last wins.
inline std::optional<HudElementId> topmost(Boxes const& boxes, float px, float py) {
    for (auto it = drawOrder.rbegin(); it != drawOrder.rend(); ++it) {
        auto const& box = boxes[static_cast<size_t>(*it)];
        if (box && box->contains(px, py)) return *it;
    }
    return std::nullopt;
}

// Inspector panel: a column on the side away from the selected element, with
// a title bar, scrolling rows and a footer with buttons.
struct Panel {
    static constexpr float width = 170, headerHeight = 18, rowHeight = 14, footerHeight = 18, pad = 6;
    static constexpr float buttonWidth = 52, buttonHeight = 11;
    float x = 0, y = 0, w = width, h = 0;
    int first = 0, visible = 0, count = 0;
    float rowsTop() const { return y + headerHeight; }
    float rowY(int index) const { return rowsTop() + (index - first) * rowHeight; }
    float footerTop() const { return y + h - footerHeight; }
    // Footer buttons are right-aligned; index 0 is the rightmost.
    float buttonX(int index) const { return x + w - pad - (index + 1) * buttonWidth - index * 4; }
    float buttonY() const { return footerTop() + (footerHeight - buttonHeight) / 2; }
    // Row controls sit in the right part of the row.
    static constexpr float controlWidth = 72, arrowWidth = 11;
    float controlX() const { return x + w - pad - controlWidth; }
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
inline Panel fitPanel(float screenW, float screenH, std::optional<Box> selected, int count, int first) {
    Panel p;
    p.count = std::max(0, count);
    bool left = selected && selected->x + selected->w / 2 >= screenW / 2;
    p.x = left ? 8 : std::max(8.f, screenW - Panel::width - 8);
    p.y = 8;
    float maxRows = std::floor((screenH - 16 - Panel::headerHeight - Panel::footerHeight) / Panel::rowHeight);
    p.visible = std::max(0, std::min(p.count, static_cast<int>(maxRows)));
    p.first = std::clamp(first, 0, std::max(0, p.count - p.visible));
    p.h = Panel::headerHeight + p.visible * Panel::rowHeight + Panel::footerHeight;
    return p;
}
struct PanelHit {
    enum class Zone { None, Header, Row, Button } zone = Zone::None;
    int index = -1; // Row index, or footer button index.
    int part = 0;   // In a row control: -1 decrease, 1 increase, 0 value, 2 label.
};
inline PanelHit hitPanel(Panel const& p, float px, float py) {
    if (!p.contains(px, py)) return {};
    if (py < p.rowsTop()) return {PanelHit::Zone::Header};
    if (py >= p.footerTop()) {
        for (int i = 0; i < 2; ++i)
            if (px >= p.buttonX(i) && px < p.buttonX(i) + Panel::buttonWidth && py >= p.buttonY()
                && py < p.buttonY() + Panel::buttonHeight) return {PanelHit::Zone::Button, i};
        return {};
    }
    int offset = static_cast<int>((py - p.rowsTop()) / Panel::rowHeight);
    if (offset < 0 || offset >= p.visible) return {};
    int part = 2;
    float cx = p.controlX();
    if (px >= cx) part = px < cx + Panel::arrowWidth ? -1 : px >= cx + Panel::controlWidth - Panel::arrowWidth ? 1 : 0;
    return {PanelHit::Zone::Row, p.first + offset, part};
}
}
