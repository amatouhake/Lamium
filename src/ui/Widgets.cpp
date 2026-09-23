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
#include <algorithm>

namespace lamium::ui {
namespace {
constexpr mce::Color white{1.f,1.f,1.f,1.f};
void fill(MinecraftUIRenderContext& context, float x, float y, float width, float height,
          mce::Color color, float opacity) {
    if (width <= 0 || height <= 0) return;
    context.fillRectangle(RectangleArea{x,x+width,y,y+height}, color, opacity);
    context.flushImages(white,1,HashedString{"ui_fillColor"});
}
}
void label(MinecraftUIRenderContext& context, float x, float y, float width, std::string text) {
    auto& font = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default").getFont();
    text = fitLabel(text, width, [&](std::string_view value) { return font.getLineLength(value, 1.0f, false); });
    if (text.empty()) return;
    TextMeasureData const measure{1.0f, 0.0f, true, false, false, ::ui::TextAlignment::Left};
    CaretMeasureData const caret{-1, false};
    context.drawText(font, RectangleArea{x,x+width,y,y+14}, std::move(text), white, 1.0f,
        ::ui::TextAlignment::Left, measure, caret);
}
void paragraph(MinecraftUIRenderContext& context, float x, float y, float width, std::string_view text, size_t maxLines) {
    auto& font = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default").getFont();
    auto lines = wrapLabel(text, width, maxLines,
        [&](std::string_view value) { return font.getLineLength(value, 1.0f, false); });
    for (auto& line : lines) {
        label(context, x, y, width, std::move(line));
        y += 14;
    }
}
void panel(MinecraftUIRenderContext& context, float left, float top, float width, float height, float opacity) {
    fill(context,left,top,width,height,mce::Color{.07f,.08f,.11f,1.f},std::clamp(opacity,0.f,1.f));
}
void rowBackground(MinecraftUIRenderContext& context, float left, float top, float width, float height,
                   bool selected, bool hovered) {
    fill(context,left,top,width,height, selected ? mce::Color{.28f,.24f,.43f,1.f}
        : hovered ? mce::Color{.22f,.23f,.30f,1.f} : mce::Color{.15f,.16f,.21f,1.f},1);
}
}
