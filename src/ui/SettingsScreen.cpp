#include "ui/SettingsScreen.h"
#include "settings/Options.h"
#include "ui/SettingsRows.h"
#include "ui/SettingsNavigation.h"
#include "ui/SettingsTable.h"
#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "ui/WaypointPromptLayout.h"
#include "ui/SavePromptLayout.h"
#include "ui/RadialLayout.h"
#include "ui/Animations.h"
#include "ui/Toast.h"
#include "features/map/WaypointSession.h"
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

// Settings table. Navigation items: All, each section, then the Hotkeys and
// Shapes tools pinned to the sidebar bottom.
constexpr int navCount = static_cast<int>(sections.size()) + 7;
constexpr int hotkeysNav = navCount - 6;
constexpr int shapesNav = navCount - 5;
constexpr int waypointsNav = navCount - 4;
constexpr int schematicsNav = navCount - 3;
// The world map is never the current item: choosing it opens the map over
// the panel, and closing the map returns to where the settings were.
constexpr int worldMapNav = navCount - 2;
// The HUD layout editor replaces the whole panel; leaving returns to editorReturn.
constexpr int hudNav = navCount - 1;
int editorReturn = 0;
bool pendingRelease = false;
settings::Option const* sliderDrag = nullptr; // Slider being dragged with the left button.
SettingsNavigation navigation;
std::set<std::string_view> expanded;
std::set<std::string_view> searchCollapsed;
std::vector<SettingsRow> rows;
int selected = -1;
int first = 0;
// Keyboard navigation shows the selected row's key tooltip; moving the mouse
// hands it back to hover.
bool keyboardTip = false;
glm::vec2 tipPointer{};
SettingsTable displayed;
float displayedInverseScale = 0;
// Where the pointer was at the last frame, in GUI units. Wheel events carry
// no position, so the pane under the pointer is found from this.
glm::vec2 lastPointer{-1, -1};
// A list scrollbar being dragged: the list's first row and its layout.
int* scrollDragFirst = nullptr;
ShapesLayout const* scrollDragLayout = nullptr;
// The schematic preview (L-114): where it was drawn, how it is turned, and a
// drag in progress. It turns by itself until the player first drags it.
struct PreviewTurn {
    float x = 0, y = 0, w = 0, h = 0; // last drawn box, GUI units
    float yaw = 35, pitch = 30;
    bool manual = false, dragging = false;
    glm::vec2 from{};
    float fromYaw = 0, fromPitch = 0;
} previewTurn;
// A press on a list's scrollbar moves the list there and starts a drag.
bool pressScrollbar(ShapesLayout const& l, int& first, float x, float y) {
    if (!l.onScrollbar(x, y)) return false;
    first = l.firstAt(y);
    scrollDragFirst = &first;
    scrollDragLayout = &l;
    return true;
}
float displayedTabWidth = 0;
// GUI coordinates; resolved against the layout drawn in the next frame.
struct Click { float x, y; bool right; };
std::optional<Click> pendingClick;
std::vector<int> pendingKeys;
bool pendingSearch = false; // Ctrl+F, applied with the next frame.
SearchQuery query;
bool searchFocused = false;
settings::Option const* editingNumber = nullptr;
NumberInput numberInput;
bool numberDirty = false;

// Shapes view. The collection lives in the overlay session; this is only what
// the editor shows: the selected shape or an unsaved draft, and scroll state.
using ShapeZone = ShapesLayout::Zone;
bool shapesDocked = false;
std::optional<overlay::ShapeId> shapeSelected;
std::optional<overlay::ShapeDefinition> shapeDraft;
bool shapePicking = false, shapeDeleteArmed = false;
// L-46: General resets every setting, Hotkeys resets key bindings. The
// first press arms the button; the second applies. Shapes are per-world
// data and are never touched.
enum class ResetScope { None, All, Section, Keys };
bool resetArmed = false;
int shapeListFirst = 0, shapeFieldFirst = 0, shapeFieldSelected = -1, shapeLayer = 0;
shape::Reference shapeReference = shape::Reference::StandingBlock;
std::vector<overlay::shapes::Summary> shapeList;
ShapesLayout shapesDisplayed;
int editingShapeField = -1;
bool editingShapeName = false;
SearchQuery shapeNameInput;
bool shapeNameDirty = false;
// Waypoints view (L-60 step 5c): built like Shapes, from a copy of the
// current world's waypoints taken each frame.
bool waypointsDocked = false;
int waypointSelected = -2; // -2 none, -1 the death point, else an index into the set.
map::WaypointSet waypointSet;
std::vector<size_t> waypointList; // Display order, indices into the set.
ShapesLayout waypointsDisplayed;
int waypointListFirst = 0, waypointFieldFirst = 0, waypointFieldSelected = -1;
bool waypointDeleteArmed = false;
int editingWaypointField = -1;
bool editingWaypointName = false, waypointNameDirty = false;
SearchQuery waypointNameInput;
void applyWaypointName();
void drawEditText(MinecraftUIRenderContext&, float x, float top, float height, float width, SearchQuery const&);
map::Waypoint const* selectedWaypoint();
void changeSelected(std::function<void(map::Waypoint&)> const& apply);
void copyVersion();
void renderWaypointsContent(MinecraftUIRenderContext&, glm::vec2 size, glm::vec2 pointer, SettingsTable const&);
void renderSchematicsContent(MinecraftUIRenderContext&, glm::vec2 size, glm::vec2 pointer, SettingsTable const&);
// Schematics view (L-93): placements of this world above the files in the
// schematics folder, built like Waypoints. The file list is scanned when the
// view opens and on "Reload files", not every frame.
bool schematicsDocked = false;
enum class SchematicPick { None, Placement, File };
enum class SchematicTab { Files, Placements, Verify, Materials }; // mockup order
SchematicTab schematicTab = SchematicTab::Placements;
SchematicPick schematicPick = SchematicPick::None;
int schematicIndex = -1; // into the placements or the files
schematic::PlacementSet schematicSet;
std::vector<schematic::session::FileEntry> schematicFiles;
std::vector<schematic_files::Row> schematicFileRows; // the Files list: folder headings and files
// Blocks other than air and structure void, counted once per loaded structure.
std::map<schematic::Structure const*, std::uint64_t> schematicBlockCounts;
std::uint64_t blockCount(schematic::Structure const& structure) {
    auto [found, fresh] = schematicBlockCounts.try_emplace(&structure, 0);
    if (fresh) {
        if (schematicBlockCounts.size() > 256) { schematicBlockCounts.clear(); return blockCount(structure); }
        for (auto index : structure.blocks)
            if (index != schematic::voidCell && !structure.palette[static_cast<size_t>(index)].isAir()) ++found->second;
    }
    return found->second;
}
std::chrono::steady_clock::time_point schematicFilesScanned{};
std::string largeSchematicConfirmed; // a large file the player chose to load
ShapesLayout schematicsDisplayed;
int schematicListFirst = 0, schematicFieldFirst = 0, schematicFieldSelected = -1;
bool schematicDeleteArmed = false;
int editingSchematicField = -1;
bool numericEditing() { return editingNumber || editingShapeField >= 0 || editingWaypointField >= 0 || editingSchematicField >= 0; }
void changeSchematic(std::function<void(schematic::SavedPlacement&)> const& apply);
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
bool waypointsFromMap = false; // The Waypoints screen's close returns to the map.
void enterWorldMap(bool fromSettings, bool resume);
struct Wheel { float x, y; int direction; };
std::vector<Wheel> pendingWheels;
bool mapCacheArmed = false;
// The delete button as last drawn: only a click on it deletes.
float mapCacheButtonX = 0, mapCacheButtonWidth = 0;
bool onMapCacheButton(float x) { return mapCacheButtonWidth > 0 && x >= mapCacheButtonX && x < mapCacheButtonX + mapCacheButtonWidth; }

bool textHook = false;
bool textKeyboardOwned = false;
bool textKeyboardNumber = false;
std::optional<input::Action> capturing;
input::BindingCapture capture;
input::Chord uiHeld;
struct BindingEdit { input::Action action; std::optional<input::Chord> binding; };
std::optional<BindingEdit> bindingEdit;

std::string_view categoryKey() {
    return navigation.current > 0 && navigation.current < hotkeysNav
        ? sections[navigation.current - 1] : std::string_view{};
}
bool hotkeysView() { return navigation.current == hotkeysNav; }
bool shapesView() { return navigation.current == shapesNav; }
bool waypointsView() { return navigation.current == waypointsNav; }
bool schematicsView() { return navigation.current == schematicsNav; }
bool hudEditorView() { return navigation.current == hudNav; }
bool valid(int row) { return row >= 0 && row < static_cast<int>(rows.size()); }
int nextSelectable(int from, int step) {
    for (int row = from; valid(row); row += step) if (rows[row].selectable()) return row;
    return -1;
}
void rebuild(bool keepSelection) {
    std::optional<SettingsRow> previous;
    if (keepSelection && valid(selected)) previous = rows[selected];
    // preferences() returns a copy: keep it alive while its bindings are read.
    auto const preferences = Runtime::instance().preferences();
    rows = buildSettingsRows(hotkeysView(), categoryKey(), query, expanded,
        [](std::string_view key) { return translated(key); }, preferences.information.lineOrder,
        searchCollapsed);
    selected = -1;
    if (previous)
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i] == *previous) { selected = static_cast<int>(i); break; }
    if (selected < 0) selected = nextSelectable(0, 1);
    first = SettingsTable::clampFirst(first, static_cast<int>(rows.size()), displayed.visible);
}
void refreshSchematics(bool files);
void selectNav(int index, bool temporary = false) {
    // Choosing a category ends a search, which otherwise spans every category.
    query.clear();
    searchCollapsed.clear();
    resetArmed = false;
    if (navigation.current == shapesNav && index != shapesNav) { shapeDraft.reset(); shapePicking = false; overlay::shapes::setDraft({}); }
    index = std::clamp(index, 0, navCount - 1);
    waypointsFromMap = false;
    if (index == worldMapNav) { enterWorldMap(true, false); return; }
    if (index == hudNav && navigation.current != hudNav) {
        editorReturn = navigation.current;
        hud_editor::reset();
        pendingRelease = false;
    }
    if (index == schematicsNav && navigation.current != schematicsNav) refreshSchematics(true);
    navigation.select(index, temporary);
    first = 0;
    rebuild(false);
}
void observeHeld(input::Token token, bool down) {
    if (token.device == input::Device::Wheel) return;
    if (!down) std::erase(uiHeld, token);
    else if (std::find(uiHeld.begin(), uiHeld.end(), token) == uiHeld.end()) uiHeld.push_back(token);
}
void captureInput(input::Token token, bool down) {
    if (!capturing || bindingEdit) return;
    try {
        auto value = capture.observe(token, down, input::actions[static_cast<size_t>(*capturing)].behavior);
        if (value) bindingEdit = BindingEdit{*capturing, std::move(value)};
    } catch (std::exception const&) { error = translated("invalidBinding"); }
}
void cancelCapture() {
    // The table stays where it was: selection and scroll are not rebuilt.
    capturing.reset(); capture.clear(); bindingEdit.reset(); error.clear();
}
void startCapture(input::Action action) {
    capturing = action; capture.begin(uiHeld); error.clear();
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
    bool wanted = !closing && !capturing && (searchFocused || numericEditing() || editingShapeName || editingWaypointName || prompt
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
// Applies an edited definition: a draft only updates its world preview; an
// existing shape is saved through the session, reporting failures in the footer.
void applyShape(overlay::ShapeDefinition definition) {
    try {
        if (shapeDraft) { overlay::shapes::setDraft(definition); shapeDraft = std::move(definition); }
        else if (shapeSelected) overlay::shapes::edit(*shapeSelected, std::move(definition));
        error.clear();
    } catch (overlay::ShapeSaveError const&) { error = translated("shape.saveError"); }
    catch (std::exception const&) { error = translated("shape.editError"); }
}
std::optional<overlay::ShapeDefinition> currentShape() {
    if (shapeDraft) return shapeDraft;
    if (shapeSelected) return overlay::shapes::find(*shapeSelected);
    return {};
}
void applyShapeName() {
    if (!editingShapeName || !std::exchange(shapeNameDirty, false)) return;
    try {
        if (shapeDraft) {
            auto definition = *shapeDraft;
            definition.name = shapeNameInput.value();
            overlay::ShapeCollection{}.add(definition); // Validates the name only.
            shapeDraft = std::move(definition);
            overlay::shapes::setDraft(shapeDraft);
        } else if (shapeSelected) overlay::shapes::rename(*shapeSelected, shapeNameInput.value());
        error.clear();
    }
    catch (overlay::ShapeSaveError const&) { error = translated("shape.saveError"); }
    catch (std::exception const&) { error = translated("shape.nameError"); }
}
void queryChanged() { searchCollapsed.clear(); first = 0; rebuild(false); }
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
        if (editingWaypointName) { if (waypointNameInput.type(text)) waypointNameDirty = true; return; }
        if (editingShapeName) { if (shapeNameInput.type(text)) shapeNameDirty = true; return; }
        if (numericEditing()) { if (numberInput.append(text)) numberDirty = true; return; }
        if (!capturing && searchFocused && query.type(text)) queryChanged();
        return;
    }
    origin(text, impact);
}
void clear() {
    sliderDrag = nullptr;
    releaseTextKeyboard(); editingNumber = nullptr; editingShapeField = -1; editingShapeName = false; shapeNameDirty = false;
    numberDirty = false; uiHeld.clear(); capturing.reset(); bindingEdit.reset(); capture.clear(); client = nullptr;
    scene.reset(); seen = false; closing = false; pendingClick.reset(); pendingKeys.clear(); pendingSearch = false;
    // A draft is never kept once the screen is gone.
    if (shapeDraft) { shapeDraft.reset(); overlay::shapes::setDraft({}); }
    shapePicking = false; shapeDeleteArmed = false;
    prompt.reset();
    savePrompt.reset();
    if (schematicMenu) schematicMenuClosedAt = schematicMenu->category;
    schematicMenu.reset();
    if (worldMapOpen) map::world::close();
    worldMapOpen = false; promptOnMap = false; pendingWheels.clear(); mapCacheArmed = false;
    mapFromSettings = false; waypointsFromMap = false;
    editingWaypointField = -1; editingWaypointName = false; waypointNameDirty = false; waypointDeleteArmed = false;
    editingSchematicField = -1; schematicDeleteArmed = false;
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
WarningPlace warningPlace() { return {navigation.current, selected, shapeFieldSelected, shapeSelected}; }
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
    if (editingWaypointField >= 0) {
        auto parsed = numberInput.parsedPrecise(-map::coordinateLimit, map::coordinateLimit, true);
        if (!parsed) { warnRange(translated("integerRange", -map::coordinateLimit, map::coordinateLimit)); return; }
        int field = editingWaypointField, value = static_cast<int>(*parsed);
        auto const* w = selectedWaypoint();
        if (!w || (field == 0 ? w->x : field == 1 ? w->y : w->z) == value) { error.clear(); return; }
        changeSelected([&](map::Waypoint& t) { (field == 0 ? t.x : field == 1 ? t.y : t.z) = value; });
        return;
    }
    if (editingSchematicField >= 0) {
        int field = editingSchematicField;
        int limit = field == 9 ? 4096 : 30'000'000, low = field == 9 ? 1 : -limit;
        auto parsed = numberInput.parsedPrecise(low, limit, true);
        if (!parsed) { warnRange(translated("integerRange", low, limit)); return; }
        int value = static_cast<int>(*parsed);
        changeSchematic([&](schematic::SavedPlacement& p) {
            if (field == 9) p.layers.index = value - 1;
            else (field == 0 ? p.placement.origin.x : field == 1 ? p.placement.origin.y : p.placement.origin.z) = value;
        });
        return;
    }
    if (editingShapeField >= 0) {
        auto definition = currentShape();
        auto fields = definition ? shape::rows(*definition, shapeDraft.has_value()) : std::vector<shape::Row>{};
        if (!definition || editingShapeField >= static_cast<int>(fields.size())) { editingShapeField = -1; return; }
        auto field = fields[editingShapeField].field;
        auto range = shape::numeric(*definition, field);
        if (!range) { editingShapeField = -1; return; }
        auto parsed = numberInput.parsedPrecise(range->minimum,range->maximum,range->integer);
        if (!parsed) {
            warnRange(translated(range->integer ? "integerRange" : "numberRange",range->minimum,range->maximum));
            return;
        }
        if (*parsed == range->value) { error.clear(); return; }
        applyShape(shape::setNumber(*definition, field, *parsed));
        return;
    }
    auto const& range = *editingNumber->numeric;
    auto parsed = numberInput.parsed(range.minimum, range.maximum);
    if (!parsed) { warnRange(translated("numberRange", range.minimum, range.maximum)); return; }
    auto value = Runtime::instance().preferences();
    if (std::get<float>(editingNumber->read(value)) == *parsed) { error.clear(); return; }
    range.write(value, *parsed);
    error = Runtime::instance().save(value) ? std::string{} : translated("saveError");
}
void finishNumber() {
    applyNumber();
    applyShapeName();
    applyWaypointName();
    releaseTextKeyboard();
    editingNumber = nullptr; editingShapeField = -1; editingShapeName = false; numberDirty = false;
    editingWaypointField = -1; editingWaypointName = false;
    editingSchematicField = -1;
}
void close() {
    releaseTextKeyboard();
    resetArmed = false;
    if (ownsTop()) {
        if (!closing) client->getSceneFactory().getCurrentSceneStack()->schedulePopScreen(1);
        closing = true;
    } else clear();
}

