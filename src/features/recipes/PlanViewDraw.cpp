#include "features/recipes/PlanViewDraw.h"
#include "app/Runtime.h"
#include "features/recipes/CountEdit.h"
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

namespace lamium::recipes::planView {
namespace {
bool opened = false;
Tab tab = Tab::Wanted;
int scroll[4]{};
int expanded = -1; // the group whose members are shown, to pick one
std::pair<Item, int> expandedMake{-1, -1}; // a row of what to make and its group input opened to pick a member
constexpr float rowHeight = 12, headTop = 4, rowsTop = 16, iconScale = .625f, stepperWidth = 50;
}
bool isOpen() { return opened; }
void open(Tab which) {
    opened = true;
    tab = which;
}
void close() {
    opened = false;
    expanded = -1;
    expandedMake = {-1, -1};
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
    float const coverLeft = input.coverRight > input.coverLeft ? std::min(l.left, input.coverLeft) : l.left;
    float const coverRight = input.coverRight > input.coverLeft ? std::max(l.left + l.width, input.coverRight) : l.left + l.width;
    windowPanel(context, coverLeft, l.top, coverRight - coverLeft, l.height);
    OnWindow const onWindowScope;
    // The lists are the game's tabs on the window's top edge; they name the window, which needs no title.
    float x = l.left + 5;
    Tab const tabs[]{Tab::Wanted, Tab::Gather, Tab::Make, Tab::Places};
    char const* const names[]{"recipes.wanted", "recipes.gather", "recipes.make", "recipes.places"};
    for (int i = 0; i < 4; ++i) {
        std::string name = translated(names[i]);
        float const w = textWidth(context, name) + 14;
        windowTab(context, x, l.top, w, std::move(name), tab == tabs[i], over(x, l.top - windowTabRise, w - 1, windowTabRise));
        if (took(x, l.top - windowTabRise, w - 1, windowTabRise)) tab = tabs[i], expanded = -1, expandedMake = {-1, -1};
        x += w - 1; // neighbours share their edge
    }
    auto const* plan = state::plan(player);
    float const left = l.left + 6, right = l.left + l.width - 6;
    float top = l.top + rowsTop;
    int fits = std::max(1, static_cast<int>((l.height - rowsTop - RecipeViewLayout::foot) / rowHeight));
    // On the wanted tab, above the list: whether a schematic's materials still to place stand among the
    // wanted items. The same switch as on the schematic's Materials tab, shown while Schematics is on.
    if (tab == Tab::Wanted && Runtime::instance().preferences().schematic.enabled) {
        bool const on = Runtime::instance().preferences().schematic.planMaterials;
        if (over(left, top, right - left, rowHeight)) fill(context, left - 2, top, right - left + 4, rowHeight, palette::shade, .08f);
        windowLabel(context, left, top + 2, right - left - switchWidth - 4, translated("recipes.plan.schematic"));
        toggleSwitch(context, right - switchWidth, top + 1.5f, on);
        if (took(left, top, right - left, rowHeight)) {
            auto value = Runtime::instance().preferences();
            value.schematic.planMaterials = !on;
            Runtime::instance().save(value);
        }
        top += rowHeight + 2;
        fits = std::max(1, fits - 1);
    }
    // A place by what it is: the ender chest, or a store's block and where it stands.
    auto placeName = [&](Place const& place) {
        std::string name = schematic::items::shown(place.block).name;
        return place.ender ? name : translated("recipes.place.at", name, place.at.x, place.at.y, place.at.z);
    };
    auto note = [&](std::string text) { windowLabel(context, left, top + 2, right - left, std::move(text), true); };
    // Every list of the window scrolls with the shared bar, in the margin right of the rows.
    auto bar = [&](int count, int& first) {
        listScrollbar(context, right + 2, top, static_cast<float>(fits) * rowHeight, count, fits, first, px, py, pressed, input.held);
    };
    if (!plan) { note(translated("schematic.raw.reading")); return; }
    auto const& index = plan->table->index;
    Item const itemCount = static_cast<Item>(index.items.size());
    auto const turn = static_cast<size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1500);
    // A row's start: the icon and the name, which a press turns into the item's recipes. `id` past the items is a group.
    // Under the pointer the row says its whole name, which the column may cut, and `amount` in chests and stacks.
    auto lead = [&](float y, Item id, float nameEnd, Count amount = 0) {
        std::string key, name;
        if (id >= itemCount) {
            auto const& members = index.groups[static_cast<size_t>(id - itemCount)].members;
            if (!members.empty()) key = index.items[static_cast<size_t>(members[turn % members.size()])];
            name = groupName(index, id - itemCount);
        } else if (id >= 0) {
            key = index.items[static_cast<size_t>(id)];
            name = schematic::items::shown(key).name;
        }
        if (!key.empty())
            if (auto const* stack = schematic::items::iconStack(schematic::items::shown(key).icon)) late.icons.push_back({stack, left, y + 1, iconScale});
        bool const hot = id >= 0 && id < itemCount && over(left, y, nameEnd - left, rowHeight);
        if (hot) fill(context, left - 2, y, nameEnd - left + 2, rowHeight, palette::shade, .1f);
        if (late.tip.empty() && !name.empty() && over(left, y, right - left, rowHeight)) {
            late.tip = name;
            if (amount) late.tipLines.push_back(amountText(amount, key.empty() ? 64 : schematic::items::shown(key).stack));
        }
        windowLabel(context, left + 13, y + 2, nameEnd - left - 15, std::move(name));
        if (hot) late.hovered = key;
        if (id >= 0 && id < itemCount && took(left, y, nameEnd - left, rowHeight)) showItem = key;
    };
    auto number = [&](float cellRight, float y, std::string text, bool faint = true) {
        windowLabel(context, cellRight - 34, y + 2, 34, std::move(text), faint, Align::Right);
    };
    // A small button at a row's right end; returns its left edge.
    auto rowButton = [&](float end, float y, std::string text, auto&& act) {
        float const w = textWidth(context, text) + 8;
        drawButton(context, end - w, y + 1, w, 10, std::move(text), over(end - w, y + 1, w, 10));
        if (took(end - w, y + 1, w, 10)) act();
        return end - w - 3;
    };
    if (tab == Tab::Places) {
        // What counts as held: what is carried, then the ender chest and the stores as they were when last
        // seen open (what a shulker box or a bundle among them holds counts with it). Each row is a name and its switch, the switches in one
        // column; a store can be taken out as well.
        auto const all = state::places();
        auto const& places = all.list; // of a copy: the controls below change the list
        std::string const removeText = translated("recipes.remove");
        float const sx = right - (textWidth(context, removeText) + 8) - 4 - switchWidth;
        int const rows = static_cast<int>(places.size()) + 1;
        int& first = scroll[static_cast<int>(tab)];
        first = std::clamp(first + input.wheel, 0, std::max(0, rows - fits));
        bar(rows, first);
        for (int row = 0; row < fits && first + row < rows; ++row) {
            float const y = top + static_cast<float>(row) * rowHeight;
            int const at = first + row - 1;
            std::string name, icon, tip;
            bool on = false, faint = false;
            if (at < 0) name = translated("recipes.place.carried"), tip = translated("recipes.place.carriedTip"), icon = "minecraft:player_head", on = all.carried;
            else {
                auto const& place = places[static_cast<size_t>(at)];
                name = placeName(place), icon = place.block, on = place.counted;
                if (!place.ender) rowButton(right, y, removeText, [&] { state::unmarkStore(at); });
            }
            toggleSwitch(context, sx, y + 1.5f, on);
            if (took(sx - 2, y, switchWidth + 4, rowHeight)) {
                if (at < 0) state::setCarriedCounted(!all.carried);
                else state::setPlaceCounted(at, !on);
            }
            float const from = left;
            if (auto const* stack = schematic::items::iconStack(schematic::items::shown(icon).icon)) late.icons.push_back({stack, from, y + 1, iconScale});
            if (late.tip.empty() && over(left, y, sx - left - 4, rowHeight)) {
                late.tip = name;
                if (!tip.empty()) late.tipLines.push_back(std::move(tip));
            }
            windowLabel(context, from + 13, y + 2, sx - from - 17, std::move(name), faint || !on);
        }
        if (places.empty() && fits > 2) paragraph(context, left, top + rowHeight + 4, right - left, translated("recipes.place.how"), 3, palette::vanilla::windowFaint);
        return;
    }
    int& offset = scroll[static_cast<int>(tab)];
    int wheel = input.wheel;
    if (tab == Tab::Wanted) {
        // A copy: the controls below change the list. What features handed over follows, named by its
        // source where the player's own rows have their controls.
        struct Target { std::string key; Count amount = 0; std::string source; };
        std::vector<Target> targets;
        for (auto const& [key, amount] : state::wanted().targets) targets.push_back({key, amount, {}});
        for (auto const& given : state::handed()) targets.push_back({given.key, given.count, given.source});
        if (targets.empty()) { paragraph(context, left, top + 2, right - left, translated("recipes.wantedHow"), 3, palette::vanilla::windowInk); return; }
        // The wheel scrolls the list only after the rows: a stepper under the pointer takes it for itself.
        offset = std::clamp(offset, 0, std::max(0, static_cast<int>(targets.size()) - fits));
        bar(static_cast<int>(targets.size()), offset);
        for (int row = 0; row < fits && offset + row < static_cast<int>(targets.size()); ++row) {
            auto const& [key, amount, source] = targets[static_cast<size_t>(offset + row)];
            float const y = top + static_cast<float>(row) * rowHeight;
            float sx = right;
            if (source.empty() && over(left, y, right - left, rowHeight)) late.wantedRow = key;
            if (!source.empty()) {
                std::string from = translated("recipes.from." + source);
                float const w = textWidth(context, from) + 4;
                windowLabel(context, right - w, y + 2, w, std::move(from), true, Align::Right);
                number(right - w - 2, y, std::to_string(amount), false);
                sx = right - w - 38;
            } else {
            float end = rowButton(right, y, translated("recipes.remove"), [&] { state::removeWanted(key); });
            // The stepper: its ends step (minus at one takes the item out), its number is typed after a press.
            sx = end - stepperWidth;
            drawStepper(context, sx, y + 1, stepperWidth, 10, true, std::to_string(amount),
                        {.editing = countEdit::editing(key), .pointerX = px, .pointerY = py, .backed = true, .dimValue = amount == 0});
            if (took(sx, y + 1, 11, 10)) state::stepWanted(key, false, shift, true);
            else if (took(sx + stepperWidth - 11, y + 1, 11, 10)) state::stepWanted(key, true, shift, true);
            else if (took(sx, y + 1, stepperWidth, 10)) countEdit::begin(key, amount);
            else if (wheel && over(sx, y + 1, stepperWidth, 10)) {
                // The wheel over a stepper steps it: up is more.
                state::stepWanted(key, wheel < 0, shift, false);
                wheel = 0;
            }
            }
            Item const id = index.findKey(key);
            if (id >= 0) lead(y, id, sx - 4, amount);
            else {
                // Not in this world's table: shown by what the game calls it, kept as it is.
                auto const& shown = schematic::items::shown(key);
                if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, left, y + 1, iconScale});
                windowLabel(context, left + 13, y + 2, sx - left - 19, shown.name, true);
            }
        }
        offset = std::clamp(offset + wheel, 0, std::max(0, static_cast<int>(targets.size()) - fits));
        return;
    }
    float const bottom = l.top + l.height - RecipeViewLayout::foot;
    // A group's members as cells under its row, to pick one (or "any" again); a row's height in all.
    auto members = [&](float y, int group, auto&& done) {
        auto const& list = index.groups[static_cast<size_t>(group)].members;
        int const across = std::max(1, static_cast<int>((right - left - 12) / 18));
        for (size_t m = 0; m < list.size(); ++m) {
            float const cx = left + 12 + static_cast<float>(m % static_cast<size_t>(across)) * 18, cy = y + static_cast<float>(m / static_cast<size_t>(across)) * 18;
            if (cy + 18 > bottom) break;
            auto const& key = index.items[static_cast<size_t>(list[m])];
            auto const& shown = schematic::items::shown(key);
            bool const hot = over(cx, cy, 18, 18);
            windowSlot(context, cx, cy, hot);
            if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, cx + 1, cy + 1});
            auto held = plan->held.find(list[m]);
            if (held != plan->held.end() && held->second) late.counts.push_back({cx, cy, std::to_string(held->second)});
            if (hot) late.tip = shown.name, late.hovered = key;
            if (took(cx, cy, 18, 18)) state::setPick(groupKey(index, group), key), done();
        }
        float const rows = static_cast<float>((list.size() + static_cast<size_t>(across) - 1) / static_cast<size_t>(across));
        if (plan->request.picks.contains(group)) {
            float const by = y + rows * 18 + 1;
            if (by + 11 <= bottom) rowButton(right, by - 1, translated("recipes.backToAny"), [&] { state::setPick(groupKey(index, group), {}); done(); });
            return rows * 18 + 14;
        }
        return rows * 18 + 2;
    };
    if (tab == Tab::Gather) {
        // One line of the whole: what is left of all there is to gather, with a bar. Rows still to gather come
        // first, most left first; done ones (held whole, or marked gathered) follow, faint (L-367).
        struct Row { GatherLine line; std::string key; bool stopped = false, done = false; };
        std::vector<Row> rows, finished;
        for (auto const& line : plan->view.gather) {
            bool const stopped = line.id < itemCount && plan->request.stops.contains(line.id);
            if (line.left > 0) rows.push_back({line, {}, stopped});
            else if (stopped) finished.push_back({line, {}, true, true});
        }
        // What the recipe table does not know is gathered as it is, after what the recipes ask for.
        for (auto const& line : plan->loose) {
            Row row{{-1, line.need, line.held, line.left}, line.key, false, line.left <= 0};
            (row.done ? finished : rows).push_back(row);
        }
        for (auto const& line : plan->view.done) finished.push_back({line, {}, false, true});
        rows.insert(rows.end(), finished.begin(), finished.end());
        if (rows.empty()) { note(translated("schematic.raw.nothing")); return; }
        std::vector<GatherLine> all;
        for (auto const& row : rows) all.push_back(row.line);
        auto const total = gatherTotal(all);
        {
            float const barW = 70, ty = l.top + headTop;
            windowLabel(context, left, ty + 1, right - left - barW - 6, translated("recipes.gather.total", total.left, total.need, total.percent()));
            progressBar(context, right - barW, ty + 4, barW, 4, total.fraction());
        }
        int const count = static_cast<int>(rows.size());
        offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
        bar(count, offset);
        // Each row: the icon and name, "have / need" and a short bar at its right end.
        constexpr float barW = 26;
        float const numbersEnd = right - barW - 4, numbersW = 46;
        float y = top;
        for (int at = offset; at < count && y + rowHeight <= bottom; ++at) {
            auto const& row = rows[static_cast<size_t>(at)];
            auto const& line = row.line;
            size_t const iconsBefore = late.icons.size();
            std::string const amounts = std::format("{} / {}", line.held, line.need);
            windowLabel(context, numbersEnd - numbersW, y + 2, numbersW, amounts, row.done, Align::Right);
            progressBar(context, right - barW, y + 4, barW, 3, line.need > 0 ? static_cast<float>(line.held) / static_cast<float>(line.need) : 1.f);
            // A count that includes what a place held when last seen is starred, and says under the pointer where
            // the items are.
            if (line.id >= 0 && line.id < itemCount) {
                auto const stored = plan->stored.find(line.id);
                if (stored != plan->stored.end() && stored->second) {
                    windowLabel(context, numbersEnd + 1, y + 2, 4, "*", true);
                    if (over(numbersEnd - numbersW, y, numbersW + 5, rowHeight)) {
                        auto const carried = plan->held.find(line.id);
                        late.tip = translated("recipes.place.tipCarried", carried == plan->held.end() ? Count{0} : carried->second);
                        for (auto const& place : state::places().list) {
                            if (!place.counted) continue;
                            Count here = 0;
                            for (auto const& item : place.items) if (index.find(item.name, item.aux) == line.id) here += item.count;
                            if (here) late.tipLines.push_back(translated("recipes.place.tipPlace", placeName(place), here));
                        }
                    }
                }
            }
            float nameEnd = numbersEnd - numbersW - 4;
            if (row.stopped) nameEnd = rowButton(nameEnd, y, translated("recipes.toMake"), [&] { state::setStop(index.items[static_cast<size_t>(line.id)], false); });
            if (!row.key.empty()) {
                // Not in the recipe table: shown by what the game calls it.
                auto const& shown = schematic::items::shown(row.key);
                if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, left, y + 1, iconScale});
                if (late.tip.empty() && over(left, y, nameEnd - left, rowHeight)) late.tip = shown.name, late.tipLines.push_back(amountText(line.left, shown.stack)), late.hovered = row.key;
                windowLabel(context, left + 13, y + 2, nameEnd - left - 15, shown.name, row.done);
            } else if (line.id >= itemCount) {
                int const group = line.id - itemCount;
                // The whole row opens the members to pick one from.
                if (over(left, y, nameEnd - left, rowHeight)) fill(context, l.left + 2, y, l.width - 4, rowHeight, palette::shade, .1f);
                if (took(left, y, nameEnd - left, rowHeight)) expanded = expanded == group ? -1 : group;
                lead(y, line.id, nameEnd, line.left);
                y += rowHeight;
                if (expanded == group) y += members(y, group, [&] { expanded = -1; });
                continue;
            } else lead(y, line.id, nameEnd, line.left);
            if (row.done)
                for (size_t k = iconsBefore; k < late.icons.size(); ++k) late.icons[k].alpha = .45f;
            y += rowHeight;
        }
        return;
    }
    // What to make, in the order to make it: each row its inputs with counts, an arrow, the product with the
    // count made, the station, the recipe it is made by (stepped through in place where there are several) and
    // "gather" at its right end. A press on an input any of several items serves opens them to pick one.
    auto const& steps = plan->view.steps;
    if (steps.empty()) { note(translated("recipes.nothingWanted")); return; }
    int const count = static_cast<int>(steps.size());
    offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
    bar(count, offset);
    float y = top;
    for (int at = offset; at < count && y + rowHeight <= bottom; ++at) {
        auto const& step = steps[static_cast<size_t>(at)];
        auto const& key = index.items[static_cast<size_t>(step.item)];
        auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
        float end = rowButton(right, y, translated("recipes.toGather"), [&] { state::setStop(key, true); });
        // The recipe it is made by: previous / "1 / 3" / next, only where there are several.
        auto const options = recipeOptions(index, step.item);
        if (options.size() > 1) {
            int const now = static_cast<int>(std::ranges::find(options, step.recipe) - options.begin());
            auto const pager = drawPager(context, end, y + .5f, now, static_cast<int>(options.size()), px, py);
            auto choose = [&](int to) { state::setChoice(key, choiceKey(index.recipes[static_cast<size_t>(options[static_cast<size_t>(to)])])); };
            if (pager.prevOn && took(pager.prevX, pager.y, Pager::size, Pager::size)) choose(now - 1);
            else if (pager.nextOn && took(pager.nextX, pager.y, Pager::size, Pager::size)) choose(now + 1);
            end = pager.prevX - 3;
        }
        // Where it is made, as that station's item.
        if (auto const station = stationItem(recipe.station); !station.empty()) {
            auto const& shown = schematic::items::shown(std::string(station));
            if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, end - 11, y + 1, iconScale});
            if (over(end - 11, y, 11, rowHeight)) late.tip = shown.name;
            end -= 14;
        }
        // The product and how many the crafts make, right-aligned against the station.
        auto const& product = schematic::items::shown(key);
        std::string const made = std::format("{}", step.made);
        float const madeW = textWidth(context, made), productX = end - madeW - 13;
        if (auto const* stack = schematic::items::iconStack(product.icon)) late.icons.push_back({stack, productX, y + 1, iconScale});
        windowLabel(context, productX + 11, y + 2, madeW + 2, made);
        bool const onProduct = over(productX, y, madeW + 13, rowHeight);
        if (onProduct) late.tip = product.name, late.tipLines = stepInputLines(index, plan->request, step), late.hovered = key;
        if (took(productX, y, madeW + 13, rowHeight)) showItem = key;
        float const arrowX = productX - 10;
        changeArrow(context, arrowX, y + 3.5f, 1, palette::vanilla::windowInk);
        // The inputs from the left, as many as there is room for; the rest says itself under the pointer.
        float x = left;
        int openGroup = -1;
        auto const& uses = recipe.uses;
        for (size_t u = 0; u < uses.size(); ++u) {
            auto const& part = uses[u];
            Item const item = partItem(index, plan->request, part);
            std::string const amount = std::to_string(step.crafts * part.count);
            float const w = 11 + textWidth(context, amount) + 4;
            if (x + w > arrowX - 2) {
                windowLabel(context, x, y + 2, arrowX - 2 - x, "...", true);
                if (over(x, y, arrowX - x, rowHeight)) late.tip = product.name, late.tipLines = stepInputLines(index, plan->request, step);
                break;
            }
            std::string iconKey, name;
            if (item >= itemCount) {
                auto const& list = index.groups[static_cast<size_t>(item - itemCount)].members;
                if (!list.empty()) iconKey = index.items[static_cast<size_t>(list[turn % list.size()])];
                name = groupName(index, item - itemCount);
            } else if (item >= 0) iconKey = index.items[static_cast<size_t>(item)], name = schematic::items::shown(iconKey).name;
            if (!iconKey.empty())
                if (auto const* stack = schematic::items::iconStack(schematic::items::shown(iconKey).icon)) late.icons.push_back({stack, x, y + 1, iconScale});
            windowLabel(context, x + 11, y + 2, w - 11, amount);
            bool const hot = over(x, y, w, rowHeight);
            if (part.group) {
                // Any of several: a press opens the members under the row.
                if (hot) fill(context, x - 1, y, w, rowHeight, palette::shade, .1f), late.tip = name;
                if (took(x, y, w, rowHeight)) openGroup = part.id;
                if (expandedMake == std::pair<Item, int>{step.item, part.id}) fill(context, x - 1, y + rowHeight - 1, w, 1, palette::accentDeep);
            } else if (hot) late.tip = name, late.hovered = iconKey;
            x += w;
        }
        if (openGroup >= 0) expandedMake = expandedMake == std::pair<Item, int>{step.item, openGroup} ? std::pair<Item, int>{-1, -1} : std::pair<Item, int>{step.item, openGroup};
        y += rowHeight;
        if (expandedMake.first == step.item) y += members(y, expandedMake.second, [&] { expandedMake = {-1, -1}; });
    }
    if (!showItem.empty()) showChoice = false;
}
}
