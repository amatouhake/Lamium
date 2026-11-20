#include "features/recipes/RecipeViewDraw.h"
#include "features/recipes/RecipeService.h"
#include "features/recipes/RecipeState.h"
#include "features/recipes/RecipeText.h"
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
    bool uses = false, choose = false;
    int tab = 0, page = 0;
    // Its recipes by station, found once: what uses an item is a walk over the whole table.
    std::vector<ViewTab> tabs;
    std::shared_ptr<service::Table const> from;
    std::shared_ptr<Choice const> chosen;
};
std::vector<Target> trail; // the last is shown; the back key goes one back
// What the last frame drew that a press acts on: an item, a tab, one of the heading's buttons, a pager's
// button or a recipe's "place".
enum class Act { Item, Tab, Favourite, Wanted, Place, PagePrev, PageNext, HowTo };
struct Region {
    float x, y, w, h;
    Act act = Act::Item;
    std::string item; // Item: its key
    int tab = -1; // Tab: which; Place: the recipe
    bool has(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
std::vector<Region> regions;
}
bool isOpen() { return !trail.empty(); }
void open(std::string item, bool uses, bool choose) {
    if (!trail.empty() && trail.back().item == item && trail.back().uses == uses) { trail.back().choose = choose; return; }
    if (trail.size() >= 64) trail.erase(trail.begin());
    trail.push_back({std::move(item), uses, choose, 0, 0, {}, {}, {}});
}
void close() {
    trail.clear();
    regions.clear();
}
namespace {
// "Place": an arrow down onto a line, the grid it lays into.
void placeMark(MinecraftUIRenderContext& context, float x, float y, ui::Rgb color) {
    using namespace lamium::ui;
    fill(context, x + 5, y + 2, 1, 4, color);
    fill(context, x + 3, y + 5, 5, 1, color);
    fill(context, x + 4, y + 6, 3, 1, color);
    fill(context, x + 5, y + 7, 1, 1, color);
    fill(context, x + 2, y + 8, 7, 1, color);
}
// In use: a small check.
void inUseMark(MinecraftUIRenderContext& context, float x, float y, ui::Rgb color) {
    using namespace lamium::ui;
    fill(context, x, y + 2, 1, 2, color);
    fill(context, x + 1, y + 3, 1, 2, color);
    fill(context, x + 2, y + 4, 1, 1, color);
    for (int i = 0; i < 5; ++i) fill(context, x + 3 + static_cast<float>(i), y + 3 - static_cast<float>(i) * .75f, 1, 1.5f, color);
}
}
void draw(MinecraftUIRenderContext& context, Input const& input, Late& late) {
    using namespace lamium::ui;
    if (trail.empty()) return;
    if (input.close) { close(); return; }
    if (input.back && trail.size() > 1) trail.pop_back();
    else if (!input.clicks.empty()) {
        // One press a frame: it acts on what the last frame drew, which the next press no longer sees.
        bool const left = input.clicks.front();
        for (auto const& region : regions) {
            if (!region.has(input.pointerX, input.pointerY)) continue;
            std::string const shownItem = trail.back().item; // open() below may move the trail
            if (region.act == Act::Item) open(std::string(region.item), !left);
            else if (!left) {}
            else if (region.act == Act::Tab) trail.back().tab = region.tab, trail.back().page = 0;
            else if (region.act == Act::Favourite) state::toggleFavourite(shownItem);
            else if (region.act == Act::Wanted) state::toggleWanted(shownItem);
            else if (region.act == Act::HowTo) late.howTo = region.item;
            else if (region.act == Act::PagePrev) trail.back().page -= 1;
            else if (region.act == Act::PageNext) trail.back().page += 1;
            else if (region.act == Act::Place && input.placer) {
                // Laid in: the grid is what the player wants to see next.
                if (input.placer->run(region.tab, input.shift)) { late.placed = true; return; }
            }
            break;
        }
    }
    regions.clear();
    auto& at = trail.back();
    at.page += input.wheel;
    auto const l = recipeViewAt(input.screenWidth, input.screenHeight);
    // The game's window lies under the view and still answers the pointer: nothing of it may show through.
    float const coverLeft = input.coverRight > input.coverLeft ? std::min(l.left, input.coverLeft) : l.left;
    float const coverRight = input.coverRight > input.coverLeft ? std::max(l.left + l.width, input.coverRight) : l.left + l.width;
    windowPanel(context, coverLeft, l.top, coverRight - coverLeft, l.height);
    OnWindow const onWindowScope;
    static_assert(RecipeViewLayout::tabsAbove == windowTabRise);
    // Two heading lines: the item's name with its two switches at the right end; under it the station's
    // name and the pager. The stations are the game's tabs on the window's top edge.
    float headEnd = l.left + l.width - 7;
    // Each on or off, like the world map's layers: two buttons sharing a border, green when on (a trial
    // beside "a name and a switch", 2026-11-19). The one that is on is drawn last, so its frame is whole.
    bool const wanted = state::wanted().has(at.item), liked = state::notes().favourite(at.item);
    std::string const in = translated("recipes.wanted"), favourite = translated("recipes.favourite");
    float const wantedWidth = textWidth(context, in) + 10, likedWidth = textWidth(context, favourite) + 10;
    // Without the plan there is nothing to want an item for: the favourite button stands alone.
    // The heading's first line keeps room from the window's edge (L-367).
    constexpr float lineOne = 7;
    Region const wantedRegion{headEnd - (input.plan ? wantedWidth : 1), l.top + lineOne - 1, input.plan ? wantedWidth : 1, 11, Act::Wanted, {}, -1};
    Region const likedRegion{wantedRegion.x - likedWidth + 1, l.top + lineOne - 1, likedWidth, 11, Act::Favourite, {}, -1};
    for (bool pass : {false, true}) {
        if (input.favourites && liked == pass)
            drawOnOffButton(context, likedRegion.x, likedRegion.y, likedRegion.w, likedRegion.h, favourite, likedRegion.has(input.pointerX, input.pointerY), liked);
        if (input.plan && wanted == pass)
            drawOnOffButton(context, wantedRegion.x, wantedRegion.y, wantedRegion.w, wantedRegion.h, in, wantedRegion.has(input.pointerX, input.pointerY), wanted);
    }
    if (input.plan) regions.push_back(wantedRegion);
    if (input.favourites) regions.push_back(likedRegion);
    headEnd = (input.favourites ? likedRegion.x : input.plan ? wantedRegion.x : headEnd) - 6;
    // The item's icon leads its name; under the pointer either names the item in its own tooltip, so a name
    // cut short on a narrow screen can still be read.
    {
        auto const& shown = schematic::items::shown(at.item);
        if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, l.left + 7, l.top + lineOne - 1, .625f});
        float const nameX = l.left + 20;
        windowLabel(context, nameX, l.top + lineOne, headEnd - nameX, translated(at.uses ? "recipes.heading.uses" : "recipes.heading.makes", shown.name));
        if (input.pointerX >= l.left + 7 && input.pointerX < headEnd && input.pointerY >= l.top + lineOne - 1 && input.pointerY < l.top + lineOne + 10)
            late.tip = shown.name, late.hovered = at.item;
    }
    auto line = [&](std::string_view key) {
        windowLabel(context, l.left + 6, l.top + RecipeViewLayout::head + 4, l.width - 12, translated(key), true);
    };
    auto const reading = service::want();
    auto const table = service::table();
    if (!table) {
        line(reading == service::State::Paused ? "schematic.raw.paused"
             : reading == service::State::Unreadable ? "schematic.raw.unreadable" : "schematic.raw.reading");
        return;
    }
    auto const& index = table->index;
    auto const key = splitItemKey(at.item);
    Item const id = index.find(key.name, key.aux);
    auto const choice = state::choice(table);
    if (at.from != table || at.chosen != choice) at.tabs = viewTabs(index, *choice, id, at.uses), at.from = table, at.chosen = choice;
    auto const& tabs = at.tabs;
    if (tabs.empty()) { line(at.uses ? "recipes.none.uses" : "recipes.none.makes"); return; }
    at.tab = std::clamp(at.tab, 0, static_cast<int>(tabs.size()) - 1);
    auto const& tab = tabs[static_cast<size_t>(at.tab)];
    Station const station = tab.station;
    int const count = static_cast<int>(tab.recipes.size()), pages = l.pages(station, count);
    at.page = std::clamp(at.page, 0, pages - 1);
    // Stations are tabs, each by its block's icon and named under the pointer; the pager keeps the line's right end.
    float x = l.left + 5;
    float const tabEnd = l.left + l.width - 5;
    for (size_t i = 0; i < tabs.size(); ++i) {
        std::string name = stationName(tabs[i].station);
        auto const picture = stationItem(tabs[i].station);
        auto const* stack = picture.empty() ? nullptr : schematic::items::iconStack(schematic::items::shown(std::string(picture)).icon);
        float const w = stack ? windowTabWidth : textWidth(context, name) + 14;
        if (x + w > tabEnd) break;
        Region const region{x, l.top - windowTabRise, w - 1, windowTabRise, Act::Tab, {}, static_cast<int>(i)};
        bool const hot = region.has(input.pointerX, input.pointerY);
        bool const selected = static_cast<int>(i) == at.tab;
        if (stack) {
            windowTab(context, x, l.top, w, {}, selected, hot);
            late.icons.push_back({stack, x + (w - 16) / 2, windowTabIconY(l.top), 1.f});
            if (hot) late.tip = std::move(name);
        } else windowTab(context, x, l.top, w, std::move(name), selected, hot);
        regions.push_back(region);
        x += w - 1; // neighbours share their edge
    }
    // The second line: the station's name, and the pager at its right end.
    float const lineTwo = l.top + lineOne + 14, lineEnd = l.left + l.width - 7;
    auto const pager = drawPager(context, lineEnd, lineTwo, at.page, pages, input.pointerX, input.pointerY);
    windowLabel(context, l.left + 7, lineTwo + 1, pager.prevX - 4 - (l.left + 7), stationName(station), true);
    if (pager.prevOn) regions.push_back({pager.prevX, pager.y, Pager::size, Pager::size, Act::PagePrev, {}, -1});
    if (pager.nextOn) regions.push_back({pager.nextX, pager.y, Pager::size, Pager::size, Act::PageNext, {}, -1});
    // An ingredient any of several items satisfies shows them in turn.
    auto const turn = static_cast<size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1500);
    int const first = at.page * l.perPage(station);
    for (int i = 0; i < l.perPage(station) && first + i < count; ++i) {
        int const number = tab.recipes[static_cast<size_t>(first + i)];
        auto const& recipe = index.recipes[static_cast<size_t>(number)];
        float const bx = l.blockX(station, count, i), by = l.blockY(station, count, i);
        auto const block = viewBlock(recipe);
        // Each recipe on a raised face of the window's own edge, so where one ends is seen (L-370).
        raisedBox(context, bx - 3, by - 3, RecipeViewLayout::block + 6, RecipeViewLayout::blockHeight(station) + 1, palette::vanilla::window,
                  palette::vanilla::windowLight, palette::vanilla::buttonShadow);
        // A slot as the game draws its own; `lacks`: on the game's red, an ingredient nothing carried serves. `group`:
        // the slot takes any of a group's members, and names the group and shows them under the pointer.
        auto place = [&](float sx, float sy, Item item, Count amount, bool lacks = false, int group = -1) {
            Region region{sx, sy, 18, 18, Act::Item, {}, -1};
            bool const named = item >= 0 && item < static_cast<Item>(index.items.size());
            bool const hovered = named && region.has(input.pointerX, input.pointerY);
            windowSlot(context, sx, sy, hovered, lacks);
            if (!named) return;
            region.item = index.items[static_cast<size_t>(item)];
            auto const& shown = schematic::items::shown(region.item);
            if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, sx + 1, sy + 1});
            if (amount > 1) late.counts.push_back({sx, sy, std::to_string(amount)});
            if (group >= 0 && !state::notes().picks.contains(groupKey(index, group))) late.marks.push_back({sx + 1, sy + 1});
            if (hovered && group >= 0) {
                late.tip = groupName(index, group);
                late.tipItems.clear();
                for (Item member : index.groups[static_cast<size_t>(group)].members) late.tipItems.push_back(index.items[static_cast<size_t>(member)]);
            } else if (hovered) late.tip = shown.name, late.hovered = region.item;
            regions.push_back(std::move(region));
        };
        // Where the screen has a grid this recipe fits, it can be laid in; a cell nothing carried serves is marked.
        PlacePlan laid;
        if (input.placer && recipe.station == Station::Crafting) laid = input.placer->preview(recipe, input.shift);
        auto const& picks = state::notes().picks;
        for (auto const& part : block.slots) {
            Item item = -1;
            int group = -1;
            if (part.cell >= 0) {
                auto const& cell = recipe.cells[static_cast<size_t>(part.cell)];
                item = cell.id;
                if (cell.group) {
                    group = cell.id;
                    auto const& members = index.groups[static_cast<size_t>(cell.id)].members;
                    // Asked what an item is used in, a slot it fits shows it; else the member "How to make" names, or
                    // the members in turn.
                    bool const asked = at.uses && std::find(members.begin(), members.end(), id) != members.end();
                    Item picked = -1;
                    if (auto found = picks.find(groupKey(index, cell.id)); found != picks.end()) picked = index.findKey(found->second);
                    if (std::find(members.begin(), members.end(), picked) == members.end()) picked = -1;
                    item = members.empty() ? -1 : asked ? id : picked >= 0 ? picked : members[turn % members.size()];
                }
            }
            place(bx + part.x, by + part.y, item, 1,
                  part.cell >= 0 && std::find(laid.missing.begin(), laid.missing.end(), part.cell) != laid.missing.end(), group);
        }
        changeArrow(context, bx + block.arrowX, by + block.arrowY, 1, palette::vanilla::windowInk);
        place(bx + block.productX, by + block.productY, recipe.makes, recipe.count);
        // "Place": a small square button with a picture, named under the pointer; faint, with the reason,
        // when something is missing.
        if (laid.fits) {
            float const mx = bx + block.marksX, my = by + block.marksY, size = 11;
            Region const region{mx, my, size, size, Act::Place, {}, number};
            bool const over = region.has(input.pointerX, input.pointerY);
            Rgb const mark = drawButtonBox(context, mx, my, size, size, laid.ready() && over, ButtonKind::Plain, laid.ready());
            placeMark(context, mx, my, mark);
            if (over) late.tip = translated(laid.ready() ? "recipes.place" : "recipes.placeMissing");
            if (laid.ready()) regions.push_back(region);
        }
        // The recipe an item with several is made by, as "How to make" has it: a check when the player chose it,
        // "automatic" when the rule did; a press goes to its line there (L-370).
        bool const inUse = id >= 0 && !at.uses && choice->recipe[static_cast<size_t>(id)] == number && recipeOptions(index, id).size() > 1;
        if (inUse) {
            // The item as the table keys it, as "How to make" and the notes do.
            std::string const& itemKey = index.items[static_cast<size_t>(id)];
            bool const chosen = state::notes().choices.contains(itemKey);
            float const ux = bx + block.productX - 1, uy = by + block.productY + 20, uw = 20, uh = 8;
            if (chosen) inUseMark(context, ux + 6, uy, palette::accentDeep);
            else labelScaled(context, ux, uy - 1, uw, translated("recipes.howTo.auto"), .5f, palette::vanilla::windowFaint, Align::Center, false);
            // A press goes to "How to make", the Recipes window's (with the plan off it holds only that).
            Region const region{ux, uy - 2, uw, uh + 2, Act::HowTo, itemKey, -1};
            if (region.has(input.pointerX, input.pointerY))
                late.tip = translated(chosen ? "recipes.mark.default" : "recipes.howTo.auto"), late.tipLines = {translated("recipes.howTo.change")};
            regions.push_back(region);
        }
        if (anyOrder(recipe)) {
            float const nx = bx + block.noteX, ny = by + block.noteY;
            anyOrderMark(context, nx, ny, palette::vanilla::windowInk);
            if (input.pointerX >= nx - 2 && input.pointerX < nx + anyOrderMarkSize + 2 && input.pointerY >= ny - 2 && input.pointerY < ny + anyOrderMarkSize + 2)
                late.tip = translated("recipes.mark.anyOrder");
        }
    }
}
}