// ---- Settings table actions ----
// Read current preferences for every edit so another action cannot be
// overwritten by a stale copy captured when the screen opened.
void adjustOption(settings::Option const& option, int direction) {
    auto value = Runtime::instance().preferences();
    option.adjust(value, direction);
    error = Runtime::instance().save(value) ? std::string{} : translated("saveError");
    // The stepper of a row being typed into shows the typed text; replace it
    // with the stepped value so -/+ are visible at once.
    if (editingNumber == &option) {
        numberInput.begin(std::get<float>(option.read(Runtime::instance().preferences())));
        numberDirty = false;
    }
}
bool hasSwitch(FeatureInfo const& feature) {
    return !feature.toggle.empty() || isSessionFeature(feature.id);
}
void toggleFeature(FeatureInfo const& feature) {
    if (auto option = settings::find(feature.toggle)) adjustOption(*option, 1);
    else if (client && isSessionFeature(feature.id)) toggleSession(*client, feature.id);
}
void setExpanded(int row, bool open) {
    if (!valid(row) || !rows[row].heading() || !rows[row].children) return;
    if (open == rows[row].expanded) return;
    bool searching = query.value().find_first_not_of(' ') != std::string::npos;
    auto id = rows[row].feature->id;
    selected = row;
    if (open) { expanded.insert(id); searchCollapsed.erase(id); }
    else { expanded.erase(id); if (searching) searchCollapsed.insert(id); }
    // Rows above the feature are unchanged, so it keeps its index and screen
    // position. Reveal new children only as far as the feature stays visible.
    rebuild(true);
    if (open && displayed.visible > 0) {
        int last = row;
        while (last + 1 < static_cast<int>(rows.size()) && rows[last + 1].child()
            && rows[last + 1].feature == rows[row].feature) ++last;
        if (last >= first + displayed.visible) first = std::min(row, last - displayed.visible + 1);
    }
    first = SettingsTable::clampFirst(first, static_cast<int>(rows.size()), displayed.visible);
}
void beginNumber(settings::Option const& option) {
    editingNumber = &option;
    numberInput.begin(std::get<float>(option.read(Runtime::instance().preferences())));
    error.clear();
}
// Enter / Space / click on the name of a row.
void openLayout(std::optional<HudElementId> element) {
    selectNav(hudNav);
    hud_editor::select(element);
}
void setSlider(settings::Option const& option, float fraction) {
    auto const& range = *option.numeric;
    float value = SettingsTable::sliderValue(fraction, range.minimum, range.maximum, range.step);
    auto preferences = Runtime::instance().preferences();
    if (option.read(preferences) == settings::OptionValue{value}) return;
    range.write(preferences, value);
    preferences.normalize();
    error = Runtime::instance().save(preferences) ? std::string{} : translated("saveError");
}
void pressMapCache() {
    if (!std::exchange(mapCacheArmed, true)) return;
    mapCacheArmed = false;
    if (map::store::clear()) showMessageToast(translated("mapCacheCleared"));
}
void activateRow(int row, bool space) {
    if (!valid(row)) return;
    auto const& entry = rows[row];
    switch (entry.kind) {
    case RowKind::Section: return;
    case RowKind::Feature:
        if (space && hasSwitch(*entry.feature)) { toggleFeature(*entry.feature); return; }
        if (entry.children) { setExpanded(row, !entry.expanded); return; }
        if (hasSwitch(*entry.feature)) { toggleFeature(*entry.feature); return; }
        if (auto primary = primaryAction(*entry.feature)) startCapture(*primary);
        return;
    case RowKind::Option:
        if (entry.option->numeric) beginNumber(*entry.option);
        else adjustOption(*entry.option, 1);
        return;
    case RowKind::Action: startCapture(*entry.action); return;
    case RowKind::Layout: openLayout(*entry.layout); return;
    case RowKind::MapCache: pressMapCache(); return;
    }
}
void moveSelection(int step) {
    int target = valid(selected) ? selected + step : (step > 0 ? 0 : static_cast<int>(rows.size()) - 1);
    target = std::clamp(target, 0, std::max(0, static_cast<int>(rows.size()) - 1));
    int found = nextSelectable(target, step > 0 ? 1 : -1);
    if (found < 0) found = nextSelectable(target, step > 0 ? -1 : 1);
    if (found >= 0) selected = found;
    first = SettingsTable::reveal(first, selected, displayed.visible);
}
ResetScope resetScope() {
    if (hotkeysView()) return ResetScope::Keys;
    if (query.value().find_first_not_of(' ') != std::string::npos) return ResetScope::None;
    if (navigation.current == 0) return ResetScope::All;
    return categoryKey().empty() ? ResetScope::None : ResetScope::Section;
}
void pressReset(ResetScope scope) {
    if (!resetArmed) { resetArmed = true; return; }
    resetArmed = false;
    auto value = Runtime::instance().preferences();
    if (scope == ResetScope::Keys) value.bindings = {};
    else if (scope == ResetScope::Section) resetSection(value, categoryKey());
    else value = Settings{};
    error = Runtime::instance().save(value) ? std::string{} : translated("saveError");
    rebuild(false);
}
void handleClick(SettingsTable::Hit const& hit, bool right) {
    if (!(hit.zone == Zone::Row && valid(hit.index) && rows[hit.index].kind == RowKind::MapCache && !right
          && onMapCacheButton(hit.x)))
        mapCacheArmed = false;
    auto scope = capturing ? ResetScope::None : resetScope();
    bool head = scope != ResetScope::None && displayed.headAction(hit.x, hit.y, hotkeysView());
    if (!head || right) resetArmed = false;
    if (head && !right) { finishNumber(); pressReset(scope); return; }
    if (hit.zone != Zone::Row || !valid(hit.index) || rows[hit.index].kind != RowKind::Option
        || editingNumber != rows[hit.index].option) finishNumber();
    if (capturing) {
        if (hit.zone == Zone::Footer && !right) {
            int button = displayed.footerButton(hit.x, hit.y);
            if (!input::canClear(*capturing)) {
                if (button == 0) bindingEdit = BindingEdit{*capturing, std::nullopt};
                else if (button == 1) cancelCapture();
            } else if (button == 2) cancelCapture();
            else if (button >= 0) bindingEdit = BindingEdit{*capturing, button == 0
                ? std::optional<input::Chord>(input::Chord{}) : std::nullopt};
        }
        return;
    }
    switch (hit.zone) {
    case Zone::Search: searchFocused = true; return;
    case Zone::Close: close(); return;
    case Zone::Version: copyVersion(); return;
    case Zone::Nav: searchFocused = false; selectNav(hit.index); return;
    case Zone::Row: break;
    default: return;
    }
    searchFocused = false;
    keyboardTip = false;
    if (!valid(hit.index) || !rows[hit.index].selectable()) return;
    selected = hit.index;
    auto const& entry = rows[hit.index];
    if (right) {
        if (entry.option) adjustOption(*entry.option, -1);
        return;
    }
    switch (entry.kind) {
    case RowKind::Feature:
        if (hit.column == Column::State) toggleFeature(*entry.feature);
        else if (hit.column == Column::Key) {
            if (auto primary = primaryAction(*entry.feature)) startCapture(*primary);
        } else if (entry.children) setExpanded(hit.index, !entry.expanded);
        return;
    case RowKind::Option: {
        auto linked = optionAction(entry.option->id);
        if (linked && hit.column == Column::Key) { startCapture(*linked); return; }
        auto value = entry.option->read(Runtime::instance().preferences());
        if (std::holds_alternative<bool>(value)) {
            if (hit.column != Column::Name) adjustOption(*entry.option, 1);
            return;
        }
        // While typing, the row shows the stepper, so clicks go to its buttons.
        if (entry.option->numeric && entry.option->numeric->step > 0 && editingNumber != entry.option) {
            float fraction = displayed.sliderFraction(hit.x);
            if (fraction < 0) { beginNumber(*entry.option); return; }
            if (hit.x < displayed.sliderX()) return;
            sliderDrag = entry.option;
            setSlider(*entry.option, fraction);
            return;
        }
        int part = displayed.stepperPart(hit.x, linked.has_value());
        if (part == -1 || part == 1) adjustOption(*entry.option, part);
        else if (part == 0) {
            if (entry.option->numeric) beginNumber(*entry.option);
            else adjustOption(*entry.option, 1);
        }
        return;
    }
    case RowKind::Action:
        if (hit.column == Column::Key) startCapture(*entry.action);
        return;
    case RowKind::Layout: openLayout(*entry.layout); return;
    case RowKind::MapCache: if (onMapCacheButton(hit.x)) pressMapCache(); return;
    default: return;
    }
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
void handleKey(int key) {
    if (searchFocused) {
        switch (key) {
        case 0x41: if (heldCtrl()) query.selectAll(); break;
        case 0x08: if (query.backspace()) queryChanged(); break;
        case 0x1b: searchFocused = false; break;
        case 0x0d: case 0x09: case 0x28:
            searchFocused = false; selected = nextSelectable(0, 1); first = 0; break;
        }
        return;
    }
    if (editingNumber) {
        switch (key) {
        case 0x08: if (numberInput.backspace()) numberDirty = true; break;
        case 0x41: if (heldCtrl()) numberInput.selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: finishNumber(); break;
        }
        return;
    }
    auto* entry = valid(selected) ? &rows[selected] : nullptr;
    int page = std::max(1, displayed.visible - 1);
    switch (key) {
    case 0x1b: close(); break;
    case 0x26: moveSelection(-1); keyboardTip = true; break;
    case 0x28: moveSelection(1); keyboardTip = true; break;
    case 0x21: moveSelection(-page); keyboardTip = true; break;
    case 0x22: moveSelection(page); keyboardTip = true; break;
    case 0x24: selected = -1; moveSelection(1); keyboardTip = true; break; // Home
    case 0x23: selected = static_cast<int>(rows.size()); moveSelection(-1); keyboardTip = true; break; // End
    case 0x09: selectNav((navigation.current + (heldShift() ? worldMapNav - 1 : 1)) % worldMapNav); break;
    case 0x25: case 0x27: {
        int direction = key == 0x27 ? 1 : -1;
        if (!entry) break;
        if (entry->heading()) setExpanded(selected, direction > 0);
        else if (entry->option) adjustOption(*entry->option, direction);
        break;
    }
    case 0x0d: activateRow(selected, false); break;
    case 0x20: activateRow(selected, true); break;
    }
}

// ---- Settings table drawing ----
std::string featureName(FeatureInfo const& feature) { return translated(feature.name); }
std::string actionLabel(input::Action action, bool child = false) {
    return actionName(translated(actionTranslationKey(action, child)));
}
std::string behaviorText(input::Action action) {
    auto behavior = input::actions[static_cast<size_t>(action)].behavior;
    // Freelook is the one action whose activation is a named feature setting.
    if (action == input::Action::Freelook && Runtime::instance().preferences().camera.freelookToggle)
        behavior = input::Behavior::Toggle;
    return translated(behavior == input::Behavior::Hold ? "behavior.hold"
        : behavior == input::Behavior::Toggle ? "behavior.toggle" : "behavior.press");
}
std::string optionValueText(settings::Option const& option, settings::OptionValue const& value) {
    auto pattern = splitLabel(translated(option.label)).value;
    try {
        if (auto choice = std::get_if<settings::ChoiceValue>(&value)) return translated(choice->label);
        if (auto number = std::get_if<float>(&value)) {
            float secondary = option.numeric ? *number * option.numeric->secondary : 0;
            return std::vformat(pattern, std::make_format_args(*number, secondary));
        }
    } catch (std::exception const&) {}
    return {};
}
std::vector<std::string> bindingKeys(IClientInstance& current, input::Action action) {
    // preferences() returns a copy: keep it alive while its bindings are read.
    auto const preferences = Runtime::instance().preferences();
    auto const& binding = preferences.bindings[static_cast<size_t>(action)];
    std::vector<std::string> keys;
    if (binding) {
        for (auto token : *binding) keys.push_back(bindingChordName(current, input::Chord{token}));
        return keys;
    }
    auto name = actionBindingName(current, action);
    if (name != translated("unbound")) keys.push_back(std::move(name));
    return keys;
}
std::vector<std::string> chordKeys(IClientInstance& current, input::Chord const& chord) {
    std::vector<std::string> keys;
    for (auto token : chord) keys.push_back(bindingChordName(current, input::Chord{token}));
    return keys;
}
input::Relation strongestConflict(std::vector<input::Conflict> const& conflicts) {
    auto relation = input::Relation::None;
    for (auto conflict : conflicts) relation = std::max(relation, conflict.relation);
    return relation;
}
KeyTone conflictTone(input::Relation relation) {
    return relation == input::Relation::Shared ? KeyTone::Filled
        : relation == input::Relation::Overlap ? KeyTone::Outline : KeyTone::Plain;
}
void drawKeyCell(MinecraftUIRenderContext& context, IClientInstance& current, float y, input::Action action) {
    // Cap text sits at the cap top in Japanese; start the cap low enough that
    // its text lines up with the row name at y + 3.
    float x = displayed.keyX, width = displayed.keyWidth, cy = y + 2;
    if (capturing == action) {
        fill(context,x,cy-1,width,capHeight+2,palette::accent,.25f);
        frame(context,x,cy-1,width,capHeight+2,palette::accent);
        auto text = capture.value().empty() ? translated("captureBox") : bindingChordName(current, capture.value());
        label(context,x+3,cy+boxTextInset(),width-6,std::move(text));
        return;
    }
    auto keys = bindingKeys(current, action);
    if (keys.empty()) { label(context,x,cy+1,width,translated("unbound"),palette::faint); return; }
    auto relation = strongestConflict(input::bindingConflicts(Runtime::instance().preferences().bindings, action));
    keycaps(context,x,cy,width,keys,conflictTone(relation));
}
float badge(MinecraftUIRenderContext& context, float x, float y, std::string text, Rgb color) {
    float w = textWidth(context, text) + 5;
    frame(context,x,y+1,w,SettingsTable::rowHeight-3,color);
    label(context,x+3,y+1+boxTextInset(),w-3,std::move(text),color);
    return w;
}
// Name cell with optional trailing count and experimental badge, truncated to fit.
void drawName(MinecraftUIRenderContext& context, float x, float y, float right, std::string name, Rgb color,
              int count, bool experimental) {
    std::string countText = count > 0 ? std::to_string(count) : std::string{};
    std::string exp = experimental ? translated("experimental") : std::string{};
    float extras = (count > 0 ? textWidth(context, countText) + 5 : 0) + (experimental ? textWidth(context, exp) + 10 : 0);
    float nameWidth = std::max(0.0f, right - x - extras);
    label(context,x,y+3,nameWidth,name,color);
    float cursor = x + std::min(nameWidth, textWidth(context, name)) + 5;
    if (count > 0) { label(context,cursor,y+3,right-cursor,countText,palette::faint); cursor += textWidth(context, countText) + 5; }
    if (experimental && cursor + 8 < right) badge(context,cursor,y,std::move(exp),palette::experimental);
}
void drawStepper(MinecraftUIRenderContext& context, float y, settings::Option const& option, std::string const& value,
                 bool editing, bool warn = false) {
    bool keyed = optionAction(option.id).has_value();
    float x = displayed.stepperX(keyed), w = displayed.stepperWidth(keyed), aw = SettingsTable::arrowWidth;
    float cy = y + 1, h = SettingsTable::rowHeight - 2;
    fill(context,x,cy,aw,h,palette::keyFill);
    fill(context,x+w-aw,cy,aw,h,palette::keyFill);
    frame(context,x,cy,w,h,editing ? palette::accent : warn ? palette::warning : palette::keyEdge);
    bool numeric = option.numeric.has_value();
    if (numeric) {
        label(context,x,cy+1+boxTextInset(),aw,"-",palette::dim,Align::Center);
        label(context,x+w-aw,cy+1+boxTextInset(),aw,"+",palette::dim,Align::Center);
    } else {
        arrow(context,x+4,cy+3,true);
        arrow(context,x+w-aw+4,cy+3,false);
    }
    std::string text = editing
        ? (numberInput.selectedAll() ? "[" + numberInput.value() + "]" : numberInput.value() + "_") : value;
    label(context,x+aw+1,cy+1+boxTextInset(),w-2*aw-2,std::move(text),warn ? palette::warning : palette::text,Align::Center);
}
void drawGuide(MinecraftUIRenderContext& context, float y, bool last) {
    float x = displayed.nameX + 3;
    fill(context,x,y,1,last ? SettingsTable::rowHeight / 2 : SettingsTable::rowHeight,palette::white,.18f);
    fill(context,x+1,y+SettingsTable::rowHeight/2,5,1,palette::white,.18f);
}
std::string navLabel(int index, bool compact) {
    if (index == 0) return translated("nav.all");
    if (index == hotkeysNav) return translated("nav.hotkeys");
    if (index == shapesNav) return translated("nav.shapes");
    if (index == waypointsNav) return translated("nav.waypoints");
    if (index == schematicsNav) return translated("nav.schematics");
    if (index == worldMapNav) return translated("nav.worldMap");
    if (index == hudNav) return translated("nav.hudLayout");
    auto key = std::string(sections[index-1]);
    return translated(compact ? key + ".short" : key);
}
std::string sectionCount(std::string_view section, Settings const& preferences) {
    int on = 0, total = 0;
    for (auto const& feature : features) {
        if (featureSection(feature.id) != section) continue;
        if (auto option = settings::find(feature.toggle)) {
            ++total;
            if (std::get<bool>(option->read(preferences))) ++on;
        }
    }
    return total ? std::to_string(on) + "/" + std::to_string(total) : std::string{};
}
std::string description() {
    if (capturing) return actionLabel(*capturing) + " - " + behaviorText(*capturing);
    if (!valid(selected)) return query.value().find_first_not_of(' ') != std::string::npos
        ? translated("noResultsFor", query.value()) : std::string{};
    auto const& entry = rows[selected];
    if (entry.child() && effectsPaused(entry.feature->id,Runtime::instance().preferences()))
        return translated("help.effectsPaused");
    switch (entry.kind) {
    case RowKind::Feature: {
        auto text = translated(entry.feature->description);
        if (auto primary = primaryAction(*entry.feature)) text += " " + behaviorText(*primary);
        return text;
    }
    case RowKind::Option: {
        if (auto warning = optionWarning(entry.option->id,Runtime::instance().preferences())) return translated(*warning);
        if (entry.option->numeric) {
            auto const& range = *entry.option->numeric;
            return translated(editingNumber ? "numberRange" : "numberControl", range.minimum, range.maximum);
        }
        auto helpKey = "help." + std::string(entry.option->id);
        auto help = translated(helpKey);
        return help != helpKey ? help : translated(entry.feature->description);
    }
    case RowKind::Action: {
        // An action may explain itself ("help.key.<id>"); the press/hold/toggle note follows.
        auto helpKey = "help.key." + std::string(input::actions[static_cast<size_t>(*entry.action)].id);
        auto help = translated(helpKey);
        return (hotkeysView() ? featureName(*entry.feature) + ": " : std::string{})
            + (help != helpKey ? help + " " : std::string{}) + behaviorText(*entry.action);
    }
    case RowKind::Layout: return translated("help.layoutLink");
    case RowKind::MapCache: return translated("help.mapCache");
    default: return {};
    }
}
// ---- Shapes view ----
// Reference point for new or moved shapes, and the reference actually used: a
// missing target falls back to the standing block with a notice.
std::optional<std::pair<overlay::Point, shape::Reference>> referencePoint() {
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return {};
    if (shapeReference == shape::Reference::TargetBlock) {
        auto const& hit = client->getLatestHitResult();
        if (hit.mType == HitResultType::Tile)
            return std::pair{overlay::Point{hit.mBlock.x + .5, hit.mBlock.y + .5, hit.mBlock.z + .5}, shapeReference};
        error = translated("shape.targetMissing");
        auto feet = player->getFeetPos();
        return std::pair{overlay::Point{feet.x, feet.y, feet.z}, shape::Reference::StandingBlock};
    }
    auto feet = player->getFeetPos();
    return std::pair{overlay::Point{feet.x, feet.y, feet.z}, shapeReference};
}
int playerDimension() {
    auto* player = client ? client->getLocalPlayer() : nullptr;
    return player ? static_cast<int>(player->getDimensionId()) : 0;
}
overlay::Point centerOf(overlay::ShapeDefinition const& definition) {
    if (auto spec = std::get_if<overlay::ShapeSpec>(&definition.geometry)) return spec->center;
    auto const& origin = std::get<overlay::PlaneSpec>(definition.geometry).origin;
    return {double(origin.x), double(origin.y), double(origin.z)};
}
void resetShapeEditor() { shapeFieldFirst = 0; shapeFieldSelected = -1; shapeLayer = 0; shapeDeleteArmed = false; }
void cancelDraft() {
    if (shapeDraft) { shapeDraft.reset(); overlay::shapes::setDraft({}); }
    shapePicking = false;
}
void beginDraft(int type) {
    auto point = referencePoint();
    if (!point) return;
    overlay::ShapeDefinition definition;
    definition.name = translated(shape::types[type].name);
    definition.dimension = playerDimension();
    shapeDraft = shape::withType(std::move(definition), type, point->first, point->second);
    shapePicking = false;
    shapeSelected.reset();
    resetShapeEditor();
    try { overlay::shapes::setDraft(shapeDraft); }
    catch (std::exception const&) { error = translated("shape.editError"); }
}
void createDraft() {
    if (!shapeDraft) return;
    try {
        auto id = overlay::shapes::add(*shapeDraft);
        cancelDraft();
        shapeSelected = id;
        resetShapeEditor();
        error.clear();
    } catch (overlay::ShapeSaveError const&) { error = translated("shape.saveError"); }
    catch (std::exception const&) { error = translated("shape.editError"); }
}
void selectShape(std::optional<overlay::ShapeId> id) {
    cancelDraft();
    shapeSelected = id;
    resetShapeEditor();
}
// Click or key activation of an editor row. part: -1/1 step, 0 value, 2 label.
void activateShapeField(int index, int part) {
    auto definition = currentShape();
    if (!definition) return;
    auto fields = shape::rows(*definition, shapeDraft.has_value());
    if (index < 0 || index >= static_cast<int>(fields.size()) || fields[index].kind == shape::Row::Kind::Group) return;
    shapeFieldSelected = index;
    if (part == 2) return;
    int direction = part == -1 ? -1 : 1;
    auto const& row = fields[index];
    switch (row.field) {
    case shape::Field::Type: {
        if (!shapeDraft) return;
        int count = static_cast<int>(shape::types.size());
        int old = shape::typeIndex(*shapeDraft), type = (old + direction + count) % count;
        auto next = shape::withType(*shapeDraft, type, centerOf(*shapeDraft), shapeReference);
        if (next.name == translated(shape::types[old].name)) next.name = translated(shape::types[type].name);
        applyShape(std::move(next));
        return;
    }
    case shape::Field::Reference: {
        shapeReference = static_cast<shape::Reference>((static_cast<int>(shapeReference) + direction + 3) % 3);
        if (shapeDraft) if (auto point = referencePoint()) {
            auto moved = *shapeDraft;
            shape::place(moved, point->first, point->second);
            applyShape(std::move(moved));
        }
        return;
    }
    case shape::Field::MoveHere: {
        if (auto point = referencePoint()) {
            auto moved = *definition;
            shape::place(moved, point->first, point->second);
            moved.dimension = playerDimension();
            applyShape(std::move(moved));
        }
        return;
    }
    default: break;
    }
    if (auto range = shape::numeric(*definition, row.field); range && part == 0) {
        editingShapeField = index;
        numberInput.beginPrecise(range->value);
        error.clear();
        return;
    }
    applyShape(shape::adjust(*definition, row.field, direction));
}
void moveShapeField(int step) {
    auto definition = currentShape();
    if (!definition) return;
    auto fields = shape::rows(*definition, shapeDraft.has_value());
    int index = shapeFieldSelected;
    for (size_t tries = 0; tries < fields.size(); ++tries) {
        index = std::clamp(index + step, 0, static_cast<int>(fields.size()) - 1);
        if (fields[index].kind != shape::Row::Kind::Group) break;
        if (index == 0 || index == static_cast<int>(fields.size()) - 1) step = -step;
    }
    shapeFieldSelected = index;
    int visible = shapesDisplayed.fieldVisible;
    if (visible > 0) {
        if (index < shapeFieldFirst) shapeFieldFirst = index;
        if (index >= shapeFieldFirst + visible) shapeFieldFirst = index - visible + 1;
    }
}
void openShapeKeySettings() {
    // Show the Shape rendering row, expanded, in its category.
    cancelDraft();
    int category = 0;
    for (size_t i = 0; i < sections.size(); ++i) if (sections[i] == featureSection("shapes")) category = static_cast<int>(i) + 1;
    expanded.insert("shapes");
    selectNav(category);
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].heading() && rows[i].feature->id == "shapes") { selected = static_cast<int>(i); break; }
    first = SettingsTable::reveal(first, selected, displayed.visible);
}
void handleShapeClick(float x, float y, bool right) {
    finishNumber();
    if (!right && pressScrollbar(shapesDisplayed, shapeListFirst, x, y)) return;
    if (!shapesDocked) {
        auto nav = displayed.hit(x, y, navCount, displayedTabWidth);
        if (nav.zone == Zone::Nav) { selectNav(nav.index); return; }
        if (nav.zone == Zone::Version) { copyVersion(); return; }
    }
    auto hit = shapesDisplayed.hit(x, y);
    if (!(hit.zone == ShapeZone::Action && hit.index == 1 && !shapeDraft)) shapeDeleteArmed = false;
    switch (hit.zone) {
    case ShapeZone::Close: close(); return;
    case ShapeZone::Dock: shapesDocked = !shapesDocked; return;
    case ShapeZone::Keys: openShapeKeySettings(); return;
    case ShapeZone::DrawAll:
        if (auto option = settings::find("overlays.shapes")) adjustOption(*option, 1);
        return;
    case ShapeZone::NewShape:
        cancelDraft();
        shapeSelected.reset();
        shapePicking = true;
        resetShapeEditor();
        return;
    case ShapeZone::ListRow: {
        int index = hit.index - (shapeDraft ? 1 : 0);
        if (index < 0 || index >= static_cast<int>(shapeList.size())) return;
        auto const& item = shapeList[index];
        auto const& l = shapesDisplayed;
        if (x >= l.listLeft + l.listWidth - ShapesLayout::pad - switchWidth - 2) {
            try { overlay::shapes::setVisible(item.id, !item.definition.visible); error.clear(); }
            catch (overlay::ShapeSaveError const&) { error = translated("shape.saveError"); }
            catch (std::exception const&) { error = translated("shape.editError"); }
            return;
        }
        if (shapeSelected != item.id || shapeDraft) selectShape(item.id);
        return;
    }
    case ShapeZone::Name:
        if (auto definition = currentShape()) {
            editingShapeName = true;
            shapeNameInput.clear();
            shapeNameInput.append(definition->name);
            shapeNameInput.selectAll();
            error.clear();
        }
        return;
    case ShapeZone::LayerDown: --shapeLayer; return;
    case ShapeZone::LayerUp: ++shapeLayer; return;
    case ShapeZone::Field:
        if (!right && hit.part != 2) if (auto definition = currentShape()) {
            auto fields = shape::rows(*definition, shapeDraft.has_value());
            if (hit.index >= 0 && hit.index < static_cast<int>(fields.size()) && fields[hit.index].field == shape::Field::Color) {
                shapeFieldSelected = hit.index;
                if (int swatch = shapesDisplayed.swatchAt(x, 4); swatch >= 0) {
                    definition->color = static_cast<overlay::ShapeColor>(swatch);
                    applyShape(std::move(*definition));
                }
                return;
            }
        }
        activateShapeField(hit.index, right ? -1 : hit.part);
        return;
    case ShapeZone::Pick: beginDraft(hit.index); return;
    case ShapeZone::Action:
        if (shapeDraft) { if (hit.index == 0) createDraft(); else cancelDraft(); return; }
        if (!shapeSelected) return;
        if (hit.index == 0) {
            if (auto definition = currentShape()) {
                definition->name = translated("shape.copyName", definition->name);
                if (definition->name.size() > 128) definition->name.resize(128);
                try { selectShape(overlay::shapes::add(*definition)); error.clear(); }
                catch (overlay::ShapeSaveError const&) { error = translated("shape.saveError"); }
                catch (std::exception const&) { error = translated("shape.editError"); }
            }
            return;
        }
        if (!shapeDeleteArmed) { shapeDeleteArmed = true; return; }
        try {
            size_t position = 0;
            for (; position < shapeList.size() && shapeList[position].id != *shapeSelected; ++position) {}
            overlay::shapes::remove(*shapeSelected);
            auto remaining = overlay::shapes::list();
            selectShape(remaining.empty() ? std::nullopt
                : std::optional(remaining[std::min(position, remaining.size() - 1)].id));
            error.clear();
        } catch (overlay::ShapeSaveError const&) { error = translated("shape.saveError"); }
        catch (std::exception const&) { error = translated("shape.editError"); }
        return;
    default: return;
    }
}
void handleShapeKey(int key) {
    if (editingShapeName) {
        switch (key) {
        case 0x08: if (shapeNameInput.backspace()) shapeNameDirty = true; break;
        case 0x41: if (heldCtrl()) shapeNameInput.selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: finishNumber(); break;
        }
        return;
    }
    if (editingShapeField >= 0) {
        switch (key) {
        case 0x08: if (numberInput.backspace()) numberDirty = true; break;
        case 0x41: if (heldCtrl()) numberInput.selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: finishNumber(); break;
        }
        return;
    }
    switch (key) {
    case 0x1b:
        if (shapePicking || shapeDraft) cancelDraft();
        else close();
        break;
    case 0x26: moveShapeField(-1); break;
    case 0x28: moveShapeField(1); break;
    case 0x25: activateShapeField(shapeFieldSelected, -1); break;
    case 0x27: activateShapeField(shapeFieldSelected, 1); break;
    case 0x0d: case 0x20: activateShapeField(shapeFieldSelected, 0); break;
    case 0x21: case 0x22: {
        // Previous / next shape in the list.
        if (shapeList.empty()) break;
        int index = 0;
        for (size_t i = 0; i < shapeList.size(); ++i) if (shapeSelected == shapeList[i].id) index = static_cast<int>(i);
        index = std::clamp(index + (key == 0x22 ? 1 : -1), 0, static_cast<int>(shapeList.size()) - 1);
        selectShape(shapeList[index].id);
        break;
    }
    case 0x09: selectNav((navigation.current + (heldShift() ? worldMapNav - 1 : 1)) % worldMapNav); break;
    }
}

