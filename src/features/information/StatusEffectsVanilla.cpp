#include "features/information/StatusEffectsVanilla.h"
#include "features/information/InfoHud.h"
#include "app/Runtime.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/controls/renderers/HudMobEffectsRenderer.h"
#include <atomic>

namespace lamium::information::statusEffects {
namespace {
bool installed = false;
std::atomic<bool> covered{false};
// The row order is per world (L-247): leaving one forgets it, so an effect in
// the next world cannot inherit a row it kept in the old one.
ll::event::ListenerPtr exitListener;
}
// The renderer behind the hud screen's mob_effects_renderer control.
LL_TYPE_INSTANCE_HOOK(VanillaEffectIcons, ll::memory::HookPriority::Normal, HudMobEffectsRenderer,
    &HudMobEffectsRenderer::$render, void, MinecraftUIRenderContext& context, IClientInstance& client, UIControl& owner,
    int pass) {
    try {
        if (covered.load() && Runtime::instance().enabled()) return;
    } catch (...) {}
    origin(context, client, owner, pass);
}
void covers(bool value) { covered = value; }
void start() noexcept {
    try {
        if (!installed) installed = VanillaEffectIcons::hook(true) == 0;
        if (!installed)
            Runtime::instance().self().getLogger().warn("Status effects HUD cannot hide the game's effect icons; they stay drawn");
        auto& bus = ll::event::EventBus::getInstance();
        if (!exitListener)
            exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { forgetEffectRows(); });
    } catch (...) {}
}
void stop() {
    covered = false;
    forgetEffectRows();
    if (exitListener) {
        ll::event::EventBus::getInstance().removeListener(exitListener);
        exitListener.reset();
    }
    if (installed && VanillaEffectIcons::unhook(true)) installed = false;
}
}
