#include "features/recipes/ItemList.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inspection/hover/HoverTracker.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/recipes/ItemKey.h"
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/ListFilter.h"
#include "features/recipes/PlanViewDraw.h"
#include "features/recipes/RecipeState.h"
#include "features/recipes/RecipeText.h"
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
#include "mc/client/input/KeyboardManager.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"
#include "ui/ListViewWidgets.h"
#include "ui/Localization.h"
#include "ui/TextEdit.h"
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
// The list narrowed: by the search field's text and to what can be made from what is carried.
enum class Filter { All, CanMake };
Filter filter = Filter::All;
ui::TextEdit search{64};
bool searchFocus = false, keyboardOwned = false;
// The items' names as the game shows them, lowered, for searching: read a part a frame (each asks the game).
std::vector<std::string> lowNames;
std::vector<int> visible; // positions in `names`
struct VisibleFor {
    bool valid = false;
    std::string query;
    Filter filter = Filter::All;
    size_t named = 0;
    std::chrono::steady_clock::time_point at{};
} visibleFor;
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
    ListBox field;                  // the search field
    SideLayout side;                // the band left of the game's window
    bool searchFocus = false;
} shown;
struct Queued {
    std::vector<bool> clicks; // true: the left button
    int listTurns = 0, viewTurns = 0, sideTurns = 0;
    bool back = false, close = false;
    std::vector<int> fieldKeys; // keys pressed while the search field has the focus; 0x10000 set with Ctrl
    std::string text;           // text the game handed the focused field
    bool outside = false;       // a press the game kept, which takes the focus from the field
} queued;
bool ctrlHeld = false; // the listeners' own
std::atomic<bool> shiftHeld{false};
bool sideGather = true; // the left band's short list: what is left to gather, or the wanted items
// The recipe view lies over the window of what is wanted; both go with the screen they were opened on.
void closeViews() {
    view::close();
    planView::close();
}
bool anyView() { return view::isOpen() || planView::isOpen(); }
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
void releaseKeyboard() {
    if (!keyboardOwned) return;
    keyboardOwned = false;
    if (auto client = ll::service::getClientInstance()) {
        auto& keyboard = client->getKeyboardManager();
        keyboard.disableKeyboard();
        keyboard.releaseKeyboardOwnership();
    }
}
// The list as narrowed now. What can be made changes with the inventory, so it is asked again twice a second.
void narrow(LocalPlayer& player) {
    for (int budget = 150; budget > 0 && lowNames.size() < names.size(); --budget)
        lowNames.push_back(lowered(schematic::items::shown(names[lowNames.size()]).name));
    std::string query = lowered(search.value());
    auto const now = std::chrono::steady_clock::now();
    if (visibleFor.valid && query == visibleFor.query && filter == visibleFor.filter && lowNames.size() == visibleFor.named
        && (filter != Filter::CanMake || now - visibleFor.at < std::chrono::milliseconds(500)))
        return;
    std::shared_ptr<service::Table const> table;
    std::vector<bool> can;
    if (filter == Filter::CanMake && (table = service::table())) {
        std::map<Item, Count> held;
        for (auto const& stack : schematic::items::carriedStacks(player))
            if (Item const id = table->index.find(stack.name, stack.aux); id >= 0) held[id] += stack.count;
        can = canMakeNow(table->index, held);
    }
    visible.clear();
    for (size_t i = 0; i < names.size(); ++i) {
        if (!query.empty() && !searchMatches(query, i < lowNames.size() ? std::string_view(lowNames[i]) : std::string_view(), names[i])) continue;
        if (filter == Filter::CanMake) {
            if (!table) continue; // not read yet
            auto const key = splitItemKey(names[i]);
            Item const id = table->index.find(key.name, key.aux);
            if (id < 0 || !can[static_cast<size_t>(id)]) continue;
        }
        visible.push_back(static_cast<int>(i));
    }
    if (query != visibleFor.query || filter != visibleFor.filter) page = 0;
    visibleFor = {true, std::move(query), filter, lowNames.size(), now};
}
void readNames(LocalPlayer& player) {
    names.clear();
    listed.clear();
    lowNames.clear();
    visibleFor = {};
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
// `left`: where the window begins, the game's recipe book included while it shows.
bool windowOf(ScreenView& view, glm::vec2& at, glm::vec2& size, float& left) {
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
        if (!(size.x > 0 && size.y > 0 && size.x < screen.x - 1)) continue; // not laid out yet
        left = at.x;
        if (stacked) {
            // Left of the inventory's panel: the recipe book, unless it is switched off. In doubt it counts,
            // so the left band never lies over it.
            static std::string const book = "recipe_book";
            auto shown = tree->getControlByName(book, true);
            if (shown && (static_cast<int>(shown->mVisible) & 1) && !shown->mCachedPositionDirty && shown->mSize->x > 0)
                left = std::min(left, shown->mCachedPosition->x);
        }
        return true;
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
        closeViews();
        return false;
    }
    lastScreen = screenName;
    {
        // Another item screen, or the same one after a pause: the view belonged to the one that is gone.
        std::scoped_lock lock(shownLock);
        if (anyView() && (screenName != viewScreen || !fresh())) closeViews();
    }
    // The anvil's sides belong to the enchantment order's panels.
    if (screenName == "anvil_screen") return false;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return false;
    if (forget.exchange(false)) names.clear(), closeViews(), search.clear(), searchFocus = false;
    if (names.empty()) readNames(*player);
    glm::vec2 at{}, size{};
    float windowLeft = 0;
    if (names.empty() || !windowOf(screenView, at, size, windowLeft)) return false;
    glm::vec2 const screen = *screenView.mSize;
    // The centre keeps the recipe view's two columns free, so the list never moves when it opens.
    auto const viewLayout = recipeViewAt(screen.x, screen.y);
    float const right = std::max(at.x + size.x, viewLayout.left + RecipeViewLayout::width);
    auto const layout = itemListRight(screen.x, screen.y, 0, right);
    if (!layout.usable()) { closeViews(); return false; }
    auto const side = itemListLeft(screen.y, std::min(windowLeft, viewLayout.left));
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
    // The search field: what was typed since the last frame, then where this frame's press went.
    auto const field = searchFieldAt(screen.x, screen.y);
    if (searchFocus) {
        for (int pressed : input.fieldKeys) {
            int const code = pressed & 0xFFFF;
            bool const ctrl = (pressed & 0x10000) != 0;
            if (code == 0x1B || code == 0x0D) searchFocus = false;
            else if (code == 0x08) search.backspace();
            else if (code == 0x2E) search.erase();
            else if (code == 0x25) search.left(ctrl);
            else if (code == 0x27) search.right(ctrl);
            else if (code == 0x24) search.home();
            else if (code == 0x23) search.end();
            else if (ctrl && code == 0x41) search.selectAll();
        }
        if (!input.text.empty()) search.type(input.text);
    }
    // One press a frame. Each control asks whether it was on it; what none takes goes to the recipe view.
    bool pressed = !input.clicks.empty();
    bool const leftPress = pressed && input.clicks.front();
    auto took = [&](ListBox const& box) {
        if (!pressed || !box.has(px, py)) return false;
        pressed = false;
        return true;
    };
    bool caretPress = false;
    if (pressed || input.outside) searchFocus = false;
    if (took(field)) searchFocus = caretPress = true;
    narrow(*player);
    int const count = static_cast<int>(visible.size()), pages = layout.pages(count);
    page = std::clamp(page + input.listTurns, 0, pages - 1);
    int const first = page * layout.perPage(), hovered = layout.hit(px, py);
    auto nameAt = [&](int cell) -> std::string const& { return names[static_cast<size_t>(visible[static_cast<size_t>(first + cell)])]; };
    std::string hoverItem;
    if (hovered >= 0 && first + hovered < count) hoverItem = nameAt(hovered);
    auto& context = event.uiRenderContext();
    // A dimmed band to the screen's edge: the game dims the world too little for bare icons.
    fill(context, layout.left, layout.top, layout.width, layout.height, palette::panel, .5f);
    label(context, layout.left, layout.top + 3, layout.width - 4, std::format("{} / {}", page + 1, pages), palette::dim, Align::Right);
    // The filters keep the bar's left end: both where the band is wide, one switch where it is not.
    float barX = layout.left + 2;
    auto barButton = [&](std::string text, bool on) {
        ListBox const box{barX, layout.top + 1, textWidth(context, text) + 10, 11};
        drawButton(context, box.x, box.y, box.w, box.h, std::move(text), box.has(px, py), on ? ButtonKind::Primary : ButtonKind::Plain);
        barX += box.w + 2;
        return leftPress && took(box);
    };
    if (layout.width >= 130) {
        if (barButton(translated("recipes.filter.all"), filter == Filter::All)) filter = Filter::All;
        if (barButton(translated("recipes.filter.canMake"), filter == Filter::CanMake)) filter = Filter::CanMake;
    } else if (barButton(translated("recipes.filter.canMakeShort"), filter == Filter::CanMake))
        filter = filter == Filter::CanMake ? Filter::All : Filter::CanMake;
    if (!count) label(context, layout.left + 4, layout.top + ItemListLayout::bar + 4, layout.width - 8,
                      translated(filter == Filter::CanMake && !service::table() ? "schematic.raw.reading" : "recipes.notFound"), palette::dim);
    view::Late late;
    std::vector<ImageRect> dots; // a favourite's mark, drawn over its icon
    for (int i = 0; i < layout.perPage() && first + i < count; ++i) {
        auto const& item = schematic::items::shown(nameAt(i));
        float const x = layout.cellX(i), y = layout.cellY(i);
        if (i == hovered) {
            fill(context, x, y, ItemListLayout::cell, ItemListLayout::cell, palette::white, .14f);
            late.tip = item.name;
        }
        if (auto const* stack = schematic::items::iconStack(item.icon)) late.icons.push_back({stack, x + 1, y + 1});
        if (state::notes().favourite(nameAt(i))) dots.push_back({x + 13, y + 1, 4, 4});
    }
    // A press on an item shows its recipes, a right press what it is used in, also beside an open view.
    auto show = [&](std::string const& item) {
        view::open(item, !leftPress);
        viewScreen = screenName;
        pressed = false;
    };
    if (pressed && hovered >= 0 && first + hovered < count) show(nameAt(hovered));
    // The left band: favourites above, what is wanted below.
    constexpr float smallIcon = .625f;
    if (side.usable()) {
        fill(context, side.left, side.top, side.width, side.height, palette::panel, .5f);
        label(context, side.left + 4, side.top + 3, side.width - 8, translated("recipes.favourites"), palette::dim);
        auto const favourites = state::notes().favourites;
        int const favouriteHover = side.hitFavourite(px, py), favouriteCount = std::min(static_cast<int>(favourites.size()), side.favourites());
        for (int i = 0; i < favouriteCount; ++i) {
            auto const& item = schematic::items::shown(favourites[static_cast<size_t>(i)]);
            float const x = side.cellX(i), y = side.cellY(i);
            if (i == favouriteHover) {
                fill(context, x, y, SideLayout::cell, SideLayout::cell, palette::white, .14f);
                late.tip = item.name;
                hoverItem = favourites[static_cast<size_t>(i)];
            }
            if (auto const* stack = schematic::items::iconStack(item.icon)) late.icons.push_back({stack, x + 1, y + 1});
        }
        if (pressed && favouriteHover >= 0 && favouriteHover < favouriteCount) show(favourites[static_cast<size_t>(favouriteHover)]);
        fill(context, side.left + 3, side.top + side.split(), side.width - 6, 1, palette::white, .2f);
        // The bar: the list's name, which of the two it shows, and the window. Buttons from the right end.
        float barEnd = side.left + side.width - 2;
        auto sideButton = [&](std::string text, bool on) {
            float const w = textWidth(context, text) + 8;
            barEnd -= w;
            ListBox const box{barEnd, side.barY(), w, 11};
            drawButton(context, box.x, box.y, box.w, box.h, std::move(text), box.has(px, py), on ? ButtonKind::Primary : ButtonKind::Plain);
            barEnd -= 2;
            return leftPress && took(box);
        };
        auto openPlan = [&](planView::Tab tab) {
            planView::open(tab);
            viewScreen = screenName;
        };
        if (sideButton(translated("recipes.open"), false)) openPlan(sideGather ? planView::Tab::Gather : planView::Tab::Wanted);
        if (side.wide()) {
            if (sideButton(translated("recipes.wanted"), !sideGather)) sideGather = false;
            if (sideButton(translated("recipes.gather"), sideGather)) sideGather = true;
        }
        if (std::string title = translated("recipes.wanted"); barEnd - side.left - 4 >= textWidth(context, title))
            label(context, side.left + 4, side.barY() + 2, barEnd - side.left - 4, std::move(title), palette::dim);
        // The short list: an icon, the name where there is room, and how many; a wanted item's count has the stepper.
        struct SideRow {
            std::string key, name;
            Count count = 0;
        };
        std::vector<SideRow> rows;
        auto const* plan = state::plan(*player);
        if (!sideGather) {
            for (auto const& [key, amount] : state::wanted().targets) rows.push_back({key, schematic::items::shown(key).name, amount});
        } else if (plan) {
            auto const& index = plan->table->index;
            Item const items = static_cast<Item>(index.items.size());
            auto const turn = static_cast<size_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1500);
            for (auto const& line : plan->view.gather) {
                if (!line.left) continue;
                if (line.id < items) {
                    auto const& key = index.items[static_cast<size_t>(line.id)];
                    rows.push_back({key, schematic::items::shown(key).name, line.left});
                } else {
                    auto const& members = index.groups[static_cast<size_t>(line.id - items)].members;
                    rows.push_back({members.empty() ? std::string() : index.items[static_cast<size_t>(members[turn % members.size()])],
                                    groupName(index, line.id - items), line.left});
                }
            }
        }
        int const fits = side.rows(), total = static_cast<int>(rows.size());
        int const drawn = total > fits ? std::max(0, fits - 1) : total;
        constexpr float stepperWidth = 46;
        for (int i = 0; i < drawn; ++i) {
            auto const& row = rows[static_cast<size_t>(i)];
            float const y = side.rowY(i), end = side.left + side.width - 3;
            ListBox const whole{side.left, y, side.width, SideLayout::row};
            if (!row.key.empty())
                if (auto const* stack = schematic::items::iconStack(schematic::items::shown(row.key).icon)) late.icons.push_back({stack, side.left + 3, y + 1, smallIcon});
            float nameEnd = end - 30;
            if (!sideGather) {
                float const sx = end - stepperWidth;
                drawStepper(context, sx, y + 1, stepperWidth, 10, true, std::to_string(row.count), {.lowEnabled = row.count > 1, .pointerX = px, .pointerY = py});
                Count const step = shiftHeld.load() ? 16 : 1;
                if (leftPress && took({sx, y + 1, 11, 10})) state::setWanted(row.key, row.count > step ? row.count - step : 1);
                else if (leftPress && took({sx + stepperWidth - 11, y + 1, 11, 10})) state::setWanted(row.key, row.count + step);
                else if (took({sx, y + 1, stepperWidth, 10})) {}
                else if (input.sideTurns && ListBox{sx, y + 1, stepperWidth, 10}.has(px, py))
                    state::setWanted(row.key, input.sideTurns < 0 ? row.count + step : row.count > step ? row.count - step : 1);
                nameEnd = sx - 3;
            } else label(context, end - 40, y + 2, 40, std::to_string(row.count), palette::text, Align::Right);
            if (side.wide()) label(context, side.left + 16, y + 2, nameEnd - side.left - 16, row.name, palette::dim);
            if (whole.has(px, py)) {
                if (hoverItem.empty()) hoverItem = row.key;
                if (!side.wide()) late.tip = row.name;
            }
            if (leftPress && took(whole)) openPlan(sideGather ? planView::Tab::Gather : planView::Tab::Wanted);
        }
        if (total > drawn && fits > 0)
            label(context, side.left + 4, side.rowY(drawn) + 2, side.width - 8, translated("recipes.more", total - drawn), palette::faint);
        if (!total)
            label(context, side.left + 4, side.rowY(0) + 2, side.width - 8,
                  translated(!sideGather ? "recipes.nothingWanted" : plan ? "schematic.raw.nothing" : "schematic.raw.reading"), palette::faint);
        // A press on the band that nothing took stays there: it must not reach the view under the pointer's line.
        if (pressed && side.has(px, py)) pressed = false;
    }
    if (!pressed) input.clicks.clear();
    drawTextField(context, field.x, field.y, field.w, field.h, searchFocus, &search, search.value(), translated("recipes.search"));
    if (caretPress) placeCaret(context, search, px, py);
    // Typing needs the platform's keyboard: claimed while the field has the focus, given back with it.
    if (searchFocus && !keyboardOwned) {
        auto& keyboard = client->getKeyboardManager();
        if (keyboard.tryClaimKeyboardOwnership()) {
            if (keyboard.tryEnableKeyboard({}, 128, true, false, false, Vec2{field.x, field.y}, 20.0f)) keyboardOwned = true;
            else keyboard.releaseKeyboardOwnership();
        }
        if (!keyboardOwned) searchFocus = false;
    }
    if (!searchFocus) releaseKeyboard();
    // The recipe view is on top; under it, the window of what is wanted.
    if (view::isOpen())
        view::draw(context, {screen.x, screen.y, px, py, std::move(input.clicks), input.viewTurns, input.back, input.close}, late);
    else if (planView::isOpen()) {
        std::string showItem;
        planView::draw(context, {screen.x, screen.y, px, py, std::move(input.clicks), input.viewTurns, false, input.close}, late, *player,
                       shiftHeld.load(), showItem);
        if (!showItem.empty()) view::open(std::move(showItem), false), viewScreen = screenName;
    }
    // The key hints keep one place, the view's last line; closed, only where the game's window leaves it free.
    bool const open = anyView();
    static std::vector<int> inventoryKeys;
    static std::string backKey, favouriteKey, wantedKey; // the actions' keys as the keymap names them; empty without one
    static std::chrono::steady_clock::time_point keysRead;
    if (auto const now = std::chrono::steady_clock::now(); now - keysRead > std::chrono::seconds(1)) {
        keysRead = now;
        if (auto keyboard = client->getOptions().getCurrentKeyboardRemapping()) {
            auto const& mapped = keyboard->getKeymappingByAction("key.inventory").mKeys.get();
            if (mapped.size() <= 4) inventoryKeys = mapped;
        }
        auto const bindings = Runtime::instance().preferences().bindings;
        auto keyOf = [&](input::Action action) {
            return input::effectiveChord(bindings, action).empty() ? std::string() : actionBindingName(*client, action);
        };
        backKey = keyOf(input::Action::RecipeBack);
        favouriteKey = keyOf(input::Action::RecipeFavourite);
        wantedKey = keyOf(input::Action::RecipeWanted);
    }
    if (open || viewLayout.hintY() >= at.y + size.y + 2) {
        // One line whatever the language: shrunk to the window's width where it is longer.
        std::string hint = translated("recipes.hint");
        if (!favouriteKey.empty()) hint += " | " + translated("recipes.hint.favourite", favouriteKey);
        if (!wantedKey.empty()) hint += " | " + translated("recipes.hint.wanted", wantedKey);
        if (view::isOpen() && !backKey.empty()) hint += " | " + translated("recipes.hint.back", backKey);
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
        shown = {std::chrono::steady_clock::now(), layout, viewLayout, open, px, py, std::move(hoverItem), inventoryKeys, field, side, searchFocus};
    }
    context.flushText(0, std::nullopt);
    if (!late.icons.empty()) inspection::render::drawItemIcons(context, late.icons, 17);
    fills(context, dots, palette::accent);
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
    else if (shown.side.has(shown.pointerX, shown.pointerY)) queued.sideTurns += pages;
    else return shown.viewOpen; // an open view lies over the game's window: its slots must not scroll
    return true;
}
bool press(int button, bool left, bool right, bool down) {
    std::scoped_lock lock(shownLock);
    bool const ours = fresh() && (shown.viewOpen || shown.list.has(shown.pointerX, shown.pointerY) || shown.field.has(shown.pointerX, shown.pointerY)
        || shown.side.has(shown.pointerX, shown.pointerY));
    if (!ours && down && fresh() && shown.searchFocus) queued.outside = true;
    if (!keep(keptButtons, button, down, ours)) return false;
    if (down && (left || right) && queued.clicks.size() < 8) queued.clicks.push_back(left);
    return true;
}
bool key(int code, bool down) {
    // The modifier keys stay the game's: one held while the view opens must still be released.
    if (code == 0x11 || code == 0xA2 || code == 0xA3) ctrlHeld = down;
    if (code == 0x10 || code == 0xA0 || code == 0xA1) shiftHeld = down;
    if (code == 0x10 || code == 0x11 || code == 0x12 || (code >= 0xA0 && code <= 0xA5)) return false;
    std::scoped_lock lock(shownLock);
    // The focused search field takes every key: letters are text, which the game hands over itself.
    if (fresh() && shown.searchFocus) {
        if (!keep(keptKeys, code, down, true)) return false;
        if (down && queued.fieldKeys.size() < 64) queued.fieldKeys.push_back(code | (ctrlHeld ? 0x10000 : 0));
        return true;
    }
    if (!keep(keptKeys, code, down, fresh() && shown.viewOpen)) return false;
    // Esc and the game's inventory key close the view and leave the screen under it open: the key that
    // closes the inventory closes what is on top first.
    bool const games = std::find(shown.inventoryKeys.begin(), shown.inventoryKeys.end(), code) != shown.inventoryKeys.end();
    if (down && (code == 0x1B || games)) queued.close = true;
    return true;
}
bool typing() {
    std::scoped_lock lock(shownLock);
    return fresh() && shown.searchFocus;
}
bool type(std::string const& text) {
    std::scoped_lock lock(shownLock);
    if (!fresh() || !shown.searchFocus) return false;
    if (queued.text.size() + text.size() <= 256) queued.text += text;
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
namespace {
std::string hovered() {
    std::scoped_lock lock(shownLock);
    return fresh() ? shown.hoverItem : std::string();
}
}
void favourite() {
    if (auto item = hovered(); !item.empty()) state::toggleFavourite(item);
}
void want() {
    if (auto item = hovered(); !item.empty()) state::toggleWanted(item);
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
    renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
        try {
            if (drawList(event) || !keyboardOwned) return;
            // The list is gone with the field focused (the screen closed, the switch went off): the keyboard goes back.
            bool gone = false;
            {
                std::scoped_lock lock(shownLock);
                gone = !fresh();
            }
            if (gone) searchFocus = false, releaseKeyboard();
        } catch (...) {}
    });
    // The names belong to the world's items: read them again in the next world.
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { forget = true; });
}
void stop() {
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    forget = true;
}
}