// Preview is regenerated only when the geometry or layer changes.
struct PreviewCache {
    std::optional<overlay::ShapeDefinition> definition;
    int layer = 0;
    std::optional<shape::Preview> preview;
} previewCache;
bool sameGeometry(overlay::ShapeDefinition const& a, overlay::ShapeDefinition const& b) {
    auto sa = std::get_if<overlay::ShapeSpec>(&a.geometry), sb = std::get_if<overlay::ShapeSpec>(&b.geometry);
    if (sa && sb) return sa->shape == sb->shape && sa->center == sb->center && sa->snap == sb->snap
        && sa->radius == sb->radius && sa->height == sb->height;
    auto pa = std::get_if<overlay::PlaneSpec>(&a.geometry), pb = std::get_if<overlay::PlaneSpec>(&b.geometry);
    return pa && pb && pa->origin == pb->origin && pa->width == pb->width && pa->depth == pb->depth
        && pa->spacing == pb->spacing && pa->plane == pb->plane;
}
shape::Preview const* previewFor(overlay::ShapeDefinition const& definition) {
    if (!previewCache.definition || !sameGeometry(*previewCache.definition, definition) || previewCache.layer != shapeLayer) {
        previewCache.definition = definition;
        previewCache.layer = shapeLayer;
        try { previewCache.preview = shape::preview(definition, shapeLayer); }
        catch (std::exception const&) { previewCache.preview.reset(); }
        if (previewCache.preview) {
            // Keep the shown layer inside the shape, then rebuild once for it.
            int layer = std::clamp(shapeLayer, previewCache.preview->lowLayer, previewCache.preview->highLayer);
            if (layer != shapeLayer) {
                shapeLayer = previewCache.layer = layer;
                try { previewCache.preview = shape::preview(definition, shapeLayer); }
                catch (std::exception const&) { previewCache.preview.reset(); }
            }
        }
    }
    return previewCache.preview ? &*previewCache.preview : nullptr;
}
Rgb shapeRgb(overlay::ShapeColor color) {
    switch (color) {
    case overlay::ShapeColor::Yellow: return {.95f,.8f,.24f};
    case overlay::ShapeColor::Pink: return {.94f,.5f,.75f};
    case overlay::ShapeColor::White: return {.95f,.95f,.95f};
    default: return {.25f,.82f,.88f};
    }
}
constexpr Rgb draftRgb{.62f,.83f,1.f};
void drawTypeIcon(MinecraftUIRenderContext& context, float x, float y, int type, Rgb color) {
    auto pattern = shape::typeGlyphs[static_cast<size_t>(std::clamp(type, 0, static_cast<int>(shape::typeGlyphs.size()) - 1))];
    for (int i = 0; i < 25 && i < static_cast<int>(pattern.size()); ++i)
        if (pattern[i] == '#') fill(context, x + (i % 5) * 2, y + (i / 5) * 2, 2, 2, color);
}
void drawSmallButton(MinecraftUIRenderContext& context, float x, float y, float w, float h, std::string text,
                     bool hovered, Rgb fillColor = palette::keyFill, Rgb edge = palette::keyEdge, Rgb textColor = palette::text) {
    fill(context,x,y,w,h,fillColor);
    if (hovered) fill(context,x,y,w,h,palette::white,.1f);
    frame(context,x,y,w,h,edge);
    label(context,x,y+(h-10)/2+boxTextInset()-1,w,std::move(text),textColor,Align::Center);
}
void drawShapeStepper(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, bool numeric, std::string text,
                      bool editing) {
    float x = l.stepperX(), w = l.stepperWidth(), aw = ShapesLayout::arrowWidth, h = ShapesLayout::rowHeight - 2;
    fill(context,x,y+1,aw,h,palette::keyFill);
    fill(context,x+w-aw,y+1,aw,h,palette::keyFill);
    frame(context,x,y+1,w,h,editing ? palette::accent : palette::keyEdge);
    if (numeric) {
        label(context,x,y+2+boxTextInset(),aw,"-",palette::dim,Align::Center);
        label(context,x+w-aw,y+2+boxTextInset(),aw,"+",palette::dim,Align::Center);
    } else {
        arrow(context,x+4,y+4,true);
        arrow(context,x+w-aw+4,y+4,false);
    }
    if (editing) text = numberInput.selectedAll() ? "[" + numberInput.value() + "]" : numberInput.value() + "_";
    label(context,x+aw+1,y+2+boxTextInset(),w-2*aw-2,std::move(text),palette::text,Align::Center);
}
// A row of color swatches in the editor's value column; the chosen one framed.
template<class ColorOf>
void drawSwatchRow(MinecraftUIRenderContext& context, ShapesLayout const& l, float y, int count, int chosen, ColorOf colorOf) {
    float size = l.swatchSize(count), top = y + (ShapesLayout::rowHeight - size) / 2;
    for (int i = 0; i < count; ++i) {
        float x = l.swatchX(i, count);
        if (i == chosen) frame(context,x-2,top-2,size+4,size+4,palette::white);
        fill(context,x,top,size,size,colorOf(i));
        frame(context,x,top,size,size,Rgb{0,0,0},.6f);
    }
}
std::string shapeDescription(std::optional<overlay::ShapeDefinition> const& definition) {
    if (shapePicking) return translated("shape.pickType");
    if (!definition) return translated(shapeList.empty() ? "shape.empty" : "shape.selectHint");
    if (shapeDraft) return translated("shape.draftNote");
    if (definition->dimension != playerDimension()) return definition->name + ": " + translated("shape.elsewhere");
    if (editingShapeField >= 0) {
        auto fields = shape::rows(*definition, false);
        if (editingShapeField < static_cast<int>(fields.size()))
            if (auto range = shape::numeric(*definition, fields[editingShapeField].field))
                return translated(range->integer ? "integerRange" : "numberRange", range->minimum, range->maximum);
    }
    return definition->name + ": " + translated(definition->visible ? "shape.shownState" : "shape.hiddenState");
}
// List, editor, header controls and footer of the Shapes view.
void drawShapesBody(MinecraftUIRenderContext& context, ShapesLayout const& l, glm::vec2 pointer,
                    std::optional<overlay::ShapeDefinition> const& definition) {
    auto const preferences = Runtime::instance().preferences();
    auto hover = l.hit(pointer.x, pointer.y);
    auto over = [&](ShapeZone zone, int index = -1) { return hover.zone == zone && (index < 0 || hover.index == index); };
    float top = l.top + 4;
    // Header controls.
    label(context,l.drawAllX,l.drawAllY+1+boxTextInset(),l.drawAllWidth-switchWidth-4,translated("shape.drawAll"),
        over(ShapeZone::DrawAll) ? palette::text : palette::dim,Align::Right);
    toggleSwitch(context,l.drawAllX+l.drawAllWidth-switchWidth,l.drawAllY+2,preferences.overlays.shapes);
    label(context,l.keysX,top+1+boxTextInset(),ShapesLayout::keysWidth,translated("shape.keys"),
        over(ShapeZone::Keys) ? palette::text : palette::accent,Align::Center);
    fill(context,l.keysX+6,top+11,ShapesLayout::keysWidth-12,1,palette::accent,over(ShapeZone::Keys) ? 1.f : .5f);
    drawSmallButton(context,l.dockX,top,ShapesLayout::dockWidth,12,translated(shapesDocked ? "shape.undock" : "shape.dock"),over(ShapeZone::Dock));

    // List pane.
    float listRight = l.listLeft + l.listWidth;
    drawSmallButton(context,l.listLeft+ShapesLayout::pad,l.toolbarTop+2,ShapesLayout::newWidth,12,translated("shape.new"),
        over(ShapeZone::NewShape),shapePicking ? palette::accent : palette::accentDeep,palette::accent);
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);
    float nameX = l.listLeft + ShapesLayout::pad + 14;
    float shownX = listRight - ShapesLayout::pad - switchWidth - 2;
    float typeX = shownX - 50;
    label(context,nameX,l.theadTop+2,typeX-nameX-4,translated("shape.columnName"),palette::faint);
    label(context,typeX,l.theadTop+2,48,translated("shape.columnType"),palette::faint);
    label(context,shownX-6,l.theadTop+2,switchWidth+12,translated("shape.columnShown"),palette::faint,Align::Center);
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    int draftOffset = shapeDraft ? 1 : 0;
    if (l.listCount == 0)
        paragraph(context,l.listLeft+ShapesLayout::pad,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,translated("shape.empty"),3,palette::faint);
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        bool isDraft = shapeDraft && i == 0;
        auto const* item = isDraft ? nullptr : &shapeList[i - draftOffset];
        auto const& shown = isDraft ? *shapeDraft : item->definition;
        bool chosen = isDraft || (!shapeDraft && item && shapeSelected == item->id);
        if (i % 2) fill(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,palette::white,.025f);
        rowBackground(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,chosen && !isDraft,over(ShapeZone::ListRow,i));
        if (isDraft) frame(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,draftRgb);
        bool elsewhere = shown.dimension != playerDimension();
        // The same type glyph and color as the opened shape; dimmed when hidden or elsewhere.
        Rgb glyph = isDraft ? draftRgb : shapeRgb(shown.color);
        if (!shown.visible || elsewhere) glyph = {glyph.r * .4f, glyph.g * .4f, glyph.b * .4f};
        drawTypeIcon(context,l.listLeft+ShapesLayout::pad,y+2,shape::typeIndex(shown),glyph);
        std::string suffix = isDraft ? translated("shape.draftTag") : elsewhere ? translated("shape.otherDimension", shown.dimension) : "";
        float suffixWidth = suffix.empty() ? 0 : textWidth(context, suffix) + 6;
        label(context,nameX,y+3,typeX-nameX-4-suffixWidth,shown.name,isDraft ? draftRgb : elsewhere ? palette::faint : palette::text);
        if (!suffix.empty())
            label(context,typeX-4-suffixWidth+2,y+3,suffixWidth,suffix,isDraft ? draftRgb : palette::faint);
        label(context,typeX,y+3,48,translated(shape::types[shape::typeIndex(shown)].name),palette::dim);
        if (!isDraft) toggleSwitch(context,shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,shown.visible);
    }
    if (l.listCount > l.listVisible) {
        float track = l.listVisible * ShapesLayout::rowHeight;
        float thumb = std::max(8.0f, track * l.listVisible / l.listCount);
        float thumbY = l.rowsTop + (track - thumb) * l.listFirst / (l.listCount - l.listVisible);
        fill(context,listRight-3,l.rowsTop,2,track,palette::white,.08f);
        fill(context,listRight-3,thumbY,2,thumb,palette::keyEdge);
    }
    // Divider between list and editor.
    if (l.docked) fill(context,l.left,l.detailTop-1,l.width,1,palette::white,.14f);
    else fill(context,l.detailLeft-1,l.toolbarTop,1,l.footerTop-l.toolbarTop,palette::white,.14f);

    // Editor pane.
    float dx = l.detailLeft + ShapesLayout::pad, dw = l.detailWidth - 2 * ShapesLayout::pad;
    if (shapePicking) {
        label(context,dx,l.nameY+2,dw,translated("shape.group.basic"),palette::accent);
        for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(shape::types.size()); ++i) {
            float y = l.fieldY(i);
            if (over(ShapeZone::Pick, i)) fill(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::pickHeight,palette::white,.07f);
            drawTypeIcon(context,dx+1,y+6,i,palette::dim);
            label(context,dx+16,y+2,dw-16,translated(shape::types[i].name));
            label(context,dx+16,y+11,dw-16,translated(shape::types[i].description),palette::faint);
        }
    } else if (!definition) {
        paragraph(context,dx,l.detailTop+6,dw,translated(shapeList.empty() ? "shape.empty" : "shape.selectHint"),3,palette::faint);
    } else {
        if (shapeDraft) label(context,dx,l.detailTop+4,dw,translated("shape.draftNote"),draftRgb);
        // Name field.
        fill(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,Rgb{0,0,0},.4f);
        frame(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,editingShapeName ? palette::accent : palette::keyEdge);
        drawTypeIcon(context,dx+3,l.nameY+1.5f,shape::typeIndex(*definition),shapeDraft ? draftRgb : shapeRgb(definition->color));
        if (editingShapeName) drawEditText(context,dx+17,l.nameY,ShapesLayout::rowHeight-1,dw-20,shapeNameInput);
        else label(context,dx+17,l.nameY+1+boxTextInset(),dw-20,definition->name);
        // Preview: one layer seen from above; plane seen along its normal.
        float px = dx, py = l.previewY, size = ShapesLayout::previewSize;
        fill(context,px,py,size,size,Rgb{0,0,0},.35f);
        frame(context,px,py,size,size,palette::white,.14f);
        auto const* preview = previewFor(*definition);
        if (preview) {
            float spanU = float(preview->maxU - preview->minU + 1), spanV = float(preview->maxV - preview->minV + 1);
            float scale = std::min((size - 4) / spanU, (size - 4) / spanV);
            float ox = px + (size - spanU * scale) / 2 - preview->minU * scale;
            float oy = py + (size - spanV * scale) / 2 - preview->minV * scale;
            Rgb color = shapeDraft ? draftRgb : shapeRgb(definition->color);
            for (auto const& run : preview->runs)
                fill(context,ox + run.u * scale,oy + run.v * scale,std::max(1.0f, run.length * scale),std::max(1.0f, scale),color,.9f);
            fill(context,ox,oy,std::max(1.0f, scale),std::max(1.0f, scale),palette::accent);
        }
        float ix = px + size + 8, iw = dw - size - 8;
        label(context,ix,py+1,iw,translated(shape::types[shape::typeIndex(*definition)].name));
        if (preview) label(context,ix,py+12,iw,translated("shape.cellCount",preview->cells),palette::dim);
        if (auto* player = client ? client->getLocalPlayer() : nullptr) {
            auto feet = player->getFeetPos();
            auto center = centerOf(*definition);
            int distance = static_cast<int>(std::round(std::hypot(center.x - feet.x, center.z - feet.z)));
            label(context,ix,py+22,iw,translated("shape.distance",distance),palette::dim);
        }
        if (preview && preview->lowLayer != preview->highLayer) {
            float lx = l.layerStepperX(), ly = l.layerY(), lw = 64, aw = ShapesLayout::arrowWidth;
            fill(context,lx,ly+1,aw,ShapesLayout::rowHeight-2,palette::keyFill);
            fill(context,lx+lw-aw,ly+1,aw,ShapesLayout::rowHeight-2,palette::keyFill);
            frame(context,lx,ly+1,lw,ShapesLayout::rowHeight-2,palette::keyEdge);
            label(context,lx,ly+2+boxTextInset(),aw,"-",palette::dim,Align::Center);
            label(context,lx+lw-aw,ly+2+boxTextInset(),aw,"+",palette::dim,Align::Center);
            label(context,lx+aw,ly+2+boxTextInset(),lw-2*aw,translated("shape.layer",(shapeLayer >= 0 ? "+" : "") + std::to_string(shapeLayer)),palette::text,Align::Center);
        }
        paragraph(context,ix,py+45,iw,translated("shape.previewHint"),1,palette::faint);
        // Fields.
        auto fields = shape::rows(*definition, shapeDraft.has_value());
        for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(fields.size()); ++i) {
            float y = l.fieldY(i);
            auto const& row = fields[i];
            if (row.kind == shape::Row::Kind::Group) {
                label(context,dx,y+4,dw,translated(row.label),palette::faint);
                continue;
            }
            rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,shapeFieldSelected == i,over(ShapeZone::Field,i));
            label(context,dx,y+3,l.stepperX()-dx-4,translated(row.label),palette::dim);
            switch (row.kind) {
            case shape::Row::Kind::Switch:
                toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,definition->visible);
                break;
            case shape::Row::Kind::Button:
                drawSmallButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,translated(row.label),over(ShapeZone::Field,i));
                break;
            default: {
                if (row.field == shape::Field::Color) {
                    drawSwatchRow(context,l,y,4,static_cast<int>(definition->color),
                        [](int c) { return shapeRgb(static_cast<overlay::ShapeColor>(c)); });
                    break;
                }
                auto range = shape::numeric(*definition, row.field);
                std::string value;
                if (range) {
                    value = range->integer ? std::to_string(static_cast<long long>(range->value)) : std::format("{:.3g}", range->value);
                    if (row.field == shape::Field::X || row.field == shape::Field::Y || row.field == shape::Field::Z)
                        value = range->integer ? value : std::format("{:.3f}", range->value);
                    else value = translated("shape.blocks", value);
                } else value = translated(shape::choiceLabel(*definition, row.field, shapeReference));
                drawShapeStepper(context,l,y,range.has_value(),std::move(value),editingShapeField == i);
                break;
            }
            }
        }
        // Actions.
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        if (shapeDraft) {
            drawSmallButton(context,l.actionX(0),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.create"),
                over(ShapeZone::Action,0),palette::accentDeep,palette::accent);
            drawSmallButton(context,l.actionX(1),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.cancel"),over(ShapeZone::Action,1));
        } else {
            drawSmallButton(context,l.actionX(0),l.actionsY+2,ShapesLayout::actionWidth,12,translated("shape.duplicateShort"),over(ShapeZone::Action,0));
            drawSmallButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
                translated(shapeDeleteArmed ? "shape.deleteConfirm" : "shape.delete"),over(ShapeZone::Action,1),
                shapeDeleteArmed ? Rgb{.54f,.18f,.16f} : palette::keyFill,Rgb{.54f,.23f,.2f},
                shapeDeleteArmed ? palette::text : Rgb{1.f,.7f,.68f});
        }
    }

    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked || displayed.shortFooter;
    // Simple graphics draw faces poorly (additive, order dependent); say so
    // rather than tuning that mode further.
    bool simple = client && client->getOptions().getGraphicsMode() == GraphicsMode::Simple;
    bool warn = !error.empty() || simple;
    std::string text = !error.empty() ? error : simple && shortFooter ? translated("shape.simpleWarning") : shapeDescription(definition);
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),warn ? palette::warning : palette::text);
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,error.empty() ? palette::text : palette::warning);
        label(context,textLeft,l.footerTop+30,available,translated(simple ? "shape.simpleWarning"
            : editingShapeName || editingShapeField >= 0 ? "shape.numberHint" : "shape.hint"),simple ? palette::warning : palette::faint);
    }
}
std::optional<overlay::ShapeDefinition> visibleShape() {
    auto definition = currentShape();
    if (!definition && shapeSelected) shapeSelected.reset(); // Removed elsewhere.
    return definition;
}
void renderShapesContent(MinecraftUIRenderContext& context, IClientInstance&, glm::vec2 size, glm::vec2 pointer,
                         SettingsTable const& t) {
    auto definition = visibleShape();
    int fieldCount = shapePicking ? static_cast<int>(shape::types.size())
        : definition ? static_cast<int>(shape::rows(*definition, shapeDraft.has_value()).size()) : 0;
    auto l = ShapesLayout::fit(t, size.x, size.y, false, static_cast<int>(shapeList.size()) + (shapeDraft ? 1 : 0),
        shapeListFirst, fieldCount, shapeFieldFirst, shapeDraft.has_value(), shapePicking);
    shapesDisplayed = l;
    shapeListFirst = l.listFirst; shapeFieldFirst = l.fieldFirst;
    if (l.usable()) drawShapesBody(context, l, pointer, definition);
    context.flushText(0,std::nullopt);
}
void renderShapesDocked(MinecraftUIRenderContext& context, IClientInstance&, glm::vec2 size, glm::vec2 pointer) {
    auto definition = visibleShape();
    int fieldCount = shapePicking ? static_cast<int>(shape::types.size())
        : definition ? static_cast<int>(shape::rows(*definition, shapeDraft.has_value()).size()) : 0;
    auto l = ShapesLayout::fit(displayed, size.x, size.y, true, static_cast<int>(shapeList.size()) + (shapeDraft ? 1 : 0),
        shapeListFirst, fieldCount, shapeFieldFirst, shapeDraft.has_value(), shapePicking);
    shapesDisplayed = l;
    shapeListFirst = l.listFirst; shapeFieldFirst = l.fieldFirst;
    if (!l.usable()) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    // Docked: the world stays visible; only the panel is drawn.
    panel(context,l.left,l.top,l.width,l.height,.82f);
    frame(context,l.left,l.top,l.width,l.height,palette::white,.14f);
    label(context,l.left+ShapesLayout::pad,l.top+6,l.drawAllX-l.left-10,translated("nav.shapes"));
    bool closeHover = l.hit(pointer.x, pointer.y).zone == ShapeZone::Close;
    drawSmallButton(context,l.closeX,l.top+4,ShapesLayout::closeWidth,12,translated("closeButton"),closeHover,
        palette::keyFill,palette::keyEdge,closeHover ? palette::text : palette::dim);
    fill(context,l.left,l.top+ShapesLayout::headerHeight-1,l.width,1,palette::white,.14f);
    drawShapesBody(context, l, pointer, definition);
    context.flushText(0,std::nullopt);
}

