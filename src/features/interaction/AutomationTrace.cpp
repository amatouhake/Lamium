#include "features/interaction/AutomationTrace.h"
#ifdef LAMIUM_AUTOMATION_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/deps/input/InputHandler.h"
#include <stdexcept>
#include <utility>

namespace lamium::interaction::automationTrace {
namespace {
bool enabled = false;
unsigned registrations = 0, dispatches = 0;
// Observe registration names instead of guessing input hashes or retaining
// callbacks for replay. Existing callback arguments and ordering are preserved.
InputHandler::ButtonPressHandler observe(std::string name, bool down, bool suspendable,
                                        InputHandler::ButtonPressHandler handler) {
    if (!handler || registrations >= 128) return handler;
    ++registrations;
    Runtime::instance().self().getLogger().info(
        "Automation input registration: name={} down={} suspendable={}", name, down, suspendable);
    return [name = std::move(name), down, handler = std::move(handler)](FocusImpact focus, IClientInstance& client) {
        if (enabled && dispatches < 64) {
            ++dispatches;
            Runtime::instance().self().getLogger().info(
                "Automation input dispatch: name={} down={} focus={}", name, down, static_cast<int>(focus));
        }
        handler(focus, client);
    };
}
LL_TYPE_INSTANCE_HOOK(Down, ll::memory::HookPriority::Normal, InputHandler,
    &InputHandler::registerButtonDownHandler, void, std::string name,
    InputHandler::ButtonPressHandler handler, bool suspendable) {
    auto wrapped = observe(name, true, suspendable, std::move(handler));
    origin(std::move(name), std::move(wrapped), suspendable);
}
LL_TYPE_INSTANCE_HOOK(Up, ll::memory::HookPriority::Normal, InputHandler,
    &InputHandler::registerButtonUpHandler, void, std::string name,
    InputHandler::ButtonPressHandler handler, bool suspendable) {
    auto wrapped = observe(name, false, suspendable, std::move(handler));
    origin(std::move(name), std::move(wrapped), suspendable);
}
bool downInstalled = false, upInstalled = false;
}
void start() {
    try {
        if (!downInstalled) {
            if (Down::hook(true) != 0) throw std::runtime_error("Could not observe button-down registration");
            downInstalled = true;
        }
        if (!upInstalled) {
            if (Up::hook(true) != 0) throw std::runtime_error("Could not observe button-up registration");
            upInstalled = true;
        }
        enabled = true;
    } catch (...) { stop(); throw; }
}
void stop() {
    enabled = false;
    if (upInstalled && Up::unhook(true)) upInstalled = false;
    if (downInstalled && Down::unhook(true)) downInstalled = false;
}
}
#else
namespace lamium::interaction::automationTrace {
void start() {}
void stop() {}
}
#endif
