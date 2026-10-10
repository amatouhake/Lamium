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
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include <glm/vec2.hpp>
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "ui/Translations.h"
#include <algorithm>
#include <mutex>
#include <unordered_set>
#ifdef LAMIUM_RESEARCH_TRACE
#include "app/Runtime.h"
#endif

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
float textWidthScaled(MinecraftUIRenderContext& context, std::string_view text, float size) {
    if (!(size > 0) || !std::isfinite(size)) size = 1;
    return static_cast<float>(defaultFont(context).getLineLength(text, size, false));
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
// The engine shadow sits one GUI unit away, which reads as doubled on dense
// Japanese lines; Lamium draws its own, closer copy instead.
float const labelShadowOffset = .5f;
void drawRun(MinecraftUIRenderContext& context, Font& font, float x, float y, float width, std::string text, Rgb value,
             ::ui::TextAlignment align, float size) {
    TextMeasureData const measure{size, 0.0f, false, false, false, align};
    CaretMeasureData const caret{-1, false};
    context.drawText(font, RectangleArea{x,x+width,y,y+14*size}, std::move(text), color(value), size, align, measure, caret);
}
}
float boxTextInset() { return japaneseLocale() ? 0.f : 1.f; }
float shapeTextDrop() { return japaneseLocale() ? 1.f : 0.f; }
float labelWidth(MinecraftUIRenderContext& context, std::string_view text, float size) {
    if (!(size > 0) || !std::isfinite(size)) size = 1;
    auto& font = defaultFont(context);
    bool latin = std::any_of(text.begin(), text.end(), [](unsigned char ch) { return ch < 0x80 && ch != ' '; });
    if (!latinRaise() || !latin) return static_cast<float>(font.getLineLength(text, size, false));
    float total = 0;
    for (size_t start = 0; start < text.size();) {
        bool ascii = static_cast<unsigned char>(text[start]) < 0x80;
        size_t end = start;
        while (end < text.size() && (static_cast<unsigned char>(text[end]) < 0x80) == ascii) ++end;
        total += static_cast<float>(font.getLineLength(text.substr(start, end - start), size, false));
        start = end;
    }
    return total;
}
void label(MinecraftUIRenderContext& context, float x, float y, float width, std::string text, Rgb value, Align align) {
    labelScaled(context, x, y, width, std::move(text), 1.f, value, align);
}
void labelScaled(MinecraftUIRenderContext& context, float x, float y, float width, std::string text, float size,
                 Rgb value, Align align, bool shadow) {
    if (!(size > 0) || !std::isfinite(size)) size = 1;
    auto& font = defaultFont(context);
    auto measure = [&](std::string_view part) { return static_cast<float>(font.getLineLength(part, size, false)); };
    text = fitLabel(text, width, measure);
    if (text.empty()) return;
    auto paint = [&](float px, float py, Rgb ink) {
        float raise = latinRaise() * size;
        bool latin = std::any_of(text.begin(), text.end(), [](unsigned char ch) { return ch < 0x80 && ch != ' '; });
        if (!raise || !latin) {
            auto native = align == Align::Right ? ::ui::TextAlignment::Right
                : align == Align::Center ? ::ui::TextAlignment::Center : ::ui::TextAlignment::Left;
            drawRun(context, font, px, py, width, text, ink, native, size);
            return;
        }
        // Mixed or Latin-only text: the runs are drawn separately so Latin runs
        // can be raised. Right and center lines are laid out from the anchored
        // right edge with the engine's own right alignment, so a run's measured
        // width being slightly off stays between runs instead of moving the edge
        // (positioning runs left to right was visibly ragged).
        struct Run { std::string_view text; bool ascii; float width; };
        std::vector<Run> runs;
        float total = 0;
        for (size_t start = 0; start < text.size();) {
            bool ascii = static_cast<unsigned char>(text[start]) < 0x80;
            size_t end = start;
            while (end < text.size() && (static_cast<unsigned char>(text[end]) < 0x80) == ascii) ++end;
            std::string_view run(text.data() + start, end - start);
            float runWidth = measure(run);
            runs.push_back({run, ascii, runWidth});
            total += runWidth;
            start = end;
        }
        if (align == Align::Left) {
            float cursor = px;
            for (auto const& run : runs) {
                drawRun(context, font, cursor, run.ascii ? py - raise : py, run.width + 2, std::string(run.text), ink,
                        ::ui::TextAlignment::Left, size);
                cursor += run.width;
            }
            return;
        }
        float boundary = align == Align::Right ? px + width : px + (width + total) / 2;
        float cursor = boundary;
        for (auto it = runs.rbegin(); it != runs.rend(); ++it) {
            drawRun(context, font, px, it->ascii ? py - raise : py, std::max(cursor - px, it->width + 2),
                    std::string(it->text), ink, ::ui::TextAlignment::Right, size);
            cursor -= it->width;
        }
    };
    if (shadow) {
        float offset = labelShadowOffset * size;
        paint(x + offset, y + offset, Rgb{value.r * .25f, value.g * .25f, value.b * .25f});
    }
    paint(x, y, value);
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
size_t paragraphLines(MinecraftUIRenderContext& context, float width, std::string_view text, size_t maxLines) {
    auto& font = defaultFont(context);
    return wrapLabel(text, width, maxLines, [&](std::string_view part) { return font.getLineLength(part, 1.0f, false); }).size();
}
void panel(MinecraftUIRenderContext& context, float left, float top, float width, float height, float opacity) {
    fill(context,left,top,width,height,palette::panel,opacity);
}
void card(MinecraftUIRenderContext& context, float left, float top, float width, float height, float opacity) {
    if (width < 3 || height < 3) { fill(context,left,top,width,height,palette::panel,opacity); return; }
    fill(context,left+1,top,width-2,1,palette::panel,opacity);
    fill(context,left,top+1,width,height-2,palette::panel,opacity);
    fill(context,left+1,top+height-1,width-2,1,palette::panel,opacity);
}
void rowBackground(MinecraftUIRenderContext& context, float left, float top, float width, float height,
                   bool selected, bool hovered) {
    if (selected) {
        fill(context,left,top,width,height,palette::accent,.16f);
        frame(context,left,top,width,height,palette::accent,.9f);
    } else if (hovered) fill(context,left,top,width,height,palette::white,.07f);
}
void images(MinecraftUIRenderContext& context, std::string_view texture, std::vector<ImageRect> const& rects,
            float opacity) {
    if (rects.empty()) return;
    auto pointer = context.getTexture(ResourceLocation(Core::PathView(texture)), false);
    std::shared_ptr<BedrockTextureData const> const& data = pointer.mClientTexture;
    if (!data) return;
    for (auto const& r : rects)
        context.drawImage(*data->mClientTexture, glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h}, glm::vec2{0, 0},
                          glm::vec2{1, 1}, false);
    context.flushImages(white, std::clamp(opacity, 0.f, 1.f), HashedString{"ui_textured_and_glcolor"});
}
namespace {
// The ui loader answers an unknown path with its missing-texture placeholder,
// which would draw a magenta/black square. An empty result draws no icon.
std::shared_ptr<BedrockTextureData const> iconTexture(MinecraftUIRenderContext& context, std::string_view texture) {
    auto data = context.getTexture(ResourceLocation(Core::PathView(texture)), false).mClientTexture;
    if (!data) return {};
#ifdef LAMIUM_RESEARCH_TRACE
    {
        static std::mutex mutex;
        static std::unordered_set<std::string> logged;
        std::scoped_lock lock(mutex);
        if (logged.insert(std::string(texture)).second)
            try {
                IsMissingTexture missing = data->mIsMissingTexture;
                TextureLoadState state = data->mTextureLoadState;
                Runtime::instance().self().getLogger().info("L-58 load {} missing={} state={} size={}x{}", texture,
                                                            static_cast<int>(missing), static_cast<int>(state),
                                                            data->mTextureDescription->mWidth,
                                                            data->mTextureDescription->mHeight);
            } catch (...) {}
    }
#endif
    IsMissingTexture missing = data->mIsMissingTexture;
    return missing == IsMissingTexture::Yes ? std::shared_ptr<BedrockTextureData const>{} : data;
}
}
void imageUv(MinecraftUIRenderContext& context, std::string_view texture, ImageRect rect, float u0, float v0, float u1,
             float v1, float opacity) {
    auto data = iconTexture(context, texture);
    if (!data) return;
    context.drawImage(*data->mClientTexture, glm::vec2{rect.x, rect.y}, glm::vec2{rect.w, rect.h},
                      glm::vec2{u0, v0}, glm::vec2{u1 - u0, v1 - v0}, false);
    context.flushImages(white, std::clamp(opacity, 0.f, 1.f), HashedString{"ui_textured_and_glcolor"});
}
bool runtimeImage(MinecraftUIRenderContext& context, ResourceLocation const& texture, ImageRect rect, float opacity,
                  Rgb tint) {
    return runtimeImage(context, texture, rect, 0, 0, 1, 1, opacity, tint);
}
bool runtimeImage(MinecraftUIRenderContext& context, ResourceLocation const& texture, ImageRect rect, float u, float v,
                  float uSpan, float vSpan, float opacity, Rgb tint) {
    auto data = context.getTexture(texture, false).mClientTexture;
    if (!data) return false;
    IsMissingTexture missing = data->mIsMissingTexture;
    if (missing == IsMissingTexture::Yes) return false;
    context.drawImage(*data->mClientTexture, glm::vec2{rect.x, rect.y}, glm::vec2{rect.w, rect.h}, glm::vec2{u, v},
                      glm::vec2{uSpan, vSpan}, false);
    context.flushImages(color(tint), std::clamp(opacity, 0.f, 1.f), HashedString{"ui_textured_and_glcolor"});
    return true;
}
void toggleSwitch(MinecraftUIRenderContext& context, float x, float y, bool on) {
    fill(context,x,y,switchWidth,switchHeight,on ? palette::accentDeep : palette::off);
    frame(context,x,y,switchWidth,switchHeight,on ? palette::accent : Rgb{.18f,.18f,.19f});
    float knob = switchHeight - 2;
    float knobX = on ? x + switchWidth - 1 - knob : x + 1;
    fill(context,knobX,y+1,knob,knob,on ? palette::knobOn : palette::knobOff);
    fill(context,knobX,y+knob-1,knob,2,on ? Rgb{.71f,.71f,.72f} : Rgb{.6f,.61f,.62f});
}
void slider(MinecraftUIRenderContext& context, float x, float y, float width, float fraction, bool active) {
    if (width < sliderKnobWidth + 2) return;
    fraction = std::clamp(std::isfinite(fraction) ? fraction : 0.f, 0.f, 1.f);
    float knobX = x + (width - sliderKnobWidth) * fraction;
    float trackY = y + (sliderKnobHeight - sliderTrackHeight) / 2;
    float split = knobX + sliderKnobWidth / 2;
    fill(context,x,trackY,split-x,sliderTrackHeight,palette::accentDeep);
    fill(context,split,trackY,x+width-split,sliderTrackHeight,palette::off);
    frame(context,x,trackY,width,sliderTrackHeight,Rgb{.18f,.18f,.19f});
    fill(context,x+1,trackY,split-x-1,1,palette::accent);
    fill(context,knobX,y,sliderKnobWidth,sliderKnobHeight,active ? palette::white : palette::knobOn);
    fill(context,knobX,y+sliderKnobHeight-2,sliderKnobWidth,2,Rgb{.6f,.61f,.62f});
    frame(context,knobX,y,sliderKnobWidth,sliderKnobHeight,Rgb{.18f,.18f,.19f});
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
void changeArrow(MinecraftUIRenderContext& context, float x, float y, float scale, Rgb color) {
    fill(context, x, y + 2 * scale, 4 * scale, scale, color); // Shaft.
    for (int i = 0; i < 3; ++i)                               // Head, narrowing to the tip.
        fill(context, x + (4 + i) * scale, y + i * scale, scale, (5 - 2 * i) * scale, color);
}
float keycaps(MinecraftUIRenderContext& context, float x, float y, float width, std::vector<std::string> const& keys,
              KeyTone tone) {
    bool filled = tone == KeyTone::Filled;
    Rgb edge = tone == KeyTone::Plain ? palette::keyEdge : palette::warning;
    Rgb ink = tone == KeyTone::Outline ? palette::warning : filled ? palette::panel : palette::text;
    float used = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) {
            if (used + 6 > width) break;
            label(context,x+used,y+boxTextInset(),6,"+",palette::faint,Align::Center);
            used += 6;
        }
        float capWidth = std::min(textWidth(context, keys[i]) + 6, width - used);
        if (capWidth < 8) break;
        fill(context,x+used,y,capWidth,capHeight,filled ? palette::warning : palette::keyFill);
        frame(context,x+used,y,capWidth,capHeight,edge);
        fill(context,x+used+1,y+capHeight-2,capWidth-2,1,Rgb{0,0,0},filled ? .25f : .5f);
        // Dark text on the filled cap would smear with the font's shadow.
        labelScaled(context,x+used+3,y+boxTextInset(),capWidth-5,keys[i],1.f,ink,Align::Left,!filled);
        used += capWidth;
    }
    return used;
}
}
