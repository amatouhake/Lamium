#include "features/recipes/ItemList.h"
#include "features/research/ItemScreenProbe.h"
#include "input/GameKeymap.h"
#include "input/CustomInput.h"
#include "features/camera/CameraSessions.h"
#include "features/interaction/PermanentSneak.h"
#include "features/interaction/PeriodicInput.h"
#include "features/inventory/EnchantOrderGame.h"
#include "features/inventory/FakeOffhand.h"
#include "features/inventory/RepeatTrades.h"
#include "features/schematic/PlacementAssist.h"
#include "input/Binding.h"
#include "input/Actions.h"
#include "app/Runtime.h"
#include "ui/Toast.h"
#include "features/inventory/game/ScreenTracker.h"
#include "features/inventory/game/TextInputTracker.h"
#include "features/inventory/game/TransferSession.h"
#include "ui/SettingsScreen.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "app/ClientTasks.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/deps/input/HIDController.h"
#include "mc/deps/input/MouseAction.h"
#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace lamium::input {
namespace {
// Key and mouse events arrive on the client thread but between client ticks
// (seen 2026-11-26, L-353). Game state (inventories in particular) must not
// change there, so actions are queued in order and run in the tick.
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
    // Keys bound alike arrive together: their toggles share one toast.
    ui::beginToastGroup();
    for (auto [action, press] : work) {
        if (!press) { releaseAction(action); continue; }
        if (auto client = ll::service::getClientInstance()) executeAction(*client, action);
    }
    ui::endToastGroup();
}
void post(Action action, bool press) {
    std::scoped_lock lock(queueLock);
    queued.emplace_back(action, press);
    if (flushScheduled) return;
    flushScheduled = true;
    clientTasks::post(flush);
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
void invalidate() {
    inventory::repeatTrades::cancel();
    interaction::periodic::interrupt();
    inventory::fakeOffhand::rightChord(false);
    inventory::game::TransferSession::cancel();
    releaseStates();
    held.invalidate();
}
LL_TYPE_INSTANCE_HOOK(CustomInputFocusLost, ll::memory::HookPriority::Normal, MinecraftGame,
    &MinecraftGame::$onAppFocusLost, void) {
    invalidate();
    ui::cancelInputCapture();
    recipes::itemList::focusLost();
    inventory::enchantOrder::game::anvilFocusLost();
    schematic::assist::focusLost();
    origin();
}
void sync(IClientInstance& client) {
    auto name = client.getScreenName();
    auto const published = Runtime::instance().snapshot();
    auto const& overrides = published->bindings;
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
// `windowOpen`: a Lamium window over the item screen (the recipe view, the plan window, the bands' editor),
// which lets through only the actions declared for it (input::whereActs).
bool process(Token token, bool down, bool cancelled, bool textEditing = false, bool windowOpen = false) {
    auto current = ll::service::getClientInstance();
    if (!current) { invalidate(); return false; }
    sync(*current);
    bool const fresh = held.observe(token, down, !cancelled && !textEditing);
    if (!Runtime::instance().enabled() || textEditing || ui::ownsInput()) {
        invalidate();
        return false;
    }
    bool const gameplay = gameplayScreen(current->getScreenName());
    bool const outside = outsideWorldMenu(current->getScreenName(), current->getLocalPlayer() != nullptr);
    auto& tracker = inventory::game::ScreenTracker::getInstance();
    bool const container = tracker.current() && !inventory::game::TextInputTracker::getInstance().isEditing(tracker.currentView());
    ChordSet allowed;
    auto const published = Runtime::instance().snapshot(); // as sync() reads the bindings
    auto const& inventoryPrefs = published->inventory;
    InputState const state{currentLayers(gameplay, container, windowOpen, outside),
        CameraSessions::instance().blocksPerspective(), inventoryPrefs.recipes, inventoryPrefs.plan};
    for (size_t i = 0; i < allowed.size(); ++i)
        if (actionAllowed(static_cast<Action>(i), state)) allowed[i] = previous[i];
    // The game's Ctrl + Alt and Alt keys (coordinate copies, macros) reach the game in play (L-412).
    if ((state.layers & layer::gameplay) && down && !cancelled)
        passGameChords(allowed, dispatch, held.value(), token, gameKeyCodes(*current));
    // A different consumer (notably Zoom's wheel adjustment) may own this
    // event. Preserve held inputs, but always observe key-up releases.
    auto result = dispatch.update(allowed, held.value(), down && !cancelled ? std::optional<Token>(token) : std::nullopt, fresh);
    bool menu = false;
    for (auto [index, pressed] : result.transitions) {
        post(static_cast<Action>(index), pressed);
        menu = menu || (pressed && opensMenu(static_cast<Action>(index)));
    }
    // A mouse button bound alone keeps the game's own click (attack, use, pick block): the action runs and the
    // game gets the click too. A chord with a modifier (Alt + left click) still takes it, also while it waits
    // for its release or opens a menu, so this is asked before the reset below.
    bool const gameKeeps = gameClick(Chord{token}) != GameClick::None && !dispatch.takes(allowed, token);
    // Opening a menu takes input ownership once the queue runs. Actions that
    // share the chord already fired together; nothing else may follow.
    if (menu) invalidate();
    // A right-click activation chord is marked here, before vanilla receives
    // the click; a queued action would reach the build tick too late.
    auto const& fakeChord = allowed[static_cast<size_t>(Action::FakeOffhandUse)];
    bool const rightChord = !fakeChord.empty() && fakeChord.back() == Token{Device::Mouse, 2}
        && dispatch.isActive(Action::FakeOffhandUse);
    inventory::fakeOffhand::rightChord(rightChord);
    bool nativeUse = rightChord && token == Token{Device::Mouse, 2};
    if (nativeUse)
        for (size_t i = 0; i < actions.size(); ++i)
            if (i != static_cast<size_t>(Action::FakeOffhandUse)
                && dispatch.isActive(static_cast<Action>(i)) && contains(allowed[i], token)) nativeUse = false;
    return result.consumed && !nativeUse && !gameKeeps;
}
bool transferDropPress(int key, bool repeat, bool cancelled) {
    auto const& keys = held.value();
    auto any = [&](std::initializer_list<int> codes) {
        return std::ranges::any_of(codes, [&](int code) { return contains(keys, Token{Device::Key, code}); });
    };
    return inventory::game::TransferSession::dropKeyPress(key, repeat, any({0x10, 0xA0, 0xA1}), any({0x11, 0xA2, 0xA3}),
        any({0x12, 0xA4, 0xA5}), cancelled);
}
}
void resetCustomInput() { invalidate(); }
void queuePress(int action) { post(static_cast<Action>(action), true); }
void startCustomInput() {
    if (installed) return;
    if (CustomInputFocusLost::hook(true) != 0) throw std::runtime_error("Could not install input focus hook");
    installed = true;
    auto& bus = ll::event::EventBus::getInstance();
    listeners[0] = bus.emplaceListener<ll::event::input::KeyInputEvent>([](auto& event) {
        // L-359 probe: a focused field under a game screen keeps the keys from it.
        if (researchTrace::itemScreen::key(event.keyCode(), event.isDown())) { event.cancel(); return; }
        // Auto-repeat of a held key (Shift for Shift + right click) is not new input.
        static std::vector<int> physical;
        bool const repeat = event.isDown() && std::ranges::find(physical, event.keyCode()) != physical.end();
        if (!event.isDown()) std::erase(physical, event.keyCode());
        else if (std::ranges::find(physical, event.keyCode()) == physical.end()) {
            physical.push_back(event.keyCode());
            inventory::repeatTrades::cancel();
            // Modifiers stay out: Shift held for a transfer is not a press that ends a run.
            int const code = event.keyCode();
            if (!(code == 0x10 || code == 0x11 || code == 0x12 || (code >= 0xA0 && code <= 0xA5))) recipes::itemList::playerPressed(code);
        }
        bool const textEditing = event.controller().mTextboxIsFocused || event.controller().mTextboxIsSelected
            || recipes::itemList::typing() || inventory::enchantOrder::game::anvilTyping();
        bool consumed = process({Device::Key, event.keyCode()}, event.isDown(), event.isCancelled(), textEditing,
            recipes::itemList::viewOpen());
        if (!event.isDown() && (event.keyCode() == 0x10 || event.keyCode() == 0x11
            || event.keyCode() == 0xA0 || event.keyCode() == 0xA1
            || event.keyCode() == 0xA2 || event.keyCode() == 0xA3))
            inventory::game::TransferSession::modifierReleased();
        if (consumed && event.isDown()) event.cancel();
        // Ctrl + Shift + the game's drop key over an item: Item transfer drops its kind.
        // Not under Lamium's own screens and windows, which take the keys.
        else if (event.isDown() && !textEditing && !ui::ownsInput() && !recipes::itemList::viewOpen()
                 && transferDropPress(event.keyCode(), repeat, event.isCancelled())) event.cancel();
        // A level typed beside the anvil keeps the keys from the game.
        else if (!event.isCancelled() && inventory::enchantOrder::game::anvilKey(event.keyCode(), event.isDown())) event.cancel();
        // An open recipe view beside a game screen keeps the other keys from it (L-359).
        else if (!event.isCancelled() && recipes::itemList::key(event.keyCode(), event.isDown())) event.cancel();
    });
    listeners[1] = bus.emplaceListener<ll::event::input::MouseInputEvent>([](auto& event) {
        int button = event.actionButtonId();
        if (button == MouseAction::ActionMove || button == MouseAction::ActionMoveRelative) return;
        bool wheel = button == MouseAction::ActionWheel;
        if (wheel && event.buttonData() == 0) return;
        Token token = wheel ? Token{Device::Wheel, event.buttonData() > 0 ? 1 : -1}
            : Token{Device::Mouse, button > MouseAction::ActionWheel ? button - 1 : button};
        bool down = wheel || event.buttonData() == MouseAction::DataDown;
        if (down) inventory::repeatTrades::cancel();
        if (down && !wheel) recipes::itemList::playerPressed(0);
        // The panels beside the anvil: their presses and wheel turns stay from the game. The Change button
        // opens the planner on the client thread.
        if (!wheel && !event.isCancelled()) {
            if (int const taken = inventory::enchantOrder::game::anvilPress(button == MouseAction::ActionLeft, down)) {
                if (taken == 2 && down) clientTasks::post([] {
                    if (auto client = ll::service::getClientInstance()) ui::openEnchantOrder(*client, true);
                });
                event.cancel();
                return;
            }
        } else if (wheel && !event.isCancelled() && inventory::enchantOrder::game::anvilWheel(event.buttonData() > 0 ? -3 : 3)) {
            event.cancel();
            return;
        }
        // The item list beside a game screen and the recipe view it opens: the wheel pages them, presses
        // on them stay from the game (L-359). A button bound to an action acts first, so the recipe keys
        // work on mouse buttons over the list and the view too.
        if (wheel && !event.isCancelled() && recipes::itemList::wheel(event.buttonData() > 0 ? -1 : 1)) {
            event.cancel();
            return;
        }
        bool consumed = process(token, down, event.isCancelled(), false, recipes::itemList::viewOpen());
        if (!wheel && !event.isCancelled() && !(consumed && down)
            && recipes::itemList::press(button, button == MouseAction::ActionLeft, button == MouseAction::ActionRight, down)) {
            event.cancel();
            return;
        }
        auto const& keys = held.value();
        bool shift = contains(keys, Token{Device::Key, 0x10}) || contains(keys, Token{Device::Key, 0xA0})
            || contains(keys, Token{Device::Key, 0xA1});
        bool control = contains(keys, Token{Device::Key, 0x11}) || contains(keys, Token{Device::Key, 0xA2})
            || contains(keys, Token{Device::Key, 0xA3});
        bool alt = contains(keys, Token{Device::Key, 0x12}) || contains(keys, Token{Device::Key, 0xA4})
            || contains(keys, Token{Device::Key, 0xA5});
        bool transfer = wheel
            ? inventory::game::TransferSession::wheel(event.buttonData() > 0 ? 1 : -1, shift, control,
                                                       event.isCancelled() || consumed)
            : inventory::game::TransferSession::mouseButton(button, down, shift, control, alt,
                std::ranges::any_of(keys, [](Token key) {
                    return key.device == Device::Key && inventory::game::TransferSession::dropKey(key.code);
                }), event.isCancelled() || consumed);
        if ((consumed && down) || transfer) event.cancel();
    });
    listeners[2] = bus.emplaceListener<ll::event::BeforeUIRenderEvent>([](auto& event) { sync(event.uiRenderContext().mClient); });
    listeners[3] = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
        invalidate();
        inventory::fakeOffhand::release();
        interaction::sneak::cancel();
        interaction::sprint::cancel();
        interaction::periodic::endSession();
        screen.clear();
    });
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
