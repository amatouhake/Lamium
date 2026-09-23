#include "ui/SettingsScreen.h"
#include "settings/Options.h"
#include "ui/SettingsLayout.h"
#include "ui/SettingsRows.h"
#include "ui/SettingsTable.h"
#include "ui/SearchQuery.h"
#include "ui/NumberInput.h"
#include "ui/Widgets.h"
#include "ui/ShapePanel.h"
#include "ui/Localization.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "features/information/InfoHud.h"
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
#include "mc/deps/core/math/Vec2.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/deps/input/MouseAction.h"
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
bool seen = false;
bool closing = false;
std::string error;

// Settings table. Navigation items: All, each section, then Hotkeys.
constexpr int navCount = static_cast<int>(sections.size()) + 2;
constexpr int hotkeysNav = navCount - 1;
int navIndex = 0;
std::set<std::string_view> expanded;
std::vector<SettingsRow> rows;
int selected = -1;
int first = 0;
SettingsTable displayed;
float displayedInverseScale = 0;
float displayedTabWidth = 0;
struct Click { SettingsTable::Hit hit; bool right; };
std::optional<Click> pendingClick;
std::vector<int> pendingKeys;
SearchQuery query;
bool searchFocused = false;
settings::Option const* editingNumber = nullptr;
NumberInput numberInput;
bool numberDirty = false;

