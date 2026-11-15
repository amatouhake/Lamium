#include "ui/ListViewWidgets.h"
#include "ui/ItemAmount.h"
#include "ui/Localization.h"
#include "ui/ScreenParts.h"
#include "ui/TextFit.h"
#include <vector>
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace lamium::ui {
namespace {
void box(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool hovered, Rgb fillColor, Rgb edge) {
    fill(context,x,y,w,h,fillColor);
    if (hovered) fill(context,x,y,w,h,palette::white,.1f);
    frame(context,x,y,w,h,edge);
}
void centered(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text, Rgb color) {
    label(context,x,y+(h-10)/2+boxTextInset()-1,w,std::move(text),color,Align::Center);
}
}
Rgb drawButtonBox(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool hovered, ButtonKind kind,
                  bool enabled) {
    bool primary = enabled && kind == ButtonKind::Primary;
    box(context,x,y,w,h,enabled && hovered,primary ? palette::accentDeep : palette::keyFill,primary ? palette::accent : palette::keyEdge);
    return enabled ? palette::text : palette::faint;
}
void drawButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                bool hovered, ButtonKind kind, bool enabled) {
    Rgb color = drawButtonBox(context,x,y,w,h,hovered,kind,enabled);
    centered(context,x,y,w,h,std::move(text),color);
}
void drawDangerButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                      bool hovered, bool armed) {
    bool red = armed || hovered;
    box(context,x,y,w,h,hovered,armed ? palette::danger : palette::keyFill,red ? palette::dangerEdge : palette::keyEdge);
    centered(context,x,y,w,h,std::move(text),red && !armed ? palette::dangerText : palette::text);
}
void drawListHeadings(MinecraftUIRenderContext& context, ListColumns const& c, float top, std::string_view ownKey) {
    auto shown = translated("shape.columnShown");
    float shownW = std::max(c.shownWidth + 12, textWidth(context, shown));
    float limit = c.headingLimit(shownW);
    auto right = [&](float columnEnd, std::string const& text) {
        float end = std::min(columnEnd, limit), w = textWidth(context, text);
        label(context,end-w,top+2,w,text,palette::faint);
        return end - w;
    };
    float nameEnd = c.nameX + c.nameWidth;
    if (c.hasOwn() && !ownKey.empty()) nameEnd = std::min(nameEnd, right(c.ownX + c.ownWidth, translated(ownKey)) - ListColumns::gap);
    if (c.hasDistance()) nameEnd = std::min(nameEnd, right(c.distanceX + c.distanceWidth, translated("waypoint.columnDistance")) - ListColumns::gap);
    label(context,c.nameX,top+2,nameEnd-c.nameX,translated("shape.columnName"),palette::faint);
    if (c.shownWidth > 0) label(context,c.shownHeadingX(shownW),top+2,shownW,shown,palette::faint,Align::Center);
}
std::string listDistanceText(int x, int z, int dimension) {
    if (dimension != screen::playerDimension()) return screen::dimensionName(dimension);
    return translated("waypoint.meters", screen::horizontalDistanceTo(x, z));
}
std::string listDistanceText(int x, int y, int z, int dimension) {
    if (dimension != screen::playerDimension()) return screen::dimensionName(dimension);
    return translated("waypoint.meters", screen::distanceTo(x, y, z));
}
void drawFootprintGlyph(MinecraftUIRenderContext& context, float x, float cy, Rgb color, float alpha) {
    fill(context, x, cy - 3, 7, 6, color, .16f * alpha);
    frame(context, x, cy - 3, 7, 6, color, alpha);
    for (float dx : {-1.f, 6.f})
        for (float dy : {-4.f, 2.f}) fill(context, x + dx, cy + dy, 2, 2, color, alpha);
}
void drawKindSquare(MinecraftUIRenderContext& context, float x, float cy, Rgb color, bool hollow, float size) {
    if (hollow) frame(context, x, cy - size / 2, size, size, color);
    else fill(context, x, cy - size / 2, size, size, color);
}
namespace {
constexpr float tagText = .8f;
}
float tagWidth(MinecraftUIRenderContext& context, std::string_view text) { return textWidthScaled(context, text, tagText) + 6; }
float drawTag(MinecraftUIRenderContext& context, float x, float y, std::string text, float maxWidth) {
    float w = std::min(tagWidth(context, text), maxWidth);
    if (w <= 6) return 0;
    frame(context, x, y, w, 10, palette::dim, .8f);
    labelScaled(context, x + 3, y + (10 - 8 * tagText) / 2 - 1 + boxTextInset(), w - 4, std::move(text), tagText, palette::dim);
    return w;
}
void drawSelectionPane(MinecraftUIRenderContext& context, SelectionPaneLayout const& pane, size_t count,
                       std::vector<SelectedName> const& names, std::function<void(size_t, float, float)> const& drawMark,
                       std::vector<BulkAction> const& actions, BulkArm const& arm, float pointerX, float pointerY,
                       size_t shown, float ownWidth, std::vector<bool> const& enabled) {
    label(context,pane.x,pane.y+3,pane.countW > 0 ? pane.countW : pane.w,translated("list.selectedCount", count));
    shown = std::min(shown, count);
    if (pane.summary) label(context,pane.x,pane.summaryY+2,pane.w,translated("list.shownHidden", shown, count - shown),palette::dim);
    auto const c = listColumns(pane.x - ListColumns::pad, pane.w + 2 * ListColumns::pad, ownWidth, 0);
    float y = pane.linesTop;
    for (size_t i = 0; i < names.size() && i < static_cast<size_t>(pane.fit.names); ++i, y += pane.lineHeight) {
        auto const& line = names[i];
        Rgb ink = line.faded ? palette::faint : palette::text;
        if (drawMark) drawMark(i, c.markX + 4, y + pane.lineHeight / 2);
        label(context,c.nameX,y+2,c.nameWidth,line.name,ink);
        if (c.hasDistance()) label(context,c.distanceX,y+2,c.distanceWidth,line.distance,palette::dim,Align::Right);
        if (c.hasOwn()) label(context,c.ownX,y+2,c.ownWidth,line.own,line.faded ? palette::faint : palette::dim,Align::Right);
    }
    if (pane.fit.more > 0) label(context,c.nameX,y+2,pane.w-(c.nameX-pane.x),translated("list.moreSelected", pane.fit.more),palette::faint);
    auto text = [&](BulkAction action) {
        switch (action) {
        case BulkAction::Show: return translated("list.showSelected");
        case BulkAction::Hide: return translated("list.hideSelected");
        case BulkAction::ShowInWorld: return translated("schematic.showInWorld");
        case BulkAction::IgnorePlaces: return translated("schematic.ignore.placeShort");
        case BulkAction::Unignore: return translated("schematic.ignore.stopShort");
        default: return std::string{};
        }
    };
    for (size_t i = 0; i < actions.size() && i < pane.buttonX.size(); ++i) {
        bool on = i >= enabled.size() || enabled[i];
        if (!on) {
            drawButton(context,pane.buttonX[i],pane.buttonsY,pane.buttonW[i],pane.buttonH,text(actions[i]),false,ButtonKind::Plain,false);
            continue;
        }
        bool over = pane.at(pointerX, pointerY) == static_cast<int>(i);
        bool armed = arm.armed == actions[i];
        if (irreversible(actions[i]))
            drawDangerButton(context,pane.buttonX[i],pane.buttonsY,pane.buttonW[i],pane.buttonH,
                armedLabel(context,bulkDeleteLabel(count,false),bulkDeleteLabel(count,true),armed,pane.buttonW[i]),over,armed);
        else drawButton(context,pane.buttonX[i],pane.buttonsY,pane.buttonW[i],pane.buttonH,text(actions[i]),over);
    }
}
std::string bulkDeleteLabel(size_t count, bool armed) {
    return armed ? confirmDelete(static_cast<int>(count)) : translated("list.deleteSelected");
}
std::string armedLabel(MinecraftUIRenderContext& context, std::string label, std::string confirm, bool armed, float width) {
    if (!armed) return label;
    // Inside the button's 1-unit edges.
    return armedText(label, confirm, translated("confirm.short"), armed, width - 2,
        [&](std::string_view text) { return textWidth(context, text); });
}
std::string armedLabel(std::string label, std::string confirm, bool armed) { return armed ? confirm : label; }
float armedWidth(MinecraftUIRenderContext& context, std::string_view label, std::string_view confirm, float padding) {
    return armedTextWidth(label, confirm, [&](std::string_view text) { return textWidth(context, text); }) + padding;
}
std::string confirmDelete(std::optional<int> count) {
    return count ? translated("confirm.deleteCount", *count) : translated("confirm.delete");
}
void drawPageHeading(MinecraftUIRenderContext& context, SettingsTable const& t, std::string_view section, std::string_view feature,
                     float backX, bool backHovered, std::string const& aside) {
    float asideWidth = aside.empty() ? 0 : textWidth(context, aside) + SettingsTable::gap;
    label(context,t.nameX,t.theadTop+2,backX-t.nameX-SettingsTable::gap-asideWidth,
        translated(section) + "  >  " + translated(feature),palette::faint);
    if (!aside.empty()) label(context,backX-asideWidth,t.theadTop+2,asideWidth-SettingsTable::gap,aside,palette::faint);
    drawButton(context,backX,t.theadTop,ShapesLayout::closeWidth,11,"< " + translated(section),backHovered);
    fill(context,t.tableLeft,t.rowsTop-1,t.tableWidth,1,palette::white,.14f);
}
void drawToolHeader(MinecraftUIRenderContext& context, ShapesLayout const& l, std::string const& showAll, bool showAllOn, bool docked,
                    bool overShowAll, bool overKeys, bool overDock) {
    float top = l.top + 4;
    label(context,l.drawAllX,l.drawAllY+1+boxTextInset(),l.drawAllWidth-switchWidth-4,showAll,overShowAll ? palette::text : palette::dim,Align::Right);
    toggleSwitch(context,l.drawAllX+l.drawAllWidth-switchWidth,l.drawAllY+2,showAllOn);
    label(context,l.keysX,top+1+boxTextInset(),ShapesLayout::keysWidth,translated("shape.keys"),overKeys ? palette::text : palette::accent,Align::Center);
    fill(context,l.keysX+6,top+11,ShapesLayout::keysWidth-12,1,palette::accent,overKeys ? 1.f : .5f);
    drawButton(context,l.dockX,top,ShapesLayout::dockWidth,12,translated(docked ? "shape.undock" : "shape.dock"),overDock);
}
void drawDockedFrame(MinecraftUIRenderContext& context, ShapesLayout const& l, std::string const& title, std::string const& close,
                     bool closeHover) {
    panel(context,l.left,l.top,l.width,l.height,opacity::screen);
    frame(context,l.left,l.top,l.width,l.height,palette::white,.14f);
    label(context,l.left+ShapesLayout::pad,l.top+6,l.dockedTitleWidth(),title);
    drawButton(context,l.closeX,l.top+4,ShapesLayout::closeWidth,12,close,closeHover);
    fill(context,l.left,l.top+ShapesLayout::headerHeight-1,l.width,1,palette::white,.14f);
}
void drawTipBox(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool warns) {
    context.flushText(0, std::nullopt);
    fill(context,x,y,w,h,palette::panel,opacity::floating);
    frame(context,x,y,w,h,warns ? palette::warning : palette::keyEdge);
}
void drawNotice(MinecraftUIRenderContext& context, float centerX, float y, float maxWidth, std::string const& text, bool warns,
                bool overWorld) {
    float w = std::min(maxWidth, textWidth(context, text) + 12), x = centerX - w / 2;
    fill(context,x,y,w,12,palette::panel,overWorld ? opacity::screen : opacity::floating);
    frame(context,x,y,w,12,warns ? palette::warning : palette::keyEdge);
    label(context,x,y+1+boxTextInset()-1,w,text,warns ? palette::warning : palette::text,Align::Center);
}
void drawDockedTooShort(MinecraftUIRenderContext& context, ShapesLayout const& l, float pointerX, float pointerY) {
    drawButton(context,l.dockX,l.top+4,ShapesLayout::dockWidth,12,translated("shape.undock"),
        l.hit(pointerX, pointerY).zone == ShapesLayout::Zone::Dock);
    float pad = ShapesLayout::pad;
    paragraph(context,l.left+pad,l.top+ShapesLayout::headerHeight+6,l.width-2*pad,translated("smallWindow"),3,palette::faint);
}
void drawShapeStepper(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, bool numeric, std::string text,
                      NumberInput const* editing, float pointerX, float pointerY) {
    drawStepperBox(context, l.stepperX(), y, l.stepperWidth(), numeric, std::move(text), editing, pointerX, pointerY);
}
void drawStepper(MinecraftUIRenderContext& context, float x, float top, float w, float h, bool numeric, std::string text,
                 StepperLook const& look) {
    float aw = look.cell, textY = top + (h - 10) / 2 + boxTextInset();
    Rgb ink = look.warn ? palette::warning : look.ink;
    int hot = look.hot;
    if (!hot && look.pointerY >= top && look.pointerY < top + h) {
        if (look.pointerX >= x && look.pointerX < x + aw) hot = -1;
        else if (look.pointerX >= x + w - aw && look.pointerX < x + w) hot = 1;
    }
    for (int side = 0; side < 2; ++side) {
        float cx = side ? x + w - aw : x;
        bool enabled = side ? look.highEnabled : look.lowEnabled;
        fill(context,cx,top,aw,h,palette::keyFill);
        if (enabled && hot == (side ? 1 : -1)) fill(context,cx,top,aw,h,palette::white,.1f);
        Rgb mark = !enabled ? palette::faint : hot == (side ? 1 : -1) ? palette::text : palette::dim;
        if (numeric) label(context,cx,textY,aw,side ? "+" : "-",mark,Align::Center);
        else arrow(context,cx+std::floor((aw-3)/2),top+std::floor((h-5)/2),side == 0,mark);
    }
    frame(context,x,top,w,h,look.editing ? palette::accent : look.warn ? palette::warning : palette::keyEdge);
    if (look.editing) drawEditField(context,x+aw+1,top,h,w-2*aw-2,*look.editing,Align::Center,ink);
    else label(context,x+aw+1,textY,w-2*aw-2,std::move(text),ink,Align::Center);
}
void drawStepperBox(MinecraftUIRenderContext& context, float x, float y, float w, bool numeric, std::string text, NumberInput const* editing,
                    float pointerX, float pointerY) {
    drawStepper(context,x,y+1,w,ShapesLayout::rowHeight-2,numeric,std::move(text),
        {.editing = editing, .pointerX = pointerX, .pointerY = pointerY});
}
namespace {
// Where the field being edited was drawn last, for clicks (L-172).
struct EditFieldPlace {
    TextEdit const* field = nullptr;
    float x = 0, top = 0, width = 0, height = 0, origin = 0;
    std::size_t start = 0;
} lastField;
}
void drawFieldBox(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool editing) {
    fill(context,x,y,w,h,palette::shade,.45f);
    frame(context,x,y,w,h,editing ? palette::accent : palette::keyEdge);
}
void drawTextField(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool editing, TextEdit const* input,
                   std::string const& text, std::string const& placeholder, float inset, Align align) {
    drawFieldBox(context,x,y,w,h,editing);
    float textX = x + inset, textW = w - inset - 4;
    float textY = y + 1 + boxTextInset(); // where drawEditField puts typed text, so clicking does not move it
    if (editing && input) {
        if (input->value().empty() && !placeholder.empty()) label(context,textX,textY,textW,placeholder,palette::faint,align);
        drawEditField(context,textX,y,h,textW,*input,align);
    } else if (text.empty()) label(context,textX,textY,textW,placeholder,palette::faint,align);
    else label(context,textX,textY,textW,text,palette::text,align);
}
void caretBar(MinecraftUIRenderContext& context, float x, float top, float height) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (ms / 530 % 2 == 0) fill(context, x - 1, top + 2, 1, height - 4, palette::text);
}
void drawEditField(MinecraftUIRenderContext& context, float x, float top, float height, float width, TextEdit const& input,
                   Align align, Rgb color) {
    auto const& value = input.value();
    float room = width - 3;
    // Long text scrolls so the caret stays in the field.
    std::size_t start = 0, caret = input.selectedAll() ? value.size() : input.caret();
    while (start < caret && textWidth(context, std::string_view(value).substr(start, caret - start)) > room) {
        ++start;
        while (start < value.size() && (static_cast<unsigned char>(value[start]) & 0xc0) == 0x80) ++start;
    }
    std::string shown = value.substr(start);
    float shownWidth = std::min(textWidth(context, shown), room);
    float origin = align == Align::Center ? x + std::max(0.f, (width - shownWidth) / 2) : x;
    float markTop = top + 2, markHeight = height - 4;
    if (input.selectedAll() && !value.empty()) fill(context, origin - 1, markTop, shownWidth + 2, markHeight, palette::accentDeep);
    label(context, origin, top + 1 + boxTextInset(), width - (origin - x), shown, color);
    // Each glyph's advance ends in a blank spacing column: the bar goes
    // there, not onto the next glyph (a "t" hid it, 2026-10-13).
    if (!input.selectedAll())
        caretBar(context, origin + std::min(textWidth(context, std::string_view(value).substr(start, caret - start)), room), top, height);
    lastField = {&input, x, top, width, height, origin, start};
}
bool placeCaret(MinecraftUIRenderContext& context, TextEdit& field, float x, float y) {
    auto const& place = lastField;
    if (place.field != &field || x < place.x || x > place.x + place.width || y < place.top || y > place.top + place.height) return false;
    auto const& value = field.value();
    std::size_t best = place.start;
    float bestDistance = std::abs(x - place.origin);
    for (std::size_t at = place.start; at < value.size();) {
        ++at;
        while (at < value.size() && (static_cast<unsigned char>(value[at]) & 0xc0) == 0x80) ++at;
        float distance = std::abs(x - (place.origin + textWidth(context, std::string_view(value).substr(place.start, at - place.start))));
        if (distance < bestDistance) { best = at; bestDistance = distance; }
    }
    field.place(best);
    return true;
}
void forgetEditField() { lastField = {}; }
std::string amountText(std::uint64_t count, int maxStack) {
    auto a = amountOf(count, maxStack);
    std::vector<std::string> parts;
    if (a.chests) parts.push_back(translated(a.chests == 1 ? "amount.chest" : "amount.chests", a.chests));
    if (a.stacks) parts.push_back(translated(a.stacks == 1 ? "amount.stack" : "amount.stacks", a.stacks));
    if (a.items || parts.empty()) parts.push_back(std::to_string(a.items));
    std::string out;
    for (auto const& part : parts) out += (out.empty() ? "" : " + ") + part;
    return out;
}
void drawScrollbar(MinecraftUIRenderContext& context, float x, float top, float track, float thumbTop, float thumb, bool hot) {
    fill(context,x,top,2,track,palette::white,.08f);
    fill(context,x,thumbTop,2,thumb,hot ? palette::dim : palette::keyEdge);
}
void drawContentsPopover(MinecraftUIRenderContext& context, hud_editor::ContentsLayout const& layout,
                         std::vector<std::string> const& names, std::vector<bool> const& on, float pointerX, float pointerY,
                         bool scrollHot) {
    auto const& b = layout.box;
    panel(context, b.x, b.y, b.w, b.h, opacity::screen);
    frame(context, b.x, b.y, b.w, b.h, palette::white, .14f);
    auto triangle = [&](hud_editor::Point at, bool up, Rgb color) {
        for (int i = 0; i < 3; ++i) fill(context, at.x + 2 - i, up ? at.y + i : at.y + 2 - i, 1 + 2 * i, 1, color);
    };
    for (auto const& row : layout.rows) {
        auto i = static_cast<size_t>(row.index);
        if (i >= names.size() || i >= on.size() || names[i].empty()) continue;
        if (row.row.contains(pointerX, pointerY)) fill(context, row.row.x, row.row.y, row.row.w, row.row.h, palette::white, .06f);
        toggleSwitch(context, row.switchAt.x, row.switchAt.y, on[i]);
        label(context, row.labelAt.x, row.labelAt.y, row.labelWidth, names[i], palette::text);
        if (row.up) triangle(row.upAt, true, row.canUp ? palette::dim : palette::off);
        if (row.down) triangle(row.downAt, false, row.canDown ? palette::dim : palette::off);
    }
    if (auto const& t = layout.track) {
        auto thumb = hud_editor::scrollThumb(t->top, t->track, layout.count, layout.visible, layout.first);
        bool over = pointerX >= t->x - 2 && pointerX < t->x + 4 && pointerY >= t->top && pointerY < t->top + t->track;
        drawScrollbar(context, t->x + 1, t->top, t->track, thumb.top, thumb.h, scrollHot || over);
    }
}
void drawListScrollbar(MinecraftUIRenderContext& context, ShapesLayout const& l, bool hot) {
    if (l.listCount <= l.listVisible) return;
    float listRight = l.listLeft + l.listWidth;
    float track = l.listVisible * ShapesLayout::rowHeight;
    float thumb = std::max(8.0f, track * l.listVisible / l.listCount);
    float thumbY = l.rowsTop + (track - thumb) * l.listFirst / (l.listCount - l.listVisible);
    drawScrollbar(context,listRight-3,l.rowsTop,track,thumbY,thumb,hot);
}
}
