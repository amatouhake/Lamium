#include "ui/SettingsScreen.h"
#include "settings/Options.h"
#include "ui/SettingsRows.h"
#include "ui/SettingsTable.h"
#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "ui/SearchQuery.h"
#include "ui/NumberInput.h"
#include "ui/Widgets.h"
#include "overlay/ShapeSession.h"
#include "ui/Localization.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "features/information/InfoHud.h"
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
constexpr int navCount = static_cast<int>(sections.size()) + 4;
constexpr int hotkeysNav = navCount - 3;
constexpr int shapesNav = navCount - 2;
// The HUD layout editor replaces the whole panel; leaving returns to editorReturn.
constexpr int hudNav = navCount - 1;
int editorReturn = 0;
bool pendingRelease = false;
settings::Option const* sliderDrag = nullptr; // Slider being dragged with the left button.
int navIndex = 0;
std::set<std::string_view> expanded;
std::vector<SettingsRow> rows;
int selected = -1;
int first = 0;
// Keyboard navigation shows the selected row's key tooltip; moving the mouse
// hands it back to hover.
bool keyboardTip = false;
glm::vec2 tipPointer{};
SettingsTable displayed;
float displayedInverseScale = 0;
float displayedTabWidth = 0;
// GUI coordinates; resolved against the layout drawn in the next frame.
struct Click { float x, y; bool right; };
std::optional<Click> pendingClick;
std::vector<int> pendingKeys;
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
int shapeListFirst = 0, shapeFieldFirst = 0, shapeFieldSelected = -1, shapeLayer = 0;
shape::Reference shapeReference = shape::Reference::StandingBlock;
std::vector<overlay::shapes::Summary> shapeList;
ShapesLayout shapesDisplayed;
int editingShapeField = -1;
bool editingShapeName = false;
SearchQuery shapeNameInput;
bool shapeNameDirty = false;
bool numericEditing() { return editingNumber || editingShapeField >= 0; }

bool textHook = false;
bool textKeyboardOwned = false;
bool textKeyboardNumber = false;
std::optional<input::Action> capturing;
input::BindingCapture capture;
input::Chord uiHeld;
struct BindingEdit { input::Action action; std::optional<input::Chord> binding; };
std::optional<BindingEdit> bindingEdit;

