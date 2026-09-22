#include "features/inspection/render/DurabilityTooltip.h"
#include "ui/Localization.h"
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
#include "mc/client/gui/controls/MeasureResult.h"
#include "mc/client/gui/controls/UIMeasureStrategy.h"
#include "mc/deps/core/utility/NonOwnerPointer.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/Item.h"
#include <algorithm>

namespace lamium::inspection::render {
void renderDurability(ScreenView& view, MinecraftUIRenderContext& context, ItemStackBase const& item) {
    if (item.isNull() || !item.mItem || !item.isDamageableItem()) return;
    int maximum = item.mItem->getMaxDamage();
    if (maximum <= 0) return;
    int remaining = maximum - std::clamp<int>(item.getDamageValue(), 0, maximum);
    glm::vec2 pointer = view.mPointerLocationPrevious;
    glm::vec2 size = view.mSize;
    if (size.x <= 8 || size.y < 18) return;
    auto const& fontHandle = context.mClient.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default");
    auto& font = fontHandle.getFont();
    Bedrock::NotNullNonOwnerPtr<FontHandle const> const fontRef{Bedrock::NonOwnerPointer<FontHandle const>{fontHandle}};
    TextMeasureData measure{1.0f,0.0f,true,false,false,::ui::TextAlignment::Left};
    CaretMeasureData caret{-1,false};
    auto text = lamium::ui::translated("durabilityValue", remaining, maximum);
    glm::vec2 const textSize = context.getMeasureStrategy().measureText(fontRef, text, 1000, 1000, measure, caret).mSize;
    float width = std::min(textSize.x + 8.0f, size.x);
    float x = std::clamp(pointer.x+8, 0.0f, std::max(0.0f, size.x-width));
    float y = pointer.y >= 28 ? pointer.y-28 : pointer.y+24;
    y = std::clamp(y, 0.0f, std::max(0.0f, size.y-18));
    constexpr mce::Color white{1.0f,1.0f,1.0f,1.0f};
    context.fillRectangle(RectangleArea{x,x+width,y,y+18}, mce::Color{.1f,.1f,.14f,1.0f},.95f);
    context.flushImages(white,1.0f,HashedString{"ui_fillColor"});
    context.drawText(font,RectangleArea{x+4,x+width-4,y+5,y+16},
        std::move(text),white,1.0f,::ui::TextAlignment::Left,measure,caret);
    context.flushText(0,std::nullopt);
}
}
