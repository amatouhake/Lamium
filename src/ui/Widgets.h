#pragma once
#include <string>
#include <string_view>
#include <vector>
class MinecraftUIRenderContext;
class ResourceLocation;

namespace lamium::ui {
struct Rgb { float r, g, b; };
// One palette for Lamium screens: flat fills and text only, matching Bedrock's
// dark translucent surfaces with its green accent.
namespace palette {
inline constexpr Rgb panel{.063f,.067f,.075f}, white{1,1,1}, text{1,1,1}, dim{.706f,.722f,.714f},
    faint{.498f,.522f,.514f}, accent{.424f,.765f,.286f}, accentDeep{.235f,.522f,.153f}, off{.282f,.286f,.29f},
    keyFill{.169f,.173f,.176f}, keyEdge{.353f,.357f,.361f}, experimental{.725f,.545f,1.f}, warning{1.f,.761f,.29f},
    knobOn{1,1,1}, knobOff{.816f,.82f,.831f}, heart{.878f,.314f,.235f}, heartEmpty{.227f,.122f,.11f},
    armor{.722f,.725f,.769f}; // #B8B9C4, sampled from the vanilla armor icon
}
enum class Align { Left, Right, Center };

// Coordinates use the game's GUI units. Widgets borrow the render context only
// for this call; screens retain navigation and input ownership themselves.
void label(MinecraftUIRenderContext&, float x, float y, float width, std::string text,
           Rgb color = palette::text, Align align = Align::Left);
// Text gets Lamium's own drop shadow, half a GUI unit away; the engine's
// shadow sits a whole unit away and reads as doubled on dense Japanese
// lines. `shadow` false draws the text alone.
void labelScaled(MinecraftUIRenderContext&, float x, float y, float width, std::string text, float scale,
                 Rgb color = palette::text, Align align = Align::Left, bool shadow = true);
// Japanese glyphs fill more of the line; text framed by a border starts this
// much lower than its box top so it clears the bottom edge in every locale.
float boxTextInset();
// How much lower a shape drawn beside text sits so it lines up with the
// letters, in text units: Japanese glyphs sit about one unit lower than
// Latin ones (the change arrow measured 2026-10-08).
float shapeTextDrop();
void paragraph(MinecraftUIRenderContext&, float x, float y, float width, std::string_view text, size_t maxLines,
               Rgb color = palette::text);
// How many lines paragraph() would draw.
size_t paragraphLines(MinecraftUIRenderContext&, float width, std::string_view text, size_t maxLines);
float textWidth(MinecraftUIRenderContext&, std::string_view text);
float textWidthScaled(MinecraftUIRenderContext&, std::string_view text, float scale);
// The width labelScaled lays the text out at: with a Japanese locale, Latin
// runs are measured and placed one by one, which can differ from measuring
// the whole line. Use it where a background must fit the drawn text.
float labelWidth(MinecraftUIRenderContext&, std::string_view text, float scale);
void fill(MinecraftUIRenderContext&, float x, float y, float width, float height, Rgb color, float opacity = 1);
void frame(MinecraftUIRenderContext&, float x, float y, float width, float height, Rgb color, float opacity = 1);
void panel(MinecraftUIRenderContext&, float left, float top, float width, float height, float opacity = .8f);
// HUD card: the panel fill with corners stepped by one GUI pixel, like
// Bedrock's item-name popup.
void card(MinecraftUIRenderContext&, float left, float top, float width, float height, float opacity = .72f);
void rowBackground(MinecraftUIRenderContext&, float left, float top, float width, float height,
                   bool selected, bool hovered);
// Bedrock-style switch: the knob position carries the state as well as color.
void toggleSwitch(MinecraftUIRenderContext&, float x, float y, bool on);
// Draws a vanilla UI texture (e.g. "textures/ui/heart") into each rectangle,
// batched into one flush. Rectangles are x, y, width, height.
struct ImageRect { float x, y, w, h; };
void images(MinecraftUIRenderContext&, std::string_view texture, std::vector<ImageRect> const& rects, float opacity = 1);
// One rectangle with an explicit sub-rectangle of the texture (uv in 0-1);
// used for block textures whose source image holds several animation frames.
void imageUv(MinecraftUIRenderContext&, std::string_view texture, ImageRect rect, float u0, float v0, float u1,
             float v1, float opacity = 1);
// A texture Lamium uploaded at runtime (the minimap). False when the texture
// group does not have it, so the caller can upload it again.
bool runtimeImage(MinecraftUIRenderContext&, ResourceLocation const& texture, ImageRect rect, float opacity = 1,
                  Rgb tint = palette::white);
// Part of it: u, v is the top-left corner and `span` the side, both 0-1.
bool runtimeImage(MinecraftUIRenderContext&, ResourceLocation const& texture, ImageRect rect, float u, float v,
                  float uSpan, float vSpan, float opacity = 1, Rgb tint = palette::white);
constexpr float switchWidth = 18, switchHeight = 9;
// Bedrock-style slider in the switch's colors: filled track, square knob.
// `fraction` is 0-1 along the track; the knob stays inside x..x+width.
void slider(MinecraftUIRenderContext&, float x, float y, float width, float fraction, bool active = false);
constexpr float sliderKnobWidth = 6, sliderKnobHeight = 10, sliderTrackHeight = 4;
// Glyph-independent disclosure and stepper arrows drawn from rectangles.
void chevron(MinecraftUIRenderContext&, float x, float y, bool expanded, Rgb color = palette::dim);
void arrow(MinecraftUIRenderContext&, float x, float y, bool left, Rgb color = palette::dim);
// A right-pointing "changes to" arrow, 7 by 5 units times `scale`, for rows
// like "North -> East": the game fonts lack U+2192 (it fell back to another
// font in English and showed as a box in Japanese, 2026-10-08).
constexpr float changeArrowWidth = 7, changeArrowHeight = 5;
void changeArrow(MinecraftUIRenderContext&, float x, float y, float scale, Rgb color);
// Warning caps mark a binding that relates to another: Outline overlaps,
// Filled is the exact same chord (all its actions fire together).
enum class KeyTone { Plain, Outline, Filled };
// Draws key caps left to right within width and returns the width used.
float keycaps(MinecraftUIRenderContext&, float x, float y, float width, std::vector<std::string> const& keys,
              KeyTone tone = KeyTone::Plain);
constexpr float capHeight = 11;
}