// The action whose key cell gets the conflict tooltip: the hovered key cell,
// else the keyboard-selected row.
std::optional<std::pair<int, input::Action>> tipTarget(SettingsTable const& t, SettingsTable::Hit const& hover) {
    auto actionAt = [&](int row) -> std::optional<std::pair<int, input::Action>> {
        if (!valid(row) || row < t.first || row >= t.first + t.visible) return {};
        auto const& entry = rows[row];
        if (entry.kind == RowKind::Action) return std::pair{row, *entry.action};
        if (entry.kind == RowKind::Option)
            if (auto linked = optionAction(entry.option->id)) return std::pair{row, *linked};
        if (entry.kind == RowKind::Feature)
            if (auto primary = primaryAction(*entry.feature)) return std::pair{row, *primary};
        return {};
    };
    if (hover.zone == Zone::Row && hover.column == Column::Key)
        if (auto target = actionAt(hover.index)) return target;
    if (keyboardTip) return actionAt(selected);
    return {};
}
std::string linkTitle(input::Link link) {
    switch (link) {
    case input::Link::Same: return "tip.same";
    case input::Link::StartsWithThis: return "tip.starts";
    case input::Link::ContainsThis: return "tip.contains";
    case input::Link::InsideThis: return "tip.inside";
    default: return "tip.reordered";
    }
}
// Lists every binding related to the action's chord, grouped by how it
// relates, below the key cell (above it when there is more room there).
void drawConflictTip(MinecraftUIRenderContext& context, IClientInstance& current, SettingsTable const& t, int row,
                     input::Action action) {
    auto const preferences = Runtime::instance().preferences();
    auto const conflicts = input::bindingConflicts(preferences.bindings, action);
    if (conflicts.empty()) return;
    constexpr float pad = 4, headHeight = 15, lineHeight = 12, titleHeight = 13, itemHeight = 14;
    float width = std::min(240.0f, t.width - 2*SettingsTable::pad), inner = width - 2*pad;
    std::vector<std::string> notes;
    bool leads = std::any_of(conflicts.begin(), conflicts.end(), [](auto c) { return c.link == input::Link::StartsWithThis; });
    if (input::firesOnRelease(preferences.bindings, action)) notes.push_back(translated("tip.release"));
    else if (leads && input::actions[static_cast<size_t>(action)].behavior == input::Behavior::Hold)
        notes.push_back(translated("tip.holdLeads"));
    auto lines = [&](std::string const& text) { return textWidth(context, text) > inner ? size_t{2} : size_t{1}; };
    auto groupNote = [](input::Link link) { return translated(linkTitle(link) + "Note"); };
    float notesHeight = 0;
    for (auto const& note : notes) notesHeight += lines(note) * lineHeight;
    // Items that fit the budget; the rest are counted on a last line.
    auto fitting = [&](float budget, float& height) {
        height = 2*pad + headHeight + notesHeight;
        size_t shown = 0;
        for (; shown < conflicts.size(); ++shown) {
            bool group = shown == 0 || conflicts[shown].link != conflicts[shown-1].link;
            float need = (group ? titleHeight + lines(groupNote(conflicts[shown].link)) * lineHeight : 0) + itemHeight;
            float reserve = shown + 1 < conflicts.size() ? lineHeight : 0;
            if (height + need + reserve > budget) break;
            height += need;
        }
        if (shown < conflicts.size()) height += lineHeight;
        return shown;
    };
    float rowTop = t.rowY(row), rowBottom = rowTop + SettingsTable::rowHeight;
    float below = t.top + t.height - 2 - (rowBottom + 1), above = rowTop - 1 - (t.top + 2);
    float height = 0;
    size_t shown = fitting(std::numeric_limits<float>::infinity(), height);
    bool under = height <= below || (height > above && below >= above);
    if (height > (under ? below : above)) shown = fitting(under ? below : above, height);
    float x = std::max(t.left + 2, t.keyX + t.keyWidth - width);
    float y = under ? rowBottom + 1 : rowTop - 1 - height;

    // Row text is queued until a flush; flush it so the tooltip covers it.
    context.flushText(0, std::nullopt);
    fill(context,x,y,width,height,palette::panel,.97f);
    frame(context,x,y,width,height,palette::warning);
    float cursor = y + pad;
    auto relation = strongestConflict(conflicts);
    keycaps(context,x+pad,cursor,inner*.6f,chordKeys(current, input::effectiveChord(preferences.bindings, action)),
        conflictTone(relation));
    label(context,x+pad,cursor+boxTextInset(),inner,translated(relation == input::Relation::Shared ? "tip.countShared" : "tip.count",
        std::to_string(conflicts.size())),palette::warning,Align::Right);
    cursor += headHeight;
    for (auto const& note : notes) {
        paragraph(context,x+pad,cursor,inner,note,lines(note),palette::warning);
        cursor += lines(note) * lineHeight;
    }
    for (size_t i = 0; i < shown; ++i) {
        auto const& conflict = conflicts[i];
        if (i == 0 || conflict.link != conflicts[i-1].link) {
            fill(context,x+pad,cursor+1,inner,1,palette::white,.1f);
            label(context,x+pad,cursor+3,inner,translated(linkTitle(conflict.link)),palette::dim);
            auto note = groupNote(conflict.link);
            paragraph(context,x+pad,cursor+3+lineHeight,inner,note,lines(note),palette::faint);
            cursor += titleHeight + lines(note) * lineHeight;
        }
        float used = keycaps(context,x+pad,cursor+1,inner*.45f,
            chordKeys(current, input::effectiveChord(preferences.bindings, conflict.action)));
        label(context,x+pad+used+5,cursor+1+boxTextInset(),inner-used-5,actionLabel(conflict.action));
        cursor += itemHeight;
    }
    if (shown < conflicts.size())
        label(context,x+pad,cursor+1,inner,translated("tip.more", std::to_string(conflicts.size() - shown)),palette::faint);
}
// The full version line under the header version while it is hovered (L-101).
void drawVersionTip(MinecraftUIRenderContext& context, SettingsTable const& t, SettingsTable::Hit const& hover) {
    if (hover.zone != Zone::Version) return;
    constexpr float pad = 4, lineHeight = 12;
    auto line = runningVersionLine();
    auto hint = translated("version.copyHint");
    float width = std::min(t.width - 2*SettingsTable::pad, std::max(textWidth(context, line), textWidth(context, hint)) + 2*pad);
    float x = std::min(t.versionX, t.left + t.width - SettingsTable::pad - width), y = t.top + SettingsTable::headerHeight + 1;
    context.flushText(0, std::nullopt);
    fill(context,x,y,width,2*lineHeight+2*pad,palette::panel,.97f);
    frame(context,x,y,width,2*lineHeight+2*pad,palette::keyEdge);
    label(context,x+pad,y+pad,width-2*pad,std::move(line));
    label(context,x+pad,y+pad+lineHeight,width-2*pad,std::move(hint),palette::faint);
    context.flushText(0, std::nullopt);
}
void copyVersion() {
    showMessageToast(translated(copyText(runningVersionLine()) ? "version.copied" : "version.copyFailed"));
}
void renderTable(MinecraftUIRenderContext& context, IClientInstance& current, glm::vec2 size, glm::vec2 pointer) {
    auto t = SettingsTable::fit(size.x, size.y, static_cast<int>(rows.size()), first, navCount);
    t.placeVersion(textWidth(context, "Lamium"), textWidth(context, lamiumVersion()));
    displayed = t;
    if (sliderDrag && sliderDrag->numeric) setSlider(*sliderDrag, std::max(0.f, t.sliderFraction(std::min(pointer.x, t.sliderValueX() - 1))));
    first = t.first;
    fill(context,0,0,size.x,size.y,Rgb{0,0,0},.2f);
    if (!t.usable()) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    auto const preferences = Runtime::instance().preferences();
    auto hover = t.hit(pointer.x, pointer.y, navCount, displayedTabWidth);
    if (pointer != tipPointer) { keyboardTip = false; tipPointer = pointer; }
    panel(context,t.left,t.top,t.width,t.height,.8f);
    frame(context,t.left,t.top,t.width,t.height,palette::white,.14f);

    // Header: title, search field (table views), Close.
    // The sidebar or tabs show where you are; the header names only Lamium (L-101).
    label(context,t.left+SettingsTable::pad,t.top+6,80,"Lamium");
    if (t.versionWidth > 0)
        label(context,t.versionX,t.top+6,t.versionWidth+2,lamiumVersion(),hover.zone == Zone::Version ? palette::dim : palette::faint);
    if (!shapesView() && !waypointsView() && !schematicsView()) {
    fill(context,t.searchX,t.top+4,t.searchWidth,12,Rgb{0,0,0},.45f);
    frame(context,t.searchX,t.top+4,t.searchWidth,12,searchFocused ? palette::accent : palette::keyEdge);
    if (searchFocused && query.selectedAll() && !query.value().empty())
        fill(context,t.searchX+3,t.top+5,std::min(t.searchWidth-6,textWidth(context,query.value())),10,palette::accent,.35f);
    if (query.value().empty() && !searchFocused)
        label(context,t.searchX+4,t.top+5+boxTextInset(),t.searchWidth-8,translated("searchPlaceholder") + "  Ctrl+F",palette::faint);
    else label(context,t.searchX+4,t.top+5+boxTextInset(),t.searchWidth-8,query.value() + (searchFocused ? "_" : ""));
    }
    bool closeHover = hover.zone == Zone::Close;
    if (closeHover) fill(context,t.closeX,t.top+4,SettingsTable::closeWidth,12,palette::white,.07f);
    frame(context,t.closeX,t.top+4,SettingsTable::closeWidth,12,palette::keyEdge);
    label(context,t.closeX,t.top+5+boxTextInset(),SettingsTable::closeWidth,
        translated(waypointsView() && waypointsFromMap ? "worldMap.back" : "closeButton"),
        closeHover ? palette::text : palette::dim,Align::Center);
    fill(context,t.left,t.top+SettingsTable::headerHeight-1,t.width,1,palette::white,.14f);

    // Categories: sidebar, or tabs when narrow.
    // A query searches every category, so the navigation shows "All" meanwhile.
    bool searching = query.value().find_first_not_of(' ') != std::string::npos;
    int activeNav = searching && !hotkeysView() ? 0 : navigation.current;
    if (t.compact) {
        displayedTabWidth = (t.width - 4) / navCount;
        for (int i = 0; i < navCount; ++i) {
            float x = t.left + 2 + i * displayedTabWidth;
            bool active = i == activeNav, over = hover.zone == Zone::Nav && hover.index == i;
            if (over && !active) fill(context,x,t.navTop,displayedTabWidth,SettingsTable::tabsHeight,palette::white,.07f);
            if (active) fill(context,x+2,t.navBottom-2,displayedTabWidth-4,2,palette::accent);
            label(context,x+1,t.navTop+3,displayedTabWidth-2,navLabel(i,true),active || over ? palette::text : palette::dim,Align::Center);
        }
        fill(context,t.left,t.navBottom-1,t.width,1,palette::white,.14f);
    } else {
        displayedTabWidth = 0;
        fill(context,t.tableLeft-1,t.navTop,1,t.navBottom-t.navTop,palette::white,.14f);
        for (int i = 0; i < navCount; ++i) {
            bool pinned = i >= navCount - SettingsTable::pinnedItems;
            float y = pinned ? t.pinnedItemY(i - (navCount - SettingsTable::pinnedItems)) : t.navItemY(i);
            float x = t.left + 1, w = SettingsTable::sidebarWidth - 2;
            bool active = i == activeNav, over = hover.zone == Zone::Nav && hover.index == i;
            if (i == navCount - SettingsTable::pinnedItems) fill(context,x+6,y-4,w-12,1,palette::white,.14f);
            if (active) { fill(context,x,y,w,t.navStep,palette::accent,.16f); fill(context,x,y,2,t.navStep,palette::accent); }
            else if (over) fill(context,x,y,w,t.navStep,palette::white,.07f);
            std::string count = i > 0 && i < hotkeysNav ? sectionCount(sections[i-1], preferences)
                : i == shapesNav ? std::to_string(overlay::shapes::list().size())
                : i == waypointsNav ? std::to_string(map::waypoints::current().waypoints.size())
                : i == schematicsNav ? std::to_string(schematic::session::current().placements.size()) : std::string{};
            float countWidth = count.empty() ? 0 : textWidth(context, count) + 4;
            float ty = y + (t.navStep - 8) / 2;
            label(context,x+7,ty,w-12-countWidth,navLabel(i,false),active || over ? palette::text : palette::dim);
            if (!count.empty()) label(context,x+w-5-countWidth,ty,countWidth,count,palette::faint,Align::Right);
        }
    }

    if (shapesView() || waypointsView() || schematicsView()) {
        if (shapesView()) renderShapesContent(context, current, size, pointer, t);
        else if (waypointsView()) renderWaypointsContent(context, size, pointer, t);
        else renderSchematicsContent(context, size, pointer, t);
        drawVersionTip(context, t, hover);
        return;
    }

    // Column headings.
    float theadY = t.theadTop + 2;
    if (hotkeysView()) {
        label(context,t.nameX,theadY,t.keyX-t.nameX-SettingsTable::gap,translated("column.action"),palette::faint);
    } else {
        label(context,t.nameX,theadY,t.stateX-t.nameX-SettingsTable::gap,translated("column.feature"),palette::faint);
        label(context,t.stateX-6,theadY,SettingsTable::stateWidth+12,translated("column.state"),palette::faint,Align::Center);
    }
    label(context,t.keyX,theadY,t.keyWidth,translated("column.key"),palette::faint);
    if (auto scope = capturing ? ResetScope::None : resetScope(); scope != ResetScope::None) {
        bool keys = scope == ResetScope::Keys;
        drawSmallButton(context,t.headActionX(keys),t.theadTop,SettingsTable::headActionWidth,11,
            translated(resetArmed ? "reset.confirm" : keys ? "reset.keys"
                : scope == ResetScope::Section ? "reset.section" : "reset.all"),
            t.headAction(hover.x,hover.y,keys),
            resetArmed ? Rgb{.54f,.18f,.16f} : palette::keyFill,resetArmed ? Rgb{.54f,.23f,.2f} : palette::keyEdge,
            resetArmed ? palette::text : palette::dim);
    }
    fill(context,t.tableLeft,t.rowsTop-1,t.tableWidth,1,palette::white,.14f);

    // Rows.
    if (rows.empty())
        label(context,t.nameX,t.rowsTop+4,t.tableWidth-2*SettingsTable::pad,translated("noResultsFor",query.value()),palette::faint);
    for (int i = t.first; i < t.first + t.visible && valid(i); ++i) {
        float y = t.rowY(i), rowLeft = t.tableLeft + 1, rowWidth = t.tableWidth - 2;
        auto const& entry = rows[i];
        if (entry.kind == RowKind::Section) {
            label(context,t.nameX,y+4,t.tableWidth-2*SettingsTable::pad,translated(entry.section),palette::accent);
            fill(context,rowLeft,y+SettingsTable::rowHeight-1,rowWidth,1,palette::accent,.3f);
            continue;
        }
        if (i % 2) fill(context,rowLeft,y,rowWidth,SettingsTable::rowHeight,palette::white,.025f);
        if (entry.heading() && entry.expanded) fill(context,rowLeft,y,rowWidth,SettingsTable::rowHeight,palette::white,.05f);
        rowBackground(context,rowLeft,y,rowWidth,SettingsTable::rowHeight,selected == i,hover.zone == Zone::Row && hover.index == i);
        float nameRight = t.stateX - SettingsTable::gap;
        switch (entry.kind) {
        case RowKind::Feature: {
            if (entry.children) chevron(context,t.nameX,y+5,entry.expanded);
            drawName(context,t.nameX+9,y,nameRight,featureName(*entry.feature),palette::text,entry.children,entry.feature->experimental);
            if (auto option = settings::find(entry.feature->toggle))
                toggleSwitch(context,t.stateX+(SettingsTable::stateWidth-switchWidth)/2,y+(SettingsTable::rowHeight-switchHeight)/2,
                    std::get<bool>(option->read(preferences)));
            else if (isSessionFeature(entry.feature->id))
                toggleSwitch(context,t.stateX+(SettingsTable::stateWidth-switchWidth)/2,y+(SettingsTable::rowHeight-switchHeight)/2,
                    sessionState(entry.feature->id));
            if (auto primary = primaryAction(*entry.feature)) drawKeyCell(context,current,y,*primary);
            break;
        }
        case RowKind::Option: {
            drawGuide(context,y,entry.lastChild);
            auto value = entry.option->read(preferences);
            auto name = splitLabel(translated(entry.option->label)).name;
            bool paused = effectsPaused(entry.feature->id,preferences);
            if (paused) name += " (" + translated("effectsPaused") + ")";
            if (auto flag = std::get_if<bool>(&value)) {
                label(context,t.nameX+12,y+3,nameRight-t.nameX-12,std::move(name),palette::dim);
                toggleSwitch(context,t.stateX+(SettingsTable::stateWidth-switchWidth)/2,y+(SettingsTable::rowHeight-switchHeight)/2,*flag);
            } else {
                bool asSlider = entry.option->numeric && entry.option->numeric->step > 0 && editingNumber != entry.option;
                float nameEnd = asSlider ? t.sliderX() : t.stepperX(optionAction(entry.option->id).has_value());
                label(context,t.nameX+12,y+3,nameEnd-SettingsTable::gap-t.nameX-12,std::move(name),palette::dim);
                if (asSlider) {
                    auto const& range = *entry.option->numeric;
                    float number = std::get<float>(value);
                    slider(context,t.sliderX(),y+2,t.sliderWidth(),
                        SettingsTable::sliderPosition(number,range.minimum,range.maximum),sliderDrag == entry.option);
                    label(context,t.sliderValueX(),y+3,SettingsTable::sliderValueWidth,optionValueText(*entry.option,value),
                        palette::text,Align::Right);
                } else {
                    drawStepper(context,y,*entry.option,optionValueText(*entry.option,value),editingNumber == entry.option,
                        optionWarning(entry.option->id,preferences).has_value());
                }
            }
            if (auto linked = optionAction(entry.option->id)) drawKeyCell(context,current,y,*linked);
            if (paused) fill(context,t.nameX+12,y,t.rowsRight()-t.nameX-12,SettingsTable::rowHeight,palette::panel,.4f);
            break;
        }
        case RowKind::Action: {
            if (hotkeysView()) {
                drawName(context,t.nameX,y,t.keyX-SettingsTable::gap,actionLabel(*entry.action),palette::text,0,entry.feature->experimental);
            } else {
                drawGuide(context,y,entry.lastChild);
                label(context,t.nameX+12,y+3,nameRight-t.nameX-12,actionLabel(*entry.action,true),palette::dim);
            }
            drawKeyCell(context,current,y,*entry.action);
            break;
        }
        case RowKind::Layout: {
            drawGuide(context,y,entry.lastChild);
            label(context,t.nameX+12,y+3,nameRight-t.nameX-12,translated(layoutLinkLabel(*entry.layout)),palette::dim);
            label(context,t.stateX,y+3,t.controlWidth(),translated("layoutLinkValue"),palette::accent,Align::Right);
            break;
        }
        case RowKind::MapCache: {
            drawGuide(context,y,entry.lastChild);
            auto bytes = map::store::usage();
            mapCacheButtonWidth = 0;
            auto name = translated("mapCache") + "  " + (bytes ? std::format("{:.1f} MB", *bytes / 1048576.0) : translated("mapCacheNone"));
            label(context,t.nameX+12,y+3,nameRight-t.nameX-12,std::move(name),palette::dim);
            if (bytes) {
                auto text = translated(mapCacheArmed ? "mapCacheArmed" : "mapCacheClear");
                float w = textWidth(context,text) + 8, bx = t.stateX + t.controlWidth() - w;
                mapCacheButtonX = bx;
                mapCacheButtonWidth = w;
                fill(context,bx,y+2,w,capHeight,mapCacheArmed ? Rgb{.54f,.18f,.16f} : Rgb{.23f,.15f,.14f});
                frame(context,bx,y+2,w,capHeight,Rgb{.54f,.23f,.2f});
                label(context,bx,y+2+boxTextInset(),w,std::move(text),mapCacheArmed ? palette::text : Rgb{1.f,.7f,.68f},Align::Center);
            }
            break;
        }
        default: break;
        }
    }
    if (static_cast<int>(rows.size()) > t.visible) {
        float track = t.visible * SettingsTable::rowHeight;
        float thumb = std::max(8.0f, track * t.visible / rows.size());
        float thumbY = t.rowsTop + (track - thumb) * t.first / (rows.size() - t.visible);
        fill(context,t.rowsRight()-3,t.rowsTop,2,track,palette::white,.08f);
        fill(context,t.rowsRight()-3,thumbY,2,thumb,palette::keyEdge);
    }

    // Footer: description and hints, or the binding editor's buttons.
    fill(context,t.left,t.footerTop,t.width,1,palette::white,.14f);
    float textLeft = t.left + SettingsTable::pad, textWidthAvailable = t.width - 2*SettingsTable::pad;
    if (capturing) {
        std::vector<std::string> names{translated("resetShort"), translated("cancelShort")};
        if (input::canClear(*capturing)) names.insert(names.begin(), translated("clearShort"));
        for (size_t i = 0; i < names.size(); ++i) {
            float x = t.footerButtonX(static_cast<int>(i)), y = t.footerButtonY();
            bool over = hover.zone == Zone::Footer && t.footerButton(hover.x, hover.y) == static_cast<int>(i);
            fill(context,x,y,SettingsTable::footerButtonWidth,SettingsTable::footerButtonHeight,over ? Rgb{.23f,.23f,.24f} : palette::keyFill);
            frame(context,x,y,SettingsTable::footerButtonWidth,SettingsTable::footerButtonHeight,palette::keyEdge);
            label(context,x,y+boxTextInset(),SettingsTable::footerButtonWidth,names[i],palette::text,Align::Center);
        }
        float after = t.footerButtonX(3);
        label(context,after,t.footerButtonY()+1,t.left+t.width-SettingsTable::pad-after,
            error.empty() ? description() : error,error.empty() ? palette::dim : palette::warning);
        if (!t.shortFooter) label(context,textLeft,t.footerTop+30,textWidthAvailable,translated("captureInline"),palette::faint);
    } else if (t.shortFooter) {
        label(context,textLeft,t.footerTop+3,textWidthAvailable,error.empty() ? description() : error,
            error.empty() ? palette::text : palette::warning);
    } else {
        bool warns = valid(selected) && rows[selected].option
            && optionWarning(rows[selected].option->id,preferences).has_value();
        paragraph(context,textLeft,t.footerTop+3,textWidthAvailable,description(),2,warns ? palette::warning : palette::text);
        std::string hint = !error.empty() ? error : translated(searchFocused ? "searchHint" : editingNumber ? "numberHint" : "tableHint");
        label(context,textLeft,t.footerTop+30,textWidthAvailable,std::move(hint),error.empty() ? palette::faint : palette::warning);
    }
    if (!capturing)
        if (auto target = tipTarget(t, hover)) drawConflictTip(context,current,t,target->first,target->second);
    drawVersionTip(context, t, hover);
    context.flushText(0,std::nullopt);
}

