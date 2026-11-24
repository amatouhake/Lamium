#pragma once
#include <algorithm>
#include <cmath>

namespace lamium::ui {
// Lines in a detail pane that scroll under the wheel when they do not all
// fit (the Check tab's item lines, L-183): `count` lines `pitch` apart from
// `top`, each `lineHeight` tall, kept above `bottom`; `first` is clamped so
// the last line ends the view.
struct LineScroll {
    int count = 0, visible = 0, first = 0;
    bool overflows() const { return count > visible; }
};
inline int linesFitting(float top, float bottom, float pitch, float lineHeight) {
    if (!(pitch > 0) || !(bottom - top >= lineHeight)) return 0;
    return static_cast<int>(std::floor((bottom - top - lineHeight) / pitch + 1e-4f)) + 1;
}
inline LineScroll lineScroll(int count, int first, float top, float bottom, float pitch, float lineHeight) {
    LineScroll s;
    s.count = std::max(0, count);
    s.visible = std::min(s.count, linesFitting(top, bottom, pitch, lineHeight));
    s.first = std::clamp(first, 0, std::max(0, s.count - s.visible));
    return s;
}
// The first row shown of a pane showing `visible` rows, moved only as far as
// brings the selected one into view (the detail fields under the arrow keys).
constexpr int firstShowing(int selected, int first, int visible) {
    if (visible <= 0) return first;
    if (selected < first) return selected;
    if (selected >= first + visible) return selected - visible + 1;
    return first;
}
// The Check pane's preview (L-183): its usual share of the room above the
// buttons, given back so up to `maxLines` item lines fit under it (`below`
// is what stands between the preview and the first line); never under
// `minH`. Text starts 5 under the preview and ends 6 above the buttons.
inline float previewHeight(float room, int lines, float below, float pitch, int maxLines = 4,
                           float share = .55f, float minH = 60, float maxH = 240) {
    float usual = std::clamp(room * share, minH, maxH);
    if (lines <= 0) return usual;
    float wanted = room - 11 - below - static_cast<float>(std::min(lines, maxLines)) * pitch;
    return std::clamp(wanted, std::min(minH, usual), usual);
}
}
