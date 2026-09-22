#include "features/inspection/Inspection.h"
#include "features/inspection/hover/HoverTracker.h"
#include "features/inspection/preview/HoveredPreviewCache.h"
#include "features/inspection/render/PreviewRenderer.h"
#include "features/inspection/render/DurabilityTooltip.h"
#include "app/Runtime.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/safety/RedactableString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ShulkerBoxBlockItem.h"

namespace lamium::inspection {
namespace {
preview::HoveredPreviewCache cache;
render::PreviewRenderer renderer;
ll::event::ListenerPtr renderListener, exitListener;
bool tooltipHookInstalled = false;
LL_TYPE_INSTANCE_HOOK(ShulkerContentsText, ll::memory::HookPriority::Normal, ShulkerBoxBlockItem,
    &ShulkerBoxBlockItem::$appendFormattedHovertext, void, ItemStackBase const& stack,
    Level& level, Bedrock::Safety::RedactableString& hovertext, bool const showCategory) {
    auto& runtime = Runtime::instance();
    auto const preferences = runtime.preferences().inspection;
    if (runtime.enabled() && preferences.containerPreviews && preferences.shulkerPreviews
        && preferences.hideShulkerContents) {
        // Keep the generic item text (name/lore/etc.); only skip the Shulker
        // specialization which appends the contained-item list.
        Item::$appendFormattedHovertext(stack, level, hovertext, showCategory);
    } else origin(stack, level, hovertext, showCategory);
}
}
bool start() {
    try {
        tooltipHookInstalled = ShulkerContentsText::hook(true) == 0;
        if (!tooltipHookInstalled) throw std::runtime_error("Could not install Shulker contents text hook");
        hover::HoverTracker::getInstance().install();
        auto& bus = ll::event::EventBus::getInstance();
        renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
            auto settings = Runtime::instance().preferences();
            auto* controller = event.screenView().mController.get();
            if (!controller || !controller->_isContainerScreen()) return;
            if (settings.inspection.durability) {
                if (auto* item = hover::HoverTracker::getInstance().resolveItem(*controller))
                    render::renderDurability(event.screenView(), event.uiRenderContext(), *item);
            }
            if (!settings.inspection.containerPreviews) { cache.clear(); return; }
            auto const* contents = cache.resolve(*controller);
            if (!contents) return;
            bool const shulker = contents->family == preview::ContainerPreview::Family::Shulker;
            auto const& preferences = settings.inspection;
            if (!(shulker ? preferences.shulkerPreviews : preferences.bundlePreviews)) return;
            if (contents->filledSlotCount() == 0 && contents->skippedSlotCount == 0
                && !(shulker ? preferences.emptyShulkerPreviews : preferences.emptyBundlePreviews)) return;
            renderer.render(event.screenView(), event.uiRenderContext(), *contents);
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
    if (tooltipHookInstalled) {
        if (ShulkerContentsText::unhook(true)) tooltipHookInstalled = false;
        else Runtime::instance().self().getLogger().error("Could not remove Shulker contents text hook");
    }
}
}
