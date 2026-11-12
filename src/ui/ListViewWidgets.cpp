#include "ui/ListViewWidgets.h"
#include "ui/ItemAmount.h"
#include "ui/Localization.h"
#include <vector>
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace lamium::ui {
void drawSmallButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                     bool hovered, Rgb fillColor, Rgb edge, Rgb textColor) {
    fill(context,x,y,w,h,fillColor);
    if (hovered) fill(context,x,y,w,h,palette::white,.1f);
    frame(context,x,y,w,h,edge);
    label(context,x,y+(h-10)/2+boxTextInset()-1,w,std::move(text),textColor,Align::Center);
}
void drawDockedTooShort(MinecraftUIRenderContext& context, ShapesLayout const& l, float pointerX, float pointerY) {
    drawSmallButton(context,l.dockX,l.top+4,ShapesLayout::dockWidth,12,translated("shape.undock"),
        l.hit(pointerX, pointerY).zone == ShapesLayout::Zone::Dock);
    float pad = ShapesLayout::pad;
    paragraph(context,l.left+pad,l.top+ShapesLayout::headerHeight+6,l.width-2*pad,translated("smallWindow"),3,palette::faint);
}
void drawShapeStepper(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, bool numeric, std::string text,
                      NumberInput const* editing) {
    drawStepperBox(context, l.stepperX(), y, l.stepperWidth(), numeric, std::move(text), editing);
}
void drawStepperBox(MinecraftUIRenderContext& context, float x, float y, float w, bool numeric, std::string text, NumberInput const* editing) {
    float aw = ShapesLayout::arrowWidth, h = ShapesLayout::rowHeight - 2;
    fill(context,x,y+1,aw,h,palette::keyFill);
    fill(context,x+w-aw,y+1,aw,h,palette::keyFill);
    frame(context,x,y+1,w,h,editing ? palette::accent : palette::keyEdge);
    if (numeric) {
        label(context,x,y+2+boxTextInset(),aw,"-",palette::dim,Align::Center);
        label(context,x+w-aw,y+2+boxTextInset(),aw,"+",palette::dim,Align::Center);
    } else {
        arrow(context,x+4,y+4,true);
        arrow(context,x+w-aw+4,y+4,false);
    }
    if (editing) drawEditField(context,x+aw+1,y+1,h,w-2*aw-2,*editing,Align::Center);
    else label(context,x+aw+1,y+2+boxTextInset(),w-2*aw-2,std::move(text),palette::text,Align::Center);
}
namespace {
// Where the field being edited was drawn last, for clicks (L-172).
struct EditFieldPlace {
    TextEdit const* field = nullptr;
    float x = 0, top = 0, width = 0, height = 0, origin = 0;
    std::size_t start = 0;
} lastField;
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
void drawListScrollbar(MinecraftUIRenderContext& context, ShapesLayout const& l) {
    if (l.listCount <= l.listVisible) return;
    float listRight = l.listLeft + l.listWidth;
    float track = l.listVisible * ShapesLayout::rowHeight;
    float thumb = std::max(8.0f, track * l.listVisible / l.listCount);
    float thumbY = l.rowsTop + (track - thumb) * l.listFirst / (l.listCount - l.listVisible);
    fill(context,listRight-3,l.rowsTop,2,track,palette::white,.08f);
    fill(context,listRight-3,thumbY,2,thumb,palette::keyEdge);
}
}
