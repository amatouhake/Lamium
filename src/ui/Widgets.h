#pragma once
#include <string>
#include <string_view>
#include <vector>
class MinecraftUIRenderContext;

namespace lamium::ui {
struct Rgb { float r, g, b; };
// One palette for Lamium screens: flat fills and text only, matching Bedrock's
// dark translucent surfaces with its green accent.
namespace palette {
inline constexpr Rgb panel{.063f,.067f,.075f}, white{1,1,1}, text{1,1,1}, dim{.706f,.722f,.714f},
    faint{.498f,.522f,.514f}, accent{.424f,.765f,.286f}, accentDeep{.235f,.522f,.153f}, off{.282f,.286f,.29f},
    keyFill{.169f,.173f,.176f}, keyEdge{.353f,.357f,.361f}, experimental{.725f,.545f,1.f}, warning{1.f,.761f,.29f},
    knobOn{1,1,1}, knobOff{.816f,.82f,.831f};
}
enum class Align { Left, Right, Center };

// Coordinates use the game's GUI units. Widgets borrow the render context only
// for this call; screens retain navigation and input ownership themselves.
void label(MinecraftUIRenderContext&, float x, float y, float width, std::string text,
           Rgb color = palette::text, Align align = Align::Left);
void paragraph(MinecraftUIRenderContext&, float x, float y, float width, std::string_view text, size_t maxLines,
               Rgb color = palette::text);
float textWidth(MinecraftUIRenderContext&, std::string_view text);
void fill(MinecraftUIRenderContext&, float x, float y, float width, float height, Rgb color, float opacity = 1);
void frame(MinecraftUIRenderContext&, float x, float y, float width, float height, Rgb color, float opacity = 1);
void panel(MinecraftUIRenderContext&, float left, float top, float width, float height, float opacity = .8f);
void rowBackground(MinecraftUIRenderContext&, float left, float top, float width, float height,
                   bool selected, bool hovered);
// Bedrock-style switch: the knob position carries the state as well as color.
void toggleSwitch(MinecraftUIRenderContext&, float x, float y, bool on);
constexpr float switchWidth = 18, switchHeight = 9;
// Glyph-independent disclosure and stepper arrows drawn from rectangles.
void chevron(MinecraftUIRenderContext&, float x, float y, bool expanded, Rgb color = palette::dim);
void arrow(MinecraftUIRenderContext&, float x, float y, bool left, Rgb color = palette::dim);
// Draws key caps left to right within width and returns the width used.
float keycaps(MinecraftUIRenderContext&, float x, float y, float width, std::vector<std::string> const& keys);
constexpr float capHeight = 11;
}
