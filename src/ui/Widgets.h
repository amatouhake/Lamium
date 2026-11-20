#pragma once
// Every Lamium screen and HUD element draws through this file (and the list
// pieces in ListViewWidgets.h). Rules, checked by scripts/Check-UiTokens.ps1:
// - No color literals elsewhere: a missing color becomes a palette token here.
//   Each Lamium color has one meaning: purple Experimental, yellow (warning)
//   a warning, red a destructive action, light blue "reference, not Lamium's
//   own content". Colors sampled from the game only match the game.
// - A control a screen needs and this file lacks is added here, not drawn in
//   the screen and not copied from another screen. Buttons: plain, primary
//   (accent fill), destructive (press twice, armed in red).
// - A control that must look or behave unlike the one here is a design
//   question for the maintainer, not a local variant.
// Details: docs/DESIGN.md "Visual language".
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
    keyFill{.169f,.173f,.176f}, keyEdge{.353f,.357f,.361f}, experimental{.725f,.545f,1.f},
    warning{1.f,.761f,.29f}, danger{.54f,.18f,.16f}, dangerEdge{.85f,.33f,.3f}, dangerText{1.f,.7f,.68f},
    info{.62f,.83f,1.f}, knobOn{1,1,1}, knobOff{.816f,.82f,.831f},
    shade{0,0,0}, // black scrims and outlines; each use sets its own opacity
    // The schematic check's kinds, in the Check list and as marks in the world: a block that differs
    // (not destructive, so not danger), one whose state differs (not a warning), one not placed yet.
    mismatch{1.f,.35f,.3f}, wrongState{1.f,.8f,.25f}, notPlaced{.75f,.85f,.9f},
    cornerOne{1.f,.4f,.35f}, cornerTwo{.45f,.65f,1.f}; // schematic area corners, as their marks in the world
