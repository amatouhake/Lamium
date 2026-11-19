#pragma once
#include <vector>
// The game's window and item tooltip as rectangles in GUI units (L-366),
// measured from the game at 3 pixels a unit (UI zoom 75%, 1920x1080,
// 2026-11-19). Each rect is drawn in its part's color, in order: later ones
// cover earlier ones. Corners are left open, as the game's are.
namespace lamium::ui::vanilla_frame {
enum class Part { Fill, Light, Dark, Edge, Border };
struct Rect { float x, y, w, h; Part part; };

// The window: a black edge with its corners cut, two units of light on the
// upper left and of shadow on the lower right, the game's grey inside.
inline std::vector<Rect> window(float x, float y, float w, float h) {
    if (w < 8 || h < 8) return {{x, y, w, h, Part::Fill}};
    auto r = [&](float rx, float ry, float rw, float rh, Part part) { return Rect{x + rx, y + ry, rw, rh, part}; };
    return {
        r(3, 3, w - 6, h - 6, Part::Fill), r(w - 3, 2, 1, 1, Part::Fill),
        r(3, 1, w - 6, 1, Part::Light), r(2, 2, w - 5, 1, Part::Light), r(1, 3, 2, h - 6, Part::Light), r(3, 3, 1, 1, Part::Light),
        r(w - 3, 3, 2, h - 5, Part::Dark), r(2, h - 3, w - 5, 1, Part::Dark), r(3, h - 2, w - 5, 1, Part::Dark), r(w - 4, h - 4, 1, 1, Part::Dark),
        r(3, 0, w - 6, 1, Part::Edge), r(3, h - 1, w - 5, 1, Part::Edge), r(0, 3, 1, h - 6, Part::Edge), r(w - 1, 3, 1, h - 5, Part::Edge),
        r(2, 1, 1, 1, Part::Edge), r(1, 2, 1, 1, Part::Edge), r(w - 3, 1, 1, 1, Part::Edge), r(w - 2, 2, 1, 1, Part::Edge),
        r(1, h - 3, 1, 1, Part::Edge), r(2, h - 2, 1, 1, Part::Edge), r(w - 2, h - 2, 1, 1, Part::Edge),
    };
}

// The item tooltip: opaque dark fill with rounded corners and a purple line
// one unit in from its edge.
inline std::vector<Rect> tooltip(float x, float y, float w, float h) {
    if (w < 5 || h < 5) return {{x, y, w, h, Part::Fill}};
    auto r = [&](float rx, float ry, float rw, float rh, Part part) { return Rect{x + rx, y + ry, rw, rh, part}; };
    return {
        r(2, 0, w - 4, h, Part::Fill), r(0, 2, w, h - 4, Part::Fill), r(1, 1, w - 2, h - 2, Part::Fill),
        r(2, 1, w - 4, 1, Part::Border), r(2, h - 2, w - 4, 1, Part::Border), r(1, 2, 1, h - 4, Part::Border), r(w - 2, 2, 1, h - 4, Part::Border),
    };
}
// Where the tooltip's text starts inside it, and its size around text of
// the given width and lines (the game's: 6 units left, 5 above and below a
// line, 10 units a line).
inline constexpr float tooltipTextLeft = 6, tooltipTextTop = 5, tooltipLine = 10;
inline constexpr float tooltipWidth(float textWidth) { return textWidth + 2 * tooltipTextLeft; }
inline constexpr float tooltipHeight(int lines) { return 2 * tooltipTextTop - 2 + tooltipLine * static_cast<float>(lines); }
}
