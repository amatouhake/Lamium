#include "ui/ListViewWidgets.h"
#include "ui/ItemAmount.h"
#include "ui/Localization.h"
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
    panel(context,l.left,l.top,l.width,l.height,.82f);
    frame(context,l.left,l.top,l.width,l.height,palette::white,.14f);
    label(context,l.left+ShapesLayout::pad,l.top+6,l.dockedTitleWidth(),title);
    drawButton(context,l.closeX,l.top+4,ShapesLayout::closeWidth,12,close,closeHover);
    fill(context,l.left,l.top+ShapesLayout::headerHeight-1,l.width,1,palette::white,.14f);
}
void drawDockedTooShort(MinecraftUIRenderContext& context, ShapesLayout const& l, float pointerX, float pointerY) {
    drawButton(context,l.dockX,l.top+4,ShapesLayout::dockWidth,12,translated("shape.undock"),
        l.hit(pointerX, pointerY).zone == ShapesLayout::Zone::Dock);
    float pad = ShapesLayout::pad;
    paragraph(context,l.left+pad,l.top+ShapesLayout::headerHeight+6,l.width-2*pad,translated("smallWindow"),3,palette::faint);
}
void drawShapeStepper(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, bool numeric, std::string text,
                      NumberInput const* editing) {
    drawStepperBox(context, l.stepperX(), y, l.stepperWidth(), numeric, std::move(text), editing);
}
void drawStepper(MinecraftUIRenderContext& context, float x, float top, float w, float h, bool numeric, std::string text,
                 StepperLook const& look) {
    float aw = look.cell, textY = top + (h - 10) / 2 + boxTextInset();
    Rgb ink = look.warn ? palette::warning : look.ink;
    for (int side = 0; side < 2; ++side) {
        float cx = side ? x + w - aw : x;
        bool enabled = side ? look.highEnabled : look.lowEnabled;
        fill(context,cx,top,aw,h,palette::keyFill);
        if (enabled && look.hot == (side ? 1 : -1)) fill(context,cx,top,aw,h,palette::white,.1f);
        Rgb mark = !enabled ? palette::faint : look.hot == (side ? 1 : -1) ? palette::text : palette::dim;
        if (numeric) label(context,cx,textY,aw,side ? "+" : "-",mark,Align::Center);
        else arrow(context,cx+std::floor((aw-3)/2),top+std::floor((h-5)/2),side == 0,mark);
    }
    frame(context,x,top,w,h,look.editing ? palette::accent : look.warn ? palette::warning : palette::keyEdge);
    if (look.editing) drawEditField(context,x+aw+1,top,h,w-2*aw-2,*look.editing,Align::Center,ink);
    else label(context,x+aw+1,textY,w-2*aw-2,std::move(text),ink,Align::Center);
}
void drawStepperBox(MinecraftUIRenderContext& context, float x, float y, float w, bool numeric, std::string text, NumberInput const* editing) {
    drawStepper(context,x,y+1,w,ShapesLayout::rowHeight-2,numeric,std::move(text),{.editing = editing});
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
    if (editing && input) {
        if (input->value().empty() && !placeholder.empty()) label(context,textX,y+(h-10)/2+boxTextInset(),textW,placeholder,palette::faint,align);
        drawEditField(context,textX,y,h,textW,*input,align);
    } else if (text.empty()) label(context,textX,y+(h-10)/2+boxTextInset(),textW,placeholder,palette::faint,align);
    else label(context,textX,y+(h-10)/2+boxTextInset(),textW,text,palette::text,align);
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
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!input.selectedAll() && ms / 530 % 2 == 0) {
        // Each glyph's advance ends in a blank spacing column: the bar goes
        // there, not onto the next glyph (a "t" hid it, 2026-10-13).
        float caretX = origin + std::min(textWidth(context, std::string_view(value).substr(start, caret - start)), room);
        fill(context, caretX - 1, markTop, 1, markHeight, palette::text);
    }
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
    fill(context,x,top,2,track,palette::white,hot ? .14f : .08f);
    fill(context,hot ? x-1 : x,thumbTop,hot ? 3 : 2,thumb,hot ? palette::text : palette::keyEdge);
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
