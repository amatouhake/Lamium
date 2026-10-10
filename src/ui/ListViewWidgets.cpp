#include "ui/ListViewWidgets.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include <algorithm>
#include <chrono>

namespace lamium::ui {
void drawSmallButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                     bool hovered, Rgb fillColor, Rgb edge, Rgb textColor) {
    fill(context,x,y,w,h,fillColor);
    if (hovered) fill(context,x,y,w,h,palette::white,.1f);
    frame(context,x,y,w,h,edge);
    label(context,x,y+(h-10)/2+boxTextInset()-1,w,std::move(text),textColor,Align::Center);
}
void drawShapeStepper(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, bool numeric, std::string text,
                      NumberInput const* editing) {
    float x = l.stepperX(), w = l.stepperWidth(), aw = ShapesLayout::arrowWidth, h = ShapesLayout::rowHeight - 2;
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
    if (editing) text = editing->selectedAll() ? "[" + editing->value() + "]" : editing->value() + "_";
    label(context,x+aw+1,y+2+boxTextInset(),w-2*aw-2,std::move(text),palette::text,Align::Center);
}
void drawEditText(MinecraftUIRenderContext& context, float x, float top, float height, float width, SearchQuery const& input) {
    auto const& value = input.value();
    float w = std::min(textWidth(context, value), width - 2);
    float markTop = top + 2, markHeight = height - 4;
    if (input.selectedAll() && !value.empty()) fill(context, x - 1, markTop, w + 2, markHeight, palette::accentDeep);
    label(context, x, top + 1 + boxTextInset(), width, value);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!input.selectedAll() && ms / 530 % 2 == 0) fill(context, x + w + 1, markTop, 1, markHeight, palette::text);
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
