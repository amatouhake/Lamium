#include "features/recipes/ItemList.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inspection/hover/HoverTracker.h"
#include "features/inspection/render/HeldItem.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/recipes/CountEdit.h"
#include "features/recipes/ItemKey.h"
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/ListFilter.h"
#include "features/recipes/PlanViewDraw.h"
#include "features/recipes/RecipeState.h"
#include "features/recipes/RecipeText.h"
#include "features/recipes/RecipePlace.h"
#include "features/recipes/RecipePlaces.h"
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
#include <map>
#include <mutex>
#include <optional>
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
    bool countFocus = false;        // a wanted item's count is being typed
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
std::atomic<bool> shiftHeld{false}, dropFocus{false}, stopRun{false}; // stopRun: a key went down, which ends a run of crafting
// The left band's short list, as the window's tabs: the wanted items, what is left to gather, what to make.
enum class SideList { Wanted, Gather, Make };
SideList sideList = SideList::Gather;
// The recipe view lies over the window of what is wanted; both go with the screen they were opened on.
void closeViews() {
    view::close();
    planView::close();
    countEdit::cancel();
    place::stop();
}
// The crafting grid of the screen under the view, for its "place" buttons: what is carried is read once a frame.
struct GridPlacer : view::Placer {
    ScreenView& screen;
    std::shared_ptr<service::Table const> table;
    std::map<int, Item> picks;
    int side = 0;
    std::vector<PlaceSource> carried;
    GridPlacer(ScreenView& screen, std::shared_ptr<service::Table const> table, std::map<int, Item> picks, int side)
        : screen(screen), table(std::move(table)), picks(std::move(picks)), side(side), carried(place::carried(screen, this->table->index)) {}
    PlacePlan preview(Recipe const& recipe, bool many) const override { return placePlan(table->index, recipe, side, carried, picks, many); }
    bool run(int recipe, bool many) override { return place::run(screen, table, recipe, picks, many); }
};
bool anyView() { return view::isOpen() || planView::isOpen(); }
KeptPresses keptButtons, keptKeys; // under shownLock
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
        else countEdit::cancel();
        queued = {};
    }
    // The table is read while the list shows, a part a frame, so the first recipe opens at once.
    service::want();
    if (dropFocus.exchange(false)) searchFocus = false, countEdit::cancel();
    // The search field: what was typed since the last frame, then where this frame's press went.
    auto const field = searchFieldAt(screen.x, screen.y);
    if (countEdit::active()) {
        // A count being typed: digits, Enter keeps it, Esc leaves it as it was.
        for (int pressed : input.fieldKeys) {
            int const code = pressed & 0xFFFF;
            if (code == 0x1B) { countEdit::cancel(); break; }
            if (code == 0x0D || code == 0x09) { countEdit::commit(); break; }
            countEdit::typed.digitKey(code);
        }
    } else if (searchFocus) {
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
    // A run of crafting goes on by itself a round every few frames; any press of the player's, or leaving, ends it.
    if (pressed || input.outside || input.close || stopRun.exchange(false)) place::stop();
    place::tick(screenView);
    bool const leftPress = pressed && input.clicks.front();
    auto took = [&](ListBox const& box) {
        if (!pressed || !box.has(px, py)) return false;
        pressed = false;
        return true;
    };
    bool caretPress = false;
    if (pressed || input.outside) searchFocus = false, countEdit::commit();
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
    // A favourite's cell is framed; a wanted item carries how many are wanted, where a stack has its count.
    std::set<std::string_view> favouriteKeys;
    std::map<std::string_view, Count> wantedCounts;
    for (auto const& key : state::notes().favourites) favouriteKeys.insert(key);
    for (auto const& [key, amount] : state::wanted().targets) wantedCounts.emplace(key, amount);
    // What a feature handed over is wanted too.
    auto const handed = state::handed(); // a copy: asking for the plan below may replace the list
    for (auto const& target : handed) wantedCounts[target.key] += target.count;
    auto mark = [&](std::string const& key, float x, float y, bool framed) {
        if (framed && favouriteKeys.contains(key)) frame(context, x, y, ItemListLayout::cell, ItemListLayout::cell, palette::accent, .9f);
        auto wanted = wantedCounts.find(key);
        if (wanted == wantedCounts.end()) return;
        // Three characters fit a cell: thousands above that, and "++" past them.
        Count const n = wanted->second;
        late.counts.push_back({x, y, n < 1000 ? std::to_string(n) : n < 100000 ? std::format("{}k", n / 1000) : std::string("++")});
    };
    for (int i = 0; i < layout.perPage() && first + i < count; ++i) {
        auto const& item = schematic::items::shown(nameAt(i));
        float const x = layout.cellX(i), y = layout.cellY(i);
        if (i == hovered) {
            fill(context, x, y, ItemListLayout::cell, ItemListLayout::cell, palette::white, .14f);
            late.tip = item.name;
        }
        if (auto const* stack = schematic::items::iconStack(item.icon)) late.icons.push_back({stack, x + 1, y + 1});
        mark(nameAt(i), x, y, true);
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
            mark(favourites[static_cast<size_t>(i)], x, y, false); // all here are favourites
        }
        if (pressed && favouriteHover >= 0 && favouriteHover < favouriteCount) show(favourites[static_cast<size_t>(favouriteHover)]);
        fill(context, side.left + 3, side.top + side.split(), side.width - 6, 1, palette::white, .2f);
        // The bar: which of the three lists shows, named as the window's tabs, and the window at the right
        // end. Where the three do not fit, one button that goes round them.
        struct ListName { SideList list; char const* key; planView::Tab tab; };
        static constexpr ListName lists[]{{SideList::Wanted, "recipes.wanted", planView::Tab::Wanted},
                                          {SideList::Gather, "recipes.gather", planView::Tab::Gather},
                                          {SideList::Make, "recipes.make", planView::Tab::Make}};
        int const listNow = sideList == SideList::Wanted ? 0 : sideList == SideList::Gather ? 1 : 2;
        auto openPlan = [&] {
            // The recipe view lies over the window: asked for, the window comes to the front.
            view::close();
            planView::open(lists[listNow].tab);
            viewScreen = screenName;
        };
        auto barButton2 = [&](ListBox const& box, std::string text, bool on) {
            drawButton(context, box.x, box.y, box.w, box.h, std::move(text), box.has(px, py), on ? ButtonKind::Primary : ButtonKind::Plain);
            return leftPress && took(box);
        };
        std::string openText = translated("recipes.open");
        float const openWidth = textWidth(context, openText) + 8;
        ListBox const openBox{side.left + side.width - 2 - openWidth, side.barY(), openWidth, 11};
        if (barButton2(openBox, std::move(openText), false)) openPlan();
        std::string listTexts[3];
        float listWidths[3], listsWidth = 0;
        for (int i = 0; i < 3; ++i) {
            listTexts[i] = translated(lists[i].key);
            listWidths[i] = textWidth(context, listTexts[i]) + 8;
            listsWidth += listWidths[i] + 2;
        }
        float barX2 = side.left + 2;
        float const barRoom = openBox.x - 2 - barX2;
        if (listsWidth - 2 <= barRoom) {
            for (int i = 0; i < 3; ++i) {
                if (barButton2({barX2, side.barY(), listWidths[i], 11}, std::move(listTexts[i]), i == listNow)) sideList = lists[i].list;
                barX2 += listWidths[i] + 2;
            }
        } else if (float const w = std::min(listWidths[listNow], barRoom); w > 12) {
            if (barButton2({barX2, side.barY(), w, 11}, std::move(listTexts[listNow]), true)) sideList = lists[(listNow + 1) % 3].list;
        }
        // The short list: an icon, the name where there is room, and how many; a wanted item's count has the stepper.
        struct SideRow {
            std::string key, name;
            Count count = 0;
            int step = -1; // a row of what to make: its place among the plan's steps
            std::string source; // a wanted item a feature handed over: whose it is; not edited here
        };
        std::vector<SideRow> rows;
        auto const* plan = state::plan(*player);
        if (sideList == SideList::Wanted) {
            for (auto const& [key, amount] : state::wanted().targets) rows.push_back({key, schematic::items::shown(key).name, amount});
            for (auto const& target : handed) rows.push_back({target.key, schematic::items::shown(target.key).name, target.count, -1, target.source});
        } else if (plan && sideList == SideList::Gather) {
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
        } else if (plan) {
            // What to make, in the order it can be made, each with how many come out.
            auto const& index = plan->table->index;
            for (size_t i = 0; i < plan->view.steps.size(); ++i) {
                auto const& key = index.items[static_cast<size_t>(plan->view.steps[i].item)];
                rows.push_back({key, schematic::items::shown(key).name, plan->view.steps[i].made, static_cast<int>(i)});
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
            if (sideList == SideList::Wanted && row.source.empty()) {
                // The stepper: its ends step (minus at one takes the item out), its number is typed after a press.
                float const sx = end - stepperWidth;
                ListBox const box{sx, y + 1, stepperWidth, 10};
                drawStepper(context, sx, y + 1, stepperWidth, 10, true, std::to_string(row.count),
                            {.editing = countEdit::editing(row.key), .pointerX = px, .pointerY = py});
                if (leftPress && took({sx, y + 1, 11, 10})) state::stepWanted(row.key, false, shiftHeld.load(), true);
                else if (leftPress && took({sx + stepperWidth - 11, y + 1, 11, 10})) state::stepWanted(row.key, true, shiftHeld.load(), true);
                else if (leftPress && took(box)) countEdit::begin(row.key, row.count);
                else if (took(box)) {}
                else if (input.sideTurns && box.has(px, py)) state::stepWanted(row.key, input.sideTurns < 0, shiftHeld.load(), false);
                nameEnd = sx - 3;
            } else label(context, end - 40, y + 2, 40, std::to_string(row.count), palette::text, Align::Right);
            if (side.wide()) label(context, side.left + 16, y + 2, nameEnd - side.left - 16, row.name, palette::dim);
            if (whole.has(px, py)) {
                if (hoverItem.empty()) hoverItem = row.key;
                // A row of what to make says what its crafts take.
                if (row.step >= 0 && plan) late.tip = row.name, late.tipLines = stepInputLines(plan->table->index, plan->request, plan->view.steps[static_cast<size_t>(row.step)]);
                // A row a feature handed over says whose it is.
                else if (!row.source.empty()) late.tip = row.name, late.tipLines = {translated("recipes.from." + row.source)};
                else if (!side.wide()) late.tip = row.name;
            }
            if (leftPress && took(whole)) openPlan();
        }
        if (total > drawn && fits > 0)
            label(context, side.left + 4, side.rowY(drawn) + 2, side.width - 8, translated("recipes.more", total - drawn), palette::faint);
        if (!total)
            label(context, side.left + 4, side.rowY(0) + 2, side.width - 8,
                  translated(sideList == SideList::Wanted || (plan && sideList == SideList::Make) ? "recipes.nothingWanted"
                             : plan ? "schematic.raw.nothing" : "schematic.raw.reading"), palette::faint);
        // The band's last line, kept free for it: the open container made a store, or taken out again
        // (L-359 step 8). What it holds is noted while it is open, for the ender chest and for stores.
        if (auto const& open = placesGame::open(screenView)) {
            state::seePlace(open->ender, open->spot, open->pair, open->block, open->items);
            // A name and a switch, as Lamium's rows are: on makes the open container a store (off takes it
            // out again); the ender chest, which is noted anyway, is switched in or out of the count.
            int const index = open->ender ? state::places().ender() : state::places().store(open->spot);
            bool const on = index >= 0 && (!open->ender || state::places().list[static_cast<size_t>(index)].counted);
            ListBox const box{side.left + 2, side.top + side.height - 12, side.width - 4, 11};
            if (box.has(px, py)) fill(context, box.x, box.y, box.w, box.h, palette::white, .08f);
            if (side.wide()) label(context, box.x + 2, box.y + 2, box.w - switchWidth - 6, translated("recipes.place.count"), palette::dim);
            else if (box.has(px, py)) late.tip = translated("recipes.place.count");
            toggleSwitch(context, box.x + box.w - switchWidth - 1, box.y + 1, on);
            if (leftPress && took(box)) {
                if (open->ender) { if (index >= 0) state::setPlaceCounted(index, !on); }
                else if (index < 0) state::markStore(open->spot, open->pair, open->block, open->items);
                else state::unmarkStore(index);
            }
        }
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
    if (view::isOpen()) {
        std::optional<GridPlacer> placer;
        if (int const gridSide = place::gridSide(screenView))
            if (auto table = service::table()) {
                auto const* plan = state::plan(*player);
                placer.emplace(screenView, std::move(table), plan ? plan->request.picks : std::map<int, Item>{}, gridSide);
            }
        view::draw(context, {screen.x, screen.y, px, py, std::move(input.clicks), input.viewTurns, input.back, input.close, shiftHeld.load(),
                             placer ? &*placer : nullptr}, late);
    }
    else if (planView::isOpen()) {
        std::string showItem;
        bool showChoice = false;
        planView::draw(context, {screen.x, screen.y, px, py, std::move(input.clicks), input.viewTurns, false, input.close, false, nullptr}, late, *player,
                       shiftHeld.load(), showItem, showChoice);
        if (!showItem.empty()) view::open(std::move(showItem), false, showChoice), viewScreen = screenName;
    }
    // A recipe laid into the grid: nothing of Lamium's stays over it.
    if (late.placed) view::close(), planView::close(), countEdit::cancel();
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
        // One line whatever the language: the keys, with the presses before them where both fit; shrunk a
        // little to the window's width where it is still longer.
        std::string keys;
        auto add = [&](std::string part) { keys += (keys.empty() ? "" : " | ") + part; };
        if (!favouriteKey.empty()) add(translated("recipes.hint.favourite", favouriteKey));
        if (!wantedKey.empty()) add(translated("recipes.hint.wanted", wantedKey));
        if (view::isOpen() && !backKey.empty()) add(translated("recipes.hint.back", backKey));
        std::string hint = translated("recipes.hint") + (keys.empty() ? "" : " | " + keys);
        if (!keys.empty() && labelWidth(context, hint, 1) * .8f > RecipeViewLayout::width - 8) hint = std::move(keys);
        // On the open window it is the window's text: dark, without a shadow.
        float const room = RecipeViewLayout::width - 8, wide = labelWidth(context, hint, 1), scale = wide > room ? std::max(.6f, room / wide) : 1.f;
        labelScaled(context, viewLayout.left + 4, viewLayout.hintY() + (1 - scale) * 4, room, std::move(hint), scale,
                    open ? palette::vanilla::windowFaint : palette::dim, Align::Center, !open);
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
        shown = {std::chrono::steady_clock::now(), layout, viewLayout, open, px, py, std::move(hoverItem), inventoryKeys, field, side, searchFocus,
                 countEdit::active()};
    }
    context.flushText(0, std::nullopt);
    // Painted after the screen, the bands and the windows cover the item held on the pointer: where it
    // lies over one of them it is drawn once more, last so it is on top.
    auto const held = inspection::render::heldOver(screenView, inspection::render::heldStack(screenView), [&](float x, float y, float w, float h) {
        auto over = [&](float bx, float by, float bw, float bh) { return bw > 0 && x < bx + bw && x + w > bx && y < by + bh && y + h > by; };
        return over(layout.left, layout.top, layout.width, layout.height) || over(field.x, field.y, field.w, field.h)
            || (side.usable() && over(side.left, side.top, side.width, side.height))
            || (open && over(viewLayout.left, viewLayout.top, RecipeViewLayout::width, viewLayout.height));
    });
    if (!late.icons.empty()) inspection::render::drawItemIcons(context, late.icons, 17);
    // In a call of its own: within one call the batched icons, which the lists' are, are drawn after a direct one.
    if (held.stack) {
        inspection::render::drawItemIcon(context, {held.stack, held.x, held.y, held.size / 16, 0, 1.f, true, true}, 17);
        inspection::render::drawHeldMarks(context, held);
    }
    for (auto& text : late.counts) slotCount(context, text.x, text.y, std::move(text.text));
    if (!late.counts.empty()) context.flushText(0, std::nullopt);
    if (!late.tip.empty()) {
        float w = textWidth(context, late.tip);
        for (auto const& line : late.tipLines) w = std::max(w, textWidth(context, line));
        w += 8;
        float const h = 12 + 10 * static_cast<float>(late.tipLines.size());
        float const x = std::min(px + 8, screen.x - w - 2), y = std::min(py + 8, screen.y - h - 2);
        drawTipBox(context, x, y, w, h);
        label(context, x + 4, y + 2, w - 8, late.tip);
        for (size_t i = 0; i < late.tipLines.size(); ++i) label(context, x + 4, y + 12 + 10 * static_cast<float>(i), w - 8, late.tipLines[i], palette::dim);
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
    if (!ours && down && fresh() && (shown.searchFocus || shown.countFocus)) queued.outside = true;
    bool first = false;
    if (!keptButtons.decide(button, down, ours, &first)) return false;
    if (first && (left || right) && queued.clicks.size() < 8) queued.clicks.push_back(left);
    return true;
}
bool key(int code, bool down) {
    // The modifier keys stay the game's: one held while the view opens must still be released.
    if (code == 0x11 || code == 0xA2 || code == 0xA3) ctrlHeld = down;
    if (code == 0x10 || code == 0xA0 || code == 0xA1) shiftHeld = down;
    if (code == 0x10 || code == 0x11 || code == 0x12 || (code >= 0xA0 && code <= 0xA5)) return false;
    if (down) stopRun = true;
    std::scoped_lock lock(shownLock);
    // The focused search field takes every key: letters are text, which the game hands over itself. So
    // does a count being typed: the number keys must not move stacks to the hotbar.
    bool first = false;
    if (fresh() && (shown.searchFocus || shown.countFocus)) {
        if (!keptKeys.decide(code, down, true, &first)) return false;
        // Editing keys repeat while held; leaving the field happens once.
        if (down && (first || (code != 0x1B && code != 0x0D)) && queued.fieldKeys.size() < 64)
            queued.fieldKeys.push_back(code | (ctrlHeld ? 0x10000 : 0));
        return true;
    }
    if (!keptKeys.decide(code, down, fresh() && shown.viewOpen, &first)) return false;
    // Esc and the game's inventory key close the view and leave the screen under it open: the key that
    // closes the inventory closes what is on top first.
    bool const games = std::find(shown.inventoryKeys.begin(), shown.inventoryKeys.end(), code) != shown.inventoryKeys.end();
    if (first && (code == 0x1B || games)) queued.close = true;
    return true;
}
void focusLost() {
    std::scoped_lock lock(shownLock);
    keptKeys = {};
    keptButtons = {};
    queued = {};
    queued.outside = true; // ends a run of crafting with the next frame
    ctrlHeld = false;
    shiftHeld = false;
    dropFocus = true;
}
bool typing() {
    std::scoped_lock lock(shownLock);
    return fresh() && (shown.searchFocus || shown.countFocus);
}
bool type(std::string const& text) {
    std::scoped_lock lock(shownLock);
    if (fresh() && shown.countFocus) return true; // its digits come as keys
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
std::string hoveredItem() { return hovered(); }
void favourite() {
    if (auto item = hovered(); !item.empty()) state::toggleFavourite(item);
}
void want() {
    // The band shows the wanted items then, so what went in or out is seen.
    if (auto item = hovered(); !item.empty()) state::toggleWanted(item), sideList = SideList::Wanted;
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
            if (gone) searchFocus = false, countEdit::cancel(), releaseKeyboard();
        } catch (...) {}
    });
    // The names belong to the world's items: read them again in the next world.
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { forget = true; });
}
void stop() {
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    searchFocus = false;
    countEdit::cancel();
    releaseKeyboard();
    forget = true;
}
}
