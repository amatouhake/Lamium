#include "features/recipes/CountText.h"
#include "features/recipes/PlanViewDraw.h"
#include "app/Runtime.h"
#include "features/recipes/CountEdit.h"
#include "features/recipes/GroupLook.h"
#include "features/recipes/ItemList.h"
#include "features/recipes/HowTo.h"
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
std::set<std::string> howRedye; // items whose re-dyeing recipes are open in "How to make" (L-389)
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
std::string planPicture(state::Plan const& plan, Item id) {
    auto const& index = plan.table->index;
    Item const items = static_cast<Item>(index.items.size());
    if (id >= items) {
        Item const face = groupLook::face(index, id - items, plan.request.picks, &plan.held);
        return face >= 0 ? index.items[static_cast<size_t>(face)] : std::string();
    }
    return id >= 0 ? index.items[static_cast<size_t>(id)] : std::string();
}
bool stepButtons(MinecraftUIRenderContext& context, view::Late& late, view::Placer* placer, state::Plan const& plan, StepLine const& step, float x,
                 float y, float px, float py, std::function<bool(float, float, float, float)> const& took, bool ctrl, bool shift) {
    using namespace lamium::ui;
    auto const& index = plan.table->index;
    constexpr float size = 11;
    int const lay = layRecipe(index, plan.request, step, plan.held);
    auto const& recipe = index.recipes[static_cast<size_t>(lay)];
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
        return ctrl ? placer->craft(lay) : shift ? placer->runMost(lay, std::min(rounds, 64)) : placer->run(lay, false);
    // A traced group's step (L-386) makes its members, the one whose input is most carried first.
    if (took(makeX, y, size, size) && can) placer->make(stepRuns(index, plan.request, step, plan.held));
    return false;
}
void stepTip(view::Late& late, state::Plan const& plan, StepLine const& step, Count now, bool stored) {
    auto const& index = plan.table->index;
    late.tip = planItemName(index, step.item), late.hovered = planPicture(plan, step.item);
    late.tipLines = {countsLine("recipes.col.now", now, step.made)};
    for (auto& line : stepInputLines(index, plan.request, step, plan.held)) late.tipLines.push_back(std::move(line));
    if (!stored) return;
    auto const parts = stepParts(index, plan.request, step);
    auto const short_ = stepShort(index, plan.request, step, plan.held);
    Item const items = static_cast<Item>(index.items.size());
    for (size_t k = 0; k < parts.size() && k < short_.size(); ++k)
        if (short_[k] && parts[k].item < items) storedLines(late, plan, parts[k].item);
}
void wantedTip(view::Late& late, WantedEntry const& entry, Count held, bool sharedAbove) {
    using namespace lamium::ui;
    late.tip = schematic::items::shown(entry.key).name, late.hovered = entry.key;
    late.tipLines = {countsLine("recipes.col.held", held, entry.count)};
    if (!entry.source.empty()) late.tipLines.push_back(translated("recipes.from." + entry.source));
    if (sharedAbove) late.tipLines.push_back(translated("recipes.sharedAbove"));
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
    // Held/needed and what is left first (L-387); a group's own lines follow.
    late.tipLines.insert(late.tipLines.begin(), countsLine("recipes.col.held", line.held, line.need));
    // What the counted stores held when last seen, place by place, beside what is carried.
    if (key.empty() && line.id >= 0 && line.id < itemCount) storedLines(late, plan, line.id);
}
namespace {
using namespace lamium::ui;
namespace v = palette::vanilla;
// One frame of the window under its tab strip: what every tab shares, and the stores' tab, which needs no plan.
struct Window {
    Window(MinecraftUIRenderContext& context, view::Input const& input, view::Late& late, LocalPlayer& player, bool shift, std::string& showItem,
           bool& showChoice, RecipeViewLayout const& l, WindowSpan const& span, bool pressed)
        : context(context), input(input), late(late), player(player), shift(shift), showItem(showItem), showChoice(showChoice), l(l), span(span),
          pressed(pressed) {}
    MinecraftUIRenderContext& context;
    view::Input const& input;
    view::Late& late;
    LocalPlayer& player;
    bool shift;
    std::string& showItem;
    bool& showChoice;
    RecipeViewLayout const l;
    WindowSpan const span;
    float const px = input.pointerX, py = input.pointerY;
    bool pressed; // the frame's one press, until a control takes it
    state::Plan const* plan = state::plan(player);
    // The rows end short of the scrollbar and the scrollbar short of the window's shadow, 3 units each (L-369).
    float const left = span.left + 6, right = span.left + span.width - 12;
    float top = l.top + headTop;
    float const listTop = top; // every tab's scrollbar spans the same height, from here to the bottom
    float const bottom = l.top + l.height - RecipeViewLayout::foot;
    int& offset = scroll[static_cast<int>(tab)];
    int wheel = input.wheel;
    bool over(float x, float y, float w, float h) { return px >= x && px < x + w && py >= y && py < y + h; }
    bool took(float x, float y, float w, float h) {
        if (!pressed || !over(x, y, w, h)) return false;
        pressed = false;
        return true;
    }
    // Every list scrolls with the shared bar in the margin right of the rows; `visible` is how many rows of their
    // real heights fit at the list's end.
    void bar(int count, int& first, int visible) {
        listScrollbar(context, right + 5, listTop, bottom - listTop, count, std::max(1, visible), first, px, py, pressed, input.held);
    }
    int fitFromEnd(std::vector<float> const& heights) {
        float room = bottom - top;
        int fit = 0;
        for (auto h = heights.rbegin(); h != heights.rend() && room >= *h; ++h) room -= *h, ++fit;
        return std::max(1, fit);
    }
    void note(std::string text) { windowLabel(context, left, top + 2, right - left, std::move(text), true); }
    // A place by what it is: the ender chest, or a store's block and where it stands.
    std::string placeName(Place const& place) {
        std::string name = schematic::items::shown(place.block).name;
        return place.ender ? name : translated("recipes.place.at", name, place.at.x, place.at.y, place.at.z);
    }
    // A small button; returns its left edge less a gap.
    float rowButton(float end, float y, std::string text, auto&& act) {
        // `y`: where a switch's top would be; the taller button keeps the same middle.
        float const w = rowButtonWidth(context, text);
        y -= (rowButtonHeight - switchHeight) / 2;
        drawRowButton(context, end - w, y, w, std::move(text), over(end - w, y, w, rowButtonHeight));
        if (took(end - w, y, w, rowButtonHeight)) act();
        return end - w - 3;
    }
    void placesTab() {
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
    }
};

// The tabs drawn from the plan, once it is read.
struct Lists : Window {
    explicit Lists(Window const& window) : Window(window) {}
    Index const& index = plan->table->index;
    Request const& request = plan->request;
    Item const itemCount = static_cast<Item>(index.items.size());
    Notes const notes = state::notes();

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
    void columnHeads(float y, char const* wantKey, bool withName = false, std::string name = {}, char const* heldKey = "recipes.col.held") {
        if (withName) windowLabel(context, nameX, y + 1, nameEnd - nameX, std::move(name), true);
        // A heading may reach left into the gap before its column ("今作れる" whole).
        labelScaled(context, heldX - 12, y + 2, heldW + 12, translated(heldKey), .75f, v::windowFaint, Align::Right, false);
        labelScaled(context, wantX - 6, y + 2, wantW + 6, translated(wantKey), .75f, v::windowFaint, Align::Right, false);
    }
    // A count cut to fit its column shows whole under the pointer.
    void number(float x, float w, float y, Count count, bool faint) {
        std::string text = std::to_string(count);
        if (late.tip.empty() && textWidth(context, text) > w && over(x, y - 2, w, 12)) late.tip = wholeCountText(static_cast<std::uint64_t>(count));
        windowLabel(context, x, y, w, std::move(text), faint, Align::Right);
    }
    // A group as its picture, holding still (GroupLook.h): the member chosen, else the one most held, else the one it
    // is known by, unless the wheel looks through them.
    std::string groupFace(int group) {
        Item const face = groupLook::face(index, group, request.picks, &plan->held);
        return face >= 0 ? index.items[static_cast<size_t>(face)] : std::string();
    }
    std::string pictureOf(Item id) {
        if (id >= itemCount) return groupFace(id - itemCount);
        return id >= 0 ? index.items[static_cast<size_t>(id)] : std::string();
    }
    std::string nameOf(Item id) {
        if (id >= itemCount) return groupName(index, id - itemCount);
        return id >= 0 ? schematic::items::shown(index.items[static_cast<size_t>(id)]).name : std::string();
    }
    void icon(std::string const& key, float x, float y, float scale = 1.f, float alpha = 1.f) {
        if (key.empty()) return;
        if (auto const* stack = schematic::items::iconStack(schematic::items::shown(key).icon)) late.icons.push_back({stack, x, y, scale, 0, alpha});
    }
    // An item or a group as its picture; a group any member of which will do carries the mark.
    void itemIcon(Item id, float x, float y, float alpha = 1.f) {
        icon(pictureOf(id), x, y, 1.f, alpha);
        if (id >= itemCount) late.marks.push_back({x, y});
    }
    // Under the pointer a group names itself, says what it shows and shows its members. The wheel looks through
    // them only while the pointer rests on the group's picture, (x, y) its top left (L-369): over the rest of a row
    // the wheel scrolls.
    void groupTip(int group, float x, float y) {
        if (over(x, y, 18, 18)) groupLook::hover(group, x, y);
        groupLook::tip(late, index, group, request.picks, &plan->held);
    }
    // A recipe by what it is made from: its first input.
    std::string recipeName(int number) {
        auto const& recipe = index.recipes[static_cast<size_t>(number)];
        if (recipe.uses.empty()) return stationName(recipe.station);
        auto const& part = recipe.uses.front();
        return translated("recipes.howTo.from", part.group ? groupName(index, part.id) : nameOf(part.id));
    }
    // A choice's state, at the right of a row: automatic, faint; chosen, on green. A press goes to its line in
    // "How to make". Returns its left edge less a gap.
    void jumpHowTo(std::string const& key) {
        tab = Tab::HowTo;
        howOpen = howScroll = howFlash = key;
        howFlashAt = std::chrono::steady_clock::now();
    }
    float stateTag(float end, float y, std::string text, bool chosen, std::string const& key, float room = 64) {
        if (room < 14) return end;
        float const w = std::min({64.f, room, textWidth(context, text) * .75f + 6}), x = end - w;

        bool const hot = over(x, y, w, 10);
        if (hot) fill(context, x, y, w, 10, palette::white, .2f), late.hints.push_back(translated("recipes.howTo.change"));
        labelScaled(context, x + 3, y + 2.5f, w - 6, std::move(text), .75f, chosen ? v::windowInk : v::windowFaint, Align::Left, false);
        if (took(x, y, w, 10)) jumpHowTo(key);
        return x - 3;
    }
    float recipeState(float end, float y, Item item, float room = 64) {
        // Every item made has one (2026-11-22): "gather" is a choice even beside one recipe.
        if (recipeOptions(index, item).empty()) return end;
        auto const& key = index.items[static_cast<size_t>(item)];
        int const now = plan->choice->recipe[static_cast<size_t>(item)];
        bool const chosen = notes.choices.contains(key) && now >= 0;
        return stateTag(end, y, chosen ? recipeName(now) : translated("recipes.howTo.auto"), chosen, key, room);
    }
    float groupState(float end, float y, int group, float room = 64) {
        auto picked = request.picks.find(group);
        bool const chosen = picked != request.picks.end(), gathered = !chosen && request.stops.contains(index.groupItem(group));
        return stateTag(end, y, chosen ? nameOf(picked->second) : translated(gathered ? "recipes.toGather" : "recipes.howTo.any"), chosen || gathered,
                        groupKey(index, group), room);
    }
    // A step as the game's slots: inputs right-aligned to the arrow (room for four), the product in a fixed
    // column, the station. `x0` is where the inputs' room starts.
    static constexpr float inputsW = 72;
    // How to make's room: the longest recipe in the game's table, five inputs (rabbit stew).
    static constexpr float howInputsW = 90;
    // What "to make" counts as held: its steps' tips give the step and each input as held/needed, as the panel's do
    // (stepTip); `tipNow` what can be made now, `tipStored` whether the stores make up what is short.
    std::map<Item, Count> const* tipStock = nullptr;
    Count tipNow = 0;
    bool tipStored = false;
    std::vector<std::string> inputLines(StepLine const& step) {
        return tipStock ? stepInputLines(index, request, step, *tipStock) : stepInputLines(index, request, step);
    }
    float inputsRoom(std::vector<StepLine> const& steps) {
        size_t most = 1;
        for (auto const& step : steps) most = std::max(most, index.recipes[static_cast<size_t>(step.recipe)].uses.size());
        return static_cast<float>(std::min<size_t>(most, 4)) * 18;
    }
    float stepSlots(float x0, float y, StepLine const& step, bool faint, bool interactive = true, float room = inputsW,
                    std::vector<bool> const* shortParts = nullptr, std::vector<bool> const* storedParts = nullptr) {
        // A traced group's step (L-386) shows the group made from its kind; an item's step its recipe's parts.
        std::string const key = pictureOf(step.item);
        bool const grouped = groupStep(index, step);
        auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
        size_t const iconsBefore = late.icons.size();
        float const arrowX = x0 + room, productX = arrowX + 12;
        auto const uses = stepParts(index, request, step);
        size_t const shown = std::min<size_t>(uses.size(), static_cast<size_t>(std::max(1.f, room / 18)));
        for (size_t u = 0; u < shown; ++u) {
            auto const& part = uses[u].part;
            float const sx = arrowX - static_cast<float>(shown - u) * 18;
            Item const item = uses[u].item;
            // A use held too few of for all the crafts, on the game's red (L-370).
            windowSlot(context, sx, y + 1, over(sx, y + 1, 18, 18), shortParts && u < shortParts->size() && (*shortParts)[u]);
            itemIcon(item, sx + 1, y + 2);
            Count const n = uses[u].need;
            if (n > 1) late.counts.push_back({sx, y + 1, slotCountText(n)});
            bool const stored = storedParts && u < storedParts->size() && (*storedParts)[u];
            if (over(sx, y + 1, 18, 18)) {
                if (part.group) {
                    groupTip(part.id, sx, y + 1);
                    if (interactive) late.hints.push_back(translated("recipes.howTo.change"));
                } else late.tip = nameOf(item), late.hovered = pictureOf(item);
                // Held/needed where what is held is counted (L-387); else the slot's count whole where it shows cut ("44k").
                if (tipStock) late.tipLines.insert(late.tipLines.begin(), countsLine("recipes.col.held", heldOf(index, *tipStock, item), n));
                else if (n >= 1000) late.tipLines.insert(late.tipLines.begin(), translated("recipes.col.need") + ": " + wholeCountText(static_cast<std::uint64_t>(n)));
                if (stored && item < itemCount) storedLines(late, *plan, item);
            }
            if (interactive && part.group && took(sx, y + 1, 18, 18)) jumpHowTo(groupKey(index, part.id));
        }
        if (uses.size() > shown && over(x0, y, room, stepHeight))
            late.tip = nameOf(step.item), late.tipLines = inputLines(step);
        changeArrow(context, arrowX + 3, y + 7.5f, 1, v::windowInk);
        windowSlot(context, productX, y + 1, over(productX, y + 1, 18, 18));
        itemIcon(step.item, productX + 1, y + 2);
        if (step.made > 1) late.counts.push_back({productX, y + 1, slotCountText(step.made)});
        // How to make's rows (not interactive) name every picture and nothing else (L-389).
        if (!interactive && over(productX, y + 1, 18, 18)) late.tip = nameOf(step.item), late.hovered = key;
        if (interactive && over(productX, y + 1, 18, 18)) {
            if (tipStock) stepTip(late, *plan, step, tipNow, tipStored);
            else if (grouped) groupTip(step.item - itemCount, productX, y + 1), late.tipLines = inputLines(step);
            else {
                late.tip = nameOf(step.item), late.tipLines = inputLines(step), late.hovered = key;
                if (step.made >= 1000) late.tipLines.insert(late.tipLines.begin(), translated("recipes.col.need") + ": " + wholeCountText(static_cast<std::uint64_t>(step.made)));
            }
        }
        if (interactive && !grouped && took(productX, y + 1, 18, 18)) showItem = key;
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
    }

