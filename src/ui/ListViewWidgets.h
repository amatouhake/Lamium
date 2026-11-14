#pragma once
// Pieces the list-and-detail views (Shapes, Waypoints, Schematics) and the
// prompts draw with, on top of Widgets.h.
#include "ui/NumberInput.h"
#include "ui/SearchQuery.h"
#include "ui/SettingsTable.h"
#include "ui/ShapesLayout.h"
#include "ui/Widgets.h"
#include <cstdint>
#include <string>
class MinecraftUIRenderContext;
namespace lamium::ui {
// Buttons (DESIGN "Controls"): one shape, a fill with a lighter edge. Plain,
// or Primary for the main action and for a button that is on/selected. A
// hovered one is lightened; a disabled one is plain with faint text and no
// hover. The text is always white otherwise.
enum class ButtonKind { Plain, Primary };
void drawButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                bool hovered, ButtonKind kind = ButtonKind::Plain, bool enabled = true);
// The same box for a button whose content is not one centered label (an icon,
// a chevron, colored key text); returns the color its content takes.
Rgb drawButtonBox(MinecraftUIRenderContext& context, float x, float y, float w, float h,
                  bool hovered, ButtonKind kind = ButtonKind::Plain, bool enabled = true);
// A destructive button: the first press arms it (danger fill), the second acts.
// Unarmed it looks plain and turns red only under the pointer, so an always
// visible one (a section's Reset) does not draw the eye.
void drawDangerButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                      bool hovered, bool armed);
// The body of a docked view too short for its editor (!l.usable()): Expand and
// a hint under the header, which matches what ShapesLayout::hit() answers then.
void drawDockedTooShort(MinecraftUIRenderContext& context, ShapesLayout const& l, float pointerX, float pointerY);
// Steppers (DESIGN "Controls"): one box, end cells `-`/`+` for a number or
// arrows for a choice, the value between them; while `editing` the value
// shows the text being typed. `hot` -1/1 lightens the end cell under the
// pointer; a disabled end draws faint. `warn` draws the box in warning color.
struct StepperLook {
    TextEdit const* editing = nullptr;
    bool warn = false;
    int hot = 0;
    bool lowEnabled = true, highEnabled = true;
    float cell = 11;
    Rgb ink = palette::text;
};
void drawStepper(MinecraftUIRenderContext& context, float x, float top, float w, float h, bool numeric, std::string text,
                 StepperLook const& look = {});
// The lists' rows: `y` is the row's top; the box is a row high less a unit each side.
void drawStepperBox(MinecraftUIRenderContext& context, float x, float y, float w, bool numeric, std::string text,
                    NumberInput const* editing = nullptr);
void drawShapeStepper(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, bool numeric, std::string text,
                      NumberInput const* editing);
// The heading row of a page that replaces the settings rows (DESIGN
// "Screens"): "Section  >  Feature" faintly, optional `aside` text, the
// "< Section" button at `backX` and the rule under it.
void drawPageHeading(MinecraftUIRenderContext& context, SettingsTable const& t, std::string_view section, std::string_view feature,
                     float backX, bool backHovered, std::string const& aside = {});
// The tool views (Shapes, Waypoints, Schematics) share one header: the
// "show all" switch, the Keys link and Dock. Docked, the panel also draws its
// own frame with the title and Close (or Back), as drawDockedFrame.
void drawToolHeader(MinecraftUIRenderContext& context, ShapesLayout const& l, std::string const& showAll, bool showAllOn, bool docked,
                    bool overShowAll, bool overKeys, bool overDock);
void drawDockedFrame(MinecraftUIRenderContext& context, ShapesLayout const& l, std::string const& title, std::string const& close,
                     bool closeHover);
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
// Text fields (DESIGN "Controls"): a shaded back with an edge, accent while
// typing. drawFieldBox is the box alone, for a field whose content is special
// (the settings search while finding a key).
void drawFieldBox(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool editing);
// While `editing` (and `input` given) the typed text with its caret, else
// `text`, or `placeholder` faintly when `text` is empty. `inset` leaves room
// on the left (an icon).
void drawTextField(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool editing, TextEdit const* input,
                   std::string const& text, std::string const& placeholder = {}, float inset = 4, Align align = Align::Left);
// A click inside the field drawn last for `field` puts its caret at the
// nearest character boundary; false when the click was elsewhere.
bool placeCaret(MinecraftUIRenderContext& context, TextEdit& field, float x, float y);
// Called when no field is being edited, so an old place cannot take clicks.
void forgetEditField();
// "1 chest + 4 stacks + 16", number first (L-93 screen review; also the
// calculator, L-171).
std::string amountText(std::uint64_t count, int maxStack = 64);
// The scrollbar of a view's list when it does not fit.
// Scroll bars (DESIGN "Controls"): a faint track and a thumb in the 2-unit
// column from `x`; `hot` (pointed at or dragged) widens and brightens it.
void drawScrollbar(MinecraftUIRenderContext& context, float x, float top, float track, float thumbTop, float thumb, bool hot);
void drawListScrollbar(MinecraftUIRenderContext& context, ShapesLayout const& l, bool hot = false);
}
