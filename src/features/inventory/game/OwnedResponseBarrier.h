#pragma once
#include "features/inventory/game/ResponseBarrier.h"
#include <limits>
#include <optional>

namespace lamium::inventory::game {
struct TransferToken {
    std::uint64_t value;
    bool operator==(TransferToken const&) const = default;
};

// Call under the request tracker's mutex. Tokens belong to individual
// operations, so a cancelled/reentrant job cannot release a newer operation.
class OwnedResponseBarrier {
public:
    std::optional<TransferToken> begin(ResponseBarrier::Clock::time_point now) {
        if (active || serial == std::numeric_limits<std::uint64_t>::max()) return {};
        barrier.begin(now);
        active = TransferToken{++serial};
        return active;
    }
    bool owns(TransferToken token) const { return active && *active == token; }
    bool busy() const { return active.has_value(); }
    bool release(TransferToken token) {
        if (!owns(token)) return false;
        active.reset();
        return true;
    }
    // Shutdown invalidates ownership without reusing tokens after restart.
    void reset() { active.reset(); }
    void track(TransferToken token, std::int64_t id) { if (owns(token)) barrier.track(id); }
    void respond(std::int64_t id, bool accepted) { if (active) barrier.respond(id,accepted); }
    ResponseBarrier::Result result(TransferToken token, ResponseBarrier::Clock::time_point now) const {
        return owns(token) ? barrier.result(now) : ResponseBarrier::Result::Untracked;
    }
private:
    ResponseBarrier barrier;
    std::optional<TransferToken> active;
    std::uint64_t serial = 0;
};
}
