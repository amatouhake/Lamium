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
struct Drag { HudElementId id; float grabX, grabY, w, h, startX, startY; bool moved = false; };
std::optional<HudElementId> selected;
std::optional<Drag> drag;
Boxes lastBoxes;
Panel lastPanel;
float screenW = 0, screenH = 0;
int panelFirst = 0;

enum class RowKind { Heading, Hint, Element, Option, Line };
struct Row {
    RowKind kind;
    std::string text;
    settings::Option const* option = nullptr;
    std::string line;
    HudElementId element = HudElementId::Info;
};
constexpr std::array<HudElementId, 4> listed{
    HudElementId::Info, HudElementId::Target, HudElementId::Status, HudElementId::Toast};
std::string_view elementName(HudElementId id) {
    switch (id) {
    case HudElementId::Info: return "feature.infoHud";
    case HudElementId::Target: return "feature.targetInfo";
    case HudElementId::Status: return "feature.automationStatus";
    default: return "hudEditor.toast";
    }
}
std::string optionName(settings::Option const& option) { return splitLabel(translated(option.label)).name; }
std::string optionValue(settings::Option const& option, settings::OptionValue const& value) {
    auto pattern = splitLabel(translated(option.label)).value;
    try {
        if (auto choice = std::get_if<settings::ChoiceValue>(&value)) return translated(choice->label);
        if (auto number = std::get_if<float>(&value)) return std::vformat(pattern, std::make_format_args(*number));
    } catch (std::exception const&) {}
    return {};
}
// The panel rows come from the same option definitions as the settings list.
std::vector<Row> buildRows(Settings const& value) {
    std::vector<Row> rows;
    if (!selected) {
        rows.push_back({RowKind::Hint, translated("hudEditor.hintDrag")});
        rows.push_back({RowKind::Hint, translated("hudEditor.hintKeys")});
        rows.push_back({RowKind::Heading, translated("hudEditor.elements")});
        for (auto id : listed) rows.push_back({RowKind::Element, translated(elementName(id)), nullptr, {}, id});
        return rows;
    }
    bool pinned = settings::hudElement(value, *selected).pinned;
    rows.push_back({RowKind::Hint, translated(pinned ? "hudEditor.hintPinned" : "hudEditor.hintAuto")});
    std::string prefix = "hud." + std::string(hudElementKey(*selected)) + ".";
    for (auto const& option : settings::options) {
        if (!option.id.starts_with(prefix)) continue;
        auto field = option.id.substr(prefix.size());
        if (field == "anchor") rows.push_back({RowKind::Heading, translated("hudEditor.placement")});
        if (field == "scale") rows.push_back({RowKind::Heading, translated("hudEditor.look")});
        rows.push_back({RowKind::Option, optionName(option), &option});
    }
    if (*selected == HudElementId::Info) {
        rows.push_back({RowKind::Heading, translated("hudEditor.lines")});
        for (auto const& id : value.information.lineOrder)
            if (auto option = settings::find("information." + id))
                rows.push_back({RowKind::Line, optionName(*option), option, id});
    }
    return rows;
}
HudElement& layoutElement(Settings::Hud& hud, HudElementId id) {
    switch (id) {
    case HudElementId::Info: return hud.info;
    case HudElementId::Target: return hud.target;
    case HudElementId::Status: return hud.status;
    default: return hud.toast;
    }
}
bool save(Settings const& value) { return Runtime::instance().save(value); }
void select(std::optional<HudElementId> id) {
    if (selected != id) panelFirst = 0;
    selected = id;
}
void nudge(float dx, float dy) {
    if (!selected) return;
    auto value = Runtime::instance().preferences();
    auto& element = settings::hudElement(value, *selected);
    element.dx += dx;
    element.dy += dy;
    value.normalize();
    save(value);
}
// Up/down triangles for the line order buttons, drawn from rectangles.
void triangle(MinecraftUIRenderContext& context, float x, float y, bool up, Rgb color) {
    for (int i = 0; i < 3; ++i) {
        float row = up ? y + i : y + 2 - i;
        fill(context, x + 2 - i, row, 1 + 2 * i, 1, color);
    }
}
void drawRow(MinecraftUIRenderContext& context, Panel const& p, Row const& row, float y, Settings const& value,
             bool hovered) {
    float x = p.x + Panel::pad, labelWidth = p.controlX() - x - 4;
    float textY = y + 2;
    switch (row.kind) {
    case RowKind::Heading:
        label(context, x, textY, p.w - 2 * Panel::pad, row.text, palette::accent);
        return;
    case RowKind::Hint:
        label(context, x, textY, p.w - 2 * Panel::pad, row.text, palette::faint);
        return;
    case RowKind::Element:
        rowBackground(context, p.x + 1, y, p.w - 2, Panel::rowHeight, false, hovered);
        label(context, x + 4, textY, p.w - 2 * Panel::pad - 4, row.text);
        chevron(context, p.x + p.w - Panel::pad - 6, y + 4, false);
        return;
    case RowKind::Option: {
        rowBackground(context, p.x + 1, y, p.w - 2, Panel::rowHeight, false, hovered);
        label(context, x, textY, labelWidth, row.text, palette::dim);
        auto current = row.option->read(value);
        float cx = p.controlX();
        if (auto on = std::get_if<bool>(&current)) {
            toggleSwitch(context, cx + Panel::controlWidth - switchWidth, y + (Panel::rowHeight - switchHeight) / 2, *on);
            return;
        }
        arrow(context, cx + 3, y + 4, true);
        arrow(context, cx + Panel::controlWidth - 7, y + 4, false);
        label(context, cx + Panel::arrowWidth, textY, Panel::controlWidth - 2 * Panel::arrowWidth,
              optionValue(*row.option, current), palette::text, Align::Center);
        return;
    }
    case RowKind::Line: {
        rowBackground(context, p.x + 1, y, p.w - 2, Panel::rowHeight, false, hovered);
        auto current = row.option->read(value);
        bool on = false;
        if (auto state = std::get_if<bool>(&current)) on = *state;
        toggleSwitch(context, x, y + (Panel::rowHeight - switchHeight) / 2, on);
        label(context, x + switchWidth + 5, textY, p.controlX() - x - switchWidth - 9, row.text,
              on ? palette::text : palette::faint);
        float cx = p.controlX();
        auto const& order = value.information.lineOrder;
        bool firstLine = !order.empty() && order.front() == row.line;
        bool lastLine = !order.empty() && order.back() == row.line;
        triangle(context, cx + 3, y + 6, true, firstLine ? palette::off : palette::dim);
        triangle(context, cx + Panel::controlWidth - 8, y + 6, false, lastLine ? palette::off : palette::dim);
        return;
    }
    }
}
void drawPanel(MinecraftUIRenderContext& context, Settings const& value, std::vector<Row> const& rows,
               float pointerX, float pointerY) {
    auto p = fitPanel(screenW, screenH, selected ? lastBoxes[static_cast<size_t>(*selected)] : std::nullopt,
                      static_cast<int>(rows.size()), panelFirst);
    panelFirst = p.first;
    lastPanel = p;
    panel(context, p.x, p.y, p.w, p.h, .88f);
    frame(context, p.x, p.y, p.w, p.h, palette::white, .14f);
    auto title = selected ? translated(elementName(*selected)) : translated("hudEditor.title");
    label(context, p.x + Panel::pad, p.y + 5, p.w - 2 * Panel::pad, title);
    fill(context, p.x, p.rowsTop() - 1, p.w, 1, palette::white, .14f);
    auto hover = hitPanel(p, pointerX, pointerY);
    for (int i = p.first; i < p.first + p.visible; ++i)
        drawRow(context, p, rows[i], p.rowY(i), value, hover.zone == PanelHit::Zone::Row && hover.index == i);
    if (p.count > p.visible && p.visible > 0) {
        float track = p.visible * Panel::rowHeight;
        float thumb = std::max(6.f, track * p.visible / p.count);
        float top = p.rowsTop() + (track - thumb) * p.first / std::max(1, p.count - p.visible);
        fill(context, p.x + p.w - 3, top, 2, thumb, palette::white, .3f);
    }
    fill(context, p.x, p.footerTop(), p.w, 1, palette::white, .14f);
    std::array<std::string, 2> buttons{translated("hudEditor.done"), selected ? translated("hudEditor.reset") : ""};
    for (int i = 0; i < 2; ++i) {
        if (buttons[i].empty()) continue;
        bool hot = hover.zone == PanelHit::Zone::Button && hover.index == i;
        float bx = p.buttonX(i), by = p.buttonY();
        if (hot) fill(context, bx, by, Panel::buttonWidth, Panel::buttonHeight, palette::white, .07f);
        frame(context, bx, by, Panel::buttonWidth, Panel::buttonHeight, i == 0 ? palette::accent : palette::keyEdge);
        label(context, bx, by + boxTextInset(), Panel::buttonWidth, buttons[i], hot ? palette::text : palette::dim,
              Align::Center);
    }
    context.flushText(0, std::nullopt);
}
void drawDots(MinecraftUIRenderContext& context, Point from, Point to, Rgb color) {
    for (auto const& dot : dashes(from, to)) fill(context, dot.x - .5f, dot.y - .5f, 1, 1, color);
}
}
void reset() {
    selected.reset();
    drag.reset();
    lastBoxes = {};
    panelFirst = 0;
}
void render(MinecraftUIRenderContext& context, float width, float height, float pointerX, float pointerY) {
    screenW = width;
    screenH = height;
    auto const value = Runtime::instance().preferences();
    information::HudPreview preview{value.hud};
    std::optional<Box> dragBoxNow;
    if (drag) {
        auto& element = layoutElement(preview.layout, drag->id);
        auto box = dragBox(pointerX, pointerY, drag->grabX, drag->grabY, drag->w, drag->h, width, height);
        if (std::abs(box.x - drag->startX) >= 1 || std::abs(box.y - drag->startY) >= 1) drag->moved = true;
        if (drag->moved) {
            auto resolved = resolveDrag(box.x, box.y, box.w, box.h, width, height, element);
            element.anchor = resolved.anchor;
            element.dx = box.x - (margin + (width - 2 * margin - box.w) * anchorFactors(resolved.anchor).x);
            element.dy = box.y - (margin + (height - 2 * margin - box.h) * anchorFactors(resolved.anchor).y);
            dragBoxNow = box;
        }
    }
    fill(context, 0, 0, width, height, Rgb{0, 0, 0}, .25f);
    // Anchor dots: faint everywhere, bright for the selected/dragged element's anchor.
    std::optional<HudElementId> focus = drag ? std::optional(drag->id) : selected;
    std::optional<Anchor> active;
    if (focus) active = layoutElement(preview.layout, *focus).anchor;
    for (int i = 0; i < 9; ++i) {
        auto anchor = static_cast<Anchor>(i);
        auto point = anchorPoint(anchor, width, height);
        bool on = active && *active == anchor;
        if (on) fill(context, point.x - 2, point.y - 2, 5, 5, palette::accent);
        else frame(context, point.x - 2, point.y - 2, 5, 5, palette::white, .35f);
    }
    lastBoxes = information::drawHud(context, width, height, value.information, &preview);
    // Outlines: every element faintly, the hovered one brighter, the selected one in the accent.
    auto hovered = drag ? std::nullopt : topmost(lastBoxes, pointerX, pointerY);
    if (lastPanel.contains(pointerX, pointerY)) hovered.reset();
    for (auto id : listed) {
        auto const& box = lastBoxes[static_cast<size_t>(id)];
        if (!box) continue;
        bool isFocus = focus && *focus == id;
        frame(context, box->x - 1, box->y - 1, box->w + 2, box->h + 2, isFocus ? palette::accent : palette::white,
              isFocus ? .9f : hovered && *hovered == id ? .5f : .2f);
    }
    if (focus && active) {
        if (auto const& box = lastBoxes[static_cast<size_t>(*focus)])
            drawDots(context, anchorPoint(*active, width, height), elementPoint(*active, *box), palette::accent);
    }
    context.flushText(0, std::nullopt);
    drawPanel(context, value, buildRows(value), pointerX, pointerY);
}
Result press(float x, float y) {
    if (lastPanel.contains(x, y)) {
        auto hit = hitPanel(lastPanel, x, y);
        auto value = Runtime::instance().preferences();
        if (hit.zone == PanelHit::Zone::Button) {
            if (hit.index == 0) return Result::Exit;
            if (hit.index == 1 && selected) {
                settings::hudElement(value, *selected) = defaultHudElement(*selected);
                save(value);
            }
            return Result::Stay;
        }
        if (hit.zone != PanelHit::Zone::Row) return Result::Stay;
        auto rows = buildRows(value);
        if (hit.index < 0 || hit.index >= static_cast<int>(rows.size())) return Result::Stay;
        auto const& row = rows[hit.index];
        if (row.kind == RowKind::Element) { select(row.element); return Result::Stay; }
        if (row.kind == RowKind::Option) {
            bool isToggle = std::holds_alternative<bool>(row.option->read(value));
            if (!isToggle && hit.part == 2) return Result::Stay;
            row.option->adjust(value, hit.part == -1 ? -1 : 1);
            save(value);
        } else if (row.kind == RowKind::Line) {
            if (hit.part == -1 || hit.part == 1) {
                auto order = information::moveLineOrder(value.information.lineOrder, row.line, hit.part);
                if (order == value.information.lineOrder) return Result::Stay;
                value.information.lineOrder = std::move(order);
            } else row.option->adjust(value, 1);
            save(value);
        }
        return Result::Stay;
    }
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
    if (!finished.moved || !(screenW > 0) || !(screenH > 0)) return;
    // The final box comes from the last rendered frame, like the preview.
    auto const& box = lastBoxes[static_cast<size_t>(finished.id)];
    if (!box) return;
    auto value = Runtime::instance().preferences();
    auto& element = settings::hudElement(value, finished.id);
    auto resolved = resolveDrag(box->x, box->y, box->w, box->h, screenW, screenH, element);
    element.anchor = resolved.anchor;
    element.dx = resolved.dx;
    element.dy = resolved.dy;
    save(value);
}
void wheel(int step, float x, float y) {
    if (lastPanel.contains(x, y)) panelFirst = std::max(0, panelFirst + step);
}
Result key(int key, bool shift) {
    float amount = shift ? 10.f : 1.f;
    switch (key) {
    case 0x1b:
        if (drag) { drag.reset(); return Result::Stay; }
        if (selected) { select(std::nullopt); return Result::Stay; }
        return Result::Exit;
    case 0x25: nudge(-amount, 0); break;
    case 0x27: nudge(amount, 0); break;
    case 0x26: nudge(0, -amount); break;
    case 0x28: nudge(0, amount); break;
    default: break;
    }
    return Result::Stay;
}
}
