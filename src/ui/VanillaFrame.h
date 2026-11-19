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

// A tab standing on the window (L-367): the window's top corners and light, `h` high from its top, open
// at the bottom where it meets the window. `selected`: the window's shadow down its right side too.
inline std::vector<Rect> tab(float x, float y, float w, float h, bool selected) {
    if (w < 8 || h < 4) return {{x, y, w, h, Part::Fill}};
    auto r = [&](float rx, float ry, float rw, float rh, Part part) { return Rect{x + rx, y + ry, rw, rh, part}; };
    std::vector<Rect> out{
        r(3, 1, w - 6, h - 1, Part::Fill), r(2, 2, w - 4, h - 2, Part::Fill), r(1, 3, w - 2, h - 3, Part::Fill),
        r(3, 1, w - 6, 1, Part::Light), r(2, 2, w - 5, 1, Part::Light), r(1, 3, 2, h - 3, Part::Light), r(3, 3, 1, 1, Part::Light),
    };
    if (selected) out.push_back(r(w - 3, 3, 2, h - 3, Part::Dark));
    for (Rect const& edge : {r(3, 0, w - 6, 1, Part::Edge), r(2, 1, 1, 1, Part::Edge), r(1, 2, 1, 1, Part::Edge), r(w - 3, 1, 1, 1, Part::Edge),
                             r(w - 2, 2, 1, 1, Part::Edge), r(0, 3, 1, h - 3, Part::Edge), r(w - 1, 3, 1, h - 3, Part::Edge)})
        out.push_back(edge);
    return out;
}

// The item tooltip: opaque dark fill with a purple line one unit in from
// its edge. Its upper left and lower right corners are rounded by one step,
// its upper right and lower left by a two-unit diagonal, as the game's are.
inline std::vector<Rect> tooltip(float x, float y, float w, float h) {
    if (w < 8 || h < 8) return {{x, y, w, h, Part::Fill}};
    auto r = [&](float rx, float ry, float rw, float rh, Part part) { return Rect{x + rx, y + ry, rw, rh, part}; };
    return {
        r(2, 0, w - 5, 1, Part::Fill), r(1, 1, w - 3, 1, Part::Fill), r(0, 2, w - 1, 1, Part::Fill), r(0, 3, w, h - 6, Part::Fill),
        r(1, h - 3, w - 1, 1, Part::Fill), r(2, h - 2, w - 3, 1, Part::Fill), r(3, h - 1, w - 5, 1, Part::Fill),
        r(2, 1, w - 5, 1, Part::Border), r(w - 4, 2, 2, 1, Part::Border), r(w - 3, 3, 2, 1, Part::Border), r(w - 2, 4, 1, h - 6, Part::Border),
        r(1, 2, 1, h - 5, Part::Border), r(2, h - 4, 1, 1, Part::Border), r(2, h - 3, 2, 1, Part::Border), r(3, h - 2, w - 5, 1, Part::Border),
    };
}
// Where the tooltip's text starts inside it, and its size around text of
// the given width and lines (the game's: 5 units on each side, 10 units a
// line).
inline constexpr float tooltipTextLeft = 5, tooltipTextTop = 5, tooltipLine = 10;
inline constexpr float tooltipWidth(float textWidth) { return textWidth + 2 * tooltipTextLeft; }
inline constexpr float tooltipHeight(int lines) { return 2 * tooltipTextTop - 2 + tooltipLine * static_cast<float>(lines); }
}
