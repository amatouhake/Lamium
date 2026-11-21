#include "features/recipes/CountText.h"
#include "features/recipes/GroupLook.h"
#include "features/recipes/ItemList.h"
#include <cmath>
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/inspection/Inspection.h"
#include "features/inspection/hover/HoverTracker.h"
#include "features/inspection/render/HeldItem.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/recipes/BandEditorDraw.h"
#include "features/recipes/BandPages.h"
#include "features/recipes/CountEdit.h"
#include "features/recipes/ItemKey.h"
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/ItemSearch.h"
#include "features/recipes/SlotHighlight.h"
#include "features/recipes/ListFilter.h"
#include "features/inspection/EnglishSearch.h"
#include "features/recipes/PlanViewDraw.h"
#include "features/recipes/RecipeState.h"
#include "features/recipes/RecipeText.h"
#include "features/recipes/RecipePlace.h"
#include "features/recipes/RecipePlaces.h"
#include "features/recipes/RecipeService.h"
#include "features/recipes/RecipeView.h"
#include "features/recipes/RecipeViewDraw.h"
#include "features/schematic/SchematicItems.h"
#include "ui/SettingsScreen.h"
#include "ui/VanillaFrame.h"
#include "ui/WindowFade.h"
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
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemTag.h"
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
std::map<std::string, size_t, std::less<>> nameIndex; // a key's place in `names`, for a slot's item (L-378)
std::atomic<bool> forget{false};
int page = 0;
// The list narrowed: by the search field's text and to what can be made from what is carried.
enum class Filter { All, CanMake };
Filter filter = Filter::All;
ui::TextEdit search{64};
bool searchFocus = false, keyboardOwned = false;
// The items' names as the game shows them, folded for searching (ItemSearch.h): read a part a frame (each asks
// the game). Their English names and tags are read with the list; their tooltips only once a search asks for
// them, a part a frame.
std::vector<std::string> lowNames, englishNames, tagTexts, tooltipTexts;
search::History searchHistory;
std::vector<int> visible; // positions in `names`
struct VisibleFor {
    bool valid = false;
    std::string query;
    Filter filter = Filter::All;
    size_t named = 0, tooltips = 0;
    Bench bench; // the screen's grid or station, which "can be made" depends on (L-380)
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
    SideLayout side;                // the plan's list, which the wheel scrolls
    bool searchFocus = false;
    bool countFocus = false;        // a wanted item's count is being typed
    std::vector<ListBox> bands;     // the other panels and the open container's line
    ListBox favorites;              // the favorites' panel, which the wheel scrolls
    // An open Notes window's span, wider than the recipe view's: the wheel over any of it is the window's, also
    // at its left end where a group's picture stands (L-369).
    ListBox window;
} shown;
struct Queued {
    std::vector<bool> clicks; // true: the left button
    int listTurns = 0, viewTurns = 0, sideTurns = 0, wantedTurns = 0, favoriteTurns = 0;
    bool wantedUp = false; // the wanted key was released
    bool back = false, close = false;
    std::vector<int> fieldKeys; // keys pressed while the search field has the focus; 0x10000 set with Ctrl
    std::string text;           // text the game handed the focused field
    bool outside = false;       // a press the game kept, which takes the focus from the field
} queued;
std::atomic<bool> ctrlHeld{false}; // set by the listeners, read by the frame too (Ctrl + press, L-379)
// The wanted key held over an item (a single key only): the wheel then changes how many are wanted. An
// item that was wanted already goes out on the key's release, unless the wheel turned meanwhile.
std::atomic<bool> wantedHeld{false};
std::atomic<int> wantedCode{0};
std::string wantedItem;      // client thread
bool wantedLeaves = false;   // client thread: it goes out when the key is released
std::atomic<bool> leftHeld{false}; // the left button is down, as the mouse listener last saw it: a scrollbar's drag
std::atomic<bool> shiftHeld{false}, dropFocus{false}, focusAsked{false}, stopRun{false}; // stopRun: a key went down, which ends a run of crafting
std::atomic<bool> gamePress{false}; // the left button went down on the game's window (the stonecutter's notice, L-380)
// The recipe view lies over the window of what is wanted; both go with the screen they were opened on.
void closeViews() {
    bandEditor::stop();
    view::close();
    planView::close();
    countEdit::cancel();
    place::stop();
    place::abandonCraft();
}
// The crafting grid or the station of the screen under the view (L-380), for its "place" buttons: what is carried
// is read once a frame.
struct GridPlacer : view::Placer {
    ScreenView& screen;
    std::shared_ptr<service::Table const> table;
    std::map<int, Item> picks;
    Bench bench;
    std::vector<PlaceSource> carried;
    GridPlacer(ScreenView& screen, std::shared_ptr<service::Table const> table, std::map<int, Item> picks, Bench bench)
        : screen(screen), table(std::move(table)), picks(std::move(picks)), bench(bench), carried(place::carried(screen, this->table->index)) {}
    bool crafting() const { return bench.station == Station::Crafting; }
    PlacePlan preview(Recipe const& recipe, bool many) const override {
        if (!benchMakes(bench, recipe)) return {};
        return placePlan(table->index, recipe, crafting() ? bench.side : 0, carried, picks, many);
    }
    bool places(Kind kind) const override { return benchAny(bench) && kind == benchKind(bench); }
    PlacePlan need(Recipe const& recipe) const override {
        return placePlan(table->index, recipe, recipe.station == Station::Crafting ? 3 : 0, carried, picks, false);
    }
    int gridSide() const override { return crafting() ? bench.side : 0; }
    bool run(int recipe, bool many) override { return place::run(screen, table, recipe, picks, many); }
    CraftOnto craftCheck(Recipe const& recipe) const override {
        return crafting() ? place::craftCheck(screen, table->index, recipe, preview(recipe, false).ready()) : CraftOnto::Missing;
    }
    bool craft(int recipe) override { return crafting() && place::craft(screen, table, recipe, picks); }
    bool runMost(int recipe, int most) override { return place::run(screen, table, recipe, picks, true, most); }
    bool make(std::vector<std::pair<int, int>> const& steps) override {
        std::vector<place::MakeStep> out;
        for (auto [recipe, rounds] : steps) out.push_back({recipe, rounds});
        return place::make(screen, table, std::move(out), picks);
    }
};
bool anyView() { return view::isOpen() || planView::isOpen(); }
// "Can be made" (L-367): a press lays the item's recipe into the grid, or at a station into its slots (L-380), as
// the recipe view's "place" does, with `many` as many as divide evenly: the recipe the player's choices name where
// it can be laid, else the first the screen takes that can. False when none can, or the screen takes none.
bool placeItem(ScreenView& screen, std::string const& key, bool many, LocalPlayer& player) {
    Bench const bench = place::bench(screen);
    auto table = service::table();
    if (!benchAny(bench) || !table) return false;
    auto const& index = table->index;
    auto const split = splitItemKey(key);
    Item const id = index.find(split.name, split.aux);
    if (id < 0 || id >= static_cast<Item>(index.makers.size())) return false;
    auto const* plan = state::plan(player);
    GridPlacer placer(screen, table, plan ? plan->request.picks : std::map<int, Item>{}, bench);
    auto const choice = state::choice(table);
    for (int number : recipesToTry(index, choice.get(), id))
        if (placer.preview(index.recipes[static_cast<size_t>(number)], false).ready()) return placer.run(number, many);
    return false;
}
// Ctrl + a press on a listed item (L-379): one round of its crafting recipe onto the pointer, the recipe in use where
// it fits the grid, else the first that does; which recipe and what that meets now, or none (no grid, no crafting
// recipe that fits it), for the item's tip.
struct CraftAsk { int recipe = -1; CraftOnto check = CraftOnto::Missing; };
CraftAsk craftAsk(GridPlacer const& placer, Item id) {
    auto const& index = placer.table->index;
    if (!placer.crafting() || id < 0 || id >= static_cast<Item>(index.makers.size())) return {};
    auto const choice = state::choice(placer.table);
    CraftAsk out;
    for (int number : recipesToTry(index, choice.get(), id)) {
        auto const& recipe = index.recipes[static_cast<size_t>(number)];
        if (!benchMakes(placer.bench, recipe)) continue;
        if (out.recipe < 0) out.recipe = number;
        if (placer.preview(recipe, false).ready()) { out.recipe = number; break; }
    }
    if (out.recipe >= 0) out.check = placer.craftCheck(index.recipes[static_cast<size_t>(out.recipe)]);
    return out;
}
// On any item screen, for the recipe view's place buttons (which check what is missing anywhere); its `bench` says
// whether the screen takes any recipe.
std::optional<GridPlacer> gridPlacer(ScreenView& screen, LocalPlayer& player) {
    Bench const bench = place::bench(screen);
    auto table = service::table();
    if (!table) return std::nullopt;
    auto const* plan = state::plan(player);
    return std::optional<GridPlacer>(std::in_place, screen, std::move(table), plan ? plan->request.picks : std::map<int, Item>{}, bench);
}
Item itemOf(service::Table const& table, std::string const& key) {
    auto const split = splitItemKey(key);
    return table.index.find(split.name, split.aux);
}
KeptPresses keptButtons, keptKeys; // under shownLock
// Nothing is kept from the game once the list has not been drawn for a moment: a stale "open" must not swallow input.
bool fresh() { return std::chrono::steady_clock::now() - shown.at <= std::chrono::milliseconds(300); }
void releaseKeyboard() {
    if (!keyboardOwned) return;
    keyboardOwned = false;
    auto const started = std::chrono::steady_clock::now();
    if (auto client = ll::service::getClientInstance()) {
        auto& keyboard = client->getKeyboardManager();
        keyboard.disableKeyboard();
        keyboard.releaseKeyboardOwnership();
    }
    // Giving the platform keyboard back is the game's; a slow one is told, a few times a session (L-378).
    static int told = 0;
    auto const us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
    if (us >= 4000 && told < 5) ++told, Runtime::instance().self().getLogger().warn("Recipes: giving the keyboard back took {} us", us);
}
// The list as narrowed now. What can be made changes with the inventory, so it is asked again twice a second.
// `side`: the screen's crafting grid, 0 without one.
void narrow(LocalPlayer& player, Bench const& bench) {
    for (int budget = 150; budget > 0 && lowNames.size() < names.size(); --budget)
        lowNames.push_back(search::folded(schematic::items::shown(names[lowNames.size()]).name));
    std::string query(search.value());
    auto const parsed = search::parse(query);
    // The tooltips' text, only for a search that asks for it: a few dozen a frame, as the game builds each.
    if (search::uses(parsed, search::Field::Tooltip))
        for (int budget = 40; budget > 0 && tooltipTexts.size() < names.size(); --budget) {
            std::string text;
            if (auto const* stack = schematic::items::iconStack(schematic::items::shown(names[tooltipTexts.size()]).icon))
                for (auto const& line : hoverLines(stack->getFormattedHovertext(player.getLevel(), false).mUnredactedString, 24))
                    text += line.text + "\n";
            tooltipTexts.push_back(search::folded(text));
        }
    auto const now = std::chrono::steady_clock::now();
    if (visibleFor.valid && query == visibleFor.query && filter == visibleFor.filter && lowNames.size() == visibleFor.named
        && tooltipTexts.size() == visibleFor.tooltips && bench == visibleFor.bench
        && (filter != Filter::CanMake || now - visibleFor.at < std::chrono::milliseconds(500)))
        return;
    std::shared_ptr<service::Table const> table;
    std::vector<bool> can;
    if (filter == Filter::CanMake && (table = service::table())) {
        // What the inventory holds itself: a shulker box's or bundle's contents cannot go into a grid (L-367).
        std::map<Item, Count> held;
        auto& inventory = player.getInventory();
        for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
            auto const& stack = inventory.getItem(slot);
            if (stack.isNull() || stack.mCount <= 0) continue;
            if (Item const id = table->index.find(stack.getTypeName(), stack.getAuxValue()); id >= 0) held[id] += stack.mCount;
        }
        can = canMakeNow(table->index, held, bench);
    }
    visible.clear();
    for (size_t i = 0; i < names.size(); ++i) {
        auto field = [&](std::vector<std::string> const& from) { return i < from.size() ? std::string_view(from[i]) : std::string_view(); };
        if (!parsed.empty()
            && !search::matches(parsed, {field(lowNames), names[i], field(englishNames), field(tagTexts), field(tooltipTexts)}))
            continue;
        if (filter == Filter::CanMake) {
            if (!table) continue; // not read yet
            auto const key = splitItemKey(names[i]);
            Item const id = table->index.find(key.name, key.aux);
            if (id < 0 || !can[static_cast<size_t>(id)]) continue;
        }
        visible.push_back(static_cast<int>(i));
    }
    if (query != visibleFor.query || filter != visibleFor.filter) page = 0;
    visibleFor = {true, std::move(query), filter, lowNames.size(), tooltipTexts.size(), bench, now};
}
void readNames(LocalPlayer& player) {
    names.clear();
    listed.clear();
    lowNames.clear(), englishNames.clear(), tagTexts.clear(), tooltipTexts.clear(), nameIndex.clear();
    slotHighlight::forget();
    visibleFor = {};
    // English names where English search is on; tags where reading item data is confirmed for this version.
    bool const english = Runtime::instance().snapshot()->inspection.englishSearch;
    bool const tags = versionSensitiveAllowed(capabilities::Id::ItemDetails);
    auto const* registry = player.getLevel().getItemRegistry().getCreativeItemRegistry().get();
    if (!registry) return;
    auto const& entries = registry->mCreativeItems.get();
    if (entries.size() > 20000) return; // not a list of items
    for (auto const& entry : entries) {
        auto const& item = entry.mItemInstance.get();
        if (item.isNull()) continue;
        int const aux = item.getAuxValue();
        std::string name = itemKey(item.getTypeName(), aux == anyAux ? 0 : aux);
        if (name.size() > 200 || !listed.insert(name).second) continue;
        nameIndex.emplace(name, names.size());
        names.push_back(std::move(name));
        englishNames.push_back(english ? search::folded(inspection::englishSearch::englishNameOf(item)) : std::string());
        std::string tagText;
        if (::Item const* type = tags && item.mItem ? item.mItem.get() : nullptr)
            for (auto const& tag : *type->mTags) tagText += tag.getString() + "\n";
        tagTexts.push_back(search::folded(tagText));
    }
}
// The game's window on this screen: the chest's and anvil's panel, the inventory's and crafting table's
// (recipe book and all), the furnace's. Seen in game on 1.26.60 (VALIDATION-LOG 2026-11-18).
// `left`: where the window begins, the game's recipe book included while it shows.
// The screen's shown controls with a size, a bounded walk: for what stands on the window's top (tabs).
void shownControls(UIControl const& control, int depth, std::vector<band::Box>& out) {
    if (depth > 14 || out.size() > 1500) return;
    auto const& children = *control.mChildren;
    if (children.size() > 600) return; // not a child list
    for (auto const& child : children) {
        if (!child || out.size() > 1500 || !(static_cast<int>(child->mVisible) & 1) || child->mCachedPositionDirty) continue;
        glm::vec2 const p = *child->mCachedPosition, s = *child->mSize;
        if (s.x > 0 && s.y > 0) out.push_back({p.x, p.y, s.x, s.y});
        shownControls(*child, depth + 1, out);
    }
}
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
// The bands' editor asked for from the settings (L-375): the page, and when the inventory was asked for.
std::optional<band::Page> pendingEdit;
std::chrono::steady_clock::time_point editAsked{}, inventoryAsked{};
// With the settings gone and the game's HUD in front, the inventory is opened once for the editor.
void openInventoryForEdit(ScreenView& screenView) {
    auto const now = std::chrono::steady_clock::now();
    if (now - editAsked > std::chrono::seconds(5)) { pendingEdit.reset(); return; }
    if (!screenView.mHasFocus || screenView.getScreenName() != "hud_screen" || ui::ownsInput()) return;
    if (now - inventoryAsked < std::chrono::seconds(2)) return;
    inventoryAsked = now;
    if (auto client = ll::service::getClientInstance())
        if (auto* player = client->getLocalPlayer()) player->openInventory();
}
// False when nothing was drawn. The listeners then keep nothing once `fresh` runs out.
// A slow frame of the item screens' parts, told in the log with where its time went (at most a few times a
// session), so what makes a screen heavy is known.
struct FrameCost {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(), mark = start;
    long long walk = 0, highlight = 0, list = 0, plan = 0;
    std::vector<std::pair<char const*, long long>> steps; // the rest of the frame, by where it was spent
    void at(char const* name) { steps.emplace_back(name, lap()); }
    long long lap() {
        auto const now = std::chrono::steady_clock::now();
        auto const us = std::chrono::duration_cast<std::chrono::microseconds>(now - mark).count();
        mark = now;
        return us;
    }
    ~FrameCost() {
        static int told = 0;
        auto const total = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        if (total < 12000 || told >= 12) return;
        ++told;
        std::string rest;
        for (auto const& [name, us] : steps) if (us >= 300) rest += std::format(", {} {} us", name, us);
        Runtime::instance().self().getLogger().warn("Recipes: a slow frame of {} us (window top {} us, highlight {} us, item list {} us, plan {} us{})",
                                                    total, walk, highlight, list, plan, rest);
    }
};
bool drawList(ll::event::UIRenderEvent& event) {
    using namespace lamium::ui;
    FrameCost cost;
    auto& screenView = event.screenView();
    if (pendingEdit) openInventoryForEdit(screenView);
    // Several screens are drawn a frame (the HUD under a container); any but a focused item screen is not
    // this list's business and changes nothing here.
    std::string const screenName = screenView.mHasFocus && screenView.mController && screenView.mController->_isContainerScreen()
        ? screenView.getScreenName() : std::string();
    if (screenName.empty()) return false;
    // All three off (Recipes, the Plan, the stores; each works alone, L-373), or reading the game's item and
    // recipe tables not confirmed for this version: nothing of it shows.
    auto const& on = Runtime::instance().snapshot()->inventory;
    if (!Runtime::instance().enabled() || !(on.recipes || on.plan || on.stores)
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
    cost.at("start");
    if (names.empty()) readNames(*player);
    cost.at("names");
    glm::vec2 at{}, size{};
    float windowLeft = 0;
    if (names.empty() || !windowOf(screenView, at, size, windowLeft)) return false;
    glm::vec2 const screen = *screenView.mSize;
    // Lamium's parts fade with the game's window as it slides in and out, where they stand (WindowFade.h).
    static ui::WindowFade windowFade;
    windowFade.see(screenName, at.y, size.y, screen.y,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count());
    float const shownOpacity = windowFade.opacity(at.y, screen.y);
    DrawOpacity const fade{shownOpacity};
    // The centre keeps the recipe view's two columns free, so the list never moves when it opens. The Recipes
    // window is wider (planWindowAt): while it is open the bands give it room from their inner sides.
    auto const viewLayout = recipeViewAt(screen.x, screen.y);
    float right = std::max(at.x + size.x, viewLayout.left + RecipeViewLayout::width);
    // Between the two bands: what a Lamium window covers, so the game's window does not show beside it.
    float coverLeft = std::min(windowLeft, viewLayout.left);
    // The game's window with its book open, as last seen on the screens that have the book (a villager's is wide too).
    static float bookOpenWidth = planWindowWidth;
    if ((screenName == "inventory_screen" || screenName == "crafting_screen") && bookOpenSpan(coverLeft, right)) bookOpenWidth = right - coverLeft;

    if (planView::isOpen()) {
        auto const span = planWindowAt(screen.x, coverLeft, right, bookOpenWidth);
        coverLeft = std::min(coverLeft, span.left), right = std::max(right, span.left + span.width);
    }
    auto const prefs = Runtime::instance().snapshot();
    // Recipes has the item list on the right, the search field, the favorites and the recipe view.
    bool const recipesOn = prefs->inventory.recipes;
    bool const planOn = prefs->inventory.plan, storesOn = prefs->inventory.stores;
    bool const favoritesOn = recipesOn && prefs->inventory.recipesFavorites;
    // The open container's line (the stores') takes the left region's last units.
    auto const openPlace = storesOn ? placesGame::open(screenView) : std::nullopt;
    // The bands (L-374): regions beside, above and below the game's window, each page in its panel.
    // The window's top with the game's tabs on it, so a region above leaves them free: looked at again when
    // the window moves or every half second (a walk of the screen's controls).
    // The walk is made while the window rests, at most twice a second; while it slides the tabs move with it, so
    // the top keeps its distance from the window's (a walk every frame of the slide made the screen heavy).
    static struct { std::string screen; glm::vec2 at{}, size{}; float left = 0, right = 0, above = 0; std::chrono::steady_clock::time_point when{}; } tabs;
    cost.lap();
    {
        auto const now = std::chrono::steady_clock::now();
        bool const resting = shownOpacity >= .999f, fresh = tabs.screen != screenName;
        bool const changed = std::abs(tabs.at.x - at.x) > .5f || std::abs(tabs.at.y - at.y) > .5f || std::abs(tabs.size.x - size.x) > .5f
            || std::abs(tabs.size.y - size.y) > .5f || std::abs(tabs.left - coverLeft) > .5f || std::abs(tabs.right - right) > .5f;
        if (fresh || (resting && changed && now - tabs.when > std::chrono::milliseconds(500))) {
            std::vector<band::Box> controls;
            if (auto* tree = screenView.mVisualTree.get()) {
                std::shared_ptr<UIControl> const& root = tree->mRootControl;
                if (root) shownControls(*root, 0, controls);
            }
            tabs = {screenName, at, size, coverLeft, right, at.y - band::windowTop(at.y, windowLeft, at.x + size.x, controls), now};
        }
    }
    cost.walk = cost.lap();
    auto const regions = band::regionsAround(screen.x, screen.y, coverLeft, right, at.y - tabs.above, at.y + size.y, viewLayout.hintY() - 1);
    // Asked for from the settings, the editor starts on the first item screen drawn.
    static std::string editScreen; // the editor belongs to the screen it started on
    if (pendingEdit) closeViews(), bandEditor::start(*pendingEdit), pendingEdit.reset(), editScreen = screenName;
    if (bandEditor::active() && screenName != editScreen) bandEditor::stop();
    bool const editing = bandEditor::active();
    auto const shownBands = editing ? bandEditor::layout(prefs->inventory.bands) : prefs->inventory.bands;
    std::array<bool, band::pageCount> const pagesOn{recipesOn, favoritesOn, planOn};
    auto placed = band::place(shownBands, regions, pagesOn);
    auto const* itemsPanel = band::find(placed, band::Page::Items);
    auto const* planPanel = band::find(placed, band::Page::Plan);
    auto const layout = itemsPanel ? itemListIn(itemsPanel->box) : ItemListLayout{};
    auto planSide = [&](band::Box box) { return sidePanelIn(box, false); };
    // The plan's list, for the wheel over it (the listeners' side).
    auto const side = planPanel ? planSide(planPanel->box) : SideLayout{};
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
    cost.at("window");
    service::want();
    cost.at("table");
    if (dropFocus.exchange(false)) searchFocus = false, countEdit::cancel();
    // The search key (L-377): the cursor in the field, what is there selected.
    if (focusAsked.exchange(false) && prefs->inventory.recipes && !bandEditor::active()) searchFocus = true, search.selectAll(), countEdit::commit();
    // The search field: what was typed since the last frame, then where this frame's press went.
    // The row widens where the panels leave room beside it (L-382).
    float const rowW = [&] {
        float leftEnd = 0, rightStart = screen.x, top = screen.y - searchTop, bottom = top + 16, middle = screen.x / 2;
        for (auto const& panel : placed) {
            auto const& b = panel.box;
            if (b.y + b.h <= top || b.y >= bottom) continue;
            if (b.x + b.w <= middle) leftEnd = std::max(leftEnd, b.x + b.w);
            else if (b.x >= middle) rightStart = std::min(rightStart, b.x);
        }
        return rowWidth(screen.x, leftEnd, rightStart);
    }();
    auto const field = searchFieldAt(screen.x, screen.y, rowW);
    // The bottom row's buttons (L-382): the Recipes window, while a feature with a tab is on; arranging the
    // panels, while one is shown.
    auto const row = bottomRowAt(screen.x, screen.y, rowW);
    bool const windowButton = recipesOn || planOn || storesOn, layoutButton = recipesOn || planOn;
    ListBox const windowBox = windowButtonAt(row), layoutBox = layoutButtonAt(row);
    // What the listeners keep from the game for the row: the field and a few units around it, and its buttons.
    ListBox const fieldArea = [&] {
        auto const hit = searchFieldHit(field), end = searchHighlightAt(field);
        float const from = windowButton ? row.x - 2 : hit.x;
        return ListBox{from, hit.y, end.x + end.w + 2 - from, hit.h};
    }();
    if (countEdit::active()) {
        // A count being typed, with the search field's keys (L-370): digits, the caret's, Enter keeps it, Esc leaves
        // it as it was.
        for (int pressed : input.fieldKeys) {
            int const code = pressed & 0xFFFF;
            auto& typed = countEdit::typed;
            if (code == 0x1B) { countEdit::cancel(); break; }
            if (code == 0x0D || code == 0x09) { countEdit::commit(); break; }
            if (code == 0x2E) typed.erase();
            else if (code == 0x25) typed.left();
            else if (code == 0x27) typed.right();
            else if (code == 0x24) typed.home();
            else if (code == 0x23) typed.end();
            else if ((pressed & 0x10000) && code == 0x41) typed.selectAll();
            else typed.digitKey(code);
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
            else if (ctrl && (code == 0x41 || code == 0x46)) search.selectAll();
            else if ((code == 0x26 || code == 0x28) && !searchHistory.entries.empty()) {
                // Up and Down step through earlier searches; Down past the newest empties the field.
                std::string const earlier = searchHistory.step(code == 0x26);
                search.clear();
                search.append(earlier);
            }
        }
        if (!input.text.empty()) search.type(input.text);
    }
    // The wanted key held: each notch of the wheel steps the item's count; released without one, an item
    // that was wanted before the press goes out.
    if (input.wantedTurns && !wantedItem.empty() && state::wanted().listed(wantedItem)) {
        for (int i = 0; i < std::abs(input.wantedTurns) && i < 64; ++i) state::stepWanted(wantedItem, input.wantedTurns < 0, shiftHeld.load(), false);
        wantedLeaves = false;
    }
    if (input.wantedUp) {
        if (wantedLeaves && !wantedItem.empty() && state::wanted().has(wantedItem)) state::toggleWanted(wantedItem);
        wantedItem.clear();
        wantedLeaves = false;
    }
    // One press a frame. Each control asks whether it was on it; what none takes goes to the recipe view.
    bool pressed = !input.clicks.empty();
    // A run of crafting goes on by itself a round every few frames; any press of the player's, or leaving, ends it.
    if (pressed || input.outside || input.close || stopRun.exchange(false)) place::stop();
    place::tick(screenView);
    bool const leftPress = pressed && input.clicks.front();
    // While the panels are arranged every press is the editor's, and nothing under it acts; but the layout's own
    // button ends arranging again, as Done does (L-382).
    bool const layoutAgain = editing && leftPress && layoutButton && layoutBox.has(px, py);
    if (layoutAgain) bandEditor::stop();
    bool const editPress = editing && leftPress && !layoutAgain;
    if (editing) pressed = false;
    auto took = [&](ListBox const& box) {
        if (!pressed || !box.has(px, py)) return false;
        pressed = false;
        return true;
    };
    bool caretPress = false;
    if (pressed || input.outside) searchFocus = false, countEdit::commit();
    // The highlight switch beside the field (L-378): saved at once, as a setting row would.
    if (recipesOn && took(searchHighlightAt(field))) {
        auto preferences = Runtime::instance().preferences();
        preferences.inventory.searchHighlight = !preferences.inventory.searchHighlight;
        Runtime::instance().save(preferences);
    } else if (windowButton && !editing && took(windowBox)) {
        // Open on the last tab, or closed again; over the recipe view it comes to the front.
        if (planView::isOpen() && !view::isOpen()) planView::close();
        else view::close(), planView::openLast(), viewScreen = screenName;
    } else if (layoutButton && !editing && took(layoutBox)) {
        closeViews();
        bandEditor::start(planOn ? band::Page::Plan : band::Page::Items);
        editScreen = screenName;
    } else if (recipesOn && took(searchFieldHit(field))) searchFocus = caretPress = true;
    std::string hoverItem;
    auto& context = event.uiRenderContext();
    // The game's slots that do not match the search darkened (L-378), under everything of Lamium's.
    if (recipesOn && prefs->inventory.searchHighlight && !anyView() && !editing && search.value().find_first_not_of(' ') != std::string::npos) {
        auto const query = search::parse(search.value());
        cost.lap();
        slotHighlight::draw(context, screenView, shownOpacity >= .999f, at.x, at.y, search.value(), [&](ItemStack const& stack) {
            int const aux = stack.getAuxValue();
            std::string const key = itemKey(stack.getTypeName(), aux == anyAux ? 0 : aux);
            if (auto found = nameIndex.find(key); found != nameIndex.end()) {
                size_t const i = found->second;
                auto field = [&](std::vector<std::string> const& from) { return i < from.size() ? std::string_view(from[i]) : std::string_view(); };
                return search::matches(query, {field(lowNames), names[i], field(englishNames), field(tagTexts), field(tooltipTexts)});
            }
            // An item the list does not hold: its shown name and its id.
            std::string const name = search::folded(schematic::items::shown(key).name);
            return search::matches(query, {name, key, {}, {}, {}});
        });
        cost.highlight = cost.lap();
    }
    view::Late late;
    // A wanted item carries how many are wanted, where a stack has its count.
    std::map<std::string_view, Count> wantedCounts;
    if (planOn) for (auto const& [key, amount] : state::wanted().targets) wantedCounts.emplace(key, amount);
    // What a feature handed over is wanted too.
    auto const handed = state::handed(); // a copy: asking for the plan below may replace the list
    if (planOn) for (auto const& target : handed) wantedCounts[target.key] += target.count;
    auto mark = [&](std::string const& key, float x, float y, bool framed) {
        // A favorite carries no mark (L-369): it stands in the favorites' panel; green means chosen.
        (void)framed;
        auto wanted = wantedCounts.find(key);
        if (wanted == wantedCounts.end()) return;
        // Three characters fit a cell: thousands above that, and "++" past them.
        Count const n = wanted->second;
        late.counts.push_back({x, y, slotCountText(n)});
    };
    // A press on an item shows its recipes, a right press what it is used in, also beside an open view. With Shift
    // a press lays in as many as divide evenly, with Ctrl it crafts one round onto the pointer (L-379); where the
    // item cannot be (its recipes show the reason), it shows them.
    auto show = [&](std::string const& item) {
        // Shift lays in as many as divide evenly, as under "can be made" (maintainer 2026-11-21).
        if (leftPress && shiftHeld.load() && !ctrlHeld.load() && placeItem(screenView, item, true, *player)) {
            pressed = false, view::close(), planView::close();
            return;
        }
        // Where nothing can be crafted (no grid, nothing that fits it, something missing) the press shows the recipes.
        if (leftPress && ctrlHeld.load())
            if (auto placer = gridPlacer(screenView, *player))
                if (auto const ask = craftAsk(*placer, itemOf(*placer->table, item)); ask.recipe >= 0 && ask.check != CraftOnto::Missing) {
                    pressed = false;
                    placer->craft(ask.recipe);
                    return;
                }
        view::open(item, !leftPress);
        viewScreen = screenName;
        pressed = false;
    };
    // The item list on the right: Recipes'.
    if (recipesOn && layout.usable()) {
        cost.lap();
        narrow(*player, place::bench(screenView));
        cost.list = cost.lap();
        int const count = static_cast<int>(visible.size()), pages = layout.pages(count);
        page = std::clamp(page + input.listTurns, 0, pages - 1);
        int const first = page * layout.perPage(), hovered = layout.hit(px, py);
        auto nameAt = [&](int cell) -> std::string const& { return names[static_cast<size_t>(visible[static_cast<size_t>(first + cell)])]; };
        if (hovered >= 0 && first + hovered < count) hoverItem = nameAt(hovered);
        // A dimmed panel to the screen's edge: the game dims the world too little for bare icons.
        fill(context, layout.left, layout.top, layout.width, layout.height, palette::panel, .5f);
        if (!itemsPanel->first) fill(context, layout.left + 3, layout.top, layout.width - 6, 1, palette::white, .2f);
        // The pager at the bar's right end; the wheel turns pages too.
        auto const pager = drawPager(context, layout.left + layout.width - 3, layout.top + 1, page, pages, px, py);
        if (leftPress && took({pager.prevX, pager.y, Pager::size, Pager::size})) page = std::max(0, page - 1);
        else if (leftPress && took({pager.nextX, pager.y, Pager::size, Pager::size})) page = std::min(pages - 1, page + 1);
        // The filters keep the bar's left end: both where the band is wide, one switch where it is not.
        float barX = layout.left + 2;
        auto barButton = [&](std::string text, bool on) {
            ListBox const box{barX, layout.top + 1, textWidth(context, text) + 10, 11};
            drawButton(context, box.x, box.y, box.w, box.h, std::move(text), box.has(px, py), on ? ButtonKind::Primary : ButtonKind::Plain);
            barX += box.w + 2;
            return leftPress && took(box);
        };
        if (layout.width >= 130) {
            // One of the two is chosen: tabs, as the band's lists are.
            auto barTab = [&](std::string text, bool on, bool first) {
                ListBox const box{barX, layout.top + 1, textWidth(context, text) + tabPad, tabHeight};
                drawTab(context, box.x, box.y, box.w, std::move(text), on, box.has(px, py), first);
                barX += box.w;
                return leftPress && took(box);
            };
            if (barTab(translated("recipes.filter.all"), filter == Filter::All, true)) filter = Filter::All;
            if (barTab(translated("recipes.filter.canMake"), filter == Filter::CanMake, false)) filter = Filter::CanMake;
            tabRule(context, layout.left + 2, layout.top + 1 + tabHeight, layout.width - 4);
        } else if (barButton(translated("recipes.filter.canMakeShort"), filter == Filter::CanMake))
            filter = filter == Filter::CanMake ? Filter::All : Filter::CanMake;
        if (!count) label(context, layout.left + 4, layout.top + ItemListLayout::bar + 4, layout.width - 8,
                          translated(filter == Filter::CanMake && !service::table() ? "schematic.raw.reading" : "recipes.notFound"), palette::dim);
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
        if (pressed && hovered >= 0 && first + hovered < count) {
            // Under "can be made" a press lays the recipe into the screen's grid, as the game's recipe book does;
            // where nothing can be laid (no grid, nothing that fits) it shows the recipes as elsewhere.
            if (filter == Filter::CanMake && leftPress && !ctrlHeld.load() && placeItem(screenView, nameAt(hovered), shiftHeld.load(), *player))
                pressed = false, view::close(), planView::close();
            else show(nameAt(hovered));
        }
    }
    // The bands' other pages, each in its panel (L-374): the favorites, the plan, and the open container's
    // line at the left region's end.
    bandPages::Frame frame{context, px, py, leftPress, pressed, late, hoverItem, mark, show, input.sideTurns, shiftHeld.load(), leftHeld.load(),
                           input.favoriteTurns};
    auto bandPlacer = planOn ? gridPlacer(screenView, *player) : std::nullopt;
    frame.placer = bandPlacer ? &*bandPlacer : nullptr;
    frame.ctrl = ctrlHeld.load();
    ListBox favoritesBox;
    std::vector<ListBox> bandBoxes; // where a press stays from the game and the view
    for (auto const& panel : placed) {
        if (panel.page == band::Page::Items) continue;
        bandPages::ground(frame, panel.box, !panel.first);
        bandBoxes.push_back({panel.box.x, panel.box.y, panel.box.w, panel.box.h});
        if (panel.page == band::Page::Favorites) {
            favoritesBox = {panel.box.x, panel.box.y, panel.box.w, panel.box.h};
            bandPages::favorites(frame, sidePanelIn(panel.box, true));
        }
        else {
            cost.lap();
            bandPages::plan(frame, planSide(panel.box), *player, handed, [&](planView::Tab tab) {
            // The recipe view lies over the window: asked for, the window comes to the front.
            view::close();
            planView::open(tab);
            viewScreen = screenName;
        });
            cost.plan += cost.lap();
        }
    }
    if (openPlace) {
        // On the game window's title line, left of its close button and drawn as it is (L-382).
        ListBox const box{at.x + size.x - 37, at.y + 5, 15, 15}; // about the close button's size, two units left of it
        bandPages::storeButton(frame, box, *openPlace);
        bandBoxes.push_back(box);
    }
    // A press on a band that nothing took stays there: it must not reach the view under the pointer's line.
    if (pressed && std::any_of(bandBoxes.begin(), bandBoxes.end(), [&](ListBox const& box) { return box.has(px, py); })) pressed = false;
    if (editing)
        bandEditor::draw(context, {px, py, editPress, leftHeld.load(), input.close}, prefs->inventory.bands, regions, pagesOn, placed,
                         {windowLeft, at.y, at.x + size.x - windowLeft, size.y}, late);
    if (editing) input.close = false;
    cost.at("bands");
    if (!pressed) input.clicks.clear();
    if (!recipesOn) searchFocus = false;
    else drawTextField(context, field.x, field.y, field.w, field.h, searchFocus, &search, search.value(), translated("recipes.search"), 4, Align::Left, true);
    if (caretPress) placeCaret(context, search, px, py);
    if (recipesOn) {
        auto const box = searchHighlightAt(field);
        bool const on = Runtime::instance().preferences().inventory.searchHighlight, over = box.has(px, py);
        drawPictureButton(context, box.x, box.y, box.w, Picture::Highlight, over, on);
        if (over) late.tip = translated("recipes.highlight", translated(on ? "on" : "off"));
    }
    if (windowButton) {
        bool const over = windowBox.has(px, py);
        drawPictureButton(context, windowBox.x, windowBox.y, windowBox.w, Picture::Window, over, planView::isOpen() && !view::isOpen());
        if (over) {
            // Its name, and the tabs it holds now.
            late.tip = translated("recipes.notes");
            std::string tabs;
            auto add = [&](char const* key) { tabs += (tabs.empty() ? "" : translated("recipes.notes.joiner")) + translated(key); };
            if (planOn) add("recipes.wanted"), add("recipes.gather"), add("recipes.make");
            if (recipesOn) add("recipes.howTo");
            if (storesOn) add("recipes.places");
            late.tipLines = {tabs};
        }
    }
    if (layoutButton) {
        bool const over = layoutBox.has(px, py);
        drawPictureButton(context, layoutBox.x, layoutBox.y, layoutBox.w, Picture::Layout, over, editing);
        if (over) late.tip = translated("recipes.arrange");
    }
    // The syntax as a table under the pointer over the field while it is not typed in (L-377): one there means
    // to search anyway.
    if (recipesOn && !searchFocus && searchFieldHit(field).has(px, py) && late.tip.empty()) {
        late.tip = translated("recipes.search");
        late.tipTable = {{"word word", translated("recipes.search.all")}, {"-word", translated("recipes.search.not")},
                         {"a|b", translated("recipes.search.either")}, {"\"a b\"", translated("recipes.search.phrase")},
                         {"#…", translated("recipes.search.tag")}, {"$…", translated("recipes.search.tooltip")},
                         {"@…", translated("recipes.search.namespace")}, {translated("recipes.search.upDown"), translated("recipes.search.history")}};
    }
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
    // A search is kept for Up and Down once it has stood a moment while typed, or the field gives up the focus.
    static bool wasFocused = false;
    if (wasFocused && !searchFocus) searchHistory.keep(search.value());
    else if (searchFocus)
        searchHistory.typed(search.value(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count());
    wasFocused = searchFocus;
    cost.at("field");
    // The wheel over a group's slot the pointer has rested on looks through its members instead (L-369); the
    // slots drawn below say where the pointer rests now.
    if (input.viewTurns)
        if (int const group = groupLook::rested(); group >= 0) groupLook::turn(group, input.viewTurns), input.viewTurns = 0;
    groupLook::beginFrame();
    // The recipe view is on top; under it, the window of what is wanted.
    if (view::isOpen()) {
        auto placer = gridPlacer(screenView, *player);
        view::Input viewInput{screen.x, screen.y, px, py, std::move(input.clicks), input.viewTurns, input.back, input.close, shiftHeld.load(), leftHeld.load(), planOn,
                              favoritesOn, placer ? &*placer : nullptr, coverLeft, right};
        viewInput.ctrl = ctrlHeld.load();
        view::draw(context, viewInput, late);
    }
    else if (planView::isOpen()) {
        std::string showItem;
        bool showChoice = false;
        // The screen as a place to lay in or make a plan's steps (L-385).
        auto placer = gridPlacer(screenView, *player);
        view::Input planInput{screen.x, screen.y, px, py, std::move(input.clicks), input.viewTurns, false, input.close, shiftHeld.load(), leftHeld.load(), planOn,
                              true, placer ? &*placer : nullptr, coverLeft, right, recipesOn, storesOn};
        planInput.ctrl = ctrlHeld.load();
        planView::draw(context, planInput, late, *player, shiftHeld.load(), showItem, showChoice);
        if (!showItem.empty() && recipesOn) view::open(std::move(showItem), false, showChoice), viewScreen = screenName;
    }
    // A recipe laid into the grid: nothing of Lamium's stays over it.
    if (late.placed) view::close(), planView::close(), countEdit::cancel();
    // The key hints keep one place, the view's last line; closed, only where the game's window leaves it free.
    bool const open = anyView() || bandEditor::active();
    static std::vector<int> inventoryKeys;
    static std::string backKey, favoriteKey, wantedKey; // the actions' keys as the keymap names them; empty without one
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
        favoriteKey = keyOf(input::Action::RecipeFavorite);
        wantedKey = keyOf(input::Action::RecipeWanted);
        {
            auto const chord = input::effectiveChord(Runtime::instance().preferences().bindings, input::Action::RecipeWanted);
            wantedCode = chord.size() == 1 && chord.front().device == input::Device::Key ? chord.front().code : 0;
        }
    }
    if ((open || viewLayout.hintY() >= at.y + size.y + 2) && (recipesOn || planOn) && !bandEditor::active()) {
        // One line whatever the language: the keys, with the presses before them where both fit; shrunk a
        // little to the window's width where it is still longer.
        std::string keys;
        auto add = [&](std::string part) { keys += (keys.empty() ? "" : " | ") + part; };
        if (favoritesOn && !favoriteKey.empty()) add(translated("recipes.hint.favorite", favoriteKey));
        if (planOn && !wantedKey.empty()) add(translated("recipes.hint.wanted", wantedKey));
        if (view::isOpen() && !backKey.empty()) add(translated("recipes.hint.back", backKey));
        for (auto& part : late.hints) add(std::move(part)); // what the thing under the pointer does (L-369)
        std::string hint = recipesOn ? translated("recipes.hint") + (keys.empty() ? "" : " | " + keys) : keys;
        if (!keys.empty() && labelWidth(context, hint, 1) * .8f > RecipeViewLayout::width - 8) hint = std::move(keys);
        // On the open window it is the window's text: dark, without a shadow.
        float const room = RecipeViewLayout::width - 8, wide = labelWidth(context, hint, 1), scale = wide > room ? std::max(.6f, room / wide) : 1.f;
        labelScaled(context, viewLayout.left + 4, viewLayout.hintY() + (1 - scale) * 4, room, std::move(hint), scale,
                    open ? palette::vanilla::windowFaint : palette::dim, Align::Center, !open);
    }
    if (hoverItem.empty()) hoverItem = late.hovered;
    // An item the wheel took to 0 goes out once the pointer has left it (or the wanted key is up).
    state::settleWanted(wantedHeld.load() ? std::string_view(wantedItem) : !late.wantedRow.empty() ? std::string_view(late.wantedRow) : std::string_view(hoverItem));
    std::string tipItem;
    // Under an open window, as wide as it is drawn, the game's slots are covered: they name no item.
    bool const covered = open && px >= coverLeft && px < right && py >= viewLayout.top - RecipeViewLayout::tabsAbove
        && py < viewLayout.top + viewLayout.height;
    if (covered) inspection::dropDeferredTooltip();
    if (hoverItem.empty() && !covered) {
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
        tipItem = hoverItem; // kept for the tooltip: what is shown takes the string
        shown = {std::chrono::steady_clock::now(), layout, viewLayout, open, px, py, std::move(hoverItem), inventoryKeys, (recipesOn || windowButton || layoutButton) ? fieldArea : ListBox{}, side, searchFocus,
                 countEdit::active(), bandBoxes, favoritesBox};
        if (planView::isOpen() && !view::isOpen()) {
            float const from = std::min(coverLeft, viewLayout.left), to = std::max(right, viewLayout.left + RecipeViewLayout::width);
            shown.window = {from, viewLayout.top, to - from, viewLayout.height};
        }
    }
    context.flushText(0, std::nullopt);
    // Painted after the screen, the bands and the windows cover the item held on the pointer: where it
    // lies over one of them it is drawn once more, last so it is on top.
    auto const held = inspection::render::heldOver(screenView, inspection::render::heldStack(screenView), [&](float x, float y, float w, float h) {
        auto over = [&](float bx, float by, float bw, float bh) { return bw > 0 && x < bx + bw && x + w > bx && y < by + bh && y + h > by; };
        return over(layout.left, layout.top, layout.width, layout.height) || over(field.x, field.y, field.w, field.h)
            || std::any_of(bandBoxes.begin(), bandBoxes.end(), [&](ListBox const& b) { return over(b.x, b.y, b.w, b.h); })
            || (open && over(coverLeft, viewLayout.top - RecipeViewLayout::tabsAbove, right - coverLeft, viewLayout.height + RecipeViewLayout::tabsAbove));
    });
    for (auto& icon : late.icons) icon.alpha *= shownOpacity;
    if (!late.icons.empty()) inspection::render::drawItemIcons(context, late.icons, 17);
    // In a call of its own: within one call the batched icons, which the lists' are, are drawn after a direct one.
    if (held.stack) {
        inspection::render::drawItemIcon(context, {held.stack, held.x, held.y, held.size / 16, 0, 1.f, true, true}, 17);
        inspection::render::drawHeldMarks(context, held);
    }
    for (auto& text : late.counts) slotCount(context, text.x, text.y, std::move(text.text));
    for (auto const& mark : late.marks) anyMark(context, mark.x, mark.y, mark.scale);
    if (!late.counts.empty() || !late.marks.empty()) context.flushText(0, std::nullopt);
    cost.at("windows and icons");
    // At the stonecutter, after Lamium laid its input in: a tip over the result list asks for a result (L-380).
    if (bool const pressedGame = gamePress.exchange(false); place::stonecutterNotice(screenView, pressedGame) && !anyView() && !editing)
        if (auto* tree = screenView.mVisualTree.get()) {
            static std::string const listPanel = "stone_book_panel"; // the stonecutter's left panel (seen 2026-11-21)
            auto panel = tree->getControlByName(listPanel, true);
            if (panel && (static_cast<int>(panel->mVisible) & 1) && !panel->mCachedPositionDirty && panel->mSize->x > 0) {
                namespace vf = vanilla_frame;
                glm::vec2 const p = *panel->mCachedPosition, s = *panel->mSize;
                std::string const text = translated("recipes.stonecutter.choose");
                float const w = std::ceil(vf::tooltipWidth(textWidth(context, text))), h = vf::tooltipHeight(1);
                // Above the panel, clear of it: the list starts at its top edge (seen 2026-11-21). On whole units.
                float const x = std::round(p.x + (s.x - w) / 2), y = std::round(std::max(2.f, p.y - h - 2));
                context.flushText(0, std::nullopt);
                gameTooltipBox(context, x, y, w, h);
                label(context, x + vf::tooltipTextLeft, y + vf::tooltipTextTop - 1, w - 2 * vf::tooltipTextLeft, text);
                context.flushText(0, std::nullopt);
            }
        }
    if (!late.tip.empty()) {
        // An item Lamium draws gets what the game says of it under its name, with Lamium's item details
        // (L-363), in a frame like the game's tooltip; other tips (a tab's name) stay Lamium's plain tip.
        bool const item = !tipItem.empty() && !late.tipGroup; // a group's tip is about the group, not the member shown
        static std::string detailsOf;
        static std::vector<HoverLine> details;
        if (item && tipItem != detailsOf) {
            detailsOf = tipItem;
            details.clear();
            try {
                if (auto const* stack = schematic::items::iconStack(schematic::items::shown(tipItem).icon))
                    details = hoverLines(stack->getFormattedHovertext(player->getLevel(), false).mUnredactedString);
            } catch (...) { details.clear(); }
        }
        std::vector<HoverLine> const none;
        auto const& more = item ? details : none;
        float w = textWidth(context, late.tip);
        for (auto const& line : late.tipLines) w = std::max(w, textWidth(context, line));
        for (auto const& line : more) w = std::max(w, textWidth(context, line.text));
        // A table's first column as wide as its widest entry, the second after a gap.
        float tableLeft = 0;
        for (auto const& [left, right] : late.tipTable) tableLeft = std::max(tableLeft, textWidth(context, left));
        if (!late.tipTable.empty()) tableLeft += 8;
        for (auto const& [left, right] : late.tipTable) w = std::max(w, tableLeft + textWidth(context, right));
        int const lines = 1 + static_cast<int>(late.tipLines.size() + more.size() + late.tipTable.size());
        // A group's members under the lines as slots' icons, full size, eight to a row, each with what is held and
        // the one its slot shows framed (L-369).
        constexpr size_t perRow = 8;
        constexpr float cellSize = 18;
        size_t const members = std::min<size_t>(late.tipItems.size(), 40);
        float const membersH = members ? static_cast<float>((members + perRow - 1) / perRow) * cellSize + 2 : 0.f;
        if (members) w = std::max(w, static_cast<float>(std::min(members, perRow)) * cellSize);
        // An item's tip, and any tip over Lamium's light windows, is the game's tooltip box (VanillaFrame.h,
        // L-367); other tips are Lamium's, two units under the last line so letters that reach below it
        // (p, y) stay inside the frame.
        namespace vf = vanilla_frame;
        bool const game = item || late.tipGroup || covered || late.tipGame; // a group's tip is an item's too
        float const inset = game ? vf::tooltipTextLeft : 4, top = game ? vf::tooltipTextTop - 1 : 2;
        w = std::min(game ? vf::tooltipWidth(w) : w + 8, screen.x - 4);
        float const h = (game ? vf::tooltipHeight(lines) : 4 + 10 * static_cast<float>(lines)) + membersH;
        // On whole units: the pointer lies between them, and the box and the text, rounded to pixels each their
        // own way, moved a pixel apart now and then as it went.
        w = std::ceil(w);
        float const x = std::round(std::max(2.f, std::min(px + 8, screen.x - w - 2))), y = std::round(std::max(2.f, std::min(py + 8, screen.y - h - 2)));
        if (game) {
            context.flushText(0, std::nullopt);
            gameTooltipBox(context, x, y, w, h);
        } else drawTipBox(context, x, y, w, h);
        label(context, x + inset, y + top, w - 2 * inset, late.tip);
        float line = y + top + 10;
        for (auto const& text : late.tipLines) label(context, x + inset, line, w - 2 * inset, text, palette::dim), line += 10;
        for (auto const& [left, right] : late.tipTable) {
            label(context, x + inset, line, tableLeft, left, palette::accent);
            label(context, x + inset + tableLeft, line, w - 2 * inset - tableLeft, right, palette::dim);
            line += 10;
        }
        // The game's lines in the game's colors; a line without a color is white, as in the game's tooltip.
        for (auto const& text : more) label(context, x + inset, line, w - 2 * inset, text.text, item ? palette::vanilla::textCode(text.color) : palette::faint), line += 10;
        context.flushText(0, std::nullopt);
        if (members) {
            std::vector<inspection::render::IconAt> icons;
            auto cellAt = [&](size_t i) { return std::pair{x + inset + static_cast<float>(i % perRow) * cellSize, line + 1 + static_cast<float>(i / perRow) * cellSize}; };
            // The one shown framed white; the one chosen green, over the white where they are the same (L-369).
            if (late.tipCurrent >= 0 && static_cast<size_t>(late.tipCurrent) < members) {
                auto const [cx, cy] = cellAt(static_cast<size_t>(late.tipCurrent));
                lamium::ui::frame(context, cx, cy, cellSize, cellSize, palette::white, .6f);
            }
            if (late.tipChosen >= 0 && static_cast<size_t>(late.tipChosen) < members) {
                auto const [cx, cy] = cellAt(static_cast<size_t>(late.tipChosen));
                lamium::ui::frame(context, cx, cy, cellSize, cellSize, palette::accent);
            }
            for (size_t i = 0; i < members; ++i)
                if (auto const* stack = schematic::items::iconStack(schematic::items::shown(late.tipItems[i]).icon)) {
                    auto const [cx, cy] = cellAt(i);
                    icons.push_back({stack, cx + 1, cy + 1});
                }
            if (!icons.empty()) inspection::render::drawItemIcons(context, icons, 17);
            context.flushText(0, std::nullopt);
        }
    }
    cost.at("tooltip");
    return true;
}
}
bool wheel(int pages) {
    std::scoped_lock lock(shownLock);
    if (!fresh()) return false;
    if (wantedHeld.load()) { queued.wantedTurns += pages; return true; }
    if (shown.viewOpen && (shown.view.has(shown.pointerX, shown.pointerY) || shown.window.has(shown.pointerX, shown.pointerY))) queued.viewTurns += pages;
    else if (shown.list.has(shown.pointerX, shown.pointerY)) queued.listTurns += pages;
    else if (shown.side.has(shown.pointerX, shown.pointerY)) queued.sideTurns += pages;
    else if (shown.favorites.has(shown.pointerX, shown.pointerY)) queued.favoriteTurns += pages;
    else return shown.viewOpen; // an open view lies over the game's window: its slots must not scroll
    return true;
}
bool press(int button, bool left, bool right, bool down) {
    std::scoped_lock lock(shownLock);
    bool const ours = fresh() && (shown.viewOpen || shown.list.has(shown.pointerX, shown.pointerY) || shown.field.has(shown.pointerX, shown.pointerY)
        || std::any_of(shown.bands.begin(), shown.bands.end(), [](ListBox const& b) { return b.has(shown.pointerX, shown.pointerY); }));
    if (!ours && down && fresh() && (shown.searchFocus || shown.countFocus)) queued.outside = true;
    if (!ours && down && left) gamePress = true;
    bool first = false;
    if (!keptButtons.decide(button, down, ours, &first)) return false;
    if (left) leftHeld = down;
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
    if (!down && code == wantedCode.load() && wantedHeld.exchange(false)) queued.wantedUp = true;
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
    wantedHeld = false;
    leftHeld = false;
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
void focusSearch() { focusAsked = true; }
void editLayout(band::Page page) {
    pendingEdit = page;
    editAsked = std::chrono::steady_clock::now();
    inventoryAsked = {};
}
void favorite() {
    if (bandEditor::active()) return;
    auto const prefs = Runtime::instance().snapshot();
    if (!prefs->inventory.recipes || !prefs->inventory.recipesFavorites) return;
    if (auto item = hovered(); !item.empty()) state::toggleFavorite(item);
}
void want() {
    if (bandEditor::active() || !Runtime::instance().snapshot()->inventory.plan) return;
    auto item = hovered();
    if (item.empty()) return;
    bool const was = state::wanted().has(item);
    // The band shows the wanted items then, so what went in or out is seen.
    bandPages::showWanted();
    if (wantedCode.load() <= 0) { state::toggleWanted(item); return; } // not a single key: no hold to wait for
    // In at once; an item that was in stays until the key is released, so the wheel can change its count.
    if (!was) state::toggleWanted(item);
    wantedItem = std::move(item);
    wantedLeaves = was;
    wantedHeld = true;
}
void show(bool uses) {
    if (bandEditor::active()) return;
    std::string item;
    {
        std::scoped_lock lock(shownLock);
        if (!fresh() || shown.hoverItem.empty()) return;
        item = shown.hoverItem;
    }
    view::open(std::move(item), uses);
    viewScreen = lastScreen;
}
// The game's tooltip waits for the list while it shows beside the window, so it is drawn over the bands; the
// anvil's sides are the enchantment order's, which paints under the tooltip itself.
bool holdsTooltip() {
    std::scoped_lock lock(shownLock);
    return fresh() && lastScreen != "anvil_screen";
}
void start() {
    inspection::setDeferTooltip(holdsTooltip);
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
    inspection::setDeferTooltip(nullptr);
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    searchFocus = false;
    countEdit::cancel();
    releaseKeyboard();
    forget = true;
}
}