// Parts of the switch and slider drawing only: the off edge and the knob's grip line.
inline constexpr Rgb controlEdge{.18f,.18f,.19f}, gripOn{.71f,.71f,.72f}, gripOff{.6f,.61f,.62f};
// The calculator's expression colors: operators, units, variables, functions.
inline constexpr Rgb syntaxOperator{.84f,.95f,.79f}, syntaxUnit{1.f,.85f,.54f}, syntaxVariable{.66f,.83f,1.f}, syntaxFunction{.89f,.77f,1.f};
// The shapes' four colors, also the anvil's left (cyan) and right (pink) stacks.
inline constexpr Rgb shapeCyan{.25f,.82f,.88f}, shapePink{.94f,.5f,.75f}, shapeYellow{.95f,.8f,.24f}, shapeWhite{.95f,.95f,.95f};
inline constexpr Rgb mapGround{11 / 255.f, 12 / 255.f, 13 / 255.f}; // the world map where nothing is recorded
// Sampled from the game: they only match what the game draws, no Lamium meaning.
namespace vanilla {
inline constexpr Rgb heart{.878f,.314f,.235f}, heartEmpty{.227f,.122f,.11f},
    armor{.722f,.725f,.769f}, // #B8B9C4, the armor icon
    curse{1.f,.333f,.333f}, // curses in an item's tooltip
    tooltipBorder{.55f,.35f,.7f}, // the container preview's frame, as the item tooltip's
    // A window of the game's own grey and the text written on it, for a Lamium window that lies over
    // a game window and takes its place (the recipe view, L-359).
    window{.776f,.776f,.776f}, windowInk{.247f,.247f,.247f}, windowFaint{.42f,.42f,.42f},
    // The window's edge: black outside, white light on the upper left, shadow on the lower right (L-366).
    windowEdge{0,0,0}, windowLight{1,1,1}, windowShadow{.333f,.333f,.333f},
    // The item tooltip: its opaque fill and the purple line inside its edge (L-366).
    tooltipFill{.106f,.047f,.106f}, tooltipLine{.173f,.031f,.388f},
    // The game's controls on its window (L-367): a raised button's shadow, a switch's empty track, a tab
    // not chosen and the light along its top.
    buttonShadow{.392f,.392f,.392f}, switchTrack{.498f,.498f,.498f}, tabBack{.384f,.384f,.384f}, tabRim{.769f,.769f,.769f},
    // Lamium's own, not sampled: the edge of a control on the game's window (a button, a switch, a recipe's face),
    // lighter than the game's black, which stood out against the window (maintainer 2026-11-21, L-379). The
    // window's frame and its tabs keep the black.
    controlEdge{.212f,.212f,.212f},
    // The brewing stand's fuel gauge, drawn in the recipe view's brewing faces (L-361); near the game's, not sampled.
    brewingFuel{.91f,.63f,.13f},
    // A face sunk into the game's window (a breakdown, a list of candidates), under a dark upper-left edge (L-370).
    windowSunk{.706f,.706f,.706f},
    // A slot of the game's windows: its face, its dark upper-left and light lower-right edges, and the
    // red the recipe book gives what cannot be made.
    slot{.545f,.545f,.545f}, slotDark{.216f,.216f,.216f}, slotLight{1,1,1}, slotMissing{.851f,.302f,.302f};
// The game's text color for a color code letter (the section sign's), white for none or an unknown one.
// The game's table; blue (9) as measured in its tooltip (2026-11-19), the rest not checked one by one.
constexpr Rgb textCode(char code) {
    constexpr auto c = [](int hex) { return Rgb{static_cast<float>((hex >> 16) & 255) / 255, static_cast<float>((hex >> 8) & 255) / 255,
                                                static_cast<float>(hex & 255) / 255}; };
    switch (code) {
    case '0': return c(0x000000); case '1': return c(0x0000AA); case '2': return c(0x00AA00); case '3': return c(0x00AAAA);
    case '4': return c(0xAA0000); case '5': return c(0xAA00AA); case '6': return c(0xFFAA00); case '7': return c(0xAAAAAA);
    case '8': return c(0x555555); case '9': return c(0x447FFF); case 'a': return c(0x55FF55); case 'b': return c(0x55FFFF);
    case 'c': return c(0xFF5555); case 'd': return c(0xFF55FF); case 'e': return c(0xFFFF55); case 'g': return c(0xDDD605);
    case 'h': return c(0xE3D4D1); case 'i': return c(0xCECACA); case 'j': return c(0x443A3B); case 'm': return c(0x971607);
    case 'n': return c(0xB4684D); case 'p': return c(0xDEB12D); case 'q': return c(0x47A036); case 's': return c(0x2CBAA8);
    case 't': return c(0x21497B); case 'u': return c(0x9A5CC6); default: return c(0xFFFFFF);
    }
}
}
}
// Two panel opacities (DESIGN "Colors"), by what is under the panel: the
// world (the screen, docked panels, prompts, the radial, the HUD editor's
// panels) or another Lamium panel (tips, menus, the world map's side panels).
namespace opacity {
inline constexpr float screen = .8f, floating = .97f;
}
enum class Align { Left, Right, Center };

// Lamium screens lay out in their own units and shrink to fit a small GUI:
// fills, text and images multiply by this factor as they reach the game, so
// layout code, measurement and hit tests stay unscaled. 1 outside a scope.
float drawScale();
struct DrawScale {
    float previous;
    explicit DrawScale(float scale);
    ~DrawScale();
    DrawScale(DrawScale const&) = delete;
    DrawScale& operator=(DrawScale const&) = delete;
};

