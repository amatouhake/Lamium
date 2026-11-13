#include "ui/HudEditor.h"
#include "ui/HudEditorLayout.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/ScreenParts.h"
#include "ui/SettingsRows.h"
#include "ui/Widgets.h"
#include "app/Runtime.h"
#include "features/information/InfoHud.h"
#include "features/information/InfoLines.h"
#include "settings/Options.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <array>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lamium::ui::hud_editor {
namespace {
// Everything clickable is laid out while drawing and hit-tested on the next
// press, so drawing and hit testing can never disagree.
enum class Command { Done, Reset, ResetAll, OpenSnap, OpenLines, Snap, ScaleDown, ScaleUp, ScaleEdit, Background, Shadow, LineSwitch, LineUp, LineDown, Shown };
struct Control { Box box; Command command; int arg = 0; std::string line; };
enum class Popover { None, Snap, Lines };
// `under` is what lay under the press; `cycle` when the pressed element was
// already selected, so a click without a move goes to the one below it.
struct Drag {
    HudElementId id; float grabX, grabY, w, h, startX, startY;
    std::vector<HudElementId> under; bool cycle = false;
    bool moved = false; std::optional<Drop> drop;
};
// A scale handle being dragged: the element's anchor point stays put.
struct Resize { HudElementId id; Point anchor, handle; float scale; float start; };

std::optional<HudElementId> selected;
std::optional<Drag> drag;
std::optional<Resize> resize;
std::vector<Handle> handles; // The selected element's, as drawn.
bool editingScale = false;   // The toolbar's scale is being typed (screen::number()).
std::optional<Point> scaleField;
std::optional<Spot> spot; // Toolbar position; kept while only the look changes.
Popover popover = Popover::None;
int linesFirst = 0;
Boxes lastBoxes;
std::vector<Control> controls;
std::optional<Box> popoverBox;
// The lines list's scroll bar as drawn, and whether it is being dragged.
struct LinesTrack { float x, top, track; int count, visible; };
std::optional<LinesTrack> linesTrack;
bool scrolling = false;
float screenW = 0, screenH = 0;
bool resetArmed = false; // "Reset layout" asks for a second press.

constexpr std::array<std::string_view, 9> growLabels{
    "grow.topLeft", "grow.topCenter", "grow.topRight", "grow.middleLeft", "grow.center",
    "grow.middleRight", "grow.bottomLeft", "grow.bottomCenter", "grow.bottomRight"};
std::string_view elementName(HudElementId id) {
    switch (id) {
    case HudElementId::Info: return "feature.infoHud";
    case HudElementId::Target: return "feature.targetInfo";
    case HudElementId::Status: return "feature.automationStatus";
    case HudElementId::Magnification: return "hudEditor.magnification";
    case HudElementId::Durability: return "feature.durabilityHud";
    case HudElementId::Minimap: return "feature.minimap";
    case HudElementId::Schematic: return "feature.schematicHud";
    case HudElementId::PlayerList: return "feature.playerList";
    case HudElementId::Inventory: return "feature.inventoryHud";
    case HudElementId::UsedSlots: return "feature.usedSlots";
    case HudElementId::StatusEffects: return "feature.statusEffects";
    case HudElementId::Chat: return "chatPosition";
    case HudElementId::PaperDoll: return "paperDollPosition";
    case HudElementId::Scoreboard: return "scoreboardPosition";
    case HudElementId::BossBars: return "bossBarPosition";
    default: return "hudEditor.toast";
    }
}
HudElement& layoutElement(Settings::Hud& hud, HudElementId id) {
    switch (id) {
    case HudElementId::Info: return hud.info;
    case HudElementId::Target: return hud.target;
    case HudElementId::Status: return hud.status;
    case HudElementId::Magnification: return hud.magnification;
    case HudElementId::Durability: return hud.durability;
    case HudElementId::Minimap: return hud.minimap;
    case HudElementId::Schematic: return hud.schematic;
    case HudElementId::PlayerList: return hud.playerList;
    case HudElementId::Inventory: return hud.inventory;
    case HudElementId::UsedSlots: return hud.usedSlots;
    case HudElementId::StatusEffects: return hud.statusEffects;
    case HudElementId::Chat: return hud.chat;
    case HudElementId::PaperDoll: return hud.paperDoll;
    case HudElementId::Scoreboard: return hud.scoreboard;
    case HudElementId::BossBars: return hud.bossBars;
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
        if (auto flag = std::get_if<bool>(&current)) return parts.name + ": " + translated(*flag ? "on" : "off");
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
    fill(context, box.x, box.y, box.w, box.h, palette::keyFill);
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
// The toolbar, its popovers and the action bar are opaque: element previews
// (the chat sample) showed through them (L-221).
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
    std::string shadowText = shadow ? optionText(*shadow, value) : std::string{};
    std::string linesText = translated("hudEditor.lines");
    std::string resetText = translated("resetShort");
    float snapW = textWidth(context, snapText) + 17, scaleW = textWidth(context, "150%") + 24;
    float backgroundW = background ? textWidth(context, backgroundText) + 8 : 0;
    float shadowW = shadow ? textWidth(context, shadowText) + 8 : 0;
    float linesW = id == HudElementId::Info ? textWidth(context, linesText) + 17 : 0;
    // The element's reset stands apart from the look controls, behind a divider.
    float resetW = textWidth(context, resetText) + 8;
    float width = pad + snapW + (scaleW ? gap + scaleW : 0) + (backgroundW ? gap + backgroundW : 0)
        + (shadowW ? gap + shadowW : 0) + (linesW ? gap + linesW : 0) + gap + 1 + gap + resetW + pad;
    if (!spot || element.overlaps(Box{spot->x, spot->y, width, barHeight}))
        spot = toolbarSpot(element, width, barHeight, screenW, screenH);
    Box bar{spot->x, spot->y, width, barHeight};
    panel(context, bar.x, bar.y, bar.w, bar.h, 1.f);
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
    {
        // Scale: arrows at both ends of one field; a click on the value types it.
        Box scaleBox{x, y, scaleW, itemHeight};
        fill(context, scaleBox.x, scaleBox.y, scaleBox.w, scaleBox.h, palette::keyFill);
        frame(context, scaleBox.x, scaleBox.y, scaleBox.w, scaleBox.h, editingScale ? palette::accent : palette::keyEdge);
        controls.push_back({{scaleBox.x, y, 10, itemHeight}, Command::ScaleDown});
        controls.push_back({{scaleBox.x + scaleW - 10, y, 10, itemHeight}, Command::ScaleUp});
        controls.push_back({{scaleBox.x + 10, y, scaleW - 20, itemHeight}, Command::ScaleEdit});
        arrow(context, scaleBox.x + 3, y + 3, true);
        arrow(context, scaleBox.x + scaleW - 7, y + 3, false);
        if (editingScale) {
            bool valid = screen::number().parsed(hudScaleMin, hudScaleMax).has_value();
            drawEditField(context, scaleBox.x + 11, y, itemHeight, scaleW - 22, screen::number(), Align::Center,
                          valid ? palette::text : palette::warning);
            scaleField = Point{scaleBox.x + 11, y};
        } else label(context, scaleBox.x + 10, y + boxTextInset(), scaleW - 20, scaleText, palette::text, Align::Center);
        x += scaleW + gap;
    }
    if (background) {
        auto box = add(backgroundW, Command::Background);
        button(context, box, backgroundText, hovering(box, px, py));
    }
    if (shadow) {
        // The same "name: value" button as the background (L-321).
        auto box = add(shadowW, Command::Shadow);
        button(context, box, shadowText, hovering(box, px, py));
    }
    if (linesW) {
        auto box = add(linesW, Command::OpenLines);
        button(context, box, linesText, popover == Popover::Lines || hovering(box, px, py), palette::keyEdge, true);
    }
    fill(context, x, bar.y + 3, 1, barHeight - 6, palette::white, .14f);
    x += 1 + gap;
    auto resetBox = add(resetW, Command::Reset);
    button(context, resetBox, resetText, hovering(resetBox, px, py));
    popoverBox.reset();
    linesTrack.reset();
    if (popover == Popover::Snap) {
        float cellW = 14, cellH = 9, cellGap = 2;
        std::string readout = translated("hudEditor.anchorReadout",
            translated(settings::anchorLabels[static_cast<size_t>(current.anchor)]),
            translated(growLabels[static_cast<size_t>(current.anchor)]));
        float w = std::max(3 * cellW + 2 * cellGap, textWidth(context, readout)) + 12;
        float h = 6 + 3 * cellH + 2 * cellGap + 4 + 12;
        auto at = popoverSpot(*spot, barHeight, w, h, screenW, screenH);
        popoverBox = Box{at.x, at.y, w, h};
        panel(context, at.x, at.y, w, h, 1.f);
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
        // Rows take 130; the scroll bar has its own strip on the right.
        float w = 136;
        auto at = popoverSpot(*spot, barHeight, w, order.size() * rowH + 6, screenW, screenH);
        int visible = std::clamp(static_cast<int>((at.h - 6) / rowH), 0, static_cast<int>(order.size()));
        linesFirst = std::clamp(linesFirst, 0, std::max(0, static_cast<int>(order.size()) - visible));
        float h = visible * rowH + 6;
        if (!spot->below || at.y < spot->y) at.y = at.y + at.h - h; // Hug the toolbar when opening upward.
        // Dragging the bar: the track as drawn now (linesTrack is rebuilt below).
        if (scrolling)
            linesFirst = scrollFirstAt(py, at.y + 3, visible * rowH, static_cast<int>(order.size()), visible);
        popoverBox = Box{at.x, at.y, w, h};
        panel(context, at.x, at.y, w, h, 1.f);
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
            float rw = w - 6;
            triangle(context, at.x + rw - 22, ry + 4, true, firstLine ? palette::off : palette::dim);
            triangle(context, at.x + rw - 11, ry + 4, false, lastLine ? palette::off : palette::dim);
            controls.push_back({{at.x + 2, ry, rw - 30, rowH}, Command::LineSwitch, 0, line});
            controls.push_back({{at.x + rw - 26, ry, 11, rowH}, Command::LineUp, 0, line});
            controls.push_back({{at.x + rw - 15, ry, 13, rowH}, Command::LineDown, 0, line});
        }
        linesTrack.reset();
        if (visible < static_cast<int>(order.size()) && visible > 0) {
            int count = static_cast<int>(order.size());
            linesTrack = LinesTrack{at.x + w - 5, at.y + 3, visible * rowH, count, visible};
            auto thumb = scrollThumb(linesTrack->top, linesTrack->track, count, visible, linesFirst);
            fill(context, linesTrack->x, linesTrack->top, 3, linesTrack->track, palette::white, .08f);
            fill(context, linesTrack->x, thumb.top, 3, thumb.h, palette::white, scrolling ? .55f : .3f);
        }
    }
    return bar;
}
// "Reset layout" and "Done" in a small titled panel, shown only while nothing
// is selected: every corner may hold an element (L-321).
void drawIdlePanel(MinecraftUIRenderContext& context, float px, float py) {
    std::string title = translated("nav.hudLayout"), hint = translated("hudEditor.hint");
    std::string reset = translated(resetArmed ? "hudEditor.resetConfirm" : "hudEditor.resetAll");
    std::string done = translated("hudEditor.done");
    float resetW = textWidth(context, reset) + 10, doneW = textWidth(context, done) + 10;
    float buttons = resetW + gap + doneW;
    constexpr float rowH = 11, edge = 6;
    // The legend (L-334): the line is the state (dashed shows by situation,
    // the gray hatching is switched off), yellow is the game's own HUD.
    std::string situational = translated("hudEditor.situational"), hidden = translated("hudEditor.switchedOff");
    std::string game = translated("hudEditor.gameHud");
    constexpr float swatchW = 12, swatchH = 7, swatchGap = 4, pairGap = 16, legendTop = 4, legendBottom = 6;
    float legend = 2 * (swatchW + swatchGap) + textWidth(context, situational) + pairGap + textWidth(context, hidden);
    float gameLegend = swatchW + swatchGap + textWidth(context, game);
    float w = std::max({buttons, textWidth(context, title), textWidth(context, hint), legend, gameLegend}) + 2 * edge;
    float h = edge + 4 * rowH + legendTop + legendBottom + itemHeight + edge;
    Box box = idlePanel(w, h, screenW, screenH);
    panel(context, box.x, box.y, box.w, box.h, 1.f);
    frame(context, box.x, box.y, box.w, box.h, palette::white, .14f);
    label(context, box.x, box.y + edge, box.w, title, palette::text, Align::Center);
    label(context, box.x, box.y + edge + rowH, box.w, hint, palette::faint, Align::Center);
    {
        float lx = box.x + (box.w - legend) / 2, ly = box.y + edge + 2 * rowH + legendTop;
        float sy = ly + (rowH - swatchH) / 2;
        for (auto const& dash : dashedFrame({lx, sy, swatchW, swatchH}, 2, 1))
            fill(context, dash.x, dash.y, dash.horizontal ? dash.length : 1, dash.horizontal ? 1 : dash.length, palette::white, .6f);
        lx += swatchW + swatchGap;
        label(context, lx, ly, textWidth(context, situational) + 2, situational, palette::dim, Align::Left);
        lx += textWidth(context, situational) + pairGap;
        std::vector<ImageRect> dots;
        for (auto const& square : hatch({lx, sy, swatchW, swatchH}, 3, 40)) dots.push_back({square.x, square.y, square.w, square.h});
        fills(context, dots, palette::faint, .9f);
        frame(context, lx, sy, swatchW, swatchH, palette::faint, .9f);
        lx += swatchW + swatchGap;
        label(context, lx, ly, textWidth(context, hidden) + 2, hidden, palette::dim, Align::Left);
        lx = box.x + (box.w - gameLegend) / 2;
        ly += rowH;
        frame(context, lx, ly + (rowH - swatchH) / 2, swatchW, swatchH, palette::warning, .9f);
        lx += swatchW + swatchGap;
        label(context, lx, ly, textWidth(context, game) + 2, game, palette::dim, Align::Left);
    }
    float y = box.y + edge + 4 * rowH + legendTop + legendBottom, x = box.x + (box.w - buttons) / 2;
    Box resetBox{x, y, resetW, itemHeight}, doneBox{x + resetW + gap, y, doneW, itemHeight};
    button(context, resetBox, reset, resetArmed || hovering(resetBox, px, py), resetArmed ? palette::warning : palette::keyEdge);
    button(context, doneBox, done, hovering(doneBox, px, py), palette::accent);
    controls.push_back({resetBox, Command::ResetAll});
    controls.push_back({doneBox, Command::Done});
}
void resetElements(Settings& value, std::optional<HudElementId> only) {
    for (auto id : drawOrder)
        if (!only || *only == id) settings::hudElement(value, id) = defaultHudElement(id);
    // The Info HUD's own reset also restores its line order (L-221).
    if (!only || *only == HudElementId::Info) value.information.lineOrder = information::defaultLineOrder();
    linesFirst = 0;
    spot.reset();
}
void run(Control const& control) {
    auto value = Runtime::instance().preferences();
    if (control.command == Command::ResetAll) {
        if (!resetArmed) { resetArmed = true; return; }
        resetArmed = false;
        resetElements(value, std::nullopt);
        save(value);
        return;
    }
    resetArmed = false;
    if (control.command == Command::OpenSnap) { popover = popover == Popover::Snap ? Popover::None : Popover::Snap; return; }
    if (control.command == Command::OpenLines) { popover = popover == Popover::Lines ? Popover::None : Popover::Lines; return; }
    // The switch on the selected name tag: the element's own switch (L-334).
    if (control.command == Command::Shown) {
        if (auto* field = settings::hudShownField(value, static_cast<HudElementId>(control.arg))) {
            *field = !*field;
            save(value);
        }
        return;
    }
    if (!selected) return;
    auto id = *selected;
    switch (control.command) {
    // Applied at once, like the binding row's Reset.
    case Command::Reset: resetElements(value, id); break;
    case Command::ScaleEdit:
        editingScale = true;
        screen::number().begin(settings::hudElement(value, id).scale);
        return;
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
    resize.reset();
    handles.clear();
    editingScale = false;
    scaleField.reset();
    spot.reset();
    popover = Popover::None;
    linesFirst = 0;
    resetArmed = false;
    lastBoxes = {};
    controls.clear();
    popoverBox.reset();
    linesTrack.reset();
    scrolling = false;
}
void select(std::optional<HudElementId> id) {
    if (selected != id) { spot.reset(); popover = Popover::None; linesFirst = 0; resetArmed = false; editingScale = false; }
    selected = id;
}
void render(MinecraftUIRenderContext& context, float width, float height, float pointerX, float pointerY) {
    screenW = width;
    screenH = height;
    controls.clear();
    auto const value = Runtime::instance().preferences();
    information::HudPreview preview{value.hud};
    // Game parts left Standard are not here at all (L-334).
    if (selected && !settings::hudInEditor(value, *selected)) select(std::nullopt);
    for (auto id : drawOrder)
        if (!settings::hudInEditor(value, id)) preview.hidden[static_cast<size_t>(id)] = true;
    scaleField.reset();
    if (resize) {
        resize->scale = dragScale(resize->start, resize->anchor, resize->handle, {pointerX, pointerY});
        layoutElement(preview.layout, resize->id).scale = resize->scale;
    }
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
    std::optional<HudElementId> focus = drag ? std::optional(drag->id) : resize ? std::optional(resize->id) : selected;
    // Elements that appear only by situation are drawn faint (half) with a
    // dashed frame and their name; switched-off ones (L-334) are a gray hatched
    // box with their name and no contents, faint contents only while
    // selected. Both are drawn in full while pointed at or selected.
    bool overUi = popoverBox && popoverBox->contains(pointerX, pointerY);
    auto hovered = drag || resize || overUi ? std::nullopt : topmost(lastBoxes, pointerX, pointerY);
    // The selected element keeps the pointer even under another one, as it
    // keeps a press: what lies below shows no name over it.
    if (hovered && focus)
        if (auto const& box = lastBoxes[static_cast<size_t>(*focus)]; box && box->contains(pointerX, pointerY)) hovered = focus;
    std::array<bool, std::tuple_size_v<Boxes>> off{};
    for (auto id : drawOrder) {
        auto shown = settings::hudShown(value, id);
        off[static_cast<size_t>(id)] = shown && !*shown;
    }
    auto isOff = [&](HudElementId id) { return off[static_cast<size_t>(id)]; };
    auto framed = [&](HudElementId id) { return hudConditional(id) && !isOff(id) && focus != id && hovered != id; };
    constexpr float situationalOpacity = .5f, offOpacity = .3f;
    for (auto id : drawOrder) {
        auto& opacity = preview.opacity[static_cast<size_t>(id)];
        if (isOff(id)) opacity = focus == id ? offOpacity : 0.f;
        else if (framed(id)) opacity = situationalOpacity;
    }
    std::optional<Anchor> active;
    if (focus) active = layoutElement(preview.layout, *focus).anchor;
    for (int i = 0; i < 9; ++i) {
        auto anchor = static_cast<Anchor>(i);
        auto point = anchorPoint(anchor, width, height);
        float x = std::clamp(point.x - 2, 1.f, width - 6), y = std::clamp(point.y - 2, 1.f, height - 6);
        if (active && *active == anchor) fill(context, x, y, 5, 5, palette::accent);
        else frame(context, x, y, 5, 5, palette::white, .35f);
    }
    // The game's hotbar where it sits in play, to place elements beside it;
    // only a picture, not an element of the layout.
    auto hotbar = information::editorHotbar(width, height);
    float unit = hotbar.h / 22;
    images(context, "textures/ui/hotbar_start_cap", {{hotbar.x, hotbar.y, unit, hotbar.h}}, .6f);
    std::vector<ImageRect> cells;
    for (int i = 0; i < 9; ++i) cells.push_back({hotbar.x + (1 + 20 * i) * unit, hotbar.y, 20 * unit, hotbar.h});
    images(context, "textures/ui/hotbar_0", cells, .6f);
    images(context, "textures/ui/hotbar_end_cap", {{hotbar.x + 181 * unit, hotbar.y, unit, hotbar.h}}, .6f);
    // The focused element is drawn in a second pass, on top of the others,
    // so it can be read where it overlaps them.
    std::optional<Box> behind;
    if (focus) {
        if (auto const& box = lastBoxes[static_cast<size_t>(*focus)])
            for (auto id : drawOrder)
                if (auto const& other = lastBoxes[static_cast<size_t>(id)]; other && id != *focus && !framed(id) && !isOff(id) && box->overlaps(*other))
                    behind = *box;
        preview.hidden[static_cast<size_t>(*focus)] = true;
    }
    auto boxes = information::drawHud(context, width, height, value.information, &preview);
    // The elements' text first: what follows goes over it.
    context.flushText(0, std::nullopt);
    if (focus) {
        // Only where it lies on another element: a darker ground sets it apart.
        if (behind) fill(context, behind->x, behind->y, behind->w, behind->h, Rgb{0, 0, 0}, .45f);
        preview.hidden.fill(true);
        preview.hidden[static_cast<size_t>(*focus)] = !settings::hudInEditor(value, *focus);
        auto front = information::drawHud(context, width, height, value.information, &preview);
        boxes[static_cast<size_t>(*focus)] = front[static_cast<size_t>(*focus)];
        context.flushText(0, std::nullopt);
    }
    lastBoxes = boxes;
    if (drag && drag->drop) {
        if (drag->drop->lineX) fill(context, *drag->drop->lineX - .5f, 0, 1, height, palette::warning, .8f);
        if (drag->drop->lineY) fill(context, 0, *drag->drop->lineY - .5f, width, 1, palette::warning, .8f);
    }
    // The focused element's tag takes its place first, then the pointed-at
    // one's; the others keep clear of them, of each other and of the
    // elements drawn in full. A tag hangs from the element's anchored side.
    // The focused tag carries the show/hide switch (the settings screen's)
    // joined on the side away from the anchor, so the name never moves.
    struct Tag { Box box; std::string name; bool focus, off; };
    std::vector<Tag> tags;
    std::vector<Box> taken;
    std::optional<Box> focusTag;
    std::optional<std::pair<Box, bool>> focusSwitch;
    auto rightSide = [&](HudElementId id) { return anchorFactors(layoutElement(preview.layout, id).anchor).x == 1; };
    if (focus)
        if (auto const& box = lastBoxes[static_cast<size_t>(*focus)]) {
            auto name = translated(elementName(*focus));
            if (resize) name += std::format(" {}%", static_cast<int>(resize->scale));
            // The tag is placed with the switch's room, so both keep clear of
            // others; the name keeps the end at the element's corner and the
            // switch takes the end toward the element's middle.
            auto shown = settings::hudShown(value, *focus);
            constexpr float switchRoom = switchWidth + 4;
            float nameW = textWidth(context, name) + 6;
            auto whole = nameTag(*box, nameW + (shown ? switchRoom : 0), width, height, {}, rightSide(*focus));
            // The corner the tag lines up with, not its middle: a tag wider
            // than the element still hangs from one corner.
            bool fromRight = std::abs(whole.x + whole.w - (box->x + box->w + 1)) < std::abs(whole.x - (box->x - 1));
            focusTag = Box{fromRight ? whole.x + whole.w - nameW : whole.x, whole.y, nameW, whole.h};
            tags.push_back({*focusTag, std::move(name), true, isOff(*focus)});
            if (shown) focusSwitch = std::pair{Box{fromRight ? whole.x : whole.x + nameW, whole.y, switchRoom, whole.h}, *shown};
        }
    if (focusTag) taken.push_back(*focusTag);
    if (focusSwitch) taken.push_back(focusSwitch->first);
    // No other name lies on the selected element.
    if (focus)
        if (auto const& box = lastBoxes[static_cast<size_t>(*focus)]) taken.push_back(*box);
    if (hovered && hovered != focus && !drag && !resize)
        if (auto const& box = lastBoxes[static_cast<size_t>(*hovered)]) {
            auto name = translated(elementName(*hovered));
            auto place = nameTag(*box, textWidth(context, name) + 6, width, height, taken, rightSide(*hovered));
            taken.push_back(place);
            tags.push_back({place, std::move(name), false, isOff(*hovered)});
        }
    for (auto id : drawOrder)
        if (auto const& box = lastBoxes[static_cast<size_t>(id)]; box && !isOff(id)) taken.push_back(*box);
    auto watched = [&](Box const& tag) {
        for (auto id : {focus, hovered})
            if (id)
                if (auto const& box = lastBoxes[static_cast<size_t>(*id)]; box && tag.overlaps(*box)) return true;
        return false;
    };
    auto addTag = [&](Box const& element, HudElementId id) {
        auto name = translated(elementName(id));
        auto box = nameTag(element, textWidth(context, name) + 6, width, height, taken, rightSide(id));
        if (watched(box)) return;
        taken.push_back(box);
        tags.push_back({box, std::move(name), false, isOff(id)});
    };
    for (auto id : drawOrder) {
        auto const& box = lastBoxes[static_cast<size_t>(id)];
        if (!box) continue;
        bool isFocus = focus && *focus == id, isHovered = hovered && *hovered == id;
        float opacity = isFocus ? .9f : isHovered ? .6f : framed(id) ? .35f : .2f;
        Box outer{box->x - 1, box->y - 1, box->w + 2, box->h + 2};
        if (isOff(id)) {
            // Switched off: a dark veil like a disabled settings row, gray
            // hatching, and a quieter frame than a shown element's.
            fill(context, box->x, box->y, box->w, box->h, Rgb{0, 0, 0}, .35f);
            std::vector<ImageRect> lines;
            for (auto const& square : hatch(*box)) lines.push_back({square.x, square.y, square.w, square.h});
            fills(context, lines, palette::faint, .6f);
            // The hatching is the state, the frame's color who draws it.
            frame(context, outer.x, outer.y, outer.w, outer.h,
                  isFocus ? palette::accent : hudGameDrawn(id) ? palette::warning : palette::white, isFocus ? .9f : isHovered ? .6f : .35f);
            if (!isFocus && !isHovered) addTag(*box, id);
            continue;
        }
        // The game's own elements stand apart from Lamium's by their frame color.
        if (!framed(id)) {
            frame(context, outer.x, outer.y, outer.w, outer.h,
                  isFocus ? palette::accent : hudGameDrawn(id) ? palette::warning : palette::white, hudGameDrawn(id) && !isFocus ? opacity + .25f : opacity);
            continue;
        }
        for (auto const& dash : dashedFrame(outer))
            fill(context, dash.x, dash.y, dash.horizontal ? dash.length : 1, dash.horizontal ? 1 : dash.length,
                 hudGameDrawn(id) ? palette::warning : palette::white, hudGameDrawn(id) ? opacity + .4f : opacity + .15f);
        // Its faint contents say what it is: named only when pointed at or
        // selected, as a shown element is. Only a hidden one keeps its name.
    }
    if (focus && active) {
        if (auto const& box = lastBoxes[static_cast<size_t>(*focus)]) {
            for (auto const& dot : dashes(anchorPoint(*active, width, height), elementPoint(*active, *box)))
                fill(context, dot.x - .5f, dot.y - .5f, 1, 1, palette::accent);
        }
    }
    // After every frame, so a tag is never crossed by another frame's dashes:
    // the same tag for every named element, in the accent color when focused
    // and a darker gray when switched off.
    for (auto const& tag : tags) {
        fill(context, tag.box.x, tag.box.y, tag.box.w, tag.box.h,
             tag.focus ? palette::accent : tag.off ? palette::faint : palette::dim, tag.focus || tag.off ? 1.f : .75f);
        labelScaled(context, tag.box.x + 3, tag.box.y + boxTextInset(), tag.box.w - 4, tag.name, 1.f, palette::panel, Align::Left, false);
    }
    if (focusSwitch && focus) {
        auto const& [box, shown] = *focusSwitch;
        fill(context, box.x, box.y, box.w, box.h, palette::accent);
        toggleSwitch(context, box.x + 2, box.y + (box.h - switchHeight) / 2, shown);
        controls.push_back({box, Command::Shown, static_cast<int>(*focus)});
    }
    // Scale handles on the corners away from the anchor; the chat is placed only.
    handles.clear();
    if (selected && active && !(drag && drag->moved))
        if (auto const& box = lastBoxes[static_cast<size_t>(*selected)]) {
            handles = scaleHandles(*active, *box, width, height);
            for (auto const& handle : handles) {
                fill(context, handle.box.x, handle.box.y, handle.box.w, handle.box.h, palette::white);
                frame(context, handle.box.x, handle.box.y, handle.box.w, handle.box.h, palette::panel);
            }
        }
    context.flushText(0, std::nullopt);
    if (selected && !resize && !(drag && drag->moved)) {
        if (auto const& box = lastBoxes[static_cast<size_t>(*selected)]) {
            // Clear of the name tag and its switch, above or below the element.
            Box clear = *box;
            auto join = [&](Box const& other) {
                float top = std::min(clear.y, other.y), bottom = std::max(clear.y + clear.h, other.y + other.h);
                clear.y = top;
                clear.h = bottom - top;
            };
            if (focusTag) join(*focusTag);
            if (focusSwitch) join(focusSwitch->first);
            drawToolbar(context, value, clear, pointerX, pointerY);
        }
    } else if (!selected && !drag) drawIdlePanel(context, pointerX, pointerY);
    context.flushText(0, std::nullopt);
}
Result press(float x, float y) {
    // Controls drawn last sit on top, so test them in reverse.
    for (auto it = controls.rbegin(); it != controls.rend(); ++it) {
        if (!it->box.contains(x, y)) continue;
        if (it->command == Command::ScaleEdit && editingScale) return Result::Stay;
        screen::finishEditing();
        if (it->command == Command::Done) return Result::Exit;
        run(*it);
        return Result::Stay;
    }
    screen::finishEditing();
    resetArmed = false;
    if (popover == Popover::Lines && linesTrack && x >= linesTrack->x - 2 && x < linesTrack->x + 5
        && y >= linesTrack->top && y < linesTrack->top + linesTrack->track) {
        scrolling = true;
        linesFirst = scrollFirstAt(y, linesTrack->top, linesTrack->track, linesTrack->count, linesTrack->visible);
        return Result::Stay;
    }
    if (popoverBox && popoverBox->contains(x, y)) return Result::Stay;
    popover = Popover::None;
    if (selected)
        if (auto const& box = lastBoxes[static_cast<size_t>(*selected)])
            for (auto const& handle : handles) {
                if (!grabs(handle, x, y)) continue;
                auto const& element = settings::hudElement(Runtime::instance().preferences(), *selected);
                resize = Resize{*selected, elementPoint(element.anchor, *box), handle.corner, element.scale, element.scale};
                return Result::Stay;
            }
    // The selected element keeps the press even under another one, so it can
    // be dragged; a click without a move then goes to the element below.
    auto under = stackAt(lastBoxes, x, y);
    bool held = selected && std::find(under.begin(), under.end(), *selected) != under.end();
    auto id = held ? selected : under.empty() ? std::nullopt : std::optional(under.front());
    select(id);
    if (id) {
        auto const& box = *lastBoxes[static_cast<size_t>(*id)];
        drag = Drag{*id, x - box.x, y - box.y, box.w, box.h, box.x, box.y, std::move(under), held};
    }
    return Result::Stay;
}
void release() {
    scrolling = false;
    if (auto finished = std::exchange(resize, std::nullopt)) {
        auto value = Runtime::instance().preferences();
        auto& element = settings::hudElement(value, finished->id);
        if (element.scale == finished->scale) return;
        element.scale = finished->scale;
        save(value);
        spot.reset();
        return;
    }
    if (!drag) return;
    auto finished = *drag;
    drag.reset();
    if (!finished.moved) {
        if (finished.cycle && finished.under.size() > 1) select(nextBelow(finished.under, finished.id));
        return;
    }
    if (!finished.drop || !(screenW > 0) || !(screenH > 0)) return;
    moveTo(finished.id, finished.drop->box.x, finished.drop->box.y, finished.drop->box);
}
void wheel(int step, float x, float y) {
    if (popover == Popover::Lines && popoverBox && popoverBox->contains(x, y)) linesFirst = std::max(0, linesFirst + step);
}
bool editingNumber() { return editingScale && selected.has_value(); }
void applyNumber() {
    if (!editingNumber()) return;
    auto parsed = screen::number().parsed(hudScaleMin, hudScaleMax);
    if (!parsed) return;
    auto value = Runtime::instance().preferences();
    auto& element = settings::hudElement(value, *selected);
    float scale = std::round(*parsed);
    if (element.scale == scale) return;
    element.scale = scale;
    save(value);
}
void endEditing() { editingScale = false; }
void editKey(int key) {
    switch (key) {
    case 0x08: if (screen::number().backspace()) screen::numberTyped(); break;
    case 0x41: if (screen::heldCtrl()) screen::number().selectAll(); break;
    }
}
std::optional<Point> caret() { return editingNumber() ? scaleField : std::nullopt; }
Result key(int key, bool shift) {
    if (editingScale) {
        if (key == 0x1b || key == 0x0d || key == 0x09) screen::finishEditing();
        return Result::Stay;
    }
    float amount = shift ? 10.f : 1.f;
    float dx = 0, dy = 0;
    switch (key) {
    case 0x1b:
        if (resize) { resize.reset(); return Result::Stay; }
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
