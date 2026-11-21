#include "features/recipes/BandPages.h"
#include "features/recipes/CountEdit.h"
#include "features/recipes/RecipeText.h"
#include "features/schematic/SchematicItems.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include <algorithm>
#include <chrono>
#include <map>

namespace lamium::recipes::bandPages {
namespace {
// The plan's short list, as the window's tabs: the wanted items, what is left to gather, what to make.
enum class SideList { Wanted, Gather, Make };
SideList sideList = SideList::Gather;
constexpr float smallIcon = .625f;
}
void showWanted() { sideList = SideList::Wanted; }
void ground(Frame& frame, band::Box box, bool rule) {
    using namespace lamium::ui;
    // A dimmed panel to the screen's edge: the game dims the world too little for bare icons.
    fill(frame.context, box.x, box.y, box.w, box.h, palette::panel, .5f);
    if (rule) fill(frame.context, box.x + 3, box.y, box.w - 6, 1, palette::white, .2f);
}
void favorites(Frame& frame, SideLayout const& panel) {
    using namespace lamium::ui;
    auto& context = frame.context;
    float const px = frame.px, py = frame.py;
    bool const leftPress = frame.leftPress;
    auto& late = frame.late;
    auto took = [&](ListBox const& box) { return frame.took(box); };
    auto& hoverItem = frame.hoverItem;
    auto& mark = frame.mark;
    bool& pressed = frame.pressed;
    label(context, panel.left + 4, panel.top + 3, panel.width - 8, translated("recipes.favorites"), palette::dim);
    auto const favorites = state::notes().favorites;
    // More than the panel holds scrolls by rows, as the plan's list does: the wheel and the shared bar, the
    // cells making room for it.
    SideLayout grid = panel;
    int const total = static_cast<int>(favorites.size());
    bool const scrolls = total > grid.favorites();
    if (scrolls) grid.columns = std::max(1, static_cast<int>((grid.width - 2 * SideLayout::pad - rowBarGutter) / SideLayout::cell));
    int const columns = std::max(1, grid.columns), rows = grid.favoriteRows(), totalRows = (total + columns - 1) / columns;
    static int firstRow = 0;
    firstRow += frame.favoriteTurns;
    firstRow = std::clamp(firstRow, 0, std::max(0, totalRows - rows));
    if (scrolls) {
        bool onBar = pressed && leftPress;
        listScrollbar(context, grid.left + grid.width - 3, grid.top + SideLayout::head, static_cast<float>(rows) * SideLayout::cell, totalRows,
                      std::max(1, rows), firstRow, px, py, onBar, frame.held);
        if (pressed && leftPress && !onBar) pressed = false;
    }
    int const first = firstRow * columns, shownCount = std::clamp(total - first, 0, grid.favorites());
    int const favoriteHover = grid.hitFavorite(px, py);
    for (int i = 0; i < shownCount; ++i) {
        auto const& key = favorites[static_cast<size_t>(first + i)];
        auto const& item = schematic::items::shown(key);
        float const x = grid.cellX(i), y = grid.cellY(i);
        if (i == favoriteHover) {
            fill(context, x, y, SideLayout::cell, SideLayout::cell, palette::white, .14f);
            late.tip = item.name;
            hoverItem = key;
        }
        if (auto const* stack = schematic::items::iconStack(item.icon)) late.icons.push_back({stack, x + 1, y + 1});
        mark(key, x, y, false); // all here are favorites
    }
    if (pressed && favoriteHover >= 0 && favoriteHover < shownCount) frame.show(favorites[static_cast<size_t>(first + favoriteHover)]);
    (void)leftPress, (void)took;
}
void plan(Frame& frame, SideLayout const& panel, LocalPlayer& player, std::vector<Handed> const& handed,
          std::function<void(planView::Tab)> const& openPlan) {
    using namespace lamium::ui;
    auto& context = frame.context;
    float const px = frame.px, py = frame.py;
    bool const leftPress = frame.leftPress;
    auto& late = frame.late;
    auto took = [&](ListBox const& box) { return frame.took(box); };
    auto& hoverItem = frame.hoverItem;
    bool& pressed = frame.pressed;
    // The bar: which of the three lists shows, named as the window's tabs, and the window at the right
    // end. Where the three do not fit, one button that goes round them.
    struct ListName { SideList list; char const* key; planView::Tab tab; };
    static constexpr ListName lists[]{{SideList::Wanted, "recipes.wanted", planView::Tab::Wanted},
                                      {SideList::Gather, "recipes.gather", planView::Tab::Gather},
                                      {SideList::Make, "recipes.make", planView::Tab::Make}};
    int const listNow = sideList == SideList::Wanted ? 0 : sideList == SideList::Gather ? 1 : 2;
    auto openList = [&] { openPlan(lists[listNow].tab); };
    auto barButton2 = [&](ListBox const& box, std::string text, bool on) {
        drawButton(context, box.x, box.y, box.w, box.h, std::move(text), box.has(px, py), on ? ButtonKind::Primary : ButtonKind::Plain);
        return leftPress && took(box);
    };
    std::string openText = translated("recipes.open");
    float const openWidth = textWidth(context, openText) + 8;
    ListBox const openBox{panel.left + panel.width - 2 - openWidth, panel.barY() + (SideLayout::bar - 11) / 2, openWidth, 11};
    if (barButton2(openBox, std::move(openText), false)) openList();
    std::string listTexts[3];
    float listWidths[3], listsWidth = 0;
    for (int i = 0; i < 3; ++i) {
        listTexts[i] = translated(lists[i].key);
        listWidths[i] = textWidth(context, listTexts[i]) + tabPad;
        listsWidth += listWidths[i];
    }
    // The three lists are tabs. Where their names do not fit beside "Open" (the game's recipe book
    // is out), each is an item's picture, named under the pointer.
    float barX2 = panel.left + 2;
    float const barRoom = openBox.x - 2 - barX2;
    bool const named = listsWidth <= barRoom;
    static constexpr char const* pictures[]{"minecraft:writable_book", "minecraft:iron_pickaxe", "minecraft:crafting_table"};
    constexpr float pictureWidth = 20;
    if (named || 3 * pictureWidth <= barRoom) {
        for (int i = 0; i < 3; ++i) {
            float const w = named ? listWidths[i] : pictureWidth;
            ListBox const box{barX2, panel.barY(), w, SideLayout::bar};
            drawTab(context, box.x, box.y, box.w, named ? std::move(listTexts[i]) : std::string(), i == listNow, box.has(px, py), i == 0, SideLayout::bar);
            if (!named) {
                if (auto const* stack = schematic::items::iconStack(schematic::items::shown(pictures[i]).icon)) late.icons.push_back({stack, box.x + 3, box.y, .875f});
                if (box.has(px, py)) late.tip = std::move(listTexts[i]);
            }
            if (leftPress && took(box)) sideList = lists[i].list;
            barX2 += w;
        }
        tabRule(context, panel.left + 2, panel.barY() + SideLayout::bar, panel.width - 4);
    }
    // The short list: an icon, the name where there is room, and how many; a wanted item's count has the stepper.
    struct SideRow {
        std::string key, name;
        Count count = 0;
        int step = -1; // a row of what to make: its place among the plan's steps
        std::string source; // a wanted item a feature handed over: whose it is; not edited here
        bool faint = false; // a step short of something, or held
        bool group = false; // stands for any of a group
        std::string countText; // in place of `count`: a step made in part, "now/needed"
        bool done = false;     // a step held stock covers whole: a check in place of the count
        StepLine line;         // a row of what to make: its step (a held one is not among the plan's)
        bool gather = false;   // a row of what to gather: its tip says how many are left
    };
    std::vector<SideRow> rows;
    auto const* plan = state::plan(player);
    if (sideList == SideList::Wanted) {
        for (auto const& [key, amount] : state::wanted().targets) rows.push_back({key, schematic::items::shown(key).name, amount});
        for (auto const& target : handed) rows.push_back({target.key, schematic::items::shown(target.key).name, target.count, -1, target.source});
    } else if (plan && sideList == SideList::Gather) {
        auto const& index = plan->table->index;
        Item const items = static_cast<Item>(index.items.size());
        auto const turn = static_cast<size_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1500);
        // As the window's tab (L-370): what is left first, most left first, "held/needed"; then what is gathered,
        // faint with a check; the tip says how many are left.
        auto add = [&](GatherLine const& line, bool done) {
            SideRow row;
            if (line.id < items) {
                row.key = index.items[static_cast<size_t>(line.id)];
                row.name = schematic::items::shown(row.key).name;
            } else {
                auto const& members = index.groups[static_cast<size_t>(line.id - items)].members;
                row.key = members.empty() ? std::string() : index.items[static_cast<size_t>(members[turn % members.size()])];
                row.name = groupName(index, line.id - items), row.group = true;
            }
            row.count = line.left, row.countText = std::format("{}/{}", line.held, line.need), row.faint = done, row.done = done, row.gather = true;
            rows.push_back(std::move(row));
        };
        for (auto const& line : plan->view.gather) if (line.left) add(line, false);
        // What the recipe table does not know (a grass block) is gathered as it is.
        for (auto const& line : plan->loose)
            if (line.left > 0) {
                SideRow row{line.key, schematic::items::shown(line.key).name, line.left};
                row.countText = std::format("{}/{}", line.held, line.need), row.gather = true;
                rows.push_back(std::move(row));
            }
        for (auto const& line : plan->view.done) add(line, true);
    } else if (plan) {
        // What to make, as the window's tab (L-370): made now, made in part ("now/needed"), short (faint), and at the
        // end what held stock covers whole (faint, a check for its count); no headings in rows this low.
        auto const& index = plan->table->index;
        std::map<Item, Count> stock;
        for (auto const& [id, have] : plan->request.held) stock[id] += have;
        for (StepState state : {StepState::Ready, StepState::Partly, StepState::Missing})
            for (size_t i = 0; i < plan->view.steps.size(); ++i) {
                auto const& step = plan->view.steps[i];
                if (stepState(index, plan->request, step, stock) != state) continue;
                auto const& key = index.items[static_cast<size_t>(step.item)];
                SideRow row{key, schematic::items::shown(key).name, step.made, static_cast<int>(i), {}, state == StepState::Missing};
                if (state == StepState::Partly) {
                    Count const each = std::max<Count>(1, index.recipes[static_cast<size_t>(step.recipe)].count);
                    row.countText = std::format("{}/{}", stepNow(index, plan->request, step, stock) * each, step.made);
                }
                row.line = step;
                rows.push_back(std::move(row));
            }
        for (auto const& step : heldSteps(index, *plan->choice, plan->view)) {
            auto const& key = index.items[static_cast<size_t>(step.item)];
            SideRow row{key, schematic::items::shown(key).name, step.made, -1, {}, true};
            row.done = true, row.line = step;
            rows.push_back(std::move(row));
        }
    }
    // The list scrolls: the wheel over it (a stepper under the pointer takes the wheel for itself)
    // and the shared bar at its right, which the rows make room for.
    int const fits = panel.rows(), total = static_cast<int>(rows.size());
    constexpr float stepperWidth = 46;
    static int sideFirst[3]{};
    int& first = sideFirst[listNow];
    float const rowsEnd = panel.left + panel.width - 3 - (total > fits ? rowBarGutter : 0.f);
    {
        int const under = panel.hitRow(px, py) < 0 ? -1 : first + panel.hitRow(px, py);
        bool const onStepper = sideList == SideList::Wanted && under >= 0 && under < total && rows[static_cast<size_t>(under)].source.empty()
            && px >= rowsEnd - stepperWidth;
        if (frame.sideTurns && panel.has(px, py) && !onStepper) first += frame.sideTurns;
    }
    {
        bool onBar = pressed && leftPress;
        listScrollbar(context, panel.left + panel.width - 3, panel.rowY(0), static_cast<float>(fits) * SideLayout::row, total, fits, first, px, py, onBar,
                      frame.held);
        if (pressed && leftPress && !onBar) pressed = false;
    }
    int const drawn = std::min(fits, total - first);
    for (int i = 0; i < drawn; ++i) {
        auto const& row = rows[static_cast<size_t>(first + i)];
        float const y = panel.rowY(i), end = rowsEnd;
        ListBox const whole{panel.left, y, panel.width, SideLayout::row};
        if (!row.key.empty())
            if (auto const* stack = schematic::items::iconStack(schematic::items::shown(row.key).icon))
                late.icons.push_back({stack, panel.left + 3, y + 1, smallIcon, 0, row.faint ? .45f : 1.f});
        if (row.group) late.marks.push_back({panel.left + 3, y + 1, smallIcon});
        float nameEnd = end - 30;
        if (sideList == SideList::Wanted && row.source.empty()) {
            // The stepper: its ends step (minus at one takes the item out), its number is typed after a press.
            float const sx = end - stepperWidth;
            ListBox const box{sx, y + 1, stepperWidth, 10};
            drawStepper(context, sx, y + 1, stepperWidth, 10, true, std::to_string(row.count),
                        {.editing = countEdit::editing(row.key), .pointerX = px, .pointerY = py, .dimValue = row.count == 0});
            if (leftPress && took({sx, y + 1, 11, 10})) state::stepWanted(row.key, false, frame.shift, true);
            else if (leftPress && took({sx + stepperWidth - 11, y + 1, 11, 10})) state::stepWanted(row.key, true, frame.shift, true);
            else if (leftPress && took(box)) countEdit::begin(row.key, row.count);
            else if (took(box)) {}
            else if (frame.sideTurns && box.has(px, py)) state::stepWanted(row.key, frame.sideTurns < 0, frame.shift, false);
            nameEnd = sx - 3;
        } else if (row.done) {
            // Held: a small check where the count would be.
            float const cx = end - 9, cy = y + 3;
            for (int k = 0; k < 2; ++k) fill(context, cx + static_cast<float>(k), cy + 3 + static_cast<float>(k), 1, 1, palette::faint);
            for (int k = 0; k < 4; ++k) fill(context, cx + 2 + static_cast<float>(k), cy + 4 - static_cast<float>(k), 1, 1, palette::faint);
        } else label(context, end - 40, y + 2, 40, row.countText.empty() ? std::to_string(row.count) : row.countText,
                     row.faint ? palette::faint : palette::text, Align::Right);
        if (panel.wide()) label(context, panel.left + 16, y + 2, nameEnd - panel.left - 16, row.name, row.faint ? palette::faint : palette::dim);
        if (whole.has(px, py)) {
            if (hoverItem.empty()) hoverItem = row.key;
            // A row of what to make says what its crafts take, each with what is held of it (L-370).
            if (sideList == SideList::Make && plan && !row.done) {
                std::map<Item, Count> stock;
                for (auto const& [id, have] : plan->request.held) stock[id] += have;
                late.tip = row.name, late.tipLines = stepInputLines(plan->table->index, plan->request, row.line, stock);
            } else if (row.gather) {
                late.tip = row.name;
                late.tipLines = {row.done ? translated("recipes.gathered") : translated("recipes.stepShort", row.count)};
            } else if (row.done) late.tip = row.name, late.tipLines = {translated("recipes.heldSteps")};
            // A row a feature handed over says whose it is.
            else if (!row.source.empty()) late.tip = row.name, late.tipLines = {translated("recipes.from." + row.source)};
            else if (!panel.wide()) late.tip = row.name;
        }
        if (leftPress && took(whole)) openList();
    }
    if (!total)
        label(context, panel.left + 4, panel.rowY(0) + 2, panel.width - 8,
              translated(sideList == SideList::Wanted || (plan && sideList == SideList::Make) ? "recipes.nothingWanted"
                         : plan ? "schematic.raw.nothing" : "schematic.raw.reading"), palette::faint);
}
void storeLine(Frame& frame, band::Box region, placesGame::Open const& open) {
    using namespace lamium::ui;
    auto& context = frame.context;
    float const px = frame.px, py = frame.py;
    bool const leftPress = frame.leftPress;
    auto& late = frame.late;
    auto took = [&](ListBox const& box) { return frame.took(box); };
    state::seePlace(open.ender, open.spot, open.pair, open.block, open.items);
    // A name and a switch, as Lamium's rows are: on makes the open container a store (off takes it
    // out again); the ender chest, which is noted anyway, is switched in or out of the count.
    int const index = open.ender ? state::places().ender() : state::places().store(open.spot);
    bool const on = index >= 0 && (!open.ender || state::places().list[static_cast<size_t>(index)].counted);
    // One switch about the open container, under a rule like the one between the favorites and the
    // plan (no stronger a division than that), with room above and below it.
    float const footY = region.y + region.h - 12;
    ListBox const box{region.x + 2, footY, region.w - 4, 11};
    fill(context, region.x + 3, box.y - 3, region.w - 6, 1, palette::white, .2f);
    if (box.has(px, py)) fill(context, box.x, box.y, box.w, box.h, palette::white, .08f);
    if (region.w >= 75) label(context, box.x + 2, box.y + 1, box.w - switchWidth - 8, translated("recipes.place.count"), palette::dim);
    else if (box.has(px, py)) late.tip = translated("recipes.place.count");
    toggleSwitch(context, box.x + box.w - switchWidth - 2, box.y + 1, on);
    if (leftPress && took(box)) {
        if (open.ender) { if (index >= 0) state::setPlaceCounted(index, !on); }
        else if (index < 0) state::markStore(open.spot, open.pair, open.block, open.items);
        else state::unmarkStore(index);
    }
}
}
