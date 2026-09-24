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
std::array<Chord, actions.size()> previous;
std::array<BindingState, actions.size()> states;
std::array<ll::event::ListenerPtr, 4> listeners;
std::string screen;
bool installed = false;
void releaseStates() {
    for (size_t i = 0; i < states.size(); ++i)
        if (states[i].reset().released) post(static_cast<Action>(i), false);
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
    std::array<Chord, actions.size()> chords;
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
    bool const wheel = token.device == Device::Wheel;
    held.observe(token, down, !cancelled && !textEditing);
    if (!Runtime::instance().enabled() || textEditing || ui::ownsInput()) {
        invalidate();
        return false;
    }
    if (cancelled) {
        // A different consumer (notably Zoom's wheel adjustment) owns this
        // event. Preserve held inputs, but always observe key-up releases.
        for (size_t i = 0; i < states.size(); ++i)
            if (!previous[i].empty() && states[i].update(previous[i], held.value()).released)
                post(static_cast<Action>(i), false);
        return false;
    }
    bool const gameplay = gameplayScreen(current->getScreenName());
    auto& tracker = inventory::game::ScreenTracker::getInstance();
    bool const container = tracker.current() && !inventory::game::TextInputTracker::getInstance().isEditing(tracker.currentView());
    bool consumed = false;
    for (size_t i = 0; i < states.size(); ++i) {
        bool allowed = i == static_cast<size_t>(Action::Sort) ? container : gameplay;
        if (!allowed || previous[i].empty()) {
            if (states[i].reset().released) post(static_cast<Action>(i), false);
            continue;
        }
        auto edge = states[i].update(previous[i], held.value(), wheel ? std::optional<Token>(token) : std::nullopt);
        if (down && states[i].isActive()
            && std::find(previous[i].begin(), previous[i].end(), token) != previous[i].end()) consumed = true;
        if (edge.released) post(static_cast<Action>(i), false);
        if (edge.pressed) {
            post(static_cast<Action>(i), true);
            consumed = true;
            // Opening a menu takes input ownership once the queue runs. Do not
            // fire another action from the same chord.
            if (i == static_cast<size_t>(Action::Settings) || i == static_cast<size_t>(Action::OpenShapes)
                || i == static_cast<size_t>(Action::OpenHotkeys)) {
                invalidate();
                break;
            }
        }
    }
    return consumed;
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
