#include "features/inventory/game/RequestTracker.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/world/inventory/network/ItemStackNetManagerClient.h"
#include "mc/world/inventory/network/ItemStackRequestData.h"
#include "mc/world/inventory/network/ItemStackRequestScope.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/ItemStackResponseSlotInfo.h"
#include "mc/network/packet/ItemStackResponseContainerInfo.h"
#include "mc/network/packet/ItemStackResponseInfo.h"
#include "mc/network/packet/ItemStackResponsePacket.h"
#include <mutex>
#include <stdexcept>

namespace lamium::inventory::game {
namespace {
std::mutex mutex;
ResponseBarrier barrier;
thread_local bool capturing = false;
bool requestInstalled = false, responseInstalled = false;
LL_TYPE_INSTANCE_HOOK(RequestEndHook, ll::memory::HookPriority::Normal, ContainerManagerController,
    &ContainerManagerController::_updateItemStackRequest, void,
    ContainerScreenActionResult const& result, ItemStackRequestScope& scope) {
    origin(result, scope);
    auto* manager = scope.mItemStackNetManagerClient;
    if (capturing && manager && manager->mRequest) {
        std::lock_guard lock(mutex);
        barrier.track(manager->mRequest->mClientRequestId->mRawId);
    }
}
LL_TYPE_INSTANCE_HOOK(ResponseHook, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void,
    NetworkIdentifier const& source, ItemStackResponsePacket const& packet) {
    // Wait until vanilla has applied authoritative corrections.
    origin(source, packet);
    std::lock_guard lock(mutex);
    for (auto const& response : packet.mResponses.get())
        barrier.respond(response.mClientRequestId->mRawId, response.mResult == ItemStackNetResult::Success);
}
}
void installRequestTracker() {
    if (!requestInstalled) {
        if (RequestEndHook::hook(true) != 0) throw std::runtime_error("Could not track inventory requests");
        requestInstalled = true;
    }
    if (!responseInstalled) {
        if (ResponseHook::hook(true) != 0) throw std::runtime_error("Could not track inventory responses");
        responseInstalled = true;
    }
}
void removeRequestTracker() {
    if (responseInstalled && ResponseHook::unhook(true)) responseInstalled = false;
    if (requestInstalled && RequestEndHook::unhook(true)) requestInstalled = false;
    if (requestInstalled || responseInstalled)
        Runtime::instance().self().getLogger().error("Could not remove inventory response hooks");
    std::lock_guard lock(mutex);
    barrier.begin(ResponseBarrier::Clock::now());
    capturing = false;
}
void beginTransfer() {
    std::lock_guard lock(mutex);
    barrier.begin(ResponseBarrier::Clock::now());
    capturing = true;
}
void endTransfer() { capturing = false; }
ResponseBarrier::Result transferResult() {
    std::lock_guard lock(mutex);
    return barrier.result(ResponseBarrier::Clock::now());
}
}
