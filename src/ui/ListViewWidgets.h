#pragma once
// Pieces the list-and-detail views (Shapes, Waypoints, Schematics) and the
// prompts draw with, on top of Widgets.h.
#include "ui/HudContents.h"
#include "ui/HudLook.h"
#include "ui/ListColumns.h"
#include "ui/NumberInput.h"
#include "ui/RowSelection.h"
#include "ui/SearchQuery.h"
#include "ui/SettingsTable.h"
#include "ui/ShapesLayout.h"
#include "ui/Widgets.h"
#include <cstdint>
#include <functional>
#include <optional>
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
// The lists' shared columns (ListColumns): the heading row from `top`, with
// `ownKey` over the list's own column, and the distance cell's text, meters
// from the player in its dimension, else that dimension's name: horizontal
// for an area (no height, L-350), as the Waypoints setting says with a y.
void drawListHeadings(MinecraftUIRenderContext& context, ListColumns const& c, float top, std::string_view ownKey = {});
std::string listDistanceText(int x, int z, int dimension);
std::string listDistanceText(int x, int y, int z, int dimension);
// A footprint's small mark (a placement in the lists and the map, L-296):
// a faint fill and a frame with corner dots, 7 wide from `x`, centered on `cy`.
void drawFootprintGlyph(MinecraftUIRenderContext& context, float x, float cy, Rgb color, float alpha = 1);
// A kind's small square (the Schematics Check rows and filters, L-183):
// filled, or only its edge while the row is ignored; `size` wide from `x`,
// centered on `cy`.
void drawKindSquare(MinecraftUIRenderContext& context, float x, float cy, Rgb color, bool hollow = false, float size = 6);
// A framed tag after a row's name ("Place ignored"): small dim text in a
// thin dim frame, `tagWidth` wide; returns that width. Drawn cut to `maxWidth`.
float tagWidth(MinecraftUIRenderContext& context, std::string_view text);
float drawTag(MinecraftUIRenderContext& context, float x, float y, std::string text, float maxWidth);
// The selection pane while several rows are selected (L-190), laid out by
// `pane`: "N selected", "Shown N / Hidden M" (`shown` of `count`), then the
// first pane.fit.names of `names` (the selected rows in list order; faded
// ones faint) in the lists' columns without the switch: the mark, which
// `drawMark(index, x, centerY)` draws in a 9-unit column, the name, the
// distance and the list's own column (`ownWidth`, 0: none) as its rows show
// them; "N more" when they do not all fit, and a button per action; Delete
// is destructive, armed by `arm`. The view draws the rule above the buttons.
// Without pane.summary there is no summary line; `enabled` (one per action,
// empty: all) draws a button that would change nothing disabled.
struct SelectedName { std::string name; bool faded = false; std::string distance, own; };
void drawSelectionPane(MinecraftUIRenderContext& context, SelectionPaneLayout const& pane, size_t count,
                       std::vector<SelectedName> const& names, std::function<void(size_t, float, float)> const& drawMark,
                       std::vector<BulkAction> const& actions, BulkArm const& arm, float pointerX, float pointerY,
                       size_t shown, float ownWidth = 0, std::vector<bool> const& enabled = {});
// Bulk Delete's label for `count` selected rows: "Delete", armed
// "Confirm delete N" (the map panel sizes its button for both with armedWidth;
// the lists' fixed-width Delete falls back to "Confirm" through armedLabel).
std::string bulkDeleteLabel(size_t count, bool armed);
// What a press-twice button reads (DESIGN "Controls"): `label`, and once armed
// `confirm`, what a second press does ("Confirm delete", 「削除を確定」); in a
// button of `width` too narrow for that, just "Confirm" (`confirm.short`).
std::string armedLabel(MinecraftUIRenderContext& context, std::string label, std::string confirm, bool armed, float width);
// The same where nothing cuts the text (a context menu sized by its items).
std::string armedLabel(std::string label, std::string confirm, bool armed);
// A press-twice button sized by its text: wide enough for both labels plus
// `padding`, so arming moves nothing.
float armedWidth(MinecraftUIRenderContext& context, std::string_view label, std::string_view confirm, float padding);
// A delete's `confirm`: "Confirm delete", with a count "Confirm delete 5" (the
// one count a button holds, DESIGN "Controls").
std::string confirmDelete(std::optional<int> count = {});
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
    float pointerX = -1, pointerY = -1; // given instead of `hot`: the lit end follows the pointer
    bool backed = false; // fill the value's cell too: on a light surface (windowPanel) the white value needs it
};
void drawStepper(MinecraftUIRenderContext& context, float x, float top, float w, float h, bool numeric, std::string text,
                 StepperLook const& look = {});