std::string_view categoryKey() { return navIndex > 0 && navIndex < hotkeysNav ? sections[navIndex-1] : std::string_view{}; }
bool hotkeysView() { return navIndex == hotkeysNav; }
bool shapesView() { return navIndex == shapesNav; }
bool hudEditorView() { return navIndex == hudNav; }
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
        [](std::string_view key) { return translated(key); }, preferences.information.lineOrder);
    selected = -1;
    if (previous)
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i] == *previous) { selected = static_cast<int>(i); break; }
    if (selected < 0) selected = nextSelectable(0, 1);
    first = SettingsTable::clampFirst(first, static_cast<int>(rows.size()), displayed.visible);
}
void selectNav(int index) {
    // Choosing a category ends a search, which otherwise spans every category.
    query.clear();
    if (navIndex == shapesNav && index != shapesNav) { shapeDraft.reset(); shapePicking = false; overlay::shapes::setDraft({}); }
    index = std::clamp(index, 0, navCount - 1);
    if (index == hudNav && navIndex != hudNav) { editorReturn = navIndex; hud_editor::reset(); pendingRelease = false; }
    navIndex = index;
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
    bool wanted = !closing && !capturing && (searchFocused || numericEditing() || editingShapeName);
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
void queryChanged() { first = 0; rebuild(false); }
LL_TYPE_INSTANCE_HOOK(SettingsSearchText, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$handleTextChar, void, std::string const& text, FocusImpact impact) {
    std::lock_guard lock(mutex);
    if (scene.get() == this && ownsTop()) {
        // Coalesce native text events before persisting the whole workspace.
        // Never flush the world sidecar from inside a text callback.
        if (editingShapeName) { if (shapeNameInput.append(text)) shapeNameDirty = true; return; }
        if (numericEditing()) { if (numberInput.append(text)) numberDirty = true; return; }
        if (!capturing && searchFocused && query.append(text)) queryChanged();
        return;
    }
    origin(text, impact);
}
void clear() {
    sliderDrag = nullptr;
    releaseTextKeyboard(); editingNumber = nullptr; editingShapeField = -1; editingShapeName = false; shapeNameDirty = false;
    numberDirty = false; uiHeld.clear(); capturing.reset(); bindingEdit.reset(); capture.clear(); client = nullptr;
    scene.reset(); seen = false; closing = false; pendingClick.reset(); pendingKeys.clear();
    // A draft is never kept once the screen is gone.
    if (shapeDraft) { shapeDraft.reset(); overlay::shapes::setDraft({}); }
    shapePicking = false; shapeDeleteArmed = false;
}
void applyNumber() {
    if (!numericEditing() || !numberDirty) return;
    numberDirty = false;
    if (editingShapeField >= 0) {
        auto definition = currentShape();
        auto fields = definition ? shape::rows(*definition, shapeDraft.has_value()) : std::vector<shape::Row>{};
        if (!definition || editingShapeField >= static_cast<int>(fields.size())) { editingShapeField = -1; return; }
        auto field = fields[editingShapeField].field;
        auto range = shape::numeric(*definition, field);
        if (!range) { editingShapeField = -1; return; }
        auto parsed = numberInput.parsedPrecise(range->minimum,range->maximum,range->integer);
        if (!parsed) {
            error = translated(range->integer ? "integerRange" : "numberRange",range->minimum,range->maximum);
            return;
        }
        if (*parsed == range->value) { error.clear(); return; }
        applyShape(shape::setNumber(*definition, field, *parsed));
        return;
    }
    auto const& range = *editingNumber->numeric;
    auto parsed = numberInput.parsed(range.minimum, range.maximum);
    if (!parsed) { error = translated("numberRange", range.minimum, range.maximum); return; }
    auto value = Runtime::instance().preferences();
    if (std::get<float>(editingNumber->read(value)) == *parsed) { error.clear(); return; }
    range.write(value, *parsed);
    error = Runtime::instance().save(value) ? std::string{} : translated("saveError");
}
void finishNumber() {
    applyNumber();
    applyShapeName();
    releaseTextKeyboard();
    editingNumber = nullptr; editingShapeField = -1; editingShapeName = false; numberDirty = false;
}
void close() {
    releaseTextKeyboard();
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
void toggleFeature(FeatureInfo const& feature) {
    if (auto option = settings::find(feature.toggle)) adjustOption(*option, 1);
}
void setExpanded(int row, bool open) {
    if (!valid(row) || !rows[row].heading() || !rows[row].children) return;
    if (query.value().find_first_not_of(' ') != std::string::npos) return; // Search controls expansion.
    auto id = rows[row].feature->id;
    if (open == expanded.contains(id)) return;
    selected = row;
    if (open) expanded.insert(id); else expanded.erase(id);
    // Rows above the feature are unchanged, so it keeps its index and screen
    // position. Reveal new children only as far as the feature stays visible.
    rebuild(true);
    if (open && displayed.visible > 0) {
        int last = row + rows[row].children;
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
void activateRow(int row, bool space) {
    if (!valid(row)) return;
    auto const& entry = rows[row];
    switch (entry.kind) {
    case RowKind::Section: return;
    case RowKind::Feature:
        if (space && !entry.feature->toggle.empty()) { toggleFeature(*entry.feature); return; }
        if (entry.children) { setExpanded(row, !entry.expanded); return; }
        if (!entry.feature->toggle.empty()) { toggleFeature(*entry.feature); return; }
        if (auto primary = primaryAction(*entry.feature)) startCapture(*primary);
        return;
    case RowKind::Option:
        if (entry.option->numeric) beginNumber(*entry.option);
        else adjustOption(*entry.option, 1);
        return;
    case RowKind::Action: startCapture(*entry.action); return;
    case RowKind::Layout: openLayout(*entry.layout); return;
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
void handleClick(SettingsTable::Hit const& hit, bool right) {
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
        int part = displayed.stepperPart(hit.x);
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
    case 0x09: selectNav((navIndex + (heldShift() ? hudNav - 1 : 1)) % hudNav); break;
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
std::string actionLabel(input::Action action) {
    return actionName(translated("key.Lamium." + std::string(input::actions[static_cast<size_t>(action)].id)));
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
        if (auto number = std::get_if<float>(&value)) return std::vformat(pattern, std::make_format_args(*number));
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
                 bool editing) {
    float x = displayed.stepperX(), w = displayed.stepperWidth(), aw = SettingsTable::arrowWidth;
    float cy = y + 1, h = SettingsTable::rowHeight - 2;
    fill(context,x,cy,aw,h,palette::keyFill);
    fill(context,x+w-aw,cy,aw,h,palette::keyFill);
    frame(context,x,cy,w,h,editing ? palette::accent : palette::keyEdge);
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
    label(context,x+aw+1,cy+1+boxTextInset(),w-2*aw-2,std::move(text),palette::text,Align::Center);
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
    switch (entry.kind) {
    case RowKind::Feature: {
        auto text = translated(entry.feature->description);
        if (auto primary = primaryAction(*entry.feature)) text += " " + behaviorText(*primary);
        return text;
    }
    case RowKind::Option: {
        if (entry.option->numeric) {
            auto const& range = *entry.option->numeric;
            return translated(editingNumber ? "numberRange" : "numberControl", range.minimum, range.maximum);
        }
        auto helpKey = "help." + std::string(entry.option->id);
        auto help = translated(helpKey);
        return help != helpKey ? help : translated(entry.feature->description);
    }
    case RowKind::Action:
        return (hotkeysView() ? featureName(*entry.feature) + ": " : std::string{}) + behaviorText(*entry.action);
    case RowKind::Layout: return translated("help.layoutLink");
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
    if (!shapesDocked) {
        auto nav = displayed.hit(x, y, navCount, displayedTabWidth);
        if (nav.zone == Zone::Nav) { selectNav(nav.index); return; }
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
    case ShapeZone::Field: activateShapeField(hit.index, right ? -1 : hit.part); return;
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
    case 0x09: selectNav((navIndex + (heldShift() ? hudNav - 1 : 1)) % hudNav); break;
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
    // 5x5 glyphs drawn from rectangles: ring, stacked ring, ball, grid.
    static constexpr std::array<char const*,4> icons{
        ".###.#...##...##...#.###.", ".###.#####...###...#.###.",
        ".###.##########.####.###.", "#.#.######.#.######.#.#.#"};
    auto pattern = icons[static_cast<size_t>(std::clamp(type, 0, 3))];
    for (int i = 0; i < 25 && pattern[i]; ++i)
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
    float nameX = l.listLeft + ShapesLayout::pad + 12;
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
        fill(context,l.listLeft+ShapesLayout::pad,y+4,6,6,isDraft ? draftRgb : shapeRgb(shown.color),shown.visible && !elsewhere ? 1.f : .35f);
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
        std::string name = editingShapeName
            ? (shapeNameInput.selectedAll() ? "[" + shapeNameInput.value() + "]" : shapeNameInput.value() + "_") : definition->name;
        label(context,dx+17,l.nameY+1+boxTextInset(),dw-20,std::move(name));
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
void renderTable(MinecraftUIRenderContext& context, IClientInstance& current, glm::vec2 size, glm::vec2 pointer) {
    auto const t = SettingsTable::fit(size.x, size.y, static_cast<int>(rows.size()), first);
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
    label(context,t.left+SettingsTable::pad,t.top+6,80,"Lamium");
    if (shapesView()) {
        label(context,t.left+SettingsTable::pad+textWidth(context,"Lamium")+5,t.top+6,80,"> " + translated("nav.shapes"),palette::faint);
    } else {
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
    label(context,t.closeX,t.top+5+boxTextInset(),SettingsTable::closeWidth,translated("closeButton"),
        closeHover ? palette::text : palette::dim,Align::Center);
    fill(context,t.left,t.top+SettingsTable::headerHeight-1,t.width,1,palette::white,.14f);

    // Categories: sidebar, or tabs when narrow.
    // A query searches every category, so the navigation shows "All" meanwhile.
    bool searching = query.value().find_first_not_of(' ') != std::string::npos;
    int activeNav = searching && !hotkeysView() ? 0 : navIndex;
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
            if (active) { fill(context,x,y,w,SettingsTable::navItemHeight,palette::accent,.16f); fill(context,x,y,2,SettingsTable::navItemHeight,palette::accent); }
            else if (over) fill(context,x,y,w,SettingsTable::navItemHeight,palette::white,.07f);
            std::string count = i > 0 && i < hotkeysNav ? sectionCount(sections[i-1], preferences)
                : i == shapesNav ? std::to_string(overlay::shapes::list().size()) : std::string{};
            float countWidth = count.empty() ? 0 : textWidth(context, count) + 4;
            label(context,x+7,y+3,w-12-countWidth,navLabel(i,false),active || over ? palette::text : palette::dim);
            if (!count.empty()) label(context,x+w-5-countWidth,y+3,countWidth,count,palette::faint,Align::Right);
        }
    }

    if (shapesView()) { renderShapesContent(context, current, size, pointer, t); return; }

    // Column headings.
    float theadY = t.theadTop + 2;
    if (hotkeysView()) {
        label(context,t.nameX,theadY,t.keyX-t.nameX-SettingsTable::gap,translated("column.action"),palette::faint);
    } else {
        label(context,t.nameX,theadY,t.stateX-t.nameX-SettingsTable::gap,translated("column.feature"),palette::faint);
        label(context,t.stateX-6,theadY,SettingsTable::stateWidth+12,translated("column.state"),palette::faint,Align::Center);
    }
    label(context,t.keyX,theadY,t.keyWidth,translated("column.key"),palette::faint);
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
            if (auto primary = primaryAction(*entry.feature)) drawKeyCell(context,current,y,*primary);
            break;
        }
        case RowKind::Option: {
            drawGuide(context,y,entry.lastChild);
            auto value = entry.option->read(preferences);
            auto name = splitLabel(translated(entry.option->label)).name;
            if (auto flag = std::get_if<bool>(&value)) {
                label(context,t.nameX+12,y+3,nameRight-t.nameX-12,std::move(name),palette::dim);
                toggleSwitch(context,t.stateX+(SettingsTable::stateWidth-switchWidth)/2,y+(SettingsTable::rowHeight-switchHeight)/2,*flag);
            } else {
                bool asSlider = entry.option->numeric && entry.option->numeric->step > 0 && editingNumber != entry.option;
                float nameEnd = asSlider ? t.sliderX() : t.stepperX();
                label(context,t.nameX+12,y+3,nameEnd-SettingsTable::gap-t.nameX-12,std::move(name),palette::dim);
                if (asSlider) {
                    auto const& range = *entry.option->numeric;
                    float number = std::get<float>(value);
                    slider(context,t.sliderX(),y+2,t.sliderWidth(),
                        SettingsTable::sliderPosition(number,range.minimum,range.maximum),sliderDrag == entry.option);
                    label(context,t.sliderValueX(),y+3,SettingsTable::sliderValueWidth,optionValueText(*entry.option,value),
                        palette::text,Align::Right);
                } else {
                    drawStepper(context,y,*entry.option,optionValueText(*entry.option,value),editingNumber == entry.option);
                }
            }
            break;
        }
        case RowKind::Action: {
            if (hotkeysView()) {
                drawName(context,t.nameX,y,t.keyX-SettingsTable::gap,actionLabel(*entry.action),palette::text,0,entry.feature->experimental);
            } else {
                drawGuide(context,y,entry.lastChild);
                label(context,t.nameX+12,y+3,nameRight-t.nameX-12,actionLabel(*entry.action),palette::dim);
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
        paragraph(context,textLeft,t.footerTop+3,textWidthAvailable,description(),2);
        std::string hint = !error.empty() ? error : translated(searchFocused ? "searchHint" : editingNumber ? "numberHint" : "tableHint");
        label(context,textLeft,t.footerTop+30,textWidthAvailable,std::move(hint),error.empty() ? palette::faint : palette::warning);
    }
    if (!capturing)
        if (auto target = tipTarget(t, hover)) drawConflictTip(context,current,t,target->first,target->second);
    context.flushText(0,std::nullopt);
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
        if (gameplayScreen(current.getScreenName()) && hudView(view))
            information::drawHud(context,size.x,size.y,Runtime::instance().preferences().information);
        return;
    }
    if (&current != client) return;
    if (!ownsTop()) { if (seen) clear(); return; }
    seen = true;
    applyNumber();
    applyShapeName();
    if (bindingEdit) {
        auto value = Runtime::instance().preferences();
        value.bindings[static_cast<size_t>(bindingEdit->action)] = bindingEdit->binding;
        bool saved = Runtime::instance().save(value);
        cancelCapture();
        error = saved ? std::string{} : translated("saveError");
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
        if (std::exchange(pendingRelease, false)) sliderDrag = nullptr;
        if (shapesView()) {
            shapeList = overlay::shapes::list();
            if (auto click = std::exchange(pendingClick, std::nullopt)) handleShapeClick(click->x, click->y, click->right);
            for (int key : std::exchange(pendingKeys, {})) handleShapeKey(key);
        } else {
            if (auto click = std::exchange(pendingClick, std::nullopt))
                handleClick(displayed.hit(click->x, click->y, navCount, displayedTabWidth), click->right);
            for (int key : std::exchange(pendingKeys, {})) handleKey(key);
        }
    }
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
    Zoom::instance().reset();
    error.clear(); seen = false; closing = false; pendingClick.reset(); pendingKeys.clear();
    editingNumber = nullptr; editingShapeField = -1; editingShapeName = false; shapeNameDirty = false; numberDirty = false;
    query.clear(); uiHeld.clear(); searchFocused = false; capturing.reset(); bindingEdit.reset();
    // Category, expansion and scroll persist between openings in a session.
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
    if (scene) selectNav(shapesNav);
}
void openHotkeys(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (!scene) open(current);
    if (scene) selectNav(hotkeysNav);
}
void openHudLayout(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (!scene) open(current);
    if (scene) openLayout(std::nullopt);
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
        if (capturing && !shapesView()) {
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
        if (hudEditorView() && wheel) {
            if (scaled) hud_editor::wheel(event.buttonData() > 0 ? -3 : 3, x, y);
            return;
        }
        if (shapesView() && wheel) {
            // Scroll whichever pane is under the pointer; selection stays put.
            int step = event.buttonData() > 0 ? -3 : 3;
            auto const& l = shapesDisplayed;
            bool overList = scaled && x >= l.listLeft && x < l.listLeft + l.listWidth && (!l.docked || y < l.detailTop);
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
        // Keep search reachable from anywhere in the table. Capture handles
        // keys above this point, so Ctrl+F remains bindable.
        if (!shapesView() && event.keyCode() == 0x46 && heldCtrl()) {
            event.cancel();
            finishNumber();
            searchFocused = true;
            query.selectAll();
            return;
        }
        // Native text generation happens after HID onKeyDown. Keep editing
        // commands here, but let the focused native keyboard process the other
        // keys (including layout/IME input) while our modal scene owns gameplay.
        if (textKeyboardOwned && (searchFocused || numericEditing() || editingShapeName)) {
            auto key = event.keyCode();
            bool commandKey = key == 0x08 || key == 0x1b || key == 0x0d || key == 0x09
                || (searchFocused && key == 0x28);
            bool selectAll = key == 0x41 && heldCtrl();
            if (!commandKey && !selectAll) return;
        }
        event.cancel();
        // Editing keys act immediately so rapid typing keeps its order; the
        // remaining navigation runs with the next frame's layout.
        if (shapesView() ? (editingShapeName || editingShapeField >= 0) : (searchFocused || editingNumber != nullptr)) {
            if (shapesView()) handleShapeKey(event.keyCode()); else handleKey(event.keyCode());
            return;
        }
        pendingKeys.push_back(event.keyCode());
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
