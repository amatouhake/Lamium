#pragma once
#include <algorithm>
#include <optional>
#include <vector>

namespace lamium::ui {
// Several rows of a list chosen at once (L-190). Pure: the caller passes the
// rows that may take part, in the order they are drawn. No row is primary:
// with one row the detail pane shows it, with several the selection pane
// stands in for it. A range keeps its anchor, so a second Shift + click
// re-draws it from the same row.
template <class Key>
struct RowSelection {
    std::vector<Key> keys;
    std::optional<Key> anchor;

    bool contains(Key const& key) const { return std::find(keys.begin(), keys.end(), key) != keys.end(); }
    bool several() const { return keys.size() > 1; }
    // The row the detail pane shows: the selected one when it is alone.
    std::optional<Key> single() const { return keys.size() == 1 ? std::optional(keys.front()) : std::nullopt; }
    void clear() { keys.clear(); anchor.reset(); }
    void only(Key const& key) { keys = {key}; anchor = key; }
    void toggle(Key const& key) {
        anchor = key;
        if (auto found = std::find(keys.begin(), keys.end(), key); found != keys.end()) keys.erase(found);
        else keys.push_back(key);
    }
    // From the anchor to `key` in `order`; a row that is not there selects only `key`.
    void range(Key const& key, std::vector<Key> const& order) {
        auto to = std::find(order.begin(), order.end(), key);
        auto from = anchor ? std::find(order.begin(), order.end(), *anchor) : order.end();
        if (to == order.end() || from == order.end()) { only(key); return; }
        if (from > to) std::swap(from, to);
        keys.assign(from, to + 1);
    }
    // A click on a row: alone, Ctrl toggles it, Shift takes a range (Shift wins).
    void click(Key const& key, bool ctrl, bool shift, std::vector<Key> const& order) {
        if (shift) range(key, order);
        else if (ctrl) toggle(key);
        else only(key);
    }
    // Ctrl + click on a mark on the world map: no range. A row the list
    // shows (`order`, the listed layer and dimension) is added or dropped; any
    // other starts again from it alone, as the list then switches to its layer.
    void toggleListed(Key const& key, std::vector<Key> const& order) {
        if (std::find(order.begin(), order.end(), key) == order.end()) { only(key); return; }
        retain([&](Key const& k) { return std::find(order.begin(), order.end(), k) != order.end(); });
        toggle(key);
    }
    // Ctrl + A: every row.
    void selectAll(std::vector<Key> const& order) {
        if (order.empty()) return;
        keys = order;
        if (!anchor || !contains(*anchor)) anchor = order.front();
    }
    // Esc: several rows are all let go, so nothing is selected. False when
    // there were not several, so Esc goes on to do what it did before.
    bool clearSeveral() {
        if (!several()) return false;
        clear();
        return true;
    }
    template <class Exists>
    void retain(Exists exists) {
        std::erase_if(keys, [&](Key const& key) { return !exists(key); });
        if (anchor && !exists(*anchor)) anchor.reset();
    }
    // The selected rows in the list's order (keys are in the order chosen).
    std::vector<Key> inOrder(std::vector<Key> const& order) const {
        std::vector<Key> out;
        for (auto const& key : order) if (contains(key)) out.push_back(key);
        return out;
    }
};

// What the selection pane offers while several rows are selected, in its
// order. Not saved. The Schematics Check list (L-183) marks its cells in the
// world, ignores their whole blocks, or takes their own entries back.
enum class BulkAction { Show, Hide, Delete, ShowInWorld, IgnorePlaces, Unignore };
enum class SelectionList { Waypoints, Shapes, Placements, MapPanel, Check };
// Maintainer 2026-11-15: no bulk move or turn; the map's side panel matches
// the screens' lists (after the check).
inline std::vector<BulkAction> bulkActions(SelectionList list) {
    if (list == SelectionList::Check) return {BulkAction::ShowInWorld, BulkAction::IgnorePlaces, BulkAction::Unignore};
    return {BulkAction::Show, BulkAction::Hide, BulkAction::Delete};
}
inline bool irreversible(BulkAction action) { return action == BulkAction::Delete; }
// The pane's arming: an irreversible action acts on its second press in a
// row; any other press, or a change of the selection, disarms it.
struct BulkArm {
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
enum class SelectionKey { None, SelectAll, Clear };
inline SelectionKey selectionKey(int key, bool ctrl, bool several) {
    if (key == 0x41 && ctrl) return SelectionKey::SelectAll;
    if (key == 0x1b && several) return SelectionKey::Clear;
    return SelectionKey::None;
}
// How many selected names the pane lists in `room` of `lineHeight` lines:
// all when they fit, else one line fewer and a last "N more" line. Nothing
// when not even one line fits.
struct NamesFit { int names = 0, more = 0; };
inline NamesFit namesFit(int count, float room, float lineHeight) {
    int lines = lineHeight > 0 && room > 0 ? static_cast<int>(room / lineHeight) : 0;
    count = std::max(0, count);
    if (count <= lines) return {count, 0};
    if (lines <= 0) return {};
    return {lines - 1, count - (lines - 1)};
}
// The selection pane: the count on top, how many are shown and hidden under
// it, then the names, the actions' buttons in one row. Built by the list's own layout (ShapesLayout, the map
// panel), drawn by drawSelectionPane, hit by the next click.
struct SelectionPaneLayout {
    float x = 0, y = 0, w = 0; // the count's line
    float summaryY = 0;        // "Shown N / Hidden M"
    bool summary = true;       // false: a list without a shown switch has no summary line
    float linesTop = 0, lineHeight = 12;
    NamesFit fit;
    float buttonsY = 0, buttonH = 0;
    std::vector<float> buttonX, buttonW;
    int at(float px, float py) const {
        if (py < buttonsY || py >= buttonsY + buttonH) return -1;
        for (size_t i = 0; i < buttonX.size(); ++i)
            if (px >= buttonX[i] && px < buttonX[i] + buttonW[i]) return static_cast<int>(i);
        return -1;
    }
};
// The buttons where a detail pane's own sit: from `left`, `width` wide each
// with `gap` between, the irreversible one at `deleteX` instead (one Delete
// place for one and several rows). On a narrow pane the others narrow, down
// to `minWidth`, so they stay clear of it. Without an irreversible action a
// `deleteX` past `left` still bounds them (the pane's right edge).
inline void placeSelectionButtons(SelectionPaneLayout& pane, std::vector<BulkAction> const& actions, float left, float width,
                                  float gap, float deleteX, float deleteWidth, float minWidth = 24) {
    int plain = 0;
    bool hasDelete = false;
    for (auto action : actions) {
        if (irreversible(action)) hasDelete = true;
        else ++plain;
    }
    float w = width;
    if ((hasDelete || deleteX > left) && plain > 0)
        w = std::clamp((deleteX - gap - left - (plain - 1) * gap) / plain, std::min(minWidth, width), width);
    pane.buttonX.clear();
    pane.buttonW.clear();
    int k = 0;
    for (auto action : actions) {
        if (irreversible(action)) { pane.buttonX.push_back(deleteX); pane.buttonW.push_back(deleteWidth); continue; }
        pane.buttonX.push_back(left + k++ * (w + gap));
        pane.buttonW.push_back(w);
    }
}
// The selection pane in a narrow panel (the world map's side panel): the
// count from `top`, the summary and the names under it, the buttons on the
// panel's last row above `bottom`, Show and Hide `plainWidth` wide from `x`,
// Delete `deleteWidth` wide at the right edge. No buttons when that row
// would cover the count and summary.
inline SelectionPaneLayout compactSelectionPane(float x, float w, float top, float bottom, float buttonH, int count,
                                                std::vector<BulkAction> const& actions, float plainWidth, float deleteWidth,
                                                float gap = 3, float lineHeight = 11) {
    SelectionPaneLayout p;
    p.x = x; p.w = w; p.y = top;
    p.lineHeight = lineHeight;
    p.summaryY = top + 12;
    p.linesTop = p.summaryY + lineHeight;
    p.buttonH = buttonH;
    p.buttonsY = bottom - buttonH - 2;
    if (p.buttonsY < p.linesTop) return p;
    p.fit = namesFit(count, p.buttonsY - 3 - p.linesTop, lineHeight);
    deleteWidth = std::min(deleteWidth, w / 2);
    placeSelectionButtons(p, actions, x, std::min(plainWidth, w), gap, x + w - deleteWidth, deleteWidth, 16);
    return p;
}
}
