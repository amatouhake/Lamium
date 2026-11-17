#include "features/recipes/RecipeViewDraw.h"
#include "features/recipes/RecipeService.h"
#include "features/recipes/RecipeView.h"
#include "features/schematic/SchematicItems.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include <algorithm>
#include <chrono>
#include <format>

namespace lamium::recipes::view {
namespace {
struct Target {
    std::string item;
    bool uses = false;
    int tab = 0, page = 0;
    // Its recipes by station, found once: what uses an item is a walk over the whole table.
    std::vector<ViewTab> tabs;
    std::shared_ptr<service::Table const> from;
};
std::vector<Target> trail; // the last is shown; Backspace goes one back
// What the last frame drew that a press acts on: an item, or a tab.
struct Region {
    float x, y, w, h;
    std::string item;
    int tab = -1;
    bool has(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
std::vector<Region> regions;
// A station by the name of its block's item, so it reads as the game names it.
std::string stationName(Station station) {
    auto const item = stationItem(station);
    return item.empty() ? std::string("-") : schematic::items::shown(std::string(item)).name;
}
}
bool isOpen() { return !trail.empty(); }
void open(std::string item, bool uses) {
    if (!trail.empty() && trail.back().item == item && trail.back().uses == uses) return;
    if (trail.size() >= 64) trail.erase(trail.begin());
    trail.push_back({std::move(item), uses, 0, 0, {}, {}});
}
void close() {
    trail.clear();
    regions.clear();
}
void draw(MinecraftUIRenderContext& context, Input const& input, Late& late) {
    using namespace lamium::ui;
    if (trail.empty()) return;
    if (input.close) { close(); return; }
    if (input.back && trail.size() > 1) trail.pop_back();
    else if (!input.clicks.empty()) {
        // One press a frame: it acts on what the last frame drew, which the next press no longer sees.
        for (auto const& region : regions) {
            if (!region.has(input.pointerX, input.pointerY)) continue;
            if (region.tab >= 0) trail.back().tab = region.tab, trail.back().page = 0;
            else open(std::string(region.item), !input.clicks.front());
            break;
        }
    }
    regions.clear();
    auto& at = trail.back();
    at.page += input.wheel;
    auto const l = recipeViewAt(input.screenWidth, input.screenHeight);
    panel(context, l.left, l.top, l.width, l.height, opacity::floating);
    label(context, l.left + 6, l.top + 5, l.width - 12,
          translated(at.uses ? "recipes.heading.uses" : "recipes.heading.makes", schematic::items::shown(at.item).name));
    auto line = [&](std::string_view key) {
        label(context, l.left + 6, l.top + RecipeViewLayout::head + 4, l.width - 12, translated(key), palette::dim);
    };
    auto const state = service::want();
    auto const table = service::table();
    if (!table) {
        line(state == service::State::Paused ? "schematic.raw.paused"
             : state == service::State::Unreadable ? "schematic.raw.unreadable" : "schematic.raw.reading");
        return;
    }
    auto const& index = table->index;
    Item const id = index.find(at.item);
    if (at.from != table) at.tabs = viewTabs(index, table->choice, id, at.uses), at.from = table;
    auto const& tabs = at.tabs;
    if (tabs.empty()) { line(at.uses ? "recipes.none.uses" : "recipes.none.makes"); return; }
    at.tab = std::clamp(at.tab, 0, static_cast<int>(tabs.size()) - 1);
    auto const& tab = tabs[static_cast<size_t>(at.tab)];
    Station const station = tab.station;
    int const count = static_cast<int>(tab.recipes.size()), pages = l.pages(station, count);
    at.page = std::clamp(at.page, 0, pages - 1);
    // Stations are tabs; the pager keeps the line's right end.
    float x = l.left + 5;
    float const tabY = l.top + 16, tabEnd = l.left + l.width - 40;
    for (size_t i = 0; i < tabs.size(); ++i) {
        std::string name = stationName(tabs[i].station);
        float const w = textWidth(context, name) + 10;
        if (x + w > tabEnd) break;
        Region const region{x, tabY, w, 11, {}, static_cast<int>(i)};
        drawButton(context, x, tabY, w, 11, std::move(name), region.has(input.pointerX, input.pointerY),
                   static_cast<int>(i) == at.tab ? ButtonKind::Primary : ButtonKind::Plain);
        regions.push_back(region);
        x += w + 2;
    }
    if (pages > 1) label(context, l.left, tabY + 2, l.width - 6, std::format("{} / {}", at.page + 1, pages), palette::dim, Align::Right);
    // An ingredient any of several items satisfies shows them in turn.
    auto const turn = static_cast<size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1500);
    int const first = at.page * l.perPage(station);
    for (int i = 0; i < l.perPage(station) && first + i < count; ++i) {
        int const number = tab.recipes[static_cast<size_t>(first + i)];
        auto const& recipe = index.recipes[static_cast<size_t>(number)];
        float const bx = l.blockX(station, count, i), by = l.blockY(station, count, i);
        auto const block = viewBlock(recipe);
        auto place = [&](float sx, float sy, Item item, Count amount) {
            Region region{sx, sy, 18, 18, {}, -1};
            bool const named = item >= 0 && item < static_cast<Item>(index.items.size());
            bool const hovered = named && region.has(input.pointerX, input.pointerY);
            slot(context, sx, sy, hovered);
            if (!named) return;
            region.item = index.items[static_cast<size_t>(item)];
            auto const& shown = schematic::items::shown(region.item);
            if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, sx + 1, sy + 1});
            if (amount > 1) late.counts.push_back({sx, sy, std::to_string(amount)});
            if (hovered) late.tip = shown.name;
            regions.push_back(std::move(region));
        };
        for (auto const& part : block.slots) {
            Item item = -1;
            if (part.cell >= 0) {
                auto const& cell = recipe.cells[static_cast<size_t>(part.cell)];
                item = cell.id;
                if (cell.group) {
                    auto const& members = index.groups[static_cast<size_t>(cell.id)].members;
                    item = members.empty() ? -1 : members[turn % members.size()];
                }
            }
            place(bx + part.x, by + part.y, item, 1);
        }
        changeArrow(context, bx + block.arrowX, by + block.arrowY, 1, palette::dim);
        place(bx + block.productX, by + block.productY, recipe.makes, recipe.count);
        float mx = bx + block.marksX, my = by + block.marksY;
        float const marksEnd = bx + RecipeViewLayout::block;
        auto mark = [&](std::string text) {
            if (mx > bx + block.marksX && mx + tagWidth(context, text) > marksEnd) mx = bx + block.marksX, my += 11;
            mx += drawTag(context, mx, my, std::move(text), marksEnd - mx) + 2;
        };
        // The recipe the item is made by, where it has several; changing it comes with the plan.
        if (!at.uses && count + static_cast<int>(tabs.size()) > 2 && table->choice.recipe[static_cast<size_t>(id)] == number)
            mark(translated("recipes.mark.default"));
        if (anyOrder(recipe)) mark(translated("recipes.mark.anyOrder"));
        if (fitsSmallGrid(recipe)) mark(translated("recipes.mark.small"));
    }
}
}
