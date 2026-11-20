#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

// L-374: the item screens' bands as regions of panels, one page to a panel (L-371 "Decided: the bands'
// arrangement"). Regions lie beside, above and below the game's window; a region's panels split it along
// its length (top to bottom beside, left to right above and below) by their shares. Pure.
namespace lamium::recipes::band {
enum class Page { Items, Favourites, Plan };
inline constexpr int pageCount = 3;
inline constexpr std::array<std::string_view, pageCount> pageNames{"items", "favourites", "plan"};
inline std::optional<Page> pageNamed(std::string_view name) {
    for (int i = 0; i < pageCount; ++i) if (pageNames[static_cast<size_t>(i)] == name) return static_cast<Page>(i);
    return std::nullopt;
}
enum Region { Left, Right, Top, Bottom, regionCount };
inline constexpr std::array<std::string_view, regionCount> regionNames{"left", "right", "top", "bottom"};
inline bool across(int region) { return region == Top || region == Bottom; } // panels side by side

struct Panel {
    Page page = Page::Items;
    int share = 100; // percent of its region, a multiple of 5
    bool operator==(Panel const&) const = default;
};
struct Layout {
    std::array<std::vector<Panel>, regionCount> regions;
    bool operator==(Layout const&) const = default;
};
// Today's look: favourites over the plan on the left, the item list on the right.
inline Layout defaultLayout() {
    Layout out;
    out.regions[Left] = {{Page::Favourites, 50}, {Page::Plan, 50}};
    out.regions[Right] = {{Page::Items, 100}};
    return out;
}
// Each page once (the first kept), shares of 5 to 100 in steps of 5 that add up to 100 in each region.
inline Layout normalized(Layout layout) {
    std::array<bool, pageCount> seen{};
    for (auto& list : layout.regions) {
        std::vector<Panel> kept;
        for (auto panel : list) {
            int const page = static_cast<int>(panel.page);
            if (page < 0 || page >= pageCount || seen[static_cast<size_t>(page)]) continue;
            seen[static_cast<size_t>(page)] = true;
            panel.share = std::clamp(panel.share, 1, 100);
            kept.push_back(panel);
        }
        int sum = 0;
        for (auto const& panel : kept) sum += panel.share;
        for (auto& panel : kept) panel.share = std::max(5, static_cast<int>(std::lround(panel.share * 100.0 / sum / 5)) * 5);
        if (!kept.empty()) {
            int total = 0;
            for (auto const& panel : kept) total += panel.share;
            // What rounding left over goes to the largest panel, which can best take it.
            auto largest = std::max_element(kept.begin(), kept.end(), [](Panel const& a, Panel const& b) { return a.share < b.share; });
            largest->share = std::max(5, largest->share + 100 - total);
        }
        list = std::move(kept);
    }
    return layout;
}

struct Box {
    float x = 0, y = 0, w = 0, h = 0;
    bool has(float px, float py) const { return w > 0 && h > 0 && px >= x && px < x + w && py >= y && py < y + h; }
    bool operator==(Box const&) const = default;
};
// The smallest panel: a page's heading and one row of cells (beside), or two cells across (above, below).
inline constexpr float heading = 14, cell = 18, gap = 4;
inline constexpr float minLength(int region) { return across(region) ? 2 * cell + 4 : heading + cell + 3; }
inline constexpr float minDepth = heading + cell + 3; // how deep a region above or below must be
struct Regions {
    std::array<Box, regionCount> boxes;
    bool usable(int region) const {
        auto const& b = boxes[static_cast<size_t>(region)];
        return across(region) ? b.w >= minLength(region) && b.h >= minDepth : b.h >= minLength(region) && b.w >= cell + 4;
    }
};
// `windowLeft`/`windowRight`: what the game's window and the room the centre keeps cover across;
// `windowTop`/`windowBottom`: the game's window down. `lowest`: where the bottom region ends (above the
// key hints and the search field). `foot`: units the left region leaves at its end for the open
// container's line.
inline Regions regionsAround(float screenWidth, float screenHeight, float windowLeft, float windowRight, float windowTop,
                             float windowBottom, float lowest, float foot = 0) {
    Regions out;
    out.boxes[Left] = {0, 0, std::max(0.f, windowLeft - gap), std::max(0.f, screenHeight - foot)};
    float const rightX = windowRight + gap;
    out.boxes[Right] = {rightX, 0, std::max(0.f, screenWidth - rightX), screenHeight};
    out.boxes[Top] = {windowLeft, 0, std::max(0.f, windowRight - windowLeft), std::max(0.f, windowTop - 2)};
    out.boxes[Bottom] = {windowLeft, windowBottom + 2, std::max(0.f, windowRight - windowLeft), std::max(0.f, lowest - windowBottom - 2)};
    return out;
}

struct Placed {
    Page page = Page::Items;
    Box box;
    int region = Left; // where the layout puts it
    int shownIn = Left; // where it shows: a side region for one lent by a region too low
    bool first = true;  // the first panel shown in its region, which has no rule above it
};
// Each panel's box. A page whose feature is off (`on`) is left out and its share goes to the others. Every
// panel keeps its smallest size, taken from the others in proportion. A region above or below too low for
// one row lends its panels to the end of a side region (above to the left, below to the right) on this
// screen only.
inline std::vector<Placed> place(Layout const& layout, Regions const& regions, std::array<bool, pageCount> const& on) {
    struct Entry { Panel panel; int region; };
    std::array<std::vector<Entry>, regionCount> lists;
    for (int r = 0; r < regionCount; ++r)
        for (auto const& panel : layout.regions[static_cast<size_t>(r)])
            if (on[static_cast<size_t>(panel.page)]) lists[static_cast<size_t>(r)].push_back({panel, r});
    for (int r : {Top, Bottom}) {
        auto& list = lists[static_cast<size_t>(r)];
        if (list.empty() || regions.usable(r)) continue;
        auto& to = lists[static_cast<size_t>(r == Top ? Left : Right)];
        for (auto entry : list) entry.panel.share = 20, to.push_back(entry);
        list.clear();
    }
    std::vector<Placed> out;
    for (int r = 0; r < regionCount; ++r) {
        auto const& list = lists[static_cast<size_t>(r)];
        Box const area = regions.boxes[static_cast<size_t>(r)];
        if (list.empty() || !regions.usable(r)) continue;
        bool const side = across(r);
        float const length = side ? area.w : area.h, least = minLength(r);
        float total = 0;
        for (auto const& entry : list) total += static_cast<float>(entry.panel.share);
        std::vector<float> sizes;
        for (auto const& entry : list) sizes.push_back(length * static_cast<float>(entry.panel.share) / total);
        if (least * static_cast<float>(list.size()) >= length) std::fill(sizes.begin(), sizes.end(), length / static_cast<float>(list.size()));
        else
            for (int pass = 0; pass < 4; ++pass) {
                float shortBy = 0, spare = 0;
                for (float s : sizes) shortBy += std::max(0.f, least - s), spare += std::max(0.f, s - least);
                if (shortBy <= 0 || spare <= 0) break;
                for (float& s : sizes) s = s < least ? least : s - (s - least) / spare * shortBy;
            }
        float at = side ? area.x : area.y;
        float const end = side ? area.x + area.w : area.y + area.h;
        for (size_t i = 0; i < list.size(); ++i) {
            float const size = i + 1 == list.size() ? end - at : std::floor(sizes[i]);
            Box const box = side ? Box{at, area.y, size, area.h} : Box{area.x, at, area.w, size};
            out.push_back({list[i].panel.page, box, list[i].region, r, i == 0});
            at += size;
        }
    }
    return out;
}
inline Placed const* find(std::vector<Placed> const& placed, Page page) {
    auto it = std::find_if(placed.begin(), placed.end(), [&](Placed const& p) { return p.page == page; });
    return it == placed.end() ? nullptr : &*it;
}
}
