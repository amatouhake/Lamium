#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace lamium::information::paperDoll {
// Whether Lamium's Info HUD covers the game's paper doll or its position
// line (L-145). Pure.
struct Box { float x, y, w, h; };
// The doll is drawn larger than its control (15x15 units on desktop): about
// one control width to each side, and from the top of the screen down to
// where the chat's room for it ends (50 units for a 15-unit control).
inline std::optional<Box> area(Box control) {
    for (float v : {control.x, control.y, control.w, control.h})
        if (!std::isfinite(v)) return std::nullopt;
    if (control.w <= 0 || control.h <= 0) return std::nullopt;
    float side = control.w;
    return Box{control.x - side, 0, 3 * side, control.y + side * 35 / 15};
}
// Placing the doll (L-309). The game centers the doll in its control and
// sizes it by the control (checked in game); the element the HUD layout
// moves is the doll's drawn extent for the 15-unit desktop control: one
// control width to each side, and 50 units from a control's width above it.
inline constexpr float controlSide = 15, placedWidth = 3 * controlSide, placedHeight = 50;
struct Point { float x, y; };
// Where the control's center goes for an element box with its top left at x, y.
inline Point controlCenter(float x, float y) { return {x + 1.5f * controlSide, y + 1.5f * controlSide}; }
// The control's top left for that center, whatever size the control has now
// (the screen's entrance animation shrinks it about its center).
inline std::optional<Point> controlPosition(Point center, float w, float h) {
    for (float v : {center.x, center.y, w, h}) if (!std::isfinite(v)) return std::nullopt;
    if (w < 0 || h < 0) return std::nullopt;
    return Point{center.x - w / 2, center.y - h / 2};
}
// A stand-in figure for the layout editor, where the game draws no doll:
// head, body, two arms, two legs, centered in the element box.
inline std::array<Box, 6> figure(Box area) {
    float u = std::min(area.w / placedWidth, area.h / placedHeight);
    float cx = area.x + area.w / 2, top = area.y + (area.h - 32 * u) / 2;
    return {{{cx - 4 * u, top, 8 * u, 8 * u}, {cx - 4 * u, top + 8 * u, 8 * u, 12 * u},
             {cx - 8 * u, top + 8 * u, 4 * u, 12 * u}, {cx + 4 * u, top + 8 * u, 4 * u, 12 * u},
             {cx - 4 * u, top + 20 * u, 4 * u, 12 * u}, {cx, top + 20 * u, 4 * u, 12 * u}}};
}
inline bool overlaps(Box a, Box b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
// The position line is drawn within its own control.
inline bool meets(std::optional<Box> control, std::optional<Box> hud) {
    return control && hud && control->w > 0 && control->h > 0 && overlaps(*control, *hud);
}
inline bool covered(std::optional<Box> control, std::optional<Box> hud) {
    if (!control || !hud) return false;
    auto doll = area(*control);
    return doll && overlaps(*doll, *hud);
}
}
