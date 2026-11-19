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
std::string openEntry; // the wanted entry whose breakdown is open (wantedEntryId)
// In that breakdown, a group opened to pick a member, from a step's input or from a material's row: its cells
// show under the row they were opened from only.
struct OpenMembers {
    std::string entry;
    int group = -1;
    bool fromStep = false;
    bool operator==(OpenMembers const&) const = default;
};
OpenMembers openMembers;
std::string scrollTo; // a wanted entry to bring into view when the wanted tab shows next
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
    openEntry.clear();
    openMembers = {};
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
    // `visible`: how many rows fit at the list's end, where rows are taller than `rowHeight`.
    auto bar = [&](int count, int& first, int visible = -1) {
        listScrollbar(context, right + 2, top, static_cast<float>(fits) * rowHeight, count, visible < 0 ? fits : visible, first, px, py, pressed, input.held);
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
    // The same height members() takes, without drawing.
    auto membersHeight = [&](int group) {
        auto const& list = index.groups[static_cast<size_t>(group)].members;
        size_t const across = static_cast<size_t>(std::max(1, static_cast<int>((right - left - 12) / 18)));
        float const rows = static_cast<float>((list.size() + across - 1) / across);
        return rows * 18 + (plan->request.picks.contains(group) ? 14.f : 2.f);
    };
    // How many rows of these heights fit, counted from the list's end: the scroll goes down to them.
    auto fitFromEnd = [&](std::vector<float> const& heights) {
        float room = bottom - top;
        int fit = 0;
        for (auto h = heights.rbegin(); h != heights.rend() && room >= *h; ++h) room -= *h, ++fit;
        return std::max(1, fit);
    };
    // A step as the game's slots: its inputs, an arrow, its product, and its station (L-370). `choosing`: in a
    // breakdown, where a group input opens its members and the recipe and "gather" are chosen; `entry` is
    // that breakdown's. `faint`: its icons dim (a step that cannot be made yet). Returns nothing; the row is
    // `stepHeight` high.
    constexpr float stepHeight = 20, targetHeight = 22, materialHeight = 18, labelHeight = 11;
    auto stepRow = [&](float x0, float y, StepLine const& step, Request const& request, bool choosing, std::string const& entry, bool faint) {
        auto const& key = index.items[static_cast<size_t>(step.item)];
        auto const& recipe = index.recipes[static_cast<size_t>(step.recipe)];
        size_t const iconsBefore = late.icons.size();
        float end = right;
        if (choosing) {
            end = rowButton(right, y + 4, translated("recipes.toGather"), [&] { state::setStop(key, true); });
            auto const options = recipeOptions(index, step.item);
            if (options.size() > 1) {
                int const now = static_cast<int>(std::ranges::find(options, step.recipe) - options.begin());
                auto const pager = drawPager(context, end, y + 4.5f, now, static_cast<int>(options.size()), px, py);
                auto choose = [&](int to) { state::setChoice(key, choiceKey(index.recipes[static_cast<size_t>(options[static_cast<size_t>(to)])])); };
                if (pager.prevOn && took(pager.prevX, pager.y, Pager::size, Pager::size)) choose(now - 1);
                else if (pager.nextOn && took(pager.nextX, pager.y, Pager::size, Pager::size)) choose(now + 1);
                end = pager.prevX - 3;
            }
        }
        if (auto const station = stationItem(recipe.station); !station.empty()) {
            auto const& shown = schematic::items::shown(std::string(station));
            if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, end - 11, y + 5, iconScale});
            if (over(end - 11, y, 11, stepHeight)) late.tip = shown.name;
            end -= 14;
        }
        // The inputs from the left, as many as there is room for before the arrow and the product.
        float x = x0;
        float const room = end - 18 - 12;
        auto slotAt = [&](float sx, std::string const& iconKey, Count count) {
            windowSlot(context, sx, y + 1, over(sx, y + 1, 18, 18));
            if (!iconKey.empty())
                if (auto const* stack = schematic::items::iconStack(schematic::items::shown(iconKey).icon)) late.icons.push_back({stack, sx + 1, y + 2});
            if (count > 1) late.counts.push_back({sx, y + 1, std::to_string(count)});
        };
        auto const& uses = recipe.uses;
        for (size_t u = 0; u < uses.size(); ++u) {
            auto const& part = uses[u];
            if (x + 18 > room) {
                windowLabel(context, x + 1, y + 6, room - x, "...", true);
                if (over(x, y, room - x, stepHeight)) late.tip = schematic::items::shown(key).name, late.tipLines = stepInputLines(index, request, step);
                break;
            }
            Item const item = partItem(index, request, part);
            std::string iconKey, name;
            if (item >= itemCount) {
                auto const& list = index.groups[static_cast<size_t>(item - itemCount)].members;
                if (!list.empty()) iconKey = index.items[static_cast<size_t>(list[turn % list.size()])];
                name = groupName(index, item - itemCount);
            } else if (item >= 0) iconKey = index.items[static_cast<size_t>(item)], name = schematic::items::shown(iconKey).name;
            slotAt(x, iconKey, step.crafts * part.count);
            if (over(x, y + 1, 18, 18)) late.tip = name, late.hovered = item < itemCount ? iconKey : std::string();
            if (choosing && part.group) {
                // Any of several: a press opens its members under the row to pick one.
                OpenMembers const here{entry, part.id, true};
                bool const open = openMembers == here;
                if (open) fill(context, x, y + 19, 18, 1, palette::accentDeep);
                if (took(x, y + 1, 18, 18)) openMembers = open ? OpenMembers{} : here;
            }
            x += 18;
        }
        changeArrow(context, x + 3, y + 7.5f, 1, palette::vanilla::windowInk);
        x += 12;
        slotAt(x, key, step.made);
        if (over(x, y + 1, 18, 18)) late.tip = schematic::items::shown(key).name, late.tipLines = stepInputLines(index, request, step), late.hovered = key;
        if (took(x, y + 1, 18, 18)) showItem = key;
        if (faint) for (size_t k = iconsBefore; k < late.icons.size(); ++k) late.icons[k].alpha = .45f;
    };
    if (tab == Tab::Wanted) {
        // Each wanted item, in its priority order: the finished ones held against those wanted and a bar of how
        // much of what it takes is gathered; a press opens its breakdown, where its choices are made (L-370).
        auto const* shares = state::shares(player);
        if (!shares) { note(translated("schematic.raw.reading")); return; }
        auto const& entries = shares->entries;
        if (entries.empty()) { paragraph(context, left, top + 2, right - left, translated("recipes.wantedHow"), 3, palette::vanilla::windowInk); return; }
        // The rows as lines of their own heights, so a breakdown's lines scroll like the rest.
        struct Line { enum class Kind { Target, Step, Label, Material, Members } kind; size_t entry = 0; size_t at = 0; float height = 0; };
        std::vector<Line> lines;
        for (size_t e = 0; e < entries.size(); ++e) {
            if (scrollTo == wantedEntryId(entries[e])) offset = static_cast<int>(lines.size()), scrollTo.clear();
            lines.push_back({Line::Kind::Target, e, 0, targetHeight});
            if (openEntry != wantedEntryId(entries[e]) || e >= shares->targets.size() || shares->targets[e].item < 0) continue;
            auto const& view = shares->targets[e].view;
            if (!view.steps.empty()) lines.push_back({Line::Kind::Label, e, 0, labelHeight});
            for (size_t k = 0; k < view.steps.size(); ++k) {
                lines.push_back({Line::Kind::Step, e, k, stepHeight});
                if (openMembers.fromStep && openMembers.entry == openEntry) {
                    auto const& uses = index.recipes[static_cast<size_t>(view.steps[k].recipe)].uses;
                    if (std::ranges::any_of(uses, [&](Part const& part) { return part.group && part.id == openMembers.group; }) &&
                        std::ranges::none_of(lines, [](Line const& line) { return line.kind == Line::Kind::Members; }))
                        lines.push_back({Line::Kind::Members, e, k, 0});
                }
            }
            if (!view.gather.empty()) lines.push_back({Line::Kind::Label, e, 1, labelHeight});
            for (size_t k = 0; k < view.gather.size(); ++k) {
                Item const id = view.gather[k].id;
                bool const opened = id >= itemCount && openMembers == OpenMembers{openEntry, id - itemCount, false};
                lines.push_back({Line::Kind::Material, e, k, materialHeight + (opened ? membersHeight(id - itemCount) : 0.f)});
            }
        }
        int const count = static_cast<int>(lines.size());
        std::vector<float> heights;
        for (auto const& line : lines) heights.push_back(line.kind == Line::Kind::Members ? membersHeight(openMembers.group) : line.height);
        offset = std::clamp(offset, 0, std::max(0, count - 1));
        bar(count, offset, fitFromEnd(heights));
        float y = top;
        auto const& request = plan->request;
        for (int at = offset; at < count; ++at) {
            auto const& line = lines[static_cast<size_t>(at)];
            auto const& entry = entries[line.entry];
            std::string const id = wantedEntryId(entry);
            TargetPlan const* target = line.entry < shares->targets.size() ? &shares->targets[line.entry] : nullptr;
            float height = line.height;
            if (line.kind == Line::Kind::Members) {
                if (openMembers.group < 0) continue;
                height = members(y, openMembers.group, [&] { openMembers = {}; });
            }
            if (y + std::min(height, stepHeight) > bottom) break;
            if (line.kind == Line::Kind::Label) {
                windowLabel(context, left + 8, y + 1, right - left - 8, translated(line.at == 0 ? "recipes.make" : "recipes.gather"), true);
            } else if (line.kind == Line::Kind::Step && target) {
                stepRow(left + 8, y, target->view.steps[line.at], request, true, id, false);
            } else if (line.kind == Line::Kind::Material && target) {
                auto const& material = target->view.gather[line.at];
                std::string const amounts = std::format("{} / {}", material.held, material.need);
                constexpr float barW = 26;
                float const numbersEnd = right - barW - 4, numbersW = 46;
                windowLabel(context, numbersEnd - numbersW, y + 5, numbersW, amounts, false, Align::Right);
                progressBar(context, right - barW, y + 7, barW, 3, material.need > 0 ? static_cast<float>(material.held) / static_cast<float>(material.need) : 1.f);
                float nameEnd = numbersEnd - numbersW - 4;
                bool const stopped = material.id >= 0 && material.id < itemCount && request.stops.contains(material.id);
                if (stopped) nameEnd = rowButton(nameEnd, y + 3, translated("recipes.toMake"), [&] { state::setStop(index.items[static_cast<size_t>(material.id)], false); });
                std::string iconKey, name;
                if (material.id >= itemCount) {
                    auto const& list = index.groups[static_cast<size_t>(material.id - itemCount)].members;
                    if (!list.empty()) iconKey = index.items[static_cast<size_t>(list[turn % list.size()])];
                    name = groupName(index, material.id - itemCount);
                    // A group: a press on the row opens its members to pick one.
                    OpenMembers const here{id, material.id - itemCount, false};
                    bool const open = openMembers == here;
                    if (over(left, y, nameEnd - left, height)) fill(context, left + 6, y, nameEnd - left - 6, height, palette::shade, .1f);
                    if (took(left, y, nameEnd - left, height)) openMembers = open ? OpenMembers{} : here;
                } else if (material.id >= 0) iconKey = index.items[static_cast<size_t>(material.id)], name = schematic::items::shown(iconKey).name;
                if (!iconKey.empty())
                    if (auto const* stack = schematic::items::iconStack(schematic::items::shown(iconKey).icon)) late.icons.push_back({stack, left + 8, y + 1});
                if (late.tip.empty() && over(left, y, nameEnd - left, height)) late.tip = name, late.hovered = material.id < itemCount ? iconKey : std::string();
                windowLabel(context, left + 27, y + 5, nameEnd - left - 29, name);
                // A group opened from this row shows its members right under it.
                if (material.id >= itemCount && openMembers == OpenMembers{id, material.id - itemCount, false}) {
                    members(y + materialHeight, material.id - itemCount, [&] { openMembers = {}; });
                }
            } else if (line.kind == Line::Kind::Target) {
                // The row: a fold mark, the icon and the name; under the name its bar; "have / want" and the
                // share at the right of the bar; the controls at the right end, up and down under the pointer.
                bool const manual = entry.source.empty();
                bool const hot = over(left, y, right - left, height);
                if (manual && hot) late.wantedRow = entry.key;
                Item const item = index.findKey(entry.key);
                bool const known = item >= 0 && target && target->item >= 0;
                float sx = right;
                if (!manual) {
                    std::string from = translated("recipes.from." + entry.source);
                    float const w = textWidth(context, from) + 4;
                    windowLabel(context, right - w, y + 6, w, std::move(from), true, Align::Right);
                    sx = right - w - 4;
                } else {
                    float const end = rowButton(right, y + 5, translated("recipes.remove"), [&] { state::removeWanted(entry.key); });
                    sx = end - stepperWidth;
                    drawStepper(context, sx, y + 6, stepperWidth, 10, true, std::to_string(entry.count),
                                {.editing = countEdit::editing(entry.key), .pointerX = px, .pointerY = py, .backed = true, .dimValue = entry.count == 0});
                    if (took(sx, y + 6, 11, 10)) state::stepWanted(entry.key, false, shift, true);
                    else if (took(sx + stepperWidth - 11, y + 6, 11, 10)) state::stepWanted(entry.key, true, shift, true);
                    else if (took(sx, y + 6, stepperWidth, 10)) countEdit::begin(entry.key, entry.count);
                    else if (wheel && over(sx, y + 6, stepperWidth, 10)) {
                        state::stepWanted(entry.key, wheel < 0, shift, false);
                        wheel = 0;
                    }
                    sx -= 3;
                }
                if (hot) {
                    sx -= moveButtonsWidth;
                    int const move = drawMoveButtons(context, sx, y + 5.5f, line.entry > 0, line.entry + 1 < entries.size(), px, py);
                    if (move && took(sx, y + 5.5f, moveButtonsWidth, moveButtonSize)) state::moveWanted(id, move);
                    sx -= 3;
                }
                Count want = entry.count, have = 0;
                float fraction = 1;
                int percent = 100;
                if (known) have = target->finished, fraction = target->fraction(), percent = target->percent();
                else
                    for (auto const& loose : plan->loose)
                        if (loose.key == entry.key) {
                            have = loose.held;
                            fraction = loose.need > 0 ? static_cast<float>(loose.held) / static_cast<float>(loose.need) : 1.f;
                            percent = static_cast<int>(fraction * 100);
                        }
                bool const done = want > 0 && have >= want;
                if (known) chevron(context, left, y + 8, openEntry == id, palette::vanilla::windowInk);
                auto const& shown = schematic::items::shown(entry.key);
                if (auto const* stack = schematic::items::iconStack(shown.icon)) late.icons.push_back({stack, left + 8, y + 3, 1.f, 0, done ? .45f : 1.f});
                std::string const amounts = done ? std::format("{} / {}", have, want) : std::format("{} / {}  {}%", have, want, percent);
                float const amountsW = textWidth(context, amounts);
                windowLabel(context, left + 27, y + 2, sx - left - 29, shown.name, done);
                windowLabel(context, sx - amountsW - 2, y + 11, amountsW, amounts, true, Align::Right);
                progressBar(context, left + 27, y + 14, std::max(10.f, sx - amountsW - 6 - (left + 27)), 3, fraction);
                if (late.tip.empty() && over(left, y, sx - left, height)) late.tip = shown.name, late.hovered = entry.key;
                if (known && took(left, y, sx - left, height)) {
                    openEntry = openEntry == id ? std::string() : id;
                    openMembers = {};
                }
            }
            y += height;
        }
        offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
        return;
    }
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
        std::vector<float> heights;
        for (auto const& row : rows)
            heights.push_back(rowHeight + (row.key.empty() && row.line.id >= itemCount && expanded == row.line.id - itemCount ? membersHeight(expanded) : 0.f));
        offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
        bar(count, offset, fitFromEnd(heights));
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
    // What to make: the work to do. Steps that can be made now come first, the others under them faint; a press
    // opens the breakdown of the wanted item that needs the step, where its choices are made (L-370).
    auto const& steps = plan->view.steps;
    if (steps.empty()) { note(translated("recipes.nothingWanted")); return; }
    std::map<Item, Count> stock;
    for (auto const& [id, have] : plan->request.held) stock[id] += have;
    std::vector<std::pair<StepLine const*, bool>> order;
    for (auto const& step : steps) if (stepReady(index, plan->request, step, stock)) order.push_back({&step, true});
    for (auto const& step : steps) if (!stepReady(index, plan->request, step, stock)) order.push_back({&step, false});
    int const count = static_cast<int>(order.size());
    offset = std::clamp(offset + wheel, 0, std::max(0, count - 1));
    bar(count, offset, std::max(1, static_cast<int>((bottom - top) / stepHeight)));
    float y = top;
    for (int at = offset; at < count && y + stepHeight <= bottom; ++at) {
        auto const& [step, ready] = order[static_cast<size_t>(at)];
        bool const hot = over(left, y, right - left, stepHeight);
        if (hot) fill(context, left - 2, y, right - left + 4, stepHeight, palette::shade, .08f);
        stepRow(left, y, *step, plan->request, false, {}, !ready);
        if (took(left, y, right - left, stepHeight))
            if (auto const* shares = state::shares(player)) {
            for (size_t e = 0; e < shares->targets.size() && e < shares->entries.size(); ++e)
                if (std::ranges::any_of(shares->targets[e].view.steps, [&](StepLine const& mine) { return mine.item == step->item; })) {
                    openEntry = scrollTo = wantedEntryId(shares->entries[e]);
                    openMembers = {};
                    tab = Tab::Wanted;
                    break;
                }
        }
        y += stepHeight;
    }
    if (!showItem.empty()) showChoice = false;
}
}
