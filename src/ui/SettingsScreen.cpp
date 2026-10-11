#include "ui/SettingsScreen.h"
#include "settings/Options.h"
#include "ui/SettingsRows.h"
#include "ui/SettingsNavigation.h"
#include "ui/SettingsTable.h"
#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "ui/ShapesView.h"
#include "ui/WaypointsView.h"
#include "ui/SchematicsView.h"
#include "ui/SettingsTableView.h"
#include "ui/ScreenParts.h"
#include "ui/ListViewWidgets.h"
#include "ui/WaypointPromptLayout.h"
#include "ui/SavePromptLayout.h"
#include "ui/RadialLayout.h"
#include "ui/Animations.h"
#include "ui/Toast.h"
#include "features/map/WaypointSession.h"
#include "app/SessionIds.h"
#include "features/schematic/SchematicSession.h"
#include "features/schematic/Preview.h"
#include "features/schematic/GhostRenderer.h"
#include "ui/SchematicFiles.h"
#include "features/schematic/MaterialAmount.h"
#include "app/Desktop.h"
#include "features/information/SchematicTarget.h"
#include "features/schematic/SchematicItems.h"
#include "features/schematic/Selection.h"
#include "features/schematic/SchematicActions.h"
#include "features/schematic/MenuModel.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/Tag.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "features/map/MapStore.h"
#include "features/map/WorldMap.h"
#include "ui/SearchQuery.h"
#include "ui/NumberInput.h"
#include "ui/Widgets.h"
#include "overlay/ShapeSession.h"
#include "ui/Localization.h"
#include "app/Runtime.h"
#include "app/Desktop.h"
#include "app/Versions.h"
#include "features/camera/CameraSessions.h"
#include "features/inspection/render/ItemIcon.h"
#include "features/information/InfoHud.h"
#include "features/information/HungerTrace.h"
#include "features/information/SaturationHud.h"
#include "features/information/InfoLines.h"
#include "ui/HudEditor.h"
#include <chrono>
#include "mc/client/gui/controls/VisualTree.h"
#include "input/Actions.h"
#include "input/BindingCapture.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/input/KeyboardManager.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/world/phys/HitResult.h"
#include <array>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace lamium::ui {
namespace {
using Zone = SettingsTable::Zone;
using Column = SettingsTable::Column;
std::recursive_mutex mutex;
IClientInstance* client = nullptr;
std::shared_ptr<AbstractScene> scene;
std::shared_ptr<AbstractScene> retired; // Popped scene, released next frame.
bool seen = false;
std::chrono::steady_clock::time_point openedAt;
bool closing = false;
std::string error;

// Navigation items (SettingsNavigation.h).
constexpr int navCount = nav::count, hotkeysNav = nav::hotkeys, shapesNav = nav::shapes, waypointsNav = nav::waypoints,
    schematicsNav = nav::schematics, worldMapNav = nav::worldMap, hudNav = nav::hud;
// Leaving the HUD layout editor returns to editorReturn.
int editorReturn = 0;
bool pendingRelease = false;
SettingsNavigation navigation;
float displayedInverseScale = 0;
// Where the pointer was at the last frame, in GUI units. Wheel events carry
// no position, so the pane under the pointer is found from this.
glm::vec2 lastPointer{-1, -1};
// A list scrollbar being dragged: the list's first row and its layout.
int* scrollDragFirst = nullptr;
ShapesLayout const* scrollDragLayout = nullptr;
// A press on a list's scrollbar moves the list there and starts a drag.
bool pressScrollbar(ShapesLayout const& l, int& first, float x, float y) {
    if (!l.onScrollbar(x, y)) return false;
    first = l.firstAt(y);
    scrollDragFirst = &first;
    scrollDragLayout = &l;
    return true;
}
// GUI coordinates; resolved against the layout drawn in the next frame.
struct Click { float x, y; bool right; };
std::optional<Click> pendingClick;
std::vector<int> pendingKeys;
bool pendingSearch = false; // Ctrl+F, applied with the next frame.
NumberInput numberInput;
bool numberDirty = false;

using ShapeZone = ShapesLayout::Zone; // the list-and-detail layout's zones (Schematics view)
bool numericEditing() {
    return table_view::editingNumber() || shapes_view::editingNumber() || waypoints_view::editingNumber() || schematics_view::editingNumber();
}
// Waypoint add prompt (L-60 step 5): replaces the whole panel while open.
struct WaypointPrompt { map::Waypoint draft; SearchQuery name; };
std::optional<WaypointPrompt> prompt;
// Schematic save prompt (L-93): replaces the whole panel while open, like the
// waypoint prompt. The steppers move the selected area's corners.
struct SavePrompt {
    schematic::Area area;
    int dimension = 0;
    SearchQuery name;
    bool entities = false;
    bool overwrite = false; // armed after "exists"; the next Save replaces the file
    std::string problem;
};
std::optional<SavePrompt> savePrompt;
// The schematic menu (L-93): a ring of categories, then of their items, over
// the world. Replaces the whole panel while open, like the prompts.
struct SchematicMenu {
    int category = -1, hover = -1;
    std::chrono::steady_clock::time_point shown = std::chrono::steady_clock::now(); // when this level appeared
    void show(int level) { category = level; hover = -1; shown = std::chrono::steady_clock::now(); }
};
std::optional<SchematicMenu> schematicMenu;
int schematicMenuClosedAt = -1; // the level shown when it last closed
// World map (L-60): replaces the whole panel; the add prompt opened from it
// returns to it.
bool worldMapOpen = false, promptOnMap = false;
bool mapFromSettings = false;  // Closing the map returns to the settings.
void enterWorldMap(bool fromSettings, bool resume);
struct Wheel { float x, y; int direction; };
std::vector<Wheel> pendingWheels;

bool textHook = false;
bool textKeyboardOwned = false;
bool textKeyboardNumber = false;
input::Chord uiHeld;

bool shapesView() { return navigation.current == shapesNav; }
bool waypointsView() { return navigation.current == waypointsNav; }
bool schematicsView() { return navigation.current == schematicsNav; }
bool hudEditorView() { return navigation.current == hudNav; }
void selectNav(int index, bool temporary = false) {
    // Choosing a category ends a search, which otherwise spans every category.
    table_view::endSearch();
    if (navigation.current == shapesNav && index != shapesNav) shapes_view::leave();
    index = std::clamp(index, 0, navCount - 1);
    waypoints_view::setFromMap(false);
    if (index == worldMapNav) { enterWorldMap(true, false); return; }
    if (index == hudNav && navigation.current != hudNav) {
        editorReturn = navigation.current;
        hud_editor::reset();
        pendingRelease = false;
    }
    if (index == schematicsNav && navigation.current != schematicsNav) schematics_view::refresh(true);
    navigation.select(index, temporary);
    table_view::enterCategory();
}
bool heldCtrl() {
    for (int key : {0x11, 0xa2, 0xa3})
        if (std::find(uiHeld.begin(), uiHeld.end(), input::Token{input::Device::Key, key}) != uiHeld.end()) return true;
    return false;
}
bool heldShift() {
    for (int key : {0x10, 0xa0, 0xa1})
        if (std::find(uiHeld.begin(), uiHeld.end(), input::Token{input::Device::Key, key}) != uiHeld.end()) return true;
    return false;
}
void observeHeld(input::Token token, bool down) {
    if (token.device == input::Device::Wheel) return;
    if (!down) std::erase(uiHeld, token);
    else if (std::find(uiHeld.begin(), uiHeld.end(), token) == uiHeld.end()) uiHeld.push_back(token);
}
std::array<ll::event::ListenerPtr, 5> listeners;
bool backgroundHook = false;
bool renderHook = false;
bool exitHook = false;
bool entranceHook = false;
thread_local ScreenView* settingsRenderView = nullptr;

bool ownsTop() {
    return client && scene && client->getSceneFactory().getCurrentSceneStack()->getTopScene() == scene.get();
}
void releaseTextKeyboard() {
    if (!textKeyboardOwned) return;
    textKeyboardOwned = false;
    if (client) {
        auto& keyboard = client->getKeyboardManager();
        keyboard.disableKeyboard();
        keyboard.releaseKeyboardOwnership();
    }
}
void syncTextKeyboard(float x, float y) {
    bool wanted = !closing && !table_view::capturing() && (table_view::searchFocused() || numericEditing() || shapes_view::editingName() || waypoints_view::editingName() || prompt
        || savePrompt
        || (worldMapOpen && map::world::editingName()));
    bool number = numericEditing();
    if (textKeyboardOwned && (!wanted || number != textKeyboardNumber)) releaseTextKeyboard();
    if (!wanted || textKeyboardOwned || !client) return;
    auto& keyboard = client->getKeyboardManager();
    if (!keyboard.tryClaimKeyboardOwnership()) return;
    // Drawing a caret alone does not enable the platform's UTF-8/IME path.
    // Lamium owns text and selection; this keyboard supplies insertion events.
    // Seeding its independent edit buffer with our current value leaves stale
    // suffixes when Lamium handles select-all/backspace without native editing.
    bool enabled = keyboard.tryEnableKeyboard({}, number ? 24 : 128, true, false, false, Vec2{x, y}, 20.0f);
    if (!enabled) {
        keyboard.releaseKeyboardOwnership();
        return;
    }
    textKeyboardOwned = true;
    textKeyboardNumber = number;
}
LL_TYPE_INSTANCE_HOOK(SettingsSceneRender, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$render, void, ScreenContext& context, FrameRenderObject const& object) {
    // Bedrock scene objects do not carry C++ RTTI. Identify the scene through
    // the actual UIScene call, and scope the view to this render invocation.
    // Restoring the previous value also handles nested rendering and exceptions.
    struct RestoreView {
        ScreenView* previous;
        ~RestoreView() { settingsRenderView = previous; }
    } restore{settingsRenderView};
    {
        std::lock_guard lock(mutex);
        settingsRenderView = scene.get() == this ? mScreenView.get() : nullptr;
    }
    origin(context, object);
}
LL_TYPE_INSTANCE_HOOK(SettingsWorldBackground, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$renderGameBehind, bool) {
    std::lock_guard lock(mutex);
    if (scene.get() == this) return true;
    return origin();
}
void clear();
LL_TYPE_INSTANCE_HOOK(SettingsSceneExit, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$onScreenExit, void, bool isPopping, bool transitions, std::shared_ptr<AbstractScene> next) {
    bool owned;
    {
        std::lock_guard lock(mutex);
        owned = scene.get() == this;
    }
    // The native dialog is only our focus owner; its visual exit animation is
    // not rendered. Let it finish exiting without waiting for that animation.
    origin(isPopping, owned ? false : transitions, std::move(next));
    // Popped by anything (Esc, a dimension change, a disconnect): forget it,
    // even if it never rendered, so hotkeys do not stay blocked.
    if (owned && isPopping) {
        std::lock_guard lock(mutex);
        // Keep our reference until the next frame: the stack may still be
        // using this scene after the callback returns.
        if (scene.get() == this) { retired = scene; clear(); }
    }
}
LL_TYPE_INSTANCE_HOOK(SettingsSceneEntrance, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$onScreenEntrance, void, bool revisiting, bool transitions) {
    bool owned;
    {
        std::lock_guard lock(mutex);
        owned = scene.get() == this;
    }
    origin(revisiting, owned ? false : transitions);
}
// The first native text events carrying control characters, for checking
// how an IME rewrites its composition (bounded).
void logControlText(std::string const& text) {
    static int logged = 0;
    if (logged >= 12 || std::none_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 32; })) return;
    ++logged;
    std::string shown;
    for (unsigned char c : text) shown += c < 32 ? std::format("\\x{:02x}", c) : std::string(1, static_cast<char>(c));
    try { Runtime::instance().self().getLogger().info("Text input with control characters: {}", shown); } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(SettingsSearchText, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$handleTextChar, void, std::string const& text, FocusImpact impact) {
    std::lock_guard lock(mutex);
    if (scene.get() == this && ownsTop()) {
        // Coalesce native text events before persisting the whole workspace.
        // Never flush the world sidecar from inside a text callback.
        logControlText(text);
        if (prompt) { prompt->name.type(text); return; }
        if (savePrompt) { if (savePrompt->name.type(text)) savePrompt->overwrite = false; return; }
        if (worldMapOpen && map::world::editingName()) { map::world::typeText(text); return; }
        if (waypoints_view::editingName()) { waypoints_view::type(text); return; }
        if (shapes_view::editingName()) { shapes_view::type(text); return; }
        if (numericEditing()) { if (numberInput.append(text)) numberDirty = true; return; }
        table_view::typeSearch(text);
        return;
    }
    origin(text, impact);
}
void clear() {
    releaseTextKeyboard();
    numberDirty = false; uiHeld.clear(); client = nullptr;
    table_view::reset();
    scene.reset(); seen = false; closing = false; pendingClick.reset(); pendingKeys.clear(); pendingSearch = false;
    // A draft is never kept once the screen is gone.
    shapes_view::reset();
    prompt.reset();
    savePrompt.reset();
    if (schematicMenu) schematicMenuClosedAt = schematicMenu->category;
    schematicMenu.reset();
    if (worldMapOpen) map::world::close();
    worldMapOpen = false; promptOnMap = false; pendingWheels.clear();
    mapFromSettings = false;
    waypoints_view::reset();
    schematics_view::reset();
}
// L-81: an out-of-range warning belongs to where it was raised. It goes once
// the user moves to another tab, row, shape or shape field; other messages
// that replaced it stay.
struct WarningPlace {
    int nav, row, shapeField;
    std::optional<overlay::ShapeId> shape;
    bool operator==(WarningPlace const&) const = default;
};
std::optional<std::pair<WarningPlace, std::string>> rangeWarning;
WarningPlace warningPlace() { return {navigation.current, table_view::selectedRow(), shapes_view::fieldSelected(), shapes_view::selected()}; }
void warnRange(std::string text) {
    error = std::move(text);
    rangeWarning = {warningPlace(), error};
}
void dropMovedWarning() {
    if (!rangeWarning) return;
    if (error != rangeWarning->second) rangeWarning.reset();
    else if (warningPlace() != rangeWarning->first) { error.clear(); rangeWarning.reset(); }
}
void applyNumber() {
    if (!numericEditing() || !numberDirty) return;
    numberDirty = false;
    if (waypoints_view::editingNumber()) { waypoints_view::applyNumber(); return; }
    if (schematics_view::editingNumber()) { schematics_view::applyNumber(); return; }
    if (shapes_view::editingNumber()) { shapes_view::applyNumber(); return; }
    table_view::applyNumber();
}
void finishNumber() {
    applyNumber();
    shapes_view::applyName();
    waypoints_view::applyName();
    releaseTextKeyboard();
    numberDirty = false;
    table_view::endEditing();
    shapes_view::endEditing();
    waypoints_view::endEditing();
    schematics_view::endEditing();
}
void close() {
    releaseTextKeyboard();
    table_view::disarm();
    if (ownsTop()) {
        if (!closing) client->getSceneFactory().getCurrentSceneStack()->schedulePopScreen(1);
        closing = true;
    } else clear();
}

