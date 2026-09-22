#include "input/CustomInput.h"
#include "input/Binding.h"
#include "input/Actions.h"
#include "app/Runtime.h"
#include "features/camera/Zoom.h"
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
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/deps/input/HIDController.h"
#include "mc/deps/input/MouseAction.h"
#include <stdexcept>

namespace lamium::input {
namespace {
// Input, UI lifecycle, and focus callbacks execute on the client main thread.
HeldInputs held;
Bindings previous;
std::array<BindingState, actions.size()> states;
std::array<ll::event::ListenerPtr, 4> listeners;
std::string screen;
bool installed = false;
void releaseStates() {
    for (size_t i = 0; i < states.size(); ++i)
        if (states[i].reset().released && i == static_cast<size_t>(Action::Zoom)) Zoom::instance().release();
}
void invalidate() {
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
    auto bindings = Runtime::instance().preferences().bindings;
    if (screen != name || previous != bindings) {
        if (previous[static_cast<size_t>(Action::Zoom)] != bindings[static_cast<size_t>(Action::Zoom)])
            Zoom::instance().release();
        invalidate();
        screen = name;
        previous = std::move(bindings);
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
            if (previous[i] && states[i].update(*previous[i], held.value()).released
                && i == static_cast<size_t>(Action::Zoom)) Zoom::instance().release();
        return false;
    }
    bool const gameplay = gameplayScreen(current->getScreenName());
    auto& tracker = inventory::game::ScreenTracker::getInstance();
    bool const container = tracker.current() && !inventory::game::TextInputTracker::getInstance().isEditing(tracker.currentView());
    bool consumed = false;
    for (size_t i = 0; i < states.size(); ++i) {
        bool allowed = i == static_cast<size_t>(Action::Sort) ? container : gameplay;
        if (!allowed || !previous[i]) {
            if (states[i].reset().released && i == static_cast<size_t>(Action::Zoom)) Zoom::instance().release();
            continue;
        }
        auto edge = states[i].update(*previous[i], held.value(), wheel ? std::optional<Token>(token) : std::nullopt);
        if (down && states[i].isActive()
            && std::find(previous[i]->begin(), previous[i]->end(), token) != previous[i]->end()) consumed = true;
        if (edge.released && i == static_cast<size_t>(Action::Zoom)) Zoom::instance().release();
        if (edge.pressed) {
            executeAction(*current, static_cast<Action>(i));
            consumed = true;
            // Opening a menu changes ownership immediately, before the next
            // render callback. Do not fire another action from the same chord.
            if (ui::ownsInput()) { invalidate(); break; }
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
