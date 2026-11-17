#include "features/recipes/ItemList.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inspection/hover/HoverTracker.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/recipes/ItemKey.h"
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/RecipeService.h"
#include "features/recipes/RecipeView.h"
#include "features/recipes/RecipeViewDraw.h"
#include "features/schematic/SchematicItems.h"
#include "input/Actions.h"
#include "input/Binding.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/options/IOptionRegistry.h"
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
// The items in the creative inventory's order, each once, by their keys (ItemKey.h: the name, with the aux
// where it is not 0, so every bed color and potion is listed). Read again after a world exit.
std::vector<std::string> names;
std::set<std::string, std::less<>> listed; // the same keys, to look one up
std::atomic<bool> forget{false};
int page = 0;
std::string viewScreen; // the screen the recipe view was opened on; it closes with it
std::string lastScreen; // the item screen drawn last
// What the last frame showed, for the input listeners, and what they kept from the game for the next frame.
// The listeners run off the client thread: they only decide whether the game gets the event, and queue it.
std::mutex shownLock;
struct Shown {
    std::chrono::steady_clock::time_point at{};
    ItemListLayout list;
    RecipeViewLayout view;
    bool viewOpen = false;
    float pointerX = -1, pointerY = -1;
    std::string hoverItem;          // the item under the pointer: in the list, the view or a slot of the game's
    std::vector<int> inventoryKeys; // the game's own key for the inventory, which stays the game's
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
    listed.clear();
    auto const* registry = player.getLevel().getItemRegistry().getCreativeItemRegistry().get();
    if (!registry) return;
    auto const& entries = registry->mCreativeItems.get();
    if (entries.size() > 20000) return; // not a list of items
    for (auto const& entry : entries) {
        auto const& item = entry.mItemInstance.get();
        if (item.isNull()) continue;
        int const aux = item.getAuxValue();
        std::string name = itemKey(item.getTypeName(), aux == anyAux ? 0 : aux);
        if (name.size() <= 200 && listed.insert(name).second) names.push_back(std::move(name));
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
// False when nothing was drawn. The listeners then keep nothing once `fresh` runs out.
bool drawList(ll::event::UIRenderEvent& event) {
    using namespace lamium::ui;
    auto& screenView = event.screenView();
    // Several screens are drawn a frame (the HUD under a container); any but a focused item screen is not
    // this list's business and changes nothing here.
    std::string const screenName = screenView.mHasFocus && screenView.mController && screenView.mController->_isContainerScreen()
        ? screenView.getScreenName() : std::string();
    if (screenName.empty()) return false;
    // Off, or reading the game's item and recipe tables not confirmed for this version: nothing of it shows.
    if (!Runtime::instance().enabled() || !Runtime::instance().snapshot()->inventory.recipes
        || !versionSensitiveAllowed(capabilities::Id::RecipeTable)) {
        view::close();
        return false;
    }
    lastScreen = screenName;
    {
        // Another item screen, or the same one after a pause: the view belonged to the one that is gone.
        std::scoped_lock lock(shownLock);
        if (view::isOpen() && (screenName != viewScreen || !fresh())) view::close();
    }
    // The anvil's sides belong to the enchantment order's panels.
    if (screenName == "anvil_screen") return false;
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
    std::string hoverItem;
    if (hovered >= 0 && first + hovered < count) hoverItem = names[static_cast<size_t>(first + hovered)];
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
    static std::vector<int> inventoryKeys;
    static std::string backKey; // the back action's key as the keymap names it; empty when it has none
    static std::chrono::steady_clock::time_point keysRead;
    if (auto const now = std::chrono::steady_clock::now(); now - keysRead > std::chrono::seconds(1)) {
        keysRead = now;
        if (auto keyboard = client->getOptions().getCurrentKeyboardRemapping()) {
            auto const& mapped = keyboard->getKeymappingByAction("key.inventory").mKeys.get();
            if (mapped.size() <= 4) inventoryKeys = mapped;
        }
        backKey = input::effectiveChord(Runtime::instance().preferences().bindings, input::Action::RecipeBack).empty()
            ? std::string() : actionBindingName(*client, input::Action::RecipeBack);
    }
    if (open || viewLayout.hintY() >= at.y + size.y + 2) {
        // One line whatever the language: shrunk to the window's width where it is longer.
        std::string hint = open && !backKey.empty() ? translated("recipes.hint.open", backKey) : translated("recipes.hint");
        float const room = RecipeViewLayout::width - 8, wide = textWidth(context, hint), scale = wide > room ? room / wide : 1.f;
        labelScaled(context, viewLayout.left + 4, viewLayout.hintY() + (1 - scale) * 4, room / scale, std::move(hint), scale, palette::dim, Align::Center);
    }
    if (hoverItem.empty()) hoverItem = late.hovered;
    if (hoverItem.empty() && !(open && viewLayout.has(px, py))) {
        // A slot of the game's own window, as its hover callbacks last said.
        if (auto const* stack = inspection::hover::HoverTracker::getInstance().resolveItem(*screenView.mController); stack && !stack->isNull()) {
            // Its aux counts where it tells items apart (a red bed, a potion), not where it is wear.
            std::string const name = stack->getTypeName();
            int const aux = stack->getAuxValue();
            auto const table = service::table();
            if (name.size() > 200) {}
            else if (std::string key = itemKey(name, aux == anyAux ? 0 : aux); listed.contains(key)) hoverItem = std::move(key);
            else hoverItem = table ? table->index.keyOf(name, aux) : name;
        }
    }
    {
        std::scoped_lock lock(shownLock);
        shown = {std::chrono::steady_clock::now(), layout, viewLayout, open, px, py, std::move(hoverItem), inventoryKeys};
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
    // Esc and the game's inventory key close the view and leave the screen under it open: the key that
    // closes the inventory closes what is on top first.
    bool const games = std::find(shown.inventoryKeys.begin(), shown.inventoryKeys.end(), code) != shown.inventoryKeys.end();
    if (down && (code == 0x1B || games)) queued.close = true;
    return true;
}
bool viewOpen() {
    std::scoped_lock lock(shownLock);
    return fresh() && shown.viewOpen;
}
void back() {
    std::scoped_lock lock(shownLock);
    if (fresh() && shown.viewOpen) queued.back = true;
}
void show(bool uses) {
    std::string item;
    {
        std::scoped_lock lock(shownLock);
        if (!fresh() || shown.hoverItem.empty()) return;
        item = shown.hoverItem;
    }
    view::open(std::move(item), uses);
    viewScreen = lastScreen;
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) { try { drawList(event); } catch (...) {} });
    // The names belong to the world's items: read them again in the next world.
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { forget = true; });
}
void stop() {
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    forget = true;
}
}
