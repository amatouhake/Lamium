#include "ui/SettingsScreen.h"
#include "settings/Options.h"
#include "ui/SettingsLayout.h"
#include "ui/SettingsRows.h"
#include "ui/SearchQuery.h"
#include "ui/NumberInput.h"
#include "ui/Widgets.h"
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
std::recursive_mutex mutex;
IClientInstance* client = nullptr;
std::shared_ptr<AbstractScene> scene;
int selected = 0;
int hovered = -1;
int command = 0;
int commandRow = 0;
int firstVisible = 0;
bool seen = false;
bool closing = false;
SearchQuery query;
bool searchFocused = false;
settings::Option const* editingNumber = nullptr;
NumberInput numberInput;
bool numberDirty = false;
bool textHook = false;
std::vector<SettingsRow> visibleRows;
std::set<std::string_view> collapsed = [] {
    std::set<std::string_view> result;
    for (auto const& feature : features) result.insert(feature.id);
    return result;
}();
bool hotkeys = false;
std::optional<input::Action> capturing;
int captureFirstVisible = 0;
input::BindingCapture capture;
input::Chord uiHeld;
struct BindingEdit { input::Action action; std::optional<input::Chord> binding; };
std::optional<BindingEdit> bindingEdit;
int rowCount() { return capturing ? 4 : static_cast<int>(visibleRows.size()) + 3; }
void filterOptions() {
    visibleRows = buildSettingsRows(hotkeys, query, collapsed, [](std::string_view key) { return translated(key); });
    selected = 0; hovered = -1; firstVisible = 0; command = 0;
}
std::string error;
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
    auto action = capturing;
    capturing.reset(); capture.clear(); bindingEdit.reset(); error.clear();
    filterOptions();
    // Return to the edited action, keeping the surrounding list in view. This
    // also covers save, Clear, Reset, Escape, and focus-loss cancellation.
    if (action) {
        for (size_t i = 0; i < visibleRows.size(); ++i) {
            if (visibleRows[i].action == action) {
                selected = static_cast<int>(i) + 2;
                firstVisible = captureFirstVisible;
                break;
            }
        }
    }
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
LL_TYPE_INSTANCE_HOOK(SettingsSearchText, ll::memory::HookPriority::Normal, UIScene,
    &UIScene::$handleTextChar, void, std::string const& text, FocusImpact impact) {
    std::lock_guard lock(mutex);
    if (scene.get() == this && ownsTop()) {
        if (editingNumber) { if (numberInput.append(text)) numberDirty = true; return; }
        if (!capturing && searchFocused && query.append(text)) { filterOptions(); selected = 1; }
        return;
    }
    origin(text, impact);
}
void clear() { editingNumber = nullptr; numberDirty = false; uiHeld.clear(); capturing.reset(); bindingEdit.reset(); capture.clear(); client = nullptr; scene.reset(); seen = false; closing = false; command = 0; hovered = -1; }
void applyNumber() {
    if (!editingNumber || !numberDirty) return;
    numberDirty = false;
    auto const& range = *editingNumber->numeric;
    auto parsed = numberInput.parsed(range.minimum, range.maximum);
    if (!parsed) { error = translated("numberRange", range.minimum, range.maximum); return; }
    auto value = Runtime::instance().preferences();
    if (std::get<float>(editingNumber->read(value)) == *parsed) { error.clear(); return; }
    range.write(value, *parsed);
    error = Runtime::instance().save(value) ? std::string{} : translated("saveError");
}
void finishNumber() { applyNumber(); editingNumber = nullptr; numberDirty = false; }
void close() {
    if (ownsTop()) {
        if (!closing) client->getSceneFactory().getCurrentSceneStack()->schedulePopScreen(1);
        closing = true;
    } else clear();
}
void activate(int row, int direction) {
    finishNumber();
    // Read current preferences for every edit so another action cannot be
    // overwritten by a stale copy captured when the screen opened.
    auto value = Runtime::instance().preferences();
    if (row == rowCount() - 1) { close(); return; }
    if (row == 0) { hotkeys = !hotkeys; searchFocused = false; filterOptions(); return; }
    if (row == 1) { searchFocused = true; return; }
    if (row < 0 || row >= rowCount() - 1) return;
    searchFocused = false;
    auto const& entry = visibleRows[row-2];
    if (entry.heading()) {
        if (query.value().find_first_not_of(' ') != std::string::npos) return;
        auto id = entry.feature->id;
        if (!collapsed.erase(id)) collapsed.insert(id);
        filterOptions();
        for (size_t index = 0; index < visibleRows.size(); ++index)
            if (visibleRows[index].heading() && visibleRows[index].feature->id == id) selected = static_cast<int>(index) + 2;
        return;
    }
    if (entry.action) {
        capturing = entry.action; capture.begin(uiHeld); error.clear();
        captureFirstVisible = firstVisible;
        selected = 0; firstVisible = 0; hovered = -1;
        return;
    }
    if (entry.option->numeric && direction == 0) {
        editingNumber = entry.option;
        numberInput.begin(std::get<float>(entry.option->read(value)));
        error.clear();
        return;
    }
    entry.option->adjust(value, direction);
    error = Runtime::instance().save(value) ? std::string{} : translated("saveError");
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
    if (bindingEdit) {
        auto value = Runtime::instance().preferences();
        value.bindings[static_cast<size_t>(bindingEdit->action)] = bindingEdit->binding;
        bool saved = Runtime::instance().save(value);
        cancelCapture();
        error = saved ? std::string{} : translated("saveError");
    }
    int action = closing ? 0 : std::exchange(command, 0);
    if (action == 1) activate(commandRow, 1);
    if (action == -1) activate(commandRow, -1);
    if (action == 3) activate(commandRow, 0);
    if (action == 2) close();
    if (!scene) return;
    information::drawHud(context,size.x,size.y,Runtime::instance().preferences().information);
    auto layout = SettingsLayout::fit(size.x, size.y, rowCount(), selected, firstVisible);
    firstVisible = layout.first;
    float width = layout.width, left = layout.left, top = layout.top;
    panel(context,0,0,size.x,size.y,.25f);
    panel(context,left-6,top-6,width+12,layout.footer+40-top);
    if (!layout.visible) {
        hovered = -1;
        label(context, 4, 4, std::max(1.0f, size.x - 8), translated("smallWindow"));
        context.flushText(0, std::nullopt);
        return;
    }
    label(context,left,top,width,capturing
        ? translated("key.Lamium." + std::string(input::actions[static_cast<size_t>(*capturing)].id)) : translated("title"));
    if (layout.subtitle) label(context,left,top+18,width,translated(visibleRows.empty() ? "noResults" : "subtitle"));
    auto const preferences = Runtime::instance().preferences();
    auto rowLabel = [&](int index) {
        if (capturing) {
            if (index == 0) return translated("capturing", bindingChordName(current, capture.value()));
            return translated(index == 1 ? "clearBinding" : index == 2 ? "resetBinding" : "cancelBinding");
        }
        if (index == rowCount() - 1) return translated("close");
        if (index == 0) return translated(hotkeys ? "hotkeysView" : "featuresView");
        if (index == 1) return translated("search", query.value() + (searchFocused ? "_" : ""));
        auto const& entry = visibleRows[index-2];
        if (entry.heading()) {
            bool expanded = !collapsed.contains(entry.feature->id) || query.value().find_first_not_of(' ') != std::string::npos;
            auto text = std::string(expanded ? "[-] " : "[+] ") + translated(entry.feature->name);
            if (auto option = settings::find(entry.feature->toggle))
                text += " | " + translated(std::get<bool>(option->read(preferences)) ? "on" : "off");
            for (size_t actionIndex = 0; actionIndex < input::actions.size(); ++actionIndex)
                if (input::actions[actionIndex].feature == entry.feature->id)
                    text += " | " + actionBindingName(current, static_cast<input::Action>(actionIndex));
            return text;
        }
        if (entry.action) {
            auto actionIndex = static_cast<size_t>(*entry.action);
            auto text = translated("bindingRow",
                translated("key.Lamium." + std::string(input::actions[actionIndex].id)),
                actionBindingName(current, *entry.action));
            auto const& binding = preferences.bindings[actionIndex];
            if (binding && !binding->empty()) {
                for (size_t other = 0; other < preferences.bindings.size(); ++other)
                    if (other != actionIndex && preferences.bindings[other] == binding)
                        return translated("sharedBinding", text);
            }
            return text;
        }
        auto const& option = *entry.option;
        auto value = option.read(preferences);
        if (editingNumber == &option)
            return translated("numberInput", translated(option.label, std::get<float>(value)),
                numberInput.selectedAll() ? "[" + numberInput.value() + "]" : numberInput.value() + "_");
        if (auto flag = std::get_if<bool>(&value))
            return translated(option.label, translated(*flag ? "on" : "off"));
        if (auto choice = std::get_if<settings::ChoiceValue>(&value))
            return translated(option.label, translated(choice->label));
        return translated(option.label, std::get<float>(value));
    };
    glm::vec2 pointer = view.mPointerLocationPrevious;
    hovered = layout.hit(pointer.x, pointer.y);
    for (int i=layout.first;i<layout.first+layout.visible;++i) {
        float y = layout.rowY(i);
        rowBackground(context,left,y,width,20,selected == i,hovered == i);
        label(context,left+6,y+5,width-12,rowLabel(i));
    }
    label(context,left,layout.footer,width,error.empty() ? translated(editingNumber ? "numberHint" : capturing ? "captureHint" : "navigation") : error);
    if (layout.secondHint) {
        auto description = translated("adjustment");
        if (!capturing && selected >= 2 && selected < rowCount()-1)
            description = translated(visibleRows[selected-2].feature->description);
        label(context,left,layout.footer+15,width,description);
    }
    context.flushText(0,std::nullopt);
}
}
void open(IClientInstance& current) {
    std::lock_guard lock(mutex);
    if (scene || !gameplayScreen(current.getScreenName())) return;
    Zoom::instance().reset();
    selected = 0; hovered = -1; command = 0; error.clear(); seen = false; closing = false;
    firstVisible = 0; commandRow = 0;
    editingNumber = nullptr; numberDirty = false;
    query.clear(); uiHeld.clear(); searchFocused = false; hotkeys = false; capturing.reset(); bindingEdit.reset(); filterOptions();
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
        observeHeld(token, down);
        if (capturing) {
            if (down) event.cancel();
            if (button == MouseAction::ActionLeft && down && hovered >= 1 && hovered <= 3) {
                if (hovered == 3) cancelCapture();
                else bindingEdit = BindingEdit{*capturing, hovered == 1 ? std::optional<input::Chord>(input::Chord{}) : std::nullopt};
            } else captureInput(token, down);
            return;
        }
        // A button may already be down when F8 opens the panel. Let vanilla
        // observe its release, just as we do for keys, so it cannot stay held.
        if (event.actionButtonId() != MouseAction::ActionWheel && event.buttonData() == MouseAction::DataUp) return;
        event.cancel();
        if (event.actionButtonId() == MouseAction::ActionWheel && event.buttonData() != 0) {
            finishNumber();
            searchFocused = false;
            selected = std::clamp(selected + (event.buttonData() > 0 ? -1 : 1), 0, rowCount()-1);
        }
        if (event.actionButtonId() == MouseAction::ActionLeft && event.buttonData() == MouseAction::DataDown && hovered >= 0) {
            selected = hovered; commandRow = hovered; command = 3;
        }
        if (event.actionButtonId() == MouseAction::ActionRight && event.buttonData() == MouseAction::DataDown && hovered >= 0 && hovered < rowCount()-1) {
            selected = hovered; commandRow = hovered; command = -1;
        }
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
        event.cancel();
        if (editingNumber) {
            switch (event.keyCode()) {
            case 0x08: if (numberInput.backspace()) numberDirty = true; break;
            case 0x41:
                if (std::find(uiHeld.begin(), uiHeld.end(), input::Token{input::Device::Key, 0x11}) != uiHeld.end()
                    || std::find(uiHeld.begin(), uiHeld.end(), input::Token{input::Device::Key, 0xa2}) != uiHeld.end()
                    || std::find(uiHeld.begin(), uiHeld.end(), input::Token{input::Device::Key, 0xa3}) != uiHeld.end()) numberInput.selectAll();
                break;
            case 0x1b: case 0x0d: finishNumber(); break;
            case 0x09: finishNumber(); selected = (selected+1)%rowCount(); break;
            }
            return;
        }
        if (searchFocused) {
            switch (event.keyCode()) {
            case 0x08: if (query.backspace()) { filterOptions(); selected = 1; } break;
            case 0x1b: searchFocused = false; break;
            case 0x0d: case 0x09: case 0x28:
                searchFocused = false; selected = 2; break;
            }
            return;
        }
        switch (event.keyCode()) {
        case 0x1b: command = 2; break;
        case 0x26: selected = (selected+rowCount()-1)%rowCount(); break;
        case 0x09: case 0x28: selected = (selected+1)%rowCount(); break;
        case 0x25: if (selected < rowCount()-1) { commandRow = selected; command = -1; } break;
        case 0x27: if (selected < rowCount()-1) { commandRow = selected; command = 1; } break;
        case 0x0d: case 0x20: commandRow = selected; command = 3; break;
        }
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

