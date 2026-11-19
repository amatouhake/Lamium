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
constexpr float optionHeight = 24, rowHeight = 18, stepHeight = 20, labelHeight = 11, headHeight = 11, headTop = 4, rowsTop = 16, small = .625f, stepperWidth = 42;
}
bool isOpen() { return opened; }
void open(Tab which) {
    opened = true;
    tab = which;
}
void openHowTo(std::string const& key) {
    opened = true;
    tab = Tab::HowTo;
    howOpen = howScroll = howFlash = key;
    howFlashAt = std::chrono::steady_clock::now();
}
void close() {
    opened = false;
    openEntry.clear();
    howOpen.clear();
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
    float const coverLeft = input.coverRight > input.coverLeft ? std::min(l.left, input.coverLeft) : l.left;
    float const coverRight = input.coverRight > input.coverLeft ? std::max(l.left + l.width, input.coverRight) : l.left + l.width;
    windowPanel(context, coverLeft, l.top, coverRight - coverLeft, l.height);
    OnWindow const onWindowScope;
    // The lists are the game's tabs on the window's top edge; they name the window, which needs no title.
    // Without the plan the window holds only "How to make", which is the whole Recipes feature's.
    if (!input.plan) tab = Tab::HowTo;
    {
        Tab const all[]{Tab::Wanted, Tab::Gather, Tab::Make, Tab::HowTo, Tab::Places};
        char const* const allNames[]{"recipes.wanted", "recipes.gather", "recipes.make", "recipes.howTo", "recipes.places"};
        int const first = input.plan ? 0 : 3, shown = input.plan ? 5 : 1;
        // One width for all, as wide as the widest name needs and no wider than the window allows.
        float widest = 0;
        for (int i = first; i < first + shown; ++i) widest = std::max(widest, textWidth(context, translated(allNames[i])));
        float const w = std::floor(std::min(widest + 12, (l.width - 10 + static_cast<float>(shown - 1)) / static_cast<float>(shown)));
        float x = l.left + 5;
        for (int i = first; i < first + shown; ++i) {
            std::string name = translated(allNames[i]);
            windowTab(context, x, l.top, w, std::move(name), tab == all[i], over(x, l.top - windowTabRise, w - 1, windowTabRise));
            if (took(x, l.top - windowTabRise, w - 1, windowTabRise)) tab = all[i];
            x += w - 1; // neighbours share their edge
        }
    }
    auto const* plan = state::plan(player);
    float const left = l.left + 6, right = l.left + l.width - 6;
    float top = l.top + headTop;
    float const bottom = l.top + l.height - RecipeViewLayout::foot;
    int& offset = scroll[static_cast<int>(tab)];
    int wheel = input.wheel;
    // Every list scrolls with the shared bar in the margin right of the rows; `visible` is how many rows of their
    // real heights fit at the list's end.
    auto bar = [&](int count, int& first, int visible) {
        listScrollbar(context, right + 2, top, bottom - top, count, std::max(1, visible), first, px, py, pressed, input.held);
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
        float const w = textWidth(context, text) + 8;
        drawButton(context, end - w, y, w, 10, std::move(text), over(end - w, y, w, 10));
        if (took(end - w, y, w, 10)) act();
        return end - w - 3;
    };
    if (tab == Tab::Places) {
        // What counts as held: what is carried, then the ender chest and the stores as they were when last seen
        // open. Each row is a name and its switch, the switches in one column; a store can be taken out as well.
        constexpr float placeRow = 12;
        auto const all = state::places();
        auto const& places = all.list; // of a copy: the controls below change the list
        std::string const removeText = translated("recipes.remove");
        float const sx = right - (textWidth(context, removeText) + 8) - 4 - switchWidth;
        int const rows = static_cast<int>(places.size()) + 1, fits = static_cast<int>((bottom - top) / placeRow);
        offset = std::clamp(offset + wheel, 0, std::max(0, rows - fits));
        bar(rows, offset, fits);
        for (int row = 0; row < fits && offset + row < rows; ++row) {
            float const y = top + static_cast<float>(row) * placeRow;
            int const at = offset + row - 1;
            std::string name, icon, tip;
            bool on = false;
            if (at < 0) name = translated("recipes.place.carried"), tip = translated("recipes.place.carriedTip"), icon = "minecraft:player_head", on = all.carried;
            else {
                auto const& place = places[static_cast<size_t>(at)];
                name = placeName(place), icon = place.block, on = place.counted;
                if (!place.ender) rowButton(right, y + 1, removeText, [&] { state::unmarkStore(at); });
            }
            toggleSwitch(context, sx, y + 1.5f, on);
            if (took(sx - 2, y, switchWidth + 4, placeRow)) {
                if (at < 0) state::setCarriedCounted(!all.carried);
                else state::setPlaceCounted(at, !on);
            }
            if (auto const* stack = schematic::items::iconStack(schematic::items::shown(icon).icon)) late.icons.push_back({stack, left, y + 1, small});
            if (late.tip.empty() && over(left, y, sx - left - 4, placeRow)) {
                late.tip = name;
                if (!tip.empty()) late.tipLines.push_back(std::move(tip));
            }
            windowLabel(context, left + 13, y + 2, sx - left - 17, std::move(name), !on);
        }
        if (places.empty()) paragraph(context, left, top + placeRow + 4, right - left, translated("recipes.place.how"), 3, v::windowFaint);
        return;
    }
    if (!plan) { note(translated("schematic.raw.reading")); return; }
    auto const& index = plan->table->index;
    auto const& request = plan->request;
    Item const itemCount = static_cast<Item>(index.items.size());
    auto const turn = static_cast<size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1500);
    auto const notes = state::notes();

    // ---- Shared pieces ----
    // The table's columns, the same on every list: fold, icon, name, held, wanted (or needed), a bar, controls.
    constexpr float controlsW = 26, barW = 24, wantW = stepperWidth, heldW = 24;
    float const controlsX = right - controlsW, barX = controlsX - 2 - barW, wantX = barX - 3 - wantW, heldX = wantX - 2 - heldW;
    float const nameX = left + 26, nameEnd = heldX - 3;
    auto columnHeads = [&](float y, char const* wantKey, bool withName = false, std::string name = {}) {
        if (withName) windowLabel(context, nameX, y + 1, nameEnd - nameX, std::move(name), true);
        labelScaled(context, heldX, y + 2, heldW, translated("recipes.col.held"), .75f, v::windowFaint, Align::Right, false);
        labelScaled(context, wantX, y + 2, wantW, translated(wantKey), .75f, v::windowFaint, Align::Right, false);
        labelScaled(context, barX, y + 2, barW, translated("recipes.col.gathered"), .75f, v::windowFaint, Align::Left, false);
    };
    auto number = [&](float x, float w, float y, Count count, bool faint) {
        windowLabel(context, x, y, w, std::to_string(count), faint, Align::Right);
    };
    // An item or a group as its picture and name: a group shows the member its line in "How to make" names, or
    // turns through them.
    auto pictureOf = [&](Item id) -> std::string {
        if (id >= itemCount) {
            int const group = id - itemCount;
            if (auto picked = request.picks.find(group); picked != request.picks.end()) return index.items[static_cast<size_t>(picked->second)];
            auto const& list = index.groups[static_cast<size_t>(group)].members;
            return list.empty() ? std::string() : index.items[static_cast<size_t>(list[turn % list.size()])];
        }
        return id >= 0 ? index.items[static_cast<size_t>(id)] : std::string();
    };
    auto groupFace = [&](int group) -> std::string {
        auto const& list = index.groups[static_cast<size_t>(group)].members;
        return list.empty() ? std::string() : index.items[static_cast<size_t>(list.front())];
    };
    auto groupPicture = [&](int group) -> std::string {
        auto const& list = index.groups[static_cast<size_t>(group)].members;
        return list.empty() ? std::string() : index.items[static_cast<size_t>(list[turn % list.size()])];
    };
    auto nameOf = [&](Item id) -> std::string {
        if (id >= itemCount) return groupName(index, id - itemCount);
        return id >= 0 ? schematic::items::shown(index.items[static_cast<size_t>(id)]).name : std::string();
    };
    auto icon = [&](std::string const& key, float x, float y, float scale = 1.f, float alpha = 1.f) {
        if (key.empty()) return;
        if (auto const* stack = schematic::items::iconStack(schematic::items::shown(key).icon)) late.icons.push_back({stack, x, y, scale, 0, alpha});
    };
    // Under the pointer a group names itself and shows its members as small icons.
    auto groupTip = [&](int group) {
        late.tip = groupName(index, group);
        late.tipLines.clear();
        late.tipItems.clear();
        for (Item member : index.groups[static_cast<size_t>(group)].members) late.tipItems.push_back(index.items[static_cast<size_t>(member)]);
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
        if (chosen) fill(context, x, y, w, 10, palette::accentDeep);
        bool const hot = over(x, y, w, 10);
        if (hot) fill(context, x, y, w, 10, palette::white, .2f), late.tip = translated("recipes.howTo.change");
        labelScaled(context, x + 3, y + 2.5f, w - 6, std::move(text), .75f, chosen ? palette::text : v::windowFaint, Align::Left, chosen);
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
    auto inputsRoom = [&](std::vector<StepLine> const& steps) {
        size_t most = 2;
        for (auto const& step : steps) most = std::max(most, index.recipes[static_cast<size_t>(step.recipe)].uses.size());
        return static_cast<float>(std::min<size_t>(most, 4)) * 18;
    };
    auto stepSlots = [&](float x0, float y, StepLine const& step, bool faint, bool interactive = true, float room = inputsW) {
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
            windowSlot(context, sx, y + 1, over(sx, y + 1, 18, 18));
            icon(pictureOf(item), sx + 1, y + 2);
            if (Count const n = step.crafts * part.count; n > 1) late.counts.push_back({sx, y + 1, std::to_string(n)});
            auto const picked = part.group ? request.picks.find(part.id) : request.picks.end();
            if (picked != request.picks.end()) frame(context, sx, y + 1, 18, 18, palette::accentDeep);
            if (over(sx, y + 1, 18, 18)) {
                if (part.group) {
                    groupTip(part.id);
                    late.tipLines = {picked != request.picks.end() ? translated("recipes.howTo.chosen") + ": " + nameOf(picked->second) : translated("recipes.howTo.any")};
                    if (interactive) late.tipLines.push_back(translated("recipes.howTo.change"));
                } else late.tip = nameOf(item), late.hovered = pictureOf(item);
            }
            if (interactive && part.group && took(sx, y + 1, 18, 18)) jumpHowTo(groupKey(index, part.id));
        }
        if (uses.size() > shown && over(x0, y, room, stepHeight))
            late.tip = schematic::items::shown(key).name, late.tipLines = stepInputLines(index, request, step);
        changeArrow(context, arrowX + 3, y + 7.5f, 1, v::windowInk);
        windowSlot(context, productX, y + 1, over(productX, y + 1, 18, 18));
        icon(key, productX + 1, y + 2);
        if (step.made > 1) late.counts.push_back({productX, y + 1, std::to_string(step.made)});
        if (interactive && over(productX, y + 1, 18, 18))
            late.tip = schematic::items::shown(key).name, late.tipLines = stepInputLines(index, request, step), late.hovered = key;
        if (interactive && took(productX, y + 1, 18, 18)) showItem = key;
        if (auto const station = stationItem(recipe.station); !station.empty()) {
            auto const& at = schematic::items::shown(std::string(station));
            icon(std::string(station), productX + 22, y + 5, small);
            if (over(productX + 22, y + 4, 11, 12)) late.tip = at.name;
        }
        if (faint) for (size_t k = iconsBefore; k < late.icons.size(); ++k) late.icons[k].alpha = .45f;
        return productX + 36; // past the station's icon: productX + 22 + 11, and a gap
    };

    if (tab == Tab::Wanted) {
        // The schematic's switch above the table, while Schematics is on.
        if (Runtime::instance().preferences().schematic.enabled) {
            bool const on = Runtime::instance().preferences().schematic.planMaterials;
            if (over(left, top, right - left, 12)) fill(context, left - 2, top, right - left + 4, 12, palette::shade, .08f);
            windowLabel(context, left, top + 2, right - left - switchWidth - 4, translated("recipes.plan.schematic"));
            toggleSwitch(context, right - switchWidth, top + 1.5f, on);
            if (took(left, top, right - left, 12)) {
                auto value = Runtime::instance().preferences();
                value.schematic.planMaterials = !on;
                Runtime::instance().save(value);
            }
            top += 13;
        }
        auto const* shares = state::shares(player);
        if (!shares) { note(translated("schematic.raw.reading")); return; }
        auto const& entries = shares->entries;
        if (entries.empty()) { paragraph(context, left, top + 2, right - left, translated("recipes.wantedHow"), 3, v::windowInk); return; }
        columnHeads(top, "recipes.col.wanted");
        top += headHeight;
        // Lines of their own heights: a wanted row, and under an open one its breakdown.
        struct Line { enum class Kind { Target, Label, Step, Material, End } kind; size_t entry = 0; size_t at = 0; float height = 0; };
        std::vector<Line> lines;
        for (size_t e = 0; e < entries.size(); ++e) {
            if (scrollTo == wantedEntryId(entries[e])) offset = static_cast<int>(lines.size()), scrollTo.clear();
            lines.push_back({Line::Kind::Target, e, 0, rowHeight});
            if (openEntry != wantedEntryId(entries[e]) || e >= shares->targets.size() || shares->targets[e].item < 0) continue;
            auto const& view = shares->targets[e].view;
            if (!view.steps.empty()) lines.push_back({Line::Kind::Label, e, 0, labelHeight});
            for (size_t k = 0; k < view.steps.size(); ++k) lines.push_back({Line::Kind::Step, e, k, stepHeight});
            if (!view.gather.empty()) lines.push_back({Line::Kind::Label, e, 1, labelHeight});
            for (size_t k = 0; k < view.gather.size(); ++k) lines.push_back({Line::Kind::Material, e, k, rowHeight});
            lines.push_back({Line::Kind::End, e, 0, 3});
        }
        int const count = static_cast<int>(lines.size());
        std::vector<float> heights;
        for (auto const& line : lines) heights.push_back(line.height);
        offset = std::clamp(offset, 0, std::max(0, count - 1));
        bar(count, offset, fitFromEnd(heights));
        float const faceX = left + 8, faceW = right - faceX, x0 = faceX + 4;
        float y = top;
        for (int at = offset; at < count && y + lines[static_cast<size_t>(at)].height <= bottom + .5f; ++at) {
            auto const& line = lines[static_cast<size_t>(at)];
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
                float const after = stepSlots(x0, y, step, false, true, inputsRoom(target->view.steps));
                float const end = rowButton(right - 3, y + 5, translated("recipes.toGather"), [&] { state::setStop(index.items[static_cast<size_t>(step.item)], true); });
                // The recipe's state in the room left between the station and the button.
                recipeState(end, y + 5, step.item, end - after);
            } else if (line.kind == Line::Kind::Material && target) {
                auto const& material = target->view.gather[line.at];
                // One icon column with the steps: the material under the inputs' last slot, its name after.
                float const room = inputsRoom(target->view.steps);
                float const iconX = x0 + room - 17, labelX = x0 + room + 3;
                icon(pictureOf(material.id), iconX, y + 1);
                float end = right - 3;
                std::string const amounts = std::format("{} / {}", material.held, material.need);
                float const amountsW = textWidth(context, amounts);
                windowLabel(context, end - amountsW, y + 5, amountsW, amounts, false, Align::Right);
                end -= amountsW + 4;
                bool const stopped = material.id >= 0 && material.id < itemCount && request.stops.contains(material.id);
                if (stopped) end = rowButton(end, y + 4, translated("recipes.toMake"), [&] { state::setStop(index.items[static_cast<size_t>(material.id)], false); });
                if (material.id >= itemCount) end = groupState(end, y + 4, material.id - itemCount);
                windowLabel(context, labelX, y + 5, end - labelX, nameOf(material.id));
                if (late.tip.empty() && over(iconX, y, end - iconX, rowHeight)) {
                    if (material.id >= itemCount) groupTip(material.id - itemCount);
                    else late.tip = nameOf(material.id), late.hovered = pictureOf(material.id);
                }
            } else if (line.kind == Line::Kind::Target) {
                bool const manual = entry.source.empty();
                bool const hot = over(left, y, right - left, rowHeight);
                if (manual && hot) late.wantedRow = entry.key;
                bool const known = target && target->item >= 0;
                Count have = 0;
                float fraction = 1;
                if (known) have = target->finished, fraction = target->fraction();
                else
                    for (auto const& loose : plan->loose)
                        if (loose.key == entry.key) have = loose.held, fraction = loose.need > 0 ? static_cast<float>(loose.held) / static_cast<float>(loose.need) : 1.f;
                bool const done = entry.count > 0 && have >= entry.count;
                if (known) chevron(context, left, y + 6, openEntry == id, v::windowInk);
                auto const& shown = schematic::items::shown(entry.key);
                icon(entry.key, left + 8, y + 1, 1.f, done ? .45f : 1.f);
                windowLabel(context, nameX, y + 5, nameEnd - nameX, shown.name, done);
                number(heldX, heldW, y + 5, have, done);
                progressBar(context, barX, y + 7.5f, barW, 3, fraction);
                // The player's own: "wanted" turns into the stepper and remove shows under the pointer; nothing moves.
                bool onControl = false;
                if (manual && hot) {
                    drawStepper(context, wantX, y + 4, wantW, 10, true, std::to_string(entry.count),
                                {.editing = countEdit::editing(entry.key), .pointerX = px, .pointerY = py, .backed = true, .dimValue = entry.count == 0});
                    float const xW = 11, xX = controlsX + controlsW - xW;
                    drawButton(context, xX, y + 4, xW, 10, "x", over(xX, y + 4, xW, 10));
                    onControl = over(wantX, y + 4, wantW, 10) || over(xX, y + 4, xW, 10);
                    if (took(xX, y + 4, xW, 10)) state::removeWanted(entry.key);
                    else if (took(wantX, y + 4, 11, 10)) state::stepWanted(entry.key, false, shift, true);
                    else if (took(wantX + wantW - 11, y + 4, 11, 10)) state::stepWanted(entry.key, true, shift, true);
                    else if (took(wantX, y + 4, wantW, 10)) countEdit::begin(entry.key, entry.count);
                    else if (wheel && over(wantX, y + 4, wantW, 10)) state::stepWanted(entry.key, wheel < 0, shift, false), wheel = 0;
                } else {
                    if (countEdit::editing(entry.key))
                        drawStepper(context, wantX, y + 4, wantW, 10, true, std::to_string(entry.count), {.editing = countEdit::editing(entry.key), .backed = true});
                    else number(wantX, wantW, y + 5, entry.count, done || !manual);
                    if (!manual) labelScaled(context, controlsX, y + 6, controlsW, translated("recipes.from." + entry.source), .75f, v::windowFaint, Align::Right, false);
                }
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
        struct Row { GatherLine line; std::string key; bool stopped = false, done = false; };
        std::vector<Row> rows, finished;
        for (auto const& line : plan->view.gather) {
            bool const stopped = line.id < itemCount && request.stops.contains(line.id);
            if (line.left > 0) rows.push_back({line, {}, stopped});
            else if (stopped) finished.push_back({line, {}, true, true});
        }
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
        columnHeads(top, "recipes.col.need");
        top += headHeight;
        // The whole, as the table's first row.
        windowLabel(context, nameX, top + 5, nameEnd - nameX, translated("recipes.all"));
        number(heldX, heldW, top + 5, total.need - total.left, false);
        number(wantX, wantW, top + 5, total.need, false);
        progressBar(context, barX, top + 7.5f, barW, 3, total.fraction());
        fill(context, left, top + rowHeight - 1, right - left, .5f, v::windowFaint);
        top += rowHeight;
        int const count = static_cast<int>(rows.size()), fits = static_cast<int>((bottom - top) / rowHeight);
        offset = std::clamp(offset + wheel, 0, std::max(0, count - fits));
        bar(count, offset, fits);
        for (int at = offset; at < count && at < offset + fits; ++at) {
            auto const& row = rows[static_cast<size_t>(at)];
            auto const& line = row.line;
            float const y = top + static_cast<float>(at - offset) * rowHeight;
            float const alpha = row.done ? .45f : 1.f;
            std::string const picture = row.key.empty() ? pictureOf(line.id) : row.key;
            std::string const name = row.key.empty() ? nameOf(line.id) : schematic::items::shown(row.key).name;
            icon(picture, left + 8, y + 1, 1.f, alpha);
            number(heldX, heldW, y + 5, line.held, row.done);
            number(wantX, wantW, y + 5, line.need, row.done);
            progressBar(context, barX, y + 7.5f, barW, 3, line.need > 0 ? static_cast<float>(line.held) / static_cast<float>(line.need) : 1.f);
            float end = nameEnd;
            if (row.stopped) rowButton(right, y + 4, translated("recipes.toMake"), [&] { state::setStop(index.items[static_cast<size_t>(line.id)], false); });
            else if (row.key.empty() && line.id >= itemCount) groupState(right, y + 4, line.id - itemCount);
            // A count that includes what a place held when last seen is starred and says where the items are.
            if (line.id >= 0 && line.id < itemCount) {
                auto const stored = plan->stored.find(line.id);
                if (stored != plan->stored.end() && stored->second) {
                    windowLabel(context, heldX - 5, y + 5, 5, "*", true);
                    if (over(heldX - 5, y, heldW + 5, rowHeight)) {
                        auto const carried = plan->held.find(line.id);
                        late.tip = translated("recipes.place.tipCarried", carried == plan->held.end() ? Count{0} : carried->second);
                        for (auto const& place : state::places().list) {
                            if (!place.counted) continue;
                            Count here = 0;
                            for (auto const& item : place.items) if (index.find(item.name, item.aux) == line.id) here += item.count;
                            if (here) late.tipLines.push_back(translated("recipes.place.tipPlace", placeName(place), here));
                        }
                    }
                    end -= 6;
                }
            }
            windowLabel(context, nameX, y + 5, end - nameX, name, row.done);
            if (late.tip.empty() && over(left, y, end - left, rowHeight)) {
                if (row.key.empty() && line.id >= itemCount) groupTip(line.id - itemCount);
                else late.tip = name, late.hovered = picture;
                if (line.left > 0) late.tipLines.push_back(amountText(line.left, schematic::items::shown(picture).stack));
            }
        }
        return;
    }

    if (tab == Tab::Make) {
        // The work: steps that can be made now from what is held first, the others under them faint; a press opens
        // the breakdown of the wanted item that needs the step.
        auto const& steps = plan->view.steps;
        if (steps.empty()) { note(translated("recipes.nothingWanted")); return; }
        std::map<Item, Count> stock;
        for (auto const& [id, have] : request.held) stock[id] += have;
        struct Line { StepLine const* step = nullptr; bool ready = false; char const* label = nullptr; float height = 0; };
        std::vector<Line> lines;
        for (bool ready : {true, false}) {
            bool first = true;
            for (auto const& step : steps) {
                if (stepReady(index, request, step, stock) != ready) continue;
                if (first) lines.push_back({nullptr, ready, ready ? "recipes.ready" : "recipes.notReady", labelHeight}), first = false;
                lines.push_back({&step, ready, nullptr, stepHeight});
            }
        }
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
            } else {
                if (over(left, y, right - left, stepHeight)) fill(context, left - 2, y, right - left + 4, stepHeight, palette::shade, .08f);
                float const after = stepSlots(left, y, *line.step, !line.ready);
                windowLabel(context, after + 2, y + 6, right - after - 2, schematic::items::shown(index.items[static_cast<size_t>(line.step->item)]).name, !line.ready);
                if (took(left, y, right - left, stepHeight))
                    if (auto const* shares = state::shares(player))
                        for (size_t e = 0; e < shares->targets.size() && e < shares->entries.size(); ++e)
                            if (std::ranges::any_of(shares->targets[e].view.steps, [&](StepLine const& mine) { return mine.item == line.step->item; })) {
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
    if (input.plan) {
        float const swY = top;
        windowLabel(context, left, swY + 2, right - left - switchWidth - 4, translated("recipes.howTo.showAll"));
        toggleSwitch(context, right - switchWidth, swY + 1.5f, howAll);
        if (took(left, swY, right - left, 12)) howAll = !howAll;
        top += 14;
    }
    // Which groups and items have a line: those in the current plan, and with the switch on all chosen before.
    std::vector<int> groups;
    std::vector<Item> items;
    auto addGroup = [&](int group) { if (std::ranges::find(groups, group) == groups.end()) groups.push_back(group); };
    auto addItem = [&](Item item) { if (item >= 0 && std::ranges::find(items, item) == items.end()) items.push_back(item); };
    if (input.plan) {
        for (auto const& line : plan->view.gather) if (line.id >= itemCount) addGroup(line.id - itemCount);
        for (auto const& step : plan->view.steps) {
            for (auto const& part : index.recipes[static_cast<size_t>(step.recipe)].uses) if (part.group) addGroup(part.id);
            if (recipeOptions(index, step.item).size() > 1) addItem(step.item);
        }
    }
    if (showAll) {
        // Every group the player chose a member for is among the request's picks, plan or not.
        for (auto const& [group, member] : request.picks) addGroup(group);
        for (auto const& [key, recipe] : notes.choices) if (Item const item = index.findKey(key); item >= 0 && recipeOptions(index, item).size() > 1) addItem(item);
    }
    // The line asked for (from a breakdown or the recipe view) always has a place, in the plan or not.
    if (!howOpen.empty()) {
        if (Item const item = index.findKey(howOpen); item >= 0 && recipeOptions(index, item).size() > 1) addItem(item);
        else
            for (size_t group = 0; group < index.groups.size(); ++group)
                if (groupKey(index, static_cast<int>(group)) == howOpen) { addGroup(static_cast<int>(group)); break; }
    }
    if (groups.empty() && items.empty()) { howScroll.clear(); note(translated(input.plan ? "recipes.nothingWanted" : "recipes.howTo.none")); return; }
    // By name: a choice changes the plan, and its lines must not move under the pointer.
    std::ranges::stable_sort(groups, {}, [&](int group) { return groupName(index, group); });
    std::ranges::stable_sort(items, {}, [&](Item item) { return nameOf(item); });
    struct Line { enum class Kind { Label, Group, Members, Recipe, Option } kind; int group = -1; Item item = -1; int option = -1; float height = 0; };
    std::vector<Line> lines;
    size_t const across = static_cast<size_t>(std::max(1.f, (right - left - 24) / 18));
    if (!groups.empty()) lines.push_back({Line::Kind::Label, -1, -1, 0, labelHeight});
    for (int group : groups) {
        std::string const key = groupKey(index, group);
        if (howScroll == key) offset = static_cast<int>(lines.size()), howScroll.clear();
        lines.push_back({Line::Kind::Group, group, -1, -1, stepHeight});
        if (howOpen == key) {
            size_t const cells = index.groups[static_cast<size_t>(group)].members.size() + 1;
            lines.push_back({Line::Kind::Members, group, -1, -1, static_cast<float>((cells + across - 1) / across) * 18 + 6});
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
    float const arrowX = left + 92, slotX = arrowX + 12, wordsX = slotX + 22;
    auto words = [&](float y, std::string main, std::string sub) {
        windowLabel(context, wordsX, y + 2, right - wordsX, std::move(main));
        labelScaled(context, wordsX, y + 12, right - wordsX, std::move(sub), .75f, v::windowFaint, Align::Left, false);
    };
    float y = top;
    for (int at = offset; at < count && y + lines[static_cast<size_t>(at)].height <= bottom + .5f; ++at) {
        auto const& line = lines[static_cast<size_t>(at)];
        if (line.kind == Line::Kind::Label) {
            labelScaled(context, left, y + 3, right - left, translated(line.option == 0 ? "recipes.howTo.groups" : "recipes.howTo.recipes"), .75f, v::windowFaint,
                        Align::Left, false);
        } else if (line.kind == Line::Kind::Group) {
            std::string const key = groupKey(index, line.group);
            auto picked = request.picks.find(line.group);
            bool const chosen = picked != request.picks.end();
            if (over(left, y, right - left, stepHeight)) fill(context, left - 2, y, right - left + 4, stepHeight, palette::shade, .08f);
            if (flashing && howFlash == key) frame(context, left - 2, y, right - left + 4, stepHeight, palette::accentDeep);
            icon(groupFace(line.group), left + 1, y + 2);
            std::string const name = groupName(index, line.group);
            windowLabel(context, left + 20, y + 6, arrowX - left - 22, name);
            changeArrow(context, arrowX + 3, y + 7.5f, 1, v::windowInk);
            windowSlot(context, slotX, y + 1);
            icon(chosen ? index.items[static_cast<size_t>(picked->second)] : groupPicture(line.group), slotX + 1, y + 2);
            if (!chosen) labelScaled(context, slotX + 2, y + 1, 8, "*", .75f, palette::text, Align::Left, true);
            if (chosen) words(y, nameOf(picked->second), translated("recipes.howTo.chosen"));
            else words(y, translated("recipes.howTo.any"), translated("recipes.howTo.anyTip"));
            if (late.tip.empty() && over(left, y, arrowX - left, stepHeight)) groupTip(line.group);
            if (took(left, y, right - left, stepHeight)) howOpen = howOpen == key ? std::string() : key;
        } else if (line.kind == Line::Kind::Members) {
            // The candidates as the game's slots: "any" first (the group's picture, marked), then each member.
            std::string const key = groupKey(index, line.group);
            auto picked = request.picks.find(line.group);
            sunkFace(context, left + 18, y, right - left - 18, line.height);
            auto const& members = index.groups[static_cast<size_t>(line.group)].members;
            for (size_t c = 0; c <= members.size(); ++c) {
                float const cx = left + 21 + static_cast<float>(c % across) * 18, cy = y + 3 + static_cast<float>(c / across) * 18;
                bool const hot = over(cx, cy, 18, 18);
                windowSlot(context, cx, cy, hot);
                if (c == 0) {
                    icon(groupPicture(line.group), cx + 1, cy + 1);
                    labelScaled(context, cx + 1, cy, 8, "*", .75f, palette::text, Align::Left, true);
                    if (picked == request.picks.end()) frame(context, cx, cy, 18, 18, palette::accentDeep);
                    if (hot) late.tip = translated("recipes.howTo.any"), late.tipLines = {translated("recipes.howTo.anyTip")};
                    if (took(cx, cy, 18, 18)) state::setPick(key, {}), howOpen.clear();
                } else {
                    Item const member = members[c - 1];
                    auto const& memberKey = index.items[static_cast<size_t>(member)];
                    icon(memberKey, cx + 1, cy + 1);
                    if (picked != request.picks.end() && picked->second == member) frame(context, cx, cy, 18, 18, palette::accentDeep);
                    auto held = plan->held.find(member);
                    if (held != plan->held.end() && held->second) late.counts.push_back({cx, cy, std::to_string(held->second)});
                    if (hot) late.tip = nameOf(member), late.hovered = memberKey;
                    if (took(cx, cy, 18, 18)) state::setPick(key, memberKey), howOpen.clear();
                }
            }
        } else if (line.kind == Line::Kind::Recipe) {
            std::string const& key = index.items[static_cast<size_t>(line.item)];
            int const now = plan->choice->recipe[static_cast<size_t>(line.item)], rule = plan->table->choice.recipe[static_cast<size_t>(line.item)];
            bool const chosen = notes.choices.contains(key) && now >= 0;
            if (over(left, y, right - left, stepHeight)) fill(context, left - 2, y, right - left + 4, stepHeight, palette::shade, .08f);
            if (flashing && howFlash == key) frame(context, left - 2, y, right - left + 4, stepHeight, palette::accentDeep);
            icon(key, left + 1, y + 2);
            windowLabel(context, left + 20, y + 6, arrowX - left - 22, nameOf(line.item));
            // Made from: the recipe in use shown by its first input.
            windowSlot(context, slotX, y + 1);
            if (now >= 0)
                if (auto const& recipe = index.recipes[static_cast<size_t>(now)]; !recipe.uses.empty()) icon(pictureOf(partItem(index, request, recipe.uses.front())), slotX + 1, y + 2);
            labelScaled(context, arrowX + 2, y + 6, 10, "<", 1.f, v::windowInk, Align::Center, false);
            if (chosen) words(y, recipeName(now), translated("recipes.howTo.chosen"));
            else words(y, translated("recipes.howTo.auto"), translated("recipes.howTo.autoNow", rule >= 0 ? recipeName(rule) : std::string()));
            if (late.tip.empty() && over(left, y, arrowX - left, stepHeight)) late.tip = nameOf(line.item), late.hovered = key;
            if (took(left, y, right - left, stepHeight)) howOpen = howOpen == key ? std::string() : key;
        } else if (line.kind == Line::Kind::Option) {
            // The candidates as rows of slots on a sunk face: "automatic" first, then each recipe.
            std::string const& key = index.items[static_cast<size_t>(line.item)];
            bool const first = at == 0 || lines[static_cast<size_t>(at - 1)].kind != Line::Kind::Option;
            bool const last = at + 1 >= count || lines[static_cast<size_t>(at + 1)].kind != Line::Kind::Option;
            float const h = optionHeight, sy = y + 2; // the slots' row inside it
            sunkFace(context, left + 18, y, right - left - 18, h, first, last);
            bool const chosen = notes.choices.contains(key);
            int const now = plan->choice->recipe[static_cast<size_t>(line.item)], rule = plan->table->choice.recipe[static_cast<size_t>(line.item)];
            bool const hot = over(left + 18, y, right - left - 18, h);
            if (hot) fill(context, left + 19, y, right - left - 20, h, palette::white, .2f);
            if (line.option < 0) {
                windowSlot(context, left + 22, sy + 1);
                labelScaled(context, left + 22, sy + 7, 18, translated("recipes.howTo.auto"), .5f, v::windowInk, Align::Center, false);
                if (!chosen) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                windowLabel(context, left + 44, sy + 6, right - left - 46, translated("recipes.howTo.autoNow", rule >= 0 ? recipeName(rule) : std::string()));
                if (took(left + 18, y, right - left - 18, h)) state::setChoice(key, {}), howOpen.clear();
            } else {
                int const number = recipeOptions(index, line.item)[static_cast<size_t>(line.option)];
                StepLine const one{line.item, number, 1, index.recipes[static_cast<size_t>(number)].count, 0, 0};
                float const after = stepSlots(left + 18, sy, one, false, false);
                if (chosen && now == number) frame(context, left + 19, y + 1, right - left - 20, h - 2, palette::accentDeep);
                windowLabel(context, after + 2, sy + 6, right - after - 2, recipeName(number));
                if (took(left + 18, y, right - left - 18, h)) state::setChoice(key, choiceKey(index.recipes[static_cast<size_t>(number)])), howOpen.clear();
            }
        }
        y += line.height;
    }
}
}