// Everything drawn inside the scope (fills, text) is multiplied by this
// opacity: a list that fades with a closing screen. 1 outside a scope.
float drawOpacity();
struct DrawOpacity {
    float previous;
    explicit DrawOpacity(float opacity);
    ~DrawOpacity();
    DrawOpacity(DrawOpacity const&) = delete;
    DrawOpacity& operator=(DrawOpacity const&) = delete;
};
// Inside the scope the shared controls (buttons, tabs, switches, steppers) are drawn for a window of the
// game's grey (windowPanel) as the game draws its own there (L-367): raised buttons with dark text, an on
// button keeping its green face; tabs that are blocks, the chosen one joined to the window below; a lighter
// switch track. Outside, they keep Lamium's dark look.
bool onWindow();
struct OnWindow {
    bool previous;
    OnWindow();
    ~OnWindow();
    OnWindow(OnWindow const&) = delete;
    OnWindow& operator=(OnWindow const&) = delete;
};

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
// One stroke of the current font: Minecraft's pixel font draws whole GUI
// units, the Japanese font about half (an underline under a label). Chinese
// and Korean drew the pixel font's weight on the title screen (2026-10-14).
float textStroke();
// How much lower a shape drawn beside text sits so it lines up with the
// letters, in text units: Japanese glyphs sit about one unit lower than
// Latin ones (the change arrow measured 2026-10-08).
float shapeTextDrop();
// How much higher text centered in a row starts in Japanese and Chinese, in text units: their glyphs sit about one
// unit lower in the line than the pixel font's (the toast beside its switch, measured in Japanese 2026-11-13).
float rowTextLift();
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
// A window drawn as the game's own (VanillaFrame.h): black edge with cut corners, light upper left,
// shadowed lower right, the game's grey inside; opaque: what lies under it must not show. Its edge is
// 3 units wide. Text on it is windowLabel: dark, without a shadow. Buttons, slots and fields on it
// are the shared dark ones.
void windowPanel(MinecraftUIRenderContext&, float left, float top, float width, float height);
// The game's item tooltip box (VanillaFrame.h: text 6 units in, 5 down, 10 a line), opaque, for an item
// Lamium shows in place of a slot of the game's.
void gameTooltipBox(MinecraftUIRenderContext&, float x, float y, float width, float height);
// A share done (L-367): a track with the done part in the accent green from the left. On the game's window
// the track is the slots' grey under a half-unit dark top edge, as a slot is sunk.
void progressBar(MinecraftUIRenderContext&, float x, float y, float w, float h, float fraction);
// A face sunk into the game's window (L-370): the sunk grey under a dark top and left edge and a light bottom
// and right one. `top`/`bottom`: whether this piece draws those edges, so a long face can be drawn in slices.
void sunkFace(MinecraftUIRenderContext&, float x, float y, float w, float h, bool top = true, bool bottom = true);
// The game's raised button shape: an edge (vanilla::controlEdge), half a unit of `light` along the top and left inside it
// and of `shadow` along the bottom and right, `face` inside.
void raisedBox(MinecraftUIRenderContext&, float x, float y, float w, float h, Rgb face, Rgb light, Rgb shadow);
void windowLabel(MinecraftUIRenderContext&, float x, float y, float width, std::string text, bool faint = false,
                 Align align = Align::Left);
// HUD card: the panel fill with corners stepped by one GUI pixel, like
// Bedrock's item-name popup.
void card(MinecraftUIRenderContext&, float left, float top, float width, float height, float opacity = .72f);
void rowBackground(MinecraftUIRenderContext&, float left, float top, float width, float height,
                   bool selected, bool hovered);
// An item's slot, 18 units: its 16x16 icon area filled, as the container
// preview's cells are, and lighter under the pointer. The icon and the count
// are the caller's, drawn after the fills: slotCount puts a stack's count at
// the slot's lower right, over the icon.
void slot(MinecraftUIRenderContext&, float x, float y, bool hovered = false);
// The same 18-unit cell on a window of the game's grey (windowPanel), drawn as the game draws its own
// slots so a Lamium window that takes a game window's place reads like one; `missing` is the game's red
// for what the player lacks.
void windowSlot(MinecraftUIRenderContext&, float x, float y, bool hovered = false, bool missing = false);
// A check box on the game's grey, only where one marks which of a few things is in use (DESIGN "Switch",
// L-379): sunk as a slot is, `size` wide counting its edges, as a raised button of the same size beside it.
// `Check::Auto` is the dark check (in use by the rule), `Check::Mine` the green one (the player's choice).
enum class Check { None, Auto, Mine };
void windowCheckBox(MinecraftUIRenderContext&, float x, float y, float size, Check check, bool hovered);
// One of several, chosen (decided 2026-11-19, demos/switching-controls-2.html): names side by side with
// no gap over a rule (tabRule) as wide as what they switch. On a dark Lamium panel the chosen one stands
// on a faint accent face over an accent bar, as the settings sidebar's selected row does; on a window of
// a window of the game's grey they are windowTab instead. The others are dim and lighten under the
// pointer; none is a filled button, so a strip is not mistaken for actions. Without a text the tab is an
// icon's place and the caller draws the icon. Height 11 like a button, or `height`.
inline constexpr float tabHeight = 11, tabPad = 9, tabIconWidth = 16;
void drawTab(MinecraftUIRenderContext&, float x, float y, float w, std::string text, bool selected, bool hovered, bool first,
             float height = tabHeight);
