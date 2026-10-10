#pragma once
// Pieces the list-and-detail views (Shapes, Waypoints, Schematics) and the
// prompts draw with, on top of Widgets.h.
#include "ui/NumberInput.h"
#include "ui/SearchQuery.h"
#include "ui/ShapesLayout.h"
#include "ui/Widgets.h"
#include <string>
class MinecraftUIRenderContext;
namespace lamium::ui {
void drawSmallButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                     bool hovered, Rgb fillColor = palette::keyFill, Rgb edge = palette::keyEdge, Rgb textColor = palette::text);
// A value stepper in a view's editor column; while `editing` it shows the
// number being typed.
void drawShapeStepper(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, bool numeric, std::string text,
                      NumberInput const* editing);
// A row of color swatches in the editor's value column; the chosen one framed.
template<class ColorOf>
void drawSwatchRow(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, int count, int chosen, ColorOf colorOf) {
    float size = l.swatchSize(count), top = y + (ShapesLayout::rowHeight - size) / 2;
    for (int i = 0; i < count; ++i) {
        float x = l.swatchX(i, count);
        if (i == chosen) frame(context,x-2,top-2,size+4,size+4,palette::white);
        fill(context,x,top,size,size,colorOf(i));
        frame(context,x,top,size,size,Rgb{0,0,0},.6f);
    }
}
// Text being typed in a field of `height` from `top`: the selection as a
// highlight, otherwise a blinking bar after the text, so a typed "_" never
// looks like the caret. Both keep the same margin above and below.
void drawEditText(MinecraftUIRenderContext& context, float x, float top, float height, float width, SearchQuery const& input);
// The scrollbar of a view's list when it does not fit.
void drawListScrollbar(MinecraftUIRenderContext& context, ShapesLayout const& l);
}
