#include "features/information/NetworkInfo.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/network/ClientNetworkSystem.h"
#include "mc/network/NetworkConnection.h"
#include <mutex>

namespace lamium::information {
std::optional<std::int64_t> connectionPing(IClientInstance& client) {
    if (!client.getLocalPlayer()) return {};
    auto& network = client.getClientNetworkSystem();
    // Do not stall rendering behind network work. Connection ownership and state
    // are borrowed only while the client's connection mutex is held.
    std::unique_lock lock(network.mConnectionsMutex.get(),std::try_to_lock);
    if (!lock.owns_lock()) return {};
    NetworkConnection const* selected = nullptr;
    for (auto const& connection : network.mConnections.get()) {
        if (!connection || connection->mDisconnected || connection->mShouldCloseConnection) continue;
        // A local connection has no meaningful remote RTT. Ambiguous transition
        // states must not accidentally show another peer's latency.
        if (selected) return {};
        selected = connection.get();
    }
    if (!selected || selected->mType != NetworkConnection::Type::Remote) return {};
    auto const& peer = selected->mPeer.get();
    if (!peer || peer->isLocal()) return {};
    return measuredPing(peer->getNetworkStatus().mCurrentPing->count());
}
}