// ---- Waypoints view ----
void refreshWaypoints() {
    waypointSet = map::waypoints::current();
    double x = 0, z = 0;
    if (auto* player = client ? client->getLocalPlayer() : nullptr) {
        auto feet = player->getFeetPos();
        x = feet.x;
        z = feet.z;
    }
    waypointList = map::waypointOrder(waypointSet.waypoints, playerDimension(), x, z);
    if (waypointSelected >= static_cast<int>(waypointSet.waypoints.size()) || (waypointSelected == -1 && !waypointSet.death))
        waypointSelected = -2;
}
int waypointRowCount() { return static_cast<int>(waypointList.size()) + (waypointSet.death ? 1 : 0); }
// The selection a list row stands for: -1 the death point, else an index.
int waypointAtRow(int row) {
    if (waypointSet.death) {
        if (row == 0) return -1;
        --row;
    }
    return row >= 0 && row < static_cast<int>(waypointList.size()) ? static_cast<int>(waypointList[static_cast<size_t>(row)]) : -2;
}
map::Waypoint const* selectedWaypoint() {
    return waypointSelected >= 0 && waypointSelected < static_cast<int>(waypointSet.waypoints.size())
        ? &waypointSet.waypoints[static_cast<size_t>(waypointSelected)] : nullptr;
}
void selectWaypoint(int value) {
    finishNumber();
    waypointSelected = value;
    waypointFieldFirst = 0;
    waypointFieldSelected = -1;
    waypointDeleteArmed = false;
}
void applyWaypointName() {
    if (!editingWaypointName || !std::exchange(waypointNameDirty, false)) return;
    int index = waypointSelected;
    auto name = waypointNameInput.value();
    if (name.find_first_not_of(' ') == std::string::npos) return; // An empty name keeps the old one.
    if (!map::waypoints::change([&](map::WaypointSet& set) {
            if (index < 0 || index >= static_cast<int>(set.waypoints.size())) return false;
            set.waypoints[static_cast<size_t>(index)].name = name;
            return true;
        })) error = translated("waypoint.saveError");
    refreshWaypoints();
}
struct Place { int x, y, z, dimension; };
std::optional<Place> standingPlace() {
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return std::nullopt;
    auto feet = player->getFeetPos();
    if (!std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z)) return std::nullopt;
    return Place{static_cast<int>(std::floor(feet.x)), static_cast<int>(std::floor(feet.y)),
                 static_cast<int>(std::floor(feet.z)), playerDimension()};
}
void addWaypointHere() {
    auto place = standingPlace();
    if (!place) return;
    map::Waypoint w;
    w.x = place->x; w.y = place->y; w.z = place->z; w.dimension = place->dimension;
    w.color = map::nextColor(waypointSet.lastColor);
    w.name = map::defaultWaypointName(waypointSet.waypoints, [](int n) { return translated("waypoint.defaultName", n); });
    if (map::waypoints::add(w)) {
        refreshWaypoints();
        selectWaypoint(static_cast<int>(waypointSet.waypoints.size()) - 1);
        error.clear();
    } else error = translated("waypoint.saveError");
}
// A change to the selected waypoint; reports only a failed save.
void changeSelected(std::function<void(map::Waypoint&)> const& apply) {
    int index = waypointSelected;
    if (!map::waypoints::change([&](map::WaypointSet& set) {
            if (index < 0 || index >= static_cast<int>(set.waypoints.size())) return false;
            apply(set.waypoints[static_cast<size_t>(index)]);
            return true;
        })) error = translated("waypoint.saveError");
    else error.clear();
    refreshWaypoints();
}
// Click or key activation of an editor row. part: -1/1 step, 0 value, 2 label.
void activateWaypointField(int index, int part) {
    auto const* w = selectedWaypoint();
    if (!w || index < 0 || index >= static_cast<int>(map::waypointFields.size())) return;
    waypointFieldSelected = index;
    if (part == 2) return;
    int direction = part == -1 ? -1 : 1;
    switch (map::waypointFields[static_cast<size_t>(index)]) {
    case map::WaypointField::X: case map::WaypointField::Y: case map::WaypointField::Z: {
        int value = index == 0 ? w->x : index == 1 ? w->y : w->z;
        if (part == 0) {
            editingWaypointField = index;
            numberInput.beginPrecise(value);
            error.clear();
            return;
        }
        changeSelected([&](map::Waypoint& t) {
            int& c = index == 0 ? t.x : index == 1 ? t.y : t.z;
            c = std::clamp(c + direction, -map::coordinateLimit, map::coordinateLimit);
        });
        return;
    }
    case map::WaypointField::MoveHere:
        if (auto place = standingPlace())
            changeSelected([&](map::Waypoint& t) { t.x = place->x; t.y = place->y; t.z = place->z; t.dimension = place->dimension; });
        return;
    case map::WaypointField::Visible: changeSelected([](map::Waypoint& t) { t.visible = !t.visible; }); return;
    case map::WaypointField::Color: {
        int count = static_cast<int>(map::waypointColors.size());
        changeSelected([&](map::Waypoint& t) { t.color = (map::clampColor(t.color) + direction + count) % count; });
        return;
    }
    }
}
void moveWaypointField(int step) {
    if (!selectedWaypoint()) return;
    int count = static_cast<int>(map::waypointFields.size());
    waypointFieldSelected = std::clamp(waypointFieldSelected + step, 0, count - 1);
    int visible = waypointsDisplayed.fieldVisible;
    if (visible > 0) {
        if (waypointFieldSelected < waypointFieldFirst) waypointFieldFirst = waypointFieldSelected;
        if (waypointFieldSelected >= waypointFieldFirst + visible) waypointFieldFirst = waypointFieldSelected - visible + 1;
    }
}
void openWaypointKeySettings() {
    int category = 0;
    for (size_t i = 0; i < sections.size(); ++i) if (sections[i] == featureSection("waypoints")) category = static_cast<int>(i) + 1;
    expanded.insert("waypoints");
    selectNav(category);
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].heading() && rows[i].feature->id == "waypoints") { selected = static_cast<int>(i); break; }
    first = SettingsTable::reveal(first, selected, displayed.visible);
}
void deleteSelectedWaypoint() {
    int selection = waypointSelected;
    int row = 0;
    for (int i = 0; i < waypointRowCount(); ++i) if (waypointAtRow(i) == selection) row = i;
    bool saved = map::waypoints::change([&](map::WaypointSet& set) {
        if (selection == -1) { set.death.reset(); return true; }
        if (selection < 0 || selection >= static_cast<int>(set.waypoints.size())) return false;
        set.waypoints.erase(set.waypoints.begin() + selection);
        return true;
    });
    if (!saved) { error = translated("waypoint.saveError"); return; }
    error.clear();
    refreshWaypoints();
    int rowsLeft = waypointRowCount();
    selectWaypoint(rowsLeft ? waypointAtRow(std::min(row, rowsLeft - 1)) : -2);
}
void keepDeathPoint() {
    if (!waypointSet.death) return;
    auto death = *waypointSet.death;
    map::Waypoint w{translated("waypoint.deathName"), map::nextColor(waypointSet.lastColor), death.x, death.y, death.z,
                    death.dimension, true};
    bool saved = map::waypoints::change([&](map::WaypointSet& set) {
        set.waypoints.push_back(w);
        set.lastColor = w.color;
        set.death.reset();
        return true;
    });
    if (!saved) { error = translated("waypoint.saveError"); return; }
    error.clear();
    refreshWaypoints();
    selectWaypoint(static_cast<int>(waypointSet.waypoints.size()) - 1);
}
void handleWaypointClick(float x, float y, bool right) {
    finishNumber();
    if (!right && pressScrollbar(waypointsDisplayed, waypointListFirst, x, y)) return;
    if (!waypointsDocked) {
        auto nav = displayed.hit(x, y, navCount, displayedTabWidth);
        if (nav.zone == Zone::Nav) { selectNav(nav.index); return; }
        if (nav.zone == Zone::Version) { copyVersion(); return; }
    }
    auto hit = waypointsDisplayed.hit(x, y);
    if (!(hit.zone == ShapeZone::Action && hit.index == 1)) waypointDeleteArmed = false;
    switch (hit.zone) {
    case ShapeZone::Close: if (waypointsFromMap) enterWorldMap(mapFromSettings, true); else close(); return;
    case ShapeZone::Dock: waypointsDocked = !waypointsDocked; return;
    case ShapeZone::Keys: openWaypointKeySettings(); return;
    case ShapeZone::DrawAll:
        if (auto option = settings::find("map.waypoints")) adjustOption(*option, 1);
        return;
    case ShapeZone::NewShape: addWaypointHere(); return;
    case ShapeZone::ListRow: {
        int value = waypointAtRow(hit.index);
        auto const& l = waypointsDisplayed;
        if (value >= 0 && x >= l.listLeft + l.listWidth - ShapesLayout::pad - switchWidth - 2) {
            int keep = waypointSelected;
            waypointSelected = value;
            changeSelected([](map::Waypoint& t) { t.visible = !t.visible; });
            waypointSelected = keep;
            return;
        }
        if (value != waypointSelected) selectWaypoint(value);
        return;
    }
    case ShapeZone::Name:
        if (auto const* w = selectedWaypoint()) {
            editingWaypointName = true;
            waypointNameInput.clear();
            waypointNameInput.append(w->name);
            waypointNameInput.selectAll();
            error.clear();
        }
        return;
    case ShapeZone::Field:
        if (!right && hit.part != 2 && hit.index >= 0 && hit.index < static_cast<int>(map::waypointFields.size())
            && map::waypointFields[static_cast<size_t>(hit.index)] == map::WaypointField::Color) {
            waypointFieldSelected = hit.index;
            int count = static_cast<int>(map::waypointColors.size());
            if (int swatch = waypointsDisplayed.swatchAt(x, count); swatch >= 0)
                changeSelected([&](map::Waypoint& t) { t.color = swatch; });
            return;
        }
        activateWaypointField(hit.index, right ? -1 : hit.part);
        return;
    case ShapeZone::Action:
        if (hit.index == 0) { if (waypointSelected == -1) keepDeathPoint(); return; }
        if (waypointSelected == -2) return;
        if (!waypointDeleteArmed) { waypointDeleteArmed = true; return; }
        deleteSelectedWaypoint();
        return;
    default: return;
    }
}
void handleSchematicKey(int key);
void handleSchematicClick(float x, float y, bool right);
void renderSchematicsDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer);
void handleWaypointKey(int key) {
    if (editingWaypointName) {
        switch (key) {
        case 0x08: if (waypointNameInput.backspace()) waypointNameDirty = true; break;
        case 0x41: if (heldCtrl()) waypointNameInput.selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: finishNumber(); break;
        }
        return;
    }
    if (editingWaypointField >= 0) {
        switch (key) {
        case 0x08: if (numberInput.backspace()) numberDirty = true; break;
        case 0x41: if (heldCtrl()) numberInput.selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: finishNumber(); break;
        }
        return;
    }
    switch (key) {
    case 0x1b: if (waypointsFromMap) enterWorldMap(mapFromSettings, true); else close(); break;
    case 0x26: moveWaypointField(-1); break;
    case 0x28: moveWaypointField(1); break;
    case 0x25: activateWaypointField(waypointFieldSelected, -1); break;
    case 0x27: activateWaypointField(waypointFieldSelected, 1); break;
    case 0x0d: case 0x20: activateWaypointField(waypointFieldSelected, 0); break;
    case 0x21: case 0x22: {
        int count = waypointRowCount();
        if (!count) break;
        int row = 0;
        for (int i = 0; i < count; ++i) if (waypointAtRow(i) == waypointSelected) row = i;
        row = std::clamp(row + (key == 0x22 ? 1 : -1), 0, count - 1);
        selectWaypoint(waypointAtRow(row));
        break;
    }
    case 0x09: selectNav((navigation.current + (heldShift() ? worldMapNav - 1 : 1)) % worldMapNav); break;
    }
}
Rgb waypointRgb(int color) {
    auto c = map::waypointColors[static_cast<size_t>(map::clampColor(color))];
    return {map::channel(c, 0) / 255.f, map::channel(c, 1) / 255.f, map::channel(c, 2) / 255.f};
}
constexpr Rgb deathRgb{230 / 255.f, 46 / 255.f, 46 / 255.f};
// Small pixel glyphs for list rows and the editor.
void drawDiamondGlyph(MinecraftUIRenderContext& context, float cx, float cy, int size, Rgb color, float opacity = 1) {
    auto rows = map::diamondRows(size);
    float top = cy - static_cast<float>(rows.size()) / 2;
    for (size_t i = 0; i < rows.size(); ++i)
        fill(context, cx - rows[i] - .5f, top + i, 2.f * rows[i] + 1, 1, color, opacity);
}
void drawCrossGlyph(MinecraftUIRenderContext& context, float cx, float cy, Rgb color) {
    for (int i = -2; i <= 2; ++i) {
        fill(context, cx + i - .5f, cy + i - .5f, 1, 1, color);
        fill(context, cx + i - .5f, cy - i - .5f, 1, 1, color);
    }
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
std::string waypointDescription() {
    if (waypointSelected == -1 && waypointSet.death) return translated("waypoint.deathNote");
    auto const* w = selectedWaypoint();
    if (!w) return translated(waypointSet.waypoints.empty() && !waypointSet.death ? "waypoint.empty" : "waypoint.selectHint");
    if (editingWaypointField >= 0) return translated("integerRange", -map::coordinateLimit, map::coordinateLimit);
    if (w->dimension != playerDimension()) return w->name + ": " + translated("waypoint.elsewhere");
    return w->name + ": " + translated(w->visible ? "waypoint.shownState" : "waypoint.hiddenState");
}
void drawWaypointsBody(MinecraftUIRenderContext& context, ShapesLayout const& l, glm::vec2 pointer) {
    auto const preferences = Runtime::instance().preferences();
    auto hover = l.hit(pointer.x, pointer.y);
    auto over = [&](ShapeZone zone, int index = -1) { return hover.zone == zone && (index < 0 || hover.index == index); };
    float top = l.top + 4;
    label(context,l.drawAllX,l.drawAllY+1+boxTextInset(),l.drawAllWidth-switchWidth-4,translated("waypoint.showAll"),
        over(ShapeZone::DrawAll) ? palette::text : palette::dim,Align::Right);
    toggleSwitch(context,l.drawAllX+l.drawAllWidth-switchWidth,l.drawAllY+2,preferences.map.waypoints);
    label(context,l.keysX,top+1+boxTextInset(),ShapesLayout::keysWidth,translated("shape.keys"),
        over(ShapeZone::Keys) ? palette::text : palette::accent,Align::Center);
    fill(context,l.keysX+6,top+11,ShapesLayout::keysWidth-12,1,palette::accent,over(ShapeZone::Keys) ? 1.f : .5f);
    drawSmallButton(context,l.dockX,top,ShapesLayout::dockWidth,12,translated(waypointsDocked ? "shape.undock" : "shape.dock"),over(ShapeZone::Dock));

    // List pane.
    float listRight = l.listLeft + l.listWidth;
    drawSmallButton(context,l.listLeft+ShapesLayout::pad,l.toolbarTop+2,ShapesLayout::newWidth,12,translated("waypoint.addHere"),
        over(ShapeZone::NewShape),palette::accentDeep,palette::accent);
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);
    float nameX = l.listLeft + ShapesLayout::pad + 14;
    float shownX = listRight - ShapesLayout::pad - switchWidth - 2;
    float distanceX = shownX - 56;
    label(context,nameX,l.theadTop+2,distanceX-nameX-4,translated("shape.columnName"),palette::faint);
    label(context,distanceX,l.theadTop+2,52,translated("waypoint.columnDistance"),palette::faint,Align::Right);
    label(context,shownX-6,l.theadTop+2,switchWidth+12,translated("shape.columnShown"),palette::faint,Align::Center);
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    if (l.listCount == 0)
        paragraph(context,l.listLeft+ShapesLayout::pad,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,translated("waypoint.empty"),3,palette::faint);
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        int value = waypointAtRow(i);
        if (i % 2) fill(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,palette::white,.025f);
        rowBackground(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,value == waypointSelected,over(ShapeZone::ListRow,i));
        float gx = l.listLeft + ShapesLayout::pad + 4, gy = y + ShapesLayout::rowHeight / 2;
        if (value == -1) {
            auto const& d = *waypointSet.death;
            bool here = d.dimension == playerDimension();
            drawCrossGlyph(context, gx, gy, deathRgb);
            label(context,nameX,y+3,distanceX-nameX-4,translated("waypoint.death"),here ? palette::text : palette::faint);
            label(context,distanceX,y+3,52,here ? translated("waypoint.meters", distanceTo(d.x, d.z)) : dimensionName(d.dimension),
                palette::dim,Align::Right);
            continue;
        }
        if (value < 0) continue;
        auto const& w = waypointSet.waypoints[static_cast<size_t>(value)];
        bool here = w.dimension == playerDimension();
        drawDiamondGlyph(context, gx, gy, 7, waypointRgb(w.color), w.visible && here ? 1.f : .35f);
        label(context,nameX,y+3,distanceX-nameX-4,w.name,here ? palette::text : palette::faint);
        label(context,distanceX,y+3,52,here ? translated("waypoint.meters", distanceTo(w.x, w.z)) : dimensionName(w.dimension),
            palette::dim,Align::Right);
        toggleSwitch(context,shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,w.visible);
    }
    if (l.listCount > l.listVisible) {
        float track = l.listVisible * ShapesLayout::rowHeight;
        float thumb = std::max(8.0f, track * l.listVisible / l.listCount);
        float thumbY = l.rowsTop + (track - thumb) * l.listFirst / (l.listCount - l.listVisible);
        fill(context,listRight-3,l.rowsTop,2,track,palette::white,.08f);
        fill(context,listRight-3,thumbY,2,thumb,palette::keyEdge);
    }
    if (l.docked) fill(context,l.left,l.detailTop-1,l.width,1,palette::white,.14f);
    else fill(context,l.detailLeft-1,l.toolbarTop,1,l.footerTop-l.toolbarTop,palette::white,.14f);

    // Editor pane.
    float dx = l.detailLeft + ShapesLayout::pad, dw = l.detailWidth - 2 * ShapesLayout::pad;
    auto const* w = selectedWaypoint();
    if (waypointSelected == -1 && waypointSet.death) {
        auto const& d = *waypointSet.death;
        drawCrossGlyph(context, dx + 5, l.nameY + 7, deathRgb);
        label(context,dx+14,l.nameY+1+boxTextInset(),dw-14,translated("waypoint.death"));
        label(context,dx,l.previewY+1,dw,std::format("{}, {}, {}", d.x, d.y, d.z));
        label(context,dx,l.previewY+12,dw,dimensionName(d.dimension),palette::dim);
        if (d.dimension == playerDimension())
            label(context,dx,l.previewY+23,dw,translated("waypoint.distance", distanceTo(d.x, d.z)),palette::dim);
        paragraph(context,dx,l.previewY+36,dw,translated("waypoint.deathNote"),3,palette::faint);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawSmallButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated("waypoint.keep"),
            over(ShapeZone::Action,0),palette::accentDeep,palette::accent);
    } else if (!w) {
        paragraph(context,dx,l.detailTop+6,dw,translated(waypointSet.waypoints.empty() && !waypointSet.death ? "waypoint.empty"
            : "waypoint.selectHint"),3,palette::faint);
    } else {
        // Name field with the waypoint's diamond.
        fill(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,Rgb{0,0,0},.4f);
        frame(context,dx,l.nameY,dw,ShapesLayout::rowHeight-1,editingWaypointName ? palette::accent : palette::keyEdge);
        drawDiamondGlyph(context, dx + 7, l.nameY + 6.5f, 7, waypointRgb(w->color));
        if (editingWaypointName) drawEditText(context,dx+17,l.nameY,ShapesLayout::rowHeight-1,dw-20,waypointNameInput);
        else label(context,dx+17,l.nameY+1+boxTextInset(),dw-20,w->name);
        // The preview area: a large diamond and where the waypoint is.
        float size = ShapesLayout::previewSize;
        fill(context,dx,l.previewY,size,size,Rgb{0,0,0},.35f);
        frame(context,dx,l.previewY,size,size,palette::white,.14f);
        drawDiamondGlyph(context, dx + size / 2, l.previewY + size / 2, 31, Rgb{0,0,0}, .8f);
        drawDiamondGlyph(context, dx + size / 2, l.previewY + size / 2, 25, waypointRgb(w->color));
        float ix = dx + size + 8, iw = dw - size - 8;
        label(context,ix,l.previewY+1,iw,std::format("{}, {}, {}", w->x, w->y, w->z));
        label(context,ix,l.previewY+12,iw,dimensionName(w->dimension),palette::dim);
        if (w->dimension == playerDimension())
            label(context,ix,l.previewY+23,iw,translated("waypoint.distance", distanceTo(w->x, w->z)),palette::dim);
        // Fields.
        for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(map::waypointFields.size()); ++i) {
            float y = l.fieldY(i);
            auto field = map::waypointFields[static_cast<size_t>(i)];
            rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,waypointFieldSelected == i,over(ShapeZone::Field,i));
            static constexpr std::array<std::string_view, 6> labels{"X", "Y", "Z", "waypoint.moveHere", "waypoint.shown", "waypoint.color"};
            std::string text = i < 3 ? std::string(labels[static_cast<size_t>(i)]) : translated(labels[static_cast<size_t>(i)]);
            label(context,dx,y+3,l.stepperX()-dx-4,text,palette::dim);
            switch (field) {
            case map::WaypointField::Visible:
                toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,w->visible);
                break;
            case map::WaypointField::MoveHere:
                drawSmallButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,translated("waypoint.moveHere"),
                    over(ShapeZone::Field,i));
                break;
            case map::WaypointField::Color:
                drawSwatchRow(context,l,y,static_cast<int>(map::waypointColors.size()),map::clampColor(w->color),
                    [](int c) { return waypointRgb(c); });
                break;
            default: {
                int value = i == 0 ? w->x : i == 1 ? w->y : w->z;
                drawShapeStepper(context,l,y,true,std::to_string(value),editingWaypointField == i);
                break;
            }
            }
        }
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
    }
    if (waypointSelected != -2) {
        drawSmallButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
            translated(waypointDeleteArmed ? "shape.deleteConfirm" : "shape.delete"),over(ShapeZone::Action,1),
            waypointDeleteArmed ? Rgb{.54f,.18f,.16f} : palette::keyFill,Rgb{.54f,.23f,.2f},
            waypointDeleteArmed ? palette::text : Rgb{1.f,.7f,.68f});
    }

    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked || displayed.shortFooter;
    std::string text = !error.empty() ? error : waypointDescription();
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),error.empty() ? palette::text : palette::warning);
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,error.empty() ? palette::text : palette::warning);
        label(context,textLeft,l.footerTop+30,available,translated(editingWaypointName || editingWaypointField >= 0
            ? "shape.numberHint" : "waypoint.screenHint"),palette::faint);
    }
}
ShapesLayout fitWaypoints(SettingsTable const& t, glm::vec2 size, bool docked) {
    int fieldCount = selectedWaypoint() ? static_cast<int>(map::waypointFields.size()) : 0;
    auto l = ShapesLayout::fit(t, size.x, size.y, docked, waypointRowCount(), waypointListFirst, fieldCount,
        waypointFieldFirst, false, false);
    // Keeping the death point needs a wider first button.
    if (waypointSelected == -1) l.firstActionWidth = 110;
    waypointsDisplayed = l;
    waypointListFirst = l.listFirst;
    waypointFieldFirst = l.fieldFirst;
    return l;
}
void renderWaypointsContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& t) {
    auto l = fitWaypoints(t, size, false);
    if (l.width > 0) drawWaypointsBody(context, l, pointer);
    context.flushText(0,std::nullopt);
}
void renderWaypointsDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto l = fitWaypoints(displayed, size, true);
    if (l.width <= 0) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    panel(context,l.left,l.top,l.width,l.height,.82f);
    frame(context,l.left,l.top,l.width,l.height,palette::white,.14f);
    label(context,l.left+ShapesLayout::pad,l.top+6,l.drawAllX-l.left-10,translated("nav.waypoints"));
    bool closeHover = l.hit(pointer.x, pointer.y).zone == ShapeZone::Close;
    drawSmallButton(context,l.closeX,l.top+4,ShapesLayout::closeWidth,12,translated(waypointsFromMap ? "worldMap.back" : "closeButton"),
        closeHover,palette::keyFill,palette::keyEdge,closeHover ? palette::text : palette::dim);
    fill(context,l.left,l.top+ShapesLayout::headerHeight-1,l.width,1,palette::white,.14f);
    drawWaypointsBody(context, l, pointer);
    context.flushText(0,std::nullopt);
}

