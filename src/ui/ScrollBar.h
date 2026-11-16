#pragma once
#include <algorithm>

namespace lamium::ui {
// A bar is 2 wide, 3 in from its list's right edge. Its press zone reaches
// `barReach` further left; while it shows, rows and their columns end
// `rowBarGutter` short of the edge, clear of that zone (maintainer
// 2026-11-16: tighter than the first 5 and 6, a little harder to grab).
inline constexpr float barReach = 1, rowBarGutter = 4;
// One scroll bar, whatever it scrolls: a track of `track` units from `top`
// drawn in the 2-unit column at `x` (drawScrollbar), a thumb `thumb` long,
// and a position `pos` of `range` measured from the top (rows for a list,
// units for the calculator's history). A press grips the thumb where it was
// taken; on the track the thumb's middle goes to the pointer. The held bar
// itself lives in the settings screen (screen::pressScrollbar).
struct ScrollBar {
    float x = 0, top = 0, track = 0, thumb = 0;
    float range = 0, pos = 0;
    bool scrolls() const { return range > 0 && track > thumb; }
    float thumbTop() const { return scrolls() ? top + (track - thumb) * std::clamp(pos / range, 0.f, 1.f) : top; }
    // The lists' press zone: the bar's column, `barReach` to its left and
    // what lies right of it.
    bool hit(float px, float py) const { return scrolls() && px >= x - barReach && px < x + 3 && py >= top && py < top + track; }
    float grabAt(float py) const {
        float t = thumbTop();
        return py >= t && py <= t + thumb ? py - t : thumb / 2;
    }
    float posAt(float py, float grab) const {
        float room = track - thumb;
        if (!(room > 0) || !(range > 0)) return 0;
        return std::clamp((py - grab - top) / room, 0.f, 1.f) * range;
    }
};
// A bar over rows: `count` rows, `visible` of them from `first`; the thumb
// is the shown share of the track, at least `minThumb` long.
inline ScrollBar rowScrollBar(float x, float top, float track, int count, int visible, int first, float minThumb = 8) {
    ScrollBar b{x, top, track, track};
    if (count > visible && visible > 0 && track > 0) {
        b.thumb = std::min(track, std::max(minThumb, track * static_cast<float>(visible) / static_cast<float>(count)));
        b.range = static_cast<float>(count - visible);
        b.pos = static_cast<float>(first);
    }
    return b;
}
}
