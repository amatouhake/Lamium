#pragma once
#include "ui/ScrollBar.h"
#include <cmath>
#include <functional>
#include <optional>

namespace lamium::ui::scroll_drag {
// The one scroll bar being dragged, wherever it is drawn (the settings
// screen, the world map, the panels beside the anvil): holding it moves its
// position to the pointer now, and each frame `follow` hands `set` the
// position the pointer asks for until `release`. `set` writes the view's own
// scroll state and must stay valid until release; `owner` names that state
// so a view can light the held thumb. Client thread only.
struct Held {
    ScrollBar bar;
    float grab = 0;
    std::function<void(float)> set;
    void const* owner = nullptr;
};
inline std::optional<Held>& current() {
    static std::optional<Held> held;
    return held;
}
inline void hold(ScrollBar const& bar, float y, std::function<void(float)> set, void const* owner = nullptr) {
    float grab = bar.grabAt(y);
    set(bar.posAt(y, grab));
    current() = Held{bar, grab, std::move(set), owner};
}
// Holds the bar when the press is on it (its own hit zone).
inline bool press(ScrollBar const& bar, float x, float y, std::function<void(float)> set, void const* owner = nullptr) {
    if (!bar.hit(x, y)) return false;
    hold(bar, y, std::move(set), owner);
    return true;
}
inline void follow(float y) {
    if (auto& held = current()) held->set(held->bar.posAt(y, held->grab));
}
inline void release() { current().reset(); }
inline bool holding(void const* owner) { return current() && owner && current()->owner == owner; }
// A `set` for a bar over rows: the first row shown.
inline std::function<void(float)> firstRow(int& first) {
    return [&first](float pos) { first = static_cast<int>(std::lround(pos)); };
}
// Which came last of a press and a release queued between two frames, for a
// view that ends a drag before the press that follows it: a quick click
// queues its release after the press, and that release must end what the
// press began.
struct PressOrder {
    bool pressQueued = false, releaseLast = false;
    void pressed() { pressQueued = true; releaseLast = false; }
    void released() { releaseLast = pressQueued; }
    void clear() { *this = {}; }
    // Once a frame: whether the release goes after the press.
    bool takeReleaseAfterPress() {
        bool after = releaseLast;
        clear();
        return after;
    }
};
}