// ---- Where the player stands ----
int playerDimension() {
    auto* player = client ? client->getLocalPlayer() : nullptr;
    return player ? static_cast<int>(player->getDimensionId()) : 0;
}
using screen::Place;
std::optional<Place> standingPlace() {
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return std::nullopt;
    auto feet = player->getFeetPos();
    if (!std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z)) return std::nullopt;
    return Place{static_cast<int>(std::floor(feet.x)), static_cast<int>(std::floor(feet.y)),
                 static_cast<int>(std::floor(feet.z)), playerDimension()};
}
std::string dimensionName(int dimension) {
    return translated(dimension == 1 ? "dimension.nether" : dimension == 2 ? "dimension.end" : "dimension.overworld");
}
int distanceTo(int x, int z) {
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return 0;
    auto feet = player->getFeetPos();
    return static_cast<int>(std::lround(std::hypot(x + .5 - feet.x, z + .5 - feet.z)));
}

// ---- Waypoint add prompt ----
// From the world map the prompt returns to it; otherwise the screen closes.
void endPrompt() {
    if (!promptOnMap) { close(); return; }
    prompt.reset();
    promptOnMap = false;
    releaseTextKeyboard();
}
void commitPrompt() {
    if (!prompt) return;
    auto waypoint = prompt->draft;
    auto const& typed = prompt->name.value();
    if (typed.find_first_not_of(' ') != std::string::npos) waypoint.name = typed;
    bool saved = map::waypoints::add(waypoint);
    showMessageToast(saved ? translated("waypoint.added", waypoint.name) : translated("waypoint.saveError"));
    endPrompt();
}
void handlePromptKey(int key) {
    if (!prompt) return;
    switch (key) {
    case 0x0d: commitPrompt(); break;
    case 0x1b: endPrompt(); break;
    case 0x09: {
        int step = heldShift() ? -1 : 1;
        int count = static_cast<int>(map::waypointColors.size());
        prompt->draft.color = (map::clampColor(prompt->draft.color) + step + count) % count;
        break;
    }
    }
}
void handlePromptClick(float x, float y, glm::vec2 size) {
    if (!prompt) return;
    auto hit = WaypointPromptLayout::at(size.x, size.y).hit(x, y);
    using Part = WaypointPromptLayout::Part;
    if (hit.part == Part::Swatch) prompt->draft.color = hit.swatch;
    else if (hit.part == Part::Add) commitPrompt();
    else if (hit.part == Part::Cancel) endPrompt();
}
void renderPrompt(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto l = WaypointPromptLayout::at(size.x, size.y);
    using L = WaypointPromptLayout;
    panel(context, l.left, l.top, L::width, L::height, .86f);
    frame(context, l.left, l.top, L::width, L::height, palette::white, .14f);
    float x = l.left + L::pad;
    label(context, x, l.titleY(), l.inner(), translated("waypoint.add"));
    auto const& d = prompt->draft;
    std::string_view dimension = d.dimension == 1 ? "dimension.nether" : d.dimension == 2 ? "dimension.end" : "dimension.overworld";
    label(context, x, l.whereY(), l.inner(), std::format("{}, {}, {}  {}", d.x, d.y, d.z, translated(dimension)), palette::dim);
    fill(context, x, l.fieldY(), l.inner(), L::fieldHeight, Rgb{0, 0, 0}, .4f);
    frame(context, x, l.fieldY(), l.inner(), L::fieldHeight, palette::accent);
    drawEditText(context, x + 3, l.fieldY(), L::fieldHeight, l.inner() - 6, prompt->name);
    for (int i = 0; i < L::swatches; ++i) {
        auto c = map::waypointColors[static_cast<size_t>(i)];
        float sx = l.swatchX(i), sy = l.swatchY();
        if (i == map::clampColor(d.color)) frame(context, sx - 2, sy - 2, L::swatch + 4, L::swatch + 4, palette::white);
        fill(context, sx, sy, L::swatch, L::swatch, Rgb{map::channel(c, 0) / 255.f, map::channel(c, 1) / 255.f, map::channel(c, 2) / 255.f});
        frame(context, sx, sy, L::swatch, L::swatch, Rgb{0, 0, 0}, .6f);
    }
    auto hover = l.hit(pointer.x, pointer.y).part;
    auto button = [&](float bx, std::string text, bool primary, bool over) {
        fill(context, bx, l.buttonY(), L::buttonWidth, L::buttonHeight,
             primary ? palette::accentDeep : over ? Rgb{.23f, .23f, .24f} : palette::keyFill);
        frame(context, bx, l.buttonY(), L::buttonWidth, L::buttonHeight, primary ? palette::accent : palette::keyEdge);
        label(context, bx, l.buttonY() + boxTextInset(), L::buttonWidth, std::move(text), palette::text, Align::Center);
    };
    button(l.addX(), translated("waypoint.addButton"), true, hover == WaypointPromptLayout::Part::Add);
    button(l.cancelX(), translated("waypoint.cancelButton"), false, hover == WaypointPromptLayout::Part::Cancel);
    label(context, x, l.hintY(), l.inner(), translated("waypoint.hint"), palette::faint);
    context.flushText(0, std::nullopt);
}
// ---- Schematic menu ----
// The ring is sized from the widest name and the widest center line of
// every level, measured when drawn, so it keeps its size and place while
// moving between levels. Small, everything is drawn at this scale.
constexpr float menuSmallScale = .8f;
RadialLayout::Sizes menuSizes{96, 26, 130, 64};
RadialLayout menuLayout(glm::vec2 size) {
    int count = schematicMenu && schematicMenu->category >= 0
        ? static_cast<int>(schematic::menu::categories[static_cast<size_t>(schematicMenu->category)].items.size())
        : static_cast<int>(schematic::menu::categories.size());
    return RadialLayout::at(size.x, size.y, count, menuSizes, Runtime::instance().preferences().schematic.menuSmall);
}
bool isKeyOf(input::Action action, int key) {
    auto chord = input::effectiveChord(Runtime::instance().preferences().bindings, action);
    return chord.size() == 1 && chord[0].device == input::Device::Key && chord[0].code == key;
}
// Leaves the menu for a view of the same screen (a tab, the save prompt, keys).
void leaveMenu() {
    if (schematicMenu) schematicMenuClosedAt = schematicMenu->category;
    schematicMenu.reset();
}
void openSchematicTab(schematics_view::Tab tab) {
    leaveMenu();
    selectNav(schematicsNav, true);
    schematics_view::refresh(true);
    schematics_view::show(tab);
}
void runMenuItem(schematic::menu::Item const& item, int amount) {
    namespace menu = schematic::menu;
    using C = menu::Command;
    if (!client) return;
    if (item.stepper()) { schematic::actions::step(*client, std::get<menu::Stepper>(item.what), amount ? amount : 1); return; }
    if (amount) return; // The wheel only changes steppers.
    auto command = std::get<menu::Command>(item.what);
    menu::Target target{};
    if (menu::choosesTarget(command, target)) {
        schematic::actions::setTarget(target);
        schematicMenu->show(menu::moveCategory);
        return;
    }
    auto toggle = [](char const* id, char const* name) {
        table_view::adjustOption(id, 1);
        bool on = id == std::string_view("schematic.hud") ? Runtime::instance().preferences().schematic.hud
            : Runtime::instance().preferences().schematic.enabled;
        showMessageToast(translated(name) + ": " + translated(on ? "on" : "off"));
    };
    switch (command) {
    case C::ToggleHud: toggle("schematic.hud", "schematic.menu.toggleHud"); return;
    case C::ToggleFeature: toggle("schematic.enabled", "schematic.menu.toggleFeature"); return;
    case C::SaveArea: {
        auto state = schematic::selection::current();
        auto area = state.area();
        if (!area) { showMessageToast(translated("schematic.toast.noArea")); return; }
        leaveMenu();
        savePrompt = SavePrompt{*area, state.dimension, {}, false, false, {}};
        savePrompt->name.append("schematic");
        savePrompt->name.selectAll();
        return;
    }
    case C::PlaceFile: case C::FilesTab: openSchematicTab(schematics_view::Tab::Files); return;
    case C::DeletePlacement: case C::PlacedTab: openSchematicTab(schematics_view::Tab::Placements); return;
    case C::CheckTab: openSchematicTab(schematics_view::Tab::Verify); return;
    case C::MaterialsTab: openSchematicTab(schematics_view::Tab::Materials); return;
    case C::KeySettings: leaveMenu(); screen::showFeatureKeys("schematic"); return;
    case C::NearestMistake:
        // Its marker is in the world: close so it can be followed.
        schematic::actions::run(*client, command);
        close();
        return;
    default: schematic::actions::run(*client, command); return;
    }
}
void handleMenuClick(bool right) {
    if (!schematicMenu) return;
    if (right) {
        if (schematicMenu->category >= 0) schematicMenu->show(-1);
        else close();
        return;
    }
    int hover = schematicMenu->hover;
    if (hover < 0) return;
    if (schematicMenu->category < 0) { schematicMenu->show(hover); return; }
    auto items = schematic::menu::categories[static_cast<size_t>(schematicMenu->category)].items;
    if (hover < static_cast<int>(items.size())) runMenuItem(items[static_cast<size_t>(hover)], 0);
}
void handleMenuWheel(int direction) {
    if (!schematicMenu || schematicMenu->category < 0 || schematicMenu->hover < 0) return;
    auto items = schematic::menu::categories[static_cast<size_t>(schematicMenu->category)].items;
    if (schematicMenu->hover < static_cast<int>(items.size())) runMenuItem(items[static_cast<size_t>(schematicMenu->hover)], direction);
}
void handleMenuKey(int key) {
    if (key == 0x1b || isKeyOf(input::Action::SchematicMenu, key)) close();
}
// What an item shows under its name: a stepper's value or a switch's state.
std::string menuValue(schematic::menu::Item const& item) {
    namespace menu = schematic::menu;
    if (item.stepper()) return schematic::actions::value(std::get<menu::Stepper>(item.what));
    auto const preferences = Runtime::instance().preferences();
    auto set = schematic::session::current();
    auto const* p = set.selected >= 0 && set.selected < static_cast<int>(set.placements.size())
        ? &set.placements[static_cast<size_t>(set.selected)] : nullptr;
    switch (std::get<menu::Command>(item.what)) {
    case menu::Command::ToggleHud: return translated(preferences.schematic.hud ? "on" : "off");
    case menu::Command::ToggleFeature: return translated(preferences.schematic.enabled ? "on" : "off");
    case menu::Command::ToggleShown: return p ? translated(p->visible ? "on" : "off") : std::string{};
    case menu::Command::ToggleEntities: return p ? translated(p->entities ? "on" : "off") : std::string{};
    case menu::Command::ToggleExtras: return p ? translated(p->countExtras ? "schematic.extras.show" : "schematic.extras.ignore") : std::string{};
    default: return {};
    }
}
// Drawn with the same parts as Lamium's screens: panel items with the
// selection's green frame, standard labels, a center panel with a rule
// under its title.
void renderSchematicMenu(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    namespace menu = schematic::menu;
    auto const preferences = Runtime::instance().preferences();
    static constexpr std::array<float, 3> dims{0.f, .12f, .35f};
    float dim = dims[static_cast<size_t>(std::clamp(preferences.schematic.menuBackground, 0, 2))];
    if (dim > 0) fill(context, 0, 0, size.x, size.y, Rgb{0, 0, 0}, dim);
    float s = preferences.schematic.menuSmall ? menuSmallScale : 1.f;
    auto width = [&](std::string const& text) { return textWidthScaled(context, text, s); };
    static constexpr std::array<char const*, 4> targets{"schematic.target.placement", "schematic.target.corner1",
        "schematic.target.corner2", "schematic.target.area"};
    static constexpr std::array<char const*, 6> hints{"schematic.menu.hintList", "schematic.menu.hintClose", "schematic.menu.hintStep",
        "schematic.menu.hintStepClick", "schematic.menu.hintRun", "schematic.menu.hintBack"};
    // Sizes for every level at once, so nothing moves between levels.
    float widest = 72 * s, centerWide = 100 * s;
    for (auto const& category : menu::categories) {
        widest = std::max(widest, width(translated(category.label)) + 16 * s);
        centerWide = std::max(centerWide, width(translated(category.label)) + 16 * s);
        for (auto const& item : category.items) widest = std::max(widest, width(translated(item.label)) + 16 * s);
    }
    for (auto const* key : hints) centerWide = std::max(centerWide, width(translated(key)) + 16 * s);
    for (auto const* key : targets) centerWide = std::max(centerWide, width(translated("schematic.menu.target", translated(key))) + 16 * s);
    float line = 11 * s, title = 16 * s;
    menuSizes = {widest, RadialLayout::itemHeight(s), centerWide, title + 3 * s + 4 * line + 3 * s};
    auto l = menuLayout(size);
    int hover = l.hit(pointer.x, pointer.y);
    bool list = schematicMenu->category < 0;
    // Lamium's Animations setting decides whether the ring spreads out.
    float opened = client && !animationsOn(*client) ? 1.f
        : RadialLayout::spread(std::chrono::duration<float>(std::chrono::steady_clock::now() - schematicMenu->shown).count());
    auto const& category = list ? menu::categories[0] : menu::categories[static_cast<size_t>(schematicMenu->category)];
    menu::Item const* hovered = nullptr;
    float textInset = boxTextInset() * s;
    for (int i = 0; i < l.count; ++i) {
        std::string name, value;
        if (list) name = translated(menu::categories[static_cast<size_t>(i)].label);
        else {
            auto const& item = category.items[static_cast<size_t>(i)];
            name = translated(item.label);
            value = menuValue(item);
            if (i == hover) hovered = &item;
        }
        float h = value.empty() ? 16 * s : menuSizes.itemHeight;
        float x = std::round(l.itemX(i, opened) - widest / 2), y = std::round(l.itemY(i, opened) - h / 2);
        panel(context, x, y, widest, h, .86f * opened);
        frame(context, x, y, widest, h, palette::white, .14f * opened);
        rowBackground(context, x, y, widest, h, i == hover && opened >= 1, false);
        // Text once the plates are mostly in: it cannot fade with them.
        if (opened < .4f) continue;
        labelScaled(context, x + 4, y + 3 * s + textInset, widest - 8, name, s, palette::text, Align::Center);
        if (!value.empty()) labelScaled(context, x + 4, y + 14 * s + textInset, widest - 8, value, s, palette::dim, Align::Center);
    }
    // The center: where the menu is, what Move moves, and what each button does.
    std::vector<std::pair<std::string, Rgb>> lines;
    if (!list && schematicMenu->category == menu::moveCategory) {
        static constexpr std::array<Rgb, 4> colors{palette::accent, Rgb{1.f, .4f, .35f}, Rgb{.45f, .65f, 1.f}, palette::white};
        auto t = static_cast<size_t>(schematic::actions::target());
        lines.push_back({translated("schematic.menu.target", translated(targets[t])), colors[t]});
    }
    auto hint = [&](char const* key) { lines.push_back({translated(key), palette::faint}); };
    if (list) { hint("schematic.menu.hintList"); hint("schematic.menu.hintClose"); }
    else {
        if (hovered && hovered->stepper()) { hint("schematic.menu.hintStep"); hint("schematic.menu.hintStepClick"); }
        else if (hovered) hint("schematic.menu.hintRun");
        hint("schematic.menu.hintBack");
    }
    float cw = menuSizes.centerWidth, ch = menuSizes.centerHeight;
    float cx = std::round(l.cx - cw / 2), cy = std::round(l.cy - ch / 2);
    panel(context, cx, cy, cw, ch, .9f);
    frame(context, cx, cy, cw, ch, palette::white, .14f);
    labelScaled(context, cx + 4, cy + 4 * s + textInset, cw - 8, translated(list ? "schematic.menu.title" : category.label), s,
        palette::text, Align::Center);
    fill(context, cx + 1, cy + title, cw - 2, 1, palette::white, .14f);
    float y = cy + title + 3 * s;
    for (auto const& [text, color] : lines) {
        labelScaled(context, cx + 4, y + textInset, cw - 8, text, s, color, Align::Center);
        y += line;
    }
    float below = std::round(l.cy + l.ry + menuSizes.itemHeight / 2 + 4), above = std::round(l.cy - l.ry - menuSizes.itemHeight / 2 - 4);
    auto note = [&](std::string const& text, Rgb color, bool card, bool top) {
        std::vector<std::string> parts;
        for (size_t start = 0;;) {
            auto end = text.find('\n', start);
            parts.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        float tw = 0;
        for (auto const& part : parts) tw = std::max(tw, width(part) + 10);
        tw = std::min(tw, size.x - 8);
        float th = line * static_cast<float>(parts.size()) + 2;
        float tx = std::round(std::clamp(l.cx - tw / 2, 4.f, size.x - tw - 4));
        float ty = top ? above - th : below;
        if (!top && ty + th > size.y) ty = above - th;
        if (card) panel(context, tx, ty, tw, th, .86f);
        for (size_t i = 0; i < parts.size(); ++i)
            labelScaled(context, tx + 5, ty + 1 + textInset + line * static_cast<float>(i), tw - 10, parts[i], s, color, Align::Center);
    };
    // The game's toasts are not drawn over this screen: what an item just
    // did (or why it did nothing, such as no area yet) shows under the ring.
    if (auto toast = currentToggleToast(toastNow()))
        note(toast->plain ? toast->text : toast->text + ": " + translated(toast->on ? "on" : "off"), palette::text, true, false);
    // A quiet pointer to the adjust key while it is unbound.
    if (hovered && hovered->stepper() && schematic::menu::repeatable(std::get<menu::Stepper>(hovered->what))
        && input::effectiveChord(preferences.bindings, input::Action::AdjustSchematic).empty())
        note(translated("schematic.menu.hintAdjust"), palette::faint, false, true);
    context.flushText(0, std::nullopt);
}
// ---- Schematic save prompt ----
void commitSave() {
    if (!savePrompt) return;
    auto& p = *savePrompt;
    // While a save runs, the button stops it.
    if (schematic::ghosts::saveStatus()) { schematic::ghosts::stopSaving(); return; }
    auto file = schematic::schematicFileName(p.name.value());
    if (file.empty()) { p.problem = translated("schematic.save.noName"); return; }
    if (p.area.cells() > schematic::maxCells) { p.problem = translated("schematic.save.tooLarge", p.area.cells(), schematic::maxCells); return; }
    auto path = schematic::session::folder() / std::filesystem::u8path(file);
    std::error_code code;
    if (std::filesystem::exists(path, code) && !p.overwrite) {
        p.overwrite = true;
        p.problem = translated("schematic.save.exists", file);
        return;
    }
    if (!schematic::ghosts::save({p.area, p.dimension, p.entities, path, file})) { p.problem = translated("schematic.save.busy"); return; }
    // The area stays chosen, so it can be saved again after more building.
    showMessageToast(translated("schematic.save.started", file));
    close();
}
void handleSavePromptKey(int key) {
    if (!savePrompt) return;
    if (key == 0x0d) commitSave();
    else if (key == 0x1b) close();
}
void handleSavePromptClick(float x, float y, glm::vec2 size) {
    if (!savePrompt) return;
    auto& p = *savePrompt;
    auto hit = SavePromptLayout::at(size.x, size.y).hit(x, y);
    using Part = SavePromptLayout::Part;
    switch (hit.part) {
    case Part::Minus: case Part::Plus: {
        int step = (hit.part == Part::Plus ? 1 : -1) * (heldShift() ? 10 : 1);
        auto& corner = hit.corner == 0 ? p.area.a : p.area.b;
        (hit.axis == 0 ? corner.x : hit.axis == 1 ? corner.y : corner.z) += step;
        // The frame in the world follows at once.
        schematic::selection::setArea(p.area, p.dimension);
        p.problem.clear();
        break;
    }
    case Part::Entities: p.entities = !p.entities; break;
    case Part::Save: commitSave(); break;
    case Part::Cancel: close(); break;
    case Part::Clear:
        schematic::selection::clear();
        close();
        break;
    default: break;
    }
}
void renderSavePrompt(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto& p = *savePrompt;
    auto l = SavePromptLayout::at(size.x, size.y);
    using L = SavePromptLayout;
    panel(context, l.left, l.top, L::width, L::height(), .86f);
    frame(context, l.left, l.top, L::width, L::height(), palette::white, .14f);
    float x = l.left + L::pad;
    auto hover = l.hit(pointer.x, pointer.y);
    label(context, x, l.titleY(), l.inner(), translated("schematic.save.title"));
    for (int corner = 0; corner < 2; ++corner) {
        float y = l.cornerY(corner);
        auto const& c = corner == 0 ? p.area.a : p.area.b;
        // In the colors of the corner blocks in the world.
        label(context, x, y + boxTextInset(), L::labelWidth - 4, translated(corner == 0 ? "schematic.save.corner1" : "schematic.save.corner2"),
            corner == 0 ? Rgb{1.f, .4f, .35f} : Rgb{.45f, .65f, 1.f});
        for (int axis = 0; axis < 3; ++axis) {
            float cx = l.cellX(axis), w = l.cellWidth();
            int value = axis == 0 ? c.x : axis == 1 ? c.y : c.z;
            bool overMinus = hover.part == L::Part::Minus && hover.corner == corner && hover.axis == axis;
            bool overPlus = hover.part == L::Part::Plus && hover.corner == corner && hover.axis == axis;
            fill(context, cx, y, w, L::rowHeight, Rgb{0, 0, 0}, .3f);
            drawSmallButton(context, cx, y, L::step, L::rowHeight, "-", overMinus);
            drawSmallButton(context, cx + w - L::step, y, L::step, L::rowHeight, "+", overPlus);
            static constexpr std::array<char const*, 3> axes{"X", "Y", "Z"};
            label(context, cx + L::step, y + boxTextInset(), w - 2 * L::step, std::format("{} {}", axes[static_cast<size_t>(axis)], value),
                palette::text, Align::Center);
        }
    }
    auto s = p.area.size();
    label(context, x, l.sizeY(), l.inner(), translated("schematic.save.size", s.x, s.y, s.z, p.area.cells()), palette::dim);
    fill(context, x, l.fieldY(), l.inner(), L::fieldHeight, Rgb{0, 0, 0}, .4f);
    frame(context, x, l.fieldY(), l.inner(), L::fieldHeight, palette::accent);
    drawEditText(context, x + 3, l.fieldY(), L::fieldHeight, l.inner() - 6, p.name);
    auto file = schematic::schematicFileName(p.name.value());
    label(context, x, l.whereY(), l.inner(), translated("schematic.save.where", file.empty() ? std::string("-") : file), palette::faint);
    label(context, x, l.entitiesY() + boxTextInset(), l.inner() - L::switchWidth - 4, translated("schematic.save.entities"),
        hover.part == L::Part::Entities ? palette::text : palette::dim);
    toggleSwitch(context, l.switchX(), l.entitiesY() + (L::rowHeight - switchHeight) / 2, p.entities);
    auto running = schematic::ghosts::saveStatus();
    if (running) {
        int percent = running->total ? static_cast<int>(running->done * 100 / running->total) : 0;
        label(context, x, l.statusY(), l.inner(), translated(running->waiting ? "schematic.save.progressWaiting" : "schematic.save.progress",
            running->file, percent), running->waiting ? palette::warning : palette::dim);
    } else if (!p.problem.empty()) label(context, x, l.statusY(), l.inner(), p.problem, palette::warning);
    drawSmallButton(context, l.saveX(), l.buttonY(), L::buttonWidth, L::buttonHeight,
        translated(running ? "schematic.save.stop" : p.overwrite ? "schematic.save.overwrite" : "schematic.save.button"),
        hover.part == L::Part::Save, p.overwrite || running ? Rgb{.54f, .18f, .16f} : palette::accentDeep,
        p.overwrite || running ? Rgb{.54f, .23f, .2f} : palette::accent);
    drawSmallButton(context, l.clearX(), l.buttonY(), L::buttonWidth, L::buttonHeight, translated("schematic.save.clear"),
        hover.part == L::Part::Clear);
    drawSmallButton(context, l.cancelX(), l.buttonY(), L::buttonWidth, L::buttonHeight, translated("schematic.save.cancel"),
        hover.part == L::Part::Cancel);
    paragraph(context, x, l.hintY(), l.inner(), translated("schematic.save.hint"), 3, palette::faint);
    label(context, x, l.keysY(), l.inner(), translated("schematic.save.keys"), palette::faint);
    context.flushText(0, std::nullopt);
}
void enterWorldMap(bool fromSettings, bool resume) {
    if (!client) return;
    finishNumber();
    worldMapOpen = true;
    mapFromSettings = fromSettings;
    waypoints_view::setFromMap(false);
    map::world::open(*client, resume);
}
bool isWorldMapKey(int key) {
    auto chord = input::effectiveChord(Runtime::instance().preferences().bindings, input::Action::OpenWorldMap);
    return chord.size() == 1 && chord[0].device == input::Device::Key && chord[0].code == key;
}
void handleMapRequest(map::world::Request const& request) {
    using Kind = map::world::Request::Kind;
    switch (request.kind) {
    case Kind::Close:
        if (!mapFromSettings) { close(); break; }
        map::world::close();
        worldMapOpen = false;
        mapFromSettings = false;
        table_view::rebuild(true);
        break;
    case Kind::AddWaypoint:
        prompt = WaypointPrompt{request.draft, {}};
        prompt->name.append(prompt->draft.name);
        prompt->name.selectAll();
        promptOnMap = true;
        break;
    case Kind::OpenWaypoints: {
        bool fromSettings = mapFromSettings;
        map::world::close();
        worldMapOpen = false;
        selectNav(waypointsNav, !fromSettings);
        mapFromSettings = fromSettings;
        waypoints_view::setFromMap(true);
        waypoints_view::refresh();
        if (request.mark) waypoints_view::select(request.mark);
        break;
    }
    case Kind::OpenSchematic: {
        // The Placed tab with the placement chosen on the map, if it is still there.
        bool fromSettings = mapFromSettings;
        map::world::close();
        worldMapOpen = false;
        selectNav(schematicsNav, !fromSettings);
        mapFromSettings = fromSettings;
        schematics_view::refresh(true);
        schematics_view::show(schematics_view::Tab::Placements);
        if (request.mark) schematics_view::showPlacement(request.mark->id);
        break;
    }
    case Kind::OpenShape: {
        bool fromSettings = mapFromSettings;
        map::world::close();
        worldMapOpen = false;
        selectNav(shapesNav, !fromSettings);
        mapFromSettings = fromSettings;
        shapes_view::refresh();
        if (request.mark) shapes_view::select(request.mark->id);
        break;
    }
    default: break;
    }
}
bool hudView(ScreenView const& view) {
    auto const& tree = view.mVisualTree;
    return tree && std::string_view(*tree->mRootControlName).ends_with(".hud_screen");
}
void render(ll::event::UIRenderEvent& event) {
    std::lock_guard lock(mutex);
    auto& context = event.uiRenderContext();
    auto& current = context.mClient;
    auto& view = event.screenView();
    glm::vec2 size = view.mSize;
    if (!scene) {
        // The gameplay screen renders four views per frame (crosshair, hud,
        // debug, toast). Drawing on each stacked translucent fills four times,
        // so draw once, on the HUD view itself.
        if (gameplayScreen(current.getScreenName()) && hudView(view)) {
            auto const& settings = Runtime::instance().preferences().information;
            information::drawHud(context,size.x,size.y,settings);
            if (!current.getOptions().getHideHud()) {
                information::drawOffhandSlot(context,view,settings);
                information::drawSaturation(context,view,settings);
            }
#ifdef LAMIUM_HUNGER_TRACE
            information::traceHunger(context,view);
#endif
        }
        return;
    }
    if (&current != client) return;
    if (!ownsTop()) { if (seen) clear(); return; }
    seen = true;
    lastPointer = view.mPointerLocationPrevious;
    applyNumber();
    shapes_view::applyName();
    table_view::applyBinding();
    if (worldMapOpen && !prompt) {
        // Press before release: a quick click delivers both between two
        // frames and must not leave a drag running.
        bool released = std::exchange(pendingRelease, false);
        if (!closing) {
            if (auto click = std::exchange(pendingClick, std::nullopt))
                handleMapRequest(map::world::press(click->x, click->y, click->right));
            for (auto wheel : std::exchange(pendingWheels, {})) map::world::wheel(wheel.direction);
            for (int key : std::exchange(pendingKeys, {}))
                if (worldMapOpen && !prompt && !closing) handleMapRequest(map::world::key(key, isWorldMapKey(key)));
        }
        if (released) map::world::release();
        if (!scene || !worldMapOpen) return;
        displayedInverseScale = current.getGuiData()->mInvGuiScale;
        auto at = map::world::namePosition();
        syncTextKeyboard(at.x, at.y);
        map::world::render(context, size, view.mPointerLocationPrevious, Runtime::instance().preferences().map);
        return;
    }
    if (prompt) {
        pendingRelease = false;
        if (!closing) {
            if (auto click = std::exchange(pendingClick, std::nullopt); click && !click->right)
                handlePromptClick(click->x, click->y, size);
            for (int key : std::exchange(pendingKeys, {})) if (prompt && !closing) handlePromptKey(key);
        }
        if (!scene || !prompt) return;
        displayedInverseScale = current.getGuiData()->mInvGuiScale;
        auto l = WaypointPromptLayout::at(size.x, size.y);
        syncTextKeyboard(l.left + WaypointPromptLayout::pad, l.fieldY());
        if (worldMapOpen) map::world::render(context, size, {-1, -1}, Runtime::instance().preferences().map);
        renderPrompt(context, size, view.mPointerLocationPrevious);
        return;
    }
    if (schematicMenu) {
        pendingRelease = false;
        glm::vec2 pointer = view.mPointerLocationPrevious;
        if (!closing) {
            schematicMenu->hover = menuLayout(size).hit(pointer.x, pointer.y);
            if (auto click = std::exchange(pendingClick, std::nullopt)) handleMenuClick(click->right);
            for (auto wheel : std::exchange(pendingWheels, {})) if (schematicMenu && !closing) handleMenuWheel(wheel.direction);
            for (int key : std::exchange(pendingKeys, {})) if (schematicMenu && !closing) handleMenuKey(key);
        }
        if (!scene) return;
        displayedInverseScale = current.getGuiData()->mInvGuiScale;
        if (schematicMenu) {
            renderSchematicMenu(context, size, pointer);
            return;
        }
    }
    if (savePrompt) {
        pendingRelease = false;
        if (!closing) {
            if (auto click = std::exchange(pendingClick, std::nullopt); click && !click->right)
                handleSavePromptClick(click->x, click->y, size);
            for (int key : std::exchange(pendingKeys, {})) if (savePrompt && !closing) handleSavePromptKey(key);
        }
        if (!scene || !savePrompt) return;
        displayedInverseScale = current.getGuiData()->mInvGuiScale;
        auto l = SavePromptLayout::at(size.x, size.y);
        syncTextKeyboard(l.left + SavePromptLayout::pad, l.fieldY());
        renderSavePrompt(context, size, view.mPointerLocationPrevious);
        return;
    }
    if (!closing && hudEditorView()) {
        // Release first so a click that lands after a drag starts fresh.
        if (std::exchange(pendingRelease, false)) hud_editor::release();
        bool exit = false;
        if (auto click = std::exchange(pendingClick, std::nullopt); click && !click->right)
            exit = hud_editor::press(click->x, click->y) == hud_editor::Result::Exit;
        for (int key : std::exchange(pendingKeys, {}))
            if (!exit) exit = hud_editor::key(key, heldShift()) == hud_editor::Result::Exit;
        if (exit) { hud_editor::reset(); selectNav(editorReturn); }
    } else if (!closing) {
        if (std::exchange(pendingRelease, false)) {
            table_view::release();
            scrollDragFirst = nullptr;
            schematics_view::release();
        }
        schematics_view::drag(lastPointer);
        if (scrollDragFirst && scrollDragLayout) *scrollDragFirst = scrollDragLayout->firstAt(lastPointer.y);
        if (shapesView()) {
            shapes_view::refresh();
            if (auto click = std::exchange(pendingClick, std::nullopt)) shapes_view::click(click->x, click->y, click->right);
            for (int key : std::exchange(pendingKeys, {})) shapes_view::key(key);
        } else if (waypointsView()) {
            waypoints_view::refresh();
            if (auto click = std::exchange(pendingClick, std::nullopt)) waypoints_view::click(click->x, click->y, click->right);
            for (int key : std::exchange(pendingKeys, {})) waypoints_view::key(key);
        } else if (schematicsView()) {
            schematics_view::refresh(false);
            if (auto click = std::exchange(pendingClick, std::nullopt)) schematics_view::click(click->x, click->y, click->right);
            for (int key : std::exchange(pendingKeys, {})) schematics_view::key(key);
        } else {
            if (std::exchange(pendingSearch, false)) { finishNumber(); table_view::focusSearch(); }
            if (auto click = std::exchange(pendingClick, std::nullopt)) table_view::click(click->x, click->y, click->right);
            for (int key : std::exchange(pendingKeys, {})) table_view::key(key);
        }
    }
    dropMovedWarning();
    if (!scene) return;
    // No HUD under the settings list: the overlap made both hard to read.
    displayedInverseScale = current.getGuiData()->mInvGuiScale;
    glm::vec2 pointer = view.mPointerLocationPrevious;
    if (hudEditorView()) {
        releaseTextKeyboard();
        hud_editor::render(context, size.x, size.y, pointer.x, pointer.y);
        return;
    }
    if (shapesView()) {
        shapes_view::refresh();
        auto caret = shapes_view::caret();
        syncTextKeyboard(caret.x, caret.y);
        if (shapes_view::docked()) shapes_view::renderDocked(context, size, pointer);
        else table_view::render(context, current, size, pointer);
    } else if (waypointsView()) {
        waypoints_view::refresh();
        auto caret = waypoints_view::caret();
        syncTextKeyboard(caret.x, caret.y);
        if (waypoints_view::docked()) waypoints_view::renderDocked(context, size, pointer);
        else table_view::render(context, current, size, pointer);
    } else if (schematicsView()) {
        schematics_view::refresh(false);
        auto caret = schematics_view::caret();
        syncTextKeyboard(caret.x, caret.y);
        if (schematics_view::docked()) schematics_view::renderDocked(context, size, pointer);
        else table_view::render(context, current, size, pointer);
    } else {
        auto caret = table_view::caret();
        syncTextKeyboard(caret.x, caret.y);
        table_view::render(context, current, size, pointer);
    }
}
}
namespace screen {
IClientInstance* client() { return ui::client; }
std::string const& message() { return error; }
void setMessage(std::string text) { error = std::move(text); }
void clearMessage() { error.clear(); }
void warnRange(std::string text) { ui::warnRange(std::move(text)); }
NumberInput& number() { return numberInput; }
void numberTyped() { numberDirty = true; }
void finishEditing() { finishNumber(); }
bool heldCtrl() { return ui::heldCtrl(); }
bool heldShift() { return ui::heldShift(); }
void close() { ui::close(); }
void nextNav(bool back) { selectNav((navigation.current + (back ? worldMapNav - 1 : 1)) % worldMapNav); }
bool navClick(float x, float y) {
    auto hit = table_view::layout().hit(x, y, navCount, table_view::tabWidth());
    if (hit.zone == Zone::Nav) { ui::selectNav(hit.index); return true; }
    if (hit.zone == Zone::Version) { table_view::copyVersion(); return true; }
    return false;
}
bool pressScrollbar(ShapesLayout const& layout, int& first, float x, float y) { return ui::pressScrollbar(layout, first, x, y); }
void toggleOption(std::string_view id) { table_view::adjustOption(id, 1); }
void showFeatureKeys(std::string_view featureId) {
    int category = 0;
    for (size_t i = 0; i < sections.size(); ++i) if (sections[i] == featureSection(featureId)) category = static_cast<int>(i) + 1;
    table_view::expand(featureId);
    ui::selectNav(category);
    table_view::selectFeature(featureId);
}
SettingsTable const& table() { return table_view::layout(); }
int currentNav() { return navigation.current; }
void selectNav(int index) { ui::selectNav(index); }
input::Chord const& heldKeys() { return uiHeld; }
void numberReset() { numberDirty = false; }
std::optional<Place> standingPlace() { return ui::standingPlace(); }
int playerDimension() { return ui::playerDimension(); }
std::string dimensionName(int dimension) { return ui::dimensionName(dimension); }
int distanceTo(int x, int z) { return ui::distanceTo(x, z); }
void returnToMap() { enterWorldMap(mapFromSettings, true); }
}
void open(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene || !gameplayScreen(current.getScreenName())) return;
    CameraSessions::instance().suspendInput();
    error.clear(); rangeWarning.reset(); seen = false; closing = false; pendingClick.reset(); pendingKeys.clear(); pendingSearch = false;
    numberDirty = false; uiHeld.clear();
    shapes_view::endEditing();
    table_view::open();
    // Category, expansion and scroll persist between openings in a session.
    navigation.reopenNormal();
    table_view::rebuild(true);
    // This native information screen supplies focus/cursor ownership. It has no
    // form ID, packet, or server callback. Lamium draws and handles its own UI.
    scene = current.getSceneFactory().createCommonDialogInfoScreen("Lamium", "");
    if (!scene) return;
    client = &current;
    openedAt = std::chrono::steady_clock::now();
    current.getSceneFactory().getCurrentSceneStack()->pushScreen(scene, false);
}
void openShapes(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (!scene) open(current);
    if (scene) selectNav(shapesNav, true);
}
void openWaypointPrompt(IClientInstance& current, map::Waypoint draft) {
    std::lock_guard lock(mutex);
    if (scene) return; // Only from gameplay; the open settings screen keeps its own work.
    open(current);
    if (!scene) return;
    prompt = WaypointPrompt{std::move(draft), {}};
    prompt->name.append(prompt->draft.name);
    prompt->name.selectAll();
}
void openWaypoints(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (!scene) open(current);
    if (scene && !prompt) selectNav(waypointsNav, true);
}
void openSchematics(IClientInstance& current, int tab) {
    std::lock_guard lock(mutex);
    if (!scene) open(current);
    if (!scene || prompt || savePrompt) return;
    selectNav(schematicsNav, true);
    schematics_view::refresh(true);
    if (tab >= 0 && tab < 4) schematics_view::show(static_cast<schematics_view::Tab>(tab));
}
void openSchematicSave(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene) return; // Only from gameplay, like the waypoint prompt.
    auto state = schematic::selection::current();
    auto area = state.area();
    if (!area) return;
    open(current);
    if (!scene) return;
    savePrompt = SavePrompt{*area, state.dimension, {}, false, false, {}};
    savePrompt->name.append("schematic");
    savePrompt->name.selectAll();
}
void openSchematicMenu(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene) return; // From gameplay only.
    open(current);
    if (!scene) return;
    auto const preferences = Runtime::instance().preferences();
    schematicMenu = SchematicMenu{};
    schematicMenu->show(schematic::menu::openAt(preferences.schematic.menuReopen, schematicMenuClosedAt));
}
void openWorldMap(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene) return;
    open(current);
    if (!scene) return;
    enterWorldMap(false, false);
}
void openHotkeys(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (!scene) open(current);
    if (scene) selectNav(hotkeysNav, true);
}
void openHudLayout(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (!scene) open(current);
    if (scene) {
        selectNav(hudNav, true);
        hud_editor::select(std::nullopt);
    }
}
bool ownsInput() {
    std::lock_guard lock(mutex);
    return scene != nullptr;
}
void cancelInputCapture() {
    std::lock_guard lock(mutex);
    releaseTextKeyboard();
    if (table_view::capturing()) table_view::cancelCapture();
    finishNumber();
    table_view::unfocusSearch();
    uiHeld.clear();
}
void start() {
    backgroundHook = SettingsWorldBackground::hook(true) == 0;
    if (!backgroundHook) throw std::runtime_error("Could not install settings world background hook");
    textHook = SettingsSearchText::hook(true) == 0;
    if (!textHook) throw std::runtime_error("Could not install settings text input hook");
    renderHook = SettingsSceneRender::hook(true) == 0;
    if (!renderHook) throw std::runtime_error("Could not install settings scene render hook");
    exitHook = SettingsSceneExit::hook(true) == 0;
    if (!exitHook) throw std::runtime_error("Could not install settings scene exit hook");
    entranceHook = SettingsSceneEntrance::hook(true) == 0;
    if (!entranceHook) throw std::runtime_error("Could not install settings scene entrance hook");
    auto& bus = ll::event::EventBus::getInstance();
    listeners[0] = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        retired.reset();
        if (scene && &event.uiRenderContext().mClient == client && !ownsTop()) {
            // A push can also be dropped by a screen transition without an exit
            // callback. Give an unseen dialog a moment to reach the top first.
            if (seen || std::chrono::steady_clock::now() - openedAt > std::chrono::seconds(3)) clear();
        }
        if (!scene) render(event);
    });
    listeners[4] = bus.emplaceListener<ll::event::BeforeUIRenderEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        if (!scene || settingsRenderView != &event.screenView()) return;
        // Only replace our focus-owning dialog's drawing. Other native screens
        // and the world keep their normal rendering and resource-pack behavior.
        event.cancel();
        render(event);
    });
    listeners[1] = bus.emplaceListener<ll::event::input::MouseInputEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        if (!ownsTop()) return;
        if (event.actionButtonId() == MouseAction::ActionMove || event.actionButtonId() == MouseAction::ActionMoveRelative) return;
        bool wheel = event.actionButtonId() == MouseAction::ActionWheel;
        if (wheel && event.buttonData() == 0) return;
        int button = event.actionButtonId();
        input::Token token = wheel ? input::Token{input::Device::Wheel, event.buttonData() > 0 ? 1 : -1}
            : input::Token{input::Device::Mouse, button > MouseAction::ActionWheel ? button-1 : button};
        bool down = wheel || event.buttonData() == MouseAction::DataDown;
        // Render hover can still describe the previous pointer position when
        // movement and a click arrive between frames. Hit the displayed layout
        // using this event's pixel coordinates and the scale used to draw it.
        float x = event.x() * displayedInverseScale, y = event.y() * displayedInverseScale;
        bool scaled = std::isfinite(displayedInverseScale) && displayedInverseScale > 0;
        observeHeld(token, down);
        if (table_view::capturing() && !shapesView() && !waypointsView() && !schematicsView()) {
            if (down) event.cancel();
            if (button == MouseAction::ActionLeft && down && scaled && table_view::onCaptureButton(x, y)) pendingClick = Click{x, y, false};
            else table_view::captureInput(token, down);
            return;
        }
        // A button may already be down when L opens the panel. Let vanilla
        // observe its release, just as we do for keys, so it cannot stay held.
        if (!wheel && event.buttonData() == MouseAction::DataUp) {
            if (button == MouseAction::ActionLeft) pendingRelease = true;
            return;
        }
        event.cancel();
        if (schematicMenu && wheel) {
            pendingWheels.push_back({x, y, event.buttonData() > 0 ? 1 : -1});
            return;
        }
        if (worldMapOpen && !prompt && wheel) {
            if (scaled) pendingWheels.push_back({x, y, event.buttonData() > 0 ? 1 : -1});
            return;
        }
        if (hudEditorView() && wheel) {
            if (scaled) hud_editor::wheel(event.buttonData() > 0 ? -3 : 3, x, y);
            return;
        }
        if (schematicsView() && wheel) {
            schematics_view::wheel(event.buttonData() > 0 ? -3 : 3, lastPointer, heldShift());
            return;
        }
        if (waypointsView() && wheel) {
            waypoints_view::wheel(event.buttonData() > 0 ? -3 : 3, lastPointer);
            return;
        }
        if (shapesView() && wheel) {
            // Scroll whichever pane is under the pointer; selection stays put.
            shapes_view::wheel(event.buttonData() > 0 ? -3 : 3, lastPointer);
            return;
        }
        if (wheel) {
            // The wheel scrolls the table; selection stays on its row.
            table_view::wheel(event.buttonData() > 0 ? -3 : 3);
            return;
        }
        if (!scaled || !down) return;
        if (button == MouseAction::ActionLeft) pendingClick = Click{x, y, false};
        if (button == MouseAction::ActionRight) pendingClick = Click{x, y, true};
    });
    listeners[2] = bus.emplaceListener<ll::event::input::KeyInputEvent>([](auto& event) {
        std::lock_guard lock(mutex);
        if (!ownsTop()) return;
        input::Token token{input::Device::Key, event.keyCode()};
        observeHeld(token, event.isDown());
        if (table_view::capturing()) {
            if (event.isDown()) event.cancel();
            if (event.isDown() && event.keyCode() == 0x1b) table_view::cancelCapture();
            else table_view::captureInput(token, event.isDown());
            return;
        }
        // Let key-up through so keys pressed before opening cannot stick.
        if (!event.isDown()) return;
        if (prompt) {
            // Text goes through the native keyboard; editing keys act at once,
            // the rest (Enter, Esc, Tab) run with the next frame.
            auto key = event.keyCode();
            bool selectAll = key == 0x41 && heldCtrl();
            bool command = key == 0x08 || key == 0x1b || key == 0x0d || key == 0x09 || selectAll;
            if (textKeyboardOwned && !command) return;
            event.cancel();
            if (key == 0x08) prompt->name.backspace();
            else if (selectAll) prompt->name.selectAll();
            else pendingKeys.push_back(key);
            return;
        }
        if (savePrompt) {
            auto key = event.keyCode();
            bool selectAll = key == 0x41 && heldCtrl();
            bool command = key == 0x08 || key == 0x1b || key == 0x0d || selectAll;
            if (textKeyboardOwned && !command) return;
            event.cancel();
            if (key == 0x08) { if (savePrompt->name.backspace()) savePrompt->overwrite = false; }
            else if (selectAll) savePrompt->name.selectAll();
            else pendingKeys.push_back(key);
            return;
        }
        if (worldMapOpen && map::world::editingName()) {
            auto key = event.keyCode();
            bool selectAll = key == 0x41 && heldCtrl();
            bool command = key == 0x08 || key == 0x1b || key == 0x0d || key == 0x09 || selectAll;
            if (textKeyboardOwned && !command) return;
            event.cancel();
            if (key == 0x08) map::world::backspace();
            else if (selectAll) map::world::selectAllName();
            else pendingKeys.push_back(key);
            return;
        }
        // Keep search reachable from anywhere in the table. Capture handles
        // keys above this point, so Ctrl+F remains bindable.
        if (!worldMapOpen && !shapesView() && !waypointsView() && !schematicsView() && event.keyCode() == 0x46 && heldCtrl()) {
            event.cancel();
            pendingSearch = true;
            return;
        }
        // Native text generation happens after HID onKeyDown. Keep editing
        // commands here, but let the focused native keyboard process the other
        // keys (including layout/IME input) while our modal scene owns gameplay.
        if (textKeyboardOwned && (table_view::searchFocused() || numericEditing() || shapes_view::editingName() || waypoints_view::editingName())) {
            auto key = event.keyCode();
            bool commandKey = key == 0x08 || key == 0x1b || key == 0x0d || key == 0x09
                || (table_view::searchFocused() && key == 0x28);
            bool selectAll = key == 0x41 && heldCtrl();
            if (!commandKey && !selectAll) return;
        }
        event.cancel();
        // Editing keys act immediately so rapid typing keeps its order. Keys
        // that finish a number or name save settings or shapes, so they run
        // with the next frame like the remaining navigation, never inside the
        // input event.
        auto key = event.keyCode();
        bool tool = shapesView() || waypointsView() || schematicsView();
        bool editing = shapesView() ? (shapes_view::editingName() || shapes_view::editingNumber())
            : waypointsView() ? (waypoints_view::editingName() || waypoints_view::editingNumber())
            : schematicsView() ? schematics_view::editingNumber()
            : (table_view::searchFocused() || table_view::editingNumber());
        bool finishes = (key == 0x1b || key == 0x0d || key == 0x09) && (tool || !table_view::searchFocused());
        if (editing && !finishes) {
            if (shapesView()) shapes_view::key(key);
            else if (waypointsView()) waypoints_view::key(key);
            else if (schematicsView()) schematics_view::key(key);
            else table_view::key(key);
            return;
        }
        pendingKeys.push_back(key);
    });
    listeners[3] = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
        std::lock_guard lock(mutex); clear();
    });
}
void stop() {
    std::lock_guard lock(mutex);
    close();
    clear();
    for (auto& listener : listeners) {
        if (listener) ll::event::EventBus::getInstance().removeListener(listener);
        listener.reset();
    }
    if (entranceHook) { SettingsSceneEntrance::unhook(true); entranceHook = false; }
    if (exitHook) { SettingsSceneExit::unhook(true); exitHook = false; }
    if (renderHook) { SettingsSceneRender::unhook(true); renderHook = false; }
    if (textHook) { SettingsSearchText::unhook(true); textHook = false; }
    if (backgroundHook) { SettingsWorldBackground::unhook(true); backgroundHook = false; }
}
}
