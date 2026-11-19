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
        if (took(x, l.top - windowTabRise, w - 1, windowTabRise)) tab = tabs[i], expanded = -1;
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
    auto heading = [&](float cellRight, char const* key) {
        windowLabel(context, cellRight - 40, l.top + headTop + 1, 40, translated(key), true, Align::Right);
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
    // On both lists the numbers stand left of the row's button, which keeps the right end.
    float const columns = right - std::max(textWidth(context, translated("recipes.toGather")), textWidth(context, translated("recipes.toMake"))) - 11;
    if (tab == Tab::Gather) {
        heading(columns, "schematic.raw.left");
        heading(columns - 40, "recipes.col.have");
        heading(columns - 80, "recipes.col.need");
        // What is still to gather, and what the player marked as gathered even when nothing is left of it.
        struct Line { GatherLine line; bool stopped = false; };
        std::vector<Line> lines;
        for (auto const& line : plan->view.gather) {
            bool const stopped = line.id < itemCount && plan->request.stops.contains(line.id);
            if (line.left > 0 || stopped) lines.push_back({line, stopped});
        }
        // What the recipe table does not know is gathered as it is, after what the recipes ask for.
        std::vector<LooseLine> loose;
        for (auto const& line : plan->loose) if (line.left > 0) loose.push_back(line);
        if (lines.empty() && loose.empty()) { note(translated("schematic.raw.nothing")); return; }
        // An open group takes rows of its own for its members, so rows are counted in units.
        int const all = static_cast<int>(lines.size() + loose.size());
        offset = std::clamp(offset + wheel, 0, std::max(0, all - 1));
        bar(all, offset);
        float y = top;
        float const bottom = l.top + l.height - RecipeViewLayout::foot;
        for (size_t at = static_cast<size_t>(offset); at < lines.size() && y + rowHeight <= bottom; ++at) {
            auto const& [line, stopped] = lines[at];
            number(columns, y, std::to_string(line.left), false);
            number(columns - 40, y, std::to_string(line.held));
            // A count that includes what a place held when last seen is starred, in a gutter of its own, and
            // says under the pointer where the items are.
            if (line.id < itemCount) {
                auto const stored = plan->stored.find(line.id);
                if (stored != plan->stored.end() && stored->second) {
                    windowLabel(context, columns - 39, y + 2, 5, "*", true);
                    if (over(columns - 74, y, 40, rowHeight)) {
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
            number(columns - 80, y, std::to_string(line.need));
            float const end = columns - 118;
            if (line.id >= itemCount) {
                int const group = line.id - itemCount;
                // The whole row opens the members to pick one from.
                if (over(left, y, right - left, rowHeight)) fill(context, l.left + 2, y, l.width - 4, rowHeight, palette::shade, .1f);
                if (took(left, y, right - left, rowHeight)) expanded = expanded == group ? -1 : group;
                lead(y, line.id, end, line.left);
                y += rowHeight;
                if (expanded != group) continue;
                auto const& members = index.groups[static_cast<size_t>(group)].members;
                int const across = std::max(1, static_cast<int>((right - left - 12) / 18));
                for (size_t m = 0; m < members.size(); ++m) {
                    float const cx = left + 12 + static_cast<float>(m % static_cast<size_t>(across)) * 18, cy = y + static_cast<float>(m / static_cast<size_t>(across)) * 18;
                    if (cy + 18 > bottom) break;
                    auto const& key = index.items[static_cast<size_t>(members[m])];
                    auto const& shown = schematic::items::shown(key);
                    bool const hot = over(cx, cy, 18, 18);
                    slot(context, cx, cy, hot);
                    if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, cx + 1, cy + 1});
                    auto held = plan->held.find(members[m]);
                    if (held != plan->held.end() && held->second) late.counts.push_back({cx, cy, std::to_string(held->second)});
                    if (hot) late.tip = shown.name, late.hovered = key;
                    if (took(cx, cy, 18, 18)) state::setPick(groupKey(index, group), key), expanded = -1;
                }
                y += static_cast<float>((members.size() + static_cast<size_t>(across) - 1) / static_cast<size_t>(across)) * 18 + 2;
                continue;
            }
            if (stopped) rowButton(right, y, translated("recipes.toMake"), [&] { state::setStop(index.items[static_cast<size_t>(line.id)], false); });
            lead(y, line.id, end, line.left);
            y += rowHeight;
        }
        for (size_t at = static_cast<size_t>(offset) > lines.size() ? static_cast<size_t>(offset) - lines.size() : 0; at < loose.size() && y + rowHeight <= bottom; ++at) {
            auto const& line = loose[at];
            number(columns, y, std::to_string(line.left), false);
            number(columns - 40, y, std::to_string(line.held));
            number(columns - 80, y, std::to_string(line.need));
            auto const& shown = schematic::items::shown(line.key);
            if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, left, y + 1, iconScale});
            if (late.tip.empty() && over(left, y, right - left, rowHeight)) late.tip = shown.name, late.tipLines.push_back(amountText(line.left, shown.stack)), late.hovered = line.key;
            windowLabel(context, left + 13, y + 2, columns - 118 - left - 15, shown.name);
            y += rowHeight;
        }
        return;
    }
    heading(columns, "schematic.raw.spare");
    heading(columns - 40, "schematic.raw.made");
    heading(columns - 80, "schematic.raw.crafts");
    auto const& steps = plan->view.steps;
    if (steps.empty()) { note(translated("recipes.nothingWanted")); return; }
    offset = std::clamp(offset + wheel, 0, std::max(0, static_cast<int>(steps.size()) - fits));
    bar(static_cast<int>(steps.size()), offset);
    for (int row = 0; row < fits && offset + row < static_cast<int>(steps.size()); ++row) {
        auto const& step = steps[static_cast<size_t>(offset + row)];
        float const y = top + static_cast<float>(row) * rowHeight;
        auto const& key = index.items[static_cast<size_t>(step.item)];
        rowButton(right, y, translated("recipes.toGather"), [&] { state::setStop(key, true); });
        number(columns, y, step.spare ? std::to_string(step.spare) : std::string("-"));
        number(columns - 40, y, std::to_string(step.made));
        number(columns - 80, y, std::to_string(step.crafts), false);
        float end = columns - 118;
        // A member the player picked for a group can go back to "any".
        for (auto const& [group, member] : plan->request.picks)
            if (member == step.item) {
                end = rowButton(end, y, translated("recipes.backToAny"), [&, group = group] { state::setPick(groupKey(index, group), {}); });
                break;
            }
        // Where it is made, as that station's item.
        auto const station = stationItem(index.recipes[static_cast<size_t>(step.recipe)].station);
        if (!station.empty()) {
            auto const& shown = schematic::items::shown(std::string(station));
            if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, end - 11, y + 1, iconScale});
            if (over(end - 11, y, 11, rowHeight)) late.tip = shown.name;
            end -= 14;
        }
        // Under the pointer the row says what its crafts take; a press on the name shows the recipes to choose from.
        if (over(left, y, end - left, rowHeight)) late.tip = schematic::items::shown(key).name, late.tipLines = stepInputLines(index, plan->request, step);
        lead(y, step.item, end);
        if (!showItem.empty()) showChoice = true;
    }
}
}
