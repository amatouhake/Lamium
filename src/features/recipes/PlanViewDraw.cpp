#include "features/recipes/CountText.h"
#include "features/recipes/PlanViewDraw.h"
#include "app/Runtime.h"
#include "features/recipes/CountEdit.h"
#include "features/recipes/GroupLook.h"
#include "features/recipes/ListFilter.h"
#include "features/recipes/PlanRows.h"
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

// L-370 (docs/demos/plan-*.html): the Recipes window. "Wanted" is a table of the wanted items whose rows open
// read-only breakdowns; "to gather" and "to make" share its columns; "How to make" holds the choices for all
// plans and worlds (a group's member, an item's recipe); "Stores" what counts as held.
namespace lamium::recipes::planView {
namespace {
bool opened = false;
Tab tab = Tab::Wanted;
int scroll[5]{};
std::string openEntry;  // the wanted entry whose breakdown is open (wantedEntryId)
std::string scrollTo;   // a wanted entry to bring into view when the wanted tab shows next
std::string howOpen;    // the line of "How to make" whose candidates are open: an item's key or a group's
std::string howScroll;  // a line of "How to make" to bring into view
std::string howFlash;   // a line just asked for, lit for a moment
std::chrono::steady_clock::time_point howFlashAt{};
bool howAll = false;    // "How to make" also lists what was chosen before, beyond the current plan
constexpr float switchLine = 16, optionHeight = 24, rowHeight = 18, stepHeight = 20, labelHeight = 11, headHeight = 11, headTop = 4;
}
bool isOpen() { return opened; }
void open(Tab which) {
    opened = true;
    tab = which;
}
void openLast() { opened = true; }
void close() {
    opened = false;
    openEntry.clear();
    howOpen.clear();
}
void storedLines(view::Late& late, state::Plan const& plan, Item item) {
    using namespace lamium::ui;
    auto const& index = plan.table->index;
    auto const stored = plan.stored.find(item);
    if (item < 0 || stored == plan.stored.end() || !stored->second) return;
    auto const carried = plan.held.find(item);
    late.tipLines.push_back(translated("recipes.place.tipCarried", carried == plan.held.end() ? Count{0} : carried->second));
    for (auto const& place : state::places().list) {
        if (!place.counted) continue;
        Count here = 0;
        for (auto const& entry : place.items) if (index.find(entry.name, entry.aux) == item) here += entry.count;
        if (!here) continue;
        std::string name = schematic::items::shown(place.block).name;
        if (!place.ender) name = translated("recipes.place.at", name, place.at.x, place.at.y, place.at.z);
        late.tipLines.push_back(translated("recipes.place.tipPlace", name, here));
    }
}
bool stepButtons(MinecraftUIRenderContext& context, view::Late& late, view::Placer* placer, Index const& index, StepLine const& step, float x,
                 float y, float px, float py, std::function<bool(float, float, float, float)> const& took, bool ctrl, bool shift) {
    using namespace lamium::ui;
    constexpr float size = 11;
    auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
    bool const can = placer && placer->preview(recipe, false).ready();
    float const makeX = x + size + 2;
    auto over = [&](float bx) { return px >= bx && px < bx + size && py >= y && py < y + size; };
    bool const overPlace = over(x), overMake = over(makeX);
    drawPictureButton(context, x, y, size, Picture::Place, overPlace, false, can);
    drawPictureButton(context, makeX, y, size, Picture::Make, overMake, false, can);
    if (overPlace || overMake) {
        late.tip = translated(overPlace ? "recipes.place" : "recipes.makeSteps");
        late.tipLines.clear();
        // Why not: the station it is made at (a three-wide recipe at the inventory's 2x2: the crafting table), else
        // what is short.
        if (!can)
            late.tipLines.push_back(!placer || !placer->places(kindOf(recipe.station)) ? translated("recipes.makeAt", stationName(recipe.station))
                                    : placer->gridSide() && !fitsGrid(recipe, placer->gridSide()) ? translated("recipes.makeAt", stationName(Station::Crafting))
                                                                                                   : translated("recipes.notReady"));
        if (overPlace && can) late.hints.push_back(translated("recipes.hint.placeStep"));
    }
    int const rounds = static_cast<int>(std::min<Count>(step.crafts, 1 << 20));
    if (took(x, y, size, size) && can)
        return ctrl ? placer->craft(step.recipe) : shift ? placer->runMost(step.recipe, std::min(rounds, 64)) : placer->run(step.recipe, false);
    if (took(makeX, y, size, size) && can) placer->make({{step.recipe, rounds}});
    return false;
}
void gatherTip(view::Late& late, state::Plan const& plan, GatherLine const& line, std::string const& key) {
    using namespace lamium::ui;
    auto const& index = plan.table->index;
    Item const itemCount = static_cast<Item>(index.items.size());
    bool const group = key.empty() && line.id >= itemCount;
    late.tipLines.clear();
    late.tipItems.clear();
    late.tipCurrent = -1;
    std::string picture = key;
    if (group) {
        groupLook::tip(late, index, line.id - itemCount, plan.request.picks, &plan.held);
        picture = late.hovered;
    } else {
        if (picture.empty() && line.id >= 0) picture = index.items[static_cast<size_t>(line.id)];
        late.tip = schematic::items::shown(picture).name, late.hovered = picture;
    }
    // Held and needed first, then what is left; a group's own lines follow.
    std::vector<std::string> amounts{translated("recipes.col.held") + ": " + std::to_string(line.held) + "  " + translated("recipes.col.need") + ": "
                                     + std::to_string(line.need)};
    if (line.left > 0) amounts.push_back(translated("recipes.stepShort", amountText(line.left, schematic::items::shown(picture).stack)));
    late.tipLines.insert(late.tipLines.begin(), amounts.begin(), amounts.end());
    // What the counted stores held when last seen, place by place, beside what is carried.
    if (key.empty() && line.id >= 0 && line.id < itemCount) storedLines(late, plan, line.id);
}
void draw(MinecraftUIRenderContext& context, view::Input const& input, view::Late& late, LocalPlayer& player, bool shift, std::string& showItem,
          bool& showChoice) {
    using namespace lamium::ui;
    namespace v = palette::vanilla;
    if (!opened) return;
    if (input.close) { close(); return; }
    float const px = input.pointerX, py = input.pointerY;
    // One press a frame; each control asks whether it was on it.
    bool pressed = !input.clicks.empty() && input.clicks.front();
    auto over = [&](float x, float y, float w, float h) { return px >= x && px < x + w && py >= y && py < y + h; };
    auto took = [&](float x, float y, float w, float h) {
        if (!pressed || !over(x, y, w, h)) return false;
        pressed = false;
        return true;
    };
    auto const l = recipeViewAt(input.screenWidth, input.screenHeight);
    // In the recipe view's place and look: the game's window under it must not show.
    // The window's own span is wider than the recipe view's (planWindowAt); the grey covers that and the game's window.
    auto const span = planWindowAt(input.screenWidth, input.coverRight > input.coverLeft ? std::min(l.left, input.coverLeft) : l.left,
                                   input.coverRight > input.coverLeft ? std::max(l.left + l.width, input.coverRight) : l.left + l.width);
    float const coverLeft = input.coverRight > input.coverLeft ? std::min(span.left, input.coverLeft) : span.left;
    float const coverRight = input.coverRight > input.coverLeft ? std::max(span.left + span.width, input.coverRight) : span.left + span.width;
    windowPanel(context, coverLeft, l.top, coverRight - coverLeft, l.height);
    OnWindow const onWindowScope;
    // The lists are the game's tabs on the window's top edge; they name the window, which needs no title.
    // Each feature brings its own (L-373): the Plan its three lists, Recipes "How to make", the stores theirs.
    {
        Tab const all[]{Tab::Wanted, Tab::Gather, Tab::Make, Tab::HowTo, Tab::Places};
        char const* const allNames[]{"recipes.wanted", "recipes.gather", "recipes.make", "recipes.howTo", "recipes.places"};
        bool const on[]{input.plan, input.plan, input.plan, input.recipes, input.stores};
        std::vector<int> shownTabs;
        for (int i = 0; i < 5; ++i) if (on[i]) shownTabs.push_back(i);
        if (shownTabs.empty()) { close(); return; }
        if (std::find_if(shownTabs.begin(), shownTabs.end(), [&](int i) { return all[i] == tab; }) == shownTabs.end()) tab = all[shownTabs.front()];
        int const shown = static_cast<int>(shownTabs.size());
        // One width for all, as wide as the widest name needs and no wider than the window allows.
        float widest = 0;
        for (int i : shownTabs) widest = std::max(widest, textWidth(context, translated(allNames[i])));
        float const w = std::floor(std::min(widest + 12, (span.width - 10 + static_cast<float>(shown - 1)) / static_cast<float>(shown)));
        float x = span.left + 5;
        for (int i : shownTabs) {
            std::string name = translated(allNames[i]);
            windowTab(context, x, l.top, w, std::move(name), tab == all[i], over(x, l.top - windowTabRise, w - 1, windowTabRise));
            if (took(x, l.top - windowTabRise, w - 1, windowTabRise)) tab = all[i];
            x += w - 1; // neighbours share their edge
        }
    }
    auto const* plan = state::plan(player);
    // The rows end short of the scrollbar and the scrollbar short of the window's shadow, 3 units each (L-369).
    float const left = span.left + 6, right = span.left + span.width - 12;
    float top = l.top + headTop;
    float const listTop = top; // every tab's scrollbar spans the same height, from here to the bottom
    float const bottom = l.top + l.height - RecipeViewLayout::foot;
    int& offset = scroll[static_cast<int>(tab)];
    int wheel = input.wheel;
    // Every list scrolls with the shared bar in the margin right of the rows; `visible` is how many rows of their
    // real heights fit at the list's end.
    auto bar = [&](int count, int& first, int visible) {
        listScrollbar(context, right + 5, listTop, bottom - listTop, count, std::max(1, visible), first, px, py, pressed, input.held);
    };
    auto fitFromEnd = [&](std::vector<float> const& heights) {
        float room = bottom - top;
        int fit = 0;
        for (auto h = heights.rbegin(); h != heights.rend() && room >= *h; ++h) room -= *h, ++fit;
        return std::max(1, fit);
    };
    auto note = [&](std::string text) { windowLabel(context, left, top + 2, right - left, std::move(text), true); };
    // A place by what it is: the ender chest, or a store's block and where it stands.
    auto placeName = [&](Place const& place) {
        std::string name = schematic::items::shown(place.block).name;
        return place.ender ? name : translated("recipes.place.at", name, place.at.x, place.at.y, place.at.z);
    };
    // A small button; returns its left edge less a gap.
    auto rowButton = [&](float end, float y, std::string text, auto&& act) {
        // `y`: where a switch's top would be; the taller button keeps the same middle.
        float const w = rowButtonWidth(context, text);
        y -= (rowButtonHeight - switchHeight) / 2;
        drawRowButton(context, end - w, y, w, std::move(text), over(end - w, y, w, rowButtonHeight));
        if (took(end - w, y, w, rowButtonHeight)) act();
        return end - w - 3;
    };
    if (tab == Tab::Places) {
        // What counts as held: what is carried, then the ender chest and the stores as they were when last seen
        // open. Rows as the other tabs' (L-369): the place's icon and name, its switch under a heading, and a store
        // can be taken out as well.
        auto const all = state::places();
        auto const& places = all.list; // of a copy: the controls below change the list
        std::string const removeText = translated("recipes.remove");
        float const sx = right - rowButtonWidth(context, removeText) - 4 - switchWidth;
        labelScaled(context, sx - 10, top + 2, switchWidth + 20, translated("recipes.col.counted"), .75f, v::windowFaint, Align::Center, false);
        top += headHeight;
        int const rows = static_cast<int>(places.size()) + 1, fits = std::max(1, static_cast<int>((bottom - top) / rowHeight));
        offset = std::clamp(offset + wheel, 0, std::max(0, rows - fits));
        bar(rows, offset, fits);
        for (int row = 0; row < fits && offset + row < rows; ++row) {
            float const y = top + static_cast<float>(row) * rowHeight;
            int const at = offset + row - 1;
            std::string name, icon, tip;
            bool on = false;
            if (at < 0) name = translated("recipes.place.carried"), icon = "minecraft:player_head", on = all.carried;
            else {
                auto const& place = places[static_cast<size_t>(at)];
                name = placeName(place), icon = place.block, on = place.counted;
                if (!place.ender) rowButton(right, y + 4.5f, removeText, [&] { state::unmarkStore(at); });
            }
            toggleSwitch(context, sx, y + 4.5f, on);
            if (took(sx - 2, y, switchWidth + 4, rowHeight)) {
                if (at < 0) state::setCarriedCounted(!all.carried);
                else state::setPlaceCounted(at, !on);
            }
            if (auto const* stack = schematic::items::iconStack(schematic::items::shown(icon).icon)) late.icons.push_back({stack, left + 8, y + 1});
            if (late.tip.empty() && over(left, y, sx - left - 4, rowHeight)) {
                late.tip = name;
                if (!tip.empty()) late.tipLines.push_back(std::move(tip));
            }
            windowLabel(context, left + 26, y + 5, sx - left - 30, std::move(name));
        }
        if (places.empty()) paragraph(context, left, top + rowHeight + 4, right - left, translated("recipes.place.how"), 3, v::windowFaint);
        return;
    }
    if (!plan) { note(translated("schematic.raw.reading")); return; }
    auto const& index = plan->table->index;
    auto const& request = plan->request;
    Item const itemCount = static_cast<Item>(index.items.size());
    auto const notes = state::notes();

    // ---- Shared pieces ----
    // The table's columns, the same on every list: fold, icon, name, held, needed, a bar of held against that,
    // controls. Held, needed and the bar stand evenly apart (L-370, demos/plan-numbers.html A); the controls'
    // column holds a handed row's source whole.
    // What comes before the table (icon, name, a step's slots) takes the window's left 45% at most; the table the
    // rest, ending 8 units short of the scrollbar (L-370, 2026-11-22, demos/notes-table.html A), every control
    // always shown.
    float const numW = std::ceil(textWidth(context, "888888")) + 2;
    float const mid = std::floor(left + (right - left) * .45f);
    float const tableRight = right - 8;
    // The controls' column as wide as the widest control the tab shows.
    float const controlsW = tab == Tab::Wanted ? std::max(11.f, std::ceil(textWidthScaled(context, translated("recipes.from.schematic"), .75f)) + 2)
                          : tab == Tab::Gather ? rowButtonWidth(context, translated("recipes.toMake"))
                          : tab == Tab::Make ? 24.f : 26.f;
    float const controlsX = tableRight - controlsW;
    // Held and needed close together after the store mark's room; the bar from them to the controls.
    float const heldX = mid + 10, wantX = heldX + numW + 6, barX = wantX + numW + 8;
    float const barW = std::max(16.f, controlsX - 6 - barX);
    float const wantW = numW, heldW = numW;
    float const nameX = left + 26, nameEnd = mid - 4;
    auto columnHeads = [&](float y, char const* wantKey, bool withName = false, std::string name = {}, char const* heldKey = "recipes.col.held") {
        if (withName) windowLabel(context, nameX, y + 1, nameEnd - nameX, std::move(name), true);
        // A heading may reach left into the gap before its column ("今作れる" whole).
        labelScaled(context, heldX - 12, y + 2, heldW + 12, translated(heldKey), .75f, v::windowFaint, Align::Right, false);
        labelScaled(context, wantX - 6, y + 2, wantW + 6, translated(wantKey), .75f, v::windowFaint, Align::Right, false);
    };
    // A count cut to fit its column shows whole under the pointer.
    auto number = [&](float x, float w, float y, Count count, bool faint) {
        std::string text = std::to_string(count);
        if (late.tip.empty() && textWidth(context, text) > w && over(x, y - 2, w, 12)) late.tip = wholeCountText(static_cast<std::uint64_t>(count));
        windowLabel(context, x, y, w, std::move(text), faint, Align::Right);
    };
    // A group as its picture, holding still (GroupLook.h): the member chosen, else the one most held, else the one it
    // is known by, unless the wheel looks through them.
    auto groupFace = [&](int group) -> std::string {
        Item const face = groupLook::face(index, group, request.picks, &plan->held);
        return face >= 0 ? index.items[static_cast<size_t>(face)] : std::string();
    };
    auto pictureOf = [&](Item id) -> std::string {
        if (id >= itemCount) return groupFace(id - itemCount);
        return id >= 0 ? index.items[static_cast<size_t>(id)] : std::string();
    };
    auto nameOf = [&](Item id) -> std::string {
        if (id >= itemCount) return groupName(index, id - itemCount);
        return id >= 0 ? schematic::items::shown(index.items[static_cast<size_t>(id)]).name : std::string();
    };
    auto icon = [&](std::string const& key, float x, float y, float scale = 1.f, float alpha = 1.f) {
        if (key.empty()) return;
        if (auto const* stack = schematic::items::iconStack(schematic::items::shown(key).icon)) late.icons.push_back({stack, x, y, scale, 0, alpha});
    };
    // An item or a group as its picture; a group any member of which will do carries the mark.
    auto itemIcon = [&](Item id, float x, float y, float alpha = 1.f) {
        icon(pictureOf(id), x, y, 1.f, alpha);
        if (id >= itemCount) late.marks.push_back({x, y});
    };
    // Under the pointer a group names itself, says what it shows and shows its members. The wheel looks through
    // them only while the pointer rests on the group's picture, (x, y) its top left (L-369): over the rest of a row
    // the wheel scrolls.
    auto groupTip = [&](int group, float x, float y) {
        if (over(x, y, 18, 18)) groupLook::hover(group, x, y);
        groupLook::tip(late, index, group, request.picks, &plan->held);
    };
    // A recipe by what it is made from: its first input.
    auto recipeName = [&](int number) {
        auto const& recipe = index.recipes[static_cast<size_t>(number)];
        if (recipe.uses.empty()) return stationName(recipe.station);
        auto const& part = recipe.uses.front();
        return translated("recipes.howTo.from", part.group ? groupName(index, part.id) : nameOf(part.id));
    };
    // A choice's state, at the right of a row: automatic, faint; chosen, on green. A press goes to its line in
    // "How to make". Returns its left edge less a gap.
    auto jumpHowTo = [&](std::string const& key) {
        tab = Tab::HowTo;
        howOpen = howScroll = howFlash = key;
        howFlashAt = std::chrono::steady_clock::now();
    };
    auto stateTag = [&](float end, float y, std::string text, bool chosen, std::string const& key, float room = 64) {
        if (room < 14) return end;
        float const w = std::min({64.f, room, textWidth(context, text) * .75f + 6}), x = end - w;

        bool const hot = over(x, y, w, 10);
        if (hot) fill(context, x, y, w, 10, palette::white, .2f), late.hints.push_back(translated("recipes.howTo.change"));
        labelScaled(context, x + 3, y + 2.5f, w - 6, std::move(text), .75f, chosen ? v::windowInk : v::windowFaint, Align::Left, false);
        if (took(x, y, w, 10)) jumpHowTo(key);
        return x - 3;
    };
    auto recipeState = [&](float end, float y, Item item, float room = 64) {
        if (recipeOptions(index, item).size() < 2) return end;
        auto const& key = index.items[static_cast<size_t>(item)];
        int const now = plan->choice->recipe[static_cast<size_t>(item)];
        bool const chosen = notes.choices.contains(key) && now >= 0;
        return stateTag(end, y, chosen ? recipeName(now) : translated("recipes.howTo.auto"), chosen, key, room);
    };
    auto groupState = [&](float end, float y, int group, float room = 64) {
        auto picked = request.picks.find(group);
        bool const chosen = picked != request.picks.end();
        return stateTag(end, y, chosen ? nameOf(picked->second) : translated("recipes.howTo.any"), chosen, groupKey(index, group), room);
    };
    // A step as the game's slots: inputs right-aligned to the arrow (room for four), the product in a fixed
    // column, the station. `x0` is where the inputs' room starts.
    constexpr float inputsW = 72;
    // Whether the step's row shows what it makes in a column ("to make"): its product's tip then leaves it out.
    bool madeInColumn = false;
    // What "to make" counts as held: its steps' tips give each input as held/needed, as the panel's do.
    std::map<Item, Count> const* tipStock = nullptr;
    auto inputLines = [&](StepLine const& step) {
        return tipStock ? stepInputLines(index, request, step, *tipStock) : stepInputLines(index, request, step);
    };
    auto inputsRoom = [&](std::vector<StepLine> const& steps) {
        size_t most = 1;
        for (auto const& step : steps) most = std::max(most, index.recipes[static_cast<size_t>(step.recipe)].uses.size());
        return static_cast<float>(std::min<size_t>(most, 4)) * 18;
    };
    auto stepSlots = [&](float x0, float y, StepLine const& step, bool faint, bool interactive = true, float room = inputsW,
                         std::vector<bool> const* shortParts = nullptr, std::vector<bool> const* storedParts = nullptr) {
        auto const& key = index.items[static_cast<size_t>(step.item)];
        auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
        size_t const iconsBefore = late.icons.size();
        float const arrowX = x0 + room, productX = arrowX + 12;
        auto const& uses = recipe.uses;
        size_t const shown = std::min<size_t>(uses.size(), 4);
        for (size_t u = 0; u < shown; ++u) {
            auto const& part = uses[u];
            float const sx = arrowX - static_cast<float>(shown - u) * 18;
            Item const item = partItem(index, request, part);
            // A use held too few of for all the crafts, on the game's red (L-370).
            windowSlot(context, sx, y + 1, over(sx, y + 1, 18, 18), shortParts && u < shortParts->size() && (*shortParts)[u]);
            itemIcon(item, sx + 1, y + 2);
            Count const n = partNeed(recipe, u, step.crafts);
            if (n > 1) late.counts.push_back({sx, y + 1, slotCountText(n)});
            bool const stored = storedParts && u < storedParts->size() && (*storedParts)[u];
            if (over(sx, y + 1, 18, 18)) {
                if (part.group) {
                    groupTip(part.id, sx, y + 1);
                    if (interactive) late.hints.push_back(translated("recipes.howTo.change"));
                } else late.tip = nameOf(item), late.hovered = pictureOf(item);
                // The slot shows a count of a thousand and more cut ("44k"): the tip has it whole.
                if (n >= 1000) late.tipLines.insert(late.tipLines.begin(), translated("recipes.col.need") + ": " + wholeCountText(static_cast<std::uint64_t>(n)));
                if (stored && item < itemCount) storedLines(late, *plan, item);
            }
            if (interactive && part.group && took(sx, y + 1, 18, 18)) jumpHowTo(groupKey(index, part.id));
        }
        if (uses.size() > shown && over(x0, y, room, stepHeight))
            late.tip = schematic::items::shown(key).name, late.tipLines = inputLines(step);
        changeArrow(context, arrowX + 3, y + 7.5f, 1, v::windowInk);
        windowSlot(context, productX, y + 1, over(productX, y + 1, 18, 18));
        icon(key, productX + 1, y + 2);
        if (step.made > 1) late.counts.push_back({productX, y + 1, slotCountText(step.made)});
        if (interactive && over(productX, y + 1, 18, 18)) {
            late.tip = schematic::items::shown(key).name, late.tipLines = inputLines(step), late.hovered = key;
            if (step.made >= 1000 && !madeInColumn) late.tipLines.insert(late.tipLines.begin(), translated("recipes.col.need") + ": " + wholeCountText(static_cast<std::uint64_t>(step.made)));
        }
        if (interactive && took(productX, y + 1, 18, 18)) showItem = key;
        // Where it is made, at full size and well apart from the step (L-370, 2026-11-22), so it does not read as a
        // part of it.
        float const stationX = productX + 18 + 14;
        if (auto const station = stationItem(recipe.station); !station.empty()) {
            auto const& at = schematic::items::shown(std::string(station));
            icon(std::string(station), stationX, y + 2);
            if (over(stationX, y + 1, 16, 18)) late.tip = at.name;
        }
        if (faint) for (size_t k = iconsBefore; k < late.icons.size(); ++k) late.icons[k].alpha = .45f;
        return stationX + 20; // past the station's icon, and a gap
    };

    if (tab == Tab::Wanted) {
        auto const* shares = state::shares(player);
        if (!shares) { note(translated("schematic.raw.reading")); return; }
        auto const& entries = shares->entries;
        bool const schematicSwitch = Runtime::instance().preferences().schematic.enabled;
        // The schematic's switch, while Schematics is on: the list's first line, scrolling away with it (L-370).
        auto drawSwitch = [&](float y) {
            bool const on = Runtime::instance().preferences().schematic.planMaterials;
            if (over(left, y, right - left, switchLine)) fill(context, left - 2, y, right - left + 4, switchLine, palette::shade, .08f);
            // The switch right after its words (L-370, 2026-11-22), not at the window's far edge.
            std::string const words = translated("recipes.plan.schematic");
            float const wordsW = std::min(textWidth(context, words), right - left - switchWidth - 8);
            windowLabel(context, left, y + 4, wordsW, words);
            toggleSwitch(context, left + wordsW + 8, y + 3.5f, on);
            if (took(left, y, right - left, switchLine)) {
                auto value = Runtime::instance().preferences();
                value.schematic.planMaterials = !on;
                Runtime::instance().save(value);
            }
        };
        if (entries.empty()) {
            if (schematicSwitch) drawSwitch(top), top += switchLine;
            paragraph(context, left, top + 2, right - left, translated("recipes.wantedHow"), 3, v::windowInk);
            return;
        }
        // What of each is held: the finished items (what is gathered shows in "to gather"). Whether an item is all
        // held decides its place (under "done"), kept while the pointer is over the list (PlanRows.h).
        static std::map<std::string, bool> shownDone;
        auto const rows = wantedRows(entries, wantedHeld(entries, shares->targets, plan->loose), shownDone,
                                     over(left, listTop, right - left, bottom - listTop));
        std::vector<Count> heldOf(entries.size(), 0);
        std::vector<bool> doneOf(entries.size(), false);
        for (auto const& row : rows) heldOf[row.entry] = row.held, doneOf[row.entry] = row.done;
        // Lines of their own heights: the switch, a wanted row and under an open one its breakdown; the items
        // wanted that are all held follow under their heading, faint (L-370).
        struct Line { enum class Kind { Switch, DoneHead, Target, Label, Step, Material, End } kind; size_t entry = 0; size_t at = 0; float height = 0; };
        std::vector<Line> lines;
        if (schematicSwitch) lines.push_back({Line::Kind::Switch, 0, 0, switchLine});
        auto addEntry = [&](size_t e) {
            if (scrollTo == wantedEntryId(entries[e])) offset = static_cast<int>(lines.size()), scrollTo.clear();
            lines.push_back({Line::Kind::Target, e, 0, rowHeight});
            if (openEntry != wantedEntryId(entries[e]) || e >= shares->targets.size() || shares->targets[e].item < 0) return;
            auto const& view = shares->targets[e].view;
            if (!view.steps.empty()) lines.push_back({Line::Kind::Label, e, 0, labelHeight});
            for (size_t k = 0; k < view.steps.size(); ++k) lines.push_back({Line::Kind::Step, e, k, stepHeight});
            if (!view.gather.empty()) lines.push_back({Line::Kind::Label, e, 1, labelHeight});
            for (size_t k = 0; k < view.gather.size(); ++k) lines.push_back({Line::Kind::Material, e, k, rowHeight});
            lines.push_back({Line::Kind::End, e, 0, 3});
        };
        bool doneHead = false;
        for (auto const& row : rows) {
            if (row.done && !doneHead) lines.push_back({Line::Kind::DoneHead, 0, 0, labelHeight}), doneHead = true;
            addEntry(row.entry);
        }
        int const count = static_cast<int>(lines.size());
        offset = std::clamp(offset, 0, std::max(0, count - 1));
        // Only the column headings stay put: under the switch while it shows, at the top once it scrolled away.
        int from = offset;
        if (offset == 0 && schematicSwitch) drawSwitch(top), top += switchLine, from = 1;
        columnHeads(top, "recipes.col.need");
        top += headHeight;
        std::vector<float> heights;
        for (auto const& line : lines) heights.push_back(line.height);
        bar(count, offset, fitFromEnd(heights));
        float const faceX = left + 8, faceW = right - faceX, x0 = faceX + 4;
        // A breakdown's steps and materials one indent in from its labels, the products in one column after the room
        // the most inputs take (L-370, 2026-11-22).
        float const stepX0 = x0 + 14;
        float y = top;
        for (int at = from; at < count && y + lines[static_cast<size_t>(at)].height <= bottom + .5f; ++at) {
            auto const& line = lines[static_cast<size_t>(at)];
            if (line.kind == Line::Kind::Switch) { y += line.height; continue; }
            if (line.kind == Line::Kind::DoneHead) {
                labelScaled(context, left, y + 3, right - left, translated("recipes.wantedDone"), .75f, v::windowFaint, Align::Left, false);
                y += line.height;
                continue;
            }
            auto const& entry = entries[line.entry];
            std::string const id = wantedEntryId(entry);
            TargetPlan const* target = line.entry < shares->targets.size() ? &shares->targets[line.entry] : nullptr;
            // A breakdown's lines lie on one sunk face, drawn in slices.
            if (line.kind != Line::Kind::Target) {
                bool const first = at == 0 || lines[static_cast<size_t>(at - 1)].kind == Line::Kind::Target;
                sunkFace(context, faceX, y, faceW, line.height, first, line.kind == Line::Kind::End);
            }
            if (line.kind == Line::Kind::Label) {
                labelScaled(context, x0, y + 3, faceW - 8, translated(line.at == 0 ? "recipes.make" : "recipes.gather"), .75f, v::windowFaint, Align::Left, false);
            } else if (line.kind == Line::Kind::Step && target) {
                auto const& step = target->view.steps[line.at];
                float const after = stepSlots(stepX0, y, step, false, true, inputsRoom(target->view.steps));
                float const end = rowButton(tableRight, y + 6, translated("recipes.toGather"), [&] { state::setStop(index.items[static_cast<size_t>(step.item)], true); });
                // The recipe's state in the room left between the station and the button.
                recipeState(end, y + 5, step.item, end - after);
            } else if (line.kind == Line::Kind::Material && target) {
                auto const& material = target->view.gather[line.at];
                // One icon column with the steps: the material under the inputs' last slot, its name after.
                float const room = inputsRoom(target->view.steps);
                float const iconX = stepX0 + room - 17, labelX = stepX0 + room + 3;
                itemIcon(material.id, iconX, y + 1);
                // Held and needed in the table's columns, with its bar; "make" in the controls' column.
                number(heldX, heldW, y + 5, material.held, false);
                number(wantX, wantW, y + 5, material.need, false);
                progressBar(context, barX, y + 9, barW, 3, material.need > 0 ? static_cast<float>(material.held) / static_cast<float>(material.need) : 1.f);
                bool const stopped = material.id >= 0 && material.id < itemCount && request.stops.contains(material.id);
                if (stopped) rowButton(tableRight, y + 5.5f, translated("recipes.toMake"), [&] { state::setStop(index.items[static_cast<size_t>(material.id)], false); });
                float end = heldX - 3;
                if (material.id >= itemCount) end = groupState(end, y + 4, material.id - itemCount);
                windowLabel(context, labelX, y + 5, end - labelX, nameOf(material.id));
                if (late.tip.empty() && over(iconX, y, barX + barW - iconX, rowHeight)) {
                    if (material.id >= itemCount) groupTip(material.id - itemCount, iconX - 1, y);
                    else late.tip = nameOf(material.id), late.hovered = pictureOf(material.id);
                }
            } else if (line.kind == Line::Kind::Target) {
                bool const manual = entry.source.empty();
                bool const hot = over(left, y, right - left, rowHeight);
                if (manual && hot) late.wantedRow = entry.key;
                bool const known = target && target->item >= 0;
                Count const have = heldOf[line.entry];
                float const fraction = entry.count > 0 ? std::min(1.f, static_cast<float>(have) / static_cast<float>(entry.count)) : 1.f;
                bool const done = doneOf[line.entry];
                if (known) chevron(context, left, y + 6, openEntry == id, v::windowInk);
                auto const& shown = schematic::items::shown(entry.key);
                icon(entry.key, left + 8, y + 1, 1.f, done ? .45f : 1.f);
                windowLabel(context, nameX, y + 5, nameEnd - nameX, shown.name, done);
                number(heldX, heldW, y + 5, have, done);
                // The player's own count is edited where it stands (L-370, 2026-11-22): a faint sunk box behind it, a
                // press types it, the wheel steps it, "- +" only while it is typed; "x" always in the controls' column.
                // Every row's count keeps the needed column.
                bool onControl = false;
                auto const* editing = countEdit::editing(entry.key, countEdit::Where::Window);
                float const boxX = wantX - 2, boxW = wantW + 5;
                if (manual && editing) {
                    // Typed as the search field is (L-370, 2026-11-22): all selected at first, the caret where a
                    // press puts it; Enter keeps it, Esc does not.
                    sunkFace(context, boxX, y + 3, boxW, 12, true, true);
                    frame(context, boxX, y + 3, boxW, 12, palette::accentDeep);
                    drawEditField(context, wantX, y + 3, 12, wantW + 3, *editing, Align::Right, v::windowInk, false);
                    onControl = over(boxX, y + 3, boxW, 12);
                    if (took(boxX, y + 3, boxW, 12)) placeCaret(context, countEdit::typed, px, py);
                } else {
                    if (manual) {
                        sunkFace(context, boxX, y + 3, boxW, 12, true, true);
                        if (over(boxX, y + 3, boxW, 12)) fill(context, boxX + 1, y + 4, boxW - 2, 10, palette::white, .25f), onControl = true;
                        if (took(boxX, y + 3, boxW, 12)) countEdit::begin(entry.key, entry.count, countEdit::Where::Window);
                        else if (wheel && over(boxX, y + 3, boxW, 12)) state::stepWanted(entry.key, wheel < 0, shift, false), wheel = 0;
                    }
                    // In its box the count sits as a field's text does, so typing does not move it.
                    number(wantX, wantW, manual ? y + 4 + boxTextInset() : y + 5, entry.count, done || !manual || entry.count == 0);
                }
                progressBar(context, barX, y + 9, barW, 3, fraction);
                if (manual) {
                    float const xW = 11, xX = controlsX + controlsW - xW;
                    drawButton(context, xX, y + 4, xW, 10, "x", over(xX, y + 4, xW, 10));
                    if (over(xX, y + 4, xW, 10)) onControl = true;
                    if (took(xX, y + 4, xW, 10)) state::removeWanted(entry.key);
                } else labelScaled(context, controlsX, y + 6, controlsW, translated("recipes.from." + entry.source), .75f, v::windowFaint, Align::Right, false);
                if (late.tip.empty() && hot && !onControl) {
                    late.tip = shown.name, late.hovered = entry.key;
                    if (known && target->sharedAbove()) late.tipLines.push_back(translated("recipes.sharedAbove"));
                }
                if (known && !onControl && took(left, y, controlsX - left, rowHeight)) openEntry = openEntry == id ? std::string() : id;
            }
            y += line.height;
        }
        offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
        return;
    }

    if (tab == Tab::Gather) {
        // The shopping list for all, on the wanted table's columns: the whole first, then what is still to gather,
        // most left first, then what is done, faint.
        using Row = GatherRow;
        auto const lists = gatherRows(index, plan->view, plan->loose, request.stops);
        auto const& rows = lists.left;
        auto const& finished = lists.finished;
        if (rows.empty() && finished.empty()) { note(translated("schematic.raw.nothing")); return; }
        std::vector<GatherLine> all;
        for (auto const& row : rows) all.push_back(row.line);
        for (auto const& row : finished) all.push_back(row.line);
        auto const total = gatherTotal(all);
        columnHeads(top, "recipes.col.need");
        top += headHeight;
        // The whole as the table's first row, scrolling with the rest (only the headings stay put, L-370); then,
        // under two headings as "to make", what is still to gather, most left first, then what is gathered, faint,
        // so what is done shows too.
        struct Entry { Row const* row = nullptr; char const* label = nullptr; float height = 0; };
        std::vector<Entry> entries;
        entries.push_back({nullptr, nullptr, rowHeight + 2});
        if (!rows.empty()) entries.push_back({nullptr, "recipes.gatherLeft", labelHeight});
        for (auto const& row : rows) entries.push_back({&row, nullptr, rowHeight});
        if (!finished.empty()) entries.push_back({nullptr, "recipes.gathered", labelHeight});
        for (auto const& row : finished) entries.push_back({&row, nullptr, rowHeight});
        int const count = static_cast<int>(entries.size());
        std::vector<float> heights;
        for (auto const& entry : entries) heights.push_back(entry.height);
        offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
        bar(count, offset, fitFromEnd(heights));
        float rowY = top;
        for (int at = offset; at < count && rowY + entries[static_cast<size_t>(at)].height <= bottom + .5f; ++at) {
            auto const& entry = entries[static_cast<size_t>(at)];
            float const y = rowY;
            rowY += entry.height;
            if (entry.label) {
                labelScaled(context, left, y + 3, right - left, translated(entry.label), .75f, v::windowFaint, Align::Left, false);
                continue;
            }
            if (!entry.row) {
                // The whole as a row of the table (L-370, 2026-11-22): no icon, a rule under it.
                windowLabel(context, nameX, y + 5, nameEnd - nameX, translated("recipes.all"));
                number(heldX, heldW, y + 5, total.need - total.left, false);
                number(wantX, wantW, y + 5, total.need, false);
                progressBar(context, barX, y + 9, barW, 3, total.fraction());
                fill(context, left, y + rowHeight, tableRight - left, 1, v::windowFaint, .5f);
                continue;
            }
            auto const& row = *entry.row;
            auto const& line = row.line;
            float const alpha = row.done ? .45f : 1.f;
            std::string const picture = row.key.empty() ? pictureOf(line.id) : row.key;
            std::string const name = row.key.empty() ? nameOf(line.id) : schematic::items::shown(row.key).name;
            icon(picture, left + 8, y + 1, 1.f, alpha);
            if (row.key.empty() && line.id >= itemCount) late.marks.push_back({left + 8, y + 1});
            number(heldX, heldW, y + 5, line.held, row.done);
            number(wantX, wantW, y + 5, line.need, row.done);
            progressBar(context, barX, y + 9, barW, 3, line.need > 0 ? static_cast<float>(line.held) / static_cast<float>(line.need) : 1.f);
            float end = nameEnd;
            if (row.stopped) rowButton(tableRight, y + 5.5f, translated("recipes.toMake"), [&] { state::setStop(index.items[static_cast<size_t>(line.id)], false); });
            // A group's choice: no tag (the row has no room for one); the row itself goes to "How to make".
            bool const group = row.key.empty() && line.id >= itemCount;
            if (group && !row.stopped && took(left, y, right - left, rowHeight)) jumpHowTo(groupKey(index, line.id - itemCount));
            // A count that includes what a place held when last seen is starred; the tip says where the items are.
            if (line.id >= 0 && line.id < itemCount)
                if (auto const stored = plan->stored.find(line.id); stored != plan->stored.end() && stored->second) {
                    windowLabel(context, heldX - 5, y + 5, 5, "*", true);
                    end -= 6;
                }
            windowLabel(context, nameX, y + 5, end - nameX, name, row.done);
            if (late.tip.empty() && over(left, y, barX + barW - left, rowHeight)) {
                if (group && over(left + 7, y, 18, 18)) groupLook::hover(line.id - itemCount, left + 7, y);
                gatherTip(late, *plan, line, row.key);
                if (group) late.hints.push_back(translated("recipes.howTo.change"));
            }
        }
        return;
    }

    if (tab == Tab::Make) {
        // The work (L-370): steps that can be made now from what is carried, then those made in part, then those the
        // stores would cover (L-385), then those short of something, each under its heading; at the end, faint, what
        // held stock covers whole. A press opens the breakdown of the wanted item that needs the step; under the
        // pointer a row lays its step in or makes it, and the first heading makes them all.
        auto const& steps = plan->view.steps;
        auto const held = heldSteps(index, *plan->choice, plan->view);
        if (steps.empty() && held.empty()) { note(translated("recipes.nothingWanted")); return; }
        std::map<Item, Count> all, carried;
        for (auto const& [id, have] : request.held) all[id] += have;
        for (auto const& [id, have] : plan->held) carried[id] += have;
        // The rows as the panel's (PlanRows.h), each state under its heading.
        auto const made = makeRows(index, request, plan->view, held, carried, all);
        struct Line { MakeRow const* row = nullptr; StepState state = StepState::Ready; char const* label = nullptr; float height = 0; };
        std::vector<Line> lines;
        for (auto const& row : made) {
            if (lines.empty() || lines.back().state != row.state) {
                char const* const head = row.state == StepState::Ready ? "recipes.ready" : row.state == StepState::Partly ? "recipes.partly"
                                       : row.state == StepState::Stored ? "recipes.stored" : row.state == StepState::Missing ? "recipes.notReady"
                                                                                                                              : "recipes.heldSteps";
                lines.push_back({nullptr, row.state, head, labelHeight});
            }
            lines.push_back({&row, row.state, nullptr, stepHeight});
        }
        // What the open screen can do with a step: one round laid in from what is carried.
        auto* placer = input.placer;
        auto canLay = [&](StepLine const& step) {
            return placer && placer->preview(index.recipes[static_cast<size_t>(step.recipe)], false).ready();
        };
        // The columns of "to gather" (L-370): what can be made now and what is needed, in items, and a bar.
        columnHeads(top, "recipes.col.need", false, {}, "recipes.col.now");
        top += headHeight;
        int const count = static_cast<int>(lines.size());
        std::vector<float> heights;
        for (auto const& line : lines) heights.push_back(line.height);
        offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
        bar(count, offset, fitFromEnd(heights));
        float y = top;
        for (int at = offset; at < count && y + lines[static_cast<size_t>(at)].height <= bottom + .5f; ++at) {
            auto const& line = lines[static_cast<size_t>(at)];
            if (line.label) {
                labelScaled(context, left, y + 3, right - left, translated(line.label), .75f, v::windowFaint, Align::Left, false);
                if (line.state == StepState::Ready) {
                    // "Make all": what can be made now here, top down; a grid holds one recipe at a time, so it only makes.
                    std::vector<std::pair<int, int>> ready;
                    for (auto const& other : made)
                        if (other.state == StepState::Ready && canLay(other.step))
                            ready.emplace_back(other.step.recipe, static_cast<int>(std::min<Count>(other.step.crafts, 1 << 20)));
                    bool const can = !ready.empty() && placer && placer->gridSide() > 0;
                    std::string const text = translated("recipes.makeAll");
                    float const w = rowButtonWidth(context, text), bx = tableRight - w, by = y;
                    bool const hot = over(bx, by, w, rowButtonHeight);
                    if (can) drawRowButton(context, bx, by, w, text, hot);
                    else labelScaled(context, bx, by + 2, w, text, .75f, v::windowFaint, Align::Center, false);
                    if (hot) late.tip = translated("recipes.makeAll"), late.tipLines = {translated("recipes.makeAllTip")};
                    if (can && took(bx, by, w, rowButtonHeight)) placer->make(ready);
                }
            } else {
                auto const& step = line.row->step;
                bool const faint = line.state == StepState::Missing || line.state == StepState::Held;
                bool const hot = over(left, y, right - left, stepHeight);
                if (hot) fill(context, left - 2, y, right - left + 4, stepHeight, palette::shade, .08f);
                std::vector<bool> shortParts, storedParts;
                if (line.state != StepState::Held) {
                    // Red where nobody holds enough; the stores' chest where only they make it up.
                    shortParts = stepShort(index, request, step, all);
                    storedParts = stepShort(index, request, step, carried);
                    for (size_t k = 0; k < storedParts.size(); ++k) storedParts[k] = storedParts[k] && !shortParts[k];
                }
                madeInColumn = line.state != StepState::Held;
                tipStock = line.state != StepState::Held ? &carried : nullptr;
                stepSlots(left, y, step, faint, true, inputsW, &shortParts, &storedParts);
                madeInColumn = false, tipStock = nullptr;
                if (line.state == StepState::Held) {
                    // Held: a check where the counts would be.
                    drawCheckMark(context, wantX + wantW - 8, y + 6, v::windowFaint);
                } else {
                    Count const now = line.row->now;
                    number(heldX, heldW, y + 6, now, faint);
                    number(wantX, wantW, y + 6, step.made, faint);
                    progressBar(context, barX, y + 10, barW, 3, step.made > 0 ? static_cast<float>(now) / static_cast<float>(step.made) : 1.f);
                    // Laid in, the window steps aside as the recipe view does: the grid it covers shows the result.
                    if (stepButtons(context, late, placer, index, step, tableRight - 24, y + 4.5f, px, py, took, input.ctrl, input.shift)) late.placed = true;
                }
                // No name on a step (L-370, 2026-11-22): the product's slot names it under the pointer. No store mark
                // either: the heading says the stores make it up, the slots' tips which.
                if (line.state != StepState::Held && took(left, y, right - left, stepHeight))
                    if (auto const* shares = state::shares(player))
                        for (size_t e = 0; e < shares->targets.size() && e < shares->entries.size(); ++e)
                            if (std::ranges::any_of(shares->targets[e].view.steps, [&](StepLine const& mine) { return mine.item == step.item; })) {
                                openEntry = scrollTo = wantedEntryId(shares->entries[e]);
                                tab = Tab::Wanted;
                                break;
                            }
            }
            y += line.height;
        }
        if (!showItem.empty()) showChoice = false;
        return;
    }

    // ---- How to make ----
    // The choices for all plans and worlds: a group's member, an item's recipe. A line: the item, an arrow, the
    // chosen (or the group, or the rule's recipe) in a slot, its words by it; a press opens the candidates.
    bool const showAll = howAll || !input.plan;
    // Which groups and items have a line: those in the current plan, and with the switch on all chosen before.
    std::vector<int> groups;
    std::vector<Item> items;
    auto addGroup = [&](int group) { if (std::ranges::find(groups, group) == groups.end()) groups.push_back(group); };
    auto addItem = [&](Item item) { if (item >= 0 && std::ranges::find(items, item) == items.end()) items.push_back(item); };
    // An item has a line where there is a choice: several recipes, or one made that could be gathered instead
    // ("gather" is an option beside the recipes, L-370), which the plan gathers now.
    auto choosable = [&](Item item) {
        return item >= 0 && item < itemCount
            && (recipeOptions(index, item).size() > 1 || (request.stops.contains(item) && !index.makers[static_cast<size_t>(item)].empty()));
    };
    if (input.plan) {
        for (auto const& line : plan->view.gather) {
            if (line.id >= itemCount) addGroup(line.id - itemCount);
            else if (request.stops.contains(line.id) && choosable(line.id)) addItem(line.id);
        }
        for (auto const& line : plan->view.done) if (line.id < itemCount && request.stops.contains(line.id) && choosable(line.id)) addItem(line.id);
        for (auto const& step : plan->view.steps) {
            for (auto const& part : index.recipes[static_cast<size_t>(step.recipe)].uses) if (part.group) addGroup(part.id);
            if (recipeOptions(index, step.item).size() > 1) addItem(step.item);
        }
    }
    if (showAll) {
        // Every group the player chose a member for is among the request's picks, plan or not.
        for (auto const& [group, member] : request.picks) addGroup(group);
        for (auto const& [key, recipe] : notes.choices) if (Item const item = index.findKey(key); item >= 0 && recipeOptions(index, item).size() > 1) addItem(item);
        // The player's own "gather"; the items usually gathered show only where the plan has them.
        for (auto const& key : notes.stops) if (Item const item = index.findKey(key); choosable(item)) addItem(item);
    }
    // The line asked for (from a breakdown or the recipe view) always has a place, in the plan or not.
    if (!howOpen.empty()) {
        if (Item const item = index.findKey(howOpen); choosable(item)) addItem(item);
        else
            for (size_t group = 0; group < index.groups.size(); ++group)
                if (groupKey(index, static_cast<int>(group)) == howOpen) { addGroup(static_cast<int>(group)); break; }
    }
    if (groups.empty() && items.empty()) { howScroll.clear(); note(translated(input.plan ? "recipes.nothingWanted" : "recipes.howTo.none")); return; }
    // By name: a choice changes the plan, and its lines must not move under the pointer.
    std::ranges::stable_sort(groups, {}, [&](int group) { return groupName(index, group); });
    std::ranges::stable_sort(items, {}, [&](Item item) { return nameOf(item); });
    struct Line { enum class Kind { Switch, Label, Group, Members, Recipe, Option } kind; int group = -1; Item item = -1; int option = -1; float height = 0; };
    std::vector<Line> lines;
    // Which lines show: the first line, scrolling with them (only column headings stay put, L-370).
    if (input.plan) lines.push_back({Line::Kind::Switch, -1, -1, -1, switchLine});
    constexpr float pitch = 20; // a candidate's slot and the unit around it the chosen one's frame takes
    size_t const across = static_cast<size_t>(std::max(1.f, (right - left - 24) / pitch));
    if (!groups.empty()) lines.push_back({Line::Kind::Label, -1, -1, 0, labelHeight});
    for (int group : groups) {
        std::string const key = groupKey(index, group);
        if (howScroll == key) offset = static_cast<int>(lines.size()), howScroll.clear();
        lines.push_back({Line::Kind::Group, group, -1, -1, stepHeight});
        if (howOpen == key) {
            size_t const cells = index.groups[static_cast<size_t>(group)].members.size() + 1;
            lines.push_back({Line::Kind::Members, group, -1, -1, static_cast<float>((cells + across - 1) / across) * pitch + 6});
        }
    }
    if (!items.empty()) lines.push_back({Line::Kind::Label, -1, -1, 1, labelHeight});
    for (Item item : items) {
        std::string const& key = index.items[static_cast<size_t>(item)];
        if (howScroll == key) offset = static_cast<int>(lines.size()), howScroll.clear();
        lines.push_back({Line::Kind::Recipe, -1, item, -1, stepHeight});
        if (howOpen == key) {
            auto const options = recipeOptions(index, item);
            lines.push_back({Line::Kind::Option, -1, item, -1, optionHeight});
            // "Gather" second, after "automatic" (its option number is one past the recipes'), then the recipes.
            lines.push_back({Line::Kind::Option, -1, item, static_cast<int>(options.size()), optionHeight});
            for (size_t o = 0; o < options.size(); ++o) lines.push_back({Line::Kind::Option, -1, item, static_cast<int>(o), optionHeight});
        }
    }
    howScroll.clear(); // found above or not there: either way it is done
    int const count = static_cast<int>(lines.size());
    std::vector<float> heights;
    for (auto const& line : lines) heights.push_back(line.height);
    offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
    bar(count, offset, fitFromEnd(heights));
    bool const flashing = std::chrono::steady_clock::now() - howFlashAt < std::chrono::milliseconds(900);
    // The line's parts: item icon and name, an arrow, a slot, the words beside it (a second, small line under them).
    // In the other tabs' halves (L-370, 2026-11-22): the item as their rows have it on the left, the arrow where the
    // table starts; a candidate step's arrow under the line's.
    float const arrowX = mid, slotX = arrowX + 12, wordsX = slotX + 22;
    float const optionX = std::max(left + 18, arrowX - inputsW);
    // A line's state on one line (L-370): "Automatic: from planks" or the player's "Chosen: ...", both in the
    // window's ink; the words say which (L-369).
    auto words = [&](float y, std::string text, bool) { windowLabel(context, wordsX, y + 6, right - wordsX, std::move(text)); };
    float y = top;
    for (int at = offset; at < count && y + lines[static_cast<size_t>(at)].height <= bottom + .5f; ++at) {
        auto const& line = lines[static_cast<size_t>(at)];
        if (line.kind == Line::Kind::Switch) {
            if (over(left, y, right - left, switchLine)) fill(context, left - 2, y, right - left + 4, switchLine, palette::shade, .08f);
            std::string const words = translated("recipes.howTo.showAll");
            float const wordsW = std::min(textWidth(context, words), right - left - switchWidth - 8);
            windowLabel(context, left, y + 4, wordsW, words);
            toggleSwitch(context, left + wordsW + 8, y + 3.5f, howAll);
            if (took(left, y, right - left, switchLine)) howAll = !howAll;
        } else if (line.kind == Line::Kind::Label) {
            labelScaled(context, left, y + 3, right - left, translated(line.option == 0 ? "recipes.howTo.groups" : "recipes.howTo.recipes"), .75f, v::windowFaint,
                        Align::Left, false);
        } else if (line.kind == Line::Kind::Group) {
            std::string const key = groupKey(index, line.group);
            auto picked = request.picks.find(line.group);
            bool const chosen = picked != request.picks.end();
            if (over(left, y, right - left, stepHeight)) fill(context, left - 2, y, right - left + 4, stepHeight, palette::shade, .08f);
            if (flashing && howFlash == key) frame(context, left - 2, y, right - left + 4, stepHeight, palette::accentDeep);
            icon(groupFace(line.group), left + 8, y + 2);
            late.marks.push_back({left + 8, y + 2});
            std::string const name = groupName(index, line.group);
            windowLabel(context, nameX, y + 6, arrowX - nameX - 4, name);
            changeArrow(context, arrowX + 3, y + 7.5f, 1, v::windowInk);
            windowSlot(context, slotX, y + 1);
            icon(chosen ? index.items[static_cast<size_t>(picked->second)] : groupFace(line.group), slotX + 1, y + 2);
            late.marks.push_back({slotX + 1, y + 2});
            if (chosen) words(y, translated("recipes.howTo.chosenIs", nameOf(picked->second)), true);
            else words(y, translated("recipes.howTo.anyLine"), false);
            // Left the group; right what is in use: the member chosen as an item, or "any" as the group.
            if (late.tip.empty() && over(left, y, arrowX - left, stepHeight)) groupTip(line.group, left + 7, y + 1);
            else if (late.tip.empty() && over(arrowX, y, right - arrowX, stepHeight)) {
                if (chosen) late.tip = nameOf(picked->second), late.hovered = index.items[static_cast<size_t>(picked->second)];
                else groupTip(line.group, slotX, y + 1);
            }
            if (took(left, y, right - left, stepHeight)) howOpen = howOpen == key ? std::string() : key;
        } else if (line.kind == Line::Kind::Members) {
            // The candidates as the game's slots: "any" first (the group's picture, marked), then each member.
            std::string const key = groupKey(index, line.group);
            auto picked = request.picks.find(line.group);
            sunkFace(context, left + 18, y, right - left - 18, line.height);
            auto const& members = index.groups[static_cast<size_t>(line.group)].members;
            for (size_t c = 0; c <= members.size(); ++c) {
                float const cx = left + 22 + static_cast<float>(c % across) * pitch, cy = y + 4 + static_cast<float>(c / across) * pitch;
                bool const hot = over(cx, cy, 18, 18);
                windowSlot(context, cx, cy, hot);
                if (c == 0) {
                    icon(groupFace(line.group), cx + 1, cy + 1);
                    late.marks.push_back({cx + 1, cy + 1});
                    if (picked == request.picks.end()) frame(context, cx - 1, cy - 1, 20, 20, palette::accentDeep);
                    if (hot) late.tip = translated("recipes.howTo.any"), late.tipLines = {translated("recipes.howTo.anyTip")};
                    if (took(cx, cy, 18, 18)) state::setPick(key, {}), howOpen.clear();
                } else {
                    Item const member = members[c - 1];
                    auto const& memberKey = index.items[static_cast<size_t>(member)];
                    icon(memberKey, cx + 1, cy + 1);
                    if (picked != request.picks.end() && picked->second == member) frame(context, cx - 1, cy - 1, 20, 20, palette::accentDeep);
                    // No counts held in "How to make" (maintainer 2026-11-21).
                    if (hot) late.tip = nameOf(member), late.hovered = memberKey;
                    if (took(cx, cy, 18, 18)) state::setPick(key, memberKey), howOpen.clear();
                }
            }
        } else if (line.kind == Line::Kind::Recipe) {
            std::string const& key = index.items[static_cast<size_t>(line.item)];
            int const now = plan->choice->recipe[static_cast<size_t>(line.item)], rule = plan->table->choice.recipe[static_cast<size_t>(line.item)];
            bool const gathered = request.stops.contains(line.item);
            bool const chosen = notes.choices.contains(key) && now >= 0;
            if (over(left, y, right - left, stepHeight)) fill(context, left - 2, y, right - left + 4, stepHeight, palette::shade, .08f);
            if (flashing && howFlash == key) frame(context, left - 2, y, right - left + 4, stepHeight, palette::accentDeep);
            icon(key, left + 8, y + 2);
            windowLabel(context, nameX, y + 6, arrowX - nameX - 4, nameOf(line.item));
            // Made from: the recipe in use shown by its first input; gathered, the item itself.
            windowSlot(context, slotX, y + 1);
            if (gathered) icon(key, slotX + 1, y + 2);
            else if (now >= 0)
                if (auto const& recipe = index.recipes[static_cast<size_t>(now)]; !recipe.uses.empty()) icon(pictureOf(partItem(index, request, recipe.uses.front())), slotX + 1, y + 2);
            changeArrow(context, arrowX + 3, y + 7.5f, 1, v::windowInk);
            // Gathered reads as chosen: the player's, or the preset of an item usually gathered.
            if (gathered) words(y, translated("recipes.howTo.chosenIs", translated("recipes.toGather")), true);
            else if (chosen) words(y, translated("recipes.howTo.chosenIs", recipeName(now)), true);
            else words(y, translated("recipes.howTo.autoIs", rule >= 0 ? recipeName(rule) : std::string()), false);
            // Left the item; right what it is made from now (gathered: the item itself).
            if (late.tip.empty() && over(left, y, arrowX - left, stepHeight)) late.tip = nameOf(line.item), late.hovered = key;
            else if (late.tip.empty() && over(arrowX, y, right - arrowX, stepHeight)) {
                Item from = gathered ? line.item : -1;
                if (!gathered && now >= 0)
                    if (auto const& recipe = index.recipes[static_cast<size_t>(now)]; !recipe.uses.empty()) from = partItem(index, request, recipe.uses.front());
                if (from >= itemCount) groupTip(from - itemCount, slotX, y + 1);
                else if (from >= 0) late.tip = nameOf(from), late.hovered = index.items[static_cast<size_t>(from)];
            }
            if (took(left, y, right - left, stepHeight)) howOpen = howOpen == key ? std::string() : key;
        } else if (line.kind == Line::Kind::Option) {
            // The candidates as rows of slots on a sunk face: "automatic" first, then each recipe.
            std::string const& key = index.items[static_cast<size_t>(line.item)];
            bool const first = at == 0 || lines[static_cast<size_t>(at - 1)].kind != Line::Kind::Option;
            bool const last = at + 1 >= count || lines[static_cast<size_t>(at + 1)].kind != Line::Kind::Option;
            float const h = optionHeight, sy = y + 2; // the slots' row inside it
            sunkFace(context, left + 18, y, right - left - 18, h, first, last);
            bool const gathered = request.stops.contains(line.item);
            bool const chosen = notes.choices.contains(key);
            int const now = plan->choice->recipe[static_cast<size_t>(line.item)], rule = plan->table->choice.recipe[static_cast<size_t>(line.item)];
            bool const hot = over(left + 18, y, right - left - 18, h);
            if (hot) fill(context, left + 19, y, right - left - 20, h, palette::white, .2f);
            auto const options = recipeOptions(index, line.item);
            // Making it again: the plan no longer gathers it.
            auto make = [&] { if (gathered) state::setStop(key, false); };
            if (line.option == static_cast<int>(options.size())) {
                // "Gather": the item itself in the product's slot.
                float const productX = slotX;
                windowSlot(context, productX, sy + 1, false);
                icon(key, productX + 1, sy + 2);
                if (gathered) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                windowLabel(context, productX + 22, sy + 6, right - productX - 22, translated("recipes.toGather"));
                if (took(left + 18, y, right - left - 18, h)) {
                    if (!gathered) state::setStop(key, true);
                    howOpen.clear();
                }
            } else if (line.option < 0) {
                // "Automatic" as the other options: the rule's recipe as slots, faint (L-370).
                float after = slotX + 20;
                if (rule >= 0) {
                    StepLine const ruled{line.item, rule, 1, index.recipes[static_cast<size_t>(rule)].count, 0, 0};
                    after = stepSlots(optionX, sy, ruled, true, false);
                }
                if (!chosen && !gathered) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                windowLabel(context, after + 2, sy + 6, right - after - 2, translated("recipes.howTo.autoNow", rule >= 0 ? recipeName(rule) : std::string()));
                if (took(left + 18, y, right - left - 18, h)) state::setChoice(key, {}), make(), howOpen.clear();
            } else {
                int const number = options[static_cast<size_t>(line.option)];
                StepLine const one{line.item, number, 1, index.recipes[static_cast<size_t>(number)].count, 0, 0};
                float const after = stepSlots(optionX, sy, one, false, false);
                if (chosen && now == number && !gathered) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                windowLabel(context, after + 2, sy + 6, right - after - 2, recipeName(number));
                if (took(left + 18, y, right - left - 18, h)) state::setChoice(key, choiceKey(index.recipes[static_cast<size_t>(number)])), make(), howOpen.clear();
            }
        }
        y += line.height;
    }
}
}
