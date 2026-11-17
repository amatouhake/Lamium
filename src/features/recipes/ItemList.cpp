#include "features/recipes/ItemList.h"
#ifdef LAMIUM_ITEM_LIST
#include "app/Runtime.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/RecipeService.h"
#include "features/recipes/RecipeView.h"
#include "features/recipes/RecipeViewDraw.h"
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
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include <algorithm>
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
std::string viewScreen; // the screen the recipe view was opened on; it closes with it
// What the last frame showed, for the input listeners, and what they kept from the game for the next frame.
// The listeners run off the client thread: they only decide whether the game gets the event, and queue it.
std::mutex shownLock;
struct Shown {
    std::chrono::steady_clock::time_point at{};
    ItemListLayout list;
    RecipeViewLayout view;
    bool viewOpen = false;
    float pointerX = -1, pointerY = -1;
} shown;
struct Queued {
    std::vector<bool> clicks; // true: the left button
    int listTurns = 0, viewTurns = 0;
    bool back = false, close = false;
} queued;
// The buttons and keys whose press was kept: only their release is kept too. One the game saw go down
// must see it come up, or it stays held there.
std::vector<int> keptButtons, keptKeys;
bool keep(std::vector<int>& kept, int code, bool down, bool wanted) {
    auto const had = std::find(kept.begin(), kept.end(), code);
    if (down) {
        if (wanted && had == kept.end()) kept.push_back(code);
        if (!wanted && had != kept.end()) kept.erase(had);
        return wanted;
    }
    if (had == kept.end()) return false;
    kept.erase(had);
    return true;
}
// Nothing is kept from the game once the list has not been drawn for a moment: a stale "open" must not swallow input.
bool fresh() { return std::chrono::steady_clock::now() - shown.at <= std::chrono::milliseconds(300); }
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
    // `content_stack_panel` keeps the recipe book's width while the book is closed (seen in game), so
    // its right part, which is what is drawn then, is asked first: only the right side matters here.
    static std::string const windows[]{"root_panel", "player_inventory", "content_stack_panel", "screen_stack_panel"};
    bool const stacked = static_cast<bool>(tree->getControlByName(windows[2], true));
    for (auto const& name : windows) {
        if (&name == &windows[1] && !stacked) continue;
        auto window = tree->getControlByName(name, true);
        if (!window || window->mCachedPositionDirty) continue;
        at = *window->mCachedPosition;
        size = *window->mSize;
        glm::vec2 const screen = *view.mSize;
        if (size.x > 0 && size.y > 0 && size.x < screen.x - 1) return true; // else not laid out yet
    }
    return false;
}
// False when nothing was drawn.
bool drawList(ll::event::UIRenderEvent& event) {
    using namespace lamium::ui;
    auto& screenView = event.screenView();
    std::string const screenName = screenView.mHasFocus && screenView.mController && screenView.mController->_isContainerScreen()
        ? screenView.getScreenName() : std::string();
    if (view::isOpen() && screenName != viewScreen) view::close();
    // The anvil's sides belong to the enchantment order's panels.
    if (screenName.empty() || screenName == "anvil_screen") return false;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return false;
    if (forget.exchange(false)) names.clear(), view::close();
    if (names.empty()) readNames(*player);
    glm::vec2 at{}, size{};
    if (names.empty() || !windowOf(screenView, at, size)) return false;
    glm::vec2 const screen = *screenView.mSize;
    // The centre keeps the recipe view's two columns free, so the list never moves when it opens.
    auto const viewLayout = recipeViewAt(screen.x, screen.y);
    float const right = std::max(at.x + size.x, viewLayout.left + RecipeViewLayout::width);
    auto const layout = itemListRight(screen.x, screen.y, 0, right);
    if (!layout.usable()) { view::close(); return false; }
    float const px = screenView.mPointerLocationPrevious->x, py = screenView.mPointerLocationPrevious->y;
    Queued input;
    {
        std::scoped_lock lock(shownLock);
        // What was queued before a pause belongs to a screen that is gone.
        if (fresh()) input = std::move(queued);
        queued = {};
    }
    // The table is read while the list shows, a part a frame, so the first recipe opens at once.
    service::want();
    int const count = static_cast<int>(names.size()), pages = layout.pages(count);
    page = std::clamp(page + input.listTurns, 0, pages - 1);
    int const first = page * layout.perPage(), hovered = layout.hit(px, py);
    bool const wasOpen = view::isOpen();
    if (!wasOpen && !input.clicks.empty() && hovered >= 0 && first + hovered < count) {
        view::open(names[static_cast<size_t>(first + hovered)], !input.clicks.front());
        viewScreen = screenName;
        input.clicks.clear();
    }
    auto& context = event.uiRenderContext();
    // A dimmed band to the screen's edge: the game dims the world too little for bare icons.
    fill(context, layout.left, layout.top, layout.width, layout.height, palette::panel, .5f);
    label(context, layout.left, layout.top + 3, layout.width - 4, std::format("{} / {}", page + 1, pages), palette::dim, Align::Right);
    view::Late late;
    for (int i = 0; i < layout.perPage() && first + i < count; ++i) {
        auto const& item = schematic::items::shown(names[static_cast<size_t>(first + i)]);
        float const x = layout.cellX(i), y = layout.cellY(i);
        if (i == hovered) {
            fill(context, x, y, ItemListLayout::cell, ItemListLayout::cell, palette::white, .14f);
            late.tip = item.name;
        }
        if (auto const* stack = schematic::items::iconStack(item.icon)) late.icons.push_back({stack, x + 1, y + 1});
    }
    if (wasOpen && hovered >= 0 && first + hovered < count && !input.clicks.empty()) {
        // The list stays usable beside an open view: a press there shows that item.
        view::open(names[static_cast<size_t>(first + hovered)], !input.clicks.front());
        input.clicks.clear();
    }
    view::draw(context,
               {screen.x, screen.y, px, py, std::move(input.clicks), input.viewTurns, input.back, input.close}, late);
    // The key hints keep one place, the view's last line; closed, only where the game's window leaves it free.
    bool const open = view::isOpen();
    if (open || viewLayout.hintY() >= at.y + size.y + 2)
        label(context, viewLayout.left, viewLayout.hintY(), RecipeViewLayout::width, translated(open ? "recipes.hint.open" : "recipes.hint"),
              palette::dim, Align::Center);
    {
        std::scoped_lock lock(shownLock);
        shown = {std::chrono::steady_clock::now(), layout, viewLayout, open, px, py};
    }
    context.flushText(0, std::nullopt);
    if (!late.icons.empty()) inspection::render::drawItemIcons(context, late.icons, 17);
    for (auto& text : late.counts) slotCount(context, text.x, text.y, std::move(text.text));
    if (!late.counts.empty()) context.flushText(0, std::nullopt);
    if (!late.tip.empty()) {
        float const w = textWidth(context, late.tip) + 8, x = std::min(px + 8, screen.x - w - 2), y = std::min(py + 8, screen.y - 14);
        drawTipBox(context, x, y, w, 12);
        label(context, x + 4, y + 2, w - 8, late.tip);
        context.flushText(0, std::nullopt);
    }
    return true;
}
void draw(ll::event::UIRenderEvent& event) {
    if (drawList(event)) return;
    // Gone: the listeners keep nothing more, at once and not after the moment `fresh` allows.
    std::scoped_lock lock(shownLock);
    shown.at = {};
    shown.viewOpen = false;
}
}
bool wheel(int pages) {
    std::scoped_lock lock(shownLock);
    if (!fresh()) return false;
    if (shown.viewOpen && shown.view.has(shown.pointerX, shown.pointerY)) queued.viewTurns += pages;
    else if (shown.list.has(shown.pointerX, shown.pointerY)) queued.listTurns += pages;
    else return shown.viewOpen; // an open view lies over the game's window: its slots must not scroll
    return true;
}
bool press(int button, bool left, bool right, bool down) {
    std::scoped_lock lock(shownLock);
    bool const ours = fresh() && (shown.viewOpen || shown.list.has(shown.pointerX, shown.pointerY));
    if (!keep(keptButtons, button, down, ours)) return false;
    if (down && (left || right) && queued.clicks.size() < 8) queued.clicks.push_back(left);
    return true;
}
bool key(int code, bool down) {
    // The modifier keys stay the game's: one held while the view opens must still be released.
    if (code == 0x10 || code == 0x11 || code == 0x12 || (code >= 0xA0 && code <= 0xA5)) return false;
    std::scoped_lock lock(shownLock);
    if (!keep(keptKeys, code, down, fresh() && shown.viewOpen)) return false;
    if (down && code == 0x1B) queued.close = true;
    if (down && code == 0x08) queued.back = true;
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
namespace lamium::recipes::itemList {
void start() {}
void stop() {}
bool wheel(int) { return false; }
bool press(int, bool, bool, bool) { return false; }
bool key(int, bool) { return false; }
}
#endif
