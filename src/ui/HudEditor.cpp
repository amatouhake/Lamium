#include "ui/HudEditor.h"
#include "ui/HudEditorLayout.h"
#include "ui/Localization.h"
#include "ui/SettingsRows.h"
#include "ui/Widgets.h"
#include "app/Runtime.h"
#include "features/information/InfoHud.h"
#include "features/information/InfoLines.h"
#include "settings/Options.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace lamium::ui::hud_editor {
namespace {
// Everything clickable is laid out while drawing and hit-tested on the next
// press, so drawing and hit testing can never disagree.
enum class Command { Done, Reset, OpenSnap, OpenLines, Snap, ScaleDown, ScaleUp, Background, Shadow, LineSwitch, LineUp, LineDown };
struct Control { Box box; Command command; int arg = 0; std::string line; };
enum class Popover { None, Snap, Lines };
struct Drag { HudElementId id; float grabX, grabY, w, h, startX, startY; bool moved = false; std::optional<Drop> drop; };

std::optional<HudElementId> selected;
std::optional<Drag> drag;
std::optional<Spot> spot; // Toolbar position; kept while only the look changes.
Popover popover = Popover::None;
int linesFirst = 0;
Boxes lastBoxes;
std::vector<Control> controls;
std::optional<Box> popoverBox;
float screenW = 0, screenH = 0;
bool resetArmed = false; // Reset asks for a second press.

constexpr std::array<std::string_view, 9> growLabels{
    "grow.topLeft", "grow.topCenter", "grow.topRight", "grow.middleLeft", "grow.center",
    "grow.middleRight", "grow.bottomLeft", "grow.bottomCenter", "grow.bottomRight"};
std::string_view elementName(HudElementId id) {
    switch (id) {
    case HudElementId::Info: return "feature.infoHud";
    case HudElementId::Target: return "feature.targetInfo";
    case HudElementId::Status: return "feature.automationStatus";
    default: return "hudEditor.toast";
    }
}
HudElement& layoutElement(Settings::Hud& hud, HudElementId id) {
    switch (id) {
    case HudElementId::Info: return hud.info;
    case HudElementId::Target: return hud.target;
    case HudElementId::Status: return hud.status;
    default: return hud.toast;
    }
}
// Look controls reuse the option definitions the settings list used to show.
settings::Option const* lookOption(HudElementId id, std::string_view field) {
    return settings::find("hud." + std::string(hudElementKey(id)) + "." + std::string(field));
}
std::string optionText(settings::Option const& option, Settings const& value) {
    auto parts = splitLabel(translated(option.label));
    auto current = option.read(value);
    try {
        if (auto choice = std::get_if<settings::ChoiceValue>(&current))
            return parts.name + ": " + translated(choice->label);
        if (auto number = std::get_if<float>(&current)) return std::vformat(parts.value, std::make_format_args(*number));
    } catch (std::exception const&) {}
    return parts.name;
}
bool save(Settings const& value) { return Runtime::instance().save(value); }
void moveTo(HudElementId id, float x, float y, Box const& box) {
    auto value = Runtime::instance().preferences();
    auto& element = settings::hudElement(value, id);
    element = placeAt(element, x, y, box.w, box.h, screenW, screenH);
    save(value);
    spot.reset();
}
void button(MinecraftUIRenderContext& context, Box box, std::string text, bool hot, Rgb edge = palette::keyEdge,
            bool dropdown = false) {
    fill(context, box.x, box.y, box.w, box.h, palette::keyFill, .9f);
    if (hot) fill(context, box.x, box.y, box.w, box.h, palette::white, .08f);
    frame(context, box.x, box.y, box.w, box.h, edge);
    float textW = dropdown ? box.w - 9 : box.w;
    label(context, box.x, box.y + boxTextInset(), textW, std::move(text), hot ? palette::text : palette::dim, Align::Center);
    if (dropdown) chevron(context, box.x + box.w - 9, box.y + 3, true, hot ? palette::text : palette::dim);
}
bool hovering(Box const& box, float px, float py) { return box.contains(px, py); }
void triangle(MinecraftUIRenderContext& context, float x, float y, bool up, Rgb color) {
    for (int i = 0; i < 3; ++i) fill(context, x + 2 - i, up ? y + i : y + 2 - i, 1 + 2 * i, 1, color);
}

// ---- Toolbar ----
constexpr float barHeight = 15, itemHeight = 11, pad = 2, gap = 3;
Box drawToolbar(MinecraftUIRenderContext& context, Settings const& value, Box element, float px, float py) {
    auto id = *selected;
    auto background = lookOption(id, "background");
    auto shadow = lookOption(id, "shadow");
    auto const& current = settings::hudElement(value, id);
    // Measure first so the spot can be computed from the real width.
    std::string snapText = translated("hudEditor.snap");
    std::string scaleText = std::format("{}%", static_cast<int>(current.scale));
    std::string backgroundText = background ? optionText(*background, value) : std::string{};
    std::string shadowText = shadow ? splitLabel(translated(shadow->label)).name : std::string{};
    std::string linesText = translated("hudEditor.lines");
    float snapW = textWidth(context, snapText) + 17, scaleW = textWidth(context, "150%") + 24;
    float backgroundW = textWidth(context, backgroundText) + 8;
    float shadowW = textWidth(context, shadowText) + switchWidth + 10;
    float linesW = id == HudElementId::Info ? textWidth(context, linesText) + 17 : 0;
    float width = pad + snapW + gap + scaleW + gap + backgroundW + gap + shadowW + (linesW ? gap + linesW : 0) + pad;
    if (!spot || element.overlaps(Box{spot->x, spot->y, width, barHeight}))
        spot = toolbarSpot(element, width, barHeight, screenW, screenH);
    Box bar{spot->x, spot->y, width, barHeight};
    panel(context, bar.x, bar.y, bar.w, bar.h, .92f);
    frame(context, bar.x, bar.y, bar.w, bar.h, palette::white, .14f);
    float x = bar.x + pad, y = bar.y + (barHeight - itemHeight) / 2;
    auto add = [&](float w, Command command) {
        Box box{x, y, w, itemHeight};
        controls.push_back({box, command});
        x += w + gap;
        return box;
    };
    auto snap = add(snapW, Command::OpenSnap);
    button(context, snap, snapText, popover == Popover::Snap || hovering(snap, px, py), palette::keyEdge, true);
    // Scale: arrows at both ends of one field.
    Box scaleBox{x, y, scaleW, itemHeight};
    fill(context, scaleBox.x, scaleBox.y, scaleBox.w, scaleBox.h, palette::keyFill, .9f);
    frame(context, scaleBox.x, scaleBox.y, scaleBox.w, scaleBox.h, palette::keyEdge);
    controls.push_back({{scaleBox.x, y, 10, itemHeight}, Command::ScaleDown});
    controls.push_back({{scaleBox.x + scaleW - 10, y, 10, itemHeight}, Command::ScaleUp});
    arrow(context, scaleBox.x + 3, y + 3, true);
    arrow(context, scaleBox.x + scaleW - 7, y + 3, false);
    label(context, scaleBox.x + 10, y + boxTextInset(), scaleW - 20, scaleText, palette::text, Align::Center);
    x += scaleW + gap;
    if (background) {
        auto box = add(backgroundW, Command::Background);
        button(context, box, backgroundText, hovering(box, px, py));
    }
    if (shadow) {
        auto box = add(shadowW, Command::Shadow);
        bool on = std::get<bool>(shadow->read(value));
        if (hovering(box, px, py)) fill(context, box.x, box.y, box.w, box.h, palette::white, .08f);
        label(context, box.x + 3, y + boxTextInset(), box.w - switchWidth - 6, shadowText, palette::dim);
        toggleSwitch(context, box.x + box.w - switchWidth - 2, y + (itemHeight - switchHeight) / 2, on);
    }
    if (linesW) {
        auto box = add(linesW, Command::OpenLines);
        button(context, box, linesText, popover == Popover::Lines || hovering(box, px, py), palette::keyEdge, true);
    }
    popoverBox.reset();
    if (popover == Popover::Snap) {
        float cellW = 14, cellH = 9, cellGap = 2;
        std::string readout = translated("hudEditor.anchorReadout",
            translated(settings::anchorLabels[static_cast<size_t>(current.anchor)]),
            translated(growLabels[static_cast<size_t>(current.anchor)]));
        float w = std::max(3 * cellW + 2 * cellGap, textWidth(context, readout)) + 12;
        float h = 6 + 3 * cellH + 2 * cellGap + 4 + 12;
        auto at = popoverSpot(*spot, barHeight, w, h, screenW, screenH);
        popoverBox = Box{at.x, at.y, w, h};
        panel(context, at.x, at.y, w, h, .94f);
        frame(context, at.x, at.y, w, h, palette::white, .14f);
        for (int i = 0; i < 9; ++i) {
            Box cell{at.x + 6 + (i % 3) * (cellW + cellGap), at.y + 6 + (i / 3) * (cellH + cellGap), cellW, cellH};
            bool on = static_cast<int>(current.anchor) == i;
            fill(context, cell.x, cell.y, cell.w, cell.h, on ? palette::accent : palette::keyFill);
            frame(context, cell.x, cell.y, cell.w, cell.h, hovering(cell, px, py) ? palette::white : palette::keyEdge);
            controls.push_back({cell, Command::Snap, i});
        }
        label(context, at.x + 6, at.y + 6 + 3 * cellH + 2 * cellGap + 3, w - 12, readout, palette::faint);
    } else if (popover == Popover::Lines) {
        auto const& order = value.information.lineOrder;
        constexpr float rowH = 12;
        int visible = std::min<int>(10, static_cast<int>(order.size()));
        linesFirst = std::clamp(linesFirst, 0, std::max(0, static_cast<int>(order.size()) - visible));
        float w = 130, h = visible * rowH + 6;
        auto at = popoverSpot(*spot, barHeight, w, h, screenW, screenH);
        popoverBox = Box{at.x, at.y, w, h};
        panel(context, at.x, at.y, w, h, .94f);
        frame(context, at.x, at.y, w, h, palette::white, .14f);
        for (int i = 0; i < visible; ++i) {
            auto const& line = order[linesFirst + i];
            auto option = settings::find("information." + line);
            if (!option) continue;
            float ry = at.y + 3 + i * rowH;
            bool on = std::get<bool>(option->read(value));
            Box row{at.x + 2, ry, w - 4, rowH};
            if (hovering(row, px, py)) fill(context, row.x, row.y, row.w, row.h, palette::white, .06f);
            toggleSwitch(context, at.x + 5, ry + (rowH - switchHeight) / 2, on);
            label(context, at.x + 8 + switchWidth, ry + 1, w - switchWidth - 36, splitLabel(translated(option->label)).name,
                  on ? palette::text : palette::faint);
            bool firstLine = linesFirst + i == 0, lastLine = linesFirst + i + 1 == static_cast<int>(order.size());
            triangle(context, at.x + w - 22, ry + 4, true, firstLine ? palette::off : palette::dim);
            triangle(context, at.x + w - 11, ry + 4, false, lastLine ? palette::off : palette::dim);
            controls.push_back({{at.x + 2, ry, w - 30, rowH}, Command::LineSwitch, 0, line});
            controls.push_back({{at.x + w - 26, ry, 11, rowH}, Command::LineUp, 0, line});
            controls.push_back({{at.x + w - 15, ry, 13, rowH}, Command::LineDown, 0, line});
        }
    }
    return bar;
}
// Reset and Done sit in a bottom corner, clear of elements and the toolbar.
void drawActions(MinecraftUIRenderContext& context, std::optional<Box> toolbar, float px, float py) {
    std::string reset = translated(resetArmed ? "hudEditor.resetConfirm"
        : selected ? "hudEditor.resetElement" : "hudEditor.resetAll");
    std::string done = translated("hudEditor.done");
    float resetW = textWidth(context, reset) + 10, doneW = textWidth(context, done) + 10;
    float w = resetW + doneW + 3 * pad + 1, h = barHeight;
    auto placeAt = [&](float x) { return Box{x, screenH - h - hudInset, w, h}; };
    auto blocked = [&](Box const& bar) {
        if (toolbar && toolbar->overlaps(bar)) return true;
        for (auto const& box : lastBoxes) if (box && box->overlaps(bar)) return true;
        return false;
    };
    Box bar = placeAt(screenW - w - hudInset);
    if (blocked(bar)) bar = placeAt(hudInset);
    panel(context, bar.x, bar.y, bar.w, bar.h, .92f);
    frame(context, bar.x, bar.y, bar.w, bar.h, palette::white, .14f);
    float y = bar.y + (barHeight - itemHeight) / 2;
    Box resetBox{bar.x + pad, y, resetW, itemHeight};
    Box doneBox{resetBox.x + resetW + pad + 1, y, doneW, itemHeight};
    button(context, resetBox, reset, resetArmed || hovering(resetBox, px, py), resetArmed ? palette::warning : palette::keyEdge);
    button(context, doneBox, done, hovering(doneBox, px, py), palette::accent);
    controls.push_back({resetBox, Command::Reset});
    controls.push_back({doneBox, Command::Done});
}
void run(Control const& control) {
    auto value = Runtime::instance().preferences();
    if (control.command == Command::Reset) {
        if (!resetArmed) { resetArmed = true; return; }
        resetArmed = false;
        for (auto id : drawOrder)
            if (!selected || *selected == id) settings::hudElement(value, id) = defaultHudElement(id);
        if (!selected) value.information.lineOrder = information::defaultLineOrder();
        spot.reset();
        save(value);
        return;
    }
    resetArmed = false;
    if (control.command == Command::OpenSnap) { popover = popover == Popover::Snap ? Popover::None : Popover::Snap; return; }
    if (control.command == Command::OpenLines) { popover = popover == Popover::Lines ? Popover::None : Popover::Lines; return; }
    if (!selected) return;
    auto id = *selected;
    switch (control.command) {
    case Command::Snap: {
        auto& element = settings::hudElement(value, id);
        element = snapTo(element, static_cast<Anchor>(control.arg));
        spot.reset();
        break;
    }
    case Command::ScaleDown: case Command::ScaleUp:
        if (auto option = lookOption(id, "scale")) option->adjust(value, control.command == Command::ScaleUp ? 1 : -1);
        break;
    case Command::Background:
        if (auto option = lookOption(id, "background")) option->adjust(value, 1);
        break;
    case Command::Shadow:
        if (auto option = lookOption(id, "shadow")) option->adjust(value, 1);
        break;
    case Command::LineSwitch:
        if (auto option = settings::find("information." + control.line)) option->adjust(value, 1);
        break;
    case Command::LineUp: case Command::LineDown: {
        auto order = information::moveLineOrder(value.information.lineOrder, control.line,
                                                 control.command == Command::LineUp ? -1 : 1);
        if (order == value.information.lineOrder) return;
        value.information.lineOrder = std::move(order);
        break;
    }
    default: return;
    }
    save(value);
}
}
void reset() {
    selected.reset();
    drag.reset();
    spot.reset();
    popover = Popover::None;
    linesFirst = 0;
    resetArmed = false;
    lastBoxes = {};
    controls.clear();
    popoverBox.reset();
}
void select(std::optional<HudElementId> id) {
    if (selected != id) { spot.reset(); popover = Popover::None; linesFirst = 0; resetArmed = false; }
    selected = id;
}
void render(MinecraftUIRenderContext& context, float width, float height, float pointerX, float pointerY) {
    screenW = width;
    screenH = height;
    controls.clear();
    auto const value = Runtime::instance().preferences();
    information::HudPreview preview{value.hud};
    if (drag) {
        auto next = dragBox(pointerX, pointerY, drag->grabX, drag->grabY, drag->w, drag->h, width, height);
        if (std::abs(next.box.x - drag->startX) >= 1 || std::abs(next.box.y - drag->startY) >= 1) drag->moved = true;
        if (drag->moved) {
            drag->drop = next;
            auto& element = layoutElement(preview.layout, drag->id);
            element = placeAt(element, next.box.x, next.box.y, next.box.w, next.box.h, width, height);
        }
    }
    fill(context, 0, 0, width, height, Rgb{0, 0, 0}, .25f);
    std::optional<HudElementId> focus = drag ? std::optional(drag->id) : selected;
    std::optional<Anchor> active;
    if (focus) active = layoutElement(preview.layout, *focus).anchor;
    for (int i = 0; i < 9; ++i) {
        auto anchor = static_cast<Anchor>(i);
        auto point = anchorPoint(anchor, width, height);
        float x = std::clamp(point.x - 2, 1.f, width - 6), y = std::clamp(point.y - 2, 1.f, height - 6);
        if (active && *active == anchor) fill(context, x, y, 5, 5, palette::accent);
        else frame(context, x, y, 5, 5, palette::white, .35f);
    }
    lastBoxes = information::drawHud(context, width, height, value.information, &preview);
    if (drag && drag->drop) {
        if (drag->drop->lineX) fill(context, *drag->drop->lineX - .5f, 0, 1, height, palette::warning, .8f);
        if (drag->drop->lineY) fill(context, 0, *drag->drop->lineY - .5f, width, 1, palette::warning, .8f);
    }
    bool overUi = popoverBox && popoverBox->contains(pointerX, pointerY);
    auto hovered = drag || overUi ? std::nullopt : topmost(lastBoxes, pointerX, pointerY);
    for (auto id : drawOrder) {
        auto const& box = lastBoxes[static_cast<size_t>(id)];
        if (!box) continue;
        bool isFocus = focus && *focus == id;
        frame(context, box->x - 1, box->y - 1, box->w + 2, box->h + 2, isFocus ? palette::accent : palette::white,
              isFocus ? .9f : hovered && *hovered == id ? .5f : .2f);
    }
    if (focus && active) {
        if (auto const& box = lastBoxes[static_cast<size_t>(*focus)]) {
            for (auto const& dot : dashes(anchorPoint(*active, width, height), elementPoint(*active, *box)))
                fill(context, dot.x - .5f, dot.y - .5f, 1, 1, palette::accent);
            // Name tag above the element, or below it at the top of the screen.
            auto name = translated(elementName(*focus));
            float tagW = textWidth(context, name) + 6;
            float tagY = box->y - 13 >= 0 ? box->y - 13 : box->y + box->h + 2;
            fill(context, box->x - 1, tagY, tagW, 11, palette::accent);
            labelScaled(context, box->x + 2, tagY + boxTextInset(), tagW - 4, name, 1.f, palette::panel, Align::Left, false);
        }
    }
    context.flushText(0, std::nullopt);
    if (!drag || !drag->moved) {
        std::optional<Box> toolbar;
        if (selected)
            if (auto const& box = lastBoxes[static_cast<size_t>(*selected)]) {
                toolbar = drawToolbar(context, value, *box, pointerX, pointerY);
            }
        drawActions(context, toolbar, pointerX, pointerY);
    }
    context.flushText(0, std::nullopt);
}
Result press(float x, float y) {
    // Controls drawn last sit on top, so test them in reverse.
    for (auto it = controls.rbegin(); it != controls.rend(); ++it) {
        if (!it->box.contains(x, y)) continue;
        if (it->command == Command::Done) return Result::Exit;
        run(*it);
        return Result::Stay;
    }
    resetArmed = false;
    if (popoverBox && popoverBox->contains(x, y)) return Result::Stay;
    popover = Popover::None;
    auto id = topmost(lastBoxes, x, y);
    select(id);
    if (id) {
        auto const& box = *lastBoxes[static_cast<size_t>(*id)];
        drag = Drag{*id, x - box.x, y - box.y, box.w, box.h, box.x, box.y};
    }
    return Result::Stay;
}
void release() {
    if (!drag) return;
    auto finished = *drag;
    drag.reset();
    if (!finished.moved || !finished.drop || !(screenW > 0) || !(screenH > 0)) return;
    moveTo(finished.id, finished.drop->box.x, finished.drop->box.y, finished.drop->box);
}
void wheel(int step, float x, float y) {
    if (popover == Popover::Lines && popoverBox && popoverBox->contains(x, y)) linesFirst = std::max(0, linesFirst + step);
}
Result key(int key, bool shift) {
    float amount = shift ? 10.f : 1.f;
    float dx = 0, dy = 0;
    switch (key) {
    case 0x1b:
        if (drag) { drag.reset(); return Result::Stay; }
        if (resetArmed) { resetArmed = false; return Result::Stay; }
        if (popover != Popover::None) { popover = Popover::None; return Result::Stay; }
        if (selected) { select(std::nullopt); return Result::Stay; }
        return Result::Exit;
    case 0x25: dx = -amount; break;
    case 0x27: dx = amount; break;
    case 0x26: dy = -amount; break;
    case 0x28: dy = amount; break;
    default: return Result::Stay;
    }
    if (!selected) return Result::Stay;
    auto const& box = lastBoxes[static_cast<size_t>(*selected)];
    if (!box) return Result::Stay;
    float x = std::clamp(box->x + dx, 0.f, std::max(0.f, screenW - box->w));
    float y = std::clamp(box->y + dy, 0.f, std::max(0.f, screenH - box->h));
    moveTo(*selected, x, y, *box);
    return Result::Stay;
}
}