// ---- Schematics view (L-93) ----
// Four tabs share the list-and-detail layout: placements, files, the
// verification of the selected placement, and its materials.
enum class SchematicField { X, Y, Z, Rotation, Mirror, MoveHere, Visible, LayerAxis, LayerMode, Layer, MatchLayer, Extras, Entities };
constexpr auto schematicFields = std::to_array<SchematicField>({SchematicField::X, SchematicField::Y, SchematicField::Z,
    SchematicField::Rotation, SchematicField::Mirror, SchematicField::MoveHere, SchematicField::Visible, SchematicField::LayerAxis,
    SchematicField::LayerMode, SchematicField::Layer, SchematicField::MatchLayer, SchematicField::Extras, SchematicField::Entities});
int verifyFilter = 0; // 0 mistakes, 1 wrong or extra, 2 wrong state, 3 not placed
int verifySelected = -1;
bool materialsShownOnly = false;
std::shared_ptr<schematic::Verification const> verification;
std::vector<schematic::Mismatch const*> verifyRows;
std::vector<schematic::MaterialLine const*> materialRows; // blocks first, then entities
// The Materials list: -1 a "Blocks" heading, -2 an "Entities" heading, else an
// index into materialRows. Headings only when both kinds are present.
std::vector<int> materialListRows;
int materialSelected = -1; // index into materialRows

