#include "features/inspection/LockedTrades.h"
#include "app/Runtime.h"
#include "features/inspection/LockedTradeIndex.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/MeasureResult.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/UIMeasureStrategy.h"
#include "mc/client/gui/controls/UIPropertyBag.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/core/utility/NonOwnerPointer.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/json/Value.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/network/packet/UpdateTradePacket.h"
#include "mc/safety/RedactableString.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"
#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace lamium::inspection::lockedTrades {
namespace {
using CollectionBind = bool (ScreenController::*)(std::string const&, uint, int, std::string const&, uint,
    std::string const&, UIPropertyBag&);
bool enabled() { return Runtime::instance().snapshot()->inspection.lockedTrades; }

// Vanilla binds "#tier_visible" per entry of the "trade_tiers" collection:
// true up to the next locked level, false above it. The locked look comes
// from "#is_tier_unlocked" and "#trade_toggle_enabled", left as vanilla sets them.
LL_TYPE_INSTANCE_HOOK(TierVisibleHook, ll::memory::HookPriority::Normal, ScreenController,
    static_cast<CollectionBind>(&ScreenController::$bind), bool, std::string const& collection, uint collectionHash,
    int index, std::string const& name, uint nameHash, std::string const& override, UIPropertyBag& bag) {
    bool result = origin(collection, collectionHash, index, name, nameHash, override, bag);
    try {
        if (name == "#tier_visible" && collection == "trade_tiers" && enabled()) {
            auto& value = (*bag.mJsonValue)[override.empty() ? name : override];
            if (value.isBool()) value.value_.bool_ = true;
        }
    } catch (...) {}
    return result;
}

// The last trade offer's items, owned copies. A locked row's toggle does not
// pass the pointer to its items, so vanilla never asks for their hover text;
// Lamium draws it from these.
struct Recipe { std::array<std::optional<ItemStack>, 3> items; }; // buy A, buy B, sell
struct Offer {
    std::vector<int> tiers;
    std::vector<Recipe> recipes;
};
std::mutex offerMutex;
Offer offer;
std::optional<ItemStack> itemOf(CompoundTag const& recipe, std::string_view key) {
    auto const* tag = recipe.get(key);
    if (!tag || tag->getId() != Tag::Compound) return std::nullopt;
    auto stack = ItemStack::fromTag(static_cast<CompoundTag const&>(*tag));
    if (stack.isNull() || !stack.mItem) return std::nullopt;
    return stack;
}
Offer readOffer(CompoundTag const& data) {
    Offer result;
    auto const* list = data.get("Recipes");
    if (!list || list->getId() != Tag::List) return result;
    for (auto const& entry : static_cast<ListTag const&>(*list)) {
        if (!entry.get() || entry->getId() != Tag::Compound) continue;
        auto const& recipe = static_cast<CompoundTag const&>(*entry.get());
        auto const* tier = recipe.get("tier");
        result.tiers.push_back(tier && tier->getId() == Tag::Int ? static_cast<IntTag const*>(tier)->data : 0);
        result.recipes.push_back({{itemOf(recipe, "buyA"), itemOf(recipe, "buyB"), itemOf(recipe, "sell")}});
    }
    return result;
}
LL_TYPE_INSTANCE_HOOK(OfferHook, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void, NetworkIdentifier const& source, UpdateTradePacket const& packet) {
    try {
        auto read = readOffer(*static_cast<UpdateTradePacketPayload const&>(packet).mData);
        std::lock_guard lock{offerMutex};
        offer = std::move(read);
    } catch (...) {}
    origin(source, packet);
}

bool shown(UIControl const& control) { return (static_cast<int>(control.mVisible) & 1) != 0; }
UIControl const* shownChild(UIControl const& parent, std::string_view name) {
    for (auto const& child : *parent.mChildren)
        if (child && shown(*child) && *child->mName == name) return child.get();
    return nullptr;
}
bool contains(UIControl const& control, glm::vec2 point) {
    glm::vec2 at = *control.mCachedPosition, size = *control.mSize;
    return size.x > 0 && size.y > 0 && point.x >= at.x && point.x < at.x + size.x && point.y >= at.y
        && point.y < at.y + size.y;
}
// The item slot (0 buy A, 1 buy B, 2 sell) under the pointer inside a trade
// row; only the visible state of the toggle is laid out.
std::optional<int> slotUnder(UIControl const& control, glm::vec2 point, int depth = 0) {
    if (depth > 16 || control.mCachedPositionDirty) return std::nullopt;
    auto const& name = *control.mName;
    for (int slot = 0; slot < 3; ++slot)
        if (name == std::array{"trade_item_1", "trade_item_2", "sell_item"}[slot])
            return contains(control, point) ? std::optional{slot} : std::nullopt;
    for (auto const& child : *control.mChildren)
        if (child && shown(*child))
            if (auto slot = slotUnder(*child, point, depth + 1)) return slot;
    return std::nullopt;
}
bool tierLocked(UIControl const& tier) {
    auto const* holder = shownChild(tier, "tier_label_holder");
    return holder && shownChild(*holder, "tier_label_locked");
}
// The locked trade item under the pointer: tier, row within the tier, slot.
struct Hit { int tier, row, slot; };
std::optional<Hit> lockedItemUnder(UIControl const& list, glm::vec2 pointer) {
    if (!contains(list, pointer)) return std::nullopt;
    int tier = -1;
    for (auto const& panel : *list.mChildren) {
        if (!panel || *panel->mName != "tier_stack_panel") continue;
        ++tier;
        if (!shown(*panel) || !contains(*panel, pointer) || !tierLocked(*panel)) continue;
        auto const* rows = shownChild(*panel, "trade_toggle_stack_panel");
        if (!rows) return std::nullopt;
        int row = -1;
        for (auto const& holder : *rows->mChildren) {
            if (!holder) continue;
            ++row;
            if (!shown(*holder) || !contains(*holder, pointer)) continue;
            if (auto slot = slotUnder(*holder, pointer)) return Hit{tier, row, *slot};
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// Hover text in the look of Lamium's container previews.
void drawTip(MinecraftUIRenderContext& context, glm::vec2 pointer, glm::vec2 screen, std::string text) {
    auto& client = context.mClient;
    auto const& fontHandle = client.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default");
    Bedrock::NotNullNonOwnerPtr<FontHandle const> const fontRef{Bedrock::NonOwnerPointer<FontHandle const>{fontHandle}};
    TextMeasureData const textData{1.0f, 0.0f, true, false, false, ::ui::TextAlignment::Left}; // Color codes apply, not shown.
    CaretMeasureData const caret{-1, false};
    glm::vec2 size = context.getMeasureStrategy().measureText(fontRef, text, 1000, 1000, textData, caret).mSize;
    constexpr float pad = 4;
    float w = size.x + 2 * pad, h = size.y + 2 * pad;
    auto at = tipBox(pointer.x, pointer.y, w, h, screen.x, screen.y);
    RectangleArea frame{at.x, at.x + w, at.y, at.y + h};
    context.fillRectangle(frame, mce::Color{0.10f, 0.10f, 0.10f, 1.0f}, 0.92f);
    context.drawRectangle(frame, mce::Color{0.55f, 0.35f, 0.70f, 1.0f}, 1.0f, 1);
    context.flushImages(mce::Color{1.f, 1.f, 1.f, 1.f}, 1.0f, HashedString{"ui_fillColor"});
    context.drawText(fontHandle.getFont(), RectangleArea{at.x + pad, at.x + pad + size.x + 1, at.y + pad, at.y + pad + size.y},
        std::move(text), mce::Color{1.f, 1.f, 1.f, 1.f}, 1.0f, ::ui::TextAlignment::Left, textData, caret);
    context.flushText(0.0f, std::nullopt);
}
ll::event::ListenerPtr renderListener, exitListener;
void onRender(ll::event::AfterUIRenderEvent& event) {
    if (!enabled()) return;
    auto& view = event.screenView();
    auto* tree = view.mVisualTree.get();
    if (!tree) return;
    auto list = tree->getControlByName("trade_selector_stack_panel", true);
    if (!list || !shown(*list)) return;
    glm::vec2 pointer = view.mPointerLocationPrevious;
    auto hit = lockedItemUnder(*list, pointer);
    if (!hit) return;
    std::optional<ItemStack> stack;
    {
        std::lock_guard lock{offerMutex};
        if (auto index = recipeAt(offer.tiers, hit->tier, hit->row))
            stack = offer.recipes[*index].items[static_cast<size_t>(hit->slot)];
    }
    auto* player = event.uiRenderContext().mClient.getLocalPlayer();
    if (!stack || !player) return;
    auto text = stack->getFormattedHovertext(player->getLevel(), false).mUnredactedString;
    if (text.empty()) return;
    drawTip(event.uiRenderContext(), pointer, *view.mSize, std::move(text));
}
bool hooked = false;
}
bool start() {
    if (!hooked) {
        if (TierVisibleHook::hook(true) != 0) return false;
        if (OfferHook::hook(true) != 0) { TierVisibleHook::unhook(true); return false; }
        hooked = true;
    }
    auto& bus = ll::event::EventBus::getInstance();
    if (!renderListener)
        renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
            try { onRender(event); } catch (...) {}
        });
    if (!exitListener)
        exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
            std::lock_guard lock{offerMutex};
            offer = {};
        });
    return true;
}
void stop() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { bus.removeListener(*listener); listener->reset(); }
    if (hooked) {
        bool removed = TierVisibleHook::unhook(true);
        removed = OfferHook::unhook(true) && removed;
        if (removed) hooked = false;
        else Runtime::instance().self().getLogger().error("Could not remove the locked trade hooks");
    }
    std::lock_guard lock{offerMutex};
    offer = {};
}
}