void tabRule(MinecraftUIRenderContext&, float x, float y, float w);
// The game's tabs on a window of its grey (L-367), as measured from the game's recipe book: blocks
// standing windowTabRise above the window's top edge, side by side sharing their black edges (the next
// starts at x + w - 1), with the window's stepped corners and two units of light along the top and left; the others' face dark grey with white
// names, the chosen one in the window's grey with two units of shadow at its right, joined to the window
// through its edge. Drawn after the window. Without a text the caller draws a 16-unit icon at
// x + (w - 16) / 2 and windowTabIconY. windowTabWidth suits an icon.
inline constexpr float windowTabRise = 21, windowTabWidth = 24;
inline constexpr float windowTabIconY(float windowTop) { return windowTop - 18; }
void windowTab(MinecraftUIRenderContext&, float x, float windowTop, float w, std::string text, bool selected, bool hovered);
void slotCount(MinecraftUIRenderContext&, float x, float y, std::string text);
// An icon that stands for any of several items (L-370): a star on its top left corner, over the icon, so draw it
// after the icons. `x`, `y`: the icon's top left; `scale`: the icon's (1 for 16 units).
void anyMark(MinecraftUIRenderContext&, float x, float y, float scale = 1);
// Bedrock-style switch: the knob position carries the state as well as color.
// `scale` sizes it with a HUD element (the toggle toast); the edge stays one unit.
void toggleSwitch(MinecraftUIRenderContext&, float x, float y, bool on, float scale = 1);
// Draws a vanilla UI texture (e.g. "textures/ui/heart") into each rectangle,
// batched into one flush. Rectangles are x, y, width, height.
struct ImageRect { float x, y, w, h; };
void images(MinecraftUIRenderContext&, std::string_view texture, std::vector<ImageRect> const& rects, float opacity = 1);
// Many rectangles of one color, flushed once (the editor's hatching, L-334).
void fills(MinecraftUIRenderContext&, std::vector<ImageRect> const& rects, Rgb color, float opacity = 1);
// One rectangle with an explicit sub-rectangle of the texture (uv in 0-1);
// used for block textures whose source image holds several animation frames.
// `tint` multiplies the texture (water's gray texture, L-294).
void imageUv(MinecraftUIRenderContext&, std::string_view texture, ImageRect rect, float u0, float v0, float u1,
             float v1, float opacity = 1, Rgb tint = palette::white);
// A texture Lamium uploaded at runtime (the minimap). False when the texture
// group does not have it, so the caller can upload it again.
bool runtimeImage(MinecraftUIRenderContext&, ResourceLocation const& texture, ImageRect rect, float opacity = 1);
// Part of it: u, v is the top-left corner and `span` the side, both 0-1.
bool runtimeImage(MinecraftUIRenderContext&, ResourceLocation const& texture, ImageRect rect, float u, float v,
                  float uSpan, float vSpan, float opacity = 1);
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
// "In any order": three squares out of line, 9 by 9 units, for a recipe whose ingredients have no place.
constexpr float anyOrderMarkSize = 9;
void anyOrderMark(MinecraftUIRenderContext&, float x, float y, Rgb color);
void changeArrow(MinecraftUIRenderContext&, float x, float y, float scale, Rgb color);
// Warning caps mark a binding that relates to another: Outline overlaps,
// Filled is the exact same chord (all its actions fire together).
enum class KeyTone { Plain, Outline, Filled };
// Draws key caps left to right within width and returns the width used.
float keycaps(MinecraftUIRenderContext&, float x, float y, float width, std::vector<std::string> const& keys,
              KeyTone tone = KeyTone::Plain);
// The width keycaps() needs to draw every cap without clipping.
float keycapsWidth(MinecraftUIRenderContext&, std::vector<std::string> const& keys);
// Caps wrapped onto further lines when they do not fit one; both return the
// height the caps occupy.
float keycapsWrapHeight(MinecraftUIRenderContext&, float width, std::vector<std::string> const& keys, float lineGap = 3);
float keycapsWrap(MinecraftUIRenderContext&, float x, float y, float width, std::vector<std::string> const& keys,
                  KeyTone tone = KeyTone::Plain, float lineGap = 3);
constexpr float capHeight = 11;
}
