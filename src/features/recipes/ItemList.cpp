#include "features/recipes/ItemList.h"
#ifdef LAMIUM_ITEM_LIST
#include "app/Runtime.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/recipes/ItemListLayout.h"
#include "features/schematic/SchematicItems.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"
#include "ui/ListViewWidgets.h"
#include "ui/Widgets.h"
#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace lamium::recipes::itemList {
namespace {
ll::event::ListenerPtr renderListener, exitListener;
// The item names in the creative inventory's order, each once (items go by name, L-359). Read again after a world exit.
std::vector<std::string> names;
std::atomic<bool> forget{false};
int page = 0;
std::atomic<int> turned{0}; // wheel turns waiting for the next frame
// What the last frame showed, for the mouse listener: the band and the pointer.
std::mutex shownLock;
struct Shown {
    std::chrono::steady_clock::time_point at{};
    ItemListLayout layout;
    float pointerX = -1, pointerY = -1;
} shown;
void readNames(LocalPlayer& player) {
    names.clear();
    auto const* registry = player.getLevel().getItemRegistry().getCreativeItemRegistry().get();
    if (!registry) return;
    auto const& listed = registry->mCreativeItems.get();
    if (listed.size() > 20000) return; // not a list of items
    std::set<std::string> seen;
    for (auto const& entry : listed) {
        auto const& item = entry.mItemInstance.get();
        if (item.isNull()) continue;
        std::string name = item.getTypeName();
        if (name.size() <= 200 && seen.insert(name).second) names.push_back(std::move(name));
    }
}
// The game's window on this screen: the chest's and anvil's panel, the inventory's and crafting table's
// (recipe book and all), the furnace's. Seen in game on 1.26.60 (VALIDATION-LOG 2026-11-18).
bool windowOf(ScreenView& view, glm::vec2& at, glm::vec2& size) {
    auto* tree = view.mVisualTree.get();
    if (!tree) return false;
    static std::string const windows[]{"root_panel", "content_stack_panel", "screen_stack_panel"};
    for (auto const& name : windows) {
        auto window = tree->getControlByName(name, true);
        if (!window || window->mCachedPositionDirty) continue;
        at = *window->mCachedPosition;
        size = *window->mSize;
        glm::vec2 const screen = *view.mSize;
        if (size.x > 0 && size.y > 0 && size.x < screen.x - 1) return true; // else not laid out yet
    }
    return false;
}
void draw(ll::event::UIRenderEvent& event) {
    using namespace lamium::ui;
    auto& view = event.screenView();
    if (!view.mHasFocus || !view.mController || !view.mController->_isContainerScreen()) return;
    // The anvil's sides belong to the enchantment order's panels.
    if (view.getScreenName() == "anvil_screen") return;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    if (forget.exchange(false)) names.clear();
    if (names.empty()) readNames(*player);
    glm::vec2 at{}, size{};
    if (names.empty() || !windowOf(view, at, size)) return;
    glm::vec2 const screen = *view.mSize;
    auto const layout = itemListRight(screen.x, screen.y, at.x, size.x);
    if (!layout.usable()) return;
    int const count = static_cast<int>(names.size()), pages = layout.pages(count);
    page = std::clamp(page + turned.exchange(0), 0, pages - 1);
    float const px = view.mPointerLocationPrevious->x, py = view.mPointerLocationPrevious->y;
    {
        std::scoped_lock lock(shownLock);
        shown = {std::chrono::steady_clock::now(), layout, px, py};
    }
    auto& context = event.uiRenderContext();
    // A dimmed band to the screen's edge: the game dims the world too little for bare icons.
    fill(context, layout.left, layout.top, layout.width, layout.height, palette::panel, .5f);
    label(context, layout.left, layout.top + 3, layout.width - 4, std::format("{} / {}", page + 1, pages), palette::dim, Align::Right);
    std::vector<inspection::render::IconAt> icons;
    int const first = page * layout.perPage(), hovered = layout.hit(px, py);
    std::string tip;
    for (int i = 0; i < layout.perPage() && first + i < count; ++i) {
        auto const& item = schematic::items::shown(names[static_cast<size_t>(first + i)]);
        float const x = layout.cellX(i), y = layout.cellY(i);
        if (i == hovered) {
            fill(context, x, y, ItemListLayout::cell, ItemListLayout::cell, palette::white, .14f);
            tip = item.name;
        }
        if (auto const* stack = schematic::items::iconStack(item.icon)) icons.push_back({stack, x + 1, y + 1});
    }
    context.flushText(0, std::nullopt);
    if (!icons.empty()) inspection::render::drawItemIcons(context, icons, 17);
    if (!tip.empty()) {
        float const w = textWidth(context, tip) + 8, x = std::min(px + 8, screen.x - w - 2), y = std::min(py + 8, screen.y - 14);
        drawTipBox(context, x, y, w, 12);
        label(context, x + 4, y + 2, w - 8, tip);
        context.flushText(0, std::nullopt);
    }
}
}
bool wheel(int pages) {
    std::scoped_lock lock(shownLock);
    if (std::chrono::steady_clock::now() - shown.at > std::chrono::milliseconds(300) || !shown.layout.has(shown.pointerX, shown.pointerY)) return false;
    turned += pages;
    return true;
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) { try { draw(event); } catch (...) {} });
    // The names belong to the world's items: read them again in the next world.
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { forget = true; });
    Runtime::instance().self().getLogger().warn("L-359 item list enabled (build option)");
}
void stop() {
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    forget = true;
}
}
#else
namespace lamium::recipes::itemList { void start() {} void stop() {} bool wheel(int) { return false; } }
#endif
