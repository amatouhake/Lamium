#include "features/inspection/render/DurabilityTooltip.h"
#include "mc/client/gui/screens/ScreenView.h"
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
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/Item.h"
#include <algorithm>
#include <format>

namespace lamium::inspection::render {
void renderDurability(ScreenView& view, MinecraftUIRenderContext& context, ItemStackBase const& item) {
    if (item.isNull() || !item.mItem || !item.isDamageableItem()) return;
    int maximum = item.mItem->getMaxDamage();
    if (maximum <= 0) return;
    int remaining = maximum - std::clamp<int>(item.getDamageValue(), 0, maximum);
    glm::vec2 pointer = view.mPointerLocationPrevious;
    glm::vec2 size = view.mSize;
    float width = std::min(175.0f, size.x);
    float x = std::clamp(pointer.x+8, 0.0f, std::max(0.0f, size.x-width));
    float y = pointer.y >= 28 ? pointer.y-28 : pointer.y+24;
    y = std::clamp(y, 0.0f, std::max(0.0f, size.y-18));
    constexpr mce::Color white{1.0f,1.0f,1.0f,1.0f};
    context.fillRectangle(RectangleArea{x,x+width,y,y+18}, mce::Color{.1f,.1f,.14f,1.0f},.95f);
    context.flushImages(white,1.0f,HashedString{"ui_fillColor"});
    auto& font = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default").getFont();
    TextMeasureData measure{1.0f,0.0f,true,false,false,::ui::TextAlignment::Left};
    CaretMeasureData caret{-1,false};
    context.drawText(font,RectangleArea{x+4,x+width-4,y+5,y+16},
        std::format("Durability: {} / {}",remaining,maximum),white,1.0f,::ui::TextAlignment::Left,measure,caret);
    context.flushText(0,std::nullopt);
}
}
