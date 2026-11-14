#pragma once
// Pieces the list-and-detail views (Shapes, Waypoints, Schematics) and the
// prompts draw with, on top of Widgets.h.
#include "ui/NumberInput.h"
#include "ui/SearchQuery.h"
#include "ui/ShapesLayout.h"
#include "ui/Widgets.h"
#include <cstdint>
#include <string>
class MinecraftUIRenderContext;
namespace lamium::ui {
void drawSmallButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                     bool hovered, Rgb fillColor = palette::keyFill, Rgb edge = palette::keyEdge, Rgb textColor = palette::text);
// A destructive button: the first press arms it (danger fill), the second acts.
// Unarmed it looks plain and turns red only under the pointer, so an always
// visible one (a section's Reset) does not draw the eye.
void drawDangerButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                      bool hovered, bool armed);
// The body of a docked view too short for its editor (!l.usable()): Expand and
// a hint under the header, which matches what ShapesLayout::hit() answers then.
void drawDockedTooShort(MinecraftUIRenderContext& context, ShapesLayout const& l, float pointerX, float pointerY);
// A value stepper in a view's editor column; while `editing` it shows the
// number being typed.
// The stepper of the lists' rows on its own: arrow cells at both ends (or - and +), the value between them.
// `y` is the row's top; the box is a row high.
void drawStepperBox(MinecraftUIRenderContext& context, float x, float y, float w, bool numeric, std::string text,
                    NumberInput const* editing = nullptr);
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
        frame(context,x,top,size,size,palette::shade,.6f);
    }
}
// Text being typed in a field of `height` from `top`: the selection as a
// highlight, otherwise a blinking bar at the caret, so a typed "_" never looks
// like the caret. Both keep the same margin above and below. Long text
// scrolls to keep the caret inside. The field is remembered for clicks.
void drawEditField(MinecraftUIRenderContext& context, float x, float top, float height, float width, TextEdit const& input,
                   Align align = Align::Left, Rgb color = palette::text);
// A click inside the field drawn last for `field` puts its caret at the
// nearest character boundary; false when the click was elsewhere.
bool placeCaret(MinecraftUIRenderContext& context, TextEdit& field, float x, float y);
// Called when no field is being edited, so an old place cannot take clicks.
void forgetEditField();
// "1 chest + 4 stacks + 16", number first (L-93 screen review; also the
// calculator, L-171).
std::string amountText(std::uint64_t count, int maxStack = 64);
// The scrollbar of a view's list when it does not fit.
void drawListScrollbar(MinecraftUIRenderContext& context, ShapesLayout const& l);
}
