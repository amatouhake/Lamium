#include "features/inventory/game/RequestTracker.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/world/inventory/network/ItemStackNetManagerClient.h"
#include "mc/world/inventory/network/ItemStackRequestData.h"
#include "mc/world/inventory/network/ItemStackRequestScope.h"
#include "mc/world/inventory/network/ItemStackRequestBatch.h"
#include "mc/world/containers/managers/models/ContainerManagerModel.h"
#include "mc/world/actor/player/Player.h"
#include <set>
#include <utility>
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
std::set<int64_t> previousRequests;
ItemStackNetManagerClient* transferManager = nullptr;
bool responseInstalled = false;
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
    if (!responseInstalled) {
        if (ResponseHook::hook(true) != 0) throw std::runtime_error("Could not track inventory responses");
        responseInstalled = true;
    }
}
void removeRequestTracker() {
    if (responseInstalled && ResponseHook::unhook(true)) responseInstalled = false;

    if (responseInstalled)
        Runtime::instance().self().getLogger().error("Could not remove inventory response hooks");
    std::lock_guard lock(mutex);
    barrier.begin(ResponseBarrier::Clock::now());
    transferManager = nullptr;
}
bool beginTransfer(ContainerManagerController& controller) {
    std::lock_guard lock(mutex);
    transferManager = nullptr;
    barrier.begin(ResponseBarrier::Clock::now());
    previousRequests.clear();
    auto model = controller.mContainerManagerModel.lock();
    if (!model) return false;
    auto* base = model->mPlayer.mItemStackNetManager.get();
    if (!base || !base->mIsClientSide || !base->mIsEnabled) return false;
    auto* manager = static_cast<ItemStackNetManagerClient*>(base);
    // Do not append Lamium operations to somebody else's active request.
    if (manager->mRequest) return false;
    if (manager->mRequestBatch) {
        for (auto const& request : manager->mRequestBatch->mRequests.get())
            if (request) previousRequests.insert(request->mClientRequestId->mRawId);
    }
    transferManager = manager;
    return true;
}
void endTransfer() {
    std::lock_guard lock(mutex);
    // Vanilla adds completed scopes to this batch. We only observe new IDs;
    // packet creation, sending, and retries remain entirely vanilla-owned.
    auto* manager = std::exchange(transferManager, nullptr);
    if (!manager || !manager->mRequestBatch) return;
    for (auto const& request : manager->mRequestBatch->mRequests.get()) {
        if (request && !previousRequests.contains(request->mClientRequestId->mRawId))
            barrier.track(request->mClientRequestId->mRawId);
    }
}
ResponseBarrier::Result transferResult() {
    std::lock_guard lock(mutex);
    return barrier.result(ResponseBarrier::Clock::now());
}
}
