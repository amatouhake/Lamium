#include "input/CustomInput.h"
#include "features/interaction/PermanentSneak.h"
#include "features/interaction/PeriodicInput.h"
#include "input/Binding.h"
#include "input/Actions.h"
#include "app/Runtime.h"
#include "features/inventory/game/ScreenTracker.h"
#include "features/inventory/game/TextInputTracker.h"
#include "ui/SettingsScreen.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/thread/ClientThreadExecutor.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/deps/input/HIDController.h"
#include "mc/deps/input/MouseAction.h"
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace lamium::input {
namespace {
// Key and mouse events arrive from the window procedure, outside the client
// tick. Game state (inventories in particular) must not be touched there, so
// actions are queued in order and run on the client thread.
std::mutex queueLock;
std::vector<std::pair<Action, bool>> queued;
bool flushScheduled = false;
void flush() {
    std::vector<std::pair<Action, bool>> work;
    {
        std::scoped_lock lock(queueLock);
        work.swap(queued);
        flushScheduled = false;
    }
    for (auto [action, press] : work) {
        if (!press) { releaseAction(action); continue; }
        if (auto client = ll::service::getClientInstance()) executeAction(*client, action);
    }
}
void post(Action action, bool press) {
    std::scoped_lock lock(queueLock);
    queued.emplace_back(action, press);
    if (flushScheduled) return;
    flushScheduled = true;
    ll::thread::ClientThreadExecutor::getDefault().execute(flush);
}
HeldInputs held;
ChordSet previous;
ChordDispatch dispatch;
std::array<ll::event::ListenerPtr, 4> listeners;
std::string screen;
bool installed = false;
void releaseStates() {
    for (auto transition : dispatch.reset()) post(static_cast<Action>(transition.action), false);
}
bool opensMenu(Action action) {
    return action == Action::Settings || action == Action::OpenShapes || action == Action::OpenHotkeys
        || action == Action::OpenHudLayout;
}
void invalidate() {
    interaction::periodic::cancel();
    interaction::sneak::cancel();
    releaseStates();
    held.invalidate();
}
LL_TYPE_INSTANCE_HOOK(CustomInputFocusLost, ll::memory::HookPriority::Normal, MinecraftGame,
    &MinecraftGame::$onAppFocusLost, void) {
    invalidate();
    ui::cancelInputCapture();
    origin();
}
void sync(IClientInstance& client) {
    auto name = client.getScreenName();
    auto overrides = Runtime::instance().preferences().bindings;
    ChordSet chords;
    for (size_t i = 0; i < actions.size(); ++i) chords[i] = effectiveChord(overrides, static_cast<Action>(i));
    if (screen != name || previous != chords) {
        for (size_t i = 0; i < actions.size(); ++i)
            if (previous[i] != chords[i]) post(static_cast<Action>(i), false);
        invalidate();
        screen = name;
        previous = std::move(chords);
    }
}
bool process(Token token, bool down, bool cancelled, bool textEditing = false) {
    auto current = ll::service::getClientInstance();
    if (!current) { invalidate(); return false; }
    sync(*current);
    bool const fresh = held.observe(token, down, !cancelled && !textEditing);
    if (!Runtime::instance().enabled() || textEditing || ui::ownsInput()) {
        invalidate();
        return false;
    }
    bool const gameplay = gameplayScreen(current->getScreenName());
    auto& tracker = inventory::game::ScreenTracker::getInstance();
    bool const container = tracker.current() && !inventory::game::TextInputTracker::getInstance().isEditing(tracker.currentView());
    ChordSet allowed;
    for (size_t i = 0; i < allowed.size(); ++i)
        if (i == static_cast<size_t>(Action::Sort) ? container : gameplay) allowed[i] = previous[i];
    // A different consumer (notably Zoom's wheel adjustment) may own this
    // event. Preserve held inputs, but always observe key-up releases.
    auto result = dispatch.update(allowed, held.value(), down && !cancelled ? std::optional<Token>(token) : std::nullopt, fresh);
    bool menu = false;
    for (auto [index, pressed] : result.transitions) {
        post(static_cast<Action>(index), pressed);
        menu = menu || (pressed && opensMenu(static_cast<Action>(index)));
    }
    // Opening a menu takes input ownership once the queue runs. Actions that
    // share the chord already fired together; nothing else may follow.
    if (menu) invalidate();
    return result.consumed;
}
}
void resetCustomInput() { invalidate(); }
void startCustomInput() {
    if (installed) return;
    if (CustomInputFocusLost::hook(true) != 0) throw std::runtime_error("Could not install input focus hook");
    installed = true;
    auto& bus = ll::event::EventBus::getInstance();
    listeners[0] = bus.emplaceListener<ll::event::input::KeyInputEvent>([](auto& event) {
        bool consumed = process({Device::Key, event.keyCode()}, event.isDown(), event.isCancelled(),
            event.controller().mTextboxIsFocused || event.controller().mTextboxIsSelected);
        if (consumed && event.isDown()) event.cancel();
    });
    listeners[1] = bus.emplaceListener<ll::event::input::MouseInputEvent>([](auto& event) {
        int button = event.actionButtonId();
        if (button == MouseAction::ActionMove || button == MouseAction::ActionMoveRelative) return;
        bool wheel = button == MouseAction::ActionWheel;
        if (wheel && event.buttonData() == 0) return;
        Token token = wheel ? Token{Device::Wheel, event.buttonData() > 0 ? 1 : -1}
            : Token{Device::Mouse, button > MouseAction::ActionWheel ? button - 1 : button};
        bool down = wheel || event.buttonData() == MouseAction::DataDown;
        if (process(token, down, event.isCancelled()) && down) event.cancel();
    });
    listeners[2] = bus.emplaceListener<ll::event::BeforeUIRenderEvent>([](auto& event) { sync(event.uiRenderContext().mClient); });
    listeners[3] = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { invalidate(); screen.clear(); });
    for (auto const& listener : listeners)
        if (!listener) throw std::runtime_error("Could not subscribe custom input");
}
void stopCustomInput() {
    invalidate();
    for (auto& listener : listeners) {
        if (listener) ll::event::EventBus::getInstance().removeListener(listener);
        listener.reset();
    }
    if (installed) { CustomInputFocusLost::unhook(true); installed = false; }
    held.clear(); screen.clear();
}
}
