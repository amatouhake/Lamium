#include "features/recipes/PlanViewDraw.h"
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
int scroll[3]{};
int expanded = -1; // the group whose members are shown, to pick one
constexpr float rowHeight = 12, headTop = 29, rowsTop = 41, iconScale = .625f, stepperWidth = 50;
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
void draw(MinecraftUIRenderContext& context, view::Input const& input, view::Late& late, LocalPlayer& player, bool shift, std::string& showItem) {
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
    fill(context, l.left, l.top, l.width, l.height, palette::panel);
    panel(context, l.left, l.top, l.width, l.height, opacity::floating);
    label(context, l.left + 6, l.top + 5, l.width - 12, translated("recipes.wanted"));
    float x = l.left + 5;
    Tab const tabs[]{Tab::Wanted, Tab::Gather, Tab::Make};
    char const* const names[]{"recipes.wanted", "recipes.gather", "recipes.make"};
    for (int i = 0; i < 3; ++i) {
        std::string name = translated(names[i]);
        float const w = textWidth(context, name) + 10;
        drawButton(context, x, l.top + 16, w, 11, std::move(name), over(x, l.top + 16, w, 11), tab == tabs[i] ? ButtonKind::Primary : ButtonKind::Plain);
        if (took(x, l.top + 16, w, 11)) tab = tabs[i], expanded = -1;
        x += w + 2;
    }
    auto const* plan = state::plan(player);
    float const left = l.left + 6, right = l.left + l.width - 6, top = l.top + rowsTop;
    int const fits = std::max(1, static_cast<int>((l.height - rowsTop - RecipeViewLayout::foot) / rowHeight));
    auto note = [&](std::string text, Rgb color = palette::dim) { label(context, left, top + 2, right - left, std::move(text), color); };
    if (!plan) { note(translated("schematic.raw.reading")); return; }
    auto const& index = plan->table->index;
    Item const itemCount = static_cast<Item>(index.items.size());
    auto const turn = static_cast<size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1500);
    // A row's start: the icon and the name, which a press turns into the item's recipes. `id` past the items is a group.
    auto lead = [&](float y, Item id, float nameEnd) {
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
        label(context, left + 13, y + 2, nameEnd - left - 15, std::move(name), hot ? palette::text : palette::dim);
        if (hot) late.hovered = key;
        if (id >= 0 && id < itemCount && took(left, y, nameEnd - left, rowHeight)) showItem = key;
    };
    auto number = [&](float cellRight, float y, std::string text, Rgb color = palette::dim) {
        label(context, cellRight - 34, y + 2, 34, std::move(text), color, Align::Right);
    };
    auto heading = [&](float cellRight, char const* key) {
        label(context, cellRight - 40, l.top + headTop + 1, 40, translated(key), palette::faint, Align::Right);
    };
    // A small button at a row's right end; returns its left edge.
    auto rowButton = [&](float end, float y, std::string text, auto&& act) {
        float const w = textWidth(context, text) + 8;
        drawButton(context, end - w, y + 1, w, 10, std::move(text), over(end - w, y + 1, w, 10));
        if (took(end - w, y + 1, w, 10)) act();
        return end - w - 3;
    };
    int& offset = scroll[static_cast<int>(tab)];
    int wheel = input.wheel;
    if (tab == Tab::Wanted) {
        auto const targets = state::wanted().targets; // a copy: the controls below change the list
        if (targets.empty()) { paragraph(context, left, top + 2, right - left, translated("recipes.wantedHow"), 3, palette::dim); return; }
        offset = std::clamp(offset, 0, std::max(0, static_cast<int>(targets.size()) - fits));
        for (int row = 0; row < fits && offset + row < static_cast<int>(targets.size()); ++row) {
            auto const& [key, amount] = targets[static_cast<size_t>(offset + row)];
            float const y = top + static_cast<float>(row) * rowHeight;
            float end = rowButton(right, y, translated("recipes.remove"), [&] { state::removeWanted(key); });
            float const sx = end - stepperWidth;
            drawStepper(context, sx, y + 1, stepperWidth, 10, true, std::to_string(amount), {.lowEnabled = amount > 1, .pointerX = px, .pointerY = py});
            Count const step = shift ? 16 : 1;
            if (took(sx, y + 1, 11, 10)) state::setWanted(key, amount > step ? amount - step : 1);
            else if (took(sx + stepperWidth - 11, y + 1, 11, 10)) state::setWanted(key, amount + step);
            else if (wheel && over(sx, y + 1, stepperWidth, 10)) {
                // The wheel over a stepper steps it: up is more.
                state::setWanted(key, wheel < 0 ? amount + step : amount > step ? amount - step : 1);
                wheel = 0;
            }
            Item const id = index.findKey(key);
            if (id >= 0) lead(y, id, sx - 4);
            else {
                // Not in this world's table: shown by what the game calls it, kept as it is.
                auto const& shown = schematic::items::shown(key);
                if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, left, y + 1, iconScale});
                label(context, left + 13, y + 2, sx - left - 19, shown.name, palette::dim);
            }
        }
        offset = std::clamp(offset + wheel, 0, std::max(0, static_cast<int>(targets.size()) - fits));
        return;
    }
    if (tab == Tab::Gather) {
        heading(right, "schematic.raw.left");
        heading(right - 40, "recipes.col.have");
        heading(right - 80, "recipes.col.need");
        // What is still to gather, and what the player marked as gathered even when nothing is left of it.
        struct Line { GatherLine line; bool stopped = false; };
        std::vector<Line> lines;
        for (auto const& line : plan->view.gather) {
            bool const stopped = line.id < itemCount && plan->request.stops.contains(line.id);
            if (line.left > 0 || stopped) lines.push_back({line, stopped});
        }
        if (lines.empty()) { note(translated("schematic.raw.nothing"), palette::accent); return; }
        // An open group takes rows of its own for its members, so rows are counted in units.
        offset = std::clamp(offset + wheel, 0, std::max(0, static_cast<int>(lines.size()) - 1));
        float y = top;
        float const bottom = l.top + l.height - RecipeViewLayout::foot;
        for (size_t at = static_cast<size_t>(offset); at < lines.size() && y + rowHeight <= bottom; ++at) {
            auto const& [line, stopped] = lines[at];
            number(right, y, std::to_string(line.left), palette::text);
            number(right - 40, y, std::to_string(line.held));
            number(right - 80, y, std::to_string(line.need));
            float end = right - 120;
            if (line.id >= itemCount) {
                int const group = line.id - itemCount;
                // The whole row opens the members to pick one from.
                if (over(left, y, right - left, rowHeight)) fill(context, l.left + 2, y, l.width - 4, rowHeight, palette::white, .06f);
                if (took(left, y, right - left, rowHeight)) expanded = expanded == group ? -1 : group;
                lead(y, line.id, end);
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
            if (stopped) end = rowButton(end, y, translated("recipes.toMake"), [&] { state::setStop(index.items[static_cast<size_t>(line.id)], false); });
            lead(y, line.id, end);
            y += rowHeight;
        }
        return;
    }
    // The numbers stand left of the row's "gather" button, which keeps the right end.
    float const columns = right - textWidth(context, translated("recipes.toGather")) - 11;
    heading(columns, "schematic.raw.spare");
    heading(columns - 40, "schematic.raw.made");
    heading(columns - 80, "schematic.raw.crafts");
    auto const& steps = plan->view.steps;
    if (steps.empty()) { note(translated("recipes.nothingWanted")); return; }
    offset = std::clamp(offset + wheel, 0, std::max(0, static_cast<int>(steps.size()) - fits));
    for (int row = 0; row < fits && offset + row < static_cast<int>(steps.size()); ++row) {
        auto const& step = steps[static_cast<size_t>(offset + row)];
        float const y = top + static_cast<float>(row) * rowHeight;
        auto const& key = index.items[static_cast<size_t>(step.item)];
        rowButton(right, y, translated("recipes.toGather"), [&] { state::setStop(key, true); });
        number(columns, y, step.spare ? std::to_string(step.spare) : std::string("-"));
        number(columns - 40, y, std::to_string(step.made));
        number(columns - 80, y, std::to_string(step.crafts), palette::text);
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
        lead(y, step.item, end);
    }
}
}