    void wantedTab() {
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
        int const fits = fitFromEnd(heights);
        bar(count, offset, fits);
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
                // A traced group's step (L-386) is gathered by the group's key; its choice is the group's.
                bool const grouped = groupStep(index, step);
                std::string const key = grouped ? groupKey(index, step.item - itemCount) : index.items[static_cast<size_t>(step.item)];
                float const end = rowButton(tableRight, y + 6, translated("recipes.toGather"), [&] { state::setStop(key, true); });
                // The recipe's state in the room left between the station and the button.
                if (grouped) groupState(end, y + 5, step.item - itemCount, end - after);
                else recipeState(end, y + 5, step.item, end - after);
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
                    // The columns' counts repeated, as every row's tip does (L-387).
                    late.tipLines.insert(late.tipLines.begin(), countsLine("recipes.col.held", material.held, material.need));
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
                    // As the panel's (L-387): the columns' counts repeated.
                    wantedTip(late, entry, have, known && target->sharedAbove());
                }
                if (known && !onControl && took(left, y, controlsX - left, rowHeight)) openEntry = openEntry == id ? std::string() : id;
            }
            y += line.height;
        }
        // The wheel last, as a count's box may take it; held to what the bar allows, so a list that fits stays put.
        offset = std::clamp(offset + wheel, 0, std::max(0, count - fits));
    }

    void gatherTab() {
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
    }

    void makeTab() {
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
            return placer && placer->preview(index.recipes[static_cast<size_t>(layRecipe(index, request, step, plan->held))], false).ready();
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
                            for (auto const& run : stepRuns(index, request, other.step, plan->held)) ready.push_back(run);
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
                tipStock = line.state != StepState::Held ? &carried : nullptr;
                tipNow = line.row->now, tipStored = line.state == StepState::Stored;
                stepSlots(left, y, step, faint, true, inputsW, &shortParts, &storedParts);
                tipStock = nullptr, tipNow = 0, tipStored = false;
                if (line.state == StepState::Held) {
                    // Held: a check where the counts would be.
                    drawCheckMark(context, wantX + wantW - 8, y + 6, v::windowFaint);
                } else {
                    Count const now = line.row->now;
                    number(heldX, heldW, y + 6, now, faint);
                    number(wantX, wantW, y + 6, step.made, faint);
                    progressBar(context, barX, y + 10, barW, 3, step.made > 0 ? static_cast<float>(now) / static_cast<float>(step.made) : 1.f);
                    // Laid in, the window steps aside as the recipe view does: the grid it covers shows the result.
                    if (stepButtons(context, late, placer, *plan, step, tableRight - 24, y + 4.5f, px, py, [&](float bx, float by, float bw, float bh) { return took(bx, by, bw, bh); }, input.ctrl, input.shift)) late.placed = true;
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
    }

        // ---- How to make ----
    void howToTab() {
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
        // An item has a line where there is a choice: several recipes, or one gathered (the player's "gather", or the
        // rule giving it no recipe: leather, diamond, a loop's base) that a recipe could make.
        auto gatheredNow = [&](Item item) {
            return request.stops.contains(item) || (static_cast<size_t>(item) < plan->choice->recipe.size() && plan->choice->recipe[static_cast<size_t>(item)] < 0);
        };
        // A recipe the player chose keeps its line too, so the choice can be taken back where it was made.
        auto choosable = [&](Item item) {
            if (item < 0 || item >= itemCount) return false;
            auto const options = recipeOptions(index, item).size();
            return options > 1 || ((gatheredNow(item) || notes.choices.contains(index.items[static_cast<size_t>(item)])) && options > 0);
        };
        if (input.plan) {
            for (auto const& line : plan->view.gather) {
                if (line.id >= itemCount) addGroup(line.id - itemCount);
                else if (gatheredNow(line.id) && choosable(line.id)) addItem(line.id);
            }
            for (auto const& line : plan->view.done)
                if (line.id >= 0 && line.id < itemCount && gatheredNow(line.id) && choosable(line.id)) addItem(line.id);
            for (auto const& step : plan->view.steps) {
                for (auto const& part : stepParts(index, request, step)) if (part.part.group) addGroup(part.part.id);
                if (groupStep(index, step)) addGroup(step.item - itemCount);
                // Every item the plan makes has a line: "gather" is a choice even beside one recipe.
                if (!groupStep(index, step) && step.item < itemCount && !recipeOptions(index, step.item).empty()) addItem(step.item);
            }
        }
        if (showAll) {
            // Every group the player chose a member for is among the request's picks, plan or not.
            for (auto const& [group, member] : request.picks) addGroup(group);
            for (auto const& [key, recipe] : notes.choices) if (Item const item = index.findKey(key); choosable(item)) addItem(item);
            // The player's own "gather"; what the rule gathers shows only where the plan has it.
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
        // In the item list's creative order (2026-11-22), a group by the member it is known by: the order holds still
        // when a choice changes the plan, so no line moves under the pointer.
        auto rank = [&](Item item) { return std::pair{itemList::creativeRank(index.items[static_cast<size_t>(item)]), nameOf(item)}; };
        std::ranges::stable_sort(groups, {}, [&](int group) { return rank(groupLead(index, group)); });
        std::ranges::stable_sort(items, {}, rank);
        auto traced = [&](int group) { return static_cast<size_t>(group) < index.traces.size() && index.traces[static_cast<size_t>(group)].kind >= 0; };
        // L-389 (demos/howto-choices-3.html). An open group: "<kind>から作る" where the plan traces it, "<group>を集める",
        // then its members in the item list's order. An open item: "automatic" (the rule's way, faint), "gather", the
        // recipes (the rule's first), re-dyeing folded under one line. `option`: a group's way (0 make, 1 gather), an
        // item's -1 automatic, -2 gather, else the recipe's number; `inner`: inside the re-dyeing fold.
        struct Line { enum class Kind { Switch, Label, Group, GroupWay, Members, Recipe, Option, Fold } kind; int group = -1; Item item = -1; int option = -1; float height = 0; bool inner = false; };
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
                if (traced(group)) lines.push_back({Line::Kind::GroupWay, group, -1, 0, optionHeight});
                lines.push_back({Line::Kind::GroupWay, group, -1, 1, optionHeight});
                size_t const cells = index.groups[static_cast<size_t>(group)].members.size();
                lines.push_back({Line::Kind::Members, group, -1, -1, static_cast<float>((cells + across - 1) / across) * pitch + 6});
            }
        }
        if (!items.empty()) lines.push_back({Line::Kind::Label, -1, -1, 1, labelHeight});
        for (Item item : items) {
            std::string const& key = index.items[static_cast<size_t>(item)];
            if (howScroll == key) offset = static_cast<int>(lines.size()), howScroll.clear();
            lines.push_back({Line::Kind::Recipe, -1, item, -1, stepHeight});
            if (howOpen == key) {
                lines.push_back({Line::Kind::Option, -1, item, -1, optionHeight});
                lines.push_back({Line::Kind::Option, -1, item, -2, optionHeight});
                auto const listed = howToRecipes(index, item, plan->table->choice.recipe[static_cast<size_t>(item)]);
                for (int number : listed.main) lines.push_back({Line::Kind::Option, -1, item, number, optionHeight});
                if (!listed.redye.empty()) {
                    // Open while asked, or while the recipe in use is a re-dye, so the chosen one is never hidden.
                    int const now = plan->choice->recipe[static_cast<size_t>(item)];
                    bool const open = howRedye.contains(key) || (notes.choices.contains(key) && std::ranges::find(listed.redye, now) != listed.redye.end());
                    lines.push_back({Line::Kind::Fold, -1, item, static_cast<int>(listed.redye.size()), labelHeight + 4});
                    if (open) for (int number : listed.redye) lines.push_back({Line::Kind::Option, -1, item, number, optionHeight, true});
                }
            }
        }
        howScroll.clear(); // found above or not there: either way it is done
        int const count = static_cast<int>(lines.size());
        std::vector<float> heights;
        for (auto const& line : lines) heights.push_back(line.height);
        offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
        bar(count, offset, fitFromEnd(heights));
        bool const flashing = std::chrono::steady_clock::now() - howFlashAt < std::chrono::milliseconds(900);
        // A group's ways (L-389): made from its kind where the plan traces it (0), gathered (1), in words from names alone.
        auto wayWords = [&](int group, int way) {
            if (way == 0) return translated("recipes.howTo.makeFrom", groupName(index, index.traces[static_cast<size_t>(group)].kind));
            return translated("recipes.howTo.gatherGroup", groupName(index, group));
        };
        // Whether a group not decided is made (from its kind), not gathered: traced and not set to gather.
        auto groupMade = [&](int group) { return traced(group) && !request.stops.contains(index.groupItem(group)); };
        // The line's parts: item icon and name, the way's inputs, an arrow, the product, the station, the words. The
        // arrow stands as far right as the longest words allow (2026-11-22): the longest recipe (five inputs) shows
        // whole and the name gives way, not the recipe. An item's station and words keep one column each.
        float rightNeed = 64 + std::max(textWidth(context, translated("recipes.howTo.auto")), textWidth(context, translated("recipes.toGather"))) + 4;
        for (int group : groups) {
            auto picked = request.picks.find(group);
            std::string const now = picked != request.picks.end() ? translated("recipes.howTo.chosenIs", nameOf(picked->second))
                                                                  : wayWords(group, groupMade(group) ? 0 : 1);
            float widest = std::max(textWidth(context, now), textWidth(context, wayWords(group, 1)));
            if (traced(group)) widest = std::max(widest, textWidth(context, wayWords(group, 0)));
            rightNeed = std::max(rightNeed, 34 + widest + 4);
        }
        float const arrowX = std::clamp(std::floor(right - rightNeed), mid, std::max(mid, right - 64 - 24));
        float const slotX = arrowX + 12, wordsX = slotX + 22, stationColumnEnd = arrowX + 64;
        float const optionX = std::max(left + 18, arrowX - howInputsW);
        // A group's state on one line (L-370): what it is now in the words of its way, or the member decided.
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
                windowLabel(context, nameX, y + 6, arrowX - nameX - 4, groupName(index, line.group));
                changeArrow(context, arrowX + 3, y + 7.5f, 1, v::windowInk);
                windowSlot(context, slotX, y + 1);
                icon(chosen ? index.items[static_cast<size_t>(picked->second)] : groupFace(line.group), slotX + 1, y + 2);
                if (!chosen) late.marks.push_back({slotX + 1, y + 2});
                // What the group is now, in the words of its way (L-389): the member decided, made from its kind, gathered.
                if (chosen) words(y, translated("recipes.howTo.chosenIs", nameOf(picked->second)), true);
                else words(y, wayWords(line.group, groupMade(line.group) ? 0 : 1), false);
                // Left the group; right what is in use: the member decided as an item, else the group.
                if (late.tip.empty() && over(left, y, arrowX - left, stepHeight)) groupTip(line.group, left + 7, y + 1);
                else if (late.tip.empty() && over(arrowX, y, right - arrowX, stepHeight)) {
                    if (chosen) late.tip = nameOf(picked->second), late.hovered = index.items[static_cast<size_t>(picked->second)];
                    else groupTip(line.group, slotX, y + 1);
                }
                if (took(left, y, right - left, stepHeight)) howOpen = howOpen == key ? std::string() : key;
            } else if (line.kind == Line::Kind::GroupWay) {
                // A way of getting the group itself, a line each: made from its kind (traced, L-386), or gathered.
                std::string const key = groupKey(index, line.group);
                bool const firstWay = at == 0 || lines[static_cast<size_t>(at - 1)].kind != Line::Kind::GroupWay;
                float const h = optionHeight, sy = y + 2;
                sunkFace(context, left + 18, y, right - left - 18, h, firstWay, false);
                bool const hot = over(left + 18, y, right - left - 18, h);
                if (hot) fill(context, left + 19, y, right - left - 20, h, palette::white, .2f);
                bool const on = !request.picks.contains(line.group) && (line.option == 0) == groupMade(line.group);
                if (on) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                if (line.option == 0) {
                    // The kind it is made from, an arrow, the group: as a recipe's slots, its inputs left of the arrow.
                    auto const& trace = index.traces[static_cast<size_t>(line.group)];
                    float const kindX = arrowX - 18;
                    windowSlot(context, kindX, sy + 1, false);
                    icon(groupFace(trace.kind), kindX + 1, sy + 2);
                    late.marks.push_back({kindX + 1, sy + 2});
                    if (trace.uses > 1) late.counts.push_back({kindX, sy + 1, slotCountText(trace.uses)});
                    changeArrow(context, arrowX + 3, sy + 7.5f, 1, v::windowInk);
                    if (late.tip.empty() && over(kindX, sy + 1, 18, 18)) groupTip(trace.kind, kindX, sy + 1);
                }
                windowSlot(context, slotX, sy + 1, false);
                icon(groupFace(line.group), slotX + 1, sy + 2);
                late.marks.push_back({slotX + 1, sy + 2});
                if (line.option == 0 && index.traces[static_cast<size_t>(line.group)].makes > 1)
                    late.counts.push_back({slotX, sy + 1, slotCountText(index.traces[static_cast<size_t>(line.group)].makes)});
                windowLabel(context, slotX + 22, sy + 6, right - slotX - 22, wayWords(line.group, line.option));
                if (took(left + 18, y, right - left - 18, h)) {
                    state::setPick(key, {});
                    // Gathering a traced group is the player's "gather"; for one not traced it is what "any" does anyway.
                    state::setStop(key, line.option == 1 && traced(line.group));
                    howOpen.clear();
                }
            } else if (line.kind == Line::Kind::Members) {
                // The members as the game's slots, in the item list's order (L-389); a press decides one, whose own ways
                // are then its line under "Recipes", as any item's.
                std::string const key = groupKey(index, line.group);
                auto picked = request.picks.find(line.group);
                sunkFace(context, left + 18, y, right - left - 18, line.height, false, true);
                auto const members = groupLook::members(index, line.group);
                for (size_t c = 0; c < members.size(); ++c) {
                    float const cx = left + 22 + static_cast<float>(c % across) * pitch, cy = y + 4 + static_cast<float>(c / across) * pitch;
                    bool const hot = over(cx, cy, 18, 18);
                    windowSlot(context, cx, cy, hot);
                    Item const member = members[c];
                    auto const& memberKey = index.items[static_cast<size_t>(member)];
                    icon(memberKey, cx + 1, cy + 1);
                    if (picked != request.picks.end() && picked->second == member) frame(context, cx - 1, cy - 1, 20, 20, palette::accentDeep);
                    // No counts held in "How to make" (maintainer 2026-11-21).
                    if (hot) late.tip = nameOf(member), late.hovered = memberKey;
                    if (took(cx, cy, 18, 18)) state::setPick(key, memberKey), state::setStop(key, false), howOpen.clear();
                }
            } else if (line.kind == Line::Kind::Recipe) {
                std::string const& key = index.items[static_cast<size_t>(line.item)];
                int const now = plan->choice->recipe[static_cast<size_t>(line.item)], rule = plan->table->choice.recipe[static_cast<size_t>(line.item)];
                bool const gathered = request.stops.contains(line.item);
                bool const chosen = notes.choices.contains(key) && now >= 0;
                if (over(left, y, right - left, stepHeight)) fill(context, left - 2, y, right - left + 4, stepHeight, palette::shade, .08f);
                if (flashing && howFlash == key) frame(context, left - 2, y, right - left + 4, stepHeight, palette::accentDeep);
                // The way in use as its row below shows it (L-389), in full ink: a recipe's slots and station, "gather" by
                // the item, the rule's way with the word "automatic"; the name gives way to the slots.
                bool const automatic = !gathered && !chosen;
                int const shownRecipe = gathered ? -1 : automatic ? rule : now;
                float nameEnd = arrowX - 4;
                if (shownRecipe >= 0) {
                    StepLine const way{line.item, shownRecipe, 1, index.recipes[static_cast<size_t>(shownRecipe)].count, 0, 0};
                    size_t const parts = std::min<size_t>(stepParts(index, request, way).size(), static_cast<size_t>(howInputsW / 18));
                    nameEnd = std::max(optionX, arrowX - static_cast<float>(parts) * 18) - 4;
                    stepSlots(optionX, y, way, false, false, howInputsW);
                } else {
                    windowSlot(context, slotX, y + 1, false);
                    icon(key, slotX + 1, y + 2);
                    if (over(slotX, y + 1, 18, 18)) late.tip = nameOf(line.item), late.hovered = key;
                }
                icon(key, left + 8, y + 2);
                windowLabel(context, nameX, y + 6, nameEnd - nameX, nameOf(line.item));
                if (automatic || gathered)
                    windowLabel(context, stationColumnEnd + 2, y + 6, right - stationColumnEnd - 2, translated(automatic ? "recipes.howTo.auto" : "recipes.toGather"));
                if (late.tip.empty() && over(left, y, nameEnd - left, stepHeight)) late.tip = nameOf(line.item), late.hovered = key;
                if (took(left, y, right - left, stepHeight)) howOpen = howOpen == key ? std::string() : key;
            } else if (line.kind == Line::Kind::Option) {
                // An item's ways as rows of slots on a sunk face (L-389): "automatic" (the rule's way, faint, its word
                // alone), "gather", then each recipe as its slots and station; a row has no tip, its pictures name themselves.
                std::string const& key = index.items[static_cast<size_t>(line.item)];
                bool const first = at == 0 || (lines[static_cast<size_t>(at - 1)].kind != Line::Kind::Option && lines[static_cast<size_t>(at - 1)].kind != Line::Kind::Fold);
                bool const last = at + 1 >= count || (lines[static_cast<size_t>(at + 1)].kind != Line::Kind::Option && lines[static_cast<size_t>(at + 1)].kind != Line::Kind::Fold);
                float const h = optionHeight, sy = y + 2; // the slots' row inside it
                sunkFace(context, left + 18, y, right - left - 18, h, first, last);
                bool const gathered = request.stops.contains(line.item);
                bool const chosen = notes.choices.contains(key);
                int const now = plan->choice->recipe[static_cast<size_t>(line.item)], rule = plan->table->choice.recipe[static_cast<size_t>(line.item)];
                bool const hot = over(left + 18, y, right - left - 18, h);
                if (hot) fill(context, left + 19, y, right - left - 20, h, palette::white, .2f);
                // Making it again: the plan no longer gathers it.
                auto make = [&] { if (gathered) state::setStop(key, false); };
                if (line.option == -2) {
                    // "Gather": the item itself in the product's slot.
                    windowSlot(context, slotX, sy + 1, false);
                    icon(key, slotX + 1, sy + 2);
                    if (over(slotX, sy + 1, 18, 18)) late.tip = nameOf(line.item), late.hovered = key;
                    if (gathered) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                    windowLabel(context, stationColumnEnd + 2, sy + 6, right - stationColumnEnd - 2, translated("recipes.toGather"));
                    if (took(left + 18, y, right - left - 18, h)) {
                        if (!gathered) state::setStop(key, true);
                        howOpen.clear();
                    }
                } else if (line.option == -1) {
                    // "Automatic": the rule's recipe as slots, faint, or the item where the rule gathers it (L-370).
                    if (rule >= 0) {
                        StepLine const ruled{line.item, rule, 1, index.recipes[static_cast<size_t>(rule)].count, 0, 0};
                        stepSlots(optionX, sy, ruled, true, false, howInputsW);
                    } else {
                        windowSlot(context, slotX, sy + 1, false);
                        icon(key, slotX + 1, sy + 2, 1.f, .45f);
                        if (over(slotX, sy + 1, 18, 18)) late.tip = nameOf(line.item), late.hovered = key;
                    }
                    if (!chosen && !gathered) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                    windowLabel(context, stationColumnEnd + 2, sy + 6, right - stationColumnEnd - 2, translated("recipes.howTo.auto"));
                    if (took(left + 18, y, right - left - 18, h)) state::setChoice(key, {}), make(), howOpen.clear();
                } else {
                    int const number = line.option;
                    StepLine const one{line.item, number, 1, index.recipes[static_cast<size_t>(number)].count, 0, 0};
                    stepSlots(optionX, sy, one, false, false, howInputsW);
                    if (chosen && now == number && !gathered) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                    // No words and no tip of the row's own (L-389): its slots tell it apart, each naming itself.
                    if (took(left + 18, y, right - left - 18, h)) state::setChoice(key, choiceKey(index.recipes[static_cast<size_t>(number)])), make(), howOpen.clear();
                }
            } else if (line.kind == Line::Kind::Fold) {
                // Re-dyeing (another color of the same thing), folded under one line that opens (L-389).
                std::string const& key = index.items[static_cast<size_t>(line.item)];
                bool const last = at + 1 >= count || lines[static_cast<size_t>(at + 1)].kind != Line::Kind::Option;
                sunkFace(context, left + 18, y, right - left - 18, line.height, false, last);
                bool const hot = over(left + 18, y, right - left - 18, line.height);
                if (hot) fill(context, left + 19, y, right - left - 20, line.height, palette::white, .2f);
                bool const open = at + 1 < count && lines[static_cast<size_t>(at + 1)].kind == Line::Kind::Option && lines[static_cast<size_t>(at + 1)].inner;
                chevron(context, left + 24, y + 4, open, v::windowInk);
                windowLabel(context, left + 34, y + 4, right - left - 38, translated("recipes.howTo.redye", line.option));
                if (took(left + 18, y, right - left - 18, line.height)) {
                    if (open) howRedye.erase(key);
                    else howRedye.insert(key);
                }
            }
            y += line.height;
        }
    }
};
}

void draw(MinecraftUIRenderContext& context, view::Input const& input, view::Late& late, LocalPlayer& player, bool shift, std::string& showItem,
          bool& showChoice) {
    using namespace lamium::ui;
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
    Window window(context, input, late, player, shift, showItem, showChoice, l, span, pressed);
    if (tab == Tab::Places) { window.placesTab(); return; }
    if (!window.plan) { window.note(translated("schematic.raw.reading")); return; }
    Lists lists(window);
    if (tab == Tab::Wanted) lists.wantedTab();
    else if (tab == Tab::Gather) lists.gatherTab();
    else if (tab == Tab::Make) lists.makeTab();
    else lists.howToTab();
}
}