// Shape Manager keeps the list presentation it was built with.
bool shapeView = false;
ShapePanel shapePanel;
int shapeSelected = 0;
int shapeFirst = 0;
int shapeHovered = -1;
SettingsLayout shapeLayout;
int shapeCommand = 0;
int shapeCommandRow = 0;
int editingShapeRow = -1;
int editingShapeName = -1;
SearchQuery shapeNameInput;
bool shapeNameDirty = false;
bool numericEditing() { return editingNumber || editingShapeRow >= 0; }

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
bool valid(int row) { return row >= 0 && row < static_cast<int>(rows.size()); }
int nextSelectable(int from, int step) {
    for (int row = from; valid(row); row += step) if (rows[row].selectable()) return row;
    return -1;
}
void rebuild(bool keepSelection) {
    std::optional<SettingsRow> previous;
    if (keepSelection && valid(selected)) previous = rows[selected];
    rows = buildSettingsRows(hotkeysView(), categoryKey(), query, expanded, [](std::string_view key) { return translated(key); });
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
    navIndex = std::clamp(index, 0, navCount - 1);
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
    bool wanted = !closing && !capturing && (searchFocused || numericEditing() || editingShapeName >= 0);
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
void applyShapeName() {
    if (editingShapeName < 0 || !std::exchange(shapeNameDirty, false)) return;
    try { shapePanel.rename(shapeNameInput.value()); error.clear(); }
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
        if (editingShapeName >= 0) { if (shapeNameInput.append(text)) shapeNameDirty = true; return; }
        if (numericEditing()) { if (numberInput.append(text)) numberDirty = true; return; }
        if (!capturing && searchFocused && query.append(text)) queryChanged();
        return;
    }
    origin(text, impact);
}
void clear() { releaseTextKeyboard(); editingNumber = nullptr; editingShapeRow = -1; editingShapeName = -1; shapeNameDirty = false; numberDirty = false; uiHeld.clear(); capturing.reset(); bindingEdit.reset(); capture.clear(); client = nullptr; scene.reset(); seen = false; closing = false; pendingClick.reset(); pendingKeys.clear(); shapeCommand = 0; shapeHovered = -1; }
void applyNumber() {
    if (!numericEditing() || !numberDirty) return;
    numberDirty = false;
    if (editingShapeRow >= 0) {
        auto range = shapePanel.numeric(editingShapeRow);
        if (!range) { editingShapeRow = -1; return; }
        auto parsed = numberInput.parsedPrecise(range->minimum,range->maximum,range->integer);
        if (!parsed) {
            error = translated(range->integer ? "integerRange" : "numberRange",range->minimum,range->maximum);
            return;
        }
        try { shapePanel.setNumber(editingShapeRow,*parsed); error.clear(); }
        catch (overlay::ShapeSaveError const&) { error = translated("shape.saveError"); }
        catch (std::exception const&) { error = translated("shape.editError"); }
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
    editingNumber = nullptr; editingShapeRow = -1; editingShapeName = -1; numberDirty = false;
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
void openShapes() {
    finishNumber();
    shapePanel.open(); shapeView = true; shapeSelected = 0; shapeFirst = 0; shapeLayout = {};
}
void beginNumber(settings::Option const& option) {
    editingNumber = &option;
    numberInput.begin(std::get<float>(option.read(Runtime::instance().preferences())));
    error.clear();
}
// Enter / Space / click on the name of a row.
void activateRow(int row, bool space) {
    if (!valid(row)) return;
    auto const& entry = rows[row];
    switch (entry.kind) {
    case RowKind::Section: return;
    case RowKind::Feature:
        if (isTool(*entry.feature)) { openShapes(); return; }
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
            if (button == 2) cancelCapture();
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
            if (isTool(*entry.feature)) openShapes();
            else if (auto primary = primaryAction(*entry.feature)) startCapture(*primary);
        } else if (entry.children) setExpanded(hit.index, !entry.expanded);
        return;
    case RowKind::Option: {
        auto value = entry.option->read(Runtime::instance().preferences());
        if (std::holds_alternative<bool>(value)) {
            if (hit.column != Column::Name) adjustOption(*entry.option, 1);
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
    case 0x26: moveSelection(-1); break;
    case 0x28: moveSelection(1); break;
    case 0x21: moveSelection(-page); break;
    case 0x22: moveSelection(page); break;
    case 0x24: selected = -1; moveSelection(1); break; // Home
    case 0x23: selected = static_cast<int>(rows.size()); moveSelection(-1); break; // End
    case 0x09: selectNav((navIndex + (heldShift() ? navCount - 1 : 1)) % navCount); break;
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
bool sharedBinding(input::Action action) {
    auto const preferences = Runtime::instance().preferences();
    auto const& bindings = preferences.bindings;
    auto const& binding = bindings[static_cast<size_t>(action)];
    if (!binding || binding->empty()) return false;
    for (size_t other = 0; other < bindings.size(); ++other)
        if (other != static_cast<size_t>(action) && bindings[other] == binding) return true;
    return false;
}
void drawKeyCell(MinecraftUIRenderContext& context, IClientInstance& current, float y, input::Action action) {
    float x = displayed.keyX, width = displayed.keyWidth, cy = y + (SettingsTable::rowHeight - capHeight) / 2;
    if (capturing == action) {
        fill(context,x,cy-1,width,capHeight+2,palette::accent,.25f);
        frame(context,x,cy-1,width,capHeight+2,palette::accent);
        auto text = capture.value().empty() ? translated("captureBox") : bindingChordName(current, capture.value());
        label(context,x+3,cy+boxTextInset(),width-6,std::move(text));
        return;
    }
    auto keys = bindingKeys(current, action);
    if (keys.empty()) { label(context,x,cy+1,width,translated("unbound"),palette::faint); return; }
    float used = keycaps(context,x,cy,width,keys);
    if (sharedBinding(action)) {
        auto text = translated("shared");
        float w = textWidth(context, text) + 4;
        if (used + 3 + w <= width) {
            frame(context,x+used+3,cy,w,capHeight,palette::warning);
            label(context,x+used+5,cy+boxTextInset(),w-3,std::move(text),palette::warning);
        }
    }
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
    default: return {};
    }
}
void renderTable(MinecraftUIRenderContext& context, IClientInstance& current, glm::vec2 size, glm::vec2 pointer) {
    auto const t = SettingsTable::fit(size.x, size.y, static_cast<int>(rows.size()), first);
    displayed = t;
    first = t.first;
    fill(context,0,0,size.x,size.y,Rgb{0,0,0},.2f);
    if (!t.usable()) {
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    auto const preferences = Runtime::instance().preferences();
    auto hover = t.hit(pointer.x, pointer.y, navCount, displayedTabWidth);
    panel(context,t.left,t.top,t.width,t.height,.8f);
    frame(context,t.left,t.top,t.width,t.height,palette::white,.14f);

    // Header: title, search field, Close.
    label(context,t.left+SettingsTable::pad,t.top+6,80,"Lamium");
    fill(context,t.searchX,t.top+4,t.searchWidth,12,Rgb{0,0,0},.45f);
    frame(context,t.searchX,t.top+4,t.searchWidth,12,searchFocused ? palette::accent : palette::keyEdge);
    if (searchFocused && query.selectedAll() && !query.value().empty())
        fill(context,t.searchX+3,t.top+5,std::min(t.searchWidth-6,textWidth(context,query.value())),10,palette::accent,.35f);
    if (query.value().empty() && !searchFocused)
        label(context,t.searchX+4,t.top+5+boxTextInset(),t.searchWidth-8,translated("searchPlaceholder") + "  Ctrl+F",palette::faint);
    else label(context,t.searchX+4,t.top+5+boxTextInset(),t.searchWidth-8,query.value() + (searchFocused ? "_" : ""));
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
            float y = i == hotkeysNav ? t.navBottom - 4 - SettingsTable::navItemHeight : t.navItemY(i);
            float x = t.left + 1, w = SettingsTable::sidebarWidth - 2;
            bool active = i == activeNav, over = hover.zone == Zone::Nav && hover.index == i;
            if (i == hotkeysNav) fill(context,x+6,y-4,w-12,1,palette::white,.14f);
            if (active) { fill(context,x,y,w,SettingsTable::navItemHeight,palette::accent,.16f); fill(context,x,y,2,SettingsTable::navItemHeight,palette::accent); }
            else if (over) fill(context,x,y,w,SettingsTable::navItemHeight,palette::white,.07f);
            std::string count = i > 0 && i < hotkeysNav ? sectionCount(sections[i-1], preferences) : std::string{};
            float countWidth = count.empty() ? 0 : textWidth(context, count) + 4;
            label(context,x+7,y+3,w-12-countWidth,navLabel(i,false),active || over ? palette::text : palette::dim);
            if (!count.empty()) label(context,x+w-5-countWidth,y+3,countWidth,count,palette::faint,Align::Right);
        }
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
            if (isTool(*entry.feature)) {
                auto text = translated("open");
                float w = std::min(t.keyWidth, textWidth(context, text) + 14);
                bool over = hover.zone == Zone::Row && hover.index == i && hover.column == Column::Key;
                fill(context,t.keyX,y+2,w,SettingsTable::rowHeight-4,over ? palette::accent : palette::accentDeep);
                label(context,t.keyX,y+3,w,std::move(text),palette::text,Align::Center);
            } else if (auto primary = primaryAction(*entry.feature)) drawKeyCell(context,current,y,*primary);
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
                label(context,t.nameX+12,y+3,t.stepperX()-SettingsTable::gap-t.nameX-12,std::move(name),palette::dim);
                drawStepper(context,y,*entry.option,optionValueText(*entry.option,value),editingNumber == entry.option);
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
        std::array<std::string,3> names{translated("clearShort"),translated("resetShort"),translated("cancelShort")};
        for (int i = 0; i < 3; ++i) {
            float x = t.footerButtonX(i), y = t.footerButtonY();
            bool over = hover.zone == Zone::Footer && t.footerButton(hover.x, hover.y) == i;
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
    context.flushText(0,std::nullopt);
}

// ---- Shape Manager list ----
void activateShape(int row, int direction) {
    finishNumber();
    if (direction == 0) {
        if (auto name = shapePanel.nameAt(row)) {
            editingShapeName = row; shapeNameInput.clear(); shapeNameInput.append(*name);
            shapeNameInput.selectAll(); error.clear(); return;
        }
        if (auto range = shapePanel.numeric(row)) {
            editingShapeRow = row; numberInput.beginPrecise(range->value); error.clear(); return;
        }
    }
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    auto position = player->getPosition();
    bool wasEditing = shapePanel.isEditing();
    try {
        if (shapePanel.activate(row,direction,{position.x,position.y,position.z},static_cast<int>(player->getDimensionId()))) {
            shapeView = false; rebuild(true);
        } else if (wasEditing != shapePanel.isEditing()) { shapeSelected = 0; shapeFirst = 0; }
        shapeSelected = std::clamp(shapeSelected,0,std::max(0,shapePanel.count()-1));
        shapeLayout = {};
        error.clear();
    } catch (overlay::ShapeSaveError const&) { error = translated("shape.saveError"); }
    catch (std::exception const&) { error = translated("shape.editError"); }
}
void renderShapes(MinecraftUIRenderContext& context, glm::vec2 size, glm::vec2 pointer) {
    int count = shapePanel.count();
    auto layout = SettingsLayout::fit(size.x, size.y, count, shapeSelected, shapeFirst);
    shapeLayout = layout;
    shapeFirst = layout.first;
    fill(context,0,0,size.x,size.y,Rgb{0,0,0},.2f);
    if (!layout.visible) {
        shapeHovered = -1;
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    float width = layout.width, left = layout.left, top = layout.top;
    panel(context,left-6,top-6,width+12,layout.bottom+6-top);
    frame(context,left-6,top-6,width+12,layout.bottom+6-top,palette::white,.14f);
    label(context,left,top,width,shapePanel.title());
    if (layout.subtitle) label(context,left,top+16,width,shapePanel.subtitle(),palette::faint);
    shapeHovered = layout.hit(pointer.x, pointer.y);
    for (int i=layout.first;i<layout.first+layout.visible;++i) {
        float y = layout.rowY(i);
        if (i % 2) fill(context,left,y,width,SettingsLayout::rowHeight,palette::white,.025f);
        rowBackground(context,left,y,width,SettingsLayout::rowHeight,shapeSelected == i,shapeHovered == i);
        auto text = shapePanel.label(i);
        if (editingShapeName == i) text = translated("shape.name",
            shapeNameInput.selectedAll() ? "[" + shapeNameInput.value() + "]" : shapeNameInput.value() + "_");
        else if (editingShapeRow == i) text = translated("numberInput",text,
            numberInput.selectedAll() ? "[" + numberInput.value() + "]" : numberInput.value() + "_");
        label(context,left+6,y+2,width-16,std::move(text));
    }
    if (layout.visible < count) {
        float trackHeight = layout.visible * SettingsLayout::rowPitch - 2;
        float thumbHeight = std::max(8.0f, trackHeight * layout.visible / count);
        float thumbY = layout.rowsTop + (trackHeight-thumbHeight) * layout.first / (count-layout.visible);
        fill(context,left+width-3,layout.rowsTop,2,trackHeight,palette::white,.08f);
        fill(context,left+width-3,thumbY,2,thumbHeight,palette::keyEdge);
    }
    label(context,left,layout.footer,width,error.empty() ? translated(editingShapeName >= 0 || numericEditing()
        ? "shape.numberHint" : "shape.controls") : error, error.empty() ? palette::faint : palette::warning);
    if (layout.secondHint) {
        auto text = ShapePanel::storageDescription();
        if (auto range = shapePanel.numeric(shapeSelected))
            text = translated(range->integer ? "integerRange" : "numberRange",range->minimum,range->maximum);
        paragraph(context,left,layout.footer+15,width,text,3,palette::dim);
    }
    context.flushText(0,std::nullopt);
}
void handleShapeKey(int key) {
    int count = shapePanel.count();
    if (editingShapeName >= 0) {
        switch (key) {
        case 0x08: if (shapeNameInput.backspace()) shapeNameDirty = true; break;
        case 0x41: if (heldCtrl()) shapeNameInput.selectAll(); break;
        case 0x1b: case 0x0d: finishNumber(); break;
        case 0x09: finishNumber(); shapeSelected = (shapeSelected+1)%count; break;
        }
        return;
    }
    if (editingShapeRow >= 0) {
        switch (key) {
        case 0x08: if (numberInput.backspace()) numberDirty = true; break;
        case 0x41: if (heldCtrl()) numberInput.selectAll(); break;
        case 0x1b: case 0x0d: finishNumber(); break;
        case 0x09: finishNumber(); shapeSelected = (shapeSelected+1)%count; break;
        }
        return;
    }
    switch (key) {
    case 0x1b: activateShape(0,0); break; // Back
    case 0x26: shapeSelected = (shapeSelected+count-1)%count; break;
    case 0x28: case 0x09: shapeSelected = (shapeSelected+1)%count; break;
    case 0x24: shapeSelected = 0; break;
    case 0x23: shapeSelected = count-1; break;
    case 0x21: shapeSelected = std::max(0, shapeSelected-std::max(1, shapeLayout.visible-1)); break;
    case 0x22: shapeSelected = std::min(count-1, shapeSelected+std::max(1, shapeLayout.visible-1)); break;
    case 0x25: activateShape(shapeSelected,-1); break;
    case 0x27: activateShape(shapeSelected,1); break;
    case 0x0d: case 0x20: activateShape(shapeSelected,0); break;
    }
}

void render(ll::event::UIRenderEvent& event) {
    std::lock_guard lock(mutex);
    auto& context = event.uiRenderContext();
    auto& current = context.mClient;
    auto& view = event.screenView();
    glm::vec2 size = view.mSize;
    if (!scene) {
        if (gameplayScreen(current.getScreenName()))
            information::drawHud(context,size.x,size.y,Runtime::instance().preferences().information);
        if (gameplayScreen(current.getScreenName()) && Runtime::instance().preferences().ui.gameplayHints) {
            label(context, 6, 6, size.x-12, gameplayKeyHint(current));
            context.flushText(0, std::nullopt);
        }
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
    if (!closing) {
        if (shapeView) {
            int action = std::exchange(shapeCommand, 0);
            if (action == 1 || action == -1) activateShape(shapeCommandRow, action);
            if (action == 3) activateShape(shapeCommandRow, 0);
            for (int key : std::exchange(pendingKeys, {})) handleShapeKey(key);
        } else {
            if (auto click = std::exchange(pendingClick, std::nullopt)) handleClick(click->hit, click->right);
            for (int key : std::exchange(pendingKeys, {})) handleKey(key);
        }
    }
    if (!scene) return;
    information::drawHud(context,size.x,size.y,Runtime::instance().preferences().information);
    displayedInverseScale = current.getGuiData()->mInvGuiScale;
    glm::vec2 pointer = view.mPointerLocationPrevious;
    if (shapeView) {
        syncTextKeyboard(shapeLayout.left, shapeLayout.rowY(shapeSelected));
        renderShapes(context, size, pointer);
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
    editingNumber = nullptr; editingShapeRow = -1; editingShapeName = -1; shapeNameDirty = false; numberDirty = false;
    query.clear(); uiHeld.clear(); searchFocused = false; shapeView = false; capturing.reset(); bindingEdit.reset();
    // Category, expansion and scroll persist between openings in a session.
    rebuild(true);
    // This native information screen supplies focus/cursor ownership. It has no
    // form ID, packet, or server callback. Lamium draws and handles its own UI.
    scene = current.getSceneFactory().createCommonDialogInfoScreen("Lamium", "");
    if (!scene) return;
    client = &current;
    current.getSceneFactory().getCurrentSceneStack()->pushScreen(scene, false);
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
    if (!startLocalization()) throw std::runtime_error("Could not install Lamium action translations");
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
        if (scene && seen && &event.uiRenderContext().mClient == client && !ownsTop()) clear();
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
        if (capturing && !shapeView) {
            if (down) event.cancel();
            auto hit = scaled ? displayed.hit(x, y, navCount, displayedTabWidth) : SettingsTable::Hit{};
            if (button == MouseAction::ActionLeft && down && hit.zone == Zone::Footer
                && displayed.footerButton(hit.x, hit.y) >= 0) pendingClick = Click{hit, false};
            else captureInput(token, down);
            return;
        }
        // A button may already be down when F8 opens the panel. Let vanilla
        // observe its release, just as we do for keys, so it cannot stay held.
        if (!wheel && event.buttonData() == MouseAction::DataUp) return;
        event.cancel();
        if (shapeView) {
            int clicked = shapeLayout.hitPixels(event.x(), event.y(), displayedInverseScale);
            int count = shapePanel.count();
            if (wheel) { finishNumber(); shapeSelected = std::clamp(shapeSelected + (event.buttonData() > 0 ? -1 : 1), 0, count-1); }
            if (button == MouseAction::ActionLeft && down && clicked >= 0) { shapeSelected = clicked; shapeCommandRow = clicked; shapeCommand = 3; }
            if (button == MouseAction::ActionRight && down && clicked >= 0) { shapeSelected = clicked; shapeCommandRow = clicked; shapeCommand = -1; }
            return;
        }
        if (wheel) {
            // The wheel scrolls the table; selection stays on its row.
            first = SettingsTable::clampFirst(first + (event.buttonData() > 0 ? -3 : 3),
                static_cast<int>(rows.size()), displayed.visible);
            return;
        }
        if (!scaled || !down) return;
        auto hit = displayed.hit(x, y, navCount, displayedTabWidth);
        if (button == MouseAction::ActionLeft) pendingClick = Click{hit, false};
        if (button == MouseAction::ActionRight) pendingClick = Click{hit, true};
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
        if (!shapeView && event.keyCode() == 0x46 && heldCtrl()) {
            event.cancel();
            finishNumber();
            searchFocused = true;
            query.selectAll();
            return;
        }
        // Native text generation happens after HID onKeyDown. Keep editing
        // commands here, but let the focused native keyboard process the other
        // keys (including layout/IME input) while our modal scene owns gameplay.
        if (textKeyboardOwned && (searchFocused || numericEditing() || editingShapeName >= 0)) {
            auto key = event.keyCode();
            bool commandKey = key == 0x08 || key == 0x1b || key == 0x0d || key == 0x09
                || (searchFocused && key == 0x28);
            bool selectAll = key == 0x41 && heldCtrl();
            if (!commandKey && !selectAll) return;
        }
        event.cancel();
        // Editing keys act immediately so rapid typing keeps its order; the
        // remaining navigation runs with the next frame's layout.
        if (shapeView ? (editingShapeName >= 0 || editingShapeRow >= 0) : (searchFocused || editingNumber != nullptr)) {
            if (shapeView) handleShapeKey(event.keyCode()); else handleKey(event.keyCode());
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
    stopLocalization();
    if (entranceHook) { SettingsSceneEntrance::unhook(true); entranceHook = false; }
    if (exitHook) { SettingsSceneExit::unhook(true); exitHook = false; }
    if (renderHook) { SettingsSceneRender::unhook(true); renderHook = false; }
    if (textHook) { SettingsSearchText::unhook(true); textHook = false; }
    if (backgroundHook) { SettingsWorldBackground::unhook(true); backgroundHook = false; }
}
}
