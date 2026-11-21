#include "features/recipes/BandPages.h"
#include "features/recipes/CountText.h"
#include "features/recipes/CountEdit.h"
#include "features/recipes/GroupLook.h"
#include "features/recipes/PlanRows.h"
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
    // The bar: which of the three lists shows, named as the window's tabs. The window opens from the bottom
    // row's button (L-382) or a press on a row.
    struct ListName { SideList list; char const* key; planView::Tab tab; };
    static constexpr ListName lists[]{{SideList::Wanted, "recipes.wanted", planView::Tab::Wanted},
                                      {SideList::Gather, "recipes.gather", planView::Tab::Gather},
                                      {SideList::Make, "recipes.make", planView::Tab::Make}};
    int const listNow = sideList == SideList::Wanted ? 0 : sideList == SideList::Gather ? 1 : 2;
    auto openList = [&] { openPlan(lists[listNow].tab); };
    std::string listTexts[3];
    float listWidths[3];
    for (int i = 0; i < 3; ++i) {
        listTexts[i] = translated(lists[i].key);
        listWidths[i] = textWidth(context, listTexts[i]) + tabPad;
    }
    // The three lists are tabs. Where their names do not fit (the game's recipe book is out), each is an
    // item's picture, named under the pointer.
    float barX2 = panel.left + 2;
    float const barRoom = panel.left + panel.width - 2 - barX2;
    // One width for the three, as the window's tabs (L-369).
    float const tabWidth = std::max({listWidths[0], listWidths[1], listWidths[2]});
    bool const named = 3 * tabWidth <= barRoom;
    static constexpr char const* pictures[]{"minecraft:writable_book", "minecraft:iron_pickaxe", "minecraft:crafting_table"};
    constexpr float pictureWidth = 20;
    if (named || 3 * pictureWidth <= barRoom) {
        for (int i = 0; i < 3; ++i) {
            float const w = named ? tabWidth : pictureWidth;
            ListBox const box{barX2, panel.barY(), w, SideLayout::bar};
            drawTab(context, box.x, box.y, box.w, named ? std::move(listTexts[i]) : std::string(), i == listNow, box.has(px, py), i == 0, SideLayout::bar);
            if (!named) {
                // Twelve high from the tab's top: the chosen tab's line under it stays clear (L-369).
                if (auto const* stack = schematic::items::iconStack(schematic::items::shown(pictures[i]).icon)) late.icons.push_back({stack, box.x + 4, box.y + 1, .75f});
                if (box.has(px, py)) late.tip = std::move(listTexts[i]);
            }
            if (leftPress && took(box)) sideList = lists[i].list;
            barX2 += w;
        }
        tabRule(context, panel.left + 2, panel.barY() + SideLayout::bar, panel.width - 4);
    }
    // The short list: an icon, the name where there is room, held and needed in columns (L-370).
    struct SideRow {
        std::string key, name;
        Count count = 0;
        bool step = false;  // a row of what to make, not one held
        std::string source; // a wanted item a feature handed over: whose it is; not edited here
        bool faint = false; // a step short of something, or held
        bool group = false; // stands for any of a group
        bool done = false;     // gathered, held or all wanted held: a check in place of the count
        StepLine line;         // a row of what to make: its step (a held one is not among the plan's)
        bool gather = false;   // a row of what to gather
        Count held = 0, need = 0; // for the tip: what is held (or can be made now) and what is needed
        GatherLine gatherLine;    // a row of what to gather: its line, for the tip the window shares
        bool loose = false;       // a row of what to gather the recipe table does not know
        bool stored = false;      // a step to make after taking out of the stores (L-385): their chest before its count
        int entry = -1;           // a wanted row: its entry in the plan's shares
    };
    std::vector<SideRow> rows;
    auto const* plan = state::plan(player);
    if (sideList == SideList::Wanted) {
        // As the window's tab (L-370, PlanRows.h): the count only, the stepper under the pointer; what is all held
        // follows at the end, faint with a check. A feature's row and the player's of the same item stay two rows.
        if (auto const* shares = plan ? state::shares(player) : nullptr) {
            static std::map<std::string, bool> shownDone;
            auto const held = wantedHeld(shares->entries, shares->targets, plan->loose);
            for (auto const& wanted : wantedRows(shares->entries, held, shownDone, panel.has(px, py))) {
                auto const& entry = shares->entries[wanted.entry];
                SideRow row{entry.key, schematic::items::shown(entry.key).name, entry.count, false, entry.source};
                row.held = wanted.held, row.need = entry.count, row.done = row.faint = wanted.done, row.entry = static_cast<int>(wanted.entry);
                rows.push_back(std::move(row));
            }
        } else {
            for (auto const& [key, amount] : state::wanted().targets) rows.push_back({key, schematic::items::shown(key).name, amount});
            for (auto const& target : handed) rows.push_back({target.key, schematic::items::shown(target.key).name, target.count, false, target.source});
        }
    } else if (plan && sideList == SideList::Gather) {
        auto const& index = plan->table->index;
        Item const items = static_cast<Item>(index.items.size());
        // As the window's tab (L-370, PlanRows.h): what is left first, most left first, as the one number left; then
        // what is gathered, faint with a check; the tip says what is held and needed.
        auto add = [&](GatherRow const& from) {
            SideRow row;
            auto const& line = from.line;
            if (!from.key.empty()) {
                // What the recipe table does not know (a grass block) is gathered as it is.
                row.key = from.key, row.name = schematic::items::shown(from.key).name, row.loose = true;
            } else if (line.id < items) {
                row.key = index.items[static_cast<size_t>(line.id)];
                row.name = schematic::items::shown(row.key).name;
            } else {
                auto const& members = index.groups[static_cast<size_t>(line.id - items)].members;
                Item const face = members.empty() ? -1 : groupLook::face(index, line.id - items, plan->request.picks, &plan->held);
                row.key = face >= 0 ? index.items[static_cast<size_t>(face)] : std::string();
                row.name = groupName(index, line.id - items), row.group = true;
            }
            row.count = line.left, row.held = line.held, row.need = line.need, row.faint = row.done = from.done, row.gather = true, row.gatherLine = line;
            rows.push_back(std::move(row));
        };
        auto const lists = gatherRows(index, plan->view, plan->loose, plan->request.stops);
        for (auto const& row : lists.left) add(row);
        for (auto const& row : lists.finished) add(row);
    } else if (plan) {
        // What to make, as the window's tab (L-370): made now, made in part, made after taking out of the stores (a
        // chest before the count, L-385), short (faint), each as the one number still to make, and at the end what
        // held stock covers whole (faint, a check); no headings in rows this low. What can be made now, from what is
        // carried, is in the tip.
        auto const& index = plan->table->index;
        std::map<Item, Count> all, carried;
        for (auto const& [id, have] : plan->request.held) all[id] += have;
        for (auto const& [id, have] : plan->held) carried[id] += have;
        for (auto const& made : makeRows(index, plan->request, plan->view, heldSteps(index, *plan->choice, plan->view), carried, all)) {
            bool const held = made.state == StepState::Held;
            auto const& key = index.items[static_cast<size_t>(made.step.item)];
            SideRow row{key, schematic::items::shown(key).name, made.step.made, !held, {}, held || made.state == StepState::Missing};
            row.stored = made.state == StepState::Stored, row.done = held;
            row.held = made.now, row.need = made.step.made, row.line = made.step;
            rows.push_back(std::move(row));
        }
    }
    // The list scrolls: the wheel over it (a count's box under the pointer takes the wheel for itself) and the
    // shared bar at its right, which the rows make room for. As the Notes window's tables (L-370, 2026-11-22,
    // demos/panel-table.html A): under a line of column headings, held (or can make now) and needed in columns of
    // six digits, then the controls, every one always shown; no bar.
    bool const headed = !rows.empty();
    int const fits = std::max(0, panel.rows() - (headed ? 1 : 0)), total = static_cast<int>(rows.size());
    auto rowAt = [&](int i) { return panel.rowY(i + (headed ? 1 : 0)); };
    static int sideFirst[3]{};
    int& first = sideFirst[listNow];
    float const rowsEnd = panel.left + panel.width - 3 - (total > fits ? rowBarGutter : 0.f);
    float const numW = std::ceil(textWidth(context, "888888")) + 2;
    float const controlsW = sideList == SideList::Wanted ? 11.f : sideList == SideList::Make ? 24.f : 0.f;
    float const controlsX = rowsEnd - controlsW;
    float const needX = controlsX - (controlsW > 0 ? 3 : 0) - numW, heldX = needX - 4 - numW;
    // A band too narrow for both columns keeps the needed one; held is in the tip.
    bool const heldShown = heldX >= panel.left + 14;
    auto countBox = [&](float y) { return ListBox{needX - 2, y + 1, numW + 3, 10}; };
    {
        int const hit = panel.hitRow(px, py) - (headed ? 1 : 0);
        int const under = hit < 0 ? -1 : first + hit;
        bool const onBox = sideList == SideList::Wanted && under >= 0 && under < total && rows[static_cast<size_t>(under)].source.empty()
            && !rows[static_cast<size_t>(under)].done && countBox(rowAt(hit)).has(px, py);
        if (frame.sideTurns && panel.has(px, py) && !onBox) first += frame.sideTurns;
    }
    {
        first = std::clamp(first, 0, std::max(0, total - fits));
        bool onBar = pressed && leftPress;
        listScrollbar(context, panel.left + panel.width - 3, rowAt(0), static_cast<float>(fits) * SideLayout::row, total, fits, first, px, py, onBar,
                      frame.held);
        if (pressed && leftPress && !onBar) pressed = false;
    }
    if (headed) {
        float const y = panel.rowY(0);
        if (heldShown) labelScaled(context, heldX - 12, y + 3, numW + 12, translated(sideList == SideList::Make ? "recipes.col.now" : "recipes.col.held"), .75f,
                    palette::faint, Align::Right, false);
        labelScaled(context, needX - 6, y + 3, numW + 6, translated("recipes.col.need"), .75f, palette::faint, Align::Right, false);
    }
    // A count cut to fit its column shows whole under the pointer.
    auto number = [&](float x, float y, Count count, Rgb color) {
        std::string text = std::to_string(count);
        if (textWidth(context, text) > numW && ListBox{x, y, numW, 12}.has(px, py)) late.tip = wholeCountText(static_cast<std::uint64_t>(count)), late.tipLines.clear();
        label(context, x, y + 2, numW, std::move(text), color, Align::Right);
    };
    int const drawn = std::min(fits, total - first);
    for (int i = 0; i < drawn; ++i) {
        auto const& row = rows[static_cast<size_t>(first + i)];
        float const y = rowAt(i), end = rowsEnd;
        (void)end;
        ListBox const whole{panel.left, y, panel.width, SideLayout::row};
        if (!row.key.empty())
            if (auto const* stack = schematic::items::iconStack(schematic::items::shown(row.key).icon))
                late.icons.push_back({stack, panel.left + 3, y + 1, smallIcon, 0, row.faint ? .45f : 1.f});
        if (row.group) late.marks.push_back({panel.left + 3, y + 1, smallIcon});
        float nameEnd = heldShown ? heldX - 3 : needX - 3;
        bool const hot = whole.has(px, py);
        if (hot) fill(context, panel.left + 1, y, rowsEnd - panel.left - 1, SideLayout::row, palette::white, .08f);
        Rgb const ink = row.faint ? palette::faint : palette::text;
        if (row.done) {
            // Held: a small check where the needed count would be.
            drawCheckMark(context, needX + numW - 7, y + 3, palette::faint);
        } else {
            if (heldShown) number(heldX, y, row.held, ink);
            if (sideList == SideList::Wanted && row.source.empty()) {
                // The player's own count, edited where it stands as in the window: a press types it, the wheel steps it.
                ListBox const box = countBox(y);
                auto const* editing = countEdit::editing(row.key, countEdit::Where::Panel);
                drawFieldBox(context, box.x, box.y, box.w, box.h, editing != nullptr);
                if (editing) {
                    drawEditField(context, needX, y, 12, numW + 3, *editing, Align::Right); // room = width - 3: the count's right edge
                    if (leftPress && took(box)) placeCaret(context, countEdit::typed, px, py);
                } else {
                    label(context, needX, y + 1 + boxTextInset(), numW, std::to_string(row.count), row.count == 0 ? palette::faint : palette::text, Align::Right);
                    if (leftPress && took(box)) countEdit::begin(row.key, row.count, countEdit::Where::Panel);
                    else if (frame.sideTurns && box.has(px, py)) state::stepWanted(row.key, frame.sideTurns < 0, frame.shift, false);
                }
                ListBox const x{controlsX, y + 1, 11, 10};
                drawButton(context, x.x, x.y, x.w, x.h, "x", x.has(px, py));
                if (leftPress && took(x)) state::removeWanted(row.key);
            } else number(needX, y, sideList == SideList::Wanted ? row.count : row.need, ink);
            // Taken out of the stores first (L-385): their chest before the counts; the panel has no headings to say so.
            if (row.stored) drawStoreMark(context, heldX - 9, y + 3, palette::dim), nameEnd = heldX - 12;
            // A step to make: laying it in and making it, always in their column.
            if (sideList == SideList::Make && plan && row.step) {
                auto pressedOn = [&](float bx, float by, float bw, float bh) { return leftPress && took(ListBox{bx, by, bw, bh}); };
                // An open window would cover the grid: laid in, it steps aside, as the recipe view does.
                if (planView::stepButtons(context, late, frame.placer, plan->table->index, row.line, controlsX, y + .5f, px, py, pressedOn, frame.ctrl,
                                          frame.shift))
                    late.placed = true;
            }
        }
        if (panel.wide()) label(context, panel.left + 16, y + 2, nameEnd - panel.left - 16, row.name, row.faint ? palette::faint : palette::dim);
        if (whole.has(px, py)) {
            if (hoverItem.empty()) hoverItem = row.key;
            // The tips are the window's (L-387). A control or a cut count under the pointer has its own tip.
            if (!late.tip.empty()) {
            } else if (sideList == SideList::Make && plan && !row.done) {
                planView::stepTip(late, *plan, row.line, row.held, row.stored);
            } else if (row.gather && plan) {
                planView::gatherTip(late, *plan, row.gatherLine, row.loose ? row.key : std::string());
            } else if (sideList == SideList::Wanted) {
                auto const* shares = plan && row.entry >= 0 ? state::shares(player) : nullptr;
                if (shares && static_cast<size_t>(row.entry) < shares->entries.size()) {
                    auto const e = static_cast<size_t>(row.entry);
                    bool const above = e < shares->targets.size() && shares->targets[e].item >= 0 && shares->targets[e].sharedAbove();
                    planView::wantedTip(late, shares->entries[e], row.held, above);
                } else planView::wantedTip(late, {row.key, row.count, row.source}, row.held, false);
            } else if (row.done) late.tip = row.name, late.tipLines = {translated("recipes.heldSteps")};
            else if (!panel.wide()) late.tip = row.name;
        }
        if (leftPress && took(whole)) openList();
    }
    if (!total)
        label(context, panel.left + 4, panel.rowY(0) + 2, panel.width - 8,
              translated(sideList == SideList::Wanted || (plan && sideList == SideList::Make) ? "recipes.nothingWanted"
                         : plan ? "schematic.raw.nothing" : "schematic.raw.reading"), palette::faint);
}
void storeButton(Frame& frame, ListBox box, placesGame::Open const& open) {
    using namespace lamium::ui;
    auto& context = frame.context;
    float const px = frame.px, py = frame.py;
    bool const leftPress = frame.leftPress;
    auto& late = frame.late;
    auto took = [&](ListBox const& box) { return frame.took(box); };
    state::seePlace(open.ender, open.spot, open.pair, open.block, open.items);
    // On makes the open container a store (off takes it out again); the ender chest, which is noted anyway, is
    // switched in or out of the count. A picture button on the game window's title line (L-382), green when on.
    int const index = open.ender ? state::places().ender() : state::places().store(open.spot);
    bool const on = index >= 0 && (!open.ender || state::places().list[static_cast<size_t>(index)].counted);
    drawWindowPictureButton(context, box.x, box.y, box.w, Picture::Store, box.has(px, py), on);
    if (box.has(px, py)) late.tip = translated("recipes.place.countState", translated(on ? "on" : "off")), late.tipGame = true;
    if (leftPress && took(box)) {
        if (open.ender) { if (index >= 0) state::setPlaceCounted(index, !on); }
        else if (index < 0) state::markStore(open.spot, open.pair, open.block, open.items);
        else state::unmarkStore(index);
    }
}
}
