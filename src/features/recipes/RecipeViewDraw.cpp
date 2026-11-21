#include "features/recipes/CountText.h"
#include "features/recipes/RecipeViewDraw.h"
#include "features/recipes/GroupLook.h"
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
    bool tabTaken = false; // the first tab was picked for the screen's station (L-380)
    // Its recipes by station, found once: what uses an item is a walk over the whole table.
    std::vector<ViewTab> tabs;
    std::shared_ptr<service::Table const> from;
};
std::vector<Target> trail; // the last is shown; the back key goes one back
// What the last frame drew that a press acts on: an item, a tab, one of the heading's buttons, a pager's
// button, a recipe's "place" or its check box.
enum class Act { Item, Tab, Favorite, Wanted, Place, PagePrev, PageNext, Choose };
struct Region {
    float x, y, w, h;
    Act act = Act::Item;
    std::string item; // Item: its key; Choose: the item whose recipe it sets
    int tab = -1; // Tab: which; Place: the recipe
    std::string choice; // Choose: the recipe's choiceKey, empty to go back to automatic
    bool has(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
std::vector<Region> regions;
}
bool isOpen() { return !trail.empty(); }
void open(std::string item, bool uses, bool choose) {
    if (!trail.empty() && trail.back().item == item && trail.back().uses == uses) { trail.back().choose = choose; return; }
    if (trail.size() >= 64) trail.erase(trail.begin());
    trail.push_back({std::move(item), uses, choose, 0, 0, false, {}, {}});
}
void close() {
    trail.clear();
    regions.clear();
}
namespace {
// "Place": an arrow down onto a line, the grid it lays into.
}
namespace {
// The brewing stand's parts on a brewing face (L-361, demos/brewing-view.html F), from the block's corner: the fuel's
// tube going right, down through a short coil and right into the gauge with bubbles over it; three tubes from the
// ingredient, one to each bottle; the arrow at the ingredient's right; the empty middle bottle. The bottle is the
// game's own image, at its own size; the rest is drawn on whole units after the game's images, whose dots shrunk
// to this face did not match the slots' (maintainer 2026-11-21).
void brewingParts(MinecraftUIRenderContext& context, float x, float y) {
    using namespace lamium::ui;
    namespace v = palette::vanilla;
    auto tube = [&](float tx, float ty, float w, float h) {
        fill(context, x + tx, y + ty, w, h, v::windowInk);
        if (w > h) fill(context, x + tx, y + ty + h, w, 1, v::windowLight); else fill(context, x + tx + w, y + ty, 1, h, v::windowLight);
    };
    tube(18, 9, 4, 1);
    // The coil: two turns between the tube from the fuel and the one into the gauge.
    for (int i = 0; i < 2; ++i) {
        fill(context, x + 21, y + 11 + 2 * static_cast<float>(i), 7, 1, v::slotDark);
        fill(context, x + 21, y + 12 + 2 * static_cast<float>(i), 7, 1, v::windowLight);
    }
    tube(28, 15, 3, 1);
    // The gauge: an outlined bar, the fuel in it with a lighter top.
    frame(context, x + 31, y + 14, 12, 4, v::windowInk);
    fill(context, x + 32, y + 15, 10, 1, v::brewingFuelLight);
    fill(context, x + 32, y + 16, 10, 1, v::brewingFuel);
    // The bubbles over it, rings low and dots higher, as the stand's rise.
    frame(context, x + 34, y + 9, 3, 3, v::windowInk);
    frame(context, x + 37, y + 5, 3, 3, v::windowInk);
    fill(context, x + 35, y + 2, 2, 2, v::windowInk);
    fill(context, x + 38, y + 0, 1, 1, v::windowInk);
    // The arrow down at the ingredient's right: a plain shaft and head (an outlined, lit one read poorly so small).
    fill(context, x + 68, y + 1, 1, 11, v::windowInk);
    fill(context, x + 66, y + 12, 5, 1, v::windowInk);
    fill(context, x + 67, y + 13, 3, 1, v::windowInk);
    fill(context, x + 68, y + 14, 1, 1, v::windowInk);
    tube(50, 18, 1, 8), tube(42, 26, 8, 1);
    tube(55, 18, 1, 12);
    tube(60, 18, 1, 8), tube(61, 26, 7, 1);
    images(context, "textures/ui/bottle_empty", {{x + 47, y + 31, 16, 16}});
}
// The place button's line for Ctrl (L-379): what it does, or why it would not now.
std::string craftLine(CraftOnto check) {
    using lamium::ui::translated;
    switch (check) {
    case CraftOnto::Ready: case CraftOnto::Missing: return translated("recipes.craft");
    case CraftOnto::PlaceOnly: return translated("recipes.craft.placeOnly");
    }
    return {};
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
            else if (region.act == Act::Favorite) state::toggleFavorite(shownItem);
            else if (region.act == Act::Wanted) state::toggleWanted(shownItem);
            else if (region.act == Act::Choose) state::setChoice(region.item, region.choice);
            else if (region.act == Act::PagePrev) trail.back().page -= 1;
            else if (region.act == Act::PageNext) trail.back().page += 1;
            else if (region.act == Act::Place && input.placer && input.ctrl && input.placer->places(Kind::Crafting)) input.placer->craft(region.tab);
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
    bool const wanted = state::wanted().has(at.item), liked = state::notes().favorite(at.item);
    std::string const in = translated("recipes.wanted"), favorite = translated("recipes.favorite");
    float const wantedWidth = textWidth(context, in) + 10, likedWidth = textWidth(context, favorite) + 10;
    // Without the plan there is nothing to want an item for: the favorite button stands alone.
    // The heading's first line keeps room from the window's edge (L-367).
    constexpr float lineOne = 7;
    Region const wantedRegion{headEnd - (input.plan ? wantedWidth : 1), l.top + lineOne - 1, input.plan ? wantedWidth : 1, 11, Act::Wanted, {}, -1};
    Region const likedRegion{wantedRegion.x - likedWidth + 1, l.top + lineOne - 1, likedWidth, 11, Act::Favorite, {}, -1};
    for (bool pass : {false, true}) {
        if (input.favorites && liked == pass)
            drawOnOffButton(context, likedRegion.x, likedRegion.y, likedRegion.w, likedRegion.h, favorite, likedRegion.has(input.pointerX, input.pointerY), liked);
        if (input.plan && wanted == pass)
            drawOnOffButton(context, wantedRegion.x, wantedRegion.y, wantedRegion.w, wantedRegion.h, in, wantedRegion.has(input.pointerX, input.pointerY), wanted);
    }
    if (input.plan) regions.push_back(wantedRegion);
    if (input.favorites) regions.push_back(likedRegion);
    headEnd = (input.favorites ? likedRegion.x : input.plan ? wantedRegion.x : headEnd) - 6;
    // The item's icon leads its name; under the pointer either names the item in its own tooltip, so a name
    // cut short on a narrow screen can still be read.
    {
        auto const& shown = schematic::items::shown(at.item);
        if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, l.left + 7, l.top + lineOne - 1, .625f});
        float const nameX = l.left + 20;
        windowLabel(context, nameX, l.top + lineOne, headEnd - nameX, translated(at.uses ? "recipes.heading.uses" : "recipes.heading.makes", shown.name));
        // The item itself answers presses as any item does (a press its recipes, a right press its uses), so an
        // item with none of one kind is no dead end for the mouse (maintainer 2026-11-21).
        Region heading{l.left + 7, l.top + lineOne - 1, headEnd - (l.left + 7), 11, Act::Item, at.item, -1};
        if (heading.has(input.pointerX, input.pointerY)) late.tip = shown.name, late.hovered = at.item;
        regions.push_back(std::move(heading));
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
    // The order (the recipe in use first, its tab first) is taken when the item is shown and kept while it stays
    // shown: choosing a recipe here must not move it or change the tab (L-379).
    if (at.from != table) at.tabs = viewTabs(index, *choice, id, at.uses), at.from = table;
    auto const& tabs = at.tabs;
    if (tabs.empty()) { line(at.uses ? "recipes.none.uses" : "recipes.none.makes"); return; }
    // The tab first shown (maintainer 2026-11-21), once, so a tab chosen afterwards stays: at a station its own tab
    // where the item has one (beef at the smoker on smoking); elsewhere the tab of the recipe in use, which leads
    // it: the tabs keep their order, so this is where the choice is seen.
    if (!at.tabTaken) {
        at.tabTaken = true;
        int const inUse = !at.uses && id >= 0 && static_cast<size_t>(id) < choice->recipe.size() ? choice->recipe[static_cast<size_t>(id)] : -1;
        bool found = false;
        if (input.placer)
            for (size_t i = 0; i < tabs.size() && !found; ++i)
                if (input.placer->places(tabs[i].kind)) at.tab = static_cast<int>(i), found = true;
        for (size_t i = 0; i < tabs.size() && !found && inUse >= 0; ++i)
            if (std::find(tabs[i].recipes.begin(), tabs[i].recipes.end(), inUse) != tabs[i].recipes.end()) at.tab = static_cast<int>(i), found = true;
    }
    at.tab = std::clamp(at.tab, 0, static_cast<int>(tabs.size()) - 1);
    auto const& tab = tabs[static_cast<size_t>(at.tab)];
    Station const station = tab.station;
    int const count = static_cast<int>(tab.recipes.size()), pages = l.pages(station, count);
    at.page = std::clamp(at.page, 0, pages - 1);
    // Processes are tabs (L-376), each by its usual station's icon and named under the pointer.
    auto tabName = [&](ViewTab const& t) { return kindName(t.kind); };
    float x = l.left + 5;
    float const tabEnd = l.left + l.width - 5;
    for (size_t i = 0; i < tabs.size(); ++i) {
        std::string name = tabName(tabs[i]);
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
    // The second line: the tab's stations as items, each once (the item's tip; a press its recipes, a right press
    // its uses, as anywhere), the process's name, and the pager at its right end.
    float const lineTwo = l.top + lineOne + 14, lineEnd = l.left + l.width - 7;
    auto const pager = drawPager(context, lineEnd, lineTwo, at.page, pages, input.pointerX, input.pointerY);
    float nameX = l.left + 7;
    for (Station s : tabStations(index, tab)) {
        auto const picture = stationItem(s);
        if (picture.empty() || nameX + 12 > pager.prevX - 4) continue;
        std::string key(picture);
        float const sy = lineTwo - 1;
        if (auto const* stack = schematic::items::iconStack(schematic::items::shown(key).icon)) late.icons.push_back({stack, nameX, sy, .75f});
        Region region{nameX, sy, 12, 12, Act::Item, key, -1};
        if (region.has(input.pointerX, input.pointerY)) late.tip = schematic::items::shown(key).name, late.hovered = key;
        regions.push_back(std::move(region));
        nameX += 13;
    }
    if (nameX > l.left + 7) nameX += 2;
    windowLabel(context, nameX, lineTwo + 1, pager.prevX - 4 - nameX, kindName(tab.kind), true);
    if (pager.prevOn) regions.push_back({pager.prevX, pager.y, Pager::size, Pager::size, Act::PagePrev, {}, -1});
    if (pager.nextOn) regions.push_back({pager.nextX, pager.y, Pager::size, Pager::size, Act::PageNext, {}, -1});
    int const first = at.page * l.perPage(station);
    for (int i = 0; i < l.perPage(station) && first + i < count; ++i) {
        int const number = tab.recipes[static_cast<size_t>(first + i)];
        auto const& recipe = index.recipes[static_cast<size_t>(number)];
        float const bx = l.blockX(station, count, i), by = l.blockY(station, count, i);
        auto const block = viewBlock(recipe);
        // Each recipe on a raised face of the window's own edge, so where one ends is seen (L-370).
        constexpr float pad = RecipeViewLayout::pad;
        raisedBox(context, bx - pad, by - pad, RecipeViewLayout::block + 2 * pad, RecipeViewLayout::blockHeight(station) + 2 * pad,
                  palette::vanilla::window, palette::vanilla::windowLight, palette::vanilla::buttonShadow);
        // A slot as the game draws its own; `lacks`: on the game's red, an ingredient nothing carried serves. `group`:
        // the slot takes any of a group's members, and names the group and shows them under the pointer.
        auto place = [&](float sx, float sy, Item item, Count amount, bool lacks = false, int group = -1, Item prefer = -1) {
            Region region{sx, sy, 18, 18, Act::Item, {}, -1};
            bool const named = item >= 0 && item < static_cast<Item>(index.items.size());
            bool const hovered = named && region.has(input.pointerX, input.pointerY);
            windowSlot(context, sx, sy, hovered, lacks);
            if (!named) return;
            region.item = index.items[static_cast<size_t>(item)];
            auto const& shown = schematic::items::shown(region.item);
            if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, sx + 1, sy + 1});
            if (amount > 1) late.counts.push_back({sx, sy, slotCountText(amount)});
            if (group >= 0) late.marks.push_back({sx + 1, sy + 1});
            if (hovered && group >= 0) {
                // The tip frames what the slot shows: it starts from the same member (`prefer`).
                groupLook::hover(group, sx, sy);
                std::map<int, Item> picked;
                if (auto found = state::notes().picks.find(groupKey(index, group)); found != state::notes().picks.end())
                    if (Item const member = index.findKey(found->second); member >= 0) picked[group] = member;
                groupLook::tip(late, index, group, picked, nullptr, prefer);
            } else if (hovered) late.tip = shown.name, late.hovered = region.item;
            regions.push_back(std::move(region));
        };
        // Where the screen has a grid this recipe fits, it can be laid in; a cell nothing carried serves is marked.
        // "Place" is on every recipe (maintainer 2026-11-21); it can be pressed where the screen takes the tab's
        // recipes and they can be laid. Under its pointer the slots of what is missing turn red, wherever it is
        // shown, so a recipe can be checked anywhere and none is red unasked.
        bool const placeable = input.placer && input.placer->places(tab.kind);
        PlacePlan laid;
        if (placeable) laid = input.placer->preview(recipe, input.shift);
        Region const placeRegion{bx + block.placeX, by + block.placeY, viewButton, viewButton, Act::Place, {}, number};
        bool const overPlace = placeRegion.has(input.pointerX, input.pointerY);
        PlacePlan const asked = overPlace && input.placer ? input.placer->need(recipe) : PlacePlan{};
        auto const& picks = state::notes().picks;
        for (auto const& part : block.slots) {
            Item item = -1;
            int group = -1;
            Item prefer = -1;
            if (part.cell >= 0) {
                auto const& cell = recipe.cells[static_cast<size_t>(part.cell)];
                item = cell.id;
                if (cell.group) {
                    group = cell.id;
                    auto const& members = index.groups[static_cast<size_t>(cell.id)].members;
                    // Asked what an item is used in, a slot it fits starts from it; else from the member "How to make"
                    // names, or the one it is known by; still unless the wheel looks through them (L-369).
                    bool const asked = at.uses && index.isMember(cell.id, id);
                    Item picked = -1;
                    if (auto found = picks.find(groupKey(index, cell.id)); found != picks.end()) picked = index.findKey(found->second);
                    if (!index.isMember(cell.id, picked)) picked = -1;
                    std::map<int, Item> one;
                    if (picked >= 0) one[cell.id] = picked;
                    prefer = asked ? id : -1;
                    item = members.empty() ? -1 : groupLook::face(index, cell.id, one, nullptr, prefer);
                }
            }
            place(bx + part.x, by + part.y, item, 1,
                  part.cell >= 0 && std::find(asked.missing.begin(), asked.missing.end(), part.cell) != asked.missing.end(), group, prefer);
        }
        if (block.brewing) {
            brewingParts(context, bx, by);
            // Blaze powder in the fuel's slot: the stand takes no other fuel (it is not part of the recipe).
            place(bx + block.fuelX, by + block.fuelY, index.find("minecraft:blaze_powder", 0), 1);
        } else changeArrow(context, bx + block.arrowX, by + block.arrowY, 1, palette::vanilla::windowInk);
        place(bx + block.productX, by + block.productY, recipe.makes, recipe.count);
        // "Place": a small square button with a picture at the block's bottom right, named under the pointer; flat
        // and darker where it cannot be pressed, its tip saying why.
        {
            bool const ready = placeable && laid.ready();
            drawPictureButton(context, placeRegion.x, placeRegion.y, viewButton, Picture::Place, overPlace, false, ready);
            if (overPlace) {
                // A button of its own explains its presses (not an item's tip, maintainer 2026-11-21).
                if (ready) {
                    late.tip = translated("recipes.place");
                    late.tipLines = {translated("recipes.place.many")};
                    if (input.placer->places(Kind::Crafting)) late.tipLines.push_back(craftLine(input.placer->craftCheck(recipe)));
                } else if (placeable && laid.fits) late.tip = translated("recipes.placeMissing");
                else if (placeable && input.placer->gridSide() > 0) late.tip = translated("recipes.place.fit");
                else {
                    late.tip = translated("recipes.place.station", stationName(usualStation(tab.kind)));
                    if (!asked.missing.empty()) late.tipLines = {translated("recipes.placeMissing")};
                }
            }
            if (ready) regions.push_back(placeRegion);
        }
        // Of an item made several ways, the check box left of "place" (L-379): empty for a recipe not in use, a
        // dark check for the one the rule picked, a green check for the player's own choice. A press chooses
        // this recipe; on the player's choice it goes back to automatic. "How to make" still lists them all.
        if (id >= 0 && !at.uses) {
            auto const options = recipeOptions(index, id);
            if (options.size() > 1 && std::find(options.begin(), options.end(), number) != options.end()) {
                std::string const& itemKey = index.items[static_cast<size_t>(id)];
                bool const inUse = choice->recipe[static_cast<size_t>(id)] == number, mine = inUse && state::notes().choices.contains(itemKey);
                Region region{bx + block.chooseX, by + block.chooseY, viewButton, viewButton, Act::Choose, itemKey, -1,
                              mine ? std::string() : choiceKey(recipe)};
                bool const over = region.has(input.pointerX, input.pointerY);
                windowCheckBox(context, region.x, region.y, viewButton, mine ? Check::Mine : inUse ? Check::Auto : Check::None, over);
                if (over) {
                    late.tip = translated(mine ? "recipes.use.mine" : inUse ? "recipes.use.auto" : "recipes.use");
                    if (inUse) late.tipLines = {translated(mine ? "recipes.use.back" : "recipes.use.pick")};
                }
                regions.push_back(std::move(region));
            }
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
