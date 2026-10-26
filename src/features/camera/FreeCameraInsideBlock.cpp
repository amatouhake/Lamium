#include "features/camera/FreeCameraInsideBlock.h"
#include "features/camera/CameraSessions.h"
#include "app/CapabilityHooks.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/minecraft_renderer/objects/ViewRenderObject.h"
#include "mc/legacy/ActorRuntimeID.h"
#include <atomic>

namespace lamium::camera::insideBlock {
namespace {
constexpr auto capability = capabilities::Id::FreeCameraInsideBlock;
bool installed = false;
std::atomic<bool> reported{false};
void reportReached() noexcept {
    if (reported.exchange(true)) return;
    try { Runtime::instance().self().getLogger().info("FreeCamera view: inside-a-block effect not drawn"); }
    catch (...) {}
}
// The game builds the local player's view here, deciding the inside-a-block
// effect from the body; FreeCamera moves the camera, not the body. The
// returned object is this frame's own copy, so the flag is restored by the
// game on the next frame and nothing else is written.
LL_TYPE_INSTANCE_HOOK(InsideBlockView, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::createViewRenderObject, ViewRenderObject, ScreenContext& context, SubClientId id) {
    auto view = origin(context, id);
    try {
        auto current = ll::service::getClientInstance();
        auto* player = current ? current->getLocalPlayer() : nullptr;
        bool primary = current && static_cast<IClientInstance*>(current.as_ptr()) == &mClientInstance;
        bool free = current && player && player->hasRuntimeID()
            && CameraSessions::instance().freeCameraFor(*current, player->getRuntimeID().rawID);
        if (!suppress(primary, free) || !versionSensitiveAllowed(capability)) return view;
        auto& data = *view.mViewData;
        bool vanilla = data.mDrawInsideCubes;
        data.mDrawInsideCubes = drawsInsideCubes(vanilla, true);
        if (vanilla) reportReached();
    } catch (...) {}
    return view;
}
}
void start() noexcept {
    capabilities::begin(capability);
    try {
        if (capabilities::registry().status(capability).compatibility != capabilities::Reason::None) {
            versionSensitiveAllowed(capability);
            return;
        }
        capabilities::attachHook<InsideBlockView>(installed, {capability});
        capabilities::hooks(capability, installed);
        if (!installed)
            Runtime::instance().self().getLogger().warn("FreeCamera keeps the inside-a-block view; the effect stays drawn");
    } catch (...) { capabilities::fail(capability, capabilities::Reason::OperationFailed, "initialization failed"); }
}
void stop() noexcept {
    capabilities::stop(capability);
    try { capabilities::detachHook<InsideBlockView>(installed, {capability}); }
    catch (...) { capabilities::fail(capability, capabilities::Reason::RemovalFailed, "view hook removal failed"); }
}
}
