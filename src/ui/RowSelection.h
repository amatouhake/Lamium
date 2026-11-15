#pragma once
#include <algorithm>
#include <optional>
#include <vector>

namespace lamium::ui {
// Several rows of a list chosen at once (L-190). Pure: the caller passes the
// rows that may take part, in the order they are drawn. `shown` is the row the
// detail pane shows: the last one clicked. A range keeps its anchor, so a
// second Shift + click re-draws it from the same row.
template <class Key>
struct RowSelection {
    std::vector<Key> keys;
    std::optional<Key> anchor, shown;

    bool contains(Key const& key) const { return std::find(keys.begin(), keys.end(), key) != keys.end(); }
    bool several() const { return keys.size() > 1; }
    void clear() { keys.clear(); anchor.reset(); shown.reset(); }
    void only(Key const& key) { keys = {key}; anchor = shown = key; }
    void toggle(Key const& key) {
        anchor = key;
        if (auto found = std::find(keys.begin(), keys.end(), key); found != keys.end()) {
            keys.erase(found);
            // The shown row left: the pane shows the last one still chosen.
            if (shown == key) shown = keys.empty() ? std::nullopt : std::optional(keys.back());
            return;
        }
        keys.push_back(key);
        shown = key;
    }
    // From the anchor to `key` in `order`; a row that is not there selects only `key`.
    void range(Key const& key, std::vector<Key> const& order) {
        auto to = std::find(order.begin(), order.end(), key);
        auto from = anchor ? std::find(order.begin(), order.end(), *anchor) : order.end();
        if (to == order.end() || from == order.end()) { only(key); return; }
        if (from > to) std::swap(from, to);
        keys.assign(from, to + 1);
        shown = key;
    }
    // A click on a row: alone, Ctrl toggles it, Shift takes a range (Shift wins).
    void click(Key const& key, bool ctrl, bool shift, std::vector<Key> const& order) {
        if (shift) range(key, order);
        else if (ctrl) toggle(key);
        else only(key);
    }
    // Ctrl + A: every row; the shown row stays when it is one of them.
    void selectAll(std::vector<Key> const& order) {
        if (order.empty()) return;
        keys = order;
        if (!shown || !contains(*shown)) shown = order.front();
        if (!anchor || !contains(*anchor)) anchor = shown;
    }
    // Esc: several rows fall back to the shown one. False when there were not
    // several, so Esc goes on to do what it did before.
    bool collapse() {
        if (!several()) return false;
        only(shown ? *shown : keys.back());
        return true;
    }
    template <class Exists>
    void retain(Exists exists) {
        std::erase_if(keys, [&](Key const& key) { return !exists(key); });
        if (anchor && !exists(*anchor)) anchor.reset();
        if (shown && !exists(*shown)) shown = keys.empty() ? std::nullopt : std::optional(keys.back());
    }
};

// What the bar shown while several rows are selected offers, in the bar's
// order. Not saved.
enum class BulkAction { Show, Hide, Delete };
enum class SelectionList { Waypoints, Shapes, Placements, MapPanel };
// Maintainer 2026-11-15: no bulk move or turn; the map's side panel shows and
// hides only, deleting stays in each list's own screen.
inline std::vector<BulkAction> bulkActions(SelectionList list) {
    if (list == SelectionList::MapPanel) return {BulkAction::Show, BulkAction::Hide};
    return {BulkAction::Show, BulkAction::Hide, BulkAction::Delete};
}
inline bool irreversible(BulkAction action) { return action == BulkAction::Delete; }
// The bar's state: an irreversible action acts on its second press in a row;
// any other press, or a change of the selection, disarms it. It is kept apart
// from the detail pane's own Delete, so arming one never lets the other act.
struct BulkBar {
    std::optional<BulkAction> armed;
    // True when the action should run now.
    bool press(BulkAction action) {
        if (irreversible(action) && armed != action) { armed = action; return false; }
        armed.reset();
        return true;
    }
    void disarm() { armed.reset(); }
};
// Keys a list with a selection handles before its own (virtual-key codes),
// when nothing is being typed.
enum class SelectionKey { None, SelectAll, Collapse };
inline SelectionKey selectionKey(int key, bool ctrl, bool several) {
    if (key == 0x41 && ctrl) return SelectionKey::SelectAll;
    if (key == 0x1b && several) return SelectionKey::Collapse;
    return SelectionKey::None;
}
// Where the bar's buttons go: right-aligned in [x, x + w) with `gap` between,
// each as wide as given; the count before them takes what is left.
struct BulkBarLayout {
    static constexpr float gap = 3;
    float x = 0, y = 0, w = 0, h = 0;
    std::vector<float> buttonX, buttonW;
    float countWidth() const { return buttonX.empty() ? w : std::max(0.f, buttonX.front() - gap - x); }
    int at(float px, float py) const {
        if (py < y || py >= y + h) return -1;
        for (size_t i = 0; i < buttonX.size(); ++i)
            if (px >= buttonX[i] && px < buttonX[i] + buttonW[i]) return static_cast<int>(i);
        return -1;
    }
};
inline BulkBarLayout bulkBarLayout(float x, float y, float w, float h, std::vector<float> const& widths) {
    BulkBarLayout l{.x = x, .y = y, .w = w, .h = h};
    float right = x + w;
    l.buttonX.resize(widths.size());
    l.buttonW = widths;
    for (size_t i = widths.size(); i-- > 0;) {
        right -= widths[i];
        l.buttonX[i] = right;
        right -= BulkBarLayout::gap;
    }
    return l;
}
}
