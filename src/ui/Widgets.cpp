#include "ui/Widgets.h"
#include "ui/TextFit.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "ui/Translations.h"
#include <algorithm>

namespace lamium::ui {
namespace {
constexpr mce::Color white{1.f,1.f,1.f,1.f};
mce::Color color(Rgb value) { return {value.r, value.g, value.b, 1.f}; }
Font& defaultFont(MinecraftUIRenderContext& context) {
    return context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default").getFont();
}
}
float textWidth(MinecraftUIRenderContext& context, std::string_view text) {
    return defaultFont(context).getLineLength(text, 1.0f, false);
}
void fill(MinecraftUIRenderContext& context, float x, float y, float width, float height, Rgb value, float opacity) {
    if (width <= 0 || height <= 0) return;
    context.fillRectangle(RectangleArea{x,x+width,y,y+height}, color(value), std::clamp(opacity,0.f,1.f));
    context.flushImages(white,1,HashedString{"ui_fillColor"});
}
void frame(MinecraftUIRenderContext& context, float x, float y, float width, float height, Rgb value, float opacity) {
    fill(context,x,y,width,1,value,opacity);
    fill(context,x,y+height-1,width,1,value,opacity);
    fill(context,x,y+1,1,height-2,value,opacity);
    fill(context,x+width-1,y+1,1,height-2,value,opacity);
}
namespace {
// With a Japanese locale, Latin letters and digits are drawn from glyphs that sit
// lower than kana and kanji on the same line. Raise those runs to share the line.
bool japaneseLocale() { return translations::japanese(*getI18n().getCurrentLanguage()->mCode); }
float latinRaise() {
    auto locale = getI18n().getCurrentLanguage();
    return translations::japanese(*locale->mCode) ? 1.5f : 0.f;
}
void drawRun(MinecraftUIRenderContext& context, Font& font, float x, float y, float width, std::string text, Rgb value,
             ::ui::TextAlignment align) {
    TextMeasureData const measure{1.0f, 0.0f, true, false, false, align};
    CaretMeasureData const caret{-1, false};
    context.drawText(font, RectangleArea{x,x+width,y,y+14}, std::move(text), color(value), 1.0f, align, measure, caret);
}
}
float boxTextInset() { return japaneseLocale() ? 0.f : 1.f; }
void label(MinecraftUIRenderContext& context, float x, float y, float width, std::string text, Rgb value, Align align) {
    auto& font = defaultFont(context);
    auto measure = [&](std::string_view part) { return static_cast<float>(font.getLineLength(part, 1.0f, false)); };
    text = fitLabel(text, width, measure);
    if (text.empty()) return;
    float raise = latinRaise();
    bool latin = std::any_of(text.begin(), text.end(), [](unsigned char ch) { return ch < 0x80 && ch != ' '; });
    if (!raise || !latin) {
        auto native = align == Align::Right ? ::ui::TextAlignment::Right
            : align == Align::Center ? ::ui::TextAlignment::Center : ::ui::TextAlignment::Left;
        drawRun(context, font, x, y, width, std::move(text), value, native);
        return;
    }
    // Mixed or Latin-only text: position runs manually from the whole width.
    float total = measure(text);
    float cursor = align == Align::Right ? x + width - total : align == Align::Center ? x + (width - total) / 2 : x;
    size_t start = 0;
    while (start < text.size()) {
        bool ascii = static_cast<unsigned char>(text[start]) < 0x80;
        size_t end = start;
        while (end < text.size() && (static_cast<unsigned char>(text[end]) < 0x80) == ascii) ++end;
        auto run = text.substr(start, end - start);
        float runWidth = measure(run);
        drawRun(context, font, cursor, ascii ? y - raise : y, runWidth + 2, std::move(run), value, ::ui::TextAlignment::Left);
        cursor += runWidth;
        start = end;
    }
}
void paragraph(MinecraftUIRenderContext& context, float x, float y, float width, std::string_view text, size_t maxLines,
               Rgb value) {
    auto& font = defaultFont(context);
    auto lines = wrapLabel(text, width, maxLines,
        [&](std::string_view part) { return font.getLineLength(part, 1.0f, false); });
    for (auto& line : lines) {
        label(context, x, y, width, std::move(line), value);
        y += 12;
    }
}
void panel(MinecraftUIRenderContext& context, float left, float top, float width, float height, float opacity) {
    fill(context,left,top,width,height,palette::panel,opacity);
}
void rowBackground(MinecraftUIRenderContext& context, float left, float top, float width, float height,
                   bool selected, bool hovered) {
    if (selected) {
        fill(context,left,top,width,height,palette::accent,.16f);
        frame(context,left,top,width,height,palette::accent,.9f);
    } else if (hovered) fill(context,left,top,width,height,palette::white,.07f);
}
void toggleSwitch(MinecraftUIRenderContext& context, float x, float y, bool on) {
    fill(context,x,y,switchWidth,switchHeight,on ? palette::accentDeep : palette::off);
    frame(context,x,y,switchWidth,switchHeight,on ? palette::accent : Rgb{.18f,.18f,.19f});
    float knob = switchHeight - 2;
    float knobX = on ? x + switchWidth - 1 - knob : x + 1;
    fill(context,knobX,y+1,knob,knob,on ? palette::knobOn : palette::knobOff);
    fill(context,knobX,y+knob-1,knob,2,on ? Rgb{.71f,.71f,.72f} : Rgb{.6f,.61f,.62f});
}
void chevron(MinecraftUIRenderContext& context, float x, float y, bool expanded, Rgb value) {
    // A 5-unit triangle: pointing right when collapsed, down when expanded.
    for (int i = 0; i < 3; ++i) {
        if (expanded) fill(context,x+i,y+1+i,5-2*i,1,value);
        else fill(context,x+1+i,y+i,1,5-2*i,value);
    }
}
void arrow(MinecraftUIRenderContext& context, float x, float y, bool left, Rgb value) {
    for (int i = 0; i < 3; ++i)
        fill(context,left ? x+i : x+2-i,y+2-i,1,1+2*i,value);
}
float keycaps(MinecraftUIRenderContext& context, float x, float y, float width, std::vector<std::string> const& keys) {
    float used = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) {
            if (used + 6 > width) break;
            label(context,x+used,y+boxTextInset(),6,"+",palette::faint,Align::Center);
            used += 6;
        }
        float capWidth = std::min(textWidth(context, keys[i]) + 6, width - used);
        if (capWidth < 8) break;
        fill(context,x+used,y,capWidth,capHeight,palette::keyFill);
        frame(context,x+used,y,capWidth,capHeight,palette::keyEdge);
        fill(context,x+used+1,y+capHeight-2,capWidth-2,1,Rgb{0,0,0},.5f);
        label(context,x+used+3,y+boxTextInset(),capWidth-5,keys[i]);
        used += capWidth;
    }
    return used;
}
}
