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
#ifdef LAMIUM_RESTOCK_TRACE
#include <atomic>
#endif

namespace lamium::inventory::game {
namespace {
std::mutex mutex;
OwnedResponseBarrier barrier;
std::set<int64_t> previousRequests;
ItemStackNetManagerClient* transferManager = nullptr;
bool responseInstalled = false;
void traceRequests(char const* stage, std::size_t count, bool active) noexcept {
#ifdef LAMIUM_RESTOCK_TRACE
    try {
        if (!Runtime::instance().preferences().inventory.handRestock) return;
        static std::atomic<unsigned> samples{};
        if (samples.fetch_add(1) >= 64) return;
        Runtime::instance().self().getLogger().info(
            "Restock request trace: {} count={} active={}",stage,count,active);
    } catch (...) {}
#else
    (void)stage; (void)count; (void)active;
#endif
}
LL_TYPE_INSTANCE_HOOK(ResponseHook, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    &ClientNetworkHandler::$handle, void,
    NetworkIdentifier const& source, ItemStackResponsePacket const& packet) {
    // Wait until vanilla has applied authoritative corrections.
    origin(source, packet);
    std::lock_guard lock(mutex);
    // Observe even after an untracked use released ownership. Counts alone do
    // not associate a response with that use, and must not advance its state.
    traceRequests("responses-applied",packet.mResponses->size(),barrier.busy());
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
    transferManager = nullptr;
    previousRequests.clear();
    barrier.reset();
}
void cancelTransfer(TransferToken token) {
    std::lock_guard lock(mutex);
    if (!barrier.release(token)) return;
    transferManager = nullptr;
    previousRequests.clear();
}
std::optional<TransferToken> beginTransfer(ContainerManagerController& controller) {
    std::lock_guard lock(mutex);
    if (barrier.busy()) return {};
    auto model = controller.mContainerManagerModel.lock();
    if (!model) return {};
    auto* base = model->mPlayer.mItemStackNetManager.get();
    if (!base || !base->mIsClientSide || !base->mIsEnabled) return {};
    auto* manager = static_cast<ItemStackNetManagerClient*>(base);
    // Do not append Lamium operations to somebody else's active request.
    if (manager->mRequest) return {};
    std::set<int64_t> previous;
    if (manager->mRequestBatch) {
        for (auto const& request : manager->mRequestBatch->mRequests.get())
            if (request) previous.insert(request->mClientRequestId->mRawId);
    }
    auto token = barrier.begin(ResponseBarrier::Clock::now());
    if (!token) return {};
    previousRequests = std::move(previous);
    transferManager = manager;
    traceRequests("capture-start",previousRequests.size(),bool(manager->mRequest));
    return token;
}
void endTransfer(TransferToken token) {
    std::lock_guard lock(mutex);
    if (!barrier.owns(token)) return;
    // Vanilla adds completed scopes to this batch. We only observe new IDs;
    // packet creation, sending, and retries remain entirely vanilla-owned.
    auto* manager = std::exchange(transferManager, nullptr);
    if (!manager) return;
    traceRequests("capture-end-batch",manager->mRequestBatch ? manager->mRequestBatch->mRequests->size() : 0,
        bool(manager->mRequest));
    if (!manager->mRequestBatch) return;
    std::size_t added = 0;
    for (auto const& request : manager->mRequestBatch->mRequests.get()) {
        if (request && !previousRequests.contains(request->mClientRequestId->mRawId)) {
            barrier.track(token,request->mClientRequestId->mRawId);
            ++added;
        }
    }
    traceRequests("capture-new-requests",added,bool(manager->mRequest));
}
ResponseBarrier::Result transferResult(TransferToken token) {
    std::lock_guard lock(mutex);
    return barrier.result(token,ResponseBarrier::Clock::now());
}
}
