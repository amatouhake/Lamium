#pragma once
#include "features/recipes/BandLayout.h"
#include <cmath>
#include <optional>
#include <vector>

// L-375: arranging the bands by dragging (L-371 "Decided: the bands' arrangement", docs/demos/band-layout.html).
// A panel is grabbed anywhere and dropped where it goes; the layout after the drop shows while dragging.
// Pure.
namespace lamium::recipes::band::editor {
// What is dragged: a panel of the layout (`region`, `index`), or a page from the tray (`region` -1).
struct Drag {
    Page page = Page::Items;
    int region = -1, index = -1;
    bool fromTray() const { return region < 0; }
};
enum class Kind { None, Self, Swap, Before, After, Fill, Tray };
struct Target {
    Kind kind = Kind::None;
    int region = -1, index = -1;
    bool operator==(Target const&) const = default;
};
// Where a drop at a point goes. Hits are taken on the panels as they were when the drag began, so the preview
// does not move the targets. A lent panel takes nothing.
inline Target hit(std::vector<Placed> const& placed, Regions const& regions, Layout const& layout, Drag const& drag, float px,
                  float py, Box tray) {
    if (tray.has(px, py)) return {Kind::Tray};
    for (auto const& panel : placed) {
        if (panel.shownIn != panel.region || !panel.box.has(px, py)) continue;
        if (!drag.fromTray() && panel.region == drag.region && panel.index == drag.index) return {Kind::Self, panel.region, panel.index};
        if (!drag.fromTray() && panel.region == drag.region) return {Kind::Swap, panel.region, panel.index};
        bool const first = across(panel.region) ? px < panel.box.x + panel.box.w / 2 : py < panel.box.y + panel.box.h / 2;
        return {first ? Kind::Before : Kind::After, panel.region, panel.index};
    }
    for (int r = 0; r < regionCount; ++r)
        if (layout.regions[static_cast<size_t>(r)].empty() && regions.usable(r) && regions.boxes[static_cast<size_t>(r)].has(px, py))
            return {Kind::Fill, r};
    return {};
}
inline bool placedPage(Layout const& layout, Page page) {
    for (auto const& list : layout.regions)
        for (auto const& panel : list) if (panel.page == page) return true;
    return false;
}
// The layout after a drop; nothing where it changes nothing or cannot go.
inline std::optional<Layout> apply(Layout const& layout, Drag const& drag, Target const& target) {
    if (target.kind == Kind::None || target.kind == Kind::Self) return std::nullopt;
    Layout out = layout;
    auto takeOut = [&] {
        for (auto& list : out.regions) std::erase_if(list, [&](Panel const& p) { return p.page == drag.page; });
    };
    auto const validIndex = [&](int region, int index) {
        return region >= 0 && region < regionCount && index >= 0 && index < static_cast<int>(layout.regions[static_cast<size_t>(region)].size());
    };
    switch (target.kind) {
    case Kind::Tray:
        if (!placedPage(layout, drag.page)) return std::nullopt;
        takeOut();
        break;
    case Kind::Fill:
        if (target.region < 0 || target.region >= regionCount) return std::nullopt;
        takeOut();
        out.regions[static_cast<size_t>(target.region)] = {{drag.page, 100}};
        break;
    case Kind::Swap: {
        if (!validIndex(target.region, target.index) || !validIndex(drag.region, drag.index) || target.region != drag.region) return std::nullopt;
        auto& list = out.regions[static_cast<size_t>(target.region)];
        std::swap(list[static_cast<size_t>(target.index)], list[static_cast<size_t>(drag.index)]);
        break;
    }
    case Kind::Before:
    case Kind::After: {
        if (!validIndex(target.region, target.index)) return std::nullopt;
        Page const at = layout.regions[static_cast<size_t>(target.region)][static_cast<size_t>(target.index)].page;
        if (at == drag.page) return std::nullopt;
        takeOut();
        auto& list = out.regions[static_cast<size_t>(target.region)];
        auto it = std::find_if(list.begin(), list.end(), [&](Panel const& p) { return p.page == at; });
        int const half = std::max(10, static_cast<int>(std::lround(it->share / 2.0 / 5)) * 5);
        it->share = std::max(5, it->share - half);
        list.insert(target.kind == Kind::Before ? it : it + 1, Panel{drag.page, half});
        break;
    }
    default: return std::nullopt;
    }
    return normalized(out);
}
// The line between panels `first` and `first + 1` of a region moved `delta` units along a region `length`
// long: shares in steps of 5, neither under one row.
inline Layout resize(Layout layout, int region, int first, float startShare, float delta, float length) {
    auto& list = layout.regions[static_cast<size_t>(region)];
    if (first < 0 || first + 1 >= static_cast<int>(list.size()) || !(length > 0)) return layout;
    auto& a = list[static_cast<size_t>(first)];
    auto& b = list[static_cast<size_t>(first + 1)];
    int const sum = a.share + b.share;
    int const least = std::min(sum / 2, static_cast<int>(std::ceil(minLength(region) / length * 100 / 5)) * 5);
    int const moved = static_cast<int>(std::lround((startShare + delta / length * 100) / 5)) * 5;
    a.share = std::clamp(moved, least, sum - least);
    b.share = sum - a.share;
    return layout;
}
// The pages the layout does not place: the tray.
inline std::vector<Page> unplaced(Layout const& layout) {
    std::vector<Page> out;
    for (int i = 0; i < pageCount; ++i) if (!placedPage(layout, static_cast<Page>(i))) out.push_back(static_cast<Page>(i));
    return out;
}
}
