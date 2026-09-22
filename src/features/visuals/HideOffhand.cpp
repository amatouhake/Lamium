#include "features/visuals/HideOffhand.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/game/ItemInHandRenderer.h"
#include <stdexcept>

namespace lamium::visuals {
namespace {
bool installed = false;
LL_TYPE_INSTANCE_HOOK(OffhandVisibility, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderOffhandItem, void, BaseActorRenderContext& context,
    Player& player, ItemContextFlags flags) {
    auto& runtime = Runtime::instance();
    bool firstPerson = (static_cast<unsigned>(flags) & static_cast<unsigned>(ItemContextFlags::FirstPersonPass)) != 0;
    bool otherPass = (static_cast<unsigned>(flags) & (static_cast<unsigned>(ItemContextFlags::WorldPass)
        | static_cast<unsigned>(ItemContextFlags::UIPass))) != 0;
    if (firstPerson && !otherPass && runtime.enabled() && runtime.preferences().visuals.hideOffhand) return;
    // Never change equipped stacks, item use, or renderer-owned cached items.
    origin(context, player, flags);
}
}
void start() {
    if (installed) return;
    installed = OffhandVisibility::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install offhand visibility hook");
}
void stop() {
    if (installed && OffhandVisibility::unhook(true)) installed = false;
}
}