bool verifyMatches(schematic::Mismatch const& m) {
    using schematic::CellState;
    switch (verifyFilter) {
    case 1: return m.state == CellState::Wrong || m.state == CellState::Extra;
    case 2: return m.state == CellState::State;
    case 3: return m.state == CellState::Missing;
    default: return m.state != CellState::Missing;
    }
}
void refreshSchematics(bool files) {
    schematicSet = schematic::session::current();
    // Files copied in while the Files tab is open show up without a reload.
    auto now = std::chrono::steady_clock::now();
    if (schematicTab == SchematicTab::Files && now - schematicFilesScanned > std::chrono::seconds(2)) files = true;
    if (files) {
        schematicFiles = schematic::session::files();
        schematicFilesScanned = now;
        std::vector<std::string> paths;
        for (auto const& f : schematicFiles) paths.push_back(f.relative);
        schematicFileRows = schematic_files::rows(paths);
    }
    int placements = static_cast<int>(schematicSet.placements.size()), fileCount = static_cast<int>(schematicFiles.size());
    if ((schematicPick == SchematicPick::Placement && schematicIndex >= placements)
        || (schematicPick == SchematicPick::File && schematicIndex >= fileCount)) schematicPick = SchematicPick::None;
    verification = schematic::ghosts::verification();
    verifyRows.clear();
    materialRows.clear();
    if (verification->placement >= 0 && verification->placement == schematicSet.selected) {
        for (auto const& m : verification->mismatches) if (verifyMatches(m)) verifyRows.push_back(&m);
        for (auto const& line : materialsShownOnly ? verification->visibleMaterials : verification->materials) materialRows.push_back(&line);
        std::stable_partition(materialRows.begin(), materialRows.end(), [](auto const* line) { return !line->entity; });
    }
    materialListRows.clear();
    bool blocks = false, entities = false;
    for (auto const* line : materialRows) (line->entity ? entities : blocks) = true;
    for (int i = 0; i < static_cast<int>(materialRows.size()); ++i) {
        bool entity = materialRows[static_cast<size_t>(i)]->entity;
        if (blocks && entities && (i == 0 || entity != materialRows[static_cast<size_t>(i - 1)]->entity))
            materialListRows.push_back(entity ? -2 : -1);
        materialListRows.push_back(i);
    }
    if (materialSelected >= static_cast<int>(materialRows.size())) materialSelected = -1;
    if (verifySelected >= static_cast<int>(verifyRows.size())) verifySelected = -1;
}
int schematicRowCount() {
    switch (schematicTab) {
    case SchematicTab::Placements: return static_cast<int>(schematicSet.placements.size());
    case SchematicTab::Files: return static_cast<int>(schematicFileRows.size());
    case SchematicTab::Verify: return static_cast<int>(verifyRows.size());
    default: return static_cast<int>(materialListRows.size());
    }
}
schematic::SavedPlacement const* selectedPlacement() {
    return schematicTab == SchematicTab::Placements && schematicPick == SchematicPick::Placement && schematicIndex >= 0
        && schematicIndex < static_cast<int>(schematicSet.placements.size())
        ? &schematicSet.placements[static_cast<size_t>(schematicIndex)] : nullptr;
}
schematic::SavedPlacement const* checkedPlacement() {
    return schematicSet.selected >= 0 && schematicSet.selected < static_cast<int>(schematicSet.placements.size())
        ? &schematicSet.placements[static_cast<size_t>(schematicSet.selected)] : nullptr;
}
void pickSchematic(SchematicPick pick, int index) {
    finishNumber();
    schematicPick = pick;
    schematicIndex = index;
    schematicFieldFirst = 0;
    schematicFieldSelected = -1;
    schematicDeleteArmed = false;
    // The selected placement is the one keys, the HUD and the Verify tab act on.
    if (pick == SchematicPick::Placement && schematicSet.selected != index)
        schematic::session::change([&](schematic::PlacementSet& set) { set.selected = index; return true; });
    refreshSchematics(false);
}
void selectSchematicTab(SchematicTab tab) {
    finishNumber();
    schematicTab = tab;
    schematicListFirst = schematicFieldFirst = 0;
    schematicFieldSelected = -1;
    schematicDeleteArmed = false;
    verifySelected = -1;
    if (tab == SchematicTab::Placements) {
        schematicPick = schematicSet.selected >= 0 ? SchematicPick::Placement : SchematicPick::None;
        schematicIndex = schematicSet.selected;
    } else if (tab == SchematicTab::Files) {
        schematicPick = SchematicPick::None;
        schematicIndex = -1;
        refreshSchematics(true);
    }
}
void changeSchematic(std::function<void(schematic::SavedPlacement&)> const& apply) {
    int index = schematicPick == SchematicPick::Placement ? schematicIndex : -1;
    if (!schematic::session::change([&](schematic::PlacementSet& set) {
            if (index < 0 || index >= static_cast<int>(set.placements.size())) return false;
            apply(set.placements[static_cast<size_t>(index)]);
            return true;
        })) error = translated("schematic.saveError");
    else error.clear();
    refreshSchematics(false);
}
int placedLayers(schematic::SavedPlacement const& p) {
    auto structure = schematic::session::structure(p.file);
    if (!structure) return 1;
    return std::max(1, schematic::layerCount(schematic::placedSize(structure->size, p.placement.rotation), p.layers.axis));
}
// A large file is loaded only after the player confirms it.
bool waitsForLoad(schematic::session::FileEntry const& f) {
    return f.bytes > schematic::session::largeFileBytes && largeSchematicConfirmed != f.relative;
}
void placeSelectedFile() {
    if (schematicPick != SchematicPick::File || schematicIndex < 0 || schematicIndex >= static_cast<int>(schematicFiles.size())) return;
    if (auto const& f = schematicFiles[static_cast<size_t>(schematicIndex)]; waitsForLoad(f)) {
        largeSchematicConfirmed = f.relative;
        return;
    }
    auto place = standingPlace();
    if (!place) return;
    std::string problem;
    if (!schematic::session::place(schematicFiles[static_cast<size_t>(schematicIndex)].relative, {place->x, place->y, place->z},
            place->dimension, &problem)) {
        error = problem.empty() ? translated("schematic.saveError") : translated("schematic.notLoaded", problem);
        return;
    }
    error.clear();
    refreshSchematics(false);
    selectSchematicTab(SchematicTab::Placements);
    pickSchematic(SchematicPick::Placement, static_cast<int>(schematicSet.placements.size()) - 1);
}
void deleteSelectedPlacement() {
    int index = schematicIndex;
    if (!schematic::session::change([&](schematic::PlacementSet& set) {
            if (index < 0 || index >= static_cast<int>(set.placements.size())) return false;
            set.placements.erase(set.placements.begin() + index);
            if (set.selected == index) set.selected = -1;
            else if (set.selected > index) --set.selected;
            return true;
        })) { error = translated("schematic.saveError"); return; }
    error.clear();
    refreshSchematics(false);
    int left = static_cast<int>(schematicSet.placements.size());
    if (left) pickSchematic(SchematicPick::Placement, std::min(index, left - 1));
    else pickSchematic(SchematicPick::None, -1);
}
void showSelectedMismatch() {
    if (verifySelected < 0 || verifySelected >= static_cast<int>(verifyRows.size())) return;
    schematic::ghosts::point(verifyRows[static_cast<size_t>(verifySelected)]->position);
    close();
}
int schematicFieldCount() {
    switch (schematicTab) {
    case SchematicTab::Placements: return selectedPlacement() ? static_cast<int>(schematicFields.size()) : 0;
    case SchematicTab::Materials: return 2;
    default: return 0;
    }
}
// part: -1/1 step, 0 value (type a number or press), 2 label.
void activateSchematicField(int index, int part) {
    if (schematicTab == SchematicTab::Materials) {
        if (part == 2) return;
        if (index == 0) {
            materialsShownOnly = !materialsShownOnly;
            refreshSchematics(false);
        } else if (index == 1) {
            if (auto option = settings::find("schematic.hud")) adjustOption(*option, 1);
        }
        return;
    }
    auto const* p = selectedPlacement();
    if (!p || index < 0 || index >= static_cast<int>(schematicFields.size())) return;
    schematicFieldSelected = index;
    if (part == 2) return;
    int direction = part == -1 ? -1 : 1;
    switch (schematicFields[static_cast<size_t>(index)]) {
    case SchematicField::X: case SchematicField::Y: case SchematicField::Z: {
        int value = index == 0 ? p->placement.origin.x : index == 1 ? p->placement.origin.y : p->placement.origin.z;
        if (part == 0) { editingSchematicField = index; numberInput.beginPrecise(value); error.clear(); return; }
        changeSchematic([&](schematic::SavedPlacement& t) {
            (index == 0 ? t.placement.origin.x : index == 1 ? t.placement.origin.y : t.placement.origin.z) += direction;
        });
        return;
    }
    case SchematicField::Rotation:
        changeSchematic([&](schematic::SavedPlacement& t) { t.placement.rotation = schematic::quarterTurns(t.placement.rotation + direction); });
        return;
    case SchematicField::Mirror:
        changeSchematic([&](schematic::SavedPlacement& t) {
            t.placement.mirror = static_cast<schematic::Mirror>((static_cast<int>(t.placement.mirror) + direction + 3) % 3);
        });
        return;
    case SchematicField::MoveHere:
        if (auto place = standingPlace())
            changeSchematic([&](schematic::SavedPlacement& t) {
                t.placement.origin = {place->x, place->y, place->z};
                t.dimension = place->dimension;
            });
        return;
    case SchematicField::Visible: changeSchematic([](schematic::SavedPlacement& t) { t.visible = !t.visible; }); return;
    case SchematicField::LayerAxis: {
        auto structure = schematic::session::structure(p->file);
        auto placed = structure ? schematic::placedSize(structure->size, p->placement.rotation) : schematic::Size{1, 1, 1};
        changeSchematic([&](schematic::SavedPlacement& t) {
            t.layers = schematic::withAxis(t.layers, placed,
                static_cast<schematic::LayerAxis>((static_cast<int>(t.layers.axis) + direction + 6) % 6));
        });
        return;
    }
    case SchematicField::LayerMode:
        changeSchematic([&](schematic::SavedPlacement& t) {
            t.layers.mode = static_cast<schematic::LayerMode>((static_cast<int>(t.layers.mode) + direction + 3) % 3);
        });
        return;
    case SchematicField::Layer: {
        int count = placedLayers(*p);
        if (part == 0) { editingSchematicField = index; numberInput.beginPrecise(p->layers.index + 1); error.clear(); return; }
        changeSchematic([&](schematic::SavedPlacement& t) { t.layers.index = std::clamp(t.layers.index + direction, 0, count - 1); });
        return;
    }
    case SchematicField::MatchLayer: {
        // The layer the player stands in, along the chosen direction; showing
        // all layers switches to "this layer only".
        auto place = standingPlace();
        auto structure = schematic::session::structure(p->file);
        if (!place || !structure) return;
        auto placed = schematic::placedSize(structure->size, p->placement.rotation);
        schematic::Point offset{place->x - p->placement.origin.x, place->y - p->placement.origin.y, place->z - p->placement.origin.z};
        int layer = schematic::layerOf(placed, p->layers.axis, offset);
        int count = schematic::layerCount(placed, p->layers.axis);
        changeSchematic([&](schematic::SavedPlacement& t) {
            t.layers.index = std::clamp(layer, 0, count - 1);
            if (t.layers.mode == schematic::LayerMode::All) t.layers.mode = schematic::LayerMode::Only;
        });
        return;
    }
    case SchematicField::Extras: changeSchematic([](schematic::SavedPlacement& t) { t.countExtras = !t.countExtras; }); return;
    case SchematicField::Entities: changeSchematic([](schematic::SavedPlacement& t) { t.entities = !t.entities; }); return;
    }
}
void moveSchematicField(int step) {
    int count = schematicFieldCount();
    if (!count) return;
    schematicFieldSelected = std::clamp(schematicFieldSelected + step, 0, count - 1);
    int visible = schematicsDisplayed.fieldVisible;
    if (visible > 0) {
        if (schematicFieldSelected < schematicFieldFirst) schematicFieldFirst = schematicFieldSelected;
        if (schematicFieldSelected >= schematicFieldFirst + visible) schematicFieldFirst = schematicFieldSelected - visible + 1;
    }
}
void openSchematicKeySettings() {
    int category = 0;
    for (size_t i = 0; i < sections.size(); ++i) if (sections[i] == featureSection("schematic")) category = static_cast<int>(i) + 1;
    expanded.insert("schematic");
    selectNav(category);
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].heading() && rows[i].feature->id == "schematic") { selected = static_cast<int>(i); break; }
    first = SettingsTable::reveal(first, selected, displayed.visible);
}
// The tab strip sits in the list pane's toolbar.
constexpr int schematicTabCount = 4;
float schematicTabWidth(ShapesLayout const& l) { return (l.listWidth - 2 * ShapesLayout::pad - 3 * 2) / schematicTabCount; }
float schematicTabX(ShapesLayout const& l, int tab) { return l.listLeft + ShapesLayout::pad + tab * (schematicTabWidth(l) + 2); }
int schematicTabAt(ShapesLayout const& l, float x, float y) {
    if (!l.usable() || y < l.toolbarTop || y >= l.toolbarTop + ShapesLayout::toolbarHeight) return -1;
    for (int i = 0; i < schematicTabCount; ++i)
        if (x >= schematicTabX(l, i) && x < schematicTabX(l, i) + schematicTabWidth(l)) return i;
    return -1;
}
// The Check tab's four filters, in the list's heading row (L-93 screen review):
// sized to their text and drawn as outlined pills, so they read as part of the
// list rather than more tabs. Their spans are kept from the last draw.
std::array<std::pair<float, float>, 4> verifyChipSpans{};
int verifyChipAt(ShapesLayout const& l, float x, float y) {
    if (schematicTab != SchematicTab::Verify || y < l.theadTop || y >= l.rowsTop) return -1;
    for (int i = 0; i < 4; ++i)
        if (x >= verifyChipSpans[static_cast<size_t>(i)].first && x < verifyChipSpans[static_cast<size_t>(i)].second) return i;
    return -1;
}
int stackSizeOf(schematic::MaterialLine const& line) {
    auto const* stack = schematic::items::iconStack(line.icon);
    return stack ? std::max(1, static_cast<int>(stack->getMaxStackSize())) : 64;
}
// "1 chest + 4 stacks + 16", number first (L-93 screen review).
std::string amountText(std::uint64_t count, int maxStack) {
    auto a = schematic::amountOf(count, maxStack);
    std::vector<std::string> parts;
    if (a.chests) parts.push_back(translated(a.chests == 1 ? "amount.chest" : "amount.chests", a.chests));
    if (a.stacks) parts.push_back(translated(a.stacks == 1 ? "amount.stack" : "amount.stacks", a.stacks));
    if (a.items || parts.empty()) parts.push_back(std::to_string(a.items));
    std::string out;
    for (auto const& part : parts) out += (out.empty() ? "" : " + ") + part;
    return out;
}
std::map<std::string, std::uint64_t> carriedItems();
// Materials still to gather: remaining minus what the player carries.
struct Missing { schematic::MaterialLine const* line; std::uint64_t missing; };
std::vector<Missing> missingMaterials() {
    std::vector<Missing> out;
    auto carried = carriedItems();
    for (auto const* line : materialRows) {
        bool noItem = line->item.empty() || (line->entity && !schematic::items::iconStack(line->icon));
        if (noItem || !line->remaining()) continue;
        auto have = carried[line->item];
        if (have < line->remaining()) out.push_back({line, line->remaining() - have});
    }
    return out;
}
void handleSchematicClick(float x, float y, bool right) {
    finishNumber();
    if (!right && pressScrollbar(schematicsDisplayed, schematicListFirst, x, y)) return;
    auto& t = previewTurn;
    if (!right && t.w > 0 && x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) {
        t.dragging = t.manual = true;
        t.from = {x, y};
        t.fromYaw = t.yaw;
        t.fromPitch = t.pitch;
        return;
    }
    if (!schematicsDocked) {
        auto nav = displayed.hit(x, y, navCount, displayedTabWidth);
        if (nav.zone == Zone::Nav) { selectNav(nav.index); return; }
        if (nav.zone == Zone::Version) { copyVersion(); return; }
    }
    if (int tab = schematicTabAt(schematicsDisplayed, x, y); tab >= 0) { selectSchematicTab(static_cast<SchematicTab>(tab)); return; }
    if (int chip = verifyChipAt(schematicsDisplayed, x, y); chip >= 0) {
        verifyFilter = chip;
        verifySelected = -1;
        schematicListFirst = 0;
        refreshSchematics(false);
        return;
    }
    auto hit = schematicsDisplayed.hit(x, y);
    if (!(hit.zone == ShapeZone::Action && hit.index == 1)) schematicDeleteArmed = false;
    switch (hit.zone) {
    case ShapeZone::Close: close(); return;
    case ShapeZone::Dock: schematicsDocked = !schematicsDocked; return;
    case ShapeZone::Keys: openSchematicKeySettings(); return;
    case ShapeZone::DrawAll:
        if (auto option = settings::find("schematic.enabled")) adjustOption(*option, 1);
        return;
    case ShapeZone::ListRow: {
        int row = hit.index;
        auto const& l = schematicsDisplayed;
        switch (schematicTab) {
        case SchematicTab::Placements:
            if (x >= l.listLeft + l.listWidth - ShapesLayout::pad - switchWidth - 2) {
                schematic::session::change([&](schematic::PlacementSet& set) {
                    if (row < 0 || row >= static_cast<int>(set.placements.size())) return false;
                    set.placements[static_cast<size_t>(row)].visible = !set.placements[static_cast<size_t>(row)].visible;
                    return true;
                });
                refreshSchematics(false);
                return;
            }
            if (schematicPick != SchematicPick::Placement || row != schematicIndex) pickSchematic(SchematicPick::Placement, row);
            return;
        case SchematicTab::Files: {
            if (row < 0 || row >= static_cast<int>(schematicFileRows.size())) return;
            int file = schematicFileRows[static_cast<size_t>(row)].file;
            if (file >= 0 && (schematicPick != SchematicPick::File || file != schematicIndex)) pickSchematic(SchematicPick::File, file);
            return;
        }
        case SchematicTab::Verify: verifySelected = row; return;
        default:
            if (row >= 0 && row < static_cast<int>(materialListRows.size()) && materialListRows[static_cast<size_t>(row)] >= 0)
                materialSelected = materialListRows[static_cast<size_t>(row)];
            return;
        }
    }
    case ShapeZone::Field: activateSchematicField(hit.index, right ? -1 : hit.part); return;
    case ShapeZone::Action:
        if (schematicTab == SchematicTab::Files) {
            if (hit.index == 0) placeSelectedFile();
            else if (!schematic::session::openFolder()) error = translated("schematic.openFolderError");
            return;
        }
        if (schematicTab == SchematicTab::Verify) { if (hit.index == 0) showSelectedMismatch(); return; }
        if (schematicTab == SchematicTab::Materials) {
            if (hit.index != 0) return;
            auto items = missingMaterials();
            if (items.empty()) return;
            std::vector<std::pair<std::string, std::uint64_t>> wanted;
            for (auto const& m : items) wanted.push_back({m.line->item, m.missing});
            if (!openUrl(schematic::calculatorUrl(wanted))) error = translated("worldMap.linkFailed");
            return;
        }
        if (schematicTab != SchematicTab::Placements || hit.index != 1 || !selectedPlacement()) return;
        if (!schematicDeleteArmed) { schematicDeleteArmed = true; return; }
        deleteSelectedPlacement();
        return;
    default: return;
    }
}
void handleSchematicKey(int key) {
    if (editingSchematicField >= 0) {
        switch (key) {
        case 0x08: if (numberInput.backspace()) numberDirty = true; break;
        case 0x41: if (heldCtrl()) numberInput.selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: finishNumber(); break;
        }
        return;
    }
    switch (key) {
    case 0x1b: close(); break;
    case 0x26: moveSchematicField(-1); break;
    case 0x28: moveSchematicField(1); break;
    case 0x25: activateSchematicField(schematicFieldSelected, -1); break;
    case 0x27: activateSchematicField(schematicFieldSelected, 1); break;
    case 0x0d: case 0x20:
        if (schematicTab == SchematicTab::Files) placeSelectedFile();
        else if (schematicTab == SchematicTab::Verify) showSelectedMismatch();
        else activateSchematicField(schematicFieldSelected, 0);
        break;
    case 0x21: case 0x22: {
        int count = schematicRowCount();
        if (!count) break;
        int current = schematicTab == SchematicTab::Verify ? verifySelected
            : (schematicTab == SchematicTab::Placements && schematicPick == SchematicPick::Placement)
                || (schematicTab == SchematicTab::Files && schematicPick == SchematicPick::File) ? schematicIndex : -1;
        int step = key == 0x22 ? 1 : -1;
        if (schematicTab == SchematicTab::Files) {
            // Rows and files differ: step over folder headings.
            int at = current >= 0 ? schematic_files::rowOf(schematicFileRows, current) : (step > 0 ? -1 : count);
            for (int r = at + step; r >= 0 && r < count; r += step)
                if (int file = schematicFileRows[static_cast<size_t>(r)].file; file >= 0) { pickSchematic(SchematicPick::File, file); break; }
            break;
        }
        int row = std::clamp(current + step, 0, count - 1);
        if (schematicTab == SchematicTab::Verify) verifySelected = row;
        else if (schematicTab == SchematicTab::Placements) pickSchematic(SchematicPick::Placement, row);
        else if (schematicTab == SchematicTab::Files) pickSchematic(SchematicPick::File, row);
        break;
    }
    case 0x09: selectNav((navigation.current + (heldShift() ? worldMapNav - 1 : 1)) % worldMapNav); break;
    }
}
std::string fileTitle(std::string const& relative) {
    auto slash = relative.find_last_of('/');
    auto name = relative.substr(slash == std::string::npos ? 0 : slash + 1);
    if (name.ends_with(".mcstructure")) name.resize(name.size() - std::string_view(".mcstructure").size());
    return name;
}
std::string schematicDescription() {
    switch (schematicTab) {
    case SchematicTab::Placements:
        if (auto const* p = selectedPlacement()) {
            if (editingSchematicField >= 0) return translated("integerRange", editingSchematicField == 9 ? 1 : -30'000'000,
                editingSchematicField == 9 ? 4096 : 30'000'000);
            if (p->dimension != playerDimension()) return p->name + ": " + translated("schematic.elsewhere");
            return translated("schematic.placedHint", p->name);
        }
        return translated(schematicSet.placements.empty() ? "schematic.noPlacements" : "schematic.selectHint");
    case SchematicTab::Files:
        if (schematicPick == SchematicPick::File && schematicIndex >= 0 && schematicIndex < static_cast<int>(schematicFiles.size()))
            return translated("schematic.fileHint", fileTitle(schematicFiles[static_cast<size_t>(schematicIndex)].relative));
        return translated(schematicFiles.empty() ? "schematic.empty" : "schematic.fileSelectHint");
    case SchematicTab::Verify: return translated("schematic.verifyHint");
    default: return translated("schematic.materialsHint");
    }
}
std::string fieldValue(schematic::SavedPlacement const& p, SchematicField field) {
    switch (field) {
    case SchematicField::X: return std::to_string(p.placement.origin.x);
    case SchematicField::Y: return std::to_string(p.placement.origin.y);
    case SchematicField::Z: return std::to_string(p.placement.origin.z);
    case SchematicField::Rotation: return std::format("{}°", p.placement.rotation * 90);
    case SchematicField::Mirror:
        return translated(p.placement.mirror == schematic::Mirror::X ? "schematic.mirror.x"
            : p.placement.mirror == schematic::Mirror::Z ? "schematic.mirror.z" : "schematic.mirror.none");
    case SchematicField::LayerAxis: {
        static constexpr std::array<std::string_view, 6> axes{"schematic.axis.up", "schematic.axis.down", "schematic.axis.east",
            "schematic.axis.west", "schematic.axis.south", "schematic.axis.north"};
        return translated(axes[static_cast<size_t>(p.layers.axis)]);
    }
    case SchematicField::LayerMode:
        return translated(p.layers.mode == schematic::LayerMode::Only ? "schematic.mode.only"
            : p.layers.mode == schematic::LayerMode::UpTo ? "schematic.mode.upTo" : "schematic.mode.all");
    case SchematicField::Layer: return translated("schematic.layerValue", p.layers.index + 1, placedLayers(p));
    case SchematicField::Extras: return translated(p.countExtras ? "schematic.extras.show" : "schematic.extras.ignore");
    default: return {};
    }
}
std::string layersText(schematic::SavedPlacement const& p) {
    if (p.layers.mode == schematic::LayerMode::All) return translated("schematic.mode.all");
    return fieldValue(p, SchematicField::LayerAxis) + " " + fieldValue(p, SchematicField::Layer) + " "
        + fieldValue(p, SchematicField::LayerMode);
}
void drawItemIcon(MinecraftUIRenderContext& context, std::string const& icon, float x, float y, float size) {
    auto const* stack = schematic::items::iconStack(icon);
    auto* renderer = context.mClient.getItemRenderer();
    if (!stack || !renderer) return;
    BaseActorRenderContext renderContext(context.mScreenContext, context.mClient, context.mClient.getMinecraftGame_DEPRECATED());
    renderer->renderGuiItemNew(renderContext, *stack, 0, std::round(x), std::round(y), false, 1.f, 1.f, size / 16, 17);
}
std::map<std::string, std::uint64_t> carriedItems() {
    auto* player = client ? client->getLocalPlayer() : nullptr;
    return player ? schematic::items::carried(*player) : std::map<std::string, std::uint64_t>{};
}
std::string mismatchKind(schematic::CellState state) {
    switch (state) {
    case schematic::CellState::Wrong: return translated("schematic.kind.wrong");
    case schematic::CellState::Extra: return translated("schematic.kind.extra");
    case schematic::CellState::State: return translated("schematic.kind.state");
    default: return translated("schematic.kind.missing");
    }
}
Rgb mismatchColor(schematic::CellState state) {
    switch (state) {
    case schematic::CellState::Wrong: case schematic::CellState::Extra: return {1.f, .35f, .3f};
    case schematic::CellState::State: return {1.f, .8f, .25f};
    default: return {.75f, .85f, .9f};
    }
}
void drawSchematicsBody(MinecraftUIRenderContext& context, ShapesLayout const& l, glm::vec2 pointer) {
    auto const preferences = Runtime::instance().preferences();
    auto hover = l.hit(pointer.x, pointer.y);
    int tabHover = schematicTabAt(l, pointer.x, pointer.y);
    auto over = [&](ShapeZone zone, int index = -1) { return tabHover < 0 && hover.zone == zone && (index < 0 || hover.index == index); };
    float top = l.top + 4;
    label(context,l.drawAllX,l.drawAllY+1+boxTextInset(),l.drawAllWidth-switchWidth-4,translated("waypoint.showAll"),
        over(ShapeZone::DrawAll) ? palette::text : palette::dim,Align::Right);
    toggleSwitch(context,l.drawAllX+l.drawAllWidth-switchWidth,l.drawAllY+2,preferences.schematic.enabled);
    label(context,l.keysX,top+1+boxTextInset(),ShapesLayout::keysWidth,translated("shape.keys"),
        over(ShapeZone::Keys) ? palette::text : palette::accent,Align::Center);
    fill(context,l.keysX+6,top+11,ShapesLayout::keysWidth-12,1,palette::accent,over(ShapeZone::Keys) ? 1.f : .5f);
    drawSmallButton(context,l.dockX,top,ShapesLayout::dockWidth,12,translated(schematicsDocked ? "shape.undock" : "shape.dock"),over(ShapeZone::Dock));

    // Tabs.
    static constexpr std::array<std::string_view, schematicTabCount> tabNames{"schematic.tab.files", "schematic.tab.placements",
        "schematic.tab.verify", "schematic.tab.materials"};
    for (int i = 0; i < schematicTabCount; ++i) {
        bool active = static_cast<int>(schematicTab) == i;
        float x = schematicTabX(l, i), w = schematicTabWidth(l);
        drawSmallButton(context,x,l.toolbarTop+2,w,12,translated(tabNames[static_cast<size_t>(i)]),tabHover == i,
            active ? palette::accentDeep : palette::keyFill, active ? palette::accent : palette::keyEdge,
            active ? palette::text : palette::dim);
    }
    fill(context,l.listLeft,l.theadTop-1,l.listWidth,1,palette::white,.14f);

    // List.
    float listRight = l.listLeft + l.listWidth;
    float left = l.listLeft + ShapesLayout::pad;
    float shownX = listRight - ShapesLayout::pad - switchWidth - 2;
    auto heading = [&](float x, float w, std::string_view key, Align align = Align::Left) {
        label(context,x,l.theadTop+2,w,translated(key),palette::faint,align);
    };
    auto* checked = checkedPlacement();
    // A placement whose file cannot be loaded is never counted: say why instead.
    std::string loadProblem;
    bool unloadable = checked && (schematicTab == SchematicTab::Verify || schematicTab == SchematicTab::Materials)
        && !schematic::session::structure(checked->file, &loadProblem);
    bool counting = checked && !unloadable && (!verification->complete || verification->placement != schematicSet.selected);
    // Column positions for the Verify and Materials lists.
    // A narrow list (a large UI) drops the columns it can do without, so the
    // names keep their room: the position in Check, "placed" in Materials.
    // Files: size and block count on the right; size goes first when narrow.
    float blocksW = 40, blocksX = listRight - ShapesLayout::pad - blocksW, sizeW = 54, sizeX = blocksX - 4 - sizeW;
    bool showSize = sizeX - left >= 80;
    bool loadedOne = false; // At most one file is read per frame to fill the columns.
    // Placed: progress left of the switch; coordinates only while the name
    // keeps room (the detail pane always shows them).
    float progressW = 26, progressX = shownX - 6 - progressW;
    float coordsW = 66, coordsX = progressX - 4 - coordsW;
    bool showCoords = coordsX - left >= 70;
    if (schematicTab == SchematicTab::Placements) schematic::ghosts::wantProgress();
    float kindW = 38, distW = 28, posW = listRight - left - kindW - distW - ShapesLayout::pad - 78 >= 90 ? 78.f : 0.f;
    float numW = 34, carriedX = listRight - ShapesLayout::pad - numW, leftX = carriedX - numW - 2;
    bool showPlaced = leftX - 2 * (numW + 2) - left - 16 >= 70;
    float placedX = leftX - numW - 2, neededX = (showPlaced ? placedX : leftX) - numW - 2;
    switch (schematicTab) {
    case SchematicTab::Placements:
        heading(left, shownX - left - 74, "shape.columnName");
        heading(progressX - 2, progressW + 4, "schematic.column.progress", Align::Right);
        heading(shownX - 6, switchWidth + 12, "shape.columnShown", Align::Center);
        break;
    case SchematicTab::Files:
        heading(left, blocksX - left - 4, "shape.columnName");
        if (showSize) heading(sizeX, sizeW, "schematic.column.size", Align::Right);
        heading(blocksX, blocksW, "schematic.column.blockCount", Align::Right);
        break;
    case SchematicTab::Verify: {
        // The filters replace the column headings; each shows how many rows it has.
        static constexpr std::array<std::string_view, 4> chips{"schematic.filter.mistakes", "schematic.filter.wrong",
            "schematic.filter.state", "schematic.filter.missing"};
        auto const& t = verification->visible;
        bool counted = checked && !counting;
        std::array<std::uint64_t, 4> counts{t.wrong + t.state + t.extra, t.wrong + t.extra, t.state, t.missing};
        float cx = left;
        for (int i = 0; i < 4; ++i) {
            bool on = verifyFilter == i, hovered = verifyChipAt(l,pointer.x,pointer.y) == i;
            std::string text = translated(chips[static_cast<size_t>(i)]);
            if (counted) text += std::format(" {}", counts[static_cast<size_t>(i)]);
            constexpr float chipText = .8f;
            float w = std::min(textWidthScaled(context, text, chipText) + 8, listRight - ShapesLayout::pad - cx);
            if (w <= 8) break;
            float top = l.theadTop + 1, h = ShapesLayout::theadHeight - 2;
            if (on) fill(context,cx,top,w,h,palette::accent,.15f);
            if (hovered) fill(context,cx,top,w,h,palette::white,.08f);
            frame(context,cx,top,w,h,on ? palette::accent : palette::keyEdge);
            labelScaled(context,cx+4,top+(h-8*chipText)/2-1+boxTextInset(),w-6,std::move(text),chipText,on ? palette::accent : palette::dim);
            verifyChipSpans[static_cast<size_t>(i)] = {cx, cx + w};
            cx += w + 4;
        }
        break;
    }
    case SchematicTab::Materials:
        heading(left + 14, neededX - left - 16, "schematic.column.material");
        heading(neededX, numW, "schematic.column.needed", Align::Right);
        if (showPlaced) heading(placedX, numW, "schematic.column.placed", Align::Right);
        heading(leftX, numW, "schematic.column.left", Align::Right);
        heading(carriedX, numW, "schematic.column.carried", Align::Right);
        break;
    }
    fill(context,l.listLeft,l.rowsTop-1,l.listWidth,1,palette::white,.14f);
    std::string empty;
    if (l.listCount == 0) {
        if (schematicTab == SchematicTab::Placements) empty = translated("schematic.noPlacements");
        else if (schematicTab == SchematicTab::Files) empty = translated("schematic.empty");
        else if (!checked) empty = translated("schematic.noSelection");
        else if (unloadable) empty = translated("schematic.notLoaded", loadProblem);
        else if (counting) empty = translated("schematic.counting");
        else empty = translated(schematicTab == SchematicTab::Verify ? "schematic.noMistakes" : "schematic.noMaterials");
        paragraph(context,left,l.rowsTop+3,l.listWidth-2*ShapesLayout::pad,empty,4,palette::faint);
    }
    auto* player = client ? client->getLocalPlayer() : nullptr;
    Vec3 feet = player ? player->getFeetPos() : Vec3{0, 0, 0};
    auto carried = schematicTab == SchematicTab::Materials ? carriedItems() : std::map<std::string, std::uint64_t>{};
    struct Tip { std::string text; float x = 0, y = 0; };
    std::optional<Tip> materialTip;
    for (int i = l.listFirst; i < l.listFirst + l.listVisible && i < l.listCount; ++i) {
        float y = l.listRowY(i);
        if (i % 2) fill(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,palette::white,.025f);
        bool chosen = schematicTab == SchematicTab::Verify ? i == verifySelected
            : schematicTab == SchematicTab::Placements ? schematicPick == SchematicPick::Placement && i == schematicIndex
            : schematicTab == SchematicTab::Files ? schematicPick == SchematicPick::File
                && schematicFileRows[static_cast<size_t>(i)].file == schematicIndex : false;
        bool heading = (schematicTab == SchematicTab::Files && schematicFileRows[static_cast<size_t>(i)].file < 0)
            || (schematicTab == SchematicTab::Materials && materialListRows[static_cast<size_t>(i)] < 0);
        if (schematicTab == SchematicTab::Materials && !heading) chosen = materialListRows[static_cast<size_t>(i)] == materialSelected;
        if (!heading) rowBackground(context,l.listLeft+1,y,l.listWidth-2,ShapesLayout::rowHeight,chosen,over(ShapeZone::ListRow,i));
        switch (schematicTab) {
        case SchematicTab::Placements: {
            auto const& p = schematicSet.placements[static_cast<size_t>(i)];
            bool here = p.dimension == playerDimension();
            // The selected placement: the accent bar, like the sidebar's current entry.
            if (i == schematicSet.selected) fill(context,l.listLeft+1,y,2,ShapesLayout::rowHeight,palette::accent);
            float nameEnd = showCoords ? coordsX - 4 : progressX - 4;
            label(context,left,y+3,nameEnd-left,p.name,here ? palette::text : palette::faint);
            if (showCoords)
                label(context,coordsX,y+3,coordsW,here ? std::format("{}, {}, {}", p.placement.origin.x, p.placement.origin.y, p.placement.origin.z)
                    : dimensionName(p.dimension),palette::dim,Align::Right);
            if (auto tally = schematic::ghosts::progress(p); tally && tally->total()) {
                float fraction = static_cast<float>(tally->correct) / static_cast<float>(tally->total());
                label(context,progressX,y+1,progressW,std::format("{}%", static_cast<int>(fraction * 100)),
                    here ? palette::text : palette::faint,Align::Right);
                fill(context,progressX,y+11,progressW,1,palette::white,.12f);
                fill(context,progressX,y+11,progressW*fraction,1,palette::accent,here ? 1.f : .5f);
            } else label(context,progressX,y+3,progressW,"-",palette::faint,Align::Right);
            toggleSwitch(context,shownX,y+(ShapesLayout::rowHeight-switchHeight)/2,p.visible);
            break;
        }
        case SchematicTab::Files: {
            auto const& row = schematicFileRows[static_cast<size_t>(i)];
            if (row.file < 0) {
                label(context,left,y+3,l.listWidth-2*ShapesLayout::pad,"/" + row.folder,
                      palette::faint);
                break;
            }
            auto const& f = schematicFiles[static_cast<size_t>(row.file)];
            bool grouped = !schematicFileRows.empty() && schematicFileRows.front().file < 0;
            label(context,left+(grouped ? 6.f : 0.f),y+3,
                  blocksX-left-10,fileTitle(f.relative),palette::text);
            auto structure = schematic::session::loaded(f.relative);
            if (!structure && !loadedOne && f.bytes <= schematic::session::largeFileBytes) {
                loadedOne = true;
                structure = schematic::session::structure(f.relative);
            }
            if (structure) {
                if (showSize) label(context,sizeX,y+3,sizeW,std::format("{}x{}x{}", structure->size.x, structure->size.y, structure->size.z),
                    palette::dim,Align::Right);
                label(context,blocksX,y+3,blocksW,std::to_string(blockCount(*structure)),palette::dim,Align::Right);
            } else label(context,blocksX,y+3,blocksW,"-",palette::faint,Align::Right);
            break;
        }
        case SchematicTab::Verify: {
            auto const& m = *verifyRows[static_cast<size_t>(i)];
            fill(context,left,y+4,6,6,mismatchColor(m.state));
            label(context,left+9,y+3,kindW-10,mismatchKind(m.state),palette::dim);
            if (posW > 0) label(context,left+kindW,y+3,posW-2,std::format("{}, {}, {}", m.position.x, m.position.y, m.position.z),palette::dim);
            float bx = left + kindW + posW, bw = listRight - bx - distW - ShapesLayout::pad - 4;
            // The schematic's block, then what is there, each with its icon.
            auto block = [&](float x, float w, std::string const& icon, std::string const& name, Rgb color) {
                if (!icon.empty()) drawItemIcon(context, icon, x, y + 1, 12);
                label(context,x+14,y+3,w-14,name,color);
            };
            if (m.entity) block(bx, bw, m.expected, translated("schematic.withDetail", m.expectedName, translated("schematic.entityTag")), palette::text);
            else if (m.state == schematic::CellState::Missing || m.state == schematic::CellState::State)
                block(bx, bw, m.expected, m.expectedName, palette::text); // A wrong state: which states, in the right pane.
            else if (m.state == schematic::CellState::Extra) {
                // Same columns as a wrong block: air where the schematic's block would be.
                float half = (bw - 10) / 2;
                label(context,bx+14,y+3,half-14,translated("schematic.air"),palette::faint);
                label(context,bx+half,y+3,10,">",palette::faint,Align::Center);
                block(bx + half + 10, half, m.actual, m.actualName, palette::dim);
            } else {
                float half = (bw - 10) / 2;
                block(bx, half, m.expected, m.expectedName, palette::text);
                label(context,bx+half,y+3,10,">",palette::faint,Align::Center);
                block(bx + half + 10, half, m.actual, m.actualName, palette::dim);
            }
            double dx = m.position.x + .5 - feet.x, dy = m.position.y + .5 - feet.y, dz = m.position.z + .5 - feet.z;
            label(context,listRight-ShapesLayout::pad-distW,y+3,distW,
                translated("waypoint.meters", static_cast<int>(std::lround(std::sqrt(dx*dx + dy*dy + dz*dz)))),palette::dim,Align::Right);
            break;
        }
        case SchematicTab::Materials: {
            int index = materialListRows[static_cast<size_t>(i)];
            if (index < 0) {
                label(context,left,y+3,l.listWidth-2*ShapesLayout::pad,translated(index == -1 ? "schematic.section.blocks"
                    : "schematic.section.entities"),palette::faint);
                break;
            }
            auto const& line = *materialRows[static_cast<size_t>(index)];
            // The counts convert to chests and stacks under the pointer.
            if (tabHover < 0 && hover.zone == ShapeZone::ListRow && hover.index == i) {
                bool noItemHere = line.item.empty() || (line.entity && !schematic::items::iconStack(line.icon));
                std::optional<std::pair<std::string_view, std::uint64_t>> column;
                if (pointer.x >= carriedX) { if (!noItemHere) column = {{"schematic.column.carried", carried[line.item]}}; }
                else if (pointer.x >= leftX) column = {{"schematic.column.left", line.remaining()}};
                else if (showPlaced && pointer.x >= placedX) column = {{"schematic.column.placed", line.placed}};
                else if (pointer.x >= neededX) column = {{"schematic.column.needed", line.needed}};
                if (column) materialTip = {translated(column->first) + ": " + amountText(column->second, stackSizeOf(line)),
                    pointer.x, pointer.y};
            }
            // Entities: an icon and a carried count only when an item places them.
            bool noItem = line.item.empty() || (line.entity && !schematic::items::iconStack(line.icon));
            drawItemIcon(context, line.icon, left, y + 1, 12);
            // Under the Entities heading the name needs no "(entity)".
            bool sectioned = !materialListRows.empty() && materialListRows.front() < 0;
            std::string name = line.entity && !sectioned ? translated("schematic.withDetail", line.name, translated("schematic.entityTag")) : line.name;
            label(context,left+14,y+3,neededX-left-16,name,line.remaining() ? palette::text : palette::faint);
            auto have = noItem ? std::uint64_t{0} : carried[line.item];
            label(context,neededX,y+3,numW,std::to_string(line.needed),palette::dim,Align::Right);
            if (showPlaced) label(context,placedX,y+3,numW,std::to_string(line.placed),palette::dim,Align::Right);
            label(context,leftX,y+3,numW,std::to_string(line.remaining()),palette::text,Align::Right);
            Rgb haveColor = !line.remaining() ? palette::faint : have >= line.remaining() ? palette::accent : palette::warning;
            label(context,carriedX,y+3,numW,noItem ? "-" : std::to_string(have),noItem ? palette::faint : haveColor,Align::Right);
            break;
        }
        }
    }
    if (l.listCount > l.listVisible) {
        float track = l.listVisible * ShapesLayout::rowHeight;
        float thumb = std::max(8.0f, track * l.listVisible / l.listCount);
        float thumbY = l.rowsTop + (track - thumb) * l.listFirst / (l.listCount - l.listVisible);
        fill(context,listRight-3,l.rowsTop,2,track,palette::white,.08f);
        fill(context,listRight-3,thumbY,2,thumb,palette::keyEdge);
    }
    if (l.docked) fill(context,l.left,l.detailTop-1,l.width,1,palette::white,.14f);
    else fill(context,l.detailLeft-1,l.toolbarTop,1,l.footerTop-l.toolbarTop,palette::white,.14f);

    // Detail.
    float dx = l.detailLeft + ShapesLayout::pad, dw = l.detailWidth - 2 * ShapesLayout::pad;
    auto info = [&](std::string const& relative, float y) {
        std::string problem;
        auto structure = schematic::session::structure(relative, &problem);
        if (!structure) { paragraph(context,dx,y,dw,translated("schematic.notLoaded", problem),3,palette::warning); return; }
        std::uint64_t blocks = blockCount(*structure);
        label(context,dx,y,dw,translated("schematic.size", structure->size.x, structure->size.y, structure->size.z),palette::dim);
        label(context,dx,y+11,dw,translated("schematic.blocks", blocks),palette::dim);
        label(context,dx,y+22,dw,translated("schematic.entities", structure->entities.size()),palette::dim);
        label(context,dx,y+33,dw,relative,palette::faint);
    };
    auto stepperRow = [&](int i, std::string_view key, std::string value, bool isSwitch, bool on) {
        if (i < l.fieldFirst || i >= l.fieldFirst + l.fieldVisible) return;
        float y = l.fieldY(i);
        rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,schematicFieldSelected == i,over(ShapeZone::Field,i));
        label(context,dx,y+3,l.stepperX()-dx-4,translated(key),palette::dim);
        if (isSwitch) toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,on);
        else drawShapeStepper(context,l,y,false,std::move(value),false);
    };
    previewTurn.w = 0; // set again where the preview is drawn this frame
    switch (schematicTab) {
    case SchematicTab::Placements:
        if (auto const* p = selectedPlacement()) {
            label(context,dx,l.nameY+1+boxTextInset(),dw,p->name);
            info(p->file, l.previewY);
            if (auto tally = schematic::ghosts::progress(*p); tally && tally->total())
                label(context,dx,l.previewY+44,dw,translated("schematic.progress",
                    static_cast<int>(100.0 * static_cast<double>(tally->correct) / static_cast<double>(tally->total()))),palette::dim);
            for (int i = l.fieldFirst; i < l.fieldFirst + l.fieldVisible && i < static_cast<int>(schematicFields.size()); ++i) {
                float y = l.fieldY(i);
                auto field = schematicFields[static_cast<size_t>(i)];
                rowBackground(context,l.detailLeft+1,y,l.detailWidth-2,ShapesLayout::rowHeight,schematicFieldSelected == i,over(ShapeZone::Field,i));
                static constexpr std::array<std::string_view, 13> labels{"X", "Y", "Z", "schematic.rotation", "schematic.mirror",
                    "schematic.moveHere", "schematic.shown", "schematic.layerAxis", "schematic.layerMode", "schematic.layer",
                    "schematic.matchLayer", "schematic.extras", "schematic.showEntities"};
                std::string text = i < 3 ? std::string(labels[static_cast<size_t>(i)]) : translated(labels[static_cast<size_t>(i)]);
                label(context,dx,y+3,l.stepperX()-dx-4,text,palette::dim);
                switch (field) {
                case SchematicField::Visible: case SchematicField::Entities:
                    toggleSwitch(context,l.stepperX()+l.stepperWidth()-switchWidth,y+(ShapesLayout::rowHeight-switchHeight)/2,
                        field == SchematicField::Visible ? p->visible : p->entities);
                    break;
                case SchematicField::MoveHere: case SchematicField::MatchLayer:
                    drawSmallButton(context,l.stepperX(),y+1,l.stepperWidth(),ShapesLayout::rowHeight-2,
                        translated(field == SchematicField::MoveHere ? "schematic.moveHere" : "schematic.matchLayerButton"),
                        over(ShapeZone::Field,i));
                    break;
                default: {
                    bool numeric = field == SchematicField::X || field == SchematicField::Y || field == SchematicField::Z
                        || field == SchematicField::Layer;
                    drawShapeStepper(context,l,y,numeric,fieldValue(*p, field),editingSchematicField == i);
                    break;
                }
                }
            }
            fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
            drawSmallButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,
                translated(schematicDeleteArmed ? "shape.deleteConfirm" : "shape.delete"),over(ShapeZone::Action,1),
                schematicDeleteArmed ? Rgb{.54f,.18f,.16f} : palette::keyFill,Rgb{.54f,.23f,.2f},
                schematicDeleteArmed ? palette::text : Rgb{1.f,.7f,.68f});
        } else paragraph(context,dx,l.detailTop+6,dw,translated(schematicSet.placements.empty() ? "schematic.noPlacements"
            : "schematic.selectHint"),4,palette::faint);
        break;
    case SchematicTab::Files:
        if (schematicPick == SchematicPick::File && schematicIndex >= 0 && schematicIndex < static_cast<int>(schematicFiles.size())) {
            auto const& f = schematicFiles[static_cast<size_t>(schematicIndex)];
            label(context,dx,l.nameY+1+boxTextInset(),dw,fileTitle(f.relative));
            bool waits = waitsForLoad(f);
            if (waits) {
                // Just above its "Load" button, so the warning and the choice read together.
                auto text = translated("schematic.largeWarning", std::format("{:.1f}", static_cast<double>(f.bytes) / (1024 * 1024)));
                int lines = static_cast<int>(paragraphLines(context, dw, text, 4));
                paragraph(context,dx,std::max(l.previewY,l.actionsY-4-12.f*lines),dw,text,4,palette::warning);
            } else {
                info(f.relative, l.previewY);
                // The 3D preview below the facts, turning slowly (L-114).
                float top = l.previewY + 48, bottom = l.actionsY - 6;
                if (bottom - top >= 24) {
                    fill(context,dx,top,dw,bottom-top,Rgb{0,0,0},.25f);
                    frame(context,dx,top,dw,bottom-top,palette::white,.1f);
                    auto& t = previewTurn;
                    t.x = dx; t.y = top; t.w = dw; t.h = bottom - top;
                    float yaw = t.yaw;
                    if (!t.manual) yaw += std::fmod(static_cast<float>(std::chrono::duration<double>(
                        std::chrono::steady_clock::now().time_since_epoch()).count()) * 12.f, 360.f);
                    if (!schematic::preview::draw(context, schematic::session::structure(f.relative), dx + 1, top + 1, dw - 2, bottom - top - 2,
                            schematic::preview::View{yaw, t.pitch}))
                        t.w = 0;
                    else if (!t.manual) label(context,dx+4,bottom-12,dw-8,translated("schematic.previewHint"),palette::faint);
                }
            }
            drawSmallButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated(waits ? "schematic.loadAnyway" : "schematic.place"),
                over(ShapeZone::Action,0),palette::accentDeep,palette::accent);
        } else paragraph(context,dx,l.detailTop+6,dw,translated(schematicFiles.empty() ? "schematic.empty" : "schematic.fileSelectHint"),
            4,palette::faint);
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawSmallButton(context,l.deleteX(),l.actionsY+2,ShapesLayout::deleteWidth,12,translated("schematic.openFolder"),over(ShapeZone::Action,1));
        break;
    case SchematicTab::Verify: {
        if (!checked) { paragraph(context,dx,l.detailTop+6,dw,translated("schematic.noSelection"),4,palette::faint); break; }
        label(context,dx,l.nameY+1+boxTextInset(),dw,checked->name);
        auto const& t = verification->visible;
        // The whole placement first, then the selected position, each under a heading.
        label(context,dx,l.previewY-2,dw,translated("schematic.check.whole", layersText(*checked)),palette::faint);
        if (unloadable) paragraph(context,dx,l.previewY+10,dw,translated("schematic.notLoaded", loadProblem),3,palette::warning);
        else if (counting) label(context,dx,l.previewY+10,dw,translated("schematic.counting"),palette::dim);
        else {
            label(context,dx,l.previewY+10,dw,translated("schematic.summary.correct", t.correct, t.total()),palette::accent);
            label(context,dx,l.previewY+21,dw,translated("schematic.summary.missing", t.missing),palette::dim);
            label(context,dx,l.previewY+32,dw,translated("schematic.summary.wrong", t.wrong + t.extra),Rgb{1.f,.45f,.4f});
            label(context,dx,l.previewY+43,dw,translated("schematic.summary.state", t.state),Rgb{1.f,.8f,.3f});
        }
        if (verifySelected >= 0 && verifySelected < static_cast<int>(verifyRows.size())) {
            auto const& m = *verifyRows[static_cast<size_t>(verifySelected)];
            float y = l.previewY + 60;
            fill(context,dx,y-3,dw,1,palette::white,.1f);
            label(context,dx,y,dw,translated("schematic.check.here", std::format("{}, {}, {}", m.position.x, m.position.y, m.position.z))
                + "  " + mismatchKind(m.state),mismatchColor(m.state));
            if (!m.expectedName.empty()) {
                drawItemIcon(context, m.expected, dx, y + 12, 12);
                label(context,dx+14,y+14,dw-14,translated("schematic.expected", m.expectedName),palette::dim);
            }
            if (!m.actualName.empty()) {
                drawItemIcon(context, m.actual, dx, y + 27, 12);
                label(context,dx+14,y+29,dw-14,translated("schematic.actual", m.actualName),palette::dim);
            }
            // Every differing state: "<state>  <now> -> <should be>", named like the target card's.
            float sy = y + 45;
            if (!m.states.empty()) { label(context,dx,sy,dw,translated("schematic.check.states"),palette::faint); sy += 11; }
            Rgb stateColor{1.f, .8f, .25f};
            auto const& identifier = m.identifier;
            for (auto const& d : m.states) {
                auto translate = [](std::string_view key) { return translated(key); };
                auto now = information::stateName(d.key, d.actual, identifier, translate);
                auto want = information::stateName(d.key, d.expected, identifier, translate);
                auto const& named = now.labelIsKey ? now : want;
                std::string name = named.labelIsKey ? translated(named.label) : named.label;
                float half = dw * .45f;
                label(context,dx,sy,half-4,name,palette::dim);
                float vx = dx + half, nowW = textWidth(context, now.value);
                label(context,vx,sy,nowW+2,now.value,stateColor);
                vx += nowW + 3;
                changeArrow(context,vx,sy+1.5f+shapeTextDrop(),1,stateColor);
                vx += changeArrowWidth + 3;
                label(context,vx,sy,dx+dw-vx,want.value,stateColor);
                sy += 11;
            }
        }
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawSmallButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated("schematic.showInWorld"),
            over(ShapeZone::Action,0),verifySelected >= 0 ? palette::accentDeep : palette::keyFill,
            verifySelected >= 0 ? palette::accent : palette::keyEdge, verifySelected >= 0 ? palette::text : palette::faint);
        break;
    }
    case SchematicTab::Materials: {
        if (!checked) { paragraph(context,dx,l.detailTop+6,dw,translated("schematic.noSelection"),4,palette::faint); break; }
        label(context,dx,l.nameY+1+boxTextInset(),dw,checked->name);
        if (unloadable) paragraph(context,dx,l.previewY,dw,translated("schematic.notLoaded", loadProblem),3,palette::warning);
        else if (counting) label(context,dx,l.previewY,dw,translated("schematic.counting"),palette::dim);
        else {
            auto carried = carriedItems();
            std::uint64_t needed = 0, remaining = 0;
            int kinds = 0, shortKinds = 0;
            for (auto const* line : materialRows) {
                ++kinds;
                needed += line->needed;
                remaining += line->remaining();
                bool noItem = line->item.empty() || (line->entity && !schematic::items::iconStack(line->icon));
                if (line->remaining() && !noItem && carried[line->item] < line->remaining()) ++shortKinds;
            }
            label(context,dx,l.previewY,dw,translated("schematic.materials.kinds", kinds),palette::text);
            label(context,dx,l.previewY+11,dw,translated("schematic.materials.left", remaining, needed),palette::dim);
            label(context,dx,l.previewY+22,dw,translated("schematic.materials.short", shortKinds),shortKinds ? palette::warning : palette::accent);
        }
        label(context,dx,l.previewY+35,dw,layersText(*checked),palette::faint);
        stepperRow(0, "schematic.shownLayersOnly", {}, true, materialsShownOnly);
        stepperRow(1, "schematic.materials.hud", {}, true, Runtime::instance().preferences().schematic.hud);
        float y = l.fieldY(1) + ShapesLayout::rowHeight + 6;
        if (materialSelected >= 0 && materialSelected < static_cast<int>(materialRows.size())) {
            auto const& line = *materialRows[static_cast<size_t>(materialSelected)];
            drawItemIcon(context, line.icon, dx, y - 2, 12);
            label(context,dx+14,y,dw-14,line.name);
            label(context,dx,y+11,dw,translated("schematic.materials.leftAmount",
                amountText(line.remaining(), stackSizeOf(line))),palette::dim);
            y += 26;
        }
        // The rest of the pane is kept for raw materials from the game's recipes (L-116).
        bool missing = !missingMaterials().empty();
        fill(context,l.detailLeft,l.actionsY-2,l.detailWidth,1,palette::white,.14f);
        drawSmallButton(context,l.actionX(0),l.actionsY+2,l.firstActionWidth,12,translated("schematic.openCalculator"),
            over(ShapeZone::Action,0),missing ? palette::accentDeep : palette::keyFill,
            missing ? palette::accent : palette::keyEdge, missing ? palette::text : palette::faint);
        break;
    }
    }

    if (materialTip) {
        // Text is batched: draw what is under the tip first, or it lands on top.
        context.flushText(0, std::nullopt);
        float w = textWidth(context, materialTip->text) + 8, x = std::min(materialTip->x + 8, l.left + l.width - w - 2);
        fill(context,x,materialTip->y+10,w,13,palette::panel);
        frame(context,x,materialTip->y+10,w,13,palette::keyEdge);
        label(context,x+4,materialTip->y+12,w-6,materialTip->text,palette::text);
    }
    // Footer.
    fill(context,l.left,l.footerTop,l.width,1,palette::white,.14f);
    float textLeft = l.left + ShapesLayout::pad, available = l.width - 2 * ShapesLayout::pad;
    bool shortFooter = l.docked || displayed.shortFooter;
    std::string text = !error.empty() ? error : schematicDescription();
    if (shortFooter) label(context,textLeft,l.footerTop+3,available,std::move(text),error.empty() ? palette::text : palette::warning);
    else {
        paragraph(context,textLeft,l.footerTop+3,available,text,2,error.empty() ? palette::text : palette::warning);
        bool menuUnbound = input::effectiveChord(Runtime::instance().preferences().bindings, input::Action::SchematicMenu).empty();
        label(context,textLeft,l.footerTop+30,available,translated(editingSchematicField >= 0 ? "shape.numberHint"
            : menuUnbound ? "schematic.menuKeyHint" : "schematic.screenHint"),palette::faint);
    }
}
ShapesLayout fitSchematics(SettingsTable const& t, glm::vec2 size, bool docked) {
    auto l = ShapesLayout::fit(t, size.x, size.y, docked, schematicRowCount(), schematicListFirst, schematicFieldCount(),
        schematicFieldFirst, false, false);
    // The first action never runs into the one at the right edge.
    l.firstActionWidth = std::clamp(l.detailWidth - 2 * ShapesLayout::pad - ShapesLayout::deleteWidth - 4, 40.f, 96.f);
    // Materials has one action: the calculator link, readable across the pane.
    if (schematicTab == SchematicTab::Materials) l.firstActionWidth = std::max(40.f, l.detailWidth - 2 * ShapesLayout::pad);
    schematicsDisplayed = l;
    schematicListFirst = l.listFirst;
    schematicFieldFirst = l.fieldFirst;
    return l;
}
void renderSchematicsContent(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer, SettingsTable const& t) {
    auto l = fitSchematics(t, size, false);
    if (l.width > 0) drawSchematicsBody(context, l, pointer);
    context.flushText(0,std::nullopt);
}
void renderSchematicsDocked(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    auto l = fitSchematics(displayed, size, true);
    if (l.width <= 0) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    panel(context,l.left,l.top,l.width,l.height,.82f);
    frame(context,l.left,l.top,l.width,l.height,palette::white,.14f);
    label(context,l.left+ShapesLayout::pad,l.top+6,l.drawAllX-l.left-10,translated("nav.schematics"));
    bool closeHover = l.hit(pointer.x, pointer.y).zone == ShapeZone::Close;
    drawSmallButton(context,l.closeX,l.top+4,ShapesLayout::closeWidth,12,translated("closeButton"),
        closeHover,palette::keyFill,palette::keyEdge,closeHover ? palette::text : palette::dim);
    fill(context,l.left,l.top+ShapesLayout::headerHeight-1,l.width,1,palette::white,.14f);
    drawSchematicsBody(context, l, pointer);
    context.flushText(0,std::nullopt);
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
// Text being typed in a field of `height` from `top`: the selection as a
// highlight, otherwise a blinking bar after the text, so a typed "_" never
// looks like the caret. Both keep the same margin above and below.
void drawEditText(MinecraftUIRenderContext& context, float x, float top, float height, float width, SearchQuery const& input) {
    auto const& value = input.value();
    float w = std::min(textWidth(context, value), width - 2);
    float markTop = top + 2, markHeight = height - 4;
    if (input.selectedAll() && !value.empty()) fill(context, x - 1, markTop, w + 2, markHeight, palette::accentDeep);
    label(context, x, top + 1 + boxTextInset(), width, value);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!input.selectedAll() && ms / 530 % 2 == 0) fill(context, x + w + 1, markTop, 1, markHeight, palette::text);
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
void openSchematicTab(SchematicTab tab) {
    leaveMenu();
    selectNav(schematicsNav, true);
    refreshSchematics(true);
    selectSchematicTab(tab);
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
        if (auto option = settings::find(id)) adjustOption(*option, 1);
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
    case C::PlaceFile: case C::FilesTab: openSchematicTab(SchematicTab::Files); return;
    case C::DeletePlacement: case C::PlacedTab: openSchematicTab(SchematicTab::Placements); return;
    case C::CheckTab: openSchematicTab(SchematicTab::Verify); return;
    case C::MaterialsTab: openSchematicTab(SchematicTab::Materials); return;
    case C::KeySettings: leaveMenu(); openSchematicKeySettings(); return;
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
    waypointsFromMap = false;
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
        rebuild(true);
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
        waypointsFromMap = true;
        refreshWaypoints();
        if (request.index >= -1) selectWaypoint(request.index);
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
    applyShapeName();
    if (bindingEdit) {
        auto value = Runtime::instance().preferences();
        value.bindings[static_cast<size_t>(bindingEdit->action)] = bindingEdit->binding;
        bool saved = Runtime::instance().save(value);
        cancelCapture();
        error = saved ? std::string{} : translated("saveError");
    }
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
        if (std::exchange(pendingRelease, false)) { sliderDrag = nullptr; scrollDragFirst = nullptr; previewTurn.dragging = false; }
        if (previewTurn.dragging) {
            previewTurn.yaw = previewTurn.fromYaw + (lastPointer.x - previewTurn.from.x) * .7f;
            previewTurn.pitch = std::clamp(previewTurn.fromPitch + (lastPointer.y - previewTurn.from.y) * .7f, -60.f, 89.f);
        }
        if (scrollDragFirst && scrollDragLayout) *scrollDragFirst = scrollDragLayout->firstAt(lastPointer.y);
        if (shapesView()) {
            shapeList = overlay::shapes::list();
            if (auto click = std::exchange(pendingClick, std::nullopt)) handleShapeClick(click->x, click->y, click->right);
            for (int key : std::exchange(pendingKeys, {})) handleShapeKey(key);
        } else if (waypointsView()) {
            refreshWaypoints();
            if (auto click = std::exchange(pendingClick, std::nullopt)) handleWaypointClick(click->x, click->y, click->right);
            for (int key : std::exchange(pendingKeys, {})) handleWaypointKey(key);
        } else if (schematicsView()) {
            refreshSchematics(false);
            if (auto click = std::exchange(pendingClick, std::nullopt)) handleSchematicClick(click->x, click->y, click->right);
            for (int key : std::exchange(pendingKeys, {})) handleSchematicKey(key);
        } else {
            if (std::exchange(pendingSearch, false)) { finishNumber(); searchFocused = true; query.selectAll(); }
            if (auto click = std::exchange(pendingClick, std::nullopt))
                handleClick(displayed.hit(click->x, click->y, navCount, displayedTabWidth), click->right);
            for (int key : std::exchange(pendingKeys, {})) handleKey(key);
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
        shapeList = overlay::shapes::list();
        syncTextKeyboard(shapesDisplayed.stepperX(), editingShapeName ? shapesDisplayed.nameY : shapesDisplayed.fieldY(std::max(0, editingShapeField)));
        if (shapesDocked) renderShapesDocked(context, current, size, pointer);
        else renderTable(context, current, size, pointer);
    } else if (waypointsView()) {
        refreshWaypoints();
        syncTextKeyboard(waypointsDisplayed.stepperX(), editingWaypointName ? waypointsDisplayed.nameY
            : waypointsDisplayed.fieldY(std::max(0, editingWaypointField)));
        if (waypointsDocked) renderWaypointsDocked(context, size, pointer);
        else renderTable(context, current, size, pointer);
    } else if (schematicsView()) {
        refreshSchematics(false);
        syncTextKeyboard(schematicsDisplayed.stepperX(), schematicsDisplayed.fieldY(std::max(0, editingSchematicField)));
        if (schematicsDocked) renderSchematicsDocked(context, size, pointer);
        else renderTable(context, current, size, pointer);
    } else {
        float caretY = editingNumber && valid(selected) ? displayed.rowY(selected) : displayed.top + 4;
        syncTextKeyboard(editingNumber ? displayed.stepperX() : displayed.searchX, caretY);
        renderTable(context, current, size, pointer);
    }
}
}
void open(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene || !gameplayScreen(current.getScreenName())) return;
    CameraSessions::instance().suspendInput();
    error.clear(); rangeWarning.reset(); seen = false; closing = false; pendingClick.reset(); pendingKeys.clear(); pendingSearch = false;
    editingNumber = nullptr; editingShapeField = -1; editingShapeName = false; shapeNameDirty = false; numberDirty = false;
    query.clear(); searchCollapsed.clear(); uiHeld.clear(); searchFocused = false; capturing.reset(); bindingEdit.reset();
    // Category, expansion and scroll persist between openings in a session.
    navigation.reopenNormal();
    rebuild(true);
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
    refreshSchematics(true);
    if (tab >= 0 && tab < 4) selectSchematicTab(static_cast<SchematicTab>(tab));
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
    if (capturing) cancelCapture();
    finishNumber();
    searchFocused = false;
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
        if (capturing && !shapesView() && !waypointsView() && !schematicsView()) {
            if (down) event.cancel();
            auto hit = scaled ? displayed.hit(x, y, navCount, displayedTabWidth) : SettingsTable::Hit{};
            if (button == MouseAction::ActionLeft && down && hit.zone == Zone::Footer
                && displayed.footerButton(hit.x, hit.y) >= 0) pendingClick = Click{x, y, false};
            else captureInput(token, down);
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
            int step = event.buttonData() > 0 ? -3 : 3;
            auto const& l = schematicsDisplayed;
            float px = lastPointer.x, py = lastPointer.y;
            bool overList = px >= l.listLeft && px < l.listLeft + l.listWidth && (!l.docked || py < l.detailTop);
            if (overList) schematicListFirst = std::max(0, schematicListFirst + step);
            else schematicFieldFirst = std::max(0, schematicFieldFirst + step);
            return;
        }
        if (waypointsView() && wheel) {
            int step = event.buttonData() > 0 ? -3 : 3;
            auto const& l = waypointsDisplayed;
            float px = lastPointer.x, py = lastPointer.y;
            bool overList = px >= l.listLeft && px < l.listLeft + l.listWidth && (!l.docked || py < l.detailTop);
            if (overList) waypointListFirst = std::max(0, waypointListFirst + step);
            else waypointFieldFirst = std::max(0, waypointFieldFirst + step);
            return;
        }
        if (shapesView() && wheel) {
            // Scroll whichever pane is under the pointer; selection stays put.
            int step = event.buttonData() > 0 ? -3 : 3;
            auto const& l = shapesDisplayed;
            float px = lastPointer.x, py = lastPointer.y;
            bool overList = px >= l.listLeft && px < l.listLeft + l.listWidth && (!l.docked || py < l.detailTop);
            if (overList) shapeListFirst = std::max(0, shapeListFirst + step);
            else shapeFieldFirst = std::max(0, shapeFieldFirst + step);
            return;
        }
        if (wheel) {
            // The wheel scrolls the table; selection stays on its row.
            first = SettingsTable::clampFirst(first + (event.buttonData() > 0 ? -3 : 3),
                static_cast<int>(rows.size()), displayed.visible);
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
        if (capturing) {
            if (event.isDown()) event.cancel();
            if (event.isDown() && event.keyCode() == 0x1b) cancelCapture();
            else captureInput(token, event.isDown());
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
        if (textKeyboardOwned && (searchFocused || numericEditing() || editingShapeName || editingWaypointName)) {
            auto key = event.keyCode();
            bool commandKey = key == 0x08 || key == 0x1b || key == 0x0d || key == 0x09
                || (searchFocused && key == 0x28);
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
        bool editing = shapesView() ? (editingShapeName || editingShapeField >= 0)
            : waypointsView() ? (editingWaypointName || editingWaypointField >= 0)
            : schematicsView() ? editingSchematicField >= 0
            : (searchFocused || editingNumber != nullptr);
        bool finishes = (key == 0x1b || key == 0x0d || key == 0x09) && (tool || !searchFocused);
        if (editing && !finishes) {
            if (shapesView()) handleShapeKey(key);
            else if (waypointsView()) handleWaypointKey(key);
            else if (schematicsView()) handleSchematicKey(key);
            else handleKey(key);
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
