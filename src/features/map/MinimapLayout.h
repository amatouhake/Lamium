#pragma once
#include "ui/HudElement.h"
#include <algorithm>
#include <cstddef>

namespace lamium::map {
// The minimap's text (compass letters, the lines below, player names) is at
// the element's text zoom: the standard size or its scale (L-357,
// ui::hudTextZoom).
// The box around a map of `side` units: compass letters straddle the frame
// (half a letter outside it), the lines below are centered under it.
struct MinimapLayout {
    float lineHeight = 0, margin = 0, compassWidth = 0, nameScale = 0;
    float contentW = 0, contentH = 0;
};
inline MinimapLayout minimapLayout(float side, float textScale, bool compass, std::size_t lines, float linesWidth) {
    MinimapLayout layout;
    layout.lineHeight = 10 * textScale;
    layout.margin = compass ? 5 * textScale : 0;
    layout.compassWidth = 20 * textScale; // Wide enough that a large letter is not cut.
    layout.nameScale = textScale * .75f;
    layout.contentW = std::max(side + 2 * layout.margin, linesWidth + 2);
    layout.contentH = side + 2 * layout.margin + (lines ? 2 + lines * layout.lineHeight : 0);
    return layout;
}
}