// The lists' rows: `y` is the row's top; the box is a row high less a unit each side.
void drawStepperBox(MinecraftUIRenderContext& context, float x, float y, float w, bool numeric, std::string text,
                    NumberInput const* editing = nullptr, float pointerX = -1, float pointerY = -1);
void drawShapeStepper(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, bool numeric, std::string text,
                      NumberInput const* editing, float pointerX = -1, float pointerY = -1);
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
// A tip's box (DESIGN "Controls"): flushes queued text so it covers it, then
// the floating panel with a `keyEdge` edge, `warning` when the tip warns.
// Content goes inside with 4 units of padding, white unless it says otherwise.
inline constexpr float tipPad = 4;
void drawTipBox(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool warns = false);
// A passing notice for a screen without a status line (world map,
// calculator; DESIGN "Screens"): one line in a floating box centered on
// `centerX` with its top at `y`, shown for noticeSeconds. Failure: warning.
// Its opacity follows what lies under it (DESIGN "Colors"): `overWorld` for
// one outside its screen's panel (the calculator's, below it).
inline constexpr double noticeSeconds = 2.5;
void drawNotice(MinecraftUIRenderContext& context, float centerX, float y, float maxWidth, std::string const& text, bool warns = false,
                bool overWorld = false);
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
// The blinking bar a field draws at its caret, for a box of `height` from
// `top` (also the key cell waiting for a binding).
void caretBar(MinecraftUIRenderContext& context, float x, float top, float height);
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
// column from `x`; `hot` (pointed at or dragged) brightens the thumb a little.
void drawScrollbar(MinecraftUIRenderContext& context, float x, float top, float track, float thumbTop, float thumb, bool hot);
// A list's scrollbar in one call, for a view that has no layout of its own to carry one: `count` rows of
// which `visible` show from `first`, which is kept in range. It takes a press on it (`pressed` is then
// cleared), follows the drag while `held` (the button is down) and lets go when it is not. `first` must
// outlive the drag: it also tells the lists' bars apart. Nothing is drawn while all rows show.
void listScrollbar(MinecraftUIRenderContext& context, float x, float top, float track, int count, int visible, int& first, float pointerX,
                   float pointerY, bool& pressed, bool held);
void drawListScrollbar(MinecraftUIRenderContext& context, ShapesLayout const& l, bool hot = false);
// The HUD layout editor's Contents popover (L-336), laid out by
// hud_editor::layoutContents: the panel, then per row a switch and its name
// (faint while off; a row without a name is skipped) and, for a reorderable list, up and down arrows. `on` and
// `names` are indexed like the entries; `scrollHot` lights the scroll bar.
void drawContentsPopover(MinecraftUIRenderContext& context, hud_editor::ContentsLayout const& layout,
                         std::vector<std::string> const& names, std::vector<bool> const& on, float pointerX, float pointerY,
                         bool scrollHot);
// The HUD layout editor's Look popover (L-357), laid out by hud_editor::layoutLook: the panel, then the
// background as its name and a `< value >` stepper, and the switches as Contents rows are drawn.
struct LookPopoverRow { hud_editor::LookItem item; std::string name, value; bool on = false; };
void drawLookPopover(MinecraftUIRenderContext& context, hud_editor::Box box, std::vector<LookPopoverRow> const& rows,
                     float pointerX, float pointerY);
}
