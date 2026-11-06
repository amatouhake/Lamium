#pragma once
#include <algorithm>
#include <optional>
#include <vector>

namespace lamium::ui {
// Several rows of a list chosen at once (L-190). Pure: the caller decides which
// keys may take part and passes the rows in the order they are drawn. A range
// keeps its anchor, so a second Shift + click re-draws it from the same row.
template <class Key>
struct RowSelection {
    std::vector<Key> keys;
    std::optional<Key> anchor;

    bool contains(Key const& key) const { return std::find(keys.begin(), keys.end(), key) != keys.end(); }
    void clear() { keys.clear(); anchor.reset(); }
    void only(Key const& key) { keys = {key}; anchor = key; }
    void toggle(Key const& key) {
        auto found = std::find(keys.begin(), keys.end(), key);
        if (found == keys.end()) keys.push_back(key);
        else keys.erase(found);
        anchor = key;
    }
    // From the anchor to `key` in `order`; a row that is not there selects only `key`.
    void range(Key const& key, std::vector<Key> const& order) {
        auto to = std::find(order.begin(), order.end(), key);
        auto from = anchor ? std::find(order.begin(), order.end(), *anchor) : order.end();
        if (to == order.end() || from == order.end()) { only(key); return; }
        if (from > to) std::swap(from, to);
        keys.assign(from, to + 1);
    }
    template <class Exists>
    void retain(Exists exists) {
        std::erase_if(keys, [&](Key const& key) { return !exists(key); });
        if (anchor && !exists(*anchor)) anchor.reset();
    }
};
}
