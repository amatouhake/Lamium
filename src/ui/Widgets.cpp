#include "ui/Widgets.h"
#include "ui/KeycapLines.h"
#include "ui/TextFit.h"
#include "ui/VanillaFrame.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/Font.h"
#include "ui/GameFont.h"
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
#include <unordered_map>
#include <unordered_set>
#ifdef LAMIUM_RESEARCH_TRACE
#include "app/Runtime.h"
#endif

namespace lamium::ui {
namespace {
constexpr mce::Color white{1.f,1.f,1.f,1.f};
mce::Color color(Rgb value) { return {value.r, value.g, value.b, 1.f}; }
float currentScale = 1, currentOpacity = 1;
bool currentOnWindow = false;
Font& defaultFont(MinecraftUIRenderContext& context) {
    return fontOf(context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default"));
}
}
float drawScale() { return currentScale; }
DrawScale::DrawScale(float scale) : previous(currentScale) {
    currentScale = std::isfinite(scale) && scale > 0 ? scale : 1;
}
DrawScale::~DrawScale() { currentScale = previous; }
float drawOpacity() { return currentOpacity; }
DrawOpacity::DrawOpacity(float opacity) : previous(currentOpacity) {
    currentOpacity = std::isfinite(opacity) ? std::clamp(opacity, 0.f, 1.f) * previous : previous;
}
DrawOpacity::~DrawOpacity() { currentOpacity = previous; }
bool onWindow() { return currentOnWindow; }
OnWindow::OnWindow() : previous(std::exchange(currentOnWindow, true)) {}
OnWindow::~OnWindow() { currentOnWindow = previous; }
namespace {
// Characters the font measures 0 wide (TextFit.h, labelRuns): each is
// measured once per font and remembered.
Font const* quirkFont = nullptr;
std::unordered_map<std::string, bool> zeroWide;
bool zeroGlyph(Font& font, std::string_view glyph) {
    if (quirkFont != &font) { quirkFont = &font; zeroWide.clear(); }
    auto found = zeroWide.find(std::string(glyph));
    if (found == zeroWide.end())
        found = zeroWide.emplace(std::string(glyph), font.getLineLength(glyph, 1.0f, false) <= 0).first;
    return found->second;
}
// The width such a character is owed: a full-width character's.
float fullWidth(Font& font, float size) { return static_cast<float>(font.getLineLength("\xe4\xb8\xad", size, false)); }
// A text's width with its zero-wide characters counted in.
float measured(Font& font, std::string_view text, float size) {
    float width = static_cast<float>(font.getLineLength(text, size, false));
    if (std::none_of(text.begin(), text.end(), [](unsigned char ch) { return ch >= 0x80; })) return width;
    auto zeros = zeroGlyphs(text, [&](std::string_view glyph) { return zeroGlyph(font, glyph); });
    return zeros ? width + static_cast<float>(zeros) * fullWidth(font, size) : width;
}
}
float textWidth(MinecraftUIRenderContext& context, std::string_view text) {
    return measured(defaultFont(context), text, 1.0f);
}
float textWidthScaled(MinecraftUIRenderContext& context, std::string_view text, float size) {
    if (!(size > 0) || !std::isfinite(size)) size = 1;
    return measured(defaultFont(context), text, size);
}
void fill(MinecraftUIRenderContext& context, float x, float y, float width, float height, Rgb value, float opacity) {
    if (width <= 0 || height <= 0 || currentOpacity <= 0) return;
    float s = currentScale;
    context.fillRectangle(RectangleArea{x*s,(x+width)*s,y*s,(y+height)*s}, color(value), std::clamp(opacity,0.f,1.f) * currentOpacity);
    context.flushImages(white,1,HashedString{"ui_fillColor"});
}
void fills(MinecraftUIRenderContext& context, std::vector<ImageRect> const& rects, Rgb value, float opacity) {
    if (rects.empty() || currentOpacity <= 0) return;
    float s = currentScale, alpha = std::clamp(opacity,0.f,1.f) * currentOpacity;
    for (auto const& r : rects)
        if (r.w > 0 && r.h > 0)
            context.fillRectangle(RectangleArea{r.x*s,(r.x+r.w)*s,r.y*s,(r.y+r.h)*s}, color(value), alpha);
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
// Japanese only: there a run drawn on its own keeps the line's font. In Chinese it
// gets the pixel font, so a raise changes its shape and width as well (L-319).
bool japaneseLocale() { return translations::japanese(*getI18n().getCurrentLanguage()->mCode); }
float latinRaise() {
    auto locale = getI18n().getCurrentLanguage();
    return translations::japanese(*locale->mCode) ? 1.f : 0.f;
}
// The engine shadow sits one GUI unit away, which reads as doubled on dense
// Japanese lines; Lamium draws its own, closer copy instead.
float const labelShadowOffset = .5f;
void drawRun(MinecraftUIRenderContext& context, Font& font, float x, float y, float width, std::string text, Rgb value,
             ::ui::TextAlignment align, float size) {
    if (currentOpacity <= 0) return;
    float s = currentScale;
    TextMeasureData const measure{size*s, 0.0f, false, false, false, align};
    CaretMeasureData const caret{-1, false};
    // The text's opacity goes in the color: the next argument is its size.
    mce::Color ink = color(value);
    ink.a = currentOpacity;
    context.drawText(font, RectangleArea{x*s,(x+width)*s,y*s,(y+14*size)*s}, std::move(text), ink, size*s, align,
                     measure, caret);
}
}
float boxTextInset() { return japaneseLocale() ? 0.f : 1.f; }
float textStroke() { return japaneseLocale() ? .5f : 1.f; }
float shapeTextDrop() { return japaneseLocale() ? 1.f : 0.f; }
float rowTextLift() {
    auto const code = *getI18n().getCurrentLanguage()->mCode;
    return translations::localeFor(code) == translations::Locale::English ? 0.f : 1.f;
}
float labelWidth(MinecraftUIRenderContext& context, std::string_view text, float size) {
    if (!(size > 0) || !std::isfinite(size)) size = 1;
    auto& font = defaultFont(context);
    bool latin = std::any_of(text.begin(), text.end(), [](unsigned char ch) { return ch < 0x80 && ch != ' '; });
    if (!latinRaise() || !latin) return measured(font, text, size);
    float total = 0;
    for (auto const& run : labelRuns(text, [&](std::string_view glyph) { return zeroGlyph(font, glyph); }))
        total += measured(font, run.text, size);
    return total;
}
void label(MinecraftUIRenderContext& context, float x, float y, float width, std::string text, Rgb value, Align align) {
    labelScaled(context, x, y, width, std::move(text), 1.f, value, align);
}
void labelScaled(MinecraftUIRenderContext& context, float x, float y, float width, std::string text, float size,
                 Rgb value, Align align, bool shadow) {
    if (!(size > 0) || !std::isfinite(size)) size = 1;
    auto& font = defaultFont(context);
    auto measure = [&](std::string_view part) { return measured(font, part, size); };
    text = fitLabel(text, width, measure);
    if (text.empty()) return;
    auto parts = labelRuns(text, [&](std::string_view glyph) { return zeroGlyph(font, glyph); });
    bool zeros = std::any_of(parts.begin(), parts.end(), [](LabelRun const& run) { return run.kind == RunKind::Zero; });
    auto paint = [&](float px, float py, Rgb ink) {
        float raise = latinRaise() * size;
        bool latin = std::any_of(text.begin(), text.end(), [](unsigned char ch) { return ch < 0x80 && ch != ' '; });
        if ((!raise || !latin) && !zeros) {
            auto native = align == Align::Right ? ::ui::TextAlignment::Right
                : align == Align::Center ? ::ui::TextAlignment::Center : ::ui::TextAlignment::Left;
            drawRun(context, font, px, py, width, text, ink, native, size);
            return;
        }
        // Mixed or Latin-only text: the runs are drawn separately so Latin runs
        // can be raised. Right and center lines are laid out from the anchored
        // right edge with the engine's own right alignment, so a run's measured
        // width being slightly off stays between runs instead of moving the edge
        // (positioning runs left to right was visibly ragged). A zero-wide
        // character is drawn one width left of the pen: from the left its pen
        // starts a width further right, and right alignment already ends it
        // at the run's right edge.
        struct Run { std::string_view text; bool ascii, zero; float width; };
        std::vector<Run> runs;
        float total = 0;
        for (auto const& part : parts) {
            float runWidth = measure(part.text);
            runs.push_back({part.text, part.kind == RunKind::Ascii, part.kind == RunKind::Zero, runWidth});
            total += runWidth;
        }
        if (align == Align::Left) {
            float cursor = px;
            for (auto const& run : runs) {
                drawRun(context, font, run.zero ? cursor + run.width : cursor, run.ascii ? py - raise : py, run.width + 2,
                        std::string(run.text), ink, ::ui::TextAlignment::Left, size);
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
    auto lines = wrapLabel(text, width, maxLines, [&](std::string_view part) { return measured(font, part, 1.0f); });
    for (auto& line : lines) {
        label(context, x, y, width, std::move(line), value);
        y += 12;
    }
}
size_t paragraphLines(MinecraftUIRenderContext& context, float width, std::string_view text, size_t maxLines) {
    auto& font = defaultFont(context);
    return wrapLabel(text, width, maxLines, [&](std::string_view part) { return measured(font, part, 1.0f); }).size();
}
void panel(MinecraftUIRenderContext& context, float left, float top, float width, float height, float opacity) {
    fill(context,left,top,width,height,palette::panel,opacity);
}
void windowPanel(MinecraftUIRenderContext& context, float left, float top, float width, float height) {
    using vanilla_frame::Part;
    for (auto const& r : vanilla_frame::window(left, top, width, height)) {
        Rgb const color = r.part == Part::Light ? palette::vanilla::windowLight : r.part == Part::Dark ? palette::vanilla::windowShadow
            : r.part == Part::Edge ? palette::vanilla::windowEdge : palette::vanilla::window;
        fill(context, r.x, r.y, r.w, r.h, color);
    }
}
void sunkFace(MinecraftUIRenderContext& context, float x, float y, float w, float h, bool top, bool bottom) {
    if (w <= 2 || h <= 0) return;
    namespace v = palette::vanilla;
    fill(context, x, y, w, h, v::windowSunk);
    fill(context, x, y, 1, h, v::buttonShadow);
    fill(context, x + w - 1, y, 1, h, v::windowLight);
    if (top) fill(context, x, y, w, 1, v::buttonShadow);
    if (bottom) fill(context, x + 1, y + h - 1, w - 1, 1, v::windowLight);
}
void raisedBox(MinecraftUIRenderContext& context, float x, float y, float w, float h, Rgb face, Rgb light, Rgb shadow) {
    if (w < 3 || h < 3) return;
    // Half a unit of light and shadow: a whole unit was heavy on buttons this small (L-367).
    constexpr float e = .5f;
    fill(context, x, y, w, h, face);
    fill(context, x + 1, y + 1, w - 2, e, light);
    fill(context, x + 1, y + 1 + e, e, h - 2 - e, light);
    fill(context, x + 1, y + h - 1 - e, w - 2, e, shadow);
    fill(context, x + w - 1 - e, y + 1, e, h - 2 - e, shadow);
    frame(context, x, y, w, h, palette::vanilla::controlEdge);
}
void progressBar(MinecraftUIRenderContext& context, float x, float y, float w, float h, float fraction) {
    if (w <= 0 || h <= 0) return;
    fraction = std::clamp(std::isfinite(fraction) ? fraction : 0.f, 0.f, 1.f);
    bool const light = onWindow();
    fill(context, x, y, w, h, light ? palette::vanilla::slot : palette::off);
    if (fraction > 0) fill(context, x, y, w * fraction, h, light ? palette::accentDeep : palette::accent);
    fill(context, x, y, w, .5f, light ? palette::vanilla::slotDark : palette::controlEdge);
}
void gameTooltipBox(MinecraftUIRenderContext& context, float x, float y, float width, float height) {
    for (auto const& r : vanilla_frame::tooltip(x, y, width, height))
        fill(context, r.x, r.y, r.w, r.h, r.part == vanilla_frame::Part::Border ? palette::vanilla::tooltipLine : palette::vanilla::tooltipFill);
}
void windowLabel(MinecraftUIRenderContext& context, float x, float y, float width, std::string text, bool faint, Align align) {
    labelScaled(context,x,y,width,std::move(text),1.f,faint ? palette::vanilla::windowFaint : palette::vanilla::windowInk,align,false);
}
void anyOrderMark(MinecraftUIRenderContext& context, float x, float y, Rgb color) {
    fill(context,x,y+1,3,3,color);
    fill(context,x+5,y,3,3,color);
    fill(context,x+3,y+6,3,3,color);
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
    if (rects.empty() || currentOpacity <= 0) return;
    auto pointer = context.getTexture(ResourceLocation(Core::PathView(texture)), false);
    std::shared_ptr<BedrockTextureData const> const& data = pointer.mClientTexture;
    if (!data) return;
    for (auto const& r : rects)
        context.drawImage(*data->mClientTexture, glm::vec2{r.x, r.y} * currentScale, glm::vec2{r.w, r.h} * currentScale, glm::vec2{0, 0},
                          glm::vec2{1, 1}, false);
    context.flushImages(white, std::clamp(opacity, 0.f, 1.f) * currentOpacity, HashedString{"ui_textured_and_glcolor"});
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
             float v1, float opacity, Rgb tint) {
    if (currentOpacity <= 0) return;
    auto data = iconTexture(context, texture);
    if (!data) return;
    context.drawImage(*data->mClientTexture, glm::vec2{rect.x, rect.y} * currentScale, glm::vec2{rect.w, rect.h} * currentScale,
                      glm::vec2{u0, v0}, glm::vec2{u1 - u0, v1 - v0}, false);
    context.flushImages(mce::Color{tint.r, tint.g, tint.b, 1.f}, std::clamp(opacity, 0.f, 1.f) * currentOpacity,
                        HashedString{"ui_textured_and_glcolor"});
}
bool runtimeImage(MinecraftUIRenderContext& context, ResourceLocation const& texture, ImageRect rect, float opacity) {
    return runtimeImage(context, texture, rect, 0, 0, 1, 1, opacity);
}
bool runtimeImage(MinecraftUIRenderContext& context, ResourceLocation const& texture, ImageRect rect, float u, float v,
                  float uSpan, float vSpan, float opacity) {
    auto data = context.getTexture(texture, false).mClientTexture;
    if (!data) return false;
    IsMissingTexture missing = data->mIsMissingTexture;
    if (missing == IsMissingTexture::Yes) return false;
    if (currentOpacity <= 0) return true;
    context.drawImage(*data->mClientTexture, glm::vec2{rect.x, rect.y} * currentScale, glm::vec2{rect.w, rect.h} * currentScale, glm::vec2{u, v},
                      glm::vec2{uSpan, vSpan}, false);
    context.flushImages(white, std::clamp(opacity, 0.f, 1.f) * currentOpacity, HashedString{"ui_textured_and_glcolor"});
    return true;
}
void windowSlot(MinecraftUIRenderContext& context, float x, float y, bool hovered, bool missing) {
    namespace v = palette::vanilla;
    fill(context, x, y, 18, 18, missing ? v::slotMissing : v::slot);
    // Only the face turns red for what is missing; the edges stay the slot's, as the game draws it (L-367).
    fill(context, x, y, 18, 1, v::slotDark);
    fill(context, x, y + 1, 1, 17, v::slotDark);
    fill(context, x + 1, y + 17, 17, 1, v::slotLight);
    fill(context, x + 17, y + 1, 1, 16, v::slotLight);
    if (hovered) fill(context, x + 1, y + 1, 16, 16, palette::white, .3f);
}
void windowCheckBox(MinecraftUIRenderContext& context, float x, float y, float size, Check check, bool hovered) {
    namespace v = palette::vanilla;
    if (size < 7) return;
    // As windowSlot: the face, dark above and left, light below and right, no black edge.
    fill(context, x, y, size, size, v::slot);
    fill(context, x, y, size, 1, v::slotDark);
    fill(context, x, y + 1, 1, size - 1, v::slotDark);
    fill(context, x + 1, y + size - 1, size - 1, 1, v::slotLight);
    fill(context, x + size - 1, y + 1, 1, size - 2, v::slotLight);
    if (hovered) fill(context, x + 1, y + 1, size - 2, size - 2, palette::white, .3f);
    if (check == Check::None) return;
    // A check drawn in steps, as wide as the box allows: down to the corner, then up to the right.
    Rgb const ink = check == Check::Mine ? palette::accentDeep : v::windowInk;
    float const k = (size - 2) / 9, ox = x + 1, oy = y + 1;
    for (int i = 0; i < 3; ++i) fill(context, ox + (1 + i) * k, oy + (4 + i) * k, 1.5f * k, 1.5f * k, ink);
    for (int i = 0; i < 5; ++i) fill(context, ox + (3 + i) * k, oy + (6 - i) * k, 1.5f * k, 1.5f * k, ink);
}
void drawTab(MinecraftUIRenderContext& context, float x, float y, float w, std::string text, bool selected, bool hovered, bool first, float height) {
    (void)first;
    if (selected) {
        fill(context, x, y, w, height, palette::accent, .18f);
        fill(context, x, y + height - 1, w, 1, palette::accent);
    } else if (hovered) fill(context, x, y, w, height, palette::white, .07f);
    if (!text.empty()) label(context, x, y + (height - tabHeight) / 2 + boxTextInset(), w, std::move(text), selected ? palette::text : palette::dim, Align::Center);
}
void tabRule(MinecraftUIRenderContext& context, float x, float y, float w) {
    fill(context, x, y, w, 1, palette::white, .22f);
}
void windowTab(MinecraftUIRenderContext& context, float x, float windowTop, float w, std::string text, bool selected, bool hovered) {
    namespace v = palette::vanilla;
    // The window's own top corners (VanillaFrame.h); the chosen tab reaches 3 units down over the window's
    // edge, its sides stopping on the edge's black line.
    using vanilla_frame::Part;
    float const y0 = windowTop - windowTabRise, h = (selected ? windowTop + 3 : windowTop) - y0;
    for (auto const& r : vanilla_frame::tab(x, y0, w, h, selected)) {
        float rh = r.h;
        if (r.part == Part::Edge && r.y + r.h > windowTop + 1) rh = windowTop + 1 - r.y; // the sides end on the window's edge
        Rgb const color = r.part == Part::Fill ? (selected ? v::window : v::tabBack) : r.part == Part::Light ? (selected ? v::windowLight : v::tabRim)
            : r.part == Part::Dark ? v::windowShadow : v::windowEdge;
        fill(context, r.x, r.y, r.w, rh, color);
    }
    if (hovered && !selected) fill(context, x + 3, y0 + 3, w - 5, h - 3, palette::white, .12f);
    if (!text.empty())
        labelScaled(context, x + 2, windowTop - 14 + boxTextInset(), w - 4, std::move(text), 1.f, selected ? v::windowInk : palette::text, Align::Center, !selected);
}
void slot(MinecraftUIRenderContext& context, float x, float y, bool hovered) {
    fill(context, x + 1, y + 1, 16, 16, palette::keyFill, .9f);
    if (hovered) fill(context, x + 1, y + 1, 16, 16, palette::white, .14f);
}
void slotCount(MinecraftUIRenderContext& context, float x, float y, std::string text) {
    // Inside the slot's edge, right-aligned a unit in and a unit up: not where the game puts a stack's count (it
    // sits further right and lower), kept for being easier to read (maintainer 2026-11-21).
    // Measured, never cut: three digits are 18 wide in the English font, more than the slot's inside (2026-11-21).
    // A fourth character ("749k", "1.2M") draws the count smaller, at most three quarters and within the slot's 16
    // inside whatever the font, its foot kept; drawn twice half a unit apart so it keeps the weight of the full size.
    float const whole = labelWidth(context, text, 1);
    float const size = text.size() > 3 && whole > 0 ? std::min(.75f, 16 / whole) : 1.f, w = labelWidth(context, text, size);
    float const left = x + 17 - w, top = y + 8 + 8 * (1 - size);
    if (size < 1) labelScaled(context, left - .5f, top, w + 1, text, size, palette::text, Align::Left);
    labelScaled(context, left, top, w + 1, std::move(text), size, palette::text, Align::Left, size >= 1);
}
void anyMark(MinecraftUIRenderContext& context, float x, float y, float scale) {
    // Inside the icon's corner, so it stays within a slot's edge (maintainer 2026-11-21); "*" reads as any.
    labelScaled(context, x, y - 1.5f, 8, "*", scale < 1 ? .625f : .875f, palette::text, Align::Left, true);
}
void toggleSwitch(MinecraftUIRenderContext& context, float x, float y, bool on, float scale) {
    if (!(scale > 0)) scale = 1;
    float const w = switchWidth * scale, h = switchHeight * scale;
    // On the game's window the empty track is the game's lighter grey and the edge black, as its controls'.
    bool const light = onWindow();
    fill(context,x,y,w,h,on ? palette::accentDeep : light ? palette::vanilla::switchTrack : palette::off);
    frame(context,x,y,w,h,light ? palette::vanilla::controlEdge : on ? palette::accent : palette::controlEdge);
    float knob = h - 2;
    float knobX = on ? x + w - 1 - knob : x + 1;
    fill(context,knobX,y+1,knob,knob,on ? palette::knobOn : palette::knobOff);
    fill(context,knobX,y+1+knob-2*scale,knob,2*scale,on ? palette::gripOn : palette::gripOff);
}
void slider(MinecraftUIRenderContext& context, float x, float y, float width, float fraction, bool active) {
    if (width < sliderKnobWidth + 2) return;
    fraction = std::clamp(std::isfinite(fraction) ? fraction : 0.f, 0.f, 1.f);
    float knobX = x + (width - sliderKnobWidth) * fraction;
    float trackY = y + (sliderKnobHeight - sliderTrackHeight) / 2;
    float split = knobX + sliderKnobWidth / 2;
    fill(context,x,trackY,split-x,sliderTrackHeight,palette::accentDeep);
    fill(context,split,trackY,x+width-split,sliderTrackHeight,palette::off);
    frame(context,x,trackY,width,sliderTrackHeight,palette::controlEdge);
    fill(context,x+1,trackY,split-x-1,1,palette::accent);
    fill(context,knobX,y,sliderKnobWidth,sliderKnobHeight,active ? palette::white : palette::knobOn);
    fill(context,knobX,y+sliderKnobHeight-2,sliderKnobWidth,2,palette::gripOff);
    frame(context,knobX,y,sliderKnobWidth,sliderKnobHeight,palette::controlEdge);
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
float keycapsWidth(MinecraftUIRenderContext& context, std::vector<std::string> const& keys) {
    float width = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) width += 6;
        width += textWidth(context, keys[i]) + 6;
    }
    return width;
}
namespace {
std::vector<float> capWidths(MinecraftUIRenderContext& context, std::vector<std::string> const& keys) {
    std::vector<float> widths;
    widths.reserve(keys.size());
    for (auto const& key : keys) widths.push_back(textWidth(context, key));
    return widths;
}
}
float keycapsWrapHeight(MinecraftUIRenderContext& context, float width, std::vector<std::string> const& keys, float lineGap) {
    auto lines = keycapLines(capWidths(context, keys), width);
    return lines.empty() ? 0.f : static_cast<float>(lines.size()) * capHeight + (static_cast<float>(lines.size()) - 1) * lineGap;
}
float keycapsWrap(MinecraftUIRenderContext& context, float x, float y, float width, std::vector<std::string> const& keys,
                  KeyTone tone, float lineGap) {
    float used = 0;
    for (auto [first, last] : keycapLines(capWidths(context, keys), width)) {
        keycaps(context,x,y+used,width,
            std::vector<std::string>(keys.begin()+static_cast<std::ptrdiff_t>(first),keys.begin()+static_cast<std::ptrdiff_t>(last)),tone);
        used += capHeight + lineGap;
    }
    return used ? used - lineGap : 0;
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
        fill(context,x+used+1,y+capHeight-2,capWidth-2,1,palette::shade,filled ? .25f : .5f);
        // Dark text on the filled cap would smear with the font's shadow.
        labelScaled(context,x+used+3,y+boxTextInset(),capWidth-5,keys[i],1.f,ink,Align::Left,!filled);
        used += capWidth;
    }
    return used;
}
}
