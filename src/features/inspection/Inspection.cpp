#include "features/inspection/Inspection.h"
#include <utility>
#include "features/inspection/hover/HoverTracker.h"
#include "features/inspection/preview/HoveredPreviewCache.h"
#include "features/inspection/render/PreviewRenderer.h"
#include "features/information/SaturationHud.h"
#include "features/inspection/TooltipGlyphs.h"
#include "features/inspection/ItemDetails.h"
#include "ui/Widgets.h"
#include <format>
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/world/item/ItemTag.h"
#include "mc/locale/I18n.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/attribute/Attribute.h"
#include "mc/world/attribute/AttributeModifier.h"
#include <optional>
#include <atomic>
#include "mc/world/item/components/FuelItemComponent.h"
#include "mc/world/item/components/UseModifiersItemComponent.h"
#include <cmath>
#include <variant>
#include "mc/world/level/block/actor/FurnaceBlockActor.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/gui/controls/renderers/HoverTextRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/world/item/components/IFoodItemComponent.h"
#include <string_view>
#include <vector>
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inventory/EnchantPlan.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/safety/RedactableString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/ShulkerBoxBlockItem.h"
#include "ui/Localization.h"
#include <algorithm>

namespace lamium::inspection {
namespace {
preview::HoveredPreviewCache cache;
render::PreviewRenderer renderer;
ll::event::ListenerPtr renderListener, exitListener;
bool tooltipHookInstalled = false, durabilityHookInstalled = false, foodHooksInstalled = false;
// Food values by the hover text that carries their glyph line. The game
// builds hover text for more items than the one shown, so the painter looks
// the drawn text up instead of trusting the latest one.
struct TooltipFood { std::string text; std::vector<information::saturation::FoodIcon> icons; };
std::vector<TooltipFood> tooltipFood;
constexpr size_t tooltipFoodKept = 32;
void rememberFood(std::string text, std::vector<information::saturation::FoodIcon> icons) {
    std::erase_if(tooltipFood, [&](TooltipFood const& entry) { return entry.text == text; });
    if (tooltipFood.size() >= tooltipFoodKept) tooltipFood.erase(tooltipFood.begin());
    tooltipFood.push_back({std::move(text), std::move(icons)});
}
std::vector<information::saturation::FoodIcon> const* foodFor(std::string_view text) {
    for (auto it = tooltipFood.rbegin(); it != tooltipFood.rend(); ++it)
        if (it->text == text) return &it->icons;
    return nullptr;
}
// The effect a shown line starts with: the game's effects by their translated names, the longest that fits. Whether
// it is harmful, and what it does to attributes at its first level in the game's words. Asked again only when the
// line changes.
struct EffectInfo {
    std::optional<bool> harmful;
    std::vector<std::string> modifiers;
};
EffectInfo const& effectInfo(std::string const& shown) {
    static std::string last;
    static EffectInfo answer;
    if (shown == last) return answer;
    last = shown;
    answer = {};
    auto language = getI18n().getCurrentLanguage();
    size_t best = 0;
    for (uint id = 1; id <= 38; ++id) {
        auto const name = MobEffect::getNameById(id);
        auto const* effect = name.empty() ? nullptr : MobEffect::getByName(name);
        if (!effect) continue;
        std::string const& key = effect->mDescriptionId;
        auto const label = getI18n().get(key, language);
        if (label.size() <= best || label == key || !shown.starts_with(label)) continue;
        best = label.size();
        answer.harmful = static_cast<bool>(effect->mIsHarmful);
        answer.modifiers.clear();
        for (auto const& [attribute, modifier] : *effect->mAttributeModifiers) {
            if (!attribute || !modifier) continue;
            auto const attributeKey = "attribute.name." + attribute->mName->getString();
            auto const attributeName = getI18n().get(attributeKey, language);
            float const value = effect->getAttributeModifierValue(0, *modifier);
            if (attributeName != attributeKey && std::isfinite(value) && value != 0)
                answer.modifiers.push_back(details::modifierLine(value, static_cast<int>(modifier->mOperation), attributeName));
        }
    }
    return answer;
}
LL_TYPE_INSTANCE_HOOK(ShulkerContentsText, ll::memory::HookPriority::Normal, ShulkerBoxBlockItem,
    &ShulkerBoxBlockItem::$appendFormattedHovertext, void, ItemStackBase const& stack,
    Level& level, Bedrock::Safety::RedactableString& hovertext, bool const showCategory) {
    auto& runtime = Runtime::instance();
    auto const preferences = runtime.snapshot()->inspection;
    if (runtime.enabled() && preferences.containerPreviews && preferences.shulkerPreviews
        && preferences.hideShulkerContents) {
        // Keep the generic item text (name/lore/etc.); only skip the Shulker
        // specialization which appends the contained-item list.
        Item::$appendFormattedHovertext(stack, level, hovertext, showCategory);
    } else origin(stack, level, hovertext, showCategory);
}
// Lamium's lines at the end of the game's own tooltip (L-92): remaining
// durability in its gray, then a line of food glyphs that the painter below
// covers with hunger-bar icons. Unpainted glyphs still read as food.
LL_TYPE_INSTANCE_HOOK(DurabilityHovertext, ll::memory::HookPriority::Normal, ItemStackBase,
    &ItemStackBase::getFormattedHovertext, Bedrock::Safety::RedactableString, Level& level, bool showCategory) {
    auto text = origin(level, showCategory);
    try {
        auto& runtime = Runtime::instance();
        if (!runtime.enabled() || isNull() || !mItem) return text;
        auto const snapshot = runtime.snapshot();
        auto const& settings = snapshot->inspection;
        if (!settings.itemTooltip) return text;
        if (settings.durability && isDamageableItem()) {
            int maximum = mItem->getMaxDamage();
            if (maximum > 0) {
                int remaining = maximum - std::clamp<int>(getDamageValue(), 0, maximum);
                text += "\n\u00a77" + ui::translated("durabilityValue", remaining, maximum);
            }
        }
        // The anvil's surcharge on this item (L-319). Read the way the enchantment order reads it, and under the
        // same capability: what the number means was checked against the anvil screen for that feature.
        if (settings.workPenalty && versionSensitiveAllowed(capabilities::Id::EnchantOrder))
            if (auto const penalty = inventory::enchantPlan::workPenalty(getBaseRepairCost()))
                text += "\n\u00a77" + (penalty->uses ? ui::translated("workPenaltyValue", penalty->levels, *penalty->uses)
                                                      : ui::translated("workPenaltyOnly", penalty->levels));
        // The game tells a suspicious stew's effect only in Creative (L-319): the same lines elsewhere, where the
        // game puts them, above the food.
        if (settings.itemDetailsStew && mItem->mFullName->getString() == "minecraft:suspicious_stew") {
            auto const effect = details::plain(mItem->buildEffectDescriptionName(*this, true));
            if (!effect.empty() && details::plain(text.mUnredactedString).find(effect) == std::string::npos) {
                auto const& info = effectInfo(effect);
                auto const tint = std::string("\n\u00a7") + details::effectTint(info.harmful);
                text += tint + effect;
                for (auto const& line : info.modifiers) text += tint + line;
            }
        }
        // The glyph line goes above Lamium's detail lines; it is remembered with the whole text further down.
        std::vector<information::saturation::FoodIcon> icons;
        if (settings.foodValues)
            if (auto* food = mItem->getFood()) {
                icons = information::saturation::foodIcons(food->getNutrition(), food->getSaturationModifier());
                if (!icons.empty()) text += tooltip::glyphLine(tooltip::placeholders(static_cast<int>(icons.size())));
            }
        // The rest the game knows and does not print (L-319).
        if (settings.itemDetails || settings.itemDetailsId || settings.itemDetailsTags || settings.itemDetailsNbt) {
            details::Item item;
            if (settings.itemDetailsId) {
                item.id = getTypeName();
                item.aux = getAuxValue();
            }
            if (settings.itemDetailsTags)
                for (auto const& tag : *mItem->mTags) item.tags.push_back(tag.getString());
            // A compound or list is its whole text, cut to a line by the pure side; past the lines shown only counted.
            if (settings.itemDetailsNbt && mUserData)
                for (auto const& [key, tag] : mUserData->mTags)
                    item.data.push_back({key, item.data.size() < details::dataLines ? tag.toSnbt(SnbtFormat::Minimize) : std::string{}});
            if (settings.itemDetails) {
                item.enchantability = mItem->getEnchantValue();
                item.maxStack = getMaxStackSize();
                item.cooldownTicks = mItem->getCooldownDuration();
                // Seconds in the item's components. Coal's burn time is only there: the furnace's own answer for it is 0.
                auto seconds = [&](char const* id, auto read) -> float {
                    auto* component = mItem->getComponent(HashedString{id});
                    float const value = component ? read(component) : 0;
                    return std::isfinite(value) && value > 0 && value < 100000 ? value * details::ticksPerSecond : 0;
                };
                item.burnTicks = FurnaceBlockActor::getItemBurnDuration(*mItem, details::smeltTicks);
                if (!(item.burnTicks > 0))
                    item.burnTicks = seconds("minecraft:fuel", [](ItemComponent* c) { return static_cast<FuelItemComponent*>(c)->mFuelDuration; });
                // Eating and drinking times are the item's own; newer items keep theirs in a component.
                item.useTicks = static_cast<float>(mItem->mMaxUseDuration);
                if (!(item.useTicks > 0))
                    item.useTicks = seconds("minecraft:use_modifiers", [](ItemComponent* c) { return static_cast<UseModifiersItemComponent*>(c)->mUseDuration; });
            }
            for (auto const& line : details::lines(item))
                text += (line.dim ? "\n\u00a78" : "\n\u00a77") + (line.key.empty() ? line.value : ui::translated(line.key, line.value));
        }
        if (!icons.empty()) rememberFood(text.mUnredactedString, std::move(icons));
    } catch (...) {
    }
    return text;
}
// The tooltip text reaches the font in one call with its top-left corner;
// keep the call that carries the glyph line while the tooltip renders.
struct GlyphDraw { Font* font; float x, y; tooltip::GlyphRun run; std::vector<information::saturation::FoodIcon> icons; };
bool tooltipRendering = false;
void (*beforeTooltip)(MinecraftUIRenderContext&) = nullptr;
// The hover text's box in this frame's UI pass, for the container preview to stay clear of. Tagged with the UI
// frame it was drawn in; the preview, drawn after the UI pass, ignores one from an earlier frame.
struct TooltipBox { render::Rect rect; unsigned frame; };
std::optional<TooltipBox> tooltipBox;
std::optional<std::pair<float, float>> tooltipTextAt;
unsigned uiFrame = 0;
std::optional<GlyphDraw> glyphDraw;
LL_TYPE_INSTANCE_HOOK(TooltipFontDraw, ll::memory::HookPriority::Normal, Font, &Font::$drawCached, void,
    ScreenContext& screenContext, std::string_view str, float x, float y, mce::Color const& color,
    bool ignoreColorFormatting, bool darken, bool drawColorSymbol, mce::MaterialPtr const* optionalMat,
    int caretPosition, bool shadow, float linePadding, mce::Color const& resetColorOverride,
    mce::Color const& shaderDarkColor, float outlineWidth, float yCaretOffset,
    OffscreenCaptureDescription const& offscreenCaptureDescription, bool autoGenNormalsAndTangents) {
    if (tooltipRendering && !tooltipTextAt) tooltipTextAt = std::pair{x, y};
    if (tooltipRendering && !glyphDraw)
        if (auto run = tooltip::findGlyphRun(str)) {
            auto const* icons = foodFor(str);
            if (icons && tooltip::placeholders(static_cast<int>(icons->size())) == run->count) glyphDraw = GlyphDraw{this, x, y, *run, *icons};
            else {
                // The drawn text should be the hover text verbatim; say once if not.
                static bool told = false;
                if (!told) {
                    told = true;
                    Runtime::instance().self().getLogger().info(
                        "Food tooltip: drawn text ({} bytes) matches none of {} remembered hover texts", str.size(),
                        tooltipFood.size());
                }
            }
        }
    origin(screenContext, str, x, y, color, ignoreColorFormatting, darken, drawColorSymbol, optionalMat, caretPosition,
           shadow, linePadding, resetColorOverride, shaderDarkColor, outlineWidth, yCaretOffset,
           offscreenCaptureDescription, autoGenNormalsAndTangents);
}
LL_TYPE_INSTANCE_HOOK(TooltipPainter, ll::memory::HookPriority::Normal, HoverTextRenderer, &HoverTextRenderer::$render,
    void, MinecraftUIRenderContext& context, IClientInstance& client, UIControl& owner, int pass) {
    if (beforeTooltip) try { beforeTooltip(context); } catch (...) {}
    glyphDraw.reset();
    tooltipTextAt.reset();
    tooltipRendering = true;
    origin(context, client, owner, pass);
    tooltipRendering = false;
    try {
        glm::vec2 const at = *mCursorPosition + *mOffset, size = *mBoxDimensions;
        if (size.x > 0 && size.y > 0) tooltipBox = TooltipBox{{at.x, at.y, at.x + size.x, at.y + size.y}, uiFrame};
        static bool told = false;
        if (!told && tooltipTextAt) {
            told = true;
            Runtime::instance().self().getLogger().info("Hover text box: cursor ({}, {}) offset ({}, {}) size ({}, {}), text at ({}, {})",
                mCursorPosition->x, mCursorPosition->y, mOffset->x, mOffset->y, size.x, size.y, tooltipTextAt->first,
                tooltipTextAt->second);
        }
    } catch (...) {
    }
    try {
        if (!glyphDraw) return;
        // Text is batched and drawn after the tooltip renders; draw it now so
        // the icons land on top of the glyphs.
        context.flushText(0, std::nullopt);
        // Spaced like the hunger bar and counting from the right, within the
        // room the placeholders reserve.
        static bool measured = false;
        if (!measured) {
            measured = true;
            Runtime::instance().self().getLogger().info("Food tooltip: placeholder glyph is {} units wide",
                glyphDraw->font->getLineLength(tooltip::glyph, 1, false));
        }
        float top = glyphDraw->y + glyphDraw->run.line * tooltip::lineHeight;
        std::vector<ui::ImageRect> rects;
        int count = static_cast<int>(glyphDraw->icons.size());
        for (int i = 0; i < count; ++i)
            rects.push_back({glyphDraw->x + (count - 1 - i) * 8.f, top, 9, 9});
        information::drawFoodIcons(context, glyphDraw->icons, rects);
    } catch (...) {
    }
}
}
void setBeforeTooltip(void (*painter)(MinecraftUIRenderContext&)) { beforeTooltip = painter; }
bool start() {
    try {
        tooltipHookInstalled = ShulkerContentsText::hook(true) == 0;
        if (!tooltipHookInstalled) throw std::runtime_error("Could not install Shulker contents text hook");
        durabilityHookInstalled = DurabilityHovertext::hook(true) == 0;
        if (!durabilityHookInstalled) throw std::runtime_error("Could not install durability tooltip hook");
        foodHooksInstalled = TooltipFontDraw::hook(true) == 0;
        foodHooksInstalled = TooltipPainter::hook(true) == 0 && foodHooksInstalled;
        if (!foodHooksInstalled) throw std::runtime_error("Could not install food tooltip hooks");
        hover::HoverTracker::getInstance().install();
        auto& bus = ll::event::EventBus::getInstance();
        renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
            unsigned const frame = uiFrame++;
            auto settings = Runtime::instance().preferences();
            auto* controller = event.screenView().mController.get();
            if (!controller || !controller->_isContainerScreen()) return;
            if (!settings.inspection.containerPreviews) { cache.clear(); return; }
            auto const* contents = cache.resolve(*controller);
            if (!contents) return;
            bool const shulker = contents->family == preview::ContainerPreview::Family::Shulker;
            auto const& preferences = settings.inspection;
            if (!(shulker ? preferences.shulkerPreviews : preferences.bundlePreviews)) return;
            if (contents->filledSlotCount() == 0 && contents->skippedSlotCount == 0
                && !(shulker ? preferences.emptyShulkerPreviews : preferences.emptyBundlePreviews)) return;
            std::optional<render::Rect> hoverText;
            if (tooltipBox && tooltipBox->frame == frame) hoverText = tooltipBox->rect;
            renderer.render(event.screenView(), event.uiRenderContext(), *contents, hoverText);
        });
        exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { cache.clear(); });
        return true;
    } catch (std::exception const& error) {
        Runtime::instance().self().getLogger().error("Item inspection initialization failed: {}", error.what());
        stop();
        return false;
    }
}
void stop() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* listener : {&renderListener, &exitListener}) {
        if (*listener) { bus.removeListener(*listener); listener->reset(); }
    }
    cache.clear();
    hover::HoverTracker::getInstance().uninstall();
    if (foodHooksInstalled) {
        bool removed = TooltipFontDraw::unhook(true);
        removed = TooltipPainter::unhook(true) && removed;
        if (removed) foodHooksInstalled = false;
        else Runtime::instance().self().getLogger().error("Could not remove food tooltip hooks");
    }
    if (durabilityHookInstalled) {
        if (DurabilityHovertext::unhook(true)) durabilityHookInstalled = false;
        else Runtime::instance().self().getLogger().error("Could not remove durability tooltip hook");
    }
    if (tooltipHookInstalled) {
        if (ShulkerContentsText::unhook(true)) tooltipHookInstalled = false;
        else Runtime::instance().self().getLogger().error("Could not remove Shulker contents text hook");
    }
}
}
