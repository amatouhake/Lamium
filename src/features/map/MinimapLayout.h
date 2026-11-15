#pragma once
#include "ui/HudElement.h"
#include <algorithm>
#include <cstddef>

namespace lamium::map {
// The minimap's text (compass letters, the lines below, player names) is
// the element's HUD scale times "Text size" (L-337); both are 50-200 % in
// 5 % steps, so the text spans 25-400 % of its unscaled size.
inline float minimapTextScale(float hudScale, float textSize) {
    return ui::clampHudScale(hudScale) / 100 * ui::clampHudScale(textSize) / 100;
}
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
