#include "ui/SettingsTableView.h"
#include "ui/ScreenParts.h"
#include "ui/ListViewWidgets.h"
#include "settings/Options.h"
#include "ui/SettingsRows.h"
#include "ui/SettingsNavigation.h"
#include "ui/SettingsTable.h"
#include "ui/ShapeEditor.h"
#include "ui/ShapesLayout.h"
#include "ui/ShapesView.h"
#include "ui/WaypointsView.h"
#include "ui/SchematicsView.h"
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


namespace lamium::ui::table_view {
namespace {
using Zone = SettingsTable::Zone;
using Column = SettingsTable::Column;
void copyVersion_();
bool shapesView() { return screen::currentNav() == nav::shapes; }
bool waypointsView() { return screen::currentNav() == nav::waypoints; }
bool schematicsView() { return screen::currentNav() == nav::schematics; }
settings::Option const* sliderDrag = nullptr; // Slider being dragged with the left button.
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
float displayedTabWidth = 0;
SearchQuery query;
bool searchFocusedValue = false;
bool& searchFocused_() { return searchFocusedValue; }
settings::Option const* editingNumber_ = nullptr;
// L-46: General resets every setting, Hotkeys resets key bindings. The
// first press arms the button; the second applies. Shapes are per-world
// data and are never touched.
enum class ResetScope { None, All, Section, Keys };
bool resetArmed = false;
bool mapCacheArmed = false;
// The delete button as last drawn: only a click on it deletes.
float mapCacheButtonX = 0, mapCacheButtonWidth = 0;
bool onMapCacheButton(float x) { return mapCacheButtonWidth > 0 && x >= mapCacheButtonX && x < mapCacheButtonX + mapCacheButtonWidth; }
std::optional<input::Action> capturing_;
input::BindingCapture capture;
struct BindingEdit { input::Action action; std::optional<input::Chord> binding; };
std::optional<BindingEdit> bindingEdit;
std::string_view categoryKey() {
    return screen::currentNav() > 0 && screen::currentNav() < nav::hotkeys
        ? sections[screen::currentNav() - 1] : std::string_view{};
}
bool hotkeysView() { return screen::currentNav() == nav::hotkeys; }
bool valid(int row) { return row >= 0 && row < static_cast<int>(rows.size()); }
int nextSelectable(int from, int step) {
    for (int row = from; valid(row); row += step) if (rows[row].selectable()) return row;
    return -1;
}
void rebuild_(bool keepSelection) {
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
void captureInput_(input::Token token, bool down) {
    if (!capturing_ || bindingEdit) return;
    try {
        auto value = capture.observe(token, down, input::actions[static_cast<size_t>(*capturing_)].behavior);
        if (value) bindingEdit = BindingEdit{*capturing_, std::move(value)};
    } catch (std::exception const&) { screen::setMessage(translated("invalidBinding")); }
}
void cancelCapture_() {
    // The table stays where it was: selection and scroll are not rebuilt.
    capturing_.reset(); capture.clear(); bindingEdit.reset(); screen::clearMessage();
}
void startCapture(input::Action action) {
    capturing_ = action; capture.begin(screen::heldKeys()); screen::clearMessage();
}
void queryChanged() { searchCollapsed.clear(); first = 0; rebuild_(false); }
// ---- Settings table actions ----
// Read current preferences for every edit so another action cannot be
// overwritten by a stale copy captured when the screen opened.
void adjustOption_(settings::Option const& option, int direction) {
    auto value = Runtime::instance().preferences();
    option.adjust(value, direction);
    screen::setMessage(Runtime::instance().save(value) ? std::string{} : translated("saveError"));
    // The stepper of a row being typed into shows the typed text; replace it
    // with the stepped value so -/+ are visible at once.
    if (editingNumber_ == &option) {
        screen::number().begin(std::get<float>(option.read(Runtime::instance().preferences())));
        screen::numberReset();
    }
}
bool hasSwitch(FeatureInfo const& feature) {
    return !feature.toggle.empty() || isSessionFeature(feature.id);
}
void toggleFeature(FeatureInfo const& feature) {
    if (auto option = settings::find(feature.toggle)) adjustOption_(*option, 1);
    else if (screen::client() && isSessionFeature(feature.id)) toggleSession(*screen::client(), feature.id);
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
    rebuild_(true);
    if (open && displayed.visible > 0) {
        int last = row;
        while (last + 1 < static_cast<int>(rows.size()) && rows[last + 1].child()
            && rows[last + 1].feature == rows[row].feature) ++last;
        if (last >= first + displayed.visible) first = std::min(row, last - displayed.visible + 1);
    }
    first = SettingsTable::clampFirst(first, static_cast<int>(rows.size()), displayed.visible);
}
void beginNumber(settings::Option const& option) {
    editingNumber_ = &option;
    screen::number().begin(std::get<float>(option.read(Runtime::instance().preferences())));
    screen::clearMessage();
}
// Enter / Space / click on the name of a row.
void openLayout(std::optional<HudElementId> element) {
    screen::selectNav(nav::hud);
    hud_editor::select(element);
}
void setSlider(settings::Option const& option, float fraction) {
    auto const& range = *option.numeric;
    float value = SettingsTable::sliderValue(fraction, range.minimum, range.maximum, range.step);
    auto preferences = Runtime::instance().preferences();
    if (option.read(preferences) == settings::OptionValue{value}) return;
    range.write(preferences, value);
    preferences.normalize();
    screen::setMessage(Runtime::instance().save(preferences) ? std::string{} : translated("saveError"));
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
        else adjustOption_(*entry.option, 1);
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
    if (screen::currentNav() == 0) return ResetScope::All;
    return categoryKey().empty() ? ResetScope::None : ResetScope::Section;
}
void pressReset(ResetScope scope) {
    if (!resetArmed) { resetArmed = true; return; }
    resetArmed = false;
    auto value = Runtime::instance().preferences();
    if (scope == ResetScope::Keys) value.bindings = {};
    else if (scope == ResetScope::Section) resetSection(value, categoryKey());
    else value = Settings{};
    screen::setMessage(Runtime::instance().save(value) ? std::string{} : translated("saveError"));
    rebuild_(false);
}
void handleClick(SettingsTable::Hit const& hit, bool right) {
    if (!(hit.zone == Zone::Row && valid(hit.index) && rows[hit.index].kind == RowKind::MapCache && !right
          && onMapCacheButton(hit.x)))
        mapCacheArmed = false;
    auto scope = capturing_ ? ResetScope::None : resetScope();
    bool head = scope != ResetScope::None && displayed.headAction(hit.x, hit.y, hotkeysView());
    if (!head || right) resetArmed = false;
    if (head && !right) { screen::finishEditing(); pressReset(scope); return; }
    if (hit.zone != Zone::Row || !valid(hit.index) || rows[hit.index].kind != RowKind::Option
        || editingNumber_ != rows[hit.index].option) screen::finishEditing();
    if (capturing_) {
        if (hit.zone == Zone::Footer && !right) {
            int button = displayed.footerButton(hit.x, hit.y);
            if (!input::canClear(*capturing_)) {
                if (button == 0) bindingEdit = BindingEdit{*capturing_, std::nullopt};
                else if (button == 1) cancelCapture_();
            } else if (button == 2) cancelCapture_();
            else if (button >= 0) bindingEdit = BindingEdit{*capturing_, button == 0
                ? std::optional<input::Chord>(input::Chord{}) : std::nullopt};
        }
        return;
    }
    switch (hit.zone) {
    case Zone::Search: searchFocused_() = true; return;
    case Zone::Close: screen::close(); return;
    case Zone::Version: copyVersion_(); return;
    case Zone::Nav: searchFocused_() = false; screen::selectNav(hit.index); return;
    case Zone::Row: break;
    default: return;
    }
    searchFocused_() = false;
    keyboardTip = false;
    if (!valid(hit.index) || !rows[hit.index].selectable()) return;
    selected = hit.index;
    auto const& entry = rows[hit.index];
    if (right) {
        if (entry.option) adjustOption_(*entry.option, -1);
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
            if (hit.column != Column::Name) adjustOption_(*entry.option, 1);
            return;
        }
        // While typing, the row shows the stepper, so clicks go to its buttons.
        if (entry.option->numeric && entry.option->numeric->step > 0 && editingNumber_ != entry.option) {
            float fraction = displayed.sliderFraction(hit.x);
            if (fraction < 0) { beginNumber(*entry.option); return; }
            if (hit.x < displayed.sliderX()) return;
            sliderDrag = entry.option;
            setSlider(*entry.option, fraction);
            return;
        }
        int part = displayed.stepperPart(hit.x, linked.has_value());
        if (part == -1 || part == 1) adjustOption_(*entry.option, part);
        else if (part == 0) {
            if (entry.option->numeric) beginNumber(*entry.option);
            else adjustOption_(*entry.option, 1);
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
void handleKey(int key) {
    if (searchFocused_()) {
        switch (key) {
        case 0x41: if (screen::heldCtrl()) query.selectAll(); break;
        case 0x08: if (query.backspace()) queryChanged(); break;
        case 0x1b: searchFocused_() = false; break;
        case 0x0d: case 0x09: case 0x28:
            searchFocused_() = false; selected = nextSelectable(0, 1); first = 0; break;
        }
        return;
    }
    if (editingNumber_) {
        switch (key) {
        case 0x08: if (screen::number().backspace()) screen::numberTyped(); break;
        case 0x41: if (screen::heldCtrl()) screen::number().selectAll(); break;
        case 0x1b: case 0x0d: case 0x09: screen::finishEditing(); break;
        }
        return;
    }
    auto* entry = valid(selected) ? &rows[selected] : nullptr;
    int page = std::max(1, displayed.visible - 1);
    switch (key) {
    case 0x1b: screen::close(); break;
    case 0x26: moveSelection(-1); keyboardTip = true; break;
    case 0x28: moveSelection(1); keyboardTip = true; break;
    case 0x21: moveSelection(-page); keyboardTip = true; break;
    case 0x22: moveSelection(page); keyboardTip = true; break;
    case 0x24: selected = -1; moveSelection(1); keyboardTip = true; break; // Home
    case 0x23: selected = static_cast<int>(rows.size()); moveSelection(-1); keyboardTip = true; break; // End
    case 0x09: screen::selectNav((screen::currentNav() + (screen::heldShift() ? nav::worldMap - 1 : 1)) % nav::worldMap); break;
    case 0x25: case 0x27: {
        int direction = key == 0x27 ? 1 : -1;
        if (!entry) break;
        if (entry->heading()) setExpanded(selected, direction > 0);
        else if (entry->option) adjustOption_(*entry->option, direction);
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
    if (capturing_ == action) {
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
        ? (screen::number().selectedAll() ? "[" + screen::number().value() + "]" : screen::number().value() + "_") : value;
    label(context,x+aw+1,cy+1+boxTextInset(),w-2*aw-2,std::move(text),warn ? palette::warning : palette::text,Align::Center);
}
void drawGuide(MinecraftUIRenderContext& context, float y, bool last) {
    float x = displayed.nameX + 3;
    fill(context,x,y,1,last ? SettingsTable::rowHeight / 2 : SettingsTable::rowHeight,palette::white,.18f);
    fill(context,x+1,y+SettingsTable::rowHeight/2,5,1,palette::white,.18f);
}
std::string navLabel(int index, bool compact) {
    if (index == 0) return translated("nav.all");
    if (index == nav::hotkeys) return translated("nav.hotkeys");
    if (index == nav::shapes) return translated("nav.shapes");
    if (index == nav::waypoints) return translated("nav.waypoints");
    if (index == nav::schematics) return translated("nav.schematics");
    if (index == nav::worldMap) return translated("nav.worldMap");
    if (index == nav::hud) return translated("nav.hudLayout");
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
    if (capturing_) return actionLabel(*capturing_) + " - " + behaviorText(*capturing_);
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
            return translated(editingNumber_ ? "numberRange" : "numberControl", range.minimum, range.maximum);
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
void copyVersion_() {
    showMessageToast(translated(copyText(runningVersionLine()) ? "version.copied" : "version.copyFailed"));
}
void renderTable(MinecraftUIRenderContext& context, IClientInstance& current, glm::vec2 size, glm::vec2 pointer) {
    auto t = SettingsTable::fit(size.x, size.y, static_cast<int>(rows.size()), first, nav::count);
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
    auto hover = t.hit(pointer.x, pointer.y, nav::count, displayedTabWidth);
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
    frame(context,t.searchX,t.top+4,t.searchWidth,12,searchFocused_() ? palette::accent : palette::keyEdge);
    if (searchFocused_() && query.selectedAll() && !query.value().empty())
        fill(context,t.searchX+3,t.top+5,std::min(t.searchWidth-6,textWidth(context,query.value())),10,palette::accent,.35f);
    if (query.value().empty() && !searchFocused_())
        label(context,t.searchX+4,t.top+5+boxTextInset(),t.searchWidth-8,translated("searchPlaceholder") + "  Ctrl+F",palette::faint);
    else label(context,t.searchX+4,t.top+5+boxTextInset(),t.searchWidth-8,query.value() + (searchFocused_() ? "_" : ""));
    }
    bool closeHover = hover.zone == Zone::Close;
    if (closeHover) fill(context,t.closeX,t.top+4,SettingsTable::closeWidth,12,palette::white,.07f);
    frame(context,t.closeX,t.top+4,SettingsTable::closeWidth,12,palette::keyEdge);
    label(context,t.closeX,t.top+5+boxTextInset(),SettingsTable::closeWidth,
        translated(waypointsView() && waypoints_view::fromMap() ? "worldMap.back" : "closeButton"),
        closeHover ? palette::text : palette::dim,Align::Center);
    fill(context,t.left,t.top+SettingsTable::headerHeight-1,t.width,1,palette::white,.14f);

    // Categories: sidebar, or tabs when narrow.
    // A query searches every category, so the navigation shows "All" meanwhile.
    bool searching = query.value().find_first_not_of(' ') != std::string::npos;
    int activeNav = searching && !hotkeysView() ? 0 : screen::currentNav();
    if (t.compact) {
        displayedTabWidth = (t.width - 4) / nav::count;
        for (int i = 0; i < nav::count; ++i) {
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
        for (int i = 0; i < nav::count; ++i) {
            bool pinned = i >= nav::count - SettingsTable::pinnedItems;
            float y = pinned ? t.pinnedItemY(i - (nav::count - SettingsTable::pinnedItems)) : t.navItemY(i);
            float x = t.left + 1, w = SettingsTable::sidebarWidth - 2;
            bool active = i == activeNav, over = hover.zone == Zone::Nav && hover.index == i;
            if (i == nav::count - SettingsTable::pinnedItems) fill(context,x+6,y-4,w-12,1,palette::white,.14f);
            if (active) { fill(context,x,y,w,t.navStep,palette::accent,.16f); fill(context,x,y,2,t.navStep,palette::accent); }
            else if (over) fill(context,x,y,w,t.navStep,palette::white,.07f);
            std::string count = i > 0 && i < nav::hotkeys ? sectionCount(sections[i-1], preferences)
                : i == nav::shapes ? std::to_string(overlay::shapes::list().size())
                : i == nav::waypoints ? std::to_string(map::waypoints::current().waypoints.size())
                : i == nav::schematics ? std::to_string(schematic::session::current().placements.size()) : std::string{};
            float countWidth = count.empty() ? 0 : textWidth(context, count) + 4;
            float ty = y + (t.navStep - 8) / 2;
            label(context,x+7,ty,w-12-countWidth,navLabel(i,false),active || over ? palette::text : palette::dim);
            if (!count.empty()) label(context,x+w-5-countWidth,ty,countWidth,count,palette::faint,Align::Right);
        }
    }

    if (shapesView() || waypointsView() || schematicsView()) {
        if (shapesView()) shapes_view::renderContent(context, size, pointer, t);
        else if (waypointsView()) waypoints_view::renderContent(context, size, pointer, t);
        else schematics_view::renderContent(context, size, pointer, t);
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
    if (auto scope = capturing_ ? ResetScope::None : resetScope(); scope != ResetScope::None) {
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
                bool asSlider = entry.option->numeric && entry.option->numeric->step > 0 && editingNumber_ != entry.option;
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
                    drawStepper(context,y,*entry.option,optionValueText(*entry.option,value),editingNumber_ == entry.option,
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
    if (capturing_) {
        std::vector<std::string> names{translated("resetShort"), translated("cancelShort")};
        if (input::canClear(*capturing_)) names.insert(names.begin(), translated("clearShort"));
        for (size_t i = 0; i < names.size(); ++i) {
            float x = t.footerButtonX(static_cast<int>(i)), y = t.footerButtonY();
            bool over = hover.zone == Zone::Footer && t.footerButton(hover.x, hover.y) == static_cast<int>(i);
            fill(context,x,y,SettingsTable::footerButtonWidth,SettingsTable::footerButtonHeight,over ? Rgb{.23f,.23f,.24f} : palette::keyFill);
            frame(context,x,y,SettingsTable::footerButtonWidth,SettingsTable::footerButtonHeight,palette::keyEdge);
            label(context,x,y+boxTextInset(),SettingsTable::footerButtonWidth,names[i],palette::text,Align::Center);
        }
        float after = t.footerButtonX(3);
        label(context,after,t.footerButtonY()+1,t.left+t.width-SettingsTable::pad-after,
            screen::message().empty() ? description() : screen::message(),screen::message().empty() ? palette::dim : palette::warning);
        if (!t.shortFooter) label(context,textLeft,t.footerTop+30,textWidthAvailable,translated("captureInline"),palette::faint);
    } else if (t.shortFooter) {
        label(context,textLeft,t.footerTop+3,textWidthAvailable,screen::message().empty() ? description() : screen::message(),
            screen::message().empty() ? palette::text : palette::warning);
    } else {
        bool warns = valid(selected) && rows[selected].option
            && optionWarning(rows[selected].option->id,preferences).has_value();
        paragraph(context,textLeft,t.footerTop+3,textWidthAvailable,description(),2,warns ? palette::warning : palette::text);
        std::string hint = !screen::message().empty() ? screen::message() : translated(searchFocused_() ? "searchHint" : editingNumber_ ? "numberHint" : "tableHint");
        label(context,textLeft,t.footerTop+30,textWidthAvailable,std::move(hint),screen::message().empty() ? palette::faint : palette::warning);
    }
    if (!capturing_)
        if (auto target = tipTarget(t, hover)) drawConflictTip(context,current,t,target->first,target->second);
    drawVersionTip(context, t, hover);
    context.flushText(0,std::nullopt);
}

}

void open() {
    endEditing();
    query.clear(); searchCollapsed.clear(); searchFocused_() = false; capturing_.reset(); bindingEdit.reset();
}
void reset() {
    sliderDrag = nullptr;
    endEditing();
    capturing_.reset(); bindingEdit.reset(); capture.clear();
    mapCacheArmed = false;
}
void endSearch() {
    query.clear();
    searchCollapsed.clear();
    resetArmed = false;
}
void enterCategory() {
    first = 0;
    rebuild(false);
}
void rebuild(bool keepSelection) { rebuild_(keepSelection); }
bool searchFocused() { return searchFocused_(); }
void focusSearch() { searchFocused_() = true; query.selectAll(); }
void unfocusSearch() { searchFocused_() = false; }
void typeSearch(std::string const& text) {
    if (!capturing_ && searchFocused_() && query.type(text)) queryChanged();
}
bool editingNumber() { return editingNumber_ != nullptr; }
void applyNumber() {
    if (!editingNumber_) return;
    auto const& range = *editingNumber_->numeric;
    auto parsed = screen::number().parsed(range.minimum, range.maximum);
    if (!parsed) { screen::warnRange(translated("numberRange", range.minimum, range.maximum)); return; }
    auto value = Runtime::instance().preferences();
    if (std::get<float>(editingNumber_->read(value)) == *parsed) { screen::clearMessage(); return; }
    range.write(value, *parsed);
    screen::setMessage(Runtime::instance().save(value) ? std::string{} : translated("saveError"));
}
void endEditing() { editingNumber_ = nullptr; }
void adjustOption(std::string_view id, int direction) {
    if (auto option = settings::find(id)) adjustOption_(*option, direction);
}
bool capturing() { return capturing_.has_value(); }
void captureInput(input::Token token, bool down) { captureInput_(token, down); }
void cancelCapture() { cancelCapture_(); }
void applyBinding() {
    if (!bindingEdit) return;
    auto value = Runtime::instance().preferences();
    value.bindings[static_cast<size_t>(bindingEdit->action)] = bindingEdit->binding;
    bool saved = Runtime::instance().save(value);
    cancelCapture_();
    screen::setMessage(saved ? std::string{} : translated("saveError"));
}
bool onCaptureButton(float x, float y) {
    auto hit = displayed.hit(x, y, nav::count, displayedTabWidth);
    return hit.zone == Zone::Footer && displayed.footerButton(hit.x, hit.y) >= 0;
}
void click(float x, float y, bool right) { handleClick(displayed.hit(x, y, nav::count, displayedTabWidth), right); }
void key(int key) { handleKey(key); }
void wheel(int step) { first = SettingsTable::clampFirst(first + step, static_cast<int>(rows.size()), displayed.visible); }
void release() { sliderDrag = nullptr; }
void disarm() { resetArmed = false; }
glm::vec2 caret() {
    float caretY = editingNumber_ && valid(selected) ? displayed.rowY(selected) : displayed.top + 4;
    return {editingNumber_ ? displayed.stepperX() : displayed.searchX, caretY};
}
void render(MinecraftUIRenderContext& context, IClientInstance& client, glm::vec2 size, glm::vec2 pointer) {
    renderTable(context, client, size, pointer);
}
SettingsTable const& layout() { return displayed; }
float tabWidth() { return displayedTabWidth; }
void copyVersion() { copyVersion_(); }
int selectedRow() { return selected; }
void expand(std::string_view featureId) {
    for (auto const& feature : features) if (feature.id == featureId) expanded.insert(feature.id);
}
void selectFeature(std::string_view featureId) {
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].heading() && rows[i].feature->id == featureId) { selected = static_cast<int>(i); break; }
    first = SettingsTable::reveal(first, selected, displayed.visible);
}
}
